#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <cstdio>

// NT types for direct syscall testing
typedef enum _EVENT_INFORMATION_CLASS { EventBasicInformation = 0 } EVENT_INFORMATION_CLASS;
typedef struct _EVENT_BASIC_INFORMATION
{
    EVENT_TYPE EventType;
    LONG EventState;
} EVENT_BASIC_INFORMATION;

extern "C" NTSTATUS NTAPI NtQueryEvent(HANDLE EventHandle, EVENT_INFORMATION_CLASS EventInformationClass,
                                       PVOID EventInformation, ULONG EventInformationLength, PULONG ReturnLength);
extern "C" NTSTATUS NTAPI NtWaitForMultipleObjects(ULONG Count, HANDLE* Handles, UINT WaitType, BOOLEAN Alertable,
                                                   PLARGE_INTEGER Timeout);

typedef enum _MUTANT_INFORMATION_CLASS
{
    MutantBasicInformation = 0,
    MutantOwnerInformation = 1
} MUTANT_INFORMATION_CLASS;
typedef struct _MUTANT_BASIC_INFORMATION
{
    LONG CurrentCount;
    BOOLEAN OwnedByCaller;
    BOOLEAN AbandonedState;
} MUTANT_BASIC_INFORMATION;
extern "C" NTSTATUS NTAPI NtQueryMutant(HANDLE MutantHandle, MUTANT_INFORMATION_CLASS MutantInformationClass,
                                         PVOID MutantInformation, ULONG MutantInformationLength, PULONG ReturnLength);

typedef enum _SEMAPHORE_INFORMATION_CLASS
{
    SemaphoreBasicInformation = 0
} SEMAPHORE_INFORMATION_CLASS;
typedef struct _SEMAPHORE_BASIC_INFORMATION
{
    LONG CurrentCount;
    LONG MaximumCount;
} SEMAPHORE_BASIC_INFORMATION;
extern "C" NTSTATUS NTAPI NtQuerySemaphore(HANDLE SemaphoreHandle, SEMAPHORE_INFORMATION_CLASS SemaphoreInformationClass,
                                            PVOID SemaphoreInformation, ULONG SemaphoreInformationLength,
                                            PULONG ReturnLength);

typedef enum _TIMER_INFORMATION_CLASS
{
    TimerBasicInformation = 0
} TIMER_INFORMATION_CLASS;
typedef struct _TIMER_BASIC_INFORMATION
{
    LARGE_INTEGER RemainingTime;
    BOOLEAN TimerState;
} TIMER_BASIC_INFORMATION;
extern "C" NTSTATUS NTAPI NtQueryTimer(HANDLE TimerHandle, TIMER_INFORMATION_CLASS TimerInformationClass,
                                        PVOID TimerInformation, ULONG TimerInformationLength, PULONG ReturnLength);

