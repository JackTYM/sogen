# Repository Guidelines

## Project Overview

Sogen is a C++20 Windows user-space emulator.
It produces an `analyzer` binary that analyzes and emulates windows binaries.
Details and commandline options can be found in `src/analyzer/main.cpp`

## Build

For fast iterations during development, build the release preset:

`cmake --build --preset=release`

When fully done implementing a feature, make sure to build the tidy configuration, which includes clang-tidy.
It's very slow, so only use it at the end:

`cmake --build --preset=tidy`

**Always run builds and analyzer invocations in the foreground** — never pass `run_in_background: true` to the Bash tool. Background runs leave lingering shells and the timeout mechanism doesn't work reliably for them. Use the Bash tool's built-in `timeout` parameter (milliseconds) instead of the shell `timeout` command, and always capture and read the output yourself rather than asking the user to run commands.

## Smoke tests

Execute `analyzer.exe -s test-sample.exe` for smoke tests using the cmd in the directory of the built preset.
E.g.: `cmd /c "cd build\release\artifacts\ && analyzer.exe -s test-sample.exe"`

Other applications can also be executed in the emulator:
`cmd /c "cd build\release\artifacts\ && analyzer.exe -s path/to/binary.exe arg1 arg2 ..."`

## Debugging emulation bugs

Sogen runs the real ntdll/user32/gdi32full/win32u and hand-implements the kernel side. Most bugs
fall into one of four classes identifiable from the log alone — reach for idasql only for the
one that can't be diagnosed any other way.

### 1. Triage: classify the symptom before doing anything else

Run with `-v` from the artifacts dir:

`cd build/release/artifacts && ./analyzer -e root -v -s "c:\\sample.exe" 2>&1 | tee run.log`

Read the log and route to the right branch:

| Symptom | Cause | Action |
|---|---|---|
| `Unimplemented syscall: <name>` then stops | No handler for that NT syscall | Add a handler in `src/windows-emulator/syscalls/*.cpp` |
| `Unsupported <X> class: 0xNN` (warn/error) | A `switch` info-class case is missing | Add the case in the file/function that logged it — no RE needed |
| `[gpu-bridge] Unsupported IOCTL/command 0xNN` | GPU path gap; graphics/game apps | See GPU/Vulkan branch below |
| `Access violation … RIP=0x<garbage>` exits 1 | Kernel-callback arg-struct mismatch | See idasql branch below |
| `Access violation` with a valid RIP in a real DLL exits 1 | Wrong value written into a struct/register | Use `-d` GDB mode to inspect registers and memory |
| Exit code 124 | `timeout` killed it cleanly — message loop alive | Not a crash; verify the intended behavior is happening |

Binary staleness: after any edit, confirm `analyzer` was actually rebuilt (check mtime or `touch`
the source). `Text file busy` means the linker is still running — wait and retry.

### 2. Built-in debugging flags

`-v` verbose syscall + function trace, `-c` / `-vc` concise, `-s` silent, `-m <module>` filter to
one module, `--report run.jsonl` machine-readable events, `-d [--port N]` GDB stub (attach with
gdb/lldb for breakpoints and memory inspection when the app misbehaves without crashing),
`-t` Tenet trace, `-rep` reproducible (stubs clocks). Run `./analyzer --help` for the full list.

### 3. GPU / Vulkan branch (graphics and game apps)

The call path is: app → gdi32/d3d/Vulkan → `devices/gpu_bridge.cpp` → host Vulkan. Gaps surface
as `[gpu-bridge] Unsupported IOCTL/command 0xNN` warnings. Key files: `src/gpu-bridge-protocol`
(wire protocol), `src/vulkan-bridge-marshal` (generated marshalling, produced by
`src/tools/vulkan-bridge-generator`), `src/samples/vulkan-shim` (reference shim). Design docs:
`docs/gpu-paravirtualization.md` and `docs/windows-ui-emulation.md`.

### 4. Kernel-callback contract branch (garbage-RIP crashes)

Applies only when you see `Access violation … RIP=0x<garbage>` — the real DLL read a wrong field
from the arg buffer sogen built and called through it. Diagnose with this loop:

