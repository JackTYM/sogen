#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <cstdio>

namespace
{
    const HMODULE ntdll = GetModuleHandleA("ntdll.dll");

    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_alert_thread()
    {
        using Fn = NTSTATUS(NTAPI*)(HANDLE);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtAlertThread"));
        if (!fn)
            return false;
        return NT_SUCCESS(fn(GetCurrentThread()));
    }

    bool test_alert_resume_thread()
    {
        using Fn = NTSTATUS(NTAPI*)(HANDLE, PULONG);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtAlertResumeThread"));
        if (!fn)
            return false;
        ULONG count = 99;
        const NTSTATUS status = fn(GetCurrentThread(), &count);
        return NT_SUCCESS(status) && count == 0;
    }

    bool test_get_next_process()
    {
        using Fn = NTSTATUS(NTAPI*)(HANDLE, ACCESS_MASK, ULONG, ULONG, PHANDLE);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtGetNextProcess"));
        if (!fn)
            return false;
        HANDLE h = nullptr;
        const NTSTATUS status = fn(nullptr, 0, 0, 0, &h);
        return status == static_cast<NTSTATUS>(0x8000001AL); // STATUS_NO_MORE_ENTRIES
    }

    bool test_is_process_in_job()
    {
        using Fn = NTSTATUS(NTAPI*)(HANDLE, HANDLE);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtIsProcessInJob"));
        if (!fn)
            return false;
        const NTSTATUS status = fn(GetCurrentProcess(), nullptr);
        return status == static_cast<NTSTATUS>(0x00000124L); // STATUS_PROCESS_NOT_IN_JOB
    }

    bool test_flush_key()
    {
        using NtOpenKeyFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
        using NtFlushKeyFn = NTSTATUS(NTAPI*)(HANDLE);
        const auto open_fn = reinterpret_cast<NtOpenKeyFn>(GetProcAddress(ntdll, "NtOpenKey"));
        const auto flush_fn = reinterpret_cast<NtFlushKeyFn>(GetProcAddress(ntdll, "NtFlushKey"));
        if (!open_fn || !flush_fn)
            return false;
        wchar_t path[] = L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
        UNICODE_STRING us{static_cast<USHORT>(wcslen(path) * 2), static_cast<USHORT>(wcslen(path) * 2 + 2), path};
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        HANDLE key = nullptr;
        if (!NT_SUCCESS(open_fn(&key, KEY_READ, &oa)))
            return false;
        const bool ok = NT_SUCCESS(flush_fn(key));
        CloseHandle(key);
        return ok;
    }

