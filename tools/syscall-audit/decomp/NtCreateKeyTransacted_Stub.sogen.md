# NtCreateKeyTransacted_Stub — sogen contract

Sogen source: `src/windows-emulator/syscalls.cpp` (handle_NtCreateKeyTransacted_Stub)

This is a sogen-internal dispatch variant of NtCreateKeyTransacted with a different
parameter ordering used by the Win32 on-disk calling convention. It delegates directly
to `handle_NtCreateKey`, ignoring the transaction handle.

Kernel reference: `NtCreateKeyTransacted` (ntoskrnl.i64 0x140AE5070)
See `NtCreateKeyTransacted.c` for the kernel decompilation.

Key contract facts:
- Ignores transaction handle (CmpTransDereferenceTransaction not reached)
- Delegates to CmCreateKeyCallout path via handle_NtCreateKey
- Returns STATUS_OBJECT_NAME_NOT_FOUND if key path does not exist