**Probe.** Add temporary `fprintf(stderr, "[TAG] ...")` at:
- `user_callback_dispatch.hpp` `prepare_call_stack`: log the KCT index and arg-buffer address.
- `windows_emulator.cpp` access-violation handler: log RIP, R11, RSP; walk stack frames to `module+rva`.
- `syscalls/user.cpp` `dispatch_window_message`: log message id and wnd_proc.

Guard high-frequency probes by `thread.id` — unguarded probes flood stderr, cause pipe
backpressure, and make the run time out (looks like a hang, not a crash). Track every probe in
the `project-temp-debug-to-remove` memory entry and remove **all** before tidy or commit.

**Reverse-engineer.** Full IDB inventory at `~/.cache/sogen-symbols/`:

| IDB | Owns |
|---|---|
| `ntdll.i64` | User-mode stubs: `ZwCreate*`, KiUserCallbackDispatcher |
| `user32.i64` | Win32 window/message functions; KCT dispatch |
| `gdi32full.i64` | GDI fast-path (DC_ATTR, surface, BitBlt) |
| `win32u.i64` | GDI/user32 kernel thunks |
| `ntoskrnl.i64` | All core `Nt*` syscalls |
| `npfs.i64` | Named pipe FS driver (`NpFsdCreate`, `NpCommonRead/Write`) |
| `afd.i64` | Socket/Winsock kernel driver (`Afd*`) |
| `win32kbase.i64` | User32/GDI kernel side + KCT callback table |
| `win32kfull.i64` | GDI windowing kernel side |
| `win32k.i64` | Win32 kernel bridge |
| `kernelbase.i64` | Win32 API implementation (CreatePipe, CreateFile, …) |
| `dxgkrnl.i64` | GPU/DirectX kernel (IOCTL dispatch) |
| `dxgmms1.i64`, `dxgmms2.i64` | GPU memory management |
| `condrv.i64`, `msfs.i64` | Console/mailslot drivers |
| `cng.i64` | Crypto (`BCrypt*` kernel side) |
| `csrss.i64`, `conhost.i64`, `smss.i64` | Session/console management |

**Query convention:** Use the function name directly — no image-base math for kernel modules.
`SELECT decompile('NtCreateNamedPipeFile')` works in `ntoskrnl.i64`; `SELECT decompile('NpFsdCreate')` in `npfs.i64`.
For the four user-mode DLLs, image base is `0x180000000` (`ea = 0x180000000 + rva`).

Build a missing IDB headlessly with `/tmp/create_idb_batch.py`. PDBs are auto-fetched via
`_NT_SYMBOL_PATH="SRV*$HOME/.cache/sogen-symbols/syms*https://msdl.microsoft.com/download/symbols"`.

Query via `idasql:*` skills or `~/idasql/q -s <idb> -q "<sql>"`.
`SELECT decompile('KiUserCallbackDispatcher')` gives the full function with per-line `/* ea */`
anchors; `SELECT decompile(<faulting_ea>)` maps the crash RVA straight to a source line.

**Find the mismatch.** The path is `KiUserCallbackDispatcher` → `KiUserCallForwarder` →
`KernelCallbackTable[index]` (`__fn*`). Decompile the faulting `__fn*` and list every
`[r11+0xNN]` access — that is the exact arg-struct the real DLL expects; the icall target is the
offending field. The KCT index order is **not** address-sorted. Sogen's indices are the
`k_fn_*_callback_id` constants in `syscalls/user.cpp` — a wrong index silently lands on the wrong
callback. Compare those constants and sogen's `fn_*_message` field offsets against the real
`[r11+0xNN]` reads.

**Fix.** Pick the correct index; build the matching arg struct mirroring the existing `fn_*`
patterns; add `static_assert`s on field offsets and `sizeof`; set the dispatch pointer to
`c.proc.dispatch_client_message`.

### 5. Verify every fix

Rebuild release, re-run: the log line / crash must be gone. Confirm the intended behavior is
reached (programmatic gate plus visual/behavioral check). Remove all probes. Build tidy. Run the
smoke suite (`EMULATOR_ROOT=root ./windows-emulator-test`). One fix often exposes the next missing
case — repeat the loop.

## Development notes

- Run clang-format on changed files to ensure consistent formatting
- Do not generate code comments unless they add important information not otherwise deducible from code itself
- Do not use shortcuts or workarounds. Always prefer clean and idiomatic solutions.
