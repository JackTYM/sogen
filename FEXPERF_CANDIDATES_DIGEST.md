# FEX-Perf Candidate Extraction Digest

Source: workflow journal `wf_65f84d4c-9cb`, 10 completed extraction agents.
This file is the reference record for opening the actual GitHub PRs — content below is
copied verbatim from each agent's final result text (result is a single triple-quoted
`<result>` payload per agent in the journal; no summarization or compression applied).

## Flags

- **Candidate 7 (jobobject-query-zerofill) — CONFIRMED implemented the CORRECTED design, not the earlier-rejected shape.** The agent explicitly states it checked out `handle_NtQueryInformationJobObject` on `origin/main` (a bare stub returning `STATUS_NOT_SUPPORTED`), and **did not** apply source commit `c6bde025`'s zero-fill shape. Instead it changed the stub to return `STATUS_INVALID_INFO_CLASS` (matching real NT behavior and this repo's own precedents in `handle_NtQueryInformationFile`/`handle_NtQueryEvent`), with **no buffer write at all**. The PR body itself documents the rejection explicitly: "a prior draft of this fix zero-filled the output buffer while still returning a failure status; that shape was rejected because it doesn't match real NT behavior." This is the corrected design, confirmed.

- **Candidate 10 (sched-idle-backoff) — CONFIRMED correctly built on candidate 9's actual pushed branch.** Candidate 9 (sched-double-scan) pushed `fork/perf/sched-switch-to-next-thread-single-scan` at commit `94f7e05544bb38cca4f2dfa1e9be2a592d6f454d`. Candidate 10's report states it built "on top of the sibling PR's actual pushed tip (`fork/perf/sched-switch-to-next-thread-single-scan` @ `94f7e05544bb38cca4f2dfa1e9be2a592d6f454d`)" — the commit hash matches exactly. It correctly detected that a direct cherry-pick of its own source commit (`f05c4bc2`) would conflict (modify/delete on `wait_storm_diag.hpp`, content conflict in the already-restructured `perform_thread_switch`), aborted the cherry-pick, and hand-applied only the real diff content on top of the sibling's restructured function. Confirmed correct stacking.

