# NtOpenKeyTransactedEx_Stub — sogen contract

Sogen source: `src/windows-emulator/syscalls.cpp` (handle_NtOpenKeyTransactedEx_Stub)

This is a sogen-internal dispatch variant of NtOpenKeyTransactedEx with a different
parameter ordering used by the Win32 on-disk calling convention. It delegates directly
to `handle_NtOpenKeyTransactedEx`, which in turn calls `handle_NtOpenKey` ignoring the
transaction handle.

Kernel reference: `NtOpenKeyTransactedEx` (ntoskrnl.i64 0x1409E8540)
See `NtOpenKeyTransactedEx.c` for the kernel decompilation.

Key contract facts:
- Ignores transaction handle (CmpTransDereferenceTransaction not reached)
- Delegates to CmOpenKey path via handle_NtOpenKey
- Returns STATUS_OBJECT_NAME_NOT_FOUND if key path does not exist
