# ProcessDeviceMap Drive Enumeration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers-extended-cc:subagent-driven-development (recommended) or superpowers-extended-cc:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `NtQueryInformationProcess(ProcessDeviceMap)` report the emulation root's real configured drives instead of an all-zero drive map, so `GetLogicalDrives()`-based code (including the Windows Shell namespace layer) can see `C:` exists.

**Architecture:** Add the real `PROCESS_DEVICEMAP_INFORMATION` structure and populate its `DriveMap`/`DriveType[]` fields from `file_system::list_drives()` (already implemented and already used identically by `mount_point_manager.cpp`), replacing the current handler that unconditionally writes a single zeroed pointer.

**Tech Stack:** C++20, existing sogen syscall-handler patterns (`handle_query<T>`).

**User decisions (already made):** Implement only the `ProcessDeviceMap` fix first; defer the two fallback-IOCTL gaps (`IOCTL_MOUNTMGR_QUERY_DOS_VOLUME_PATHS` volume-GUID input, `DeviceApi\CMApi` dummy device) and the non-blocking `NtOpenEvent`/`MaximumCommitCondition` cleanliness fix — only pick those up in a follow-up cycle if the end-to-end test in this plan shows `sldim.exe` still doesn't launch.

---

## Task 1: Implement and verify the `ProcessDeviceMap` fix

**Goal:** `NtQueryInformationProcess(ProcessDeviceMap)` returns a real `PROCESS_DEVICEMAP_INFORMATION` populated from the emulation root's actual drives, and the SolidWorks installer's `ShellExecuteA` call is re-tested end-to-end to determine whether `sldim.exe` now launches.

**Files:**
- Modify: `src/emulator-platform/platform/process.hpp:1352` (insert new struct right after the existing `PROCESS_PRIORITY_CLASS` struct, before `PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION`)
- Modify: `src/windows-emulator/syscalls/process.cpp:199-203` (replace the `ProcessDeviceMap` case body)

**Acceptance Criteria:**
- [ ] `PROCESS_DEVICEMAP_INFORMATION` is defined with the real Windows layout (`ULONG DriveMap; UCHAR DriveType[32];`)
- [ ] The `ProcessDeviceMap` case populates `DriveMap`'s bit *N* and `DriveType[N] = DRIVE_FIXED (3)` for every drive letter `file_system::list_drives()` returns, and leaves all other bits/entries at zero
- [ ] `cmake --build --preset=release` succeeds with no new warnings in the touched files
- [ ] A full SolidWorks installer run under `EMULATOR_FEX=1` either shows `sldim.exe` actually launching (a new `NtCreateUserProcess: launching child 3` or `Mapped ...\sldim.exe` line) or completes without it — either outcome is a valid, informative result for this task, since the fix's job is to be correct and testable, not to guarantee the outcome by itself

**Verify:** Full installer run (command below) → grep the resulting log for `launching child 3\|Mapped.*sldim\.exe` to determine the outcome.

**Steps:**

- [ ] **Step 1: Add the `PROCESS_DEVICEMAP_INFORMATION` struct**

In `src/emulator-platform/platform/process.hpp`, the current content around line 1347-1358 is:

```cpp
    struct PROCESS_PRIORITY_CLASS
    {
        BOOLEAN Foreground;
        UCHAR PriorityClass;
    };

    struct PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION
    {
        ULONG Version;
        ULONG Reserved;
        uint64_t Callback;
    };
```

Insert a new struct between them, so it reads:

```cpp
    struct PROCESS_PRIORITY_CLASS
    {
        BOOLEAN Foreground;
        UCHAR PriorityClass;
    };

    struct PROCESS_DEVICEMAP_INFORMATION
    {
        ULONG DriveMap;
        UCHAR DriveType[32];
    };

    struct PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION
    {
        ULONG Version;
        ULONG Reserved;
        uint64_t Callback;
    };
```

- [ ] **Step 2: Replace the `ProcessDeviceMap` handler**

In `src/windows-emulator/syscalls/process.cpp`, the current code (lines 199-203) is:

```cpp
            case ProcessDeviceMap:
                return handle_query<EmulatorTraits<Emu64>::PVOID>(c.emu, process_information, process_information_length, return_length,
                                                                  [](EmulatorTraits<Emu64>::PVOID& ptr) {
                                                                      ptr = 0; //
                                                                  });
```

Replace it with:

```cpp
            case ProcessDeviceMap:
                return handle_query<PROCESS_DEVICEMAP_INFORMATION>(
                    c.emu, process_information, process_information_length, return_length,
                    [&](PROCESS_DEVICEMAP_INFORMATION& info) {
                        constexpr UCHAR drive_fixed = 3;
                        for (const auto drive : c.win_emu.file_sys.list_drives())
                        {
                            const auto drive_index = static_cast<size_t>(drive - 'a');
                            info.DriveMap |= (1u << drive_index);
                            info.DriveType[drive_index] = drive_fixed;
                        }
                    });
```