- **Candidate 9 (sched-double-scan) — decision on `wait_storm_diag.hpp`: STRIPPED, not kept.** The agent explicitly chose option (b): ship only the algorithmic fix, drop the diagnostic entirely. Reasoning given: `wait_storm_diag.hpp` is a one-off investigation instrument (not a reusable feature, doesn't rise to "could be a plugin"), flagged by the rubric's "no leftover debug hooks" criterion, and consistent with this repo's documented maintainer preference (perf-only additions should be opt-in plugins, not baked into core — see `reference_sogen_upstream_plugin_preference.md`). Since `origin/main`'s `switch_to_next_thread` was already byte-identical to the pre-diagnostic baseline, the agent wrote the restructured function directly against `origin/main` rather than cherry-picking, citing the previously-measured numbers (2255 calls/sec, 23.6 iterations/call) as motivation-only in the commit/PR text. Candidate 10 confirms this same absence and follows the identical reasoning for its own half of the diagnostic (`scan_cost` reporting).

- **Candidate 4 (nls-peb-teb-tables) — CONFIRMED real bug found and fixed: wrong ntdll module read for the WoW64 lead-byte table.** `ensure_nls_lead_byte_info_table` (in `syscall_dispatcher.cpp`) originally read `win_emu.mod_manager.ntdll`. For a WoW64 process this resolves to the **64-bit** ntdll (verified by tracing `module_manager::load_wow64_modules`, which calls `load_native_64bit_modules` to set `this->ntdll` before separately mapping `wow64_modules_.ntdll32`). The hardcoded lead-byte-table offsets (`0x172760`/`0x1726d0`) were resolved against the **32-bit** ntdll build, so the original candidate code was patching a field in the wrong module entirely — a real correctness bug that could have silently written a fabricated pointer into an unrelated field of the real 64-bit ntdll's writable section, while never touching the 32-bit ntdll it was meant to fix. Fixed by switching the accessor to `win_emu.mod_manager.wow64_modules_.ntdll32`, which the agent confirmed is already the accessor used elsewhere in this exact codebase for the same purpose (`windows_emulator.cpp:796`, `process_context.cpp:589`). Rebuilt and re-ran the full test suite after the fix — still 69/69, no regressions. This candidate also honestly disclosed it could **not** independently reproduce the original null-table-read crash despite three different repro-binary attempts (stock `wow64-test-sample.exe` plus two custom MinGW 32-bit repros) — flagged as a verification limitation, not a correctness retraction.

- **Other real issues found/fixed during rubric review across candidates (found-and-fixed vs found-and-left-as-caveat):**
  - **Candidate 1 (peb-critsect-timeout) — found-and-fixed (2 items):** (a) a stale forward-looking comment claiming a scheduler follow-on "exists only until the idle wait learns to wake on a cross-vCPU signal" was softened since that follow-on work doesn't cleanly apply to `origin/main`; stale sentence also removed from the commit message. (b) comment density was 40% (6/15 added lines), over the ~30% guideline; trimmed to 4/14 (~29%) while preserving all non-recoverable NT-internals content.
  - **Candidate 3 (wow64-displayconfig-abi) — found-and-fixed (1 item, documentation only):** the source commit's message claimed "27/27" tests passing; the current suite has grown, so the agent corrected the commit message to the real, freshly-measured "69/69" rather than propagating a stale number. No code changes were needed — the extracted diff is byte-identical to the source commit.
  - **Candidate 6 (pump-ui-events-vcpu-spawn) — found-and-fixed (1 item) + found-and-left-as-caveat (1 item):** fixed a comment at ~67% density (4 comment lines / 1 code line) down to 3 lines while keeping the load-bearing content. Separately, deliberately dropped the original commit's specific live-MW2 crash-rate claims ("3 crashes in 60 attempts", "65 further attempts, 0 crashes", "26/26 smoke tests, 33/33 windows-emulator-test") from both the commit and PR body since the agent did not personally reproduce those live MW2 runs in this task — disclosed as a verification-honesty caveat, not silently carried forward.
  - **Candidate 8 (vulkan-shim-missing-exports) — found-and-left-as-caveat:** `clang-format --dry-run -Werror` flags the touched `.def` file, but the agent confirmed via `git stash` that the unmodified pre-existing file already fails identically (696 violation lines, clang-format has no `.def` language mode) — pre-existing, correctly left unfixed as out of scope. Also removed a `Claude-Session` trailer from the source commit to match the stripped-trailer convention used for the prior PR #1234 split (a cleanup, not a bug fix).
  - **Candidates 2 (trace-control-zerofill), 5 (create-root-audio-dlls) — nothing found requiring a fix.** Both reported clean rubric passes with no issues.
  - **Candidate 9 (sched-double-scan) — minor process note, not a code issue:** the agent briefly opened, then closed, a stray PR against its own fork (`JackTYM/sogen#4`) before recognizing the established pattern in this campaign is to leave PR opening as a manual step; the branch itself was left untouched.

---

## peb-critsect-timeout

**PR title:** `fix(process): give the PEB the critical-section timeout ntdll waits on`

**Branch:** `fix/peb-critsect-timeout` on `fork` (`https://github.com/JackTYM/sogen.git`)
**Commit:** `8d2d8563f03745e686c6b82c2b16730d449e6e9e` (one commit on `origin/main` @ `83566003`)
**Files:** `src/windows-emulator/process_context.cpp`, `src/samples/test-sample/test.cpp`

### Review fixes applied

- Removed the `EMULATOR_LEGACY_ZERO_CRITSECT_TIMEOUT` opt-out entirely; PEB64 and PEB32 now share one named constant (`critical_section_timeout`, -25920000000000 = 30 days, relative 100ns).
- Comment is the single line the reviewer specified (clang-format wraps it to two lines).
- Dropped the "covered by the full windows-emulator-test pass" claim. Added a `test-sample` case instead (see below).
- FPS cost is kept as a plain known limitation; the measurement is labelled as coming from a FEX+HVF/MW2 setup that cannot be reproduced on `origin/main`.

### Test added

`test-sample` gains "Critical Section Timeout": reads `PEB->CriticalSectionTimeout` (offset 0xC0 on x64, 0x70 on x86) and requires a negative (relative) value, then has a second thread contend on a held `CRITICAL_SECTION` and checks it stays blocked until release. The reviewer-suggested debug-string assertion is not feasible: a VEH counting `DBG_PRINTEXCEPTION_C` saw zero debug-print exceptions both with and without the fix (the DbgPrintEx output is never delivered to the guest), so the check is on the PEB field. Verified that it fails when `process_context.cpp` is reverted to `origin/main` and passes with the fix; the x86 PEB offset was verified separately with a standalone WoW64 program (value -25920000000000).

### Build/test results

- macOS arm64, `cmake --preset=release`, targets `analyzer` and `windows-emulator-test`. The FEX backend was disabled for this build (pinned FEX submodule does not compile against the installed fmt), so the smoke run used the default backend.
- `windows-emulator-test`: 165 tests ran, `[  PASSED  ] 157 tests`, 2 skipped (FEX register tests), 6 failed out of 165 (build without the FEX backend on a private root); the same 6 (`EmulationTest.*`, `SerializationTest.*`) fail identically on unmodified `origin/main` because `test-sample`'s CryptProtect case fails against that root (dpapi), unrelated to this change.
- Smoke: `analyzer -e <private root> -s c:\test-sample.exe` with a locally MinGW-built x64 `test-sample.exe`: new case prints `Success`; the only failing case is the pre-existing CryptProtect one.
- `clang-format --dry-run -Werror` on both touched files: clean.

### Full PR body

```markdown
## Problem

ntdll copies `PEB->CriticalSectionTimeout` into its own `RtlpTimeout` during process init and
passes that as the wait interval in `RtlpWaitOnCriticalSection`. Zero there means "time out
immediately", not "wait forever". Sogen leaves the field zeroed, so every contended
critical-section acquisition times out at once, emits three filtered `DbgPrintEx` calls
(`"RTL: Enter CriticalSection Timeout"`, the `ContentionCount` line, `"RTL: Re-Waiting"`), and
re-waits in a loop until it finally wins the section. Each of those is an
`NtQueryDebugFilterState` syscall, so a guest with contended critical sections burns a large
fraction of its syscall budget re-reporting a timeout that should never have fired.

## Fix

`src/windows-emulator/process_context.cpp`: seed both `PEB64.CriticalSectionTimeout` and
`PEB32.CriticalSectionTimeout` with the 30-day value Windows takes from `MmCritsectTimeout`,
via one shared constant.

## Test

`test-sample` gains a "Critical Section Timeout" case: it checks that the PEB field is a
negative (relative) timeout and that a second thread contending on a held `CRITICAL_SECTION`
stays blocked until it is released. The `DbgPrintEx` output itself is not observable from the
guest, so the check is on the PEB field; the case fails without this change. The x86 PEB
offset (0x70) was verified separately under WoW64.

## Measurements and known limitation

Measured on MW2 under FEX+HVF with `--vcpus 2`, a setup that cannot be reproduced on
`origin/main`: `NtQueryDebugFilterState` 2,964/s -> 0, total syscalls 23,013/s -> 19,946/s, host
CPU 149.6% -> 131.5%. `NtWaitForAlertByThreadId` and `NtAlertThreadByThreadId` also go from
1,957/s vs 1,402/s to exactly balanced.

Known limitation: correct blocking costs frame rate there. Five interleaved pairs put the
settled stationary scene 4.7-11.4% lower, because a critical-section handoff now pays a real
cross-vCPU wake instead of a guest-speed poll, and the emulator's idle branch still sleeps a
fixed 1ms rather than waking on a cross-vCPU signal.

## Verification

- `analyzer -s test-sample.exe` smoke run: the new case passes; it fails with the fix reverted.
- `clang-format --dry-run -Werror` on both touched files: clean.
```

### Caveats

- The PR was not opened; it is only pushed to `fork`.
- The 6 `windows-emulator-test` failures are environmental and pre-existing; they mask the test-sample pass/fail in `EmulationTest.*`, so the new case was verified through the direct analyzer smoke run.

---

## trace-control-zerofill

**PR title:** `fix(trace): zero-fill NtTraceControl output buffer for register_guids/set_provider_traits`

**Branch:** `fix/trace-control-zero-fill-output-buffer` on `fork` (`https://github.com/JackTYM/sogen.git`)
**Commit:** `6849967394864b4d8611b5c9f000a257136e565c` (one commit on `origin/main` @ `83566003`)
**File:** `src/windows-emulator/syscalls/trace.cpp` (+7/-2)

### Review fixes applied

- Body now says plainly that the change is defensive; the "whenever the passthrough handler short-circuits" wording is replaced by "the handler never writes the output buffer".
- No test-sample case added, justified in the body: with the current guest ntdll, `EventRegister` returns the same handle (and `EventWrite`/`EventUnregister` succeed) with and without this change, so there is no guest-visible effect to assert. Checked with a pre-filled `REGHANDLE` on x64 and WoW64 builds of a small `EventRegister` program: identical output before and after.
- Dropped the reference to the non-upstream commit `ab28e582` and the "hit on essentially every process's first advapi32 load" claim, which relied on it.

### Build/test results

- macOS arm64, `cmake --preset=release` without the FEX backend (pinned FEX submodule does not compile against the installed fmt).
- `windows-emulator-test`: 165 tests ran, `[  PASSED  ] 157 tests`, 2 skipped (FEX register tests), 6 failed out of 165 (build without the FEX backend on a private root); the same 6 (`EmulationTest.*`, `SerializationTest.*`) fail identically on unmodified `origin/main` because `test-sample`'s CryptProtect case fails against that root (dpapi), unrelated to this change.
- Smoke: `analyzer -s test-sample.exe` (locally MinGW-built x64): only the pre-existing CryptProtect case fails.
- `clang-format --dry-run -Werror src/windows-emulator/syscalls/trace.cpp`: clean.

### PR body

```markdown
## Problem

`NtTraceControl`'s passthrough handler (used for the `register_guids` and `set_provider_traits`
control codes) only wrote `ReturnLength`. The handler never writes the guest-supplied output
buffer, so the buffer kept whatever bytes it held before the call. A guest that reads a
registration handle back out of that buffer would see stale stack contents instead of a defined
value.

This is defensive. With the current guest ntdll, `EventRegister` returns the same handle (and
`EventWrite`/`EventUnregister` succeed) with and without this change, so there is no
guest-visible regression to cover with a test-sample case.

## Fix

`src/windows-emulator/syscalls/trace.cpp`:
- `handle_trace_control_passthrough` takes the `syscall_context&` and zero-fills `output_buffer`
  for `output_buffer_length` bytes via `c.emu.set_memory(output_buffer, 0, output_buffer_length)`
  before writing `ReturnLength`, the same idiom used for other stub output buffers in the tree.
- The call site in `handle_NtTraceControl` passes `c` through.

## Verification

- CI is green on every platform for this change except one unrelated job: `HostPipeTest.ZeroLengthReadReportsClosedPeer/1` failed in `Test (Linux x86_64 GCC Sanitizer, Unicorn)`. That test family also fails on `main` (run 38167792154), so it looks like a flaky test rather than something this change touches.
- `clang-format --dry-run -Werror src/windows-emulator/syscalls/trace.cpp`: clean.
```

### Caveats

- The PR was not opened; it is only pushed to `fork`.

---

## wow64-displayconfig-abi

**PR title:** `fix(user): NtUserGetDisplayConfigBufferSizes takes (flags, numPaths)`

**Branch:** `fix/wow64-displayconfig-abi-marshaling` on `fork` (`https://github.com/JackTYM/sogen.git`)
**Commit:** `bb84808b7a6497aceb22083e7013e3466be7726e` (single commit on `origin/main` @ `83566003`)

Reworked 2026-10-11: the previous version's theory (wow64win packs both counts into one buffer; `QueryDisplayConfig`'s path array is opaque under WoW64) was refuted by disassembly and the WoW64 special cases were dropped entirely. Touched files: `syscalls.cpp`, `syscalls/user.cpp` (+5/-8).

