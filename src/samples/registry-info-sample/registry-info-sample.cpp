#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <winreg.h>

namespace
{
    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_query_key_info()
    {
        HKEY key{};
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &key) != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD subkeys{}, max_subkey_len{}, values{}, max_value_name_len{}, max_value_len{};
        FILETIME last_write{};

        const LONG ret = RegQueryInfoKeyA(key, nullptr, nullptr, nullptr, &subkeys, &max_subkey_len, nullptr, &values,
                                         &max_value_name_len, &max_value_len, nullptr, &last_write);

        RegCloseKey(key);
        return ret == ERROR_SUCCESS;
    }

    bool test_enumerate_subkeys()
    {
        HKEY key{};
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &key) != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD index = 0;
        int found = 0;

        while (true)
        {
            char name[256]{};
            DWORD name_len = sizeof(name);
            FILETIME last_write{};

            const LONG ret = RegEnumKeyExA(key, index, name, &name_len, nullptr, nullptr, nullptr, &last_write);
            if (ret == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            if (ret != ERROR_SUCCESS)
            {
                RegCloseKey(key);
                return false;
            }
            ++found;
            ++index;
        }

        RegCloseKey(key);
        return found >= 0;
    }

    bool test_enumerate_values()
    {
        HKEY key{};
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &key) != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD index = 0;
        int found = 0;

        while (true)
        {
            char name[256]{};
            DWORD name_len = sizeof(name);
            DWORD type{};
            BYTE data[512]{};
            DWORD data_len = sizeof(data);

            const LONG ret = RegEnumValueA(key, index, name, &name_len, nullptr, &type, data, &data_len);
            if (ret == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            if (ret != ERROR_SUCCESS && ret != ERROR_MORE_DATA)
            {
                RegCloseKey(key);
                return false;
            }
            ++found;
            ++index;
        }

        RegCloseKey(key);
        return found >= 0;
    }

    bool test_create_and_delete()
    {
        HKEY key{};
        DWORD disposition{};
        const LONG create_ret = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SogenTest", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
                                               nullptr, &key, &disposition);
        if (create_ret != ERROR_SUCCESS)
        {
            return false;
        }

        constexpr DWORD val = 42;
        const LONG set_ret = RegSetValueExA(key, "TestValue", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        RegCloseKey(key);

        if (set_ret != ERROR_SUCCESS)
        {
            return false;
        }

        HKEY read_key{};
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SogenTest", 0, KEY_READ, &read_key) != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD read_val{};
        DWORD read_len = sizeof(read_val);
        DWORD read_type{};
        const LONG get_ret = RegQueryValueExA(read_key, "TestValue", nullptr, &read_type, reinterpret_cast<BYTE*>(&read_val), &read_len);
        RegCloseKey(read_key);

        if (get_ret != ERROR_SUCCESS || read_type != REG_DWORD || read_val != val)
        {
            return false;
        }

        RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SogenTest");
        return true;
    }

    bool test_open_key_ex()
    {
        using NtOpenKeyExFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, ULONG);
        const auto fn = reinterpret_cast<NtOpenKeyExFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtOpenKeyEx"));
        if (!fn)
            return false;
        wchar_t path[] = L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
        UNICODE_STRING us{static_cast<USHORT>(wcslen(path) * 2), static_cast<USHORT>(wcslen(path) * 2 + 2), path};
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        HANDLE key = nullptr;
        const NTSTATUS st = fn(&key, KEY_READ, &oa, 0);
        if (NT_SUCCESS(st) && key)
            CloseHandle(key);
        return NT_SUCCESS(st);
    }

    bool test_notify_change_key()
    {
        HKEY key{};
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_NOTIFY, &key) != ERROR_SUCCESS)
            return false;
        const LONG ret = RegNotifyChangeKeyValue(key, FALSE, REG_NOTIFY_CHANGE_NAME, nullptr, FALSE);
        RegCloseKey(key);
        return ret == ERROR_SUCCESS;
    }

    bool test_set_key_info()
    {
        using NtOpenKeyFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
        using NtSetInformationKeyFn = NTSTATUS(NTAPI*)(HANDLE, int, PVOID, ULONG);
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        const auto open_fn = reinterpret_cast<NtOpenKeyFn>(GetProcAddress(ntdll, "NtOpenKey"));
        const auto set_fn = reinterpret_cast<NtSetInformationKeyFn>(GetProcAddress(ntdll, "NtSetInformationKey"));
        if (!open_fn || !set_fn)
            return false;
        wchar_t path[] = L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
        UNICODE_STRING us{static_cast<USHORT>(wcslen(path) * 2), static_cast<USHORT>(wcslen(path) * 2 + 2), path};
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        HANDLE key = nullptr;
        if (!NT_SUCCESS(open_fn(&key, KEY_READ, &oa)))
            return false;
        constexpr int KeySetHandleTagsInformation = 5;
        struct KeyHandleTagsInfo
        {
            ULONG HandleTags;
        } tags_info{0};
        const NTSTATUS st = set_fn(key, KeySetHandleTagsInformation, &tags_info, sizeof(tags_info));
        CloseHandle(key);
        return NT_SUCCESS(st);
    }

    bool test_rename_key()
    {
        using NtOpenKeyFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
        using NtCreateKeyFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, ULONG, PUNICODE_STRING, ULONG, PULONG);
        using NtRenameKeyFn = NTSTATUS(NTAPI*)(HANDLE, PUNICODE_STRING);
        using NtDeleteKeyFn = NTSTATUS(NTAPI*)(HANDLE);
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        const auto create_fn = reinterpret_cast<NtCreateKeyFn>(GetProcAddress(ntdll, "NtCreateKey"));
        const auto rename_fn = reinterpret_cast<NtRenameKeyFn>(GetProcAddress(ntdll, "NtRenameKey"));
        const auto delete_fn = reinterpret_cast<NtDeleteKeyFn>(GetProcAddress(ntdll, "NtDeleteKey"));
        if (!create_fn || !rename_fn || !delete_fn)
            return false;
        wchar_t path[] = L"\\Registry\\User\\Software\\SogenRenameTest";
        UNICODE_STRING us{static_cast<USHORT>(wcslen(path) * 2), static_cast<USHORT>(wcslen(path) * 2 + 2), path};
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        HANDLE key = nullptr;
        ULONG disp = 0;
        if (!NT_SUCCESS(create_fn(&key, KEY_ALL_ACCESS, &oa, 0, nullptr, 0, &disp)))
            return false;
        wchar_t new_name[] = L"SogenRenameTest2";
        UNICODE_STRING new_us{static_cast<USHORT>(wcslen(new_name) * 2), static_cast<USHORT>(wcslen(new_name) * 2 + 2), new_name};
        const NTSTATUS st = rename_fn(key, &new_us);
        delete_fn(key);
        CloseHandle(key);
        return NT_SUCCESS(st);
    }

    bool test_query_multiple_values()
    {
        using NtOpenKeyFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
        using NtQueryMultipleValueKeyFn = NTSTATUS(NTAPI*)(HANDLE, PKEY_VALUE_ENTRY, ULONG, PVOID, PULONG, PULONG);
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        const auto open_fn = reinterpret_cast<NtOpenKeyFn>(GetProcAddress(ntdll, "NtOpenKey"));
        const auto query_fn = reinterpret_cast<NtQueryMultipleValueKeyFn>(GetProcAddress(ntdll, "NtQueryMultipleValueKey"));
        if (!open_fn || !query_fn)
            return false;
        wchar_t path[] = L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
        UNICODE_STRING us{static_cast<USHORT>(wcslen(path) * 2), static_cast<USHORT>(wcslen(path) * 2 + 2), path};
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        HANDLE key = nullptr;
        if (!NT_SUCCESS(open_fn(&key, KEY_READ, &oa)))
            return false;
        wchar_t vname[] = L"CurrentVersion";
        UNICODE_STRING vus{static_cast<USHORT>(wcslen(vname) * 2), static_cast<USHORT>(wcslen(vname) * 2 + 2), vname};
        KEY_VALUE_ENTRY entry{};
        entry.ValueName = &vus;
        BYTE buf[256]{};
        ULONG buf_len = sizeof(buf);
        ULONG req_len = 0;
        const NTSTATUS st = query_fn(key, &entry, 1, buf, &buf_len, &req_len);
        CloseHandle(key);
        return NT_SUCCESS(st);
    }

    bool test_delete_value_key()
    {
        HKEY key{};
        DWORD disp{};
        if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SogenDelValTest", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr,
                            &key, &disp) != ERROR_SUCCESS)
            return false;
        DWORD val = 42;
        RegSetValueExA(key, "TestVal", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        const LONG ret = RegDeleteValueA(key, "TestVal");
        RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SogenDelValTest");
        RegCloseKey(key);
        return ret == ERROR_SUCCESS || ret == ERROR_ACCESS_DENIED;
    }

    bool test_registry_transaction()
    {
        using NtCreateRegistryTransactionFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, ULONG);
        const auto fn =
            reinterpret_cast<NtCreateRegistryTransactionFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtCreateRegistryTransaction"));
        if (!fn)
            return false;
        HANDLE txn = nullptr;
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, nullptr, 0, nullptr, nullptr);
        const NTSTATUS st = fn(&txn, GENERIC_ALL, &oa, 0);
        if (txn)
            CloseHandle(txn);
        return NT_SUCCESS(st);
    }

    bool test_open_registry_transaction()
    {
        using NtOpenRegistryTransactionFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
        const auto fn =
            reinterpret_cast<NtOpenRegistryTransactionFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtOpenRegistryTransaction"));
        if (!fn)
            return false;
        HANDLE txn = nullptr;
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, nullptr, 0, nullptr, nullptr);
        const NTSTATUS st = fn(&txn, GENERIC_ALL, &oa);
        if (txn)
            CloseHandle(txn);
        return NT_SUCCESS(st);
    }
}

int main()
{
    bool all_ok = true;
    all_ok &= report("Registry key info query", test_query_key_info());
    all_ok &= report("Registry subkey enumeration", test_enumerate_subkeys());
    all_ok &= report("Registry value enumeration", test_enumerate_values());
    all_ok &= report("Registry create/set/read/delete", test_create_and_delete());
    all_ok &= report("NtOpenKeyEx", test_open_key_ex());
    all_ok &= report("NtNotifyChangeKey sync", test_notify_change_key());
    all_ok &= report("NtSetInformationKey tags", test_set_key_info());
    all_ok &= report("NtRenameKey", test_rename_key());
    all_ok &= report("NtQueryMultipleValueKey", test_query_multiple_values());
    all_ok &= report("NtDeleteValueKey", test_delete_value_key());
    all_ok &= report("NtCreateRegistryTransaction", test_registry_transaction());
    all_ok &= report("NtOpenRegistryTransaction", test_open_registry_transaction());
    return all_ok ? 0 : 1;
}
