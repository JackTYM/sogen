#include <cstdio>
#include <string>
#include <array>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

namespace
{
    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_process_times()
    {
        FILETIME create_time{}, exit_time{}, kernel_time{}, user_time{};
        return GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time) != 0;
    }

    bool test_process_memory_info()
    {
        PROCESS_MEMORY_COUNTERS pmc{};
        pmc.cb = sizeof(pmc);
        return GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)) != 0;
    }

    bool test_process_image_name()
    {
        std::array<char, MAX_PATH> name{};
        DWORD len = static_cast<DWORD>(name.size());
        return QueryFullProcessImageNameA(GetCurrentProcess(), 0, name.data(), &len) != 0;
    }

    bool test_process_affinity()
    {
        DWORD_PTR proc_mask{}, sys_mask{};
        return GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask) != 0 && proc_mask != 0;
    }

    bool test_process_exit_code()
    {
        DWORD code{};
        return GetExitCodeProcess(GetCurrentProcess(), &code) != 0 && code == STILL_ACTIVE;
    }

    bool test_process_handle_count()
    {
        DWORD count{};
        return GetProcessHandleCount(GetCurrentProcess(), &count) != 0;
    }

    bool test_process_heap()
    {
        HANDLE heap = GetProcessHeap();
        if (!heap)
        {
            return false;
        }

        void* ptr = HeapAlloc(heap, 0, 128);
        if (!ptr)
        {
            return false;
        }

        *static_cast<char*>(ptr) = 'x';
        HeapFree(heap, 0, ptr);
        return true;
    }

    bool test_process_open_token()
    {
        // NtOpenProcessToken: kernel opens process token; sogen returns CURRENT_PROCESS_TOKEN
        HANDLE token{};
        const BOOL ok = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
        if (ok && token)
            CloseHandle(token);
        return ok != 0;
    }

    bool test_open_process_self()
    {
        // NtOpenProcess: kernel calls PsOpenProcess; sogen matches current PID → returns process handle
        HANDLE ph = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
        if (ph)
            CloseHandle(ph);
        return ph != nullptr;
    }
}

int main()
{
    bool all_ok = true;
    all_ok &= report("Process heap alloc/free", test_process_heap());
    all_ok &= report("Process exit code", test_process_exit_code());
    all_ok &= report("Process affinity mask", test_process_affinity());
    all_ok &= report("Process image name", test_process_image_name());
    all_ok &= report("Process times", test_process_times());
    all_ok &= report("Process memory info", test_process_memory_info());
    all_ok &= report("Process handle count", test_process_handle_count());
    all_ok &= report("OpenProcessToken", test_process_open_token());
    all_ok &= report("OpenProcess self", test_open_process_self());
    return all_ok ? 0 : 1;
}
