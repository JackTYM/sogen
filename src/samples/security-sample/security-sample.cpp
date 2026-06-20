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
        TOKEN_TYPE type{};
        DWORD ret = 0;
        const BOOL type_ok = GetTokenInformation(dup, TokenType, &type, sizeof(type), &ret);
        CloseHandle(dup);
        return type_ok != FALSE && type == TokenImpersonation;
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

    bool test_compare_tokens_same()
    {
        // NtCompareTokens: kernel calls SeCompareTokens; same token object → Equal=TRUE, STATUS_SUCCESS.
        using NtCompareTokensFn = NTSTATUS(NTAPI*)(HANDLE, HANDLE, PBOOLEAN);
        const auto fn = reinterpret_cast<NtCompareTokensFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtCompareTokens"));
        if (!fn)
            return false;
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;
        BOOLEAN equal = FALSE;
        const NTSTATUS st = fn(token, token, &equal);
        CloseHandle(token);
        return NT_SUCCESS(st) && equal == TRUE; // STATUS_SUCCESS; equal when same handle
    }

    bool test_open_process_token_bad_handle()
    {
        // NtOpenProcessToken: kernel calls ObpReferenceObjectByHandle first; invalid handle →
        // STATUS_INVALID_HANDLE (0xC0000008).
        using NtOpenProcessTokenFn = NTSTATUS(NTAPI*)(HANDLE, ACCESS_MASK, PHANDLE);
        const auto fn =
            reinterpret_cast<NtOpenProcessTokenFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtOpenProcessToken"));
        if (!fn)
            return false;
        HANDLE token = nullptr;
        const NTSTATUS st = fn(reinterpret_cast<HANDLE>(0x1234), TOKEN_QUERY, &token);
        if (token)
            CloseHandle(token);
        return st == static_cast<NTSTATUS>(0xC0000008); // STATUS_INVALID_HANDLE
    }

    bool test_open_thread_token_no_token()
    {
        // NtOpenThreadToken: kernel calls PsReferenceImpersonationToken; thread has no impersonation
        // token → STATUS_NO_TOKEN (0xC000005B).
        HANDLE token = nullptr;
        const BOOL ok = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, FALSE, &token);
        const DWORD err = GetLastError();
        if (token)
            CloseHandle(token);
        return !ok && err == 1008; // ERROR_NO_TOKEN = STATUS_NO_TOKEN
    }

    bool test_access_check_deny_all()
    {
        // NtAccessCheck with empty DACL (AceCount==0) → access_status == STATUS_ACCESS_DENIED.
        // Kernel: SeAccessCheckByType; empty DACL = deny all.
        using NtAccessCheckFn =
            NTSTATUS(NTAPI*)(PSECURITY_DESCRIPTOR, HANDLE, ACCESS_MASK, PGENERIC_MAPPING, PPRIVILEGE_SET, PULONG, PACCESS_MASK,
                             PNTSTATUS);
        const auto fn = reinterpret_cast<NtAccessCheckFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtAccessCheck"));
        if (!fn)
            return false;

        BYTE acl_buf[sizeof(ACL)]{};
        auto* const acl = reinterpret_cast<PACL>(acl_buf);
        if (!InitializeAcl(acl, sizeof(acl_buf), ACL_REVISION))
            return false;

        SECURITY_DESCRIPTOR sd{};
        if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
            return false;
        if (!SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE))
            return false;

        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;

        GENERIC_MAPPING mapping{GENERIC_READ, GENERIC_WRITE, GENERIC_EXECUTE, GENERIC_ALL};
        PRIVILEGE_SET priv_set{};
        ULONG priv_len = sizeof(priv_set);
        ACCESS_MASK granted = 0;
        NTSTATUS access_st = 0;
        const NTSTATUS st = fn(&sd, token, GENERIC_READ, &mapping, &priv_set, &priv_len, &granted, &access_st);
        CloseHandle(token);
        return NT_SUCCESS(st) && access_st == static_cast<NTSTATUS>(0xC0000022); // STATUS_ACCESS_DENIED
    }
}

int main()
{
    bool all_ok = true;
    all_ok &= report("OpenProcess (current pid)", test_open_process());
    all_ok &= report("AdjustTokenPrivileges", test_adjust_token_privileges());
    all_ok &= report("NtAccessCheck (grant all)", test_access_check());
    all_ok &= report("NtAccessCheck empty DACL deny all", test_access_check_deny_all());
    all_ok &= report("NtCompareTokens same handle", test_compare_tokens_same());
    all_ok &= report("NtOpenProcessToken bad handle", test_open_process_token_bad_handle());
    all_ok &= report("NtOpenThreadToken no impersonation", test_open_thread_token_no_token());
    all_ok &= report("NtQuerySystemInformation SystemPageFileInformation", test_system_page_file_info());
    all_ok &= report("DuplicateToken (token type)", test_duplicate_token());
    all_ok &= report("NtQueryInformationToken (type)", test_query_token_type());
    all_ok &= report("NtQuerySecurityAttributesToken", test_query_security_attrs());
    return all_ok ? 0 : 1;
}
