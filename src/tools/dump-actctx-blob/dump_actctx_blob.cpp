#include <windows.h>
#include <winternl.h>
#include <cstdio>
#include <cstdint>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "ntdll.lib")
#endif

// NtQueryInformationProcess is already declared by <winternl.h> - no need to redeclare it.

// Dumps the real ACTIVATION_CONTEXT_DATA blob a suspended process's PEB points at, so it can be
// used as a golden fixture. On real Windows this blob is built by CSRSS from the parent's CSR
// create-process message before the child ever runs - nothing inside ntdll ever constructs it
// itself, only consumes it - so this is the only way to get a real, correct one to test against.
int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: dump_actctx_blob.exe <target.exe>\n");
        return 1;
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(argv[1], nullptr, nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &pi))
    {
        fprintf(stderr, "CreateProcessA failed: %lu\n", GetLastError());
        return 1;
    }

    PROCESS_BASIC_INFORMATION pbi{};
    ULONG return_length = 0;
    const auto status = NtQueryInformationProcess(pi.hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &return_length);
    if (status != 0)
    {
        fprintf(stderr, "NtQueryInformationProcess failed: 0x%lx\n", static_cast<unsigned long>(status));
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    // PEB->ActivationContextData / PEB->ProcessAssemblyStorageMap offsets on 64-bit - confirmed
    // against this project's own PEB64 layout (kernel_mapped.hpp), not guessed.
    constexpr std::uintptr_t activation_context_data_offset = 0x2f8;
    constexpr std::uintptr_t process_assembly_storage_map_offset = 0x300;

    std::uint64_t act_ctx_ptr = 0;
    std::uint64_t storage_map_ptr = 0;
    SIZE_T bytes_read = 0;

    ReadProcessMemory(pi.hProcess, reinterpret_cast<BYTE*>(pbi.PebBaseAddress) + activation_context_data_offset, &act_ctx_ptr,
                      sizeof(act_ctx_ptr), &bytes_read);
    ReadProcessMemory(pi.hProcess, reinterpret_cast<BYTE*>(pbi.PebBaseAddress) + process_assembly_storage_map_offset, &storage_map_ptr,
                      sizeof(storage_map_ptr), &bytes_read);

    // CSRSS builds the activation context blob asynchronously relative to CreateProcess
    // returning - reading immediately after CREATE_SUSPENDED risked a torn/partial read (seen
    // empirically: a differential capture came back with a corrupted first TOC entry while an
    // otherwise-identical capture didn't). Give it a moment to finish before reading memory.
    Sleep(250);

    fprintf(stderr, "PEB=%p ActivationContextData=0x%llx ProcessAssemblyStorageMap=0x%llx\n", static_cast<const void*>(pbi.PebBaseAddress),
            static_cast<unsigned long long>(act_ctx_ptr), static_cast<unsigned long long>(storage_map_ptr));

    if (act_ctx_ptr == 0)
    {
        fprintf(stderr, "ActivationContextData is NULL - target has no process default context\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    // TotalSize's real offset within the blob isn't confirmed yet - a later step determines the
    // exact field layout. For this capture, over-read a generous fixed amount (1 MiB is far more
    // than any realistic activation context blob) and let offline analysis find the real
    // end/TotalSize field within it, rather than trusting an unconfirmed offset here.
    constexpr SIZE_T generous_read_size = 1024 * 1024;
    std::vector<std::uint8_t> buffer(generous_read_size);
    ReadProcessMemory(pi.hProcess, reinterpret_cast<void*>(act_ctx_ptr), buffer.data(), buffer.size(), &bytes_read);

    fwrite(buffer.data(), 1, bytes_read, stdout);

    TerminateProcess(pi.hProcess, 0);
    return 0;
}
