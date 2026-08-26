#include "std_include.hpp"
#include "process_control_server.hpp"

#include "cross_process_memory.hpp"
#include "memory_utils.hpp"
#include "windows_emulator.hpp"
#include "windows_objects.hpp"

#include <address_utils.hpp>

#include <cstring>

namespace sogen
{
    namespace
    {
        void execute_read_memory(windows_emulator& target, const process_control_request& request, process_control_response& response)
        {
            std::vector<std::byte> data(request.size);
            const auto transferred = copy_guest_range_out(target.memory, request.address, data);
            data.resize(transferred);

            response.payload = std::move(data);
            response.status = transfer_status(transferred, request.size);
        }

        void execute_write_memory(windows_emulator& target, const process_control_request& request, process_control_response& response)
        {
            const auto transferred = copy_guest_range_in(target.memory, request.address, request.payload);

            response.bytes_written = transferred;
            response.status = transfer_status(transferred, request.payload.size());
        }

        // Narrower than syscalls/memory.cpp's handle_NtAllocateVirtualMemoryEx in two ways, both
        // deliberate scope cuts for this task: it never validates request.size == 0 or an
        // unrecognized/empty allocation_type (the real handler rejects both with
        // STATUS_INVALID_PARAMETER, memory.cpp:426-429/574-577), and it always calls
        // memory_manager::allocate_memory - there is no commit-onto-an-existing-reservation path
        // (commit && !reserve calls memory_manager::commit_memory instead, memory.cpp:579-583). Task 7
        // adds both when this executor starts backing the real NtAllocateVirtualMemory{,Ex}
        // cross-process path.
        void execute_allocate_memory(windows_emulator& target, const process_control_request& request, process_control_response& response)
        {
            const auto protection = try_map_nt_to_emulator_protection(request.protection);
            if (!protection.has_value())
            {
                response.status = STATUS_INVALID_PAGE_PROTECTION;
                return;
            }

            const auto size = static_cast<size_t>(page_align_up(request.size));
            const bool reserve_only = (request.allocation_type & MEM_COMMIT) == 0;

            uint64_t base = 0;
            bool success = false;

            if (request.address != 0)
            {
                base = page_align_down(request.address);
                success = target.memory.allocate_memory(base, size, *protection, reserve_only);
            }
            else
            {
                base = target.memory.allocate_memory(size, *protection, reserve_only);
                success = base != 0;
            }

            if (!success)
            {
                response.status = STATUS_MEMORY_NOT_ALLOCATED;
                return;
            }

            response.base_address = base;
            response.region_size = size;
            response.status = STATUS_SUCCESS;
        }

        void execute_protect_memory(windows_emulator& target, const process_control_request& request, process_control_response& response)
        {
            const auto protection = try_map_nt_to_emulator_protection(request.protection);
            if (!protection.has_value())
            {
                response.status = STATUS_INVALID_PAGE_PROTECTION;
                return;
            }

            const auto aligned_start = page_align_down(request.address);
            const auto aligned_length = page_align_up(request.address + request.size) - aligned_start;

            nt_memory_permission old_protection_value{};
            bool success = false;

            try
            {
                success =
                    target.memory.protect_memory(aligned_start, static_cast<size_t>(aligned_length), *protection, &old_protection_value);
            }
            catch (...)
            {
                response.status = STATUS_INVALID_ADDRESS;
                return;
            }

            if (!success)
            {
                response.status = STATUS_INVALID_ADDRESS;
                return;
            }

            response.base_address = aligned_start;
            response.region_size = aligned_length;
            response.old_protection = map_emulator_to_nt_protection(old_protection_value);
            response.status = STATUS_SUCCESS;
        }

        void execute_free_memory(windows_emulator& target, const process_control_request& request, process_control_response& response)
        {
            if (request.free_type == 0)
            {
                response.status = STATUS_INVALID_PARAMETER_4;
                return;
            }

            if (request.free_type & MEM_RELEASE)
            {
                const auto release_base = page_align_down(request.address);
                const auto region_info = target.memory.get_region_info(release_base);

                if (!region_info.is_reserved || region_info.allocation_base != release_base)
                {
                    response.status = STATUS_MEMORY_NOT_ALLOCATED;
                    return;
                }

                const auto released_length = region_info.allocation_length;
                if (!target.memory.release_memory(release_base, 0))
                {
                    response.status = STATUS_MEMORY_NOT_ALLOCATED;
                    return;
                }

                response.base_address = release_base;
                response.region_size = released_length;
                response.status = STATUS_SUCCESS;
                return;
            }

            if (request.free_type & MEM_DECOMMIT)
            {
                const auto region_info = target.memory.get_region_info(request.address);
                if (!region_info.is_reserved)
                {
                    response.status = STATUS_MEMORY_NOT_ALLOCATED;
                    return;
                }

                if (!target.memory.decommit_memory(region_info.allocation_base, region_info.allocation_length))
                {
                    response.status = STATUS_MEMORY_NOT_ALLOCATED;
                    return;
                }

                response.base_address = region_info.allocation_base;
                response.region_size = region_info.allocation_length;
                response.status = STATUS_SUCCESS;
                return;
            }

            response.status = STATUS_INVALID_PARAMETER_4;
        }

