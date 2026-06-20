#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <cstdio>
#include <cstring>

namespace
{
    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_create_write_read()
    {
        const char* path = "C:\\test-fileio.tmp";
        const char data[] = "hello fileio";

        HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        DWORD written = 0;
        WriteFile(h, data, sizeof(data) - 1, &written, nullptr);
        CloseHandle(h);

        h = CreateFileA(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        char buf[64]{};
        DWORD read = 0;
        ReadFile(h, buf, sizeof(buf), &read, nullptr);
        CloseHandle(h);
        DeleteFileA(path);

        return read == sizeof(data) - 1 && std::memcmp(buf, data, read) == 0;
    }

    bool test_file_size()
    {
        const char* path = "C:\\test-size.tmp";
        HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        DWORD written = 0;
        const char data[128]{};
        WriteFile(h, data, sizeof(data), &written, nullptr);
        CloseHandle(h);

        h = CreateFileA(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            DeleteFileA(path);
            return false;
        }
        LARGE_INTEGER sz{};
        bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart == 128;
        CloseHandle(h);
        DeleteFileA(path);
        return ok;
    }

    bool test_find_files()
    {
        WIN32_FIND_DATAA fd{};
        HANDLE h = FindFirstFileA("C:\\*", &fd);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        int count = 0;
        do
        {
            ++count;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
        return count > 0;
    }

    bool test_attributes()
    {
        DWORD attrs = GetFileAttributesA("C:\\windows\\system32\\ntdll.dll");
        return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool test_directory_attributes()
    {
        DWORD attrs = GetFileAttributesA("C:\\windows\\system32");
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool test_open_file()
    {
        using NtOpenFileFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, ULONG, ULONG);
        const auto fn = reinterpret_cast<NtOpenFileFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtOpenFile"));
        if (!fn)
            return false;
        wchar_t path[] = L"\\??\\C:\\windows\\system32\\ntdll.dll";
        UNICODE_STRING us{static_cast<USHORT>(wcslen(path) * 2), static_cast<USHORT>(wcslen(path) * 2 + 2), path};
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        IO_STATUS_BLOCK isb{};
        HANDLE h = nullptr;
        const NTSTATUS st = fn(&h, GENERIC_READ | SYNCHRONIZE, &oa, &isb, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               FILE_SYNCHRONOUS_IO_NONALERT);
        if (NT_SUCCESS(st) && h)
            CloseHandle(h);
        return NT_SUCCESS(st);
    }

    bool test_flush_file()
    {
        HANDLE h = CreateFileA("C:\\test-flush.tmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        const BOOL ok = FlushFileBuffers(h);
        CloseHandle(h);
        DeleteFileA("C:\\test-flush.tmp");
        return ok != FALSE;
    }

    bool test_lock_unlock_file()
    {
        HANDLE h = CreateFileA("C:\\test-lockfile.tmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        const BOOL locked = LockFile(h, 0, 0, 16, 0);
        const BOOL unlocked = locked && UnlockFile(h, 0, 0, 16, 0);
        CloseHandle(h);
        DeleteFileA("C:\\test-lockfile.tmp");
        return locked != FALSE && unlocked != FALSE;
    }

    bool test_query_volume_info()
    {
        using NtQueryVolumeInformationFileFn = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, int);
        const auto fn = reinterpret_cast<NtQueryVolumeInformationFileFn>(
            GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryVolumeInformationFile"));
        if (!fn)
            return false;
        HANDLE h = CreateFileA("C:\\windows\\system32\\ntdll.dll", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        struct FsSizeInfo
        {
            LARGE_INTEGER TotalAllocationUnits, AvailableAllocationUnits;
            ULONG SectorsPerAllocationUnit, BytesPerSector;
        } info{};
        IO_STATUS_BLOCK isb{};
        constexpr int FileFsSizeInformation = 3;
        const NTSTATUS st = fn(h, &isb, &info, sizeof(info), FileFsSizeInformation);
        CloseHandle(h);
        return NT_SUCCESS(st) && info.BytesPerSector > 0;
    }

    bool test_query_ea_file()
    {
        using NtQueryEaFileFn = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, BOOLEAN, PVOID, ULONG, PULONG, BOOLEAN);
        const auto fn = reinterpret_cast<NtQueryEaFileFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryEaFile"));
        if (!fn)
            return false;
        HANDLE h = CreateFileA("C:\\windows\\system32\\ntdll.dll", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        BYTE buf[64]{};
        IO_STATUS_BLOCK isb{};
        const NTSTATUS st = fn(h, &isb, buf, sizeof(buf), TRUE, nullptr, 0, nullptr, TRUE);
        CloseHandle(h);
        return st == static_cast<NTSTATUS>(0x80000052L) || NT_SUCCESS(st); // STATUS_NO_EAS_ON_FILE or success
    }

    bool test_fscontrol_file()
    {
        HANDLE h = CreateFileA("C:\\windows\\system32\\ntdll.dll", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        BYTE out[64]{};
        DWORD returned = 0;
        constexpr DWORD FSCTL_GET_OBJECT_ID = 0x9009c;
        DeviceIoControl(h, FSCTL_GET_OBJECT_ID, nullptr, 0, out, sizeof(out), &returned, nullptr);
        CloseHandle(h);
        return true;
    }

    bool test_file_stub_ops()
    {
        using NtCancelIoFileExFn = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PIO_STATUS_BLOCK);
        using NtCopyFileChunkFn = NTSTATUS(NTAPI*)(HANDLE, HANDLE, HANDLE, ULONG, PLARGE_INTEGER, PLARGE_INTEGER, PVOID);
        using NtSetVolumeInformationFileFn = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, int);
        const auto ntdll = GetModuleHandleA("ntdll.dll");
        const auto cancel_ex = reinterpret_cast<NtCancelIoFileExFn>(GetProcAddress(ntdll, "NtCancelIoFileEx"));
        const auto copy_chunk = reinterpret_cast<NtCopyFileChunkFn>(GetProcAddress(ntdll, "NtCopyFileChunk"));
        const auto set_vol = reinterpret_cast<NtSetVolumeInformationFileFn>(GetProcAddress(ntdll, "NtSetVolumeInformationFile"));
        if (!cancel_ex || !copy_chunk || !set_vol)
            return false;
        HANDLE h = CreateFileA("C:\\windows\\system32\\ntdll.dll", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        IO_STATUS_BLOCK isb{};
        cancel_ex(h, nullptr, &isb);
        IO_STATUS_BLOCK isb2{};
        set_vol(h, &isb2, nullptr, 0, 1);
        CloseHandle(h);
        return true;
    }
}

int main()
{
    bool ok = true;
    ok &= report("Create/write/read file", test_create_write_read());
    ok &= report("File size query", test_file_size());
    ok &= report("FindFirstFile/FindNextFile", test_find_files());
    ok &= report("File attributes", test_attributes());
    ok &= report("Directory attributes", test_directory_attributes());
    ok &= report("NtOpenFile", test_open_file());
    ok &= report("NtFlushBuffersFile", test_flush_file());
    ok &= report("NtLockFile + NtUnlockFile", test_lock_unlock_file());
    ok &= report("NtQueryVolumeInformationFile", test_query_volume_info());
    ok &= report("NtQueryEaFile", test_query_ea_file());
    ok &= report("NtFsControlFile", test_fscontrol_file());
    ok &= report("NtFileStubOps", test_file_stub_ops());
    return ok ? 0 : 1;
}
