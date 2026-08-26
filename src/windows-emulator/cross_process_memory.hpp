#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <memory_interface.hpp>

namespace sogen
{
    // Chunked, page-boundary-respecting guest memory copy: reads out.size() bytes from src starting
    // at address into the host-side buffer out, one page at a time, stopping at the first page that
    // fails to read. Returns the number of bytes actually transferred, which is less than out.size()
    // on a partial failure. Shared by NtReadVirtualMemory/NtWriteVirtualMemory's same-process
    // handlers (syscalls/memory.cpp, where both src and the destination are the same address space)
    // and the cross-process read_memory/write_memory control-channel executors
    // (process_control_server.cpp, where the host buffer is the wire payload) - the one
    // implementation of this partial-transfer chunking both paths rely on.
    size_t copy_guest_range_out(memory_interface& src, uint64_t address, std::span<std::byte> out);

    // Mirror of copy_guest_range_out for writes: writes in.size() bytes from the host-side buffer in
    // into dst starting at address, one page at a time, stopping at the first page that fails to
    // write.
    size_t copy_guest_range_in(memory_interface& dst, uint64_t address, std::span<const std::byte> in);

    // Maps a copy_guest_range_out/_in result to the NTSTATUS NtReadVirtualMemory/NtWriteVirtualMemory
    // report: a full transfer is STATUS_SUCCESS, a partial one is STATUS_PARTIAL_COPY. A transferred
    // count of zero is reported as STATUS_INVALID_ADDRESS rather than STATUS_PARTIAL_COPY - a
    // pre-existing, deliberate divergence from real ntdll (which returns STATUS_PARTIAL_COPY even for
    // a zero-byte transfer) that this function preserves rather than "fixes".
    NTSTATUS transfer_status(size_t transferred, size_t requested);

} // namespace sogen
