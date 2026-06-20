# NtRaiseException — Contract from KiRaiseException (ntoskrnl @ 0x1403DCCB0)

NtRaiseException at 0x1406A9020 is a 1251-byte hand-crafted assembly stub;
IDA's decompiler cannot produce pseudocode for it. Contract is derived from
KiRaiseException which is the primary worker, decompilable at 0x1403DCCB0.

## KiRaiseException contract (verbatim key lines)

```
__int64 __fastcall KiRaiseException(_DWORD *a1, __int64 a2, __int64 a3, __int64 a4, char a5)
{
  // a1 = ExceptionRecord, a2 = ContextRecord, a3 = ExceptionFrame,
  // a4 = TrapFrame, a5 = FirstChance

  // Parameter validation (user-mode path):
  if ( v10 > 0xF )           // ExceptionRecord->NumberParameters > EXCEPTION_MAXIMUM_PARAMETERS
    return 3221225485LL;     // STATUS_INVALID_PARAMETER

  // Copies validated ExceptionRecord into kernel stack
  memmove(v22, v14, v13);

  // Dispatches to KiDispatchException which handles user-mode first-chance/last-chance
}
```

## Sogen contract

Sogen's `handle_NtRaiseException` receives the exception record from the emulated address space:
- Reads `exception_record.ExceptionCode`
- If `handle_exception=true` (unhandled): logs the code, sets `proc.exit_status`, calls
  `on_exception()` callback, stops emulation
- If `handle_exception=false` (raise for dispatch): sets `proc.exit_status`, calls
  `on_exception()`, stops emulation

This correctly surfaces the exception to the host without attempting real kernel exception
dispatch (which would require emulating SEH chains inside ntdll — not feasible).

Source: `src/windows-emulator/syscalls/exception.cpp`
