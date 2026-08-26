#include "std_include.hpp"
#include "cross_process_memory.hpp"

#include "windows_emulator.hpp"
#include "windows_path.hpp"

#include <algorithm>

namespace sogen
{
    namespace
    {
        constexpr size_t page_size = 0x1000;

        size_t bytes_until_page_boundary(const uint64_t address)
        {
            const auto offset = address % page_size;
            return static_cast<size_t>(offset == 0 ? page_size : page_size - offset);
        }
    }

    size_t copy_guest_range_out(memory_interface& src, const uint64_t address, const std::span<std::byte> out)
    {
        size_t transferred = 0;
        while (transferred < out.size())
        {
            const auto current_address = address + transferred;
            const auto bytes_remaining = out.size() - transferred;
            const auto chunk_size = std::min(bytes_remaining, bytes_until_page_boundary(current_address));

            if (!src.try_read_memory(current_address, out.data() + transferred, chunk_size))
            {
                break;
            }

            transferred += chunk_size;
        }

        return transferred;
    }

    size_t copy_guest_range_in(memory_interface& dst, const uint64_t address, const std::span<const std::byte> in)
    {
        size_t transferred = 0;
        while (transferred < in.size())
        {
            const auto current_address = address + transferred;
            const auto bytes_remaining = in.size() - transferred;
            const auto chunk_size = std::min(bytes_remaining, bytes_until_page_boundary(current_address));

            if (!dst.try_write_memory(current_address, in.data() + transferred, chunk_size))
            {
                break;
            }

            transferred += chunk_size;
        }

        return transferred;
    }

    NTSTATUS transfer_status(const size_t transferred, const size_t requested)
    {
        if (transferred == requested)
        {
            return STATUS_SUCCESS;
        }

        return transferred == 0 ? STATUS_INVALID_ADDRESS : STATUS_PARTIAL_COPY;
    }

    std::optional<std::u16string> get_mapped_filename(windows_emulator& emu, const uint64_t base_address)
    {
        if (const auto mapped_filename = emu.memory.get_region_mapped_filename(base_address))
        {
            try
            {
                return windows_path(*mapped_filename).to_device_path();
            }
            catch (const std::exception&)
            {
                return windows_path(*mapped_filename).to_unc_path();
            }
        }

        const auto* mod = emu.mod_manager.find_by_address(base_address);
        if (!mod || mod->module_path.empty())
        {
            return std::nullopt;
        }

        try
        {
            return mod->module_path.to_device_path();
        }
        catch (const std::exception&)
        {
            return mod->module_path.to_unc_path();
        }
    }

} // namespace sogen
