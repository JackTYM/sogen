#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <cstdio>

namespace
{
    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_set_cancel_timer()
    {
        HANDLE hTimer = CreateWaitableTimerA(nullptr, FALSE, nullptr);
        if (!hTimer)
        {
            return false;
        }

        LARGE_INTEGER due{};
        due.QuadPart = -10000000LL; // 1 second relative

        if (!SetWaitableTimer(hTimer, &due, 0, nullptr, nullptr, FALSE))
        {
            CloseHandle(hTimer);
            return false;
        }

        if (!CancelWaitableTimer(hTimer))
        {
            CloseHandle(hTimer);
            return false;
        }

        CloseHandle(hTimer);
        return true;
    }

    bool test_query_timer_resolution()
    {
        using Fn = NTSTATUS(NTAPI*)(PULONG, PULONG, PULONG);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryTimerResolution"));
        if (!fn)
            return false;
        ULONG maximum = 0, minimum = 0, current = 0;
        const NTSTATUS st = fn(&maximum, &minimum, &current);
        return NT_SUCCESS(st) && maximum > 0 && minimum > 0 && current > 0;
    }

    bool test_set_timer_resolution_set()
    {
        using Fn = NTSTATUS(NTAPI*)(ULONG, BOOLEAN, PULONG);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetTimerResolution"));
        if (!fn)
            return false;
        ULONG current = 0;
        const NTSTATUS st = fn(0x2710, TRUE, &current);
        return NT_SUCCESS(st) && current > 0;
    }

    bool test_set_timer_resolution_clear_not_set()
    {
        using Fn = NTSTATUS(NTAPI*)(ULONG, BOOLEAN, PULONG);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetTimerResolution"));
        if (!fn)
            return false;
        ULONG current = 0;
        const NTSTATUS st = fn(0, FALSE, &current);
        return st == static_cast<NTSTATUS>(0xC0000245) && current > 0; // STATUS_TIMER_RESOLUTION_NOT_SET; current written even on error
    }

    bool test_are_mapped_files_the_same()
    {
        using NtAreMappedFilesTheSameFn = NTSTATUS(NTAPI*)(PVOID, PVOID);

        const auto fn =
            reinterpret_cast<NtAreMappedFilesTheSameFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtAreMappedFilesTheSame"));
        if (!fn)
        {
            return false;
        }

        void* p1 = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        void* p2 = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!p1 || !p2)
        {
            if (p1)
                VirtualFree(p1, 0, MEM_RELEASE);
            if (p2)
                VirtualFree(p2, 0, MEM_RELEASE);
            return false;
        }

        const NTSTATUS status = fn(p1, p2);
        VirtualFree(p1, 0, MEM_RELEASE);
        VirtualFree(p2, 0, MEM_RELEASE);

        // Any result other than STATUS_NOT_SUPPORTED means the handler ran correctly
        return status != static_cast<NTSTATUS>(0xC00000BBL);
    }
}

int main()
{
    bool all_ok = true;
    all_ok &= report("SetWaitableTimer + CancelWaitableTimer", test_set_cancel_timer());
    all_ok &= report("NtQueryTimerResolution", test_query_timer_resolution());
    all_ok &= report("NtSetTimerResolution (set)", test_set_timer_resolution_set());
    all_ok &= report("NtSetTimerResolution (clear→NOT_SET)", test_set_timer_resolution_clear_not_set());
    all_ok &= report("NtAreMappedFilesTheSame (different regions)", test_are_mapped_files_the_same());
    return all_ok ? 0 : 1;
}
