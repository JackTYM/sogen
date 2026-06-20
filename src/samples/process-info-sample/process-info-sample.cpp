#include <cstdio>
#include <string>
#include <array>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

extern "C"
{
    typedef LONG NTSTATUS;
    typedef NTSTATUS(NTAPI* NtSuspendProcessFn)(HANDLE ProcessHandle);
    typedef NTSTATUS(NTAPI* NtResumeProcessFn)(HANDLE ProcessHandle);
    typedef NTSTATUS(NTAPI* NtQueryInformationProcessFn)(HANDLE, int, PVOID, ULONG, PULONG);
    typedef NTSTATUS(NTAPI* NtSetInformationProcessFn)(HANDLE, int, PVOID, ULONG);
    typedef NTSTATUS(NTAPI* NtOpenProcessFn)(PHANDLE, ACCESS_MASK, PVOID, PVOID);
}

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
        return ph != nullptr; // STATUS_SUCCESS: handle must be valid
    }

    bool test_suspend_process_invalid_handle()
    {
        // NtSuspendProcess: kernel calls ObpReferenceObjectByHandleWithTag before PsSuspendProcess.
        // A bogus handle (0x1234) that doesn't exist in the process → STATUS_INVALID_HANDLE (0xC0000008).
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn = reinterpret_cast<NtSuspendProcessFn>(GetProcAddress(ntdll, "NtSuspendProcess"));
        if (!fn)
            return false;
        const NTSTATUS status = fn(reinterpret_cast<HANDLE>(0x1234));
        return status == static_cast<NTSTATUS>(0xC0000008);
    }

    bool test_resume_process_invalid_handle()
    {
        // NtResumeProcess: kernel calls ObpReferenceObjectByHandleWithTag before PsMultiResumeProcess.
        // A bogus handle (0x1234) that doesn't exist in the process → STATUS_INVALID_HANDLE (0xC0000008).
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn = reinterpret_cast<NtResumeProcessFn>(GetProcAddress(ntdll, "NtResumeProcess"));
        if (!fn)
            return false;
        const NTSTATUS status = fn(reinterpret_cast<HANDLE>(0x1234));
        return status == static_cast<NTSTATUS>(0xC0000008);
    }

    bool test_query_process_bad_handle()
    {
        // NtQueryInformationProcess: kernel does ObpReferenceObjectByHandleWithTag first.
        // Bogus handle (0x1234) → STATUS_INVALID_HANDLE (0xC0000008).
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn =
            reinterpret_cast<NtQueryInformationProcessFn>(GetProcAddress(ntdll, "NtQueryInformationProcess"));
        if (!fn)
            return false;
        char buf[64]{};
        ULONG ret = 0;
        const NTSTATUS status = fn(reinterpret_cast<HANDLE>(0x1234), 0 /*ProcessBasicInformation*/, buf, sizeof(buf), &ret);
        return status == static_cast<NTSTATUS>(0xC0000008);
    }

    bool test_query_process_invalid_class()
    {
        // NtQueryInformationProcess: unknown info class → STATUS_INVALID_INFO_CLASS (0xC0000003).
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn =
            reinterpret_cast<NtQueryInformationProcessFn>(GetProcAddress(ntdll, "NtQueryInformationProcess"));
        if (!fn)
            return false;
        char buf[64]{};
        ULONG ret = 0;
        const NTSTATUS status = fn(GetCurrentProcess(), 99 /*invalid class*/, buf, sizeof(buf), &ret);
        return status == static_cast<NTSTATUS>(0xC0000003);
    }

    bool test_terminate_process_null_handle()
    {
        // NtTerminateProcess(NULL, code): kernel calls PsTerminateMinimalProcess; terminates all
        // other threads without terminating the current one.
        // Calling with NULL and STATUS_SUCCESS should return STATUS_SUCCESS.
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        typedef NTSTATUS(NTAPI* NtTerminateProcessFn)(HANDLE, LONG);
        const auto fn = reinterpret_cast<NtTerminateProcessFn>(GetProcAddress(ntdll, "NtTerminateProcess"));
        if (!fn)
            return false;
        const NTSTATUS status = fn(nullptr, 0 /*STATUS_SUCCESS*/);
        return status == static_cast<NTSTATUS>(0);  // STATUS_SUCCESS
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
    all_ok &= report("SuspendProcess invalid handle", test_suspend_process_invalid_handle());
    all_ok &= report("ResumeProcess invalid handle", test_resume_process_invalid_handle());
    all_ok &= report("QueryInformationProcess bad handle", test_query_process_bad_handle());
    all_ok &= report("QueryInformationProcess invalid class", test_query_process_invalid_class());
    all_ok &= report("NtTerminateProcess null handle", test_terminate_process_null_handle());
    return all_ok ? 0 : 1;
}