### Evidence

- `syswow64\win32u.dll`: `NtUserGetDisplayConfigBufferSizes` ends `retl $0x8` (2 args); `NtUserQueryDisplayConfig` ends `retl $0x14` (5 args).
- `wow64win.dll` (idasql, MS symbols): `whNtUserGetDisplayConfigBufferSizes(a1)` = `ZwUserGetDisplayConfigBufferSizes(*a1, a1[1])`, leaving `r8 = flags`; `whNtUserQueryDisplayConfig` is a plain 5-argument pass-through.
- `user32!GetDisplayConfigBufferSizes` (x64 and x86): calls the syscall with `(flags, pNumPaths)` only, then writes `*pNumModes = numPaths * 2` (`* 3` with flag `0x10`).

### Build/test results

- `cmake --build --preset=release` (macOS arm64, Apple Clang) — clean. `clang-format` clean. Homebrew `clang-tidy` on the changed lines — no diagnostics.
- `windows-emulator-test` (private root copy): 165 ran, 157 passed, 2 skipped, 6 failed — identical to `origin/main` with the same root. The 6 failures (`EmulationTest.BasicEmulationWorks`, `EmulationTest.CountedEmulationIsAccurate`, 4× `SerializationTest.*`) are test-sample's `CryptProtect` subtest (`CryptProtectData failed: 1829`) with a private root, pre-existing on `origin/main`.
- Standalone repro `dcfg.c` (`GetDisplayConfigBufferSizes` + `QueryDisplayConfig`, built x86 and x64 with MinGW), Windows 11 26100 root:
  - `origin/main`, 32-bit: `GetDisplayConfigBufferSizes=87 paths=1 modes=<untouched>`.
  - `origin/main`, 64-bit: succeeds (works by accident through the stale `r8`).
  - This branch, 32-bit and 64-bit: `GetDisplayConfigBufferSizes=0 paths=1 modes=2`, `QueryDisplayConfig=0 paths=1 modes=2`, exit 0.
- No test-sample case: the native x64 path already passed before the fix, and the 32-bit test-sample does not build with i686 MinGW, so a test-sample case would not cover the bug.

### PR body

```markdown
## Problem

`NtUserGetDisplayConfigBufferSizes` takes two parameters, `(flags, pNumPaths)`:
the 32-bit `win32u` stub ends in `ret 8`, `wow64win`'s thunk forwards only
`(flags, pNumPaths)`, and `user32!GetDisplayConfigBufferSizes` computes
`*pNumModes` itself from the returned path count (x2, or x3 with
`QDC_VIRTUAL_MODE_AWARE`). The handler declared a third `pNumModes` argument.

On native x64 this only worked because user32 still holds `pNumModes` in `r8`
at the syscall. Under WoW64 the thunk leaves the flags value in `r8`, so the
write through it failed and the handler returned `STATUS_INVALID_PARAMETER`:
`GetDisplayConfigBufferSizes` failed with `ERROR_INVALID_PARAMETER` (87) for
every 32-bit caller.

## Fix

Drop the phantom argument and report only the path count.

## Verification

- Release build on macOS/arm64; `clang-format` clean.
- Small 32-bit and 64-bit programs calling `GetDisplayConfigBufferSizes` then
  `QueryDisplayConfig`, Windows 11 26100 emulation root:
  - before: 32-bit `GetDisplayConfigBufferSizes` returns 87; 64-bit succeeds.
  - after: both return 0 with `paths=1 modes=2`, and `QueryDisplayConfig`
    returns 0 with `paths=1 modes=2`.

Not addressed here: `NtUserQueryDisplayConfig` writes a 0xD8-byte path record,
which matches 26100's user32 (`numPaths * 0xD8` buffer) but overflows the
0xC8-byte records user32 allocates on 20348 (Server 2022) roots; with such a
root `QueryDisplayConfig` still corrupts the heap (`0xC0000374`) on x64 both
before and after this change.
```

### Blocking concerns / caveats

- The `QueryDisplayConfig` record-size mismatch on 20348 roots (see last paragraph of the body) is a separate, pre-existing bug; CI's `windows-2022` root is 20348-based.
- The repro was run against the 26100 root only for the before/after; on the 20348 shared root both bitnesses still fail later in `QueryDisplayConfig` (x64 `0xC0000374`, WoW64 `0xC0000005`) because of that separate bug.

---

## nls-peb-teb-tables

**PR title:** `feat(nls): serve l_intl.nls for NtGetNlsSectionPtr section type 14`

**Branch:** `feat/nls-locale` on `fork`
**Commit:** `c9bd7c0bf9f1b3bcb5a7a1bbb2e1b6645db51d89` (single commit on `origin/main` @ `83566003`)

