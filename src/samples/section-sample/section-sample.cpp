#include <cstdio>
#include <cstring>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
    return all_ok ? 0 : 1;
}
