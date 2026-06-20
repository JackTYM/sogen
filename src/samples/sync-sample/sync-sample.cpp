#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>

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
}

int main()
{
    bool ok = true;
    ok &= report("Event set/wait", test_event_set_wait());
    ok &= report("Event reset", test_event_reset());
    ok &= report("Mutex acquire/release", test_mutex());
    ok &= report("Semaphore create validation", test_semaphore_create());
    ok &= report("Semaphore count", test_semaphore());
    ok &= report("Semaphore release count", test_semaphore_release_count());
    ok &= report("Semaphore named", test_semaphore_named());
    ok &= report("WaitForMultipleObjects", test_wait_multiple());
    ok &= report("Waitable timer", test_waitable_timer());
    return ok ? 0 : 1;
}