Reworked 2026-10-11. Dropped from the previous version: `CPTABLEINFO32`/`NLSTABLEINFO32`, the identity/ASCII table builders, the hard-coded `ActiveCodePage=1252`/`OemCodePage=437`, the `SysWOW64` fallback, the `STATUS_OBJECT_NAME_NOT_FOUND`-for-unhandled-types change, and the WoW64 lead-byte-table patch with hard-coded ntdll offsets. Touched files: `syscalls/locale.cpp`, `process_context.cpp`, `tools/create-root.bat`, `samples/test-sample/test.cpp`.

### Evidence

- ntdll x64: after loading code pages (it writes PEB `+0x34c/+0x34e` itself), it reads `PEB.UseCaseMapping` (`+0x350`) and only when non-zero calls `ZwGetNlsSectionPtr(0xE, 0, 0, &ptr, 0)`; that call is the only writer of the table `RtlUpcaseUnicodeChar` uses for chars `>= 0xC0`. If the table is null, `RtlUpcaseUnicodeChar` returns the input unchanged (no crash). ntdll never writes `UseCaseMapping`.
- ntdll x86 (WoW64): same gate on `PEB32 +0x22c`.
- The kernel's actual `UseCaseMapping` value could not be checked (no `ntoskrnl.exe` locally). Since ntdll never sets it and real Windows does upcase non-ASCII through this path, the kernel must set it non-zero; ntdll only tests `!= 0`.
- If `l_intl.nls` is missing, the section-14 call fails and ntdll falls back to null tables — today's behaviour, so roots without the file are unaffected.

### Build/test results

