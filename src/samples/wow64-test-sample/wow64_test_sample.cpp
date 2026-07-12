// Minimal, first-party 32-bit WoW64 regression test. Built as a genuine 32-bit PE (the "Windows
// x86" CI platform matrix entry already compiles everything under src/samples/ with MSVC's 32-bit
// toolset - see src/CMakeLists.txt's `if(WIN32)` guard), then run under a 64-bit analyzer via real
// WoW64 translation (as opposed to smoke-test-win32/win-test, which run a 32-bit analyzer emulating
// a 32-bit guest directly - no WoW64 involved at all).
//
// Deliberately narrow in scope: this exists to regression-test the two FEX/Apple-Silicon
// host-address-space bugs fixed alongside it (see memory_manager.cpp's find_free_host_allocation_base
// and its two call sites), not to be a general Win32 API exerciser like test-sample.exe. Each phase
// below maps to one of those bugs:
//   - Module load/unload churn: repeatedly relocating real system DLLs stresses exactly the
//     fixed-address relocation-fallback race find_free_host_allocation_base's module_mapping.cpp
//     call site fixes (foreign host VA landing on a picked relocation base).
//   - VirtualAlloc/VirtualFree churn: stresses the general host-aware allocation path (the size-only
//     allocate_memory overload) the same fix touches.
// A full WoW64-under-FEX run (process init through wow64.dll, the 32-bit rebase window, the
// heaven's-gate trampoline) already exercises the other fixed bug (the wow64-rebase-window blind
// spot in handle_NtAllocateVirtualMemoryEx) just by getting this far at all.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <windows.h>

namespace
{
    constexpr int module_cycle_count = 50;
    constexpr int alloc_cycle_count = 200;

    // Ordinary system DLLs present in any standard Windows emulation root, chosen only for being
    // real, non-trivial, relocatable modules - not for their functionality.
    constexpr const char* modules_to_cycle[] = {
        "advapi32.dll", "shell32.dll", "ole32.dll", "gdi32.dll", "user32.dll",
    };

    bool cycle_module_loads()
    {
        for (int i = 0; i < module_cycle_count; ++i)
        {
            for (const auto* name : modules_to_cycle)
            {
                const HMODULE mod = LoadLibraryA(name);
                if (!mod)
                {
                    std::printf("wow64-test: LoadLibraryA(%s) failed: %lu\n", name, GetLastError());
                    return false;
                }

                if (!FreeLibrary(mod))
                {
                    std::printf("wow64-test: FreeLibrary(%s) failed: %lu\n", name, GetLastError());
                    return false;
                }
            }
        }

        return true;
    }

    bool cycle_virtual_alloc()
    {
        for (int i = 0; i < alloc_cycle_count; ++i)
        {
            const SIZE_T size = (static_cast<SIZE_T>(i % 16) + 1) * 4096;
            void* const mem = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (!mem)
            {
                std::printf("wow64-test: VirtualAlloc(size=%lu) failed: %lu\n", static_cast<unsigned long>(size), GetLastError());
                return false;
            }

            // Touch every page so a genuinely bad mapping (not just a bad address pick) would fault here.
            std::memset(mem, static_cast<int>(i & 0xFF), size);

            if (!VirtualFree(mem, 0, MEM_RELEASE))
            {
                std::printf("wow64-test: VirtualFree failed: %lu\n", GetLastError());
                return false;
            }
        }

        return true;
    }

    bool basic_file_roundtrip()
    {
        const char path[] = "c:\\wow64_test_sample_out.txt";
        const char message[] = "wow64-test-sample file roundtrip ok";

        const HANDLE write_handle = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (write_handle == INVALID_HANDLE_VALUE)
        {
            std::printf("wow64-test: CreateFileA(write) failed: %lu\n", GetLastError());
            return false;
        }

        DWORD written = 0;
        const BOOL write_ok = WriteFile(write_handle, message, sizeof(message) - 1, &written, nullptr);
        CloseHandle(write_handle);
        if (!write_ok || written != sizeof(message) - 1)
        {
            std::printf("wow64-test: WriteFile failed: %lu\n", GetLastError());
            return false;
        }

        const HANDLE read_handle = CreateFileA(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (read_handle == INVALID_HANDLE_VALUE)
        {
            std::printf("wow64-test: CreateFileA(read) failed: %lu\n", GetLastError());
            return false;
        }

        char buffer[64] = {};
        DWORD read = 0;
        const BOOL read_ok = ReadFile(read_handle, buffer, sizeof(buffer) - 1, &read, nullptr);
        CloseHandle(read_handle);
        if (!read_ok || read != sizeof(message) - 1 || std::memcmp(buffer, message, read) != 0)
        {
            std::printf("wow64-test: ReadFile mismatch\n");
            return false;
        }

        return true;
    }
} // namespace

int main()
{
    // Guest stdout here isn't a real console, so fully-buffered stdio can silently drop everything
    // printed before an early/abnormal exit. Line-buffer instead so each printf'd line is flushed as
    // one write (unlike _IONBF, which flushes byte-by-byte and would split every line's text across
    // many single-character log entries).
    std::setvbuf(stdout, nullptr, _IOLBF, BUFSIZ);

#if defined(_WIN64)
    std::printf("wow64-test-sample: built as 64-bit, not testing WoW64 - failing loudly rather than reporting a false pass\n");
    return 1;
#else
    std::printf("wow64-test-sample: starting (32-bit)\n");

    if (!cycle_module_loads())
    {
        return 1;
    }
    std::printf("wow64-test-sample: module load/unload churn ok (%d modules x %d cycles)\n",
                static_cast<int>(sizeof(modules_to_cycle) / sizeof(modules_to_cycle[0])), module_cycle_count);

    if (!cycle_virtual_alloc())
    {
        return 1;
    }
    std::printf("wow64-test-sample: VirtualAlloc/VirtualFree churn ok (%d cycles)\n", alloc_cycle_count);

    if (!basic_file_roundtrip())
    {
        return 1;
    }
    std::printf("wow64-test-sample: file roundtrip ok\n");

    std::printf("wow64-test-sample: ok\n");
    return 0;
#endif
}
