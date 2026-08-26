#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <memory_interface.hpp>

namespace sogen
{
    class windows_emulator;

    // Chunked, page-boundary-respecting guest memory copy: reads out.size() bytes from src starting
    // at address into the host-side buffer out, one page at a time, stopping at the first page that
    // fails to read. Returns the number of bytes actually transferred, which is less than out.size()
    // on a partial failure. Shared by NtReadVirtualMemory/NtWriteVirtualMemory's same-process
    // handlers (syscalls/memory.cpp, where both src and the destination are the same address space)
    // and the cross-process read_memory/write_memory control-channel executors
    // (process_control_server.cpp, where the host buffer is the wire payload) - the one
    // implementation of this partial-transfer chunking both paths rely on.
    //
    // For the same-process handlers, this means the source range is read out to a fully-owned host
    // buffer (sized to the whole request, not one page) before copy_guest_range_in writes any of it
    // back in - unlike the single-buffer, single-chunk-reused, interleaved read/write loop this
    // replaced. That old loop could self-corrupt a forward-overlapping copy spanning more than one
    // page (writing chunk N into the destination could clobber source bytes chunk N+1 was about to
    // read), because a shared page-sized scratch buffer was reused across both directions per chunk.
    // The two-pass design here is memmove-safe: since the whole source is captured before any
    // destination byte is touched, overlapping ranges can never observe a partially-overwritten
    // source, matching how a real kernel-mediated NtReadVirtualMemory/NtWriteVirtualMemory copy
    // behaves. This is a deliberate, disclosed correctness improvement over the prior behavior, not
    // an accidental one - see cross_process_test.cpp's *Overlap* tests.
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

    // Resolves the windows path of whatever's mapped at base_address in emu's address space - a
    // section view's backing file if any (memory_manager::get_region_mapped_filename), falling back
    // to the module loaded there, if any. Shared by NtQueryVirtualMemory's same-process
    // MemoryMappedFilenameInformation handler (syscalls/memory.cpp, where emu is the requester's own
    // instance) and the cross-process query_memory executor (process_control_server.cpp, where emu is
    // the child being queried).
    std::optional<std::u16string> get_mapped_filename(windows_emulator& emu, uint64_t base_address);

} // namespace sogen