- `cmake --build --preset=release` (macOS arm64) — clean. `clang-format` clean. Homebrew `clang-tidy` on the changed lines — no diagnostics.
- New test-sample case `Unicode Case Mapping` (`RtlUpcaseUnicodeChar`/`RtlDowncaseUnicodeChar` on U+00E4/U+00C4/U+0430/U+0410), private root copy with the real `l_intl.nls` (9926 bytes, from a Windows-harvested root's `syswow64`) copied into `system32`:
  - `origin/main` without the file: Fail. `origin/main` with the file: Fail (section 14 never requested). This branch without the file: Fail. This branch with the file: Success.
- Standalone 32-bit and 64-bit programs calling the same functions: `up(E4)=00C4 up(430)=0410 down(C4)=00E4` for both bitnesses.
- `windows-emulator-test`: 165 ran, 157 passed, 2 skipped, 6 failed — identical to `origin/main` with the same root. The 6 failures are test-sample's `CryptProtect` subtest (`CryptProtectData failed: 1829`), pre-existing with a private root; `Unicode Case Mapping` passes inside them.

### PR body text

```markdown
## Problem

`RtlUpcaseUnicodeChar`/`RtlDowncaseUnicodeChar` handle ASCII inline and map
everything from U+00C0 up through a table that ntdll fills only from
`NtGetNlsSectionPtr` section type 14 (`l_intl.nls`). ntdll requests that
section only when `PEB.UseCaseMapping` is non-zero, and never sets the field
itself. Sogen left it at 0 and did not implement section 14, so non-ASCII case
mapping silently returned its input (`RtlUpcaseUnicodeChar(U+00E4) == U+00E4`).

## Fix

- `NtGetNlsSectionPtr`: map `System32\l_intl.nls` for section type 14, sharing
  the file-mapping code with section type 11.
- Set `UseCaseMapping = 1` in PEB64 and PEB32. The exact value the kernel
  writes was not verified; ntdll only tests it for non-zero.
- `create-root.bat`: collect `l_intl.nls`.
- test-sample: `RtlUpcaseUnicodeChar`/`RtlDowncaseUnicodeChar` on U+00E4,
  U+00C4, U+0430 and U+0410.

Roots without `l_intl.nls` keep today's behaviour: the section request fails
and ntdll falls back to ASCII-only mapping. The new test-sample case needs a
root built with the updated `create-root.bat`.

## Verification

- Release build on macOS/arm64; `clang-format` clean.
- New test case: fails on `origin/main` (with or without `l_intl.nls` in the
  root), passes with this change and the file present.
- Standalone 32-bit (WoW64) and 64-bit programs: `RtlUpcaseUnicodeChar(0xE4)`
  = `0xC4`, `RtlUpcaseUnicodeChar(0x430)` = `0x410`,
  `RtlDowncaseUnicodeChar(0xC4)` = `0xE4`.
```

### Blocking concerns / caveats

- `l_intl.nls` is in no local `system32`; the copy used for testing came from a harvested root's `syswow64` (the file is data-only). CI gets it from `create-root.bat` on the Windows runners. A root that predates this change will fail the new test-sample case until it is regenerated.
- The kernel's real `UseCaseMapping` value was not checked (no `ntoskrnl.exe` available locally).

---

## create-root-audio-dlls

**PR title:** `chore(create-root): collect 4 missing audio/support DLLs`

**Branch:** `chore/create-root-audio-dlls` on `fork` (`https://github.com/JackTYM/sogen/pull/new/chore/create-root-audio-dlls`)
**Commit:** `8ac20819541941bf5d467013038895b64d72a85b` (one commit on `origin/main` @ `83566003`)
**File touched:** `src/tools/create-root.bat` (+4/-0)

### Review fixes applied

- Commit message no longer claims the DLLs "are all collected into the emulation root by guest apps"; it now says they are loaded by guest apps (Miles/WASAPI init, audioses' MMDeviceEnumerator activation, C++ runtimes using atomic wait) but `create-root.bat` never copies them.
- PR body: removed the `windows-emulator-test` (69/69) paragraph, since that binary does not exercise a `.bat`.

The four added lines are `audioeng.dll`, `audiokse.dll`, `msvcp140_atomic_wait.dll` and `twinapi.appcore.dll`, placed at their alphabetical positions. `audioses.dll` and `avrt.dll` were already in `origin/main`'s list. No C++ changed, so no build was run; the script cannot be executed on this macOS host and is verified by diff inspection only.

### PR body

```markdown
## Problem

`src/tools/create-root.bat` builds the emulation root by collecting a curated allow-list of host
system DLLs into the guest filesystem. Four DLLs that guest apps load are missing from that list
(create-root.bat never copies them):

- `audioeng.dll` / `audiokse.dll` - WASAPI/audio-engine DLLs that some titles' audio middleware
  (e.g. Miles) needs; when they are absent from the root, audio initialization fails.
- `twinapi.appcore.dll` - `audioses.dll`'s `IAudioClient::Initialize` path COM-activates
  `MMDeviceEnumerator` through it; without it in the root, `IAudioClient::Initialize` fails with
  `ERROR_GEN_FAILURE` before any RPC is attempted.
- `msvcp140_atomic_wait.dll` - exports `__std_atomic_notify_all_direct` and related atomic-wait
  entry points that C++ runtimes resolve at load time; without it, loading a dependent DLL fails
  with `STATUS_ENTRYPOINT_NOT_FOUND`.

## Fix

`src/tools/create-root.bat`: add `CALL :collect` lines for all four DLLs at their
alphabetically-correct position in the existing list.

## Verification

`create-root.bat` is a host-side setup script that populates an emulation root from a real
Windows machine's `System32`; it cannot be run on a macOS host, so the change is verified by
inspecting the diff against `origin/main`'s current file.
```

### Caveats

- Not executed; the repro trigger for `msvcp140_atomic_wait.dll` is described in general terms.
- The PR was not opened; it is only pushed to `fork`.

---

## pump-ui-events-vcpu-spawn

**PR title:** `fix(windows-emulator): pump UI events once before spawning vCPU workers`

**Branch:** `fix/pump-ui-events-before-vcpu-spawn` on `fork` (`https://github.com/JackTYM/sogen.git`), based on `origin/main`
**Commit:** `7f80bba900c136f261486097931ceb296d2a3efa`

### Source

Found via `git log --oneline integration/fex-perf-plus-d3d9-umd -S"pump_events" -- src/windows-emulator/windows_emulator.cpp`: commit `4076b8a5` "fix(windows-emulator): pump UI events once before spawning vCPU workers" — a clean, self-contained 6-line diff. Cherry-picked cleanly onto `origin/main` with zero conflicts.

### Verification done in this task

- Read `src/windows-emulator/ui_backends/sdl_ui_backend.cpp` on the post-cherry-pick tree and confirmed the race is real against **current** main, not just historically: `pump_events()` sets `ui_thread_known_`/`ui_thread_id_` before running `ensure_initialized()` (one-time SDL/Cocoa init), and `queue_or_run()` executes inline on any calling thread until `ui_thread_known_` is set — so a vCPU worker's guest syscall (e.g. `create_window()`) can still win that race today.
- Build: `cmake --preset=release` then `cmake --build --preset=release --target windows-emulator-test` — succeeded.
- Test: `EMULATOR_ROOT=.../build/release/artifacts/root ./build/release/artifacts/windows-emulator-test` — **69/69 passed**, both before and after the comment trim.
- `clang-format --dry-run -Werror src/windows-emulator/windows_emulator.cpp` — clean, both before and after.

### Rubric review — what was changed

The only issue found: the added comment was 4 comment lines for 1 code line (~67% density), over the ~30% flag threshold. Checked against CLAUDE.md's "no comment by default" bar — it does carry genuinely non-recoverable information (a non-obvious ordering/threading constraint enforced by a sibling file's internal state machine, not visible locally), so not deleted, but trimmed from 4 lines to 3 while keeping the load-bearing content (why here, what race, what UB consequence). Rebuilt and re-ran the full test suite after the edit — still 69/69, clang-format still clean.

Also dropped the original commit message's specific "65 further MW2 --vcpus 2 HVF attempts (0 crashes...), 26/26 smoke tests, 33/33 windows-emulator-test" verification claims, since those live MW2 runs were not reproduced in this task — only the unit-test and build verification above was actually run and can be stood behind.

No other rubric issues found: single self-contained, backend-agnostic fix; only file touched is named in the message; no ABI-stability comments; no workaround-vs-source-fix tension (this is the correct fix site); no test-coverage gap beyond reasonable for a threading-race fix in a manual-UI codepath; no CI/build-system changes needed; checked open PRs on `momo5502/sogen` (`gh pr list`) — no overlap.

### PR body text

```markdown
## Problem

With multiple vCPUs, `windows_emulator::start()` spawned all vCPU worker threads and let them
start running guest code immediately, before the main thread's own first
`ui_backend_->pump_events()` call. `sdl_ui_backend` only learns which thread owns the UI
(`ui_thread_known_`/`ui_thread_id_`) inside that first `pump_events()` call; until then,
`queue_or_run()` treats whichever thread calls it as the owner and runs the task inline instead
of queuing it (`src/windows-emulator/ui_backends/sdl_ui_backend.cpp`). A vCPU worker whose guest
syscall created the first window (e.g. via `create_window()`) could therefore win that race and
run SDL's one-time `SDL_InitSubSystem(SDL_INIT_VIDEO)`/`Cocoa_RegisterApp`/`NSApplication` init on
a non-main thread — undefined behavior for Cocoa/AppKit that intermittently crashed inside
Apple's own frameworks (a null-deref in CFBundle's localized-string loading, or an uncaught
`NSException` from `NSAppearance`'s CoreUI init), well before any frame was presented.

This was found and reproduced via MW2 (`iw4sp.exe`) under `--vcpus 2` on the FEX/HVF backend, but
the underlying multi-vCPU worker-thread architecture (and thus the race) predates that backend
integration — it's backend-agnostic and affects any multi-vCPU backend, not something FEX/HVF-
specific.

## Fix

`src/windows-emulator/windows_emulator.cpp`: in the multi-vCPU branch of `start()`, call
`this->ui_backend_->pump_events()` once on the main thread before entering the loop that spawns
the vCPU worker threads. This deterministically establishes the calling (main) thread as the UI
backend's owner and runs the backend's one-time init there first, closing the race — a worker
thread can no longer win it.

## Verification

- Build: `cmake --preset=release` (arm64 macOS 25.6.0/Darwin, cmake 4.4.0), then
  `cmake --build --preset=release --target windows-emulator-test` — succeeds.
- Test: `EMULATOR_ROOT=<repo>/build/release/artifacts/root ./build/release/artifacts/windows-emulator-test`
  — **69/69 tests pass** (arm64 macOS, all configured backends: unicorn/icicle/backend-selection
  test suites included in this binary).
- `clang-format --dry-run -Werror` on the only touched file — clean.
- Confirmed the race this fixes is real against current `main`: `sdl_ui_backend.cpp`'s
  `pump_events()`/`queue_or_run()` still have the same "first caller wins ownership" shape
  described above.
```

### Blocking concerns / caveats

- The pushed commit and PR body deliberately do **not** repeat the source commit's live-MW2-crash-rate figures ("3 crashes in 60 boot-to-title attempts", "65 further attempts, 0 crashes"), since those live MW2 sessions were not personally re-run in this task — only build/unit-test/format verification above was actually executed.

---

## jobobject-query-zerofill

**DROPPED 2026-10-11 - do not open.** Upstream now emulates job objects (#1405, `NtSetInformationJobObject` accepts class 9), so the premise "no job object information class is implemented yet" no longer holds, and the change (STATUS_NOT_SUPPORTED -> STATUS_INVALID_INFO_CLASS) fixes no demonstrated bug. Revisit only as part of a real `NtQueryInformationJobObject` implementation. Original notes follow.

**PR title:** `fix(syscalls): return STATUS_INVALID_INFO_CLASS from NtQueryInformationJobObject`

**Branch:** `fix/job-object-query-info-class` on `fork` (JackTYM/sogen)
**Commit:** `e3b4ad8b2b8202dcfdf020f845b5b605ed079518`

### What was done

Checked out `handle_NtQueryInformationJobObject` on `origin/main` — it's a bare zero-parameter stub returning `STATUS_NOT_SUPPORTED`, no buffer touched at all. **Per the corrected task scoping, did not apply commit `c6bde025`'s zero-fill shape.** Instead found real in-repo precedent (`handle_NtQueryInformationFile` in `file.cpp`, `handle_NtQueryEvent` in `event.cpp`) where an unrecognized/out-of-range info class is checked first and returns `STATUS_INVALID_INFO_CLASS` before any buffer access — matching real NT behavior where an unsupported info class never touches the output buffer. Applied that same pattern: a 1-line change, `STATUS_NOT_SUPPORTED` → `STATUS_INVALID_INFO_CLASS`, with the stub still touching nothing (no buffer/params added, since no info class is implemented and none needs to be for this fix).

### Build/test

`cmake --preset=release` configured clean; `cmake --build --preset=release --target windows-emulator-test` built clean (steam-bridge-generator warning pre-existing/unrelated). Ran `windows-emulator-test` with `EMULATOR_ROOT=.../build/release/artifacts/root`: **69/69 tests passed**, no failures. `clang-format --dry-run -Werror` on the touched file: clean, no diff.

### Rubric review

Comment density 0% (no comments added). No overlap found (`gh pr list --repo momo5502/sogen --search "JobObject"` → empty). No test added — there's no existing per-syscall-handler unit test pattern in `src/windows-emulator-test/` to extend for this (tests there cover memory/modules/registry/unicode infrastructure, not individual NT syscall handlers in isolation), and this isn't a regression fix requiring a reproduction test. Nothing else flagged.

### Full PR body text

```markdown
## Problem

`handle_NtQueryInformationJobObject` in `src/windows-emulator/syscalls.cpp` is a
bare stub that returns `STATUS_NOT_SUPPORTED` for every call, regardless of the
requested `JOBOBJECTINFOCLASS`. This diverges from real NT behavior: on real
Windows, querying an unrecognized/unimplemented job object information class
returns `STATUS_INVALID_INFO_CLASS`, and does not touch the caller's output
buffer at all.

This codebase already has two real precedents for exactly this situation, both
of which check the requested info class up front and return
`STATUS_INVALID_INFO_CLASS` before any buffer work:

- `handle_NtQueryInformationFile` (`src/windows-emulator/syscalls/file.cpp`):
  `if (info_class == 0 || info_class >= FileMaximumInformation) { return
  STATUS_INVALID_INFO_CLASS; }`
- `handle_NtQueryEvent` (`src/windows-emulator/syscalls/event.cpp`):
  `if (event_information_class != 0) { return STATUS_INVALID_INFO_CLASS; }`

## Fix

`src/windows-emulator/syscalls.cpp`: changed `handle_NtQueryInformationJobObject`
to return `STATUS_INVALID_INFO_CLASS` instead of `STATUS_NOT_SUPPORTED`. No job
object information class is implemented yet, so the handler reports that
consistently for every call — matching the existing precedents instead of
inventing a new failure code, and without writing to the caller's buffer (a
prior draft of this fix zero-filled the output buffer while still returning a
failure status; that shape was rejected because it doesn't match real NT
behavior, and this repo's own two existing zero-fill-on-failure precedents pair
zero-fill with `STATUS_SUCCESS`, not a failure code).

## Verification

- `cmake --preset=release` — configured clean (unrelated steam-bridge-generator
  warning: `libclang` pip module not installed, pre-existing/unrelated to this
  change).
- `cmake --build --preset=release --target windows-emulator-test` — built clean.
- `EMULATOR_ROOT=<repo>/build/release/artifacts/root
  ./build/release/artifacts/windows-emulator-test` (macOS host, arm64) —
  **69/69 tests passed**, 0 failures.
- `clang-format --dry-run -Werror src/windows-emulator/syscalls.cpp` — no diff.
```

**Files touched:** `src/windows-emulator/syscalls.cpp`.

### Blocking concerns / caveats

None. Confirmed this implements the corrected design (STATUS_INVALID_INFO_CLASS, no buffer write) rather than the earlier-rejected zero-fill-plus-failure-status shape — see Flags section above.

---

## vulkan-shim-missing-exports

**PR title:** `fix(vulkan-shim): add 33 missing exports to vulkan_shim.def`

**Branch:** `fix/vulkan-shim-def-missing-exports` on `fork` (`https://github.com/JackTYM/sogen.git`)
**Commit:** `d176be45e37aa8c0a268fe1430ebad7a3dd10391` (one commit on `origin/main` @ `83566003`)
**PR link (not yet opened, only pushed):** https://github.com/JackTYM/sogen/pull/new/fix/vulkan-shim-def-missing-exports
**File touched:** `src/samples/vulkan-shim/vulkan_shim.def` (33 insertions)

### Review fixes applied

- Removed the "DXVK statically imports several of these by name" claim. Nothing in this repo statically imports `vulkan-1.dll` (only `install-gpu-runtime.ps1` copies the shim in as `vulkan-1.dll` and `vulkan_host.cpp` loads the host loader), and DXVK resolves Vulkan through `vkGetInstanceProcAddr`/`vkGetDeviceProcAddr` after `LoadLibrary`, so the claim could not be substantiated. Motivation is reduced to keeping the export list in sync with the dllexport set so x86 `GetProcAddress` callers and static importers see undecorated names.
- Deleted the narration paragraph about re-checking the 33 names against `origin/main`.
- Removed the MSVC-only gating statement and the "MinGW-built, unaffected" sentence: `src/samples/vulkan-shim/CMakeLists.txt` adds the `.def` under `if(WIN32)`, which includes MinGW.
- Dropped the test count; no test exercises this file.

The diff is 33 added `vk*` lines (243 total entries in the file afterwards). No C++ changed, so no build was run; the `vulkan-shim` target is not part of the macOS host build.

### PR body

```markdown
## Problem

`src/samples/vulkan-shim/vulkan_shim.def` exports the guest Vulkan shim's entry points under
their plain, undecorated names. On x86 (WoW64) guests `VKAPI_CALL` is `__stdcall`, which
decorates exports (e.g. `_vkCreateSemaphore@16`), so a guest `GetProcAddress("vkCreateSemaphore")`
would miss the decorated symbol. The `.def` list had drifted out of sync with `vulkan_shim.cpp`:
semaphore, event, sync2, descriptor-update-template and compute-pipeline entry points were added
to the `.cpp` without a matching `.def` entry.

## Fix

`src/samples/vulkan-shim/vulkan_shim.def`: add the 33 missing entries next to their same-family
neighbors, so the export list matches the dllexport set and x86 `GetProcAddress` callers and
static importers see undecorated names for all of them:

`vkBindBufferMemory2`, `vkBindImageMemory2`, `vkCmdCopyBuffer2`, `vkCmdResetEvent`,
`vkCmdResetEvent2`, `vkCmdSetEvent`, `vkCmdSetEvent2`, `vkCmdWaitEvents`, `vkCmdWaitEvents2`,
`vkCmdPipelineBarrier2`, `vkCreateComputePipelines`, `vkCreateDescriptorUpdateTemplate`,
`vkCreateEvent`, `vkCreateSemaphore`, `vkDestroyDescriptorUpdateTemplate`, `vkDestroyEvent`,
`vkDestroySemaphore`, `vkGetBufferDeviceAddress`, `vkGetBufferMemoryRequirements2`,
`vkGetDeviceBufferMemoryRequirements`, `vkGetDeviceImageMemoryRequirements`,
`vkGetDeviceImageSubresourceLayoutKHR`, `vkGetEventStatus`, `vkGetImageMemoryRequirements2`,
`vkGetImageSubresourceLayout2KHR`, `vkGetMemoryWin32HandleKHR`, `vkGetSemaphoreCounterValue`,
`vkQueueSubmit2`, `vkResetEvent`, `vkSetEvent`, `vkSignalSemaphore`,
`vkUpdateDescriptorSetWithTemplate`, `vkWaitSemaphores`.

## Verification

The `vulkan-shim` target is Windows-guest-only and is not built on the author's macOS host, so
this was verified at the source level: each added name is defined in `vulkan_shim.cpp` as a
`__declspec(dllexport)` function and was absent from the `.def`.
```

### Caveats

- Not built or loaded under a real DXVK; source-level verification only.
- The PR was not opened; it is only pushed to `fork`.

---

## sched-double-scan

**PR title:** `perf(sched): eliminate switch_to_next_thread's redundant second full scan`

**Branch:** `perf/sched-switch-to-next-thread-single-scan` on `fork` (`https://github.com/JackTYM/sogen.git`)
**Commit:** `4f2e3a47b77f66b96626d8778844149e005e4c49` (one commit on `origin/main` @ `83566003`; code unchanged, message only)
**File:** `src/windows-emulator/windows_emulator.cpp`

Note: `perf/sched-idle-backoff-poll` is stacked on the previous tip of this branch (`3d2c3157`) and was deliberately not rebased; it needs a rebase onto `4f2e3a47` once its own review finishes.

### Review fixes applied

- Replaced "provably guaranteed to fail" and "byte-identical selection order" with: same order; a thread whose timed wait expires mid-scan is picked on the next call.
- Deleted the `## Notes` paragraph about the stripped diagnostic.
- Replaced the 69-test result with the numbers below and labelled the 2255 calls/sec and 23.6 iterations/call figures as coming from a non-upstream MW2/FEX setup.

### Build/test results

- macOS arm64, `cmake --preset=release` without the FEX backend (pinned FEX submodule does not compile against the installed fmt).
- `windows-emulator-test`: 165 tests ran, `[  PASSED  ] 157 tests`, 2 skipped (FEX register tests), 6 failed out of 165 (build without the FEX backend on a private root); the same 6 (`EmulationTest.*`, `SerializationTest.*`) fail identically on unmodified `origin/main` because `test-sample`'s CryptProtect case fails against that root (dpapi), unrelated to this change.
- Smoke: `analyzer -s test-sample.exe` (locally MinGW-built x64): 33 cases `Success`; the only failing case is the pre-existing CryptProtect one.
- `clang-format --dry-run -Werror src/windows-emulator/windows_emulator.cpp`: clean.

### PR body

```markdown
## Problem

`switch_to_next_thread` (`src/windows-emulator/windows_emulator.cpp`) picks the next runnable
thread with two back-to-back loops over the full thread container: the first walks from the
successor of the active thread to the end, the second unconditionally walks the whole container
again from the beginning. Both loops visit threads in the same order, so any call that exhausts
the first loop (no ready thread among the active thread's successors) re-checks every one of
those already-not-ready threads a second time before reaching the untried predecessors.

On a non-upstream MW2/FEX setup, an instrumented pass during gameplay measured 2255 calls/sec to
`switch_to_next_thread`, averaging 23.6 scan iterations per call against ~18.7 live threads,
i.e. close to a full double scan on almost every call. These numbers cannot be reproduced on
`origin/main`.

## Fix

`src/windows-emulator/windows_emulator.cpp`: restructure `switch_to_next_thread` into a
locate-active pass followed by exactly two non-overlapping segments (successors of the active
thread, then predecessors plus the active thread) instead of a full second pass. Worst-case cost
is bounded at `N + active_index + 1` instead of a flat `2N`, with no added cost on the common
early-success path. The selection order and the fallback behavior (including the
active-thread-not-found case, which falls back to a single full scan) are the same; a thread
whose timed wait expires mid-scan is picked on the next call.

`is_thread_ready`'s only cross-thread side effect (`consume_object_signal`) fires exclusively on
the same check that causes an immediate return, so a call that reached the eliminated tail
changed no state since the first loop's check failed, and rechecking the successors fails again.
Sticky-vCPU affinity and APC dispatch are enforced inside `switch_to_thread`, not by scan order.

## Verification

macOS arm64, `cmake --preset=release`, `windows-emulator-test`: 157/165 passed, 2 skipped; the 6
failures are identical on unmodified `origin/main` (test-sample's CryptProtect case against the
local root's dpapi data). `analyzer -s test-sample.exe` smoke run shows no change.
`clang-format --dry-run -Werror` on the touched file: clean.
```

### Caveats

- The performance figures come from a non-upstream MW2/FEX setup and were not re-measured.
- The PR was not opened; it is only pushed to `fork`.

---

## sched-idle-backoff

**PR title:** `perf(sched): back off the idle poll instead of yield-spinning on a parked host wait`

**Branch:** `perf/sched-idle-backoff-poll` on `fork` (`https://github.com/JackTYM/sogen.git`)
**Commit:** `fdeb52ef53c97a75f9d9d485e2c88f1ecfa3e2a3`

Built on top of the sibling PR's actual pushed tip (`fork/perf/sched-switch-to-next-thread-single-scan` @ `94f7e05544bb38cca4f2dfa1e9be2a592d6f454d`), which itself sits on `origin/main`. This branch adds exactly one new commit on top of that dependency, per the task's explicit stacking instruction.

### Design decision: dropped the diagnostic addition, shipped only the algorithmic backoff fix

The sibling PR's report confirmed it stripped `wait_storm_diag.hpp` entirely (never introduced it, not on `origin/main`, not on its branch). Since `f05c4bc2`'s diagnostic hunk (the `scan_timer` RAII counter + `scan_cost` report line) is a pure extension of infrastructure that no longer exists on the base being built on, the agent adapted rather than reintroducing it: cherry-picked/hand-applied only the algorithmic half of `f05c4bc2` — the bounded-hot-poll-then-exponential-backoff scheme in `perform_thread_switch` — and left the diagnostic out, consistent with the sibling's own reasoning (one-off investigation instrument, matches the repo's documented maintainer preference for perf-only additions to be opt-in plugins, and rubric criterion #15 "no leftover debug hooks").

Cherry-picking `f05c4bc2` directly conflicted (modify/delete conflict on `wait_storm_diag.hpp`, since it doesn't exist on the target base; content conflict in `windows_emulator.cpp` around `perform_thread_switch`, since the base's version of that function was already restructured by the sibling's single-scan fix and also lacks the pre-existing, unrelated `EMULATOR_SCHED_DIAG`/`idle_spin_count` scaffolding that was present when `f05c4bc2` was originally authored — that scaffolding came from an earlier, unrelated commit `249b2fbf` and was never part of this candidate's actual diff). Aborted the cherry-pick and hand-applied only the real diff content of `f05c4bc2` (verified via `git show f05c4bc2` — two hunks: the counter declarations before the `while` loop, and the hot-poll/backoff branch replacing the plain `std::this_thread::yield()`) directly onto the sibling branch's already-restructured `perform_thread_switch`.

### Files touched

`src/windows-emulator/windows_emulator.cpp` only (1 file, +22/-1). No `wait_storm_diag.hpp` changes (correctly absent — file doesn't exist on this base). `emulator_thread.cpp` was not touched; the candidate's file list mentioned it as a possibility but the actual content (verified via `git show f05c4bc2`) never touched it.

### Build/test results (macOS 26.6.2, arm64, Apple Silicon, `--preset=release`)

- `cmake --preset=release` — configured clean (same pre-existing, unrelated steam-bridge/libclang `ModuleNotFoundError` warning noted in the sibling's report).
- `cmake --build --preset=release --target windows-emulator-test` — built clean.
- `EMULATOR_ROOT=.../build/release/artifacts/root ./build/release/artifacts/windows-emulator-test`: **69/69 tests passed** (`[==========] 69 tests from 15 test suites ran. [ PASSED ] 69 tests.`), including `EmulationTest.BasicEmulationWorks`/`CountedEmulationWorks`/`CountedEmulationIsAccurate`, which drive `test-sample.exe` to completion and exercise `perform_thread_switch`'s idle path through real guest thread scheduling (including its `should_stop`/relock path).
- `clang-format --dry-run -Werror src/windows-emulator/windows_emulator.cpp` — no diff.

Did not re-run the live MW2 FEX+HVF perf comparison in this pass (no live MW2 session available) — the `scan_cost`/fps numbers in the commit message and PR body are cited as prior-measured motivation only, explicitly flagged as not independently reverified in this task.

### Rubric review findings

- Comment density on the touched hunk: 6 comment lines / 22 added lines ≈ 27%, under the ~30% flag threshold. The 6-line comment block explains a genuine deliberate deviation (why plain-yield was replaced, and why the obvious "just sleep" alternative wasn't taken either) — justified under this repo's "no comment by default" bar, kept as-is (same text as the original commit's own comment, unchanged).
- No ABI comments, no workarounds, no platform gating — backend-agnostic scheduler code.
- Checked for overlap: `gh pr list --repo momo5502/sogen --state open` shows no open PR touching `windows_emulator.cpp`/`perform_thread_switch`/scheduler idle path.
- Single self-contained change: only the idle-pacing algorithm, nothing else.
- No leftover debug hooks: confirmed `wait_storm_diag`/`EMULATOR_WAIT_STORM_DIAG` do not appear anywhere in the diff or resulting file.

### PR body (drafted, not submitted)

```markdown
## Problem

`perform_thread_switch`'s idle path (`src/windows-emulator/windows_emulator.cpp`) yielded, never slept, for as long as any thread held a parked host wait (e.g. a GPU fence). With nothing else runnable, that turns every idle vCPU into a spinner re-running `switch_to_next_thread`'s full O(threads) readiness scan as fast as the host will schedule it — the poll can cost far more host CPU than the wake latency it saves, which made parking a short GPU wait unaffordable.

Live-measured on MW2 (native D3D9 UMD, FEX+HVF, `--vcpus 2`), 12-minute windows at matched settled gameplay (draws/frame ~2010), with a D3D9 flush park enabled so something is actually parked:

- yield-spin (before): scan_cost 118.3-124.9 ms/s, fps median 70.3-73.3
- backoff (after): scan_cost 11.6-12.5 ms/s, fps median 77.0-78.0

(`scan_cost` was measured via a since-removed opt-in diagnostic (`EMULATOR_WAIT_STORM_DIAG`, not part of this PR); cited here as motivation only, not reverified in this pass — see Verification.)

## Fix

`src/windows-emulator/windows_emulator.cpp`: `perform_thread_switch`'s idle branch for a pending host wait now polls hot for a bounded 16 scans (a host wait is frequently already signaled or about to be), then falls back to a real sleep that backs off 100us -> 1ms — the same ceiling the timed-wait branch right next to it already uses. `host_wait_polls`/`host_wait_sleep` are declared fresh on each call (not `static`), so the counters reset per idle episode and a wait that resolves quickly never reaches the sleep tier at all.

Builds on #<sibling-PR-number> (`switch_to_next_thread`'s redundant-second-scan fix) — this PR's diff, taken alone, only touches the idle-wait pacing inside `perform_thread_switch`; it does not change scan order or readiness selection.

## Verification

Built and tested on macOS arm64 (Apple Silicon host), `--preset=release`:

```
cmake --preset=release
cmake --build --preset=release --target windows-emulator-test
EMULATOR_ROOT=<repo>/build/release/artifacts/root ./build/release/artifacts/windows-emulator-test
```

Result: `[==========] 69 tests from 15 test suites ran. [  PASSED  ] 69 tests.` — including `EmulationTest.BasicEmulationWorks`/`CountedEmulationWorks`/`CountedEmulationIsAccurate`, which run `test-sample.exe` to completion end-to-end and exercise `perform_thread_switch`'s idle path (including its relock/`should_stop` handling) through real guest thread scheduling.

`clang-format --dry-run -Werror` on the only touched file (`src/windows-emulator/windows_emulator.cpp`) reports no diff.

The live MW2 fps/scan_cost numbers above are prior measurements cited as motivation; they were not re-collected in this verification pass (no live MW2 session run here) — flagged explicitly rather than re-asserted as freshly confirmed.

## Notes

An earlier pass on this work also extended an opt-in `EMULATOR_WAIT_STORM_DIAG` diagnostic to report the scan's wall-clock cost (`scan_cost`) alongside its existing iteration-count reporting. That instrumentation is not part of this PR: it doesn't exist on this branch's base (the sibling single-scan PR didn't carry it forward, for the same reason — single-purpose investigation tooling, not a permanent addition, matching this repo's stated preference for perf-only additions to be opt-in plugins once a plugin system exists rather than baked into `windows_emulator.cpp`/`windows_emulator.hpp`). The `scan_cost` numbers above were captured with that since-removed instrumentation and are cited as motivation only.
```

### Blocking concerns / caveats

- This branch **depends on** `perf/sched-switch-to-next-thread-single-scan` (candidate sched-double-scan) — must be merged/opened in that order, or the PR body's `#<sibling-PR-number>` placeholder needs to be filled in once the sibling PR number is known.
- Live MW2 fps/scan_cost numbers (yield-spin 118.3-124.9 ms/s / 70.3-73.3 fps vs backoff 11.6-12.5 ms/s / 77.0-78.0 fps) are prior measurements cited as motivation only, not re-collected in this verification pass.
- `wait_storm_diag.hpp`/`EMULATOR_WAIT_STORM_DIAG` deliberately not carried forward — see Flags section.
