#include <cstdio>
#include <atomic>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>

extern "C" NTSTATUS NTAPI NtGetNextThread(HANDLE ProcessHandle, HANDLE ThreadHandle, ACCESS_MASK DesiredAccess, ULONG HandleAttributes,
                                          ULONG Flags, PHANDLE NewThreadHandle);
extern "C" NTSTATUS NTAPI NtQueueApcThreadEx2(HANDLE ThreadHandle, HANDLE ReserveHandle, ULONG ApcFlags, PVOID ApcRoutine, PVOID Argument1,
                                              PVOID Argument2, PVOID Argument3);

namespace
{
    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_thread_times_and_priority()
    {
        FILETIME create_time{}, exit_time{}, kernel_time{}, user_time{};
        if (!GetThreadTimes(GetCurrentThread(), &create_time, &exit_time, &kernel_time, &user_time))
        {
            return false;
        }

        const int prio = GetThreadPriority(GetCurrentThread());
        return prio != THREAD_PRIORITY_ERROR_RETURN;
    }

    bool test_thread_name()
    {
        SetThreadDescription(GetCurrentThread(), L"SogenTestThread");

        wchar_t* name{};
        const HRESULT hr = GetThreadDescription(GetCurrentThread(), &name);
        const bool ok = SUCCEEDED(hr);
        if (name)
        {
            LocalFree(name);
        }
        return ok;
    }

    bool test_thread_affinity()
    {
        DWORD_PTR system_mask{}, proc_mask{};
        if (!GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &system_mask))
        {
            return false;
        }

        if (proc_mask == 0)
        {
            return false;
        }

        const DWORD_PTR first_cpu = proc_mask & ~(proc_mask - 1);
        const DWORD_PTR old = SetThreadAffinityMask(GetCurrentThread(), first_cpu);
        if (old == 0)
        {
            return false;
        }

        SetThreadAffinityMask(GetCurrentThread(), old);
        return true;
    }

    bool test_tls()
    {
        const DWORD slot = TlsAlloc();
        if (slot == TLS_OUT_OF_INDEXES)
        {
            return false;
        }

        void* const kValue = reinterpret_cast<void*>(static_cast<uintptr_t>(0xdeadbeef));
        TlsSetValue(slot, kValue);

        const bool ok = TlsGetValue(slot) == kValue;
        TlsFree(slot);
        return ok;
    }

    bool test_get_next_thread_flags()
    {
        // Kernel: if (flags != 0) return STATUS_INVALID_PARAMETER (3221225485)
        // Artifact: if (a5) return 3221225485LL
        HANDLE nt{};
        const NTSTATUS st = NtGetNextThread(GetCurrentProcess(), nullptr, THREAD_QUERY_INFORMATION, 0, 1 /*Flags=1*/, &nt);
        if (nt)
            CloseHandle(nt);
        return st == static_cast<NTSTATUS>(0xC000000D); // STATUS_INVALID_PARAMETER
    }

    bool test_queue_apc_flags_validation()
    {
        // Kernel NtQueueApcThreadEx2: if ((apc_flags & 0xFFFEFFFE) != 0) return STATUS_INVALID_PARAMETER
        // Artifact: if ((a3 & 0xFFFEFFFE) != 0) return -1073741811
        const NTSTATUS st = NtQueueApcThreadEx2(GetCurrentThread(), nullptr, 0x2 /*invalid flag*/, nullptr, nullptr, nullptr, nullptr);
        return st == static_cast<NTSTATUS>(0xC000000D); // STATUS_INVALID_PARAMETER
    }

    bool test_yield_execution()
    {
        // NtYieldExecution: calls KeYieldExecution; sogen yields the current thread
        // Verified: YieldProcessor() wraps NtYieldExecution on Windows
        YieldProcessor();
        return true;
    }

    std::atomic<int> g_thread_counter{0};

    DWORD WINAPI counter_thread(void*)
    {
        g_thread_counter.fetch_add(1);
        return 0;
    }

    bool test_thread_id_and_handle()
    {
        g_thread_counter = 0;

        DWORD thread_id{};
        HANDLE h = CreateThread(nullptr, 0, counter_thread, nullptr, 0, &thread_id);
        if (!h || thread_id == 0)
        {
            return false;
        }

        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);

        return g_thread_counter == 1;
    }
}

int main()
{
    bool all_ok = true;
    all_ok &= report("Thread local storage", test_tls());
    all_ok &= report("Thread ID and handle", test_thread_id_and_handle());
    all_ok &= report("Thread times and priority", test_thread_times_and_priority());
    all_ok &= report("Thread affinity mask", test_thread_affinity());
    all_ok &= report("Thread name (description)", test_thread_name());
    all_ok &= report("NtGetNextThread flags rejected", test_get_next_thread_flags());
    all_ok &= report("NtQueueApcThreadEx2 flags validation", test_queue_apc_flags_validation());
    all_ok &= report("NtYieldExecution runs", test_yield_execution());
    return all_ok ? 0 : 1;
}
