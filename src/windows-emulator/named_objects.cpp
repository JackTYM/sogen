#include "std_include.hpp"
#include "named_objects.hpp"

#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN) && !defined(__EMSCRIPTEN__)
#include <sys/mman.h>
#endif

namespace sogen
{
    namespace
    {
        void unlink_shm(const std::string& name)
        {
#if !defined(_WIN32) && !defined(OS_EMSCRIPTEN) && !defined(__EMSCRIPTEN__)
            ::shm_unlink(name.c_str());
#else
            (void)name;
#endif
        }

        constexpr uint64_t section_size_mask = (1ULL << 40) - 1;
        constexpr unsigned section_protection_shift = 40;
        constexpr unsigned section_reserve_shift = 48;

        uint64_t encode_section_meta(const section_object& section, const size_t size)
        {
            return (size & section_size_mask) |
                   (static_cast<uint64_t>(section.section_page_protection & 0xFF) << section_protection_shift) |
                   (static_cast<uint64_t>(section.allocation_attributes == SEC_RESERVE ? 1 : 0) << section_reserve_shift);
        }

        // Binds `section` to an already-registered section slot: opens its named pages and copies its
        // description. `reference_held` says the caller owns a reference to the slot that this consumes.
        NTSTATUS bind_section_slot(const std::shared_ptr<kernel_arena>& arena, const uint32_t slot, section_object& section)
        {
            std::string shm_name{};
            uint64_t meta{};
            if (!arena->get_shm_backing(slot, shm_name, meta))
            {
                arena->release(slot);
                return STATUS_OBJECT_TYPE_MISMATCH;
            }

            const auto size = static_cast<size_t>(meta & section_size_mask);
            auto backing = shared_backing::open_named(shm_name, size);
            if (!backing)
            {
                arena->release(slot);
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            section.maximum_size = page_align_up(size);
            section.section_page_protection = static_cast<uint32_t>((meta >> section_protection_shift) & 0xFF);
            section.allocation_attributes = ((meta >> section_reserve_shift) & 1) != 0 ? SEC_RESERVE : SEC_COMMIT;
            section.backing = std::move(backing);
            section.state.adopt(arena, slot);
            arena->release(slot);
            return STATUS_SUCCESS;
        }

        void bind_found_slot(const std::shared_ptr<kernel_arena>& arena, const uint32_t slot, kernel_state& state)
        {
            // find_named/register_named already took a reference for this caller; adopt() takes its own.
            state.adopt(arena, slot);
            arena->release(slot);
        }
    }

    NTSTATUS named_objects::open(process_context& process, const kind expected, const std::u16string_view name, kernel_state& state)
    {
        if (!process.shared_arena)
        {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        const auto found = process.shared_arena->find_named(canonical_object_name(name));
        if (!found)
        {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        if (found->kind != expected)
        {
            process.shared_arena->release(found->slot);
            return STATUS_OBJECT_TYPE_MISMATCH;
        }

        bind_found_slot(process.shared_arena, found->slot, state);
        return STATUS_SUCCESS;
    }

    NTSTATUS named_objects::publish(process_context& process, const kind object_kind, const std::u16string_view name, kernel_state& state)
    {
        if (!process.ensure_shared_arena() || !state.promote(process.shared_arena))
        {
            return STATUS_SUCCESS;
        }

        const auto existing = process.shared_arena->register_named(canonical_object_name(name), object_kind, state.slot()->index());
        if (!existing)
        {
            return STATUS_SUCCESS;
        }

        if (existing->kind != object_kind)
        {
            process.shared_arena->release(existing->slot);
            return STATUS_OBJECT_TYPE_MISMATCH;
        }

        bind_found_slot(process.shared_arena, existing->slot, state);
        return STATUS_OBJECT_NAME_EXISTS;
    }

    bool named_objects::is_shareable_named_section(const section_object& section)
    {
        return !section.name.empty() && !section.is_image() && section.file_name.empty() && section.maximum_size != 0 &&
               section.allocation_attributes != SEC_RESERVE;
    }

    NTSTATUS named_objects::publish_section(process_context& process, section_object& section)
    {
        if (!process.ensure_shared_arena())
        {
            return STATUS_SUCCESS;
        }

        const auto size = static_cast<size_t>(page_align_up(section.maximum_size));
        std::string shm_name{};
        auto backing = shared_backing::create_named(size, shm_name);
        if (!backing)
        {
            return STATUS_SUCCESS;
        }

        if (!section.state.promote(process.shared_arena))
        {
            unlink_shm(shm_name);
            return STATUS_SUCCESS;
        }

        const auto slot = section.state.slot()->index();
        if (!process.shared_arena->set_shm_backing(slot, shm_name, encode_section_meta(section, size)))
        {
            unlink_shm(shm_name);
            section.state = {};
            return STATUS_SUCCESS;
        }

        section.backing = std::move(backing);

        const auto existing = process.shared_arena->register_named(canonical_object_name(section.name), kind::section, slot);
        if (!existing)
        {
            return STATUS_SUCCESS;
        }

        if (existing->kind != kind::section)
        {
            process.shared_arena->release(existing->slot);
            return STATUS_OBJECT_TYPE_MISMATCH;
        }

        section.state = {};
        section.backing.reset();
        const auto status = bind_section_slot(process.shared_arena, existing->slot, section);
        return NT_SUCCESS(status) ? STATUS_OBJECT_NAME_EXISTS : status;
    }

    NTSTATUS named_objects::open_section(process_context& process, const std::u16string_view name, section_object& section)
    {
        if (!process.shared_arena)
        {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        const auto found = process.shared_arena->find_named(canonical_object_name(name));
        if (!found)
        {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        if (found->kind != kind::section)
        {
            process.shared_arena->release(found->slot);
            return STATUS_OBJECT_TYPE_MISMATCH;
        }

        const auto status = bind_section_slot(process.shared_arena, found->slot, section);
        if (NT_SUCCESS(status))
        {
            section.name = std::u16string(name);
        }
        return status;
    }
}