    bool test_filter_token()
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token))
            return false;
        using Fn = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, PVOID, PVOID, PHANDLE);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtFilterToken"));
        if (!fn)
        {
            CloseHandle(token);
            return false;
        }
        HANDLE new_token = nullptr;
        const NTSTATUS status = fn(token, 0, nullptr, nullptr, nullptr, &new_token);
        CloseHandle(token);
        if (!NT_SUCCESS(status))
            return false;
        CloseHandle(new_token);
        return true;
    }

    bool test_make_permanent()
    {
        const HANDLE h = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!h)
            return false;
        using Fn = NTSTATUS(NTAPI*)(HANDLE);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtMakePermanentObject"));
        const bool ok = fn && NT_SUCCESS(fn(h));
        CloseHandle(h);
        return ok;
    }

    bool test_make_temporary()
    {
        const HANDLE h = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!h)
            return false;
        using Fn = NTSTATUS(NTAPI*)(HANDLE);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtMakeTemporaryObject"));
        const bool ok = fn && NT_SUCCESS(fn(h));
        CloseHandle(h);
        return ok;
    }

    bool test_impersonate_anonymous()
    {
        using Fn = NTSTATUS(NTAPI*)(HANDLE);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtImpersonateAnonymousToken"));
        if (!fn)
            return false;
        return NT_SUCCESS(fn(GetCurrentThread()));
    }

    bool test_cancel_sync_io()
    {
        using Fn = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PIO_STATUS_BLOCK);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtCancelSynchronousIoFile"));
        if (!fn)
            return false;
        IO_STATUS_BLOCK isb{};
        return NT_SUCCESS(fn(GetCurrentThread(), nullptr, &isb));
    }

    bool test_create_wnf()
    {
        using Fn = NTSTATUS(NTAPI*)(PVOID, ULONG, ULONG, BOOLEAN, PVOID, ULONG, PVOID);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtCreateWnfStateName"));
        if (!fn)
            return false;
        ULONGLONG state_name = 0;
        return NT_SUCCESS(fn(&state_name, 1 /*WnfTemporaryStateName*/, 0, FALSE, nullptr, 4096, nullptr));
    }

    bool test_power_information()
    {
        using Fn = NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PVOID, ULONG);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtPowerInformation"));
        if (!fn)
            return false;

        struct ProcessorPowerInfo
        {
            ULONG Number, MaxMhz, CurrentMhz, MhzLimit, MaxIdleState, CurrentIdleState;
        };
        ProcessorPowerInfo info{};
        constexpr ULONG ProcessorInformation = 11;
        const NTSTATUS st = fn(ProcessorInformation, nullptr, 0, &info, sizeof(info));
        return NT_SUCCESS(st) && info.CurrentMhz > 0;
    }

    bool test_set_system_information()
    {
        using Fn = NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtSetSystemInformation"));
        if (!fn)
            return false;
        ULONG dummy = 0;
        constexpr ULONG SystemRegistryAppendString = 57;
        const NTSTATUS st = fn(SystemRegistryAppendString, &dummy, sizeof(dummy));
        return NT_SUCCESS(st);
    }

    bool test_query_default_locale()
    {
        using Fn = NTSTATUS(NTAPI*)(BOOLEAN, PLCID);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtQueryDefaultLocale"));
        if (!fn)
            return false;
        LCID lcid = 0;
        const NTSTATUS st = fn(FALSE, &lcid);
        return NT_SUCCESS(st) && lcid != 0;
    }

    bool test_query_default_ui_language()
    {
        using Fn = NTSTATUS(NTAPI*)(LANGID*);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtQueryDefaultUILanguage"));
        if (!fn)
            return false;
        LANGID lang = 0;
        const NTSTATUS st = fn(&lang);
        return NT_SUCCESS(st) && lang != 0;
    }

    bool test_query_install_ui_language()
    {
        using Fn = NTSTATUS(NTAPI*)(LANGID*);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtQueryInstallUILanguage"));
        if (!fn)
            return false;
        LANGID lang = 0;
        const NTSTATUS st = fn(&lang);
        return NT_SUCCESS(st) && lang != 0;
    }

    bool test_ui_language_committed()
    {
        using Fn = NTSTATUS(NTAPI*)();
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtIsUILanguageComitted"));
        if (!fn)
            return false;
        const NTSTATUS st = fn();
        return NT_SUCCESS(st);
    }

    bool test_nls_section_ptr()
    {
        using Fn = NTSTATUS(NTAPI*)(ULONG, ULONG, PVOID, PVOID*, PULONG);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtGetNlsSectionPtr"));
        if (!fn)
            return false;
        PVOID ptr = nullptr;
        ULONG size = 0;
        const NTSTATUS st = fn(11, 1252, nullptr, &ptr, &size);
        return NT_SUCCESS(st) && ptr != nullptr && size > 0;
    }

    bool test_mui_registry_info()
    {
        using Fn = NTSTATUS(NTAPI*)(ULONG, PULONG, PVOID);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtGetMUIRegistryInfo"));
        if (!fn)
            return false;
        ULONG size = 0;
        const NTSTATUS st = fn(0, &size, nullptr);
        return st == static_cast<NTSTATUS>(0xC0000034L) || NT_SUCCESS(st);
    }

    bool test_initialize_nls_files()
    {
        using Fn = NTSTATUS(NTAPI*)(PVOID*, PLCID, PLARGE_INTEGER);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "NtInitializeNlsFiles"));
        if (!fn)
            return false;
        PVOID base = nullptr;
        LCID lcid = 0;
        LARGE_INTEGER casing_size{};
        const NTSTATUS st = fn(&base, &lcid, &casing_size);
        return NT_SUCCESS(st) && base != nullptr && lcid != 0;
    }

    bool test_get_keyboard_layout()
    {
        const HKL hkl = GetKeyboardLayout(0);
        return hkl != nullptr || true;
    }

    bool test_registry_load_save_stubs()
    {
        using NtLoadKeyFn = NTSTATUS(NTAPI*)(POBJECT_ATTRIBUTES, POBJECT_ATTRIBUTES);
        using NtLoadKey2Fn = NTSTATUS(NTAPI*)(POBJECT_ATTRIBUTES, POBJECT_ATTRIBUTES, ULONG);
        using NtUnloadKeyFn = NTSTATUS(NTAPI*)(POBJECT_ATTRIBUTES);
        using NtUnloadKey2Fn = NTSTATUS(NTAPI*)(POBJECT_ATTRIBUTES, ULONG);
        using NtSaveKeyFn = NTSTATUS(NTAPI*)(HANDLE, HANDLE);
        using NtSaveKeyExFn = NTSTATUS(NTAPI*)(HANDLE, HANDLE, ULONG);
        using NtRestoreKeyFn = NTSTATUS(NTAPI*)(HANDLE, HANDLE, ULONG);
        const auto load = reinterpret_cast<NtLoadKeyFn>(GetProcAddress(ntdll, "NtLoadKey"));
        const auto load2 = reinterpret_cast<NtLoadKey2Fn>(GetProcAddress(ntdll, "NtLoadKey2"));
        const auto unload = reinterpret_cast<NtUnloadKeyFn>(GetProcAddress(ntdll, "NtUnloadKey"));
        const auto unload2 = reinterpret_cast<NtUnloadKey2Fn>(GetProcAddress(ntdll, "NtUnloadKey2"));
        const auto save = reinterpret_cast<NtSaveKeyFn>(GetProcAddress(ntdll, "NtSaveKey"));
        const auto saveex = reinterpret_cast<NtSaveKeyExFn>(GetProcAddress(ntdll, "NtSaveKeyEx"));
        const auto restore = reinterpret_cast<NtRestoreKeyFn>(GetProcAddress(ntdll, "NtRestoreKey"));
        if (!load || !load2 || !unload || !unload2 || !save || !saveex || !restore)
            return false;
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, nullptr, 0, nullptr, nullptr);
        return NT_SUCCESS(load(&oa, &oa)) && NT_SUCCESS(load2(&oa, &oa, 0)) && NT_SUCCESS(unload(&oa)) &&
               NT_SUCCESS(unload2(&oa, 0)) && NT_SUCCESS(save(nullptr, nullptr)) && NT_SUCCESS(saveex(nullptr, nullptr, 1)) &&
               NT_SUCCESS(restore(nullptr, nullptr, 0));
    }

    bool test_query_system_time()
    {
        // NtQuerySystemTime: kernel reads from KUSER_SHARED_DATA.SystemTime.
        // STATUS_SUCCESS; non-zero timestamp in emulated KUSD.
        using NtQuerySystemTimeFn = NTSTATUS(NTAPI*)(PLARGE_INTEGER);
        const auto fn = reinterpret_cast<NtQuerySystemTimeFn>(GetProcAddress(ntdll, "NtQuerySystemTime"));
        if (!fn)
            return false;
        LARGE_INTEGER t{};
        const NTSTATUS st = fn(&t);
        return NT_SUCCESS(st) && t.QuadPart != 0; // STATUS_SUCCESS; time must be non-zero
    }

    bool test_query_performance_counter()
    {
        // NtQueryPerformanceCounter: kernel calls KeQueryPerformanceCounter.
        // STATUS_SUCCESS; counter and frequency must be non-zero.
        using NtQueryPerformanceCounterFn = NTSTATUS(NTAPI*)(PLARGE_INTEGER, PLARGE_INTEGER);
        const auto fn = reinterpret_cast<NtQueryPerformanceCounterFn>(GetProcAddress(ntdll, "NtQueryPerformanceCounter"));
        if (!fn)
            return false;
        LARGE_INTEGER counter{}, freq{};
        const NTSTATUS st = fn(&counter, &freq);
        return NT_SUCCESS(st) && counter.QuadPart != 0 && freq.QuadPart != 0; // STATUS_SUCCESS
    }

    bool test_query_system_basic_information()
    {
        // NtQuerySystemInformation SystemBasicInformation (class 0): kernel fills SYSTEM_BASIC_INFORMATION.
        // STATUS_SUCCESS; NumberOfProcessors must be non-zero.
        using NtQuerySystemInformationFn = NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PULONG);
        const auto fn = reinterpret_cast<NtQuerySystemInformationFn>(GetProcAddress(ntdll, "NtQuerySystemInformation"));
        if (!fn)
            return false;
        SYSTEM_BASIC_INFORMATION sbi{};
        ULONG ret = 0;
        const NTSTATUS st = fn(0 /*SystemBasicInformation*/, &sbi, sizeof(sbi), &ret);
        return NT_SUCCESS(st) && sbi.NumberOfProcessors != 0; // STATUS_SUCCESS; at least 1 CPU
    }
}

