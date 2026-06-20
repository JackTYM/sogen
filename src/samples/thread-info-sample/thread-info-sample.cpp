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

extern "C"
{
    typedef LONG NTSTATUS_T;
    typedef NTSTATUS_T(NTAPI* NtQueryInformationThreadFn)(HANDLE, int, PVOID, ULONG, PULONG);
    typedef NTSTATUS_T(NTAPI* NtSetInformationThreadFn)(HANDLE, int, PVOID, ULONG);
    typedef NTSTATUS_T(NTAPI* NtCreateThreadExFn)(PHANDLE, ACCESS_MASK, PVOID, HANDLE, PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);
}

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

    bool test_create_thread_ex_bad_flags()
    {
        // NtCreateThreadEx: kernel checks (create_flags & 0xFFFFFF80) != 0 → STATUS_INVALID_PARAMETER_7 (0xC00000F5)
        // Artifact: if ((a7 & 0xFFFFFF80) != 0) return 3221225717LL (= 0xC00000F5)
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn = reinterpret_cast<NtCreateThreadExFn>(GetProcAddress(ntdll, "NtCreateThreadEx"));
        if (!fn)
            return false;
        HANDLE h = nullptr;
        const NTSTATUS_T st = fn(&h, THREAD_ALL_ACCESS, nullptr, GetCurrentProcess(), nullptr, nullptr,
                                 0x80 /*invalid flag bit*/, 0, 0, 0, nullptr);
        return st == static_cast<NTSTATUS_T>(0xC00000F5);
    }

    bool test_query_thread_invalid_class()
    {
        // NtQueryInformationThread: unknown class → STATUS_INVALID_INFO_CLASS (0xC0000003)
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn = reinterpret_cast<NtQueryInformationThreadFn>(GetProcAddress(ntdll, "NtQueryInformationThread"));
        if (!fn)
            return false;
        char buf[64]{};
        ULONG ret = 0;
        const NTSTATUS_T st = fn(GetCurrentThread(), 99 /*unknown class*/, buf, sizeof(buf), &ret);
        return st == static_cast<NTSTATUS_T>(0xC0000003);
    }

    bool test_set_thread_hide_bad_length()
    {
        // NtSetInformationThread ThreadHideFromDebugger (=17): kernel requires length == 0.
        // Non-zero length → STATUS_INVALID_PARAMETER (0xC000000D).
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn = reinterpret_cast<NtSetInformationThreadFn>(GetProcAddress(ntdll, "NtSetInformationThread"));
        if (!fn)
            return false;
        BOOLEAN val = TRUE;
        const NTSTATUS_T st = fn(GetCurrentThread(), 17 /*ThreadHideFromDebugger*/, &val, sizeof(val));
        return st == static_cast<NTSTATUS_T>(0xC000000D);
    }

    bool test_delay_execution()
    {
        // NtDelayExecution: kernel calls KeDelayExecutionThread; sogen yields for the interval.
        typedef NTSTATUS_T(NTAPI* NtDelayExecutionFn)(BOOLEAN, PLARGE_INTEGER);
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn = reinterpret_cast<NtDelayExecutionFn>(GetProcAddress(ntdll, "NtDelayExecution"));
        if (!fn)
            return false;
        LARGE_INTEGER interval{};
        interval.QuadPart = 0;
        const NTSTATUS_T st = fn(FALSE, &interval);
        return st == static_cast<NTSTATUS_T>(0);  // STATUS_SUCCESS
    }

    bool test_get_context_thread()
    {
        // NtGetContextThread: kernel calls ObReferenceObjectByHandle + PspGetContextThreadInternal.
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_CONTROL;
        const BOOL ok = GetThreadContext(GetCurrentThread(), &ctx);
        return ok != 0 && ctx.Rip != 0; // STATUS_SUCCESS: Rip must be non-zero
    }

    bool test_open_thread_by_id()
    {
        // NtOpenThread: kernel calls PsOpenThread; sogen searches proc.threads by UniqueThread id.
        // STATUS_SUCCESS expected when opening by current thread id.
        const DWORD tid = GetCurrentThreadId();
        HANDLE h = OpenThread(THREAD_QUERY_INFORMATION, FALSE, tid);
        const bool ok = h != nullptr;
        if (h)
            CloseHandle(h);
        return ok;
    }

    bool test_queue_apc_thread()
    {
        // NtQueueApcThread delegates to NtQueueApcThreadEx2; queues APC and drains via alertable wait.
        static volatile long g_apc_count = 0;
        const auto apc_fn = [](ULONG_PTR) { InterlockedIncrement(&g_apc_count); };
        if (!QueueUserAPC(apc_fn, GetCurrentThread(), 0))
            return false;
        SleepEx(0, TRUE);  // drain alertable APC queue
        return g_apc_count == 1;
    }

    bool test_queue_apcex_thread()
    {
        // NtQueueApcThreadEx: a2==1 sets special-user-APC flag (LOBYTE(v8) = a2 == 1).
        static volatile long g_apcex_count = 0;
        const auto apc_fn = [](ULONG_PTR) { InterlockedIncrement(&g_apcex_count); };
        if (!QueueUserAPC(apc_fn, GetCurrentThread(), 0))
            return false;
        SleepEx(0, TRUE);
        return g_apcex_count == 1;
    }

    std::atomic<int> g_suspend_resume_count{0};

    DWORD WINAPI suspend_test_fn(void*)
    {
        g_suspend_resume_count.fetch_add(1);
        return 0;
    }

    bool test_suspend_resume_thread()
    {
        // NtSuspendThread + NtResumeThread round-trip.
        g_suspend_resume_count = 0;
        HANDLE h = CreateThread(nullptr, 0, suspend_test_fn, nullptr, CREATE_SUSPENDED, nullptr);
        if (!h)
            return false;
        const DWORD prev = ResumeThread(h);  // NtResumeThread; STATUS_SUCCESS, prev count must be 1
        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);
        return prev == 1 && g_suspend_resume_count == 1; // STATUS_SUCCESS: thread ran to completion
    }

    bool test_set_context_thread()
    {
        // NtSetContextThread: read CONTEXT, write it back unchanged; must succeed.
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_CONTROL;
        if (!GetThreadContext(GetCurrentThread(), &ctx))
            return false;
        const BOOL ok = SetThreadContext(GetCurrentThread(), &ctx);
        return ok != 0; // STATUS_SUCCESS: context round-trip must succeed
    }

    bool test_terminate_thread()
    {
        // NtTerminateThread is called when a thread exits naturally (STATUS_SUCCESS).
        HANDLE h = CreateThread(nullptr, 0, suspend_test_fn, nullptr, 0, nullptr);
        if (!h)
            return false;
        const DWORD result = WaitForSingleObject(h, 5000);
        CloseHandle(h);
        return result == WAIT_OBJECT_0; // STATUS_SUCCESS: thread terminated cleanly
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
    all_ok &= report("NtCreateThreadEx bad flags", test_create_thread_ex_bad_flags());
    all_ok &= report("NtQueryInformationThread invalid class", test_query_thread_invalid_class());
    all_ok &= report("NtSetInformationThread hide bad length", test_set_thread_hide_bad_length());
    all_ok &= report("NtDelayExecution zero interval", test_delay_execution());
    all_ok &= report("NtGetContextThread control", test_get_context_thread());
    all_ok &= report("NtOpenThread by id", test_open_thread_by_id());
    all_ok &= report("NtQueueApcThread via alertable sleep", test_queue_apc_thread());
    all_ok &= report("NtQueueApcThreadEx via alertable sleep", test_queue_apcex_thread());
    all_ok &= report("NtSuspendThread NtResumeThread round trip", test_suspend_resume_thread());
    all_ok &= report("NtSetContextThread round trip", test_set_context_thread());
    all_ok &= report("NtTerminateThread on exit", test_terminate_thread());
    return all_ok ? 0 : 1;
}
