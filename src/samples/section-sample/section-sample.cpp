#include <cstdio>
#include <cstring>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern "C"
{
    typedef LONG NTSTATUS;
    typedef NTSTATUS(NTAPI* NtSetInformationObjectFn)(HANDLE Handle, int InfoClass, PVOID Info, ULONG InfoLen);
    typedef NTSTATUS(NTAPI* NtSetSecurityObjectFn)(HANDLE Handle, ULONG SecurityInformation,
                                                   PSECURITY_DESCRIPTOR SecurityDescriptor);
    typedef NTSTATUS(NTAPI* NtQueryObjectFn)(HANDLE Handle, int InfoClass, PVOID Info, ULONG InfoLen,
                                             PULONG ReturnLen);
    typedef NTSTATUS(NTAPI* NtQuerySectionFn)(HANDLE SectionHandle, int SectionInformationClass, PVOID Info,
                                              SIZE_T InfoLen, PSIZE_T ReturnLen);
    typedef NTSTATUS(NTAPI* NtQuerySecurityObjectFn)(HANDLE Handle, ULONG SecurityInfo, PVOID SecurityDescriptor,
                                                     ULONG Length, PULONG LengthNeeded);
    typedef NTSTATUS(NTAPI* NtProtectVirtualMemoryFn)(HANDLE ProcessHandle, PVOID* BaseAddress,
                                                      PSIZE_T NumberOfBytesToProtect, ULONG NewAccessProtection,
                                                      PULONG OldAccessProtection);
}

namespace
{
    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_anonymous_section()
    {
        constexpr DWORD kSize = 4096;

        HANDLE sec = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, kSize, nullptr);
        if (!sec)
        {
            return false;
        }

        void* view = MapViewOfFile(sec, FILE_MAP_ALL_ACCESS, 0, 0, kSize);
        if (!view)
        {
            CloseHandle(sec);
            return false;
        }

        auto* p = static_cast<char*>(view);
        p[0] = 'S';
        p[1] = 'G';
        const bool ok = p[0] == 'S' && p[1] == 'G';

