# NtContinueEx — Contract from NtContinue + KiContinueEx

NtContinueEx at 0x1406A8B90 is a 1147-byte hand-crafted assembly stub;
IDA's decompiler returns 0 pseudocode rows. NtContinue (0x1406A8B80, 6 lines) is a 
thin wrapper: `return NtContinueEx(a1, a2)`.

## Contract (derived from NtContinue wrapper + KiContinueEx)

```
// NtContinue — thin wrapper:
return NtContinueEx(ContextRecord, TestAlert);

// NtContinueEx contract:
// 1. Captures ContextRecord from user-mode address (probe + copy in PreviousMode==UserMode)
// 2. If TestAlert: checks for pending APCs (KiTestAlert)
// 3. Calls KiContinuePreviousModeUser → restores trap frame from ContextRecord
//    (sets RIP, RSP, all registers from CONTEXT structure)
// 4. Returns control directly to user-mode RIP (does not normally return to caller)
```

## Sogen contract

Sogen's `handle_NtContinue` (and NtContinueEx delegating to it) restores the emulated
CPU context from the CONTEXT structure in guest memory:
- Reads full CONTEXT from guest address
- Sets all emulated registers via `c.emu.restore_snapshot(context)`
- Returns; emulation continues from the restored RIP

This matches the observable contract: execution resumes at ContextRecord.Rip.
Source: `src/windows-emulator/syscalls/exception.cpp`
