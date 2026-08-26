#include "std_include.hpp"
#include "cross_process_memory.hpp"

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

} // namespace sogen