        void execute_query_memory(windows_emulator& target, const process_control_request& request, process_control_response& response)
        {
            if (request.info_class != MemoryBasicInformation && request.info_class != MemoryPrivilegedBasicInformation)
            {
                response.status = STATUS_NOT_SUPPORTED;
                return;
            }

            const auto region_info = target.memory.get_region_info(request.address);

            EMU_MEMORY_BASIC_INFORMATION64 info{};
            const auto state = region_info.is_reserved ? MEM_RESERVE : MEM_FREE;
            info.State = region_info.is_committed ? MEM_COMMIT : state;
            info.BaseAddress = region_info.start;
            info.AllocationBase = region_info.allocation_base;
            info.PartitionId = 0;
            info.RegionSize = static_cast<int64_t>(region_info.length);
            info.Protect = map_emulator_to_nt_protection(region_info.permissions);
            // Deliberately narrower than syscalls/memory.cpp's map_emulator_to_nt_allocation_protection:
            // this task's scope only needs private allocations (see AllocateThenWriteThenRead), for which
            // both formulas agree. Task 7 wires the full section_image-aware mapping when this executor
            // starts backing the real NtQueryVirtualMemory cross-process path.
            info.AllocationProtect = map_emulator_to_nt_protection(region_info.initial_permissions);
            info.Type = region_info.is_reserved ? memory_region_policy::to_memory_basic_information_type(region_info.kind) : 0;

            response.payload.resize(sizeof(info));
            std::memcpy(response.payload.data(), &info, sizeof(info));
            response.status = STATUS_SUCCESS;
        }

        // The initial thread is always the one with the lowest id (process_context::create_thread
        // hands out 8, 12, 16, ... in creation order, see its own doc comment) - a resume_thread
        // request never carries a thread id because on real Windows this always targets the one
        // pseudo thread handle NtCreateUserProcess minted for the child (the sandbox broker's
        // ResumeThread(target.thread_handle())), which is this thread.
        emulator_thread* find_initial_thread(process_context& process)
        {
            emulator_thread* initial = nullptr;
            for (auto& thread : process.threads | std::views::values)
            {
                if (!initial || thread.id < initial->id)
                {
                    initial = &thread;
                }
            }

            return initial;
        }

        void execute_resume_thread(windows_emulator& target, const process_control_request& /*request*/, process_control_response& response)
        {
            auto* const thread = find_initial_thread(target.process);
            if (!thread)
            {
                response.status = STATUS_UNSUCCESSFUL;
                return;
            }

            response.previous_suspend_count = thread->suspended;

            if (thread->suspended > 0)
            {
                thread->suspended -= 1;
            }

            response.status = STATUS_SUCCESS;
        }

        void execute_terminate(windows_emulator& target, const process_control_request& request, process_control_response& response)
        {
            target.process.exit_status = request.exit_status;
            target.emu().stop();
            response.status = STATUS_SUCCESS;
        }

        void execute_adopt_section(windows_emulator& target, const process_control_request& request, process_control_response& response)
        {
            section s{};
            s.object->maximum_size = request.maximum_size;
            s.object->section_page_protection = request.page_protection;
            s.object->allocation_attributes = request.allocation_attributes;
            s.object->backing_storage = request.payload;
            s.granted_access = request.granted_access;

            const auto h = target.process.sections.store(std::move(s));

            response.minted_handle_bits = h.bits;
            response.status = STATUS_SUCCESS;
        }
    }

    void execute_process_control_request(windows_emulator& target, const process_control_request& request,
                                         process_control_response& response)
    {
        response = {};
        response.request_id = request.request_id;

        switch (request.op)
        {
        case process_control_op::read_memory:
            execute_read_memory(target, request, response);
            break;
        case process_control_op::write_memory:
            execute_write_memory(target, request, response);
            break;
        case process_control_op::allocate_memory:
            execute_allocate_memory(target, request, response);
            break;
        case process_control_op::protect_memory:
            execute_protect_memory(target, request, response);
            break;
        case process_control_op::free_memory:
            execute_free_memory(target, request, response);
            break;
        case process_control_op::query_memory:
            execute_query_memory(target, request, response);
            break;
        case process_control_op::terminate:
            execute_terminate(target, request, response);
            break;
        case process_control_op::resume_thread:
            execute_resume_thread(target, request, response);
            break;
        case process_control_op::adopt_section:
            execute_adopt_section(target, request, response);
            break;
        }
    }

    void pump_process_control(windows_emulator& target, process_control_channel& channel)
    {
        while (const auto request = channel.try_receive())
        {
            process_control_response response{};
            execute_process_control_request(target, *request, response);
            channel.respond(response);
        }
    }

} // namespace sogen
