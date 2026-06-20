# NtFlushInstructionCache — Contract

NtFlushInstructionCache is not directly exported from ntoskrnl as a named function.
ZwFlushInstructionCache (0x1406A69D0) is a 31-byte kernel stub → KiServiceInternal().

## Contract

On x86-64, the instruction cache is coherent with the data cache (per Intel/AMD manual).
NtFlushInstructionCache is therefore a no-op on this architecture.

The kernel's ZwFlushInstructionCache is a syscall dispatcher stub with no real cache-flush work;
the CPU's cache-coherence hardware does it automatically.

Sogen returns STATUS_SUCCESS immediately — correct for x86-64 emulation.
Source: `src/windows-emulator/syscalls/process.cpp`