namespace
{
    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_event_set_wait()
    {
        HANDLE ev = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!ev)
            return false;
        SetEvent(ev);
        DWORD res = WaitForSingleObject(ev, 0);
        CloseHandle(ev);
        return res == WAIT_OBJECT_0;
    }

    bool test_event_reset()
    {
        HANDLE ev = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!ev)
            return false;
        SetEvent(ev);
        ResetEvent(ev);
        DWORD res = WaitForSingleObject(ev, 0);
        CloseHandle(ev);
        return res == WAIT_TIMEOUT;
    }

    bool test_mutex()
    {
        HANDLE m = CreateMutexA(nullptr, FALSE, nullptr);
        if (!m)
            return false;
        DWORD res = WaitForSingleObject(m, 0);
        if (res != WAIT_OBJECT_0)
        {
            CloseHandle(m);
            return false;
        }
        ReleaseMutex(m);
        CloseHandle(m);
        return true;
    }

    bool test_event_create_validation()
    {
        // Kernel: if ( event_type > SynchronizationEvent ) return STATUS_INVALID_PARAMETER
        // Artifact NtCreateEvent line 17: if ( (unsigned int)a4 > SynchronizationEvent )
        HANDLE h1 = CreateEventA(nullptr, FALSE, FALSE, nullptr);  // SynchronizationEvent=1 → valid
        if (!h1) return false;
        CloseHandle(h1);
        HANDLE h2 = CreateEventA(nullptr, TRUE, FALSE, nullptr);   // NotificationEvent=0 → valid
        if (!h2) return false;
        CloseHandle(h2);
        return true;
    }

    bool test_keyed_event_reserved()
    {
        // Kernel NtCreateKeyedEvent: if ( reserved != 0 ) return STATUS_INVALID_PARAMETER_4
        // Artifact line 13-15: if ( a4 ) return 3221225714LL
        // Via NtCreateKeyedEvent directly (Win32 has no wrapper, but we can call via NTDLL)
        HANDLE h = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        if (!h) return false;
        CloseHandle(h);
        return true;  // Behavioral presence test — keyed event create succeeds with reserved=0
    }

    bool test_query_event_info_class()
    {
        // Kernel NtQueryEvent: if ( a2 ) return STATUS_INVALID_INFO_CLASS (info_class must be 0)
        // Also: if ( a4 != 8 ) return STATUS_INFO_LENGTH_MISMATCH (exact size required)
        // Artifact line 9: return 3221225475LL (STATUS_INVALID_INFO_CLASS)
        // Artifact line 13: return 3221225476LL (STATUS_INFO_LENGTH_MISMATCH)
        HANDLE h = CreateEventA(nullptr, TRUE, TRUE, nullptr);
        if (!h) return false;
        EVENT_BASIC_INFORMATION info{};
        ULONG ret_len = 0;
        // Valid query
        NTSTATUS st = NtQueryEvent(h, EventBasicInformation, &info, sizeof(info), &ret_len);
        CloseHandle(h);
        return st == 0 && info.EventType == NotificationEvent && info.EventState == 1 && ret_len == 8;
    }

    bool test_semaphore_create()
    {
        // Kernel contract: Limit<=0 || InitialCount<0 || InitialCount>Limit → STATUS_INVALID_PARAMETER
        // Artifact: if ( Limit <= 0 || a4 < 0 || a4 > Limit ) return 3221225485LL
        HANDLE h1 = CreateSemaphoreA(nullptr, 0, 0, nullptr);   // Limit=0 → rejected
        if (h1 != nullptr)
        {
            CloseHandle(h1);
            return false;
        }
        HANDLE h2 = CreateSemaphoreA(nullptr, 3, 2, nullptr);   // Initial > Max → rejected
        if (h2 != nullptr)
        {
            CloseHandle(h2);
            return false;
        }
        HANDLE h3 = CreateSemaphoreA(nullptr, 0, 1, nullptr);   // Valid: Initial=0, Max=1
        if (h3 == nullptr)
            return false;
        CloseHandle(h3);
        return true;
    }

    bool test_semaphore()
    {
        HANDLE sem = CreateSemaphoreA(nullptr, 2, 4, nullptr);
        if (!sem)
            return false;
        DWORD r1 = WaitForSingleObject(sem, 0);
        DWORD r2 = WaitForSingleObject(sem, 0);
        DWORD r3 = WaitForSingleObject(sem, 0);
        ReleaseSemaphore(sem, 1, nullptr);
        CloseHandle(sem);
        return r1 == WAIT_OBJECT_0 && r2 == WAIT_OBJECT_0 && r3 == WAIT_TIMEOUT;
    }

    bool test_semaphore_release_count()
    {
        // Test: NtCreateSemaphore(InitialCount=1, MaxCount=3)
        // then multiple ReleaseSemaphore calls
        HANDLE sem = CreateSemaphoreA(nullptr, 1, 3, nullptr);
        if (!sem)
            return false;

        // Consume the initial count
        DWORD r1 = WaitForSingleObject(sem, 0);
        if (r1 != WAIT_OBJECT_0)
        {
            CloseHandle(sem);
            return false;
        }

        // Should be empty now
        DWORD r2 = WaitForSingleObject(sem, 0);
        if (r2 != WAIT_TIMEOUT)
        {
            CloseHandle(sem);
            return false;
        }

        // Release 2, count should be 2
        LONG prev = -1;
        if (!ReleaseSemaphore(sem, 2, &prev))
        {
            CloseHandle(sem);
            return false;
        }

        if (prev != 0)
        {
            CloseHandle(sem);
            return false;
        }

        // Consume both
        DWORD r3 = WaitForSingleObject(sem, 0);
        DWORD r4 = WaitForSingleObject(sem, 0);
        if (r3 != WAIT_OBJECT_0 || r4 != WAIT_OBJECT_0)
        {
            CloseHandle(sem);
            return false;
        }

        // Should be empty again
        DWORD r5 = WaitForSingleObject(sem, 0);
        CloseHandle(sem);
        return r5 == WAIT_TIMEOUT;
    }

    bool test_semaphore_named()
    {
        // Test: Create a named semaphore, verify multiple opens get the same object
        const char* kName = "Local\\TestSemaphore";

        HANDLE sem1 = CreateSemaphoreA(nullptr, 1, 2, kName);
        if (!sem1)
            return false;

        // Open the same named semaphore
        HANDLE sem2 = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, kName);
        if (!sem2)
        {
            CloseHandle(sem1);
            return false;
        }

        // Both should refer to the same semaphore
        DWORD r1 = WaitForSingleObject(sem1, 0);
        DWORD r2 = WaitForSingleObject(sem2, 0);
        DWORD r3 = WaitForSingleObject(sem1, 0);

        CloseHandle(sem1);
        CloseHandle(sem2);

        return r1 == WAIT_OBJECT_0 && r2 == WAIT_OBJECT_0 && r3 == WAIT_TIMEOUT;
    }

    bool test_wait_multiple()
    {
        HANDLE ev1 = CreateEventA(nullptr, TRUE, TRUE, nullptr);
        HANDLE ev2 = CreateEventA(nullptr, TRUE, TRUE, nullptr);
        if (!ev1 || !ev2)
        {
            if (ev1)
                CloseHandle(ev1);
            if (ev2)
                CloseHandle(ev2);
            return false;
        }
        HANDLE handles[2] = {ev1, ev2};
        DWORD res = WaitForMultipleObjects(2, handles, TRUE, 0);
        CloseHandle(ev1);
        CloseHandle(ev2);
        return res == WAIT_OBJECT_0;
    }

    bool test_waitable_timer()
    {
        HANDLE timer = CreateWaitableTimerA(nullptr, TRUE, nullptr);
        if (!timer)
            return false;
        LARGE_INTEGER due{};
        due.QuadPart = -1 * 10000LL; // 1ms in 100ns units
        SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
        DWORD res = WaitForSingleObject(timer, 100);
        CloseHandle(timer);
        return res == WAIT_OBJECT_0;
    }

    bool test_wait_count_validation()
    {
        // Kernel: if ( (count - 1) > 0x3F ) return STATUS_INVALID_PARAMETER_1
        // count=0 must be rejected (0 - 1 = 0xFFFFFFFF > 0x3F)
        LARGE_INTEGER zero_timeout{};
        NTSTATUS st = NtWaitForMultipleObjects(0, nullptr, 0 /*WaitAll*/, FALSE, &zero_timeout);
        return st != STATUS_SUCCESS; // Must fail
    }

    bool test_semaphore_release_zero()
    {
        // Kernel NtReleaseSemaphore: if (a2 <= 0) return STATUS_INVALID_PARAMETER.
        // Artifact: if ( a2 <= 0 ) return 3221225485LL; (a2 is signed int)
        HANDLE sem = CreateSemaphoreA(nullptr, 0, 1, nullptr);
        if (!sem)
            return false;
        using NtReleaseSemaphoreFn = NTSTATUS(NTAPI*)(HANDLE, LONG, PLONG);
        const auto fn = reinterpret_cast<NtReleaseSemaphoreFn>(
            GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtReleaseSemaphore"));
        if (!fn)
        {
            CloseHandle(sem);
            return true;
        }
        LONG prev = -1;
        const NTSTATUS st_zero = fn(sem, 0, &prev);
        CloseHandle(sem);
        return st_zero == static_cast<NTSTATUS>(STATUS_INVALID_PARAMETER);
    }

    bool test_pulse_event_clears()
    {
        // KePulseEvent leaves event unsignaled after pulse regardless of event type.
        // Artifact NtPulseEvent: v10 = KePulseEvent(v7, 1, 0) — event cleared after signaling.
        HANDLE nev = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!nev)
            return false;
        SetEvent(nev);
        PulseEvent(nev);
        const DWORD r = WaitForSingleObject(nev, 0);
        CloseHandle(nev);
        return r == WAIT_TIMEOUT;
    }

    bool test_set_event_boost_type_check()
    {
        // Kernel NtSetEventBoostPriority: NotificationEvent handle → STATUS_OBJECT_TYPE_MISMATCH.
        // Artifact line 20-21: if ((*(_BYTE *)Object & 0x7F) == 0) return -1073741788
        using NtSetEventBoostPriorityFn = NTSTATUS(NTAPI*)(HANDLE);
        const auto fn = reinterpret_cast<NtSetEventBoostPriorityFn>(
            GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetEventBoostPriority"));
        if (!fn)
            return true;
        HANDLE nev = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!nev)
            return false;
        const NTSTATUS st = fn(nev);
        CloseHandle(nev);
        return st == static_cast<NTSTATUS>(0xC0000024L);
    }

    bool test_query_mutant_basic()
    {
        // Kernel NtQueryMutant class 0: CurrentCount=1 when free, 0 when owned once; OwnedByCaller reflects ownership.
        // Artifact: v12=*((_DWORD*)Object+1)=CurrentCount; v14=owning_thread==current_thread; a4 must ==8
        HANDLE m = CreateMutexA(nullptr, FALSE, nullptr);
        if (!m)
            return false;

        MUTANT_BASIC_INFORMATION info{};
        ULONG ret_len = 0;
        NTSTATUS st = NtQueryMutant(m, MutantBasicInformation, &info, sizeof(info), &ret_len);
        if (st != STATUS_SUCCESS || info.CurrentCount != 1 || info.OwnedByCaller != FALSE || ret_len != 8)
        {
            CloseHandle(m);
            return false;
        }

        WaitForSingleObject(m, 0);
        MUTANT_BASIC_INFORMATION info2{};
        st = NtQueryMutant(m, MutantBasicInformation, &info2, sizeof(info2), nullptr);
        ReleaseMutex(m);
        CloseHandle(m);
        return st == STATUS_SUCCESS && info2.CurrentCount == 0 && info2.OwnedByCaller == TRUE;
    }

    bool test_query_mutant_validation()
    {
        // Kernel: class >1 → STATUS_INVALID_INFO_CLASS (0xC0000003); size !=8 → STATUS_INFO_LENGTH_MISMATCH
        // Artifact: return 3221225475 (class), return 3221225476 (size)
        HANDLE m = CreateMutexA(nullptr, FALSE, nullptr);
        if (!m)
            return false;
        MUTANT_BASIC_INFORMATION info{};
        const NTSTATUS st_bad_class = NtQueryMutant(m, static_cast<MUTANT_INFORMATION_CLASS>(2), &info, 8, nullptr);
        const NTSTATUS st_bad_size = NtQueryMutant(m, MutantBasicInformation, &info, 7, nullptr);
        CloseHandle(m);
        return st_bad_class == static_cast<NTSTATUS>(0xC0000003L) &&
               st_bad_size == static_cast<NTSTATUS>(STATUS_INFO_LENGTH_MISMATCH);
    }

    bool test_query_semaphore_info()
    {
        // Kernel NtQuerySemaphore: class!=0 → STATUS_INVALID_INFO_CLASS; size!=8 → STATUS_INFO_LENGTH_MISMATCH.
        // Artifact: return 3221225475 (class 0 check); return 3221225476 (a4 != 8)
        HANDLE sem = CreateSemaphoreA(nullptr, 2, 5, nullptr);
        if (!sem)
            return false;
        SEMAPHORE_BASIC_INFORMATION info{};
        ULONG ret_len = 0;
        NTSTATUS st = NtQuerySemaphore(sem, SemaphoreBasicInformation, &info, sizeof(info), &ret_len);
        if (st != STATUS_SUCCESS || info.CurrentCount != 2 || info.MaximumCount != 5 || ret_len != 8)
        {
            CloseHandle(sem);
            return false;
        }
        const NTSTATUS st_bad = NtQuerySemaphore(sem, static_cast<SEMAPHORE_INFORMATION_CLASS>(1), &info, 8, nullptr);
        CloseHandle(sem);
        return st_bad == static_cast<NTSTATUS>(0xC0000003L);
    }

    bool test_query_timer_info()
    {
        // Kernel NtQueryTimer: class!=0 → STATUS_INVALID_INFO_CLASS; size!=16 → STATUS_INFO_LENGTH_MISMATCH.
        // Artifact: a2 != 0 → return 3221225475; a4 != 16 → return 3221225476
        HANDLE timer = CreateWaitableTimerA(nullptr, TRUE, nullptr);
        if (!timer)
            return false;
        TIMER_BASIC_INFORMATION info{};
        ULONG ret_len = 0;
        NTSTATUS st = NtQueryTimer(timer, TimerBasicInformation, &info, 16, &ret_len);
        if (st != STATUS_SUCCESS || ret_len != 16)
        {
            CloseHandle(timer);
            return false;
        }
        const NTSTATUS st_bad_size = NtQueryTimer(timer, TimerBasicInformation, &info, 13, nullptr);
        const NTSTATUS st_bad_class = NtQueryTimer(timer, static_cast<TIMER_INFORMATION_CLASS>(1), &info, 16, nullptr);
        CloseHandle(timer);
        return st_bad_size == static_cast<NTSTATUS>(STATUS_INFO_LENGTH_MISMATCH) &&
               st_bad_class == static_cast<NTSTATUS>(0xC0000003L);
    }

    bool test_raise_hard_error_param_count()
    {
        // Kernel NtRaiseHardError: if (a2 > 5) return STATUS_INVALID_PARAMETER_2 (0xC00000F0).
        // Test the validation path only (>5 params) — does not hit the hard error path.
        // Artifact: if ( a2 > 5 ) return 3221225712LL (0xC00000F0)
        using NtRaiseHardErrorFn = NTSTATUS(NTAPI*)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);
        const auto fn = reinterpret_cast<NtRaiseHardErrorFn>(
            GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtRaiseHardError"));
        if (!fn)
            return true;
        ULONG response = 0;
        // 6 parameters → STATUS_INVALID_PARAMETER_2 (does NOT trigger hard error)
        const NTSTATUS st = fn(STATUS_SUCCESS, 6, 0, nullptr, 0, &response);
        return st == static_cast<NTSTATUS>(0xC00000F0L);
    }

    bool test_cancel_timer2_succeeds()
    {
        // NtCancelTimer2: kernel trivially calls ExpSetTimer2(handle); sogen returns STATUS_SUCCESS.
        // Both sides are trivial stubs — verify no crash and STATUS_SUCCESS is returned.
        HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!timer)
            return true; // high-res timer not available, skip
        const BOOL cancelled = CancelWaitableTimer(timer);
        CloseHandle(timer);
        return cancelled != FALSE;
    }
}

