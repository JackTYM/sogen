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

    bool test_open_process()
    {
        const DWORD pid = GetCurrentProcessId();
        HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!h)
        {
            return false;
        }
        CloseHandle(h);
        return true;
    }

    bool test_adjust_token_privileges()
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        {
            return false;
        }

        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        const BOOL ok = AdjustTokenPrivileges(token, FALSE, &tp, 0, nullptr, nullptr);
        CloseHandle(token);
        return ok != FALSE;
    }

    bool test_access_check()
    {
        using NtAccessCheckFn =
            NTSTATUS(NTAPI*)(PSECURITY_DESCRIPTOR, HANDLE, ACCESS_MASK, PGENERIC_MAPPING, PPRIVILEGE_SET, PULONG, PACCESS_MASK, PNTSTATUS);

        const auto fn = reinterpret_cast<NtAccessCheckFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtAccessCheck"));
        if (!fn)
        {
            return false;
        }

        SECURITY_DESCRIPTOR sd{};
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);

        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        {
            return false;
        }

        GENERIC_MAPPING mapping{
            .GenericRead = GENERIC_READ,
            .GenericWrite = GENERIC_WRITE,
            .GenericExecute = GENERIC_EXECUTE,
            .GenericAll = GENERIC_ALL,
        };

        PRIVILEGE_SET priv_set{};
        ULONG priv_len = sizeof(priv_set);
        ACCESS_MASK granted = 0;
        NTSTATUS access_status = 0;

        const NTSTATUS status = fn(&sd, token, GENERIC_READ, &mapping, &priv_set, &priv_len, &granted, &access_status);
        CloseHandle(token);
        return NT_SUCCESS(status);
    }

    bool test_system_page_file_info()
    {
        using NtQuerySystemInformationFn = NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PULONG);

        const auto fn =
            reinterpret_cast<NtQuerySystemInformationFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation"));
        if (!fn)
        {
            return false;
        }

        constexpr ULONG SystemPageFileInformation = 0x12;
        BYTE buf[256]{};
        ULONG ret_len = 0;
        const NTSTATUS status = fn(SystemPageFileInformation, buf, sizeof(buf), &ret_len);
        return NT_SUCCESS(status);
    }

    bool test_duplicate_token()
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE, &token))
            return false;
        HANDLE dup = nullptr;
        const BOOL ok = DuplicateTokenEx(token, TOKEN_QUERY, nullptr, SecurityImpersonation, TokenImpersonation, &dup);
        CloseHandle(token);
        if (!ok || !dup)
            return false;
        CloseHandle(dup);
        return true;
    }

    bool test_query_token_type()
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;
        TOKEN_TYPE type{};
        DWORD ret = 0;
        const BOOL ok = GetTokenInformation(token, TokenType, &type, sizeof(type), &ret);
        CloseHandle(token);
        return ok && type == TokenPrimary;
    }

    bool test_query_security_attrs()
    {
        using Fn = NTSTATUS(NTAPI*)(HANDLE, PUNICODE_STRING, ULONG, PVOID, ULONG, PULONG);
        const auto fn = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySecurityAttributesToken"));
        if (!fn)
            return false;
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;
        BYTE buf[256]{};
        ULONG ret_len = 0;
        const NTSTATUS status = fn(token, nullptr, 0, buf, sizeof(buf), &ret_len);
        CloseHandle(token);
        return NT_SUCCESS(status);
    }
}

int main()
{
    bool all_ok = true;
    all_ok &= report("OpenProcess (current pid)", test_open_process());
    all_ok &= report("AdjustTokenPrivileges", test_adjust_token_privileges());
    all_ok &= report("NtAccessCheck (grant all)", test_access_check());
    all_ok &= report("NtQuerySystemInformation SystemPageFileInformation", test_system_page_file_info());
    all_ok &= report("DuplicateToken (token type)", test_duplicate_token());
    all_ok &= report("NtQueryInformationToken (type)", test_query_token_type());
    all_ok &= report("NtQuerySecurityAttributesToken", test_query_security_attrs());
    return all_ok ? 0 : 1;
}