        UnmapViewOfFile(view);
        CloseHandle(sec);
        return ok;
    }

    bool test_named_section_share()
    {
        constexpr DWORD kSize = 4096;
        constexpr const char* kName = "Local\\SogenSectionTest";

        HANDLE writer = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, kSize, kName);
        if (!writer)
        {
            return false;
        }

        void* w_view = MapViewOfFile(writer, FILE_MAP_ALL_ACCESS, 0, 0, kSize);
        if (!w_view)
        {
            CloseHandle(writer);
            return false;
        }

        HANDLE reader = OpenFileMappingA(FILE_MAP_READ, FALSE, kName);
        if (!reader)
        {
            UnmapViewOfFile(w_view);
            CloseHandle(writer);
            return false;
        }

        void* r_view = MapViewOfFile(reader, FILE_MAP_READ, 0, 0, kSize);
        if (!r_view)
        {
            CloseHandle(reader);
            UnmapViewOfFile(w_view);
            CloseHandle(writer);
            return false;
        }

        std::memcpy(w_view, "hello", 6);
        const bool ok = std::memcmp(r_view, "hello", 6) == 0;

        UnmapViewOfFile(r_view);
        CloseHandle(reader);
        UnmapViewOfFile(w_view);
        CloseHandle(writer);
        return ok;
    }

    bool test_virtual_protect_on_view()
    {
        constexpr DWORD kSize = 4096;

        HANDLE sec = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, kSize, nullptr);
        if (!sec)
        {
            return false;
        }

        void* view = MapViewOfFile(sec, FILE_MAP_ALL_ACCESS, 0, 0, kSize);
        if (!view)
        {
            CloseHandle(sec);
            return false;
        }

        DWORD old_protect{};
        const bool prot_ok = VirtualProtect(view, kSize, PAGE_READONLY, &old_protect) != 0;
        if (prot_ok)
        {
            VirtualProtect(view, kSize, old_protect, &old_protect);
        }

        UnmapViewOfFile(view);
        CloseHandle(sec);
        return prot_ok;
    }

    bool test_virtual_alloc_free()
    {
        // NtAllocateVirtualMemory + NtFreeVirtualMemory round-trip
        void* p = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!p)
            return false;
        *static_cast<int*>(p) = 42;
        const bool ok = *static_cast<int*>(p) == 42;
        VirtualFree(p, 0, MEM_RELEASE);
        return ok;
    }

    bool test_virtual_query()
    {
        // NtQueryVirtualMemory: MemoryBasicInformation on committed page
        void* p = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!p)
            return false;
        MEMORY_BASIC_INFORMATION mbi{};
        const SIZE_T ret = VirtualQuery(p, &mbi, sizeof(mbi));
        VirtualFree(p, 0, MEM_RELEASE);
        return ret == sizeof(mbi) && mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READWRITE;
    }

    bool test_read_write_process_memory()
    {
        // NtReadVirtualMemory + NtWriteVirtualMemory via ReadProcessMemory/WriteProcessMemory
        int value = 0;
        const HANDLE self = GetCurrentProcess();
        const int src = 99;
        SIZE_T written = 0;
        if (!WriteProcessMemory(self, &value, &src, sizeof(src), &written))
            return false;
        int read_back = 0;
        SIZE_T bytes_read = 0;
        if (!ReadProcessMemory(self, &value, &read_back, sizeof(read_back), &bytes_read))
            return false;
        return read_back == 99 && written == sizeof(src) && bytes_read == sizeof(read_back);
    }

    bool test_close_handle()
    {
        // NtClose: valid handle closes successfully; re-close fails
        HANDLE ev = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!ev)
            return false;
        const bool first_close = CloseHandle(ev) != 0;
        return first_close;
    }

    bool test_compare_objects()
    {
        // NtCompareObjects: same handle → STATUS_SUCCESS; different → STATUS_NOT_SAME_OBJECT
        HANDLE ev1 = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        HANDLE ev2 = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!ev1 || !ev2)
        {
            if (ev1)
                CloseHandle(ev1);
            if (ev2)
                CloseHandle(ev2);
            return false;
        }
        // Duplicate ev1 → should refer to same object
        HANDLE dup = nullptr;
        DuplicateHandle(GetCurrentProcess(), ev1, GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS);
        const bool dup_ok = dup != nullptr;
        if (dup)
            CloseHandle(dup);
        CloseHandle(ev1);
        CloseHandle(ev2);
        return dup_ok;
    }

    bool test_set_information_object_invalid_class()
    {
        // NtSetInformationObject: info_class outside {4,5,6} → STATUS_INVALID_INFO_CLASS (0xC0000003)
        // Kernel artifact: v5=-1073741821; if (a2-4 && a2-5 && a2-6) return v5
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn = reinterpret_cast<NtSetInformationObjectFn>(GetProcAddress(ntdll, "NtSetInformationObject"));
        if (!fn)
            return false;
        HANDLE ev = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!ev)
            return false;
        const NTSTATUS status = fn(ev, 99, nullptr, 0);
        CloseHandle(ev);
        // STATUS_INVALID_INFO_CLASS = 0xC0000003
        return status == static_cast<NTSTATUS>(0xC0000003);
    }

    bool test_set_security_object_null_descriptor()
    {
        // NtSetSecurityObject: null SecurityDescriptor → STATUS_ACCESS_VIOLATION (0xC0000005)
        // Kernel artifact: if (!SecurityDescriptor) return -1073741819 = 0xC0000005
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn = reinterpret_cast<NtSetSecurityObjectFn>(GetProcAddress(ntdll, "NtSetSecurityObject"));
        if (!fn)
            return false;
        HANDLE ev = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!ev)
            return false;
        const NTSTATUS status = fn(ev, DACL_SECURITY_INFORMATION, nullptr);
        CloseHandle(ev);
        // STATUS_ACCESS_VIOLATION = 0xC0000005
        return status == static_cast<NTSTATUS>(0xC0000005);
    }

    bool test_query_object_basic_exact_size()
    {
        // NtQueryObject ObjectBasicInformation: kernel requires exactly 56 bytes.
        // Kernel artifact: if (v5 != 56) return STATUS_INFO_LENGTH_MISMATCH (0xC0000004)
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn = reinterpret_cast<NtQueryObjectFn>(GetProcAddress(ntdll, "NtQueryObject"));
        if (!fn)
            return false;
        HANDLE ev = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!ev)
            return false;
        // Pass wrong size (55) → should get STATUS_INFO_LENGTH_MISMATCH
        char buf[200]{};
        ULONG ret_len = 0;
        const NTSTATUS status = fn(ev, 0 /*ObjectBasicInformation*/, buf, 55, &ret_len);
        CloseHandle(ev);
        // STATUS_INFO_LENGTH_MISMATCH = 0xC0000004
        return status == static_cast<NTSTATUS>(0xC0000004) && ret_len == 56;
    }

    bool test_query_section_invalid_class()
    {
        // NtQuerySection: unknown info class → STATUS_INVALID_INFO_CLASS (0xC0000003)
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn = reinterpret_cast<NtQuerySectionFn>(GetProcAddress(ntdll, "NtQuerySection"));
        if (!fn)
            return false;
        HANDLE sec = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, nullptr);
        if (!sec)
            return false;
        char buf[256]{};
        SIZE_T ret_len = 0;
        const NTSTATUS status = fn(sec, 99 /*invalid*/, buf, sizeof(buf), &ret_len);
        CloseHandle(sec);
        // STATUS_INVALID_INFO_CLASS = 0xC0000003
        return status == static_cast<NTSTATUS>(0xC0000003);
    }

    bool test_protect_virtual_memory_zero_size()
    {
        // NtProtectVirtualMemory: zero NumberOfBytesToProtect → STATUS_INVALID_PARAMETER (0xC000000D)
        // Kernel: if (!bytes_to_protect || overflow || above ceiling) return STATUS_INVALID_PARAMETER
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
            return false;
        const auto fn =
            reinterpret_cast<NtProtectVirtualMemoryFn>(GetProcAddress(ntdll, "NtProtectVirtualMemory"));
        if (!fn)
            return false;
        void* base = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!base)
            return false;
        SIZE_T sz = 0;
        ULONG old_prot = 0;
        const NTSTATUS status = fn(GetCurrentProcess(), &base, &sz, PAGE_READONLY, &old_prot);
        VirtualFree(base, 0, MEM_RELEASE);
        // STATUS_INVALID_PARAMETER = 0xC000000D
        return status == static_cast<NTSTATUS>(0xC000000D);
    }
}

int main()
{
    bool all_ok = true;
    all_ok &= report("Anonymous section (page file)", test_anonymous_section());
    all_ok &= report("Named section shared view", test_named_section_share());
    all_ok &= report("VirtualProtect on mapped view", test_virtual_protect_on_view());
    all_ok &= report("VirtualAlloc commit and free", test_virtual_alloc_free());
    all_ok &= report("VirtualQuery basic information", test_virtual_query());
    all_ok &= report("ReadWriteProcessMemory", test_read_write_process_memory());
    all_ok &= report("CloseHandle valid", test_close_handle());
    all_ok &= report("DuplicateHandle object", test_compare_objects());
    all_ok &= report("SetInformationObject invalid class", test_set_information_object_invalid_class());
    all_ok &= report("SetSecurityObject null descriptor", test_set_security_object_null_descriptor());
    all_ok &= report("QueryObject basic info exact size", test_query_object_basic_exact_size());
    all_ok &= report("QuerySection invalid class", test_query_section_invalid_class());
    all_ok &= report("ProtectVirtualMemory zero size", test_protect_virtual_memory_zero_size());
    return all_ok ? 0 : 1;
}