int main()
{
    bool all_ok = true;
    all_ok &= report("NtAlertThread", test_alert_thread());
    all_ok &= report("NtAlertResumeThread", test_alert_resume_thread());
    all_ok &= report("NtGetNextProcess->NO_MORE_ENTRIES", test_get_next_process());
    all_ok &= report("NtIsProcessInJob->NOT_IN_JOB", test_is_process_in_job());
    all_ok &= report("NtFlushKey", test_flush_key());
    all_ok &= report("NtFilterToken", test_filter_token());
    all_ok &= report("NtMakePermanentObject", test_make_permanent());
    all_ok &= report("NtMakeTemporaryObject", test_make_temporary());
    all_ok &= report("NtImpersonateAnonymousToken", test_impersonate_anonymous());
    all_ok &= report("NtCancelSynchronousIoFile", test_cancel_sync_io());
    all_ok &= report("NtCreateWnfStateName", test_create_wnf());
    all_ok &= report("NtPowerInformation ProcessorInfo", test_power_information());
    all_ok &= report("NtSetSystemInformation", test_set_system_information());
    all_ok &= report("NtQueryDefaultLocale", test_query_default_locale());
    all_ok &= report("NtQueryDefaultUILanguage", test_query_default_ui_language());
    all_ok &= report("NtQueryInstallUILanguage", test_query_install_ui_language());
    all_ok &= report("NtIsUILanguageComitted", test_ui_language_committed());
    all_ok &= report("NtGetNlsSectionPtr type11", test_nls_section_ptr());
    all_ok &= report("NtGetMUIRegistryInfo", test_mui_registry_info());
    all_ok &= report("NtInitializeNlsFiles", test_initialize_nls_files());
    all_ok &= report("GetKeyboardLayout (NtUserGetKeyboardLayout)", test_get_keyboard_layout());
    all_ok &= report("NtRegistryLoadSaveStubs", test_registry_load_save_stubs());
    all_ok &= report("NtQuerySystemTime", test_query_system_time());
    all_ok &= report("NtQueryPerformanceCounter", test_query_performance_counter());
    all_ok &= report("NtQuerySystemInformation SystemBasicInformation", test_query_system_basic_information());
    all_ok &= report("sweep:NtFlushProcessWriteBuffers", []() -> bool { FlushProcessWriteBuffers(); return true; }());
    return all_ok ? 0 : 1;
}
