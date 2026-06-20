#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#include <cstdio>

namespace
{
    bool report(const char* name, bool ok)
    {
        std::printf("Running test '%s': %s\n", name, ok ? "Success" : "Failure");
        return ok;
    }

    bool test_keyed_event()
    {
        UNICODE_STRING name{};
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, nullptr, 0, nullptr, nullptr);

        using NtCreateKeyedEventFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, ULONG);
        using NtReleaseKeyedEventFn = NTSTATUS(NTAPI*)(HANDLE, PVOID, BOOLEAN, PLARGE_INTEGER);
        using NtWaitForKeyedEventFn = NTSTATUS(NTAPI*)(HANDLE, PVOID, BOOLEAN, PLARGE_INTEGER);

        const auto ntdll = GetModuleHandleA("ntdll.dll");
        const auto create_fn = reinterpret_cast<NtCreateKeyedEventFn>(GetProcAddress(ntdll, "NtCreateKeyedEvent"));
        const auto release_fn = reinterpret_cast<NtReleaseKeyedEventFn>(GetProcAddress(ntdll, "NtReleaseKeyedEvent"));
        const auto wait_fn = reinterpret_cast<NtWaitForKeyedEventFn>(GetProcAddress(ntdll, "NtWaitForKeyedEvent"));

        if (!create_fn || !release_fn || !wait_fn)
            return false;

        HANDLE h = nullptr;
        NTSTATUS status = create_fn(&h, EVENT_ALL_ACCESS, &oa, 0);
        if (!NT_SUCCESS(status))
            return false;
        if (!h)
            return false;

        LARGE_INTEGER timeout{};
        timeout.QuadPart = -1LL;
        release_fn(h, reinterpret_cast<PVOID>(0x1234), FALSE, nullptr);
        wait_fn(h, reinterpret_cast<PVOID>(0x1234), FALSE, &timeout);

        CloseHandle(h);
        return true;
    }

    bool test_cancel_io()
    {
        HANDLE hFile =
            CreateFileA("C:\\pipe-sample.exe", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;

        using NtCancelIoFileFn = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK);
        const auto fn = reinterpret_cast<NtCancelIoFileFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtCancelIoFile"));
        if (!fn)
        {
            CloseHandle(hFile);
            return false;
        }

        IO_STATUS_BLOCK isb{};
        const NTSTATUS status = fn(hFile, &isb);
        CloseHandle(hFile);
        return NT_SUCCESS(status);
    }

    bool test_delete_file()
    {
        // Write a temp file, then delete it via NtDeleteFile
        const char* tmp_path = "C:\\__io_ops_tmp__.txt";
        HANDLE hFile = CreateFileA(tmp_path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return false;
        CloseHandle(hFile);

        using NtDeleteFileFn = NTSTATUS(NTAPI*)(POBJECT_ATTRIBUTES);
        const auto fn = reinterpret_cast<NtDeleteFileFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtDeleteFile"));
        if (!fn)
            return false;

        wchar_t nt_path[] = L"\\??\\C:\\__io_ops_tmp__.txt";
        UNICODE_STRING us{};
        us.Buffer = nt_path;
        us.Length = static_cast<USHORT>(wcslen(nt_path) * 2);
        us.MaximumLength = us.Length + 2;

        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, nullptr, nullptr);

        const NTSTATUS status = fn(&oa);
        return NT_SUCCESS(status);
    }

    bool test_allocate_uuids()
    {
        using NtAllocateUuidsFn = NTSTATUS(NTAPI*)(PULARGE_INTEGER, PULONG, PULONG, PCHAR);
        const auto fn = reinterpret_cast<NtAllocateUuidsFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtAllocateUuids"));
        if (!fn)
            return false;

        ULARGE_INTEGER time{};
        ULONG range{}, sequence{};
        CHAR seed[8]{};
        return NT_SUCCESS(fn(&time, &range, &sequence, seed)) && time.QuadPart != 0;
    }

    bool test_io_completion()
    {
        using NtCreateIoCompletionFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, ULONG);
        using NtSetIoCompletionFn = NTSTATUS(NTAPI*)(HANDLE, ULONG_PTR, ULONG_PTR, NTSTATUS, ULONG_PTR);
        using NtRemoveIoCompletionFn = NTSTATUS(NTAPI*)(HANDLE, PULONG_PTR, PULONG_PTR, PIO_STATUS_BLOCK, PLARGE_INTEGER);

        const auto ntdll = GetModuleHandleA("ntdll.dll");
        const auto create_fn = reinterpret_cast<NtCreateIoCompletionFn>(GetProcAddress(ntdll, "NtCreateIoCompletion"));
        const auto set_fn = reinterpret_cast<NtSetIoCompletionFn>(GetProcAddress(ntdll, "NtSetIoCompletion"));
        const auto remove_fn = reinterpret_cast<NtRemoveIoCompletionFn>(GetProcAddress(ntdll, "NtRemoveIoCompletion"));
        if (!create_fn || !set_fn || !remove_fn)
            return false;

        HANDLE h = nullptr;
        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, nullptr, 0, nullptr, nullptr);
        if (!NT_SUCCESS(create_fn(&h, IO_COMPLETION_ALL_ACCESS, &oa, 0)))
            return false;

        if (!NT_SUCCESS(set_fn(h, 0x42, 0x43, STATUS_SUCCESS, 16)))
        {
            CloseHandle(h);
            return false;
        }

        ULONG_PTR key = 0, apc = 0;
        IO_STATUS_BLOCK isb{};
        LARGE_INTEGER timeout{};
        timeout.QuadPart = 0;
        const NTSTATUS st = remove_fn(h, &key, &apc, &isb, &timeout);
        CloseHandle(h);
        return NT_SUCCESS(st) && key == 0x42;
    }

    bool test_wait_completion_packet()
    {
        using NtCreateIoCompletionFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, ULONG);
        using NtCreateWaitCompletionPacketFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
        using NtAssociateWaitCompletionPacketFn = NTSTATUS(NTAPI*)(HANDLE, HANDLE, HANDLE, PVOID, PVOID, NTSTATUS, ULONG_PTR, PBOOLEAN);
        using NtCancelWaitCompletionPacketFn = NTSTATUS(NTAPI*)(HANDLE, BOOLEAN);

        const auto ntdll = GetModuleHandleA("ntdll.dll");
        const auto create_iocp = reinterpret_cast<NtCreateIoCompletionFn>(GetProcAddress(ntdll, "NtCreateIoCompletion"));
        const auto create_pkt = reinterpret_cast<NtCreateWaitCompletionPacketFn>(GetProcAddress(ntdll, "NtCreateWaitCompletionPacket"));
        const auto assoc_fn = reinterpret_cast<NtAssociateWaitCompletionPacketFn>(GetProcAddress(ntdll, "NtAssociateWaitCompletionPacket"));
        const auto cancel_fn = reinterpret_cast<NtCancelWaitCompletionPacketFn>(GetProcAddress(ntdll, "NtCancelWaitCompletionPacket"));
        if (!create_iocp || !create_pkt || !assoc_fn || !cancel_fn)
            return false;

        OBJECT_ATTRIBUTES oa{};
        InitializeObjectAttributes(&oa, nullptr, 0, nullptr, nullptr);

        HANDLE iocp = nullptr;
        if (!NT_SUCCESS(create_iocp(&iocp, IO_COMPLETION_ALL_ACCESS, &oa, 0)))
            return false;

        HANDLE pkt = nullptr;
        if (!NT_SUCCESS(create_pkt(&pkt, 0x1F0003, &oa)))
        {
            CloseHandle(iocp);
            return false;
        }

        HANDLE evt = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!evt)
        {
            CloseHandle(pkt);
            CloseHandle(iocp);
            return false;
        }

        BOOLEAN signaled = FALSE;
        const NTSTATUS st_assoc = assoc_fn(pkt, iocp, evt, reinterpret_cast<PVOID>(0x10), nullptr, STATUS_SUCCESS, 0, &signaled);
        const NTSTATUS st_cancel = cancel_fn(pkt, FALSE);

        CloseHandle(evt);
        CloseHandle(pkt);
        CloseHandle(iocp);
        return NT_SUCCESS(st_assoc) && NT_SUCCESS(st_cancel);
    }
}

int main()
{
    bool all_ok = true;
    all_ok &= report("NtCreateKeyedEvent + Release + Wait", test_keyed_event());
    all_ok &= report("NtCancelIoFile", test_cancel_io());
    all_ok &= report("NtDeleteFile", test_delete_file());
    all_ok &= report("NtAllocateUuids", test_allocate_uuids());
    all_ok &= report("NtCreateIoCompletion + SetRemove", test_io_completion());
    all_ok &= report("NtWaitCompletionPacket associate+cancel", test_wait_completion_packet());
    return all_ok ? 0 : 1;
}