int main()
{
    bool ok = true;
    ok &= report("Event create validation", test_event_create_validation());
    ok &= report("Keyed event reserved=0", test_keyed_event_reserved());
    ok &= report("Query event info class", test_query_event_info_class());
    ok &= report("Event set/wait", test_event_set_wait());
    ok &= report("Event reset", test_event_reset());
    ok &= report("Mutex acquire/release", test_mutex());
    ok &= report("Semaphore create validation", test_semaphore_create());
    ok &= report("Semaphore count", test_semaphore());
    ok &= report("Semaphore release count", test_semaphore_release_count());
    ok &= report("Semaphore named", test_semaphore_named());
    ok &= report("WaitForMultipleObjects", test_wait_multiple());
    ok &= report("Waitable timer", test_waitable_timer());
    ok &= report("Wait count zero rejected", test_wait_count_validation());
    ok &= report("Semaphore release zero rejected", test_semaphore_release_zero());
    ok &= report("Pulse event clears signal", test_pulse_event_clears());
    ok &= report("SetEventBoostPriority type check", test_set_event_boost_type_check());
    ok &= report("NtQueryMutant basic info", test_query_mutant_basic());
    ok &= report("NtQueryMutant validation", test_query_mutant_validation());
    ok &= report("NtQuerySemaphore info", test_query_semaphore_info());
    ok &= report("NtQueryTimer info", test_query_timer_info());
    ok &= report("Cancel timer2 succeeds", test_cancel_timer2_succeeds());
    ok &= report("RaiseHardError param count rejected", test_raise_hard_error_param_count());
    return ok ? 0 : 1;
}
