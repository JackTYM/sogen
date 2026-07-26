# `ProcessDeviceMap` drive enumeration

## Problem

`GetLogicalDrives()` (and anything built on it, e.g. Windows Shell namespace binding of drive letters) reports **zero drives** under sogen, regardless of the emulation root's actual configured filesystem.

This was found while bringing up `SolidWorksSetup.exe` on `integration/solidworks-bringup`: the installer's `ShellExecuteA("sldim\sldim.exe", ...)` call fails with `SE_ERR_ACCESSDENIED`. Live guest-code tracing (disassembly-confirmed argument construction + a `--backend unicorn` precision-hook read of live register state, cross-checked against a full run with `--modules` broadened to cover `combase.dll`/`ole32.dll`/`windows.storage.dll`/`clbcatq.dll`/`propsys.dll`/`oleaut32.dll`/`shcore.dll`) shows real in-process COM activation (`CoInitializeEx` → `SHParseDisplayName` → `CoCreateInstance` → `DllGetClassObject`) already executes correctly end-to-end as ordinary guest code — sogen needs no new COM/`IClassFactory` infrastructure for this. The actual failure is `windows.storage.dll`'s shell-namespace layer calling `GetLogicalDrives`, getting back `0`, and concluding there is no `C:` to bind — so path parsing (and everything downstream of it, including ever attempting to launch the target) fails before any process-creation attempt.

Traced to `NtQueryInformationProcess(ProcessDeviceMap)` (`src/windows-emulator/syscalls/process.cpp:199`), which `GetLogicalDrives` calls internally to read `PROCESS_DEVICEMAP_INFORMATION.Query.DriveMap`. The current handler:

```cpp
case ProcessDeviceMap:
    return handle_query<EmulatorTraits<Emu64>::PVOID>(c.emu, process_information, process_information_length, return_length,
                                                      [](EmulatorTraits<Emu64>::PVOID& ptr) {
                                                          ptr = 0; //
                                                      });
```

writes a single zeroed pointer — the wrong type and wrong size for the real structure, and it never reflects the emulation root's actual drives regardless.

This is a general sogen correctness bug, not SolidWorks-specific: any guest code that calls `GetLogicalDrives`/`GetLogicalDriveStrings` (a very common pattern — file pickers, installers, drive enumeration UIs) would hit the same wrong "no drives" result.

## Fix

Define the real `PROCESS_DEVICEMAP_INFORMATION` structure (query form only — the `Set` union arm, used for impersonation-scoped device maps, is not exercised by anything seen so far and is out of scope):

```cpp
typedef struct _PROCESS_DEVICEMAP_INFORMATION
{
    ULONG DriveMap;
    UCHAR DriveType[32];
} PROCESS_DEVICEMAP_INFORMATION;
```

matching the real Windows layout (`ULONG` bitmask, bit *N* set = drive letter `'A' + N` exists; `DriveType[N]` = the `GetDriveType()`-style value for that letter, `DRIVE_UNKNOWN` (0) if absent).

Populate it in the `ProcessDeviceMap` case from `win_emu.file_sys.list_drives()` — already used identically by `mount_point_manager.cpp`'s `query_points()`, so this reuses an established, working pattern rather than introducing a new source of drive information. Every listed drive gets `DriveType = DRIVE_FIXED (3)`, matching what a real fixed-disk emulation root should report (sogen doesn't model removable/network drives, so there's nothing else a drive here could correctly be).

Add the struct to `src/emulator-platform/platform/process.hpp` next to the existing `ProcessDeviceMap` enumerator comment (`:417`, which already names the real struct — just needs the actual definition).

## Explicitly out of scope for this fix

Two further gaps were found in the same investigation, both in **fallback** paths `windows.storage.dll` only reaches if `GetLogicalDrives` has *already* failed:

- `IOCTL_MOUNTMGR_QUERY_DOS_VOLUME_PATHS` (`devices/mount_point_manager.cpp:217`) rejects volume-GUID-form input (`\??\Volume{...}`), only accepting `\Device\HarddiskVolume<N>`-form names.
- `DeviceApi\CMApi` is a bare `dummy_device` (`io_device.cpp`), so `CM_Get_Device_Interface_List_Size_ExW`-style device-interface enumeration always returns empty.

Per the investigating agent's analysis, once `GetLogicalDrives` succeeds via this fix, the shell should never need either fallback for this scenario — so neither is touched here. **Decision on these is deferred to the test step below**: only implemented if `sldim.exe` still doesn't launch after this fix lands.

Also found, real but non-blocking: `NtOpenEvent` on `\KernelObjects\MaximumCommitCondition` returns `STATUS_NOT_FOUND` (no special-case, unlike the existing `SystemErrorPortReady` handling), which drives a repeated `clbcatq.dll` load/unload/DllMain-rollback cycle during COM catalog resolution. Activation succeeds anyway (combase falls back to direct registry reads), so this is a cleanliness/performance issue, not a blocker — left out of this fix, revisit only if it turns out to matter.

## Testing

1. Build (`cmake --build --preset=release`).
2. Full end-to-end SolidWorks installer run (the existing three-rule `--click-dialog-button` invocation used throughout this investigation), under `EMULATOR_FEX=1` first (fast, confirms end-state) — success criterion: a new `NtCreateUserProcess`/`Mapped ...\sldim.exe` line appears (`sldim.exe` actually launches), not just the installer's own clean exit.
3. If it doesn't launch, re-run under `--backend unicorn` with `--modules` broadened as in the investigation to see exactly where it now gets further, and decide from that evidence whether the two deferred fallback-IOCTL gaps are actually needed.
4. No new unit test infrastructure is proposed — this is a single syscall-handler correctness fix, verified by the existing integration-style installer run (which is already how every other fix in this investigation was validated) plus a quick manual sanity check that `GetLogicalDrives`-consuming code elsewhere in the existing test suite (if any) still passes.