(`handle_query`'s internal `ResponseType obj{};` already value-initializes the struct to all zeros before invoking this lambda — no explicit zeroing needed. `file_system::list_drives()` returns lowercase `'a'`-`'z'`, matching the exact pattern already used in `src/windows-emulator/devices/mount_point_manager.cpp`'s `query_points()`.)

- [ ] **Step 3: Build**

```bash
cd /Users/jack/Documents/Coding/C++/sogen/.worktrees/wt-solidworks-bringup
cmake --build --preset=release --target analyzer
```

Expected: clean build, no new warnings for `process.hpp`/`process.cpp`.

- [ ] **Step 4: Format the touched files**

```bash
clang-format -i src/emulator-platform/platform/process.hpp src/windows-emulator/syscalls/process.cpp
cmake --build --preset=release --target analyzer
```

Expected: clean build again after formatting (confirms formatting didn't break anything).

- [ ] **Step 5: Full end-to-end installer verification**

Run in the foreground with a background subshell + log file (this project's established pattern for long-running analyzer tests — never use the Bash tool's own `run_in_background` parameter for the analyzer itself):

```bash
cd /Users/jack/Documents/Coding/C++/sogen/.worktrees/wt-solidworks-bringup/build/release/artifacts
rm -f /tmp/sw_devicemap_fix.log
(EMULATOR_FEX=1 ./analyzer -e /Users/jack/Documents/Coding/C++/sogen/build/release/artifacts/root \
  --click-dialog-button "WinZip Self-Extractor - solidworkssetup.exe" 111 \
  --click-dialog-button "WinZip Self-Extractor - solidworkssetup.exe" 1 \
  --click-dialog-button "WinZip Self-Extractor" 2 \
  --vcpus 1 c:/solidworks/SolidWorksSetup.exe > /tmp/sw_devicemap_fix.log 2>&1 &)
```

Wait for completion (full run is ~10-25 minutes under FEX; poll every few minutes rather than sleeping tightly, or use the Monitor tool watching for `Emulation terminated` in the log). Before starting, confirm no stray `analyzer` process is already running (`ps aux | grep analyzer`).

- [ ] **Step 6: Check the result**

```bash
grep -n "launching child 3\|Mapped.*sldim\.exe\|Error 5 running command\|Emulation terminated" /tmp/sw_devicemap_fix.log
```

Two possible outcomes, both valid task completions:

- **`sldim.exe` launches** (a `launching child 3` or `Mapped ...\sldim.exe` line appears): the fix is sufficient on its own. Record this in `docs/superpowers/specs/2026-07-25-process-devicemap-design.md` (append a short "Result" section) and in memory (`/Users/jack/.claude/projects/-Users-jack-Documents-Coding-C---sogen/memory/project_solidworks_bringup.md`, new finding). The SolidWorks bring-up's Stop-hook goal is met.
- **`sldim.exe` still doesn't launch** (still `Error 5 running command` or a clean exit with neither line present): the fix is real and correct but insufficient alone — re-run under `--backend unicorn` with `-m combase.dll,ole32.dll,windows.storage.dll,clbcatq.dll,shell32.dll,propsys.dll,oleaut32.dll,shcore.dll` (same click-dialog-button flags) to see how much further execution gets, and use that evidence to decide whether the two deferred fallback-IOCTL fixes (documented in the spec's "Explicitly out of scope" section) are now the actual blocker. This becomes a new, separate follow-up task/plan — do not guess ahead of the evidence.

- [ ] **Step 7: Commit**

```bash
git add src/emulator-platform/platform/process.hpp src/windows-emulator/syscalls/process.cpp
git commit -m "$(cat <<'EOF'
fix(syscalls): implement real ProcessDeviceMap drive enumeration

NtQueryInformationProcess(ProcessDeviceMap) previously wrote a single
zeroed pointer regardless of the emulation root's configured drives,
so GetLogicalDrives() (and anything built on it, including the
Windows Shell namespace layer) always saw zero drives. Populate the
real PROCESS_DEVICEMAP_INFORMATION structure from
file_system::list_drives() instead.
EOF
)" --no-gpg-sign
```

---

## Self-Review

**Spec coverage:** The spec's "Fix" section (struct + population from `list_drives()`) is fully covered by Steps 1-2. The spec's "Testing" section (build, full FEX run, conditional Unicorn re-run) is covered by Steps 3-6. The spec's "Explicitly out of scope" items are deliberately not implemented here, per the spec's own deferral — Step 6 documents the decision point rather than silently skipping it.

**Placeholder scan:** No TBD/TODO/"handle appropriately" language. Every code block is complete, copy-pasteable, and matches the actual current file contents (verified by reading both files directly before writing this plan).

**Type consistency:** `PROCESS_DEVICEMAP_INFORMATION` is defined once (Step 1) and used with that exact name in Step 2's `handle_query<PROCESS_DEVICEMAP_INFORMATION>` call — no mismatch.
