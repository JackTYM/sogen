# PR1234-Split — Ready to Open

Generated 2026-09-06. All 13 branches below were independently re-verified (required/sensible, up-to-date, comment-balanced), reviewed against a 16-criterion rubric derived from momo5502's/Igoorx's real historical PR feedback, fixed where real blocking concerns were found, squashed to one commit each (diff verified byte-identical before/after squash for every branch), rebuilt, and pushed live to `fork` = `JackTYM/sogen` under their canonical branch names. This effort is DONE except for you manually opening each PR on GitHub.

## Recommended opening order

1. `fix/wow64-heaven-gate-module-registration`
2. `chore/create-root-extra-dlls`
3. `fix/wow64-module-base-below-4gb`
4. `fix/raise-exception-exit-status`
5. `fix/apiset-zero-value-entries`
6. `fix/bcrypt-provider-parsing`
7. `feat/registry-transaction-stubs`
8. `fix/ordinal-only-exports`
9. `fix/handle-4-byte-alignment`
10. `wt/macos-vulkan-bringup`
11. `fix/wow64-userconnect-sharedinfo-layout`
12. `feat/gdi-user-syscalls`
13. `wt/macos-vulkan-dxvk-spoof`

## Safe to open in parallel

**`fix/wow64-heaven-gate-module-registration`, `chore/create-root-extra-dlls`, `fix/wow64-module-base-below-4gb`, `fix/raise-exception-exit-status`, `fix/apiset-zero-value-entries`, `fix/bcrypt-provider-parsing`, `feat/registry-transaction-stubs`, `fix/ordinal-only-exports`, `fix/handle-4-byte-alignment`, `wt/macos-vulkan-bringup`, `fix/wow64-userconnect-sharedinfo-layout`**

All 55 pairs among these 11 branches returned exit 0 from `git merge-tree --write-tree --merge-base=origin/main`, AND a sequential `git merge --no-ff` of all 11 onto a detached origin/main in a scratch worktree (in this exact order) completed with zero conflicts. The three file overlaps inside this batch are all far-apart hunks that auto-merge: src/samples/test-sample/test.cpp is touched by handle-4-byte-alignment (+test at :1861, RUN_TEST at :1995), registry-transaction-stubs (+test at :617, RUN_TEST at :1955), raise-exception-exit-status (main body at :1906) and bcrypt-provider-parsing (+test at :1889, RUN_TEST at :1991) at distinct insertion points; src/windows-emulator/syscalls/user.cpp is touched by handle-4-byte-alignment at lines 342-347 and 4361-4368 vs userconnect-sharedinfo-layout at 1265 and 1700-1721; src/windows-emulator/module/module_mapping.cpp is touched by wow64-module-base-below-4gb at 532-566 vs ordinal-only-exports at 59-248. Ordering within the batch is risk-ascending by size/entanglement: two 1-line changes first, then small self-contained fixes, then the ones touching shared core headers (handles.hpp/process_context.hpp in handle-4-byte-alignment; the public emulator-platform/platform/user.hpp in userconnect-sharedinfo-layout) last.

**`feat/gdi-user-syscalls`**

Must be opened/merged only AFTER fix/bcrypt-provider-parsing lands, then rebased. Verified REAL conflict: both branches insert a new test function at the tail of the anonymous namespace in src/samples/test-sample/test.cpp (bcrypt `test_bcrypt_hash` at :1889, gdi `test_set_dib_bits_to_device` at :1888) and both append a RUN_TEST line at the same spot (:1946). The conflict is symmetric — sequential merge with bcrypt first fails on gdi, and with gdi first fails on bcrypt — so you can equally swap them (put gdi in batch 1, bcrypt here). Resolution is trivial (keep both hunks) but it is a genuine textual conflict, not an auto-merge. gdi is otherwise clean against every other branch (all 11 remaining pairs exit 0).

**`wt/macos-vulkan-dxvk-spoof`**

Must be opened AFTER wt/macos-vulkan-bringup merges, as a stacked PR. The two were pushed as siblings (each exactly 1 commit off origin/main) but dxvk-spoof is content-wise a superset of bringup: every added line of bringup's src/windows-emulator/devices/vulkan_host.cpp diff also appears in spoof's diff except one, which spoof refactors (bringup's `if (!requests_portability_subset && impl::is_portability_device(instance->second, pd->second))` becomes a hoisted `const bool portability = impl::is_portability_device(...)` reused across three new call sites). As siblings they hard-conflict in BOTH docs/gpu-paravirtualization.md and vulkan_host.cpp (merge-tree exit 1, sequential merge FAIL). However, the incremental patch `git diff fork/wt/macos-vulkan-bringup fork/wt/macos-vulkan-dxvk-spoof` was verified with `git apply --check` to apply cleanly on a tree that already has bringup merged. So: merge bringup, then re-derive spoof from that incremental diff (or rebase its single commit onto bringup, resolving the same two files) and force-push before opening its PR.

## File-overlap notes (verified via real `git merge-tree`, not guessed)

- REAL CONFLICT (merge-tree exit 1): fix/bcrypt-provider-parsing <-> feat/gdi-user-syscalls in src/samples/test-sample/test.cpp — both add a new test function at test.cpp:1888/1889 and a RUN_TEST at :1946 (same insertion points). Symmetric: whichever merges second needs a rebase. Trivial keep-both resolution.
- REAL CONFLICT (merge-tree exit 1): wt/macos-vulkan-bringup <-> wt/macos-vulkan-dxvk-spoof in BOTH docs/gpu-paravirtualization.md and src/windows-emulator/devices/vulkan_host.cpp — spoof is a superset of bringup pushed as a sibling instead of a stack. The incremental diff bringup->spoof applies cleanly on top of bringup (verified `git apply --check`), so re-stack spoof onto bringup rather than opening both against main.
- Clean overlap (auto-merges, distinct hunks) in src/samples/test-sample/test.cpp among fix/handle-4-byte-alignment (+test :1861, RUN_TEST :1995), feat/registry-transaction-stubs (+test :617, RUN_TEST :1955), fix/raise-exception-exit-status (main :1906), fix/bcrypt-provider-parsing (+test :1889, RUN_TEST :1991) and feat/gdi-user-syscalls (+test :1888, RUN_TEST :2026). 5 branches touch this one file; every PR after the first to merge will show a GitHub 'branch out of date' but only bcrypt/gdi actually conflict. Note registry-transaction-stubs and bcrypt-provider-parsing each also add one #include near the top (:23 and :31) — auto-merges fine.
- Clean overlap (auto-merges, hunks ~1000-3000 lines apart) in src/windows-emulator/syscalls/user.cpp: fix/handle-4-byte-alignment (lines 342-347, 4361-4368) vs fix/wow64-userconnect-sharedinfo-layout (lines 1265, 1700-1721).
- Clean overlap (auto-merges, hunks ~300 lines apart) in src/windows-emulator/module/module_mapping.cpp: fix/wow64-module-base-below-4gb (lines 532-566) vs fix/ordinal-only-exports (lines 59-248). ordinal-only-exports is the heavier change here (143+/9-), so if a rebase is ever needed, rebase module-base-below-4gb onto it, not the reverse.
- Semantic (not textual) note: fix/handle-4-byte-alignment changes src/windows-emulator/handles.hpp and process_context.hpp, which are included nearly everywhere; and fix/wow64-userconnect-sharedinfo-layout changes the public src/emulator-platform/platform/user.hpp. Neither textually collides with any other branch, but they are the two most likely to need a rebuild/re-test of the other open PRs after landing, which is why they sit at the end of batch 1.

---

## Per-branch PR bodies

## `fix/wow64-heaven-gate-module-registration`

Commit: `fe93f115d3c258ee4d9312786fb4a0e2ce6e311e`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:fix/wow64-heaven-gate-module-registration

**Commit message:**
```
fix(wow64): register the synthetic heaven-gate module instead of skipping it

install_wow64_heaven_gate guarded the module insertion with
modules_.contains(kCodeBase) instead of its negation, so the
synthetic wow64_heaven_gate entry was never added: on the first
install the key is absent and the block is skipped.

Without the entry, every module lookup on the trampoline page
(find_by_address / find_name) fails, so the compat-mode ->
KiUserExceptionDispatcher transition of a 32-bit process is reported
as coming from <N/A> instead of wow64_heaven_gate.
```

**PR body (paste into GitHub):**

```markdown
## Summary

`module_manager::install_wow64_heaven_gate` (`src/windows-emulator/module/module_manager.cpp`) builds a synthetic `wow64_heaven_gate` module for the trampoline page at `0xFF300000`, but guards the insertion with `this->modules_.contains(kCodeBase)` instead of `!contains`. On the first (and only) install the key is absent, so the block is skipped and the module is never registered. This flips the condition.

One-line change, no other files touched.

## Impact

The gate page is entered from exactly one place: `exception_dispatch.cpp` sets `rip = kCodeBase` when an exception is dispatched to a thread running in 32-bit compat mode, so the trampoline can switch to the 64-bit `KiUserExceptionDispatcher`. Because the module was never registered, every `find_by_address` / `find_name` lookup on that page failed and the transition was attributed to `<N/A>`.

## Reproduction

32-bit console binary that deliberately raises an unhandled access violation (`c:/av32.exe`, PE32 i386, staged in the emulation root), default backend, macOS host:

```
./analyzer -e root -r root/registry c:/av32.exe
```

Before (origin/main `e8824779`):

```
Executing function: KiUserExceptionDispatcher (ntdll.dll) (0x1800a4330) via 0xff300011 (<N/A>)
Emulation terminated with status: C0000005
```

After:

```
Executing entry point: wow64_heaven_gate (0xff300000)
Executing function: KiUserExceptionDispatcher (ntdll.dll) (0x1800a4330) via 0xff300011 (wow64_heaven_gate)
Emulation terminated with status: C0000005
```

(`C0000005` is the AV the sample raises on purpose; that is unchanged.)

With `-v`, the before/after logs (45,270 vs 45,271 lines) differ in exactly that one line becoming those two; nothing else in the trace changes. A 32-bit sample that raises no exception (`heaven-gate-probe-x86.exe`, 862-line trace) produces identical output before and after, as expected since the gate is never executed.

## Verification

- `cmake --build --preset=release` (analyzer + windows-emulator-test) on the squashed commit: green.
- `windows-emulator-test` with `EMULATOR_ROOT` pointed at the real root: 50/50 passed, before and after squash.
- Baseline numbers above were produced by reverting the single line on the same tree and rebuilding, so the comparison is apples-to-apples.

## Notes

No unit test is added: `windows-emulator-test` only carries a 64-bit `test-sample`, and exercising this path needs a 32-bit PE that raises an exception, which the suite has no way to build today.

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Memory-note discrepancy confirmed: the branch contains NO test file and never did on this ref; the diff is exactly one line in src/windows-emulator/module/module_manager.cpp (+1/-1). PR body is grounded only in that.
- No unit test added: windows-emulator-test has only the 64-bit test-sample and no mechanism to build a 32-bit PE; the fix is only observable when a WoW64 thread takes an exception (exception_dispatch.cpp jumps to kCodeBase). Adding one is not cheap, so verification was done live with the 32-bit av32.exe in the emulation root instead.
- The remote fork branch previously pointed at 41711393, an older version of the same fix stacked on the ~129-file integration branch; it diverged from the squashed commit, so --force-with-lease (lease pinned to 41711393) was used, as instructed. History on the fork branch was rewritten.
- The verbose baseline-vs-fixed comparison required temporarily reverting the one line in the worktree and rebuilding analyzer; the source was restored via git checkout and analyzer rebuilt from the fixed source afterwards (tree verified clean before squash and before push).

</details>

---

## `chore/create-root-extra-dlls`

Commit: `d783a79a617efbaf38bae4ce045a591c515cc7a3`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:chore/create-root-extra-dlls

**Commit message:**
```
chore(create-root): collect displaycolormanagement.dll

coloradapterclient.dll, which create-root.bat already stages, activates
the Windows.Internal.Graphics.Display.DisplayColorManagement runtime
class at runtime, and combase resolves that class to
windows.internal.graphics.display.displaycolormanagement.dll. The
script never collected that DLL, so a root either lacks it or carries a
hand-staged copy from a different Windows build than the msvcp_win.dll
it imports, which the guest loader rejects with
STATUS_ENTRYPOINT_NOT_FOUND.
```

**PR body (paste into GitHub):**

```markdown
## Summary

`src/tools/create-root.bat` (run by the `Create Emulation Root` job in `ci-reusable.yml`) stages `coloradapterclient.dll` and `msvcp_win.dll` but not `windows.internal.graphics.display.displaycolormanagement.dll`, which `coloradapterclient.dll` pulls in at runtime. This adds one `CALL :collect` line so the DLL is snapshotted from the same Windows install as everything else, for both `system32` and `syswow64`.

One line, `IF EXIST`-guarded like every other `:collect` entry, so a runner without the DLL is unaffected.

## Why the DLL is needed

`coloradapterclient.dll` does not import it statically. It calls `RoActivateInstance` on the runtime class `Windows.Internal.Graphics.Display.DisplayColorManagement.DisplayColorManagement` (the class name is in its string table, both x64 and x86 builds), and `combase.dll` resolves that class through the SOFTWARE hive's `ActivatableClassId` registration, whose `DllPath` is `C:\Windows\System32\Windows.Internal.Graphics.Display.DisplayColorManagement.dll` (and the `SysWOW64` equivalent). The registry hives `create-root.bat` captures already contain that registration, so the load is attempted on every root the script produces; only the file was missing.

`mscms.dll` (already collected) is the loader of `coloradapterclient.dll`, so any guest that touches the colour-management stack reaches this path.

## Reproduction

Observed with Call of Duty: Modern Warfare 2 (`iw4mp.exe`, 32-bit, WoW64 guest, FEX backend on Apple Silicon):

```
Emulation terminated with status: C0000139   (STATUS_ENTRYPOINT_NOT_FOUND)
Error Message: __std_atomic_notify_all_direct
```

fired when `windows.internal.graphics.display.displaycolormanagement.dll` was mapped, reproducing within ~3 minutes on two back-to-back runs. Root cause was not emulator logic: the guest `ntdll.dll` loader was correctly rejecting an import. The local root had a hand-copied `displaycolormanagement.dll` from a newer Windows build that imports `__std_atomic_notify_all_direct` from `msvcp_win.dll`, while the script-collected `msvcp_win.dll` predates that export. Because the script never collected the DLL, there was no way for a root to carry a version-consistent copy.

## Verification

- After adding the line, the fork's `Build Branch` CI (`Create Emulation Root` on both windows-2022 and windows-2025 runners) produced roots containing the DLL alongside the same-build `msvcp_win.dll`.
- With the CI-produced pair staged, the MW2 `C0000139` crash did not reproduce across a 13.3M-line run (previously hit within 3 minutes on both attempts); the analyzer smoke test (`analyzer.exe -s test-sample.exe`, 28/28) and `d3d9-triangle-test` (pixel-exact) were unaffected by the refreshed `msvcp_win.dll`.
- In the currently staged root (`10.0.26100.32995` `displaycolormanagement.dll` next to `10.0.26100.32860` `msvcp_win.dll`, same build line), all 19 of the DLL's `msvcp_win.dll` imports resolve against the staged export table (checked with `pefile`).
- `create-root.bat` remains all-CRLF (214/214 lines) with the new line matching its neighbours.

No compiled code changes; CMake does not consume this file.

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Branch was already a single commit before the squash (pre_squash_commit_count=1: 2164da0d). Performed the reset --soft + recommit anyway per instructions; the diff is byte-identical, only the hash (now d783a79a) and message changed.
- Commit message factual correction: the previous message said coloradapterclient.dll 'loads windows.internal.graphics.display.displaycolormanagement.dll by name at runtime'. pefile shows no static or delay import of it; the DLL is reached via RoActivateInstance of the runtime class Windows.Internal.Graphics.Display.DisplayColorManagement.DisplayColorManagement (string present in coloradapterclient.dll x64 and x86), resolved by combase through the SOFTWARE hive's ActivatableClassId DllPath. Reworded the commit message and PR body accordingly.
- Memory-note claim verified: current diff adds ONLY displaycolormanagement.dll (1 insertion); msvcp140_atomic_wait.dll is not in the diff. The old remote tip 12481a0a had both; it was overwritten by the force-with-lease push.
- The STATUS_ENTRYPOINT_NOT_FOUND repro and the CI/MW2 verification numbers in the PR body come from the historical record in this repo (commit c99bcd68 'docs: record the DisplayColorManagement/msvcp_win version-mismatch fix', HANDOFF_MACBOOK.md section 62, 2026-07-07), not from a re-run today; I could not re-execute the .bat or MW2 from this macOS worktree. The import-resolution check (19/19 msvcp_win imports resolve) and the ActivatableClassId registry lookup were done fresh today against build/release/artifacts/root.
- No compiled-code build or windows-emulator-test run was performed post-squash (see build_test_results for why).

</details>

---

## `fix/wow64-module-base-below-4gb`

Commit: `bc6fe2a7c23679a56b00badbe8d6dba0b647b67b`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:fix/wow64-module-base-below-4gb

**Commit message:**
```
fix(module): bound the wow64cpu.dll base search below 4GB

must_map_module_below_4gb forces wow64cpu.dll under 4GB when its
preferred base is above it, but the pick used the two-argument
find_free_allocation_base, where the start address is only a hint: once
the low arena is occupied the search walks past 4GB and hands back a
base wow64 pointer marshaling then truncates. Route the pick through
find_free_host_allocation_base with the explicit sub-4GB ceiling that
the relocation fallback in the same function already uses. On
exhaustion it returns 0, which the existing !binary.image_base path
already handles by falling through to that ceilinged fallback.

Add a memory_manager test pinning the ceiling contract the pick relies
on.
```

**PR body (paste into GitHub):**

```markdown
## Summary

`map_module_from_data` forces `wow64cpu.dll` below 4GB when its preferred `ImageBase` is above 4GB (`must_map_module_below_4gb`), but the pick it makes for that case is not actually bounded. It calls the two-argument `memory_manager::find_free_allocation_base(size, DEFAULT_ALLOCATION_ADDRESS_32BIT)`, whose `start` is only a search hint and whose ceiling is `MAX_ALLOCATION_END_EXCL - 1`. Once the low arena is occupied the search walks straight past 4GB and returns a base that WOW64 pointer marshaling later truncates to 32 bits, aliasing the module onto whatever sits at the truncated address.

The relocation fallback a few lines further down in the same function already does this correctly: it calls `find_free_host_allocation_base(size, start, highest_address)` with an explicit `0xFFFFFFFF` ceiling for 32-bit and forced-wow64cpu modules. This change routes the forced pre-search through that same ceilinged helper and hoists the `below_4gb_ceiling` constant so both call sites share it.

On exhaustion the ceilinged helper returns 0, which the existing `if (!binary.image_base || !try_map_module_at_current_base(...))` branch already handles by falling into the ceilinged relocation loop, so no new failure path is introduced. Net production change is three lines in `src/windows-emulator/module/module_mapping.cpp`; it is backend-agnostic (pure `memory_manager` bookkeeping, no backend hooks involved).

## Trigger conditions

Both must hold, so most roots never hit it:

1. The root's `wow64cpu.dll` has a preferred `ImageBase` above 4GB (the `must_map_module_below_4gb` gate; a `0x1800...`-style high-entropy base from a newer Windows build).
2. Enough of the sub-4GB arena is already reserved that the first free slot at or above `0x10000` is above 4GB.

## Test

Added `HostAllocationTest.FindFreeHostBaseWithBelow4GbCeilingReturnsZeroWhenLowArenaIsFull` in `src/windows-emulator-test/host_allocation_test.cpp`. It reserves `[MIN_ALLOCATION_ADDRESS, 4GB)` in a `memory_manager` over the existing `fake_host_memory`, then asks for a `0x9000`-byte base (the real `SizeOfImage` of the `wow64cpu.dll` in my root) starting at `DEFAULT_ALLOCATION_ADDRESS_32BIT`:

- the old call, `find_free_allocation_base(size, DEFAULT_ALLOCATION_ADDRESS_32BIT)`, returns exactly `0x100000000` -- the first address past the ceiling, i.e. the bug;
- the new call, `find_free_host_allocation_base(size, DEFAULT_ALLOCATION_ADDRESS_32BIT, 0xFFFFFFFF)`, returns `0`.

The test pins both so the contract the pre-search now relies on cannot regress silently.

## Verification (macOS arm64, Apple Clang, `--preset=release`, default backend)

- `windows-emulator-test`: 51/51 pass with `EMULATOR_ROOT` set (14/14 in `HostAllocationTest.*`).
- `analyzer -s -e root c:\test-sample.exe` (x64): all tests report `Success`.
- `analyzer -s -e root c:\wow64-test-sample.exe` (x86 / WOW64): `wow64-test-sample: ok` (module churn 5x50 cycles, VirtualAlloc/VirtualFree 200 cycles, file roundtrip all ok).

To be explicit about coverage: the `wow64cpu.dll` in my root has a preferred `ImageBase` of `0x6b100000`, so `must_map_module_below_4gb` returns false for it and the WOW64 smoke run exercises the unchanged preferred-base path -- it is regression evidence that the hoisted constant and the shared helper did not disturb the normal case, not a live reproduction of the forced-pick bug. The forced-pick behaviour itself is covered by the new unit test at the `memory_manager` level, where the wrong answer (`0x100000000`) is observed directly.

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Memory note discrepancy: it described the branch as '~6 lines in module_mapping.cpp' only, but the diff also adds a 16-line gtest (HostAllocationTest.FindFreeHostBaseWithBelow4GbCeilingReturnsZeroWhenLowArenaIsFull) in src/windows-emulator-test/host_allocation_test.cpp. Verified the test exists in the diff and passes; PR body describes both files.
- Branch was already a single commit (8ea36dba) before this pass; the reset --soft + recommit was still performed per instructions, producing bc6fe2a7 with byte-identical content (pre/post diff empty). The existing commit message was retained since every claim in it was verified against memory_manager.cpp/module_mapping.cpp.
- The worktree has no root/ directory, so windows-emulator-test needs EMULATOR_ROOT pointed at the main checkout's build/release/artifacts/root to pass its 9 emulation tests; without it they fail at 0ms with 'No EMULATOR_ROOT set!'. Environmental, not related to this change.
- The forced-below-4GB pre-search this change fixes is not exercised by any binary in the available root: its wow64cpu.dll has ImageBase 0x6b100000 (< 4GB), so must_map_module_below_4gb is false. Live smoke runs are regression coverage only; the bug itself is demonstrated by the new unit test at the memory_manager level. The PR body states this explicitly rather than claiming an end-to-end repro.

</details>

---

## `fix/raise-exception-exit-status`

Commit: `14f528eea88daef3105f461611428acff2f6b3e2`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:fix/raise-exception-exit-status

**Commit message:**
```
fix(exception): propagate ExceptionCode into process exit status

A second-chance NtRaiseException stopped the emulator without recording
the exception code, so the process finished with an empty exit_status
and the analyzer reported "Emulation terminated without status".
Store the record's ExceptionCode the way NtRaiseHardError and the
__fastfail path already do.

test-sample gains a -fail-fast mode that raises a caller-supplied
EXCEPTION_RECORD through RaiseFailFastException, and a new
windows-emulator-test asserts that its code becomes the exit status.
```

**PR body (paste into GitHub):**

```markdown
## Problem

When a guest raises a second-chance exception, `handle_NtRaiseException` (`HandleException == FALSE`) calls `on_exception()` and `emu.stop()` but never touches `process.exit_status`. The analyzer then lands in its `!exit_status.has_value()` branch and reports the generic

```
Emulation failed at: 0x1800a3194 - Emulation terminated without status
```

with no indication of *which* exception ended the run. The two sibling termination paths already record a code: `NtRaiseHardError` stores its `error_status`, and the `int 0x29` `__fastfail` path in `windows_emulator.cpp` stores `STATUS_FAIL_FAST_EXCEPTION`. `NtRaiseException` was the only one that dropped it.

## Change

- `src/windows-emulator/syscalls/exception.cpp`: read the guest `EXCEPTION_RECORD` and store its `ExceptionCode` in `c.proc.exit_status` before `on_exception()` / `stop()`. The previously unused `exception_record` parameter is now named and used. Nothing else in the handler changes.
- `src/samples/test-sample/test.cpp`: new `-fail-fast` argument that fills an `EXCEPTION_RECORD` with code `0xE0001234` and calls `RaiseFailFastException(&record, nullptr, 0)`. kernelbase forwards that to `NtRaiseException` with `HandleException = FALSE`, i.e. exactly the path being fixed. The code is deliberately not `STATUS_FAIL_FAST_EXCEPTION` (`0xC0000409`) so the test proves the record's code is propagated rather than the unrelated `int 0x29` path being hit.
- `src/windows-emulator-test/exception_test.cpp` (new) + `emulation_test_utils.hpp`: `ExceptionTest.SecondChanceExceptionCodeBecomesExitStatus` runs the sample with `sample_configuration{.fail_fast = true}` and asserts `ASSERT_TERMINATED_WITH_STATUS(emu, 0xE0001234)`.

Note on scope: the emulator stopping on a second-chance exception is pre-existing behaviour and is untouched here (the `stop()` call is unchanged, on every backend). This PR only makes the reason for that stop visible through `exit_status`, so a run that previously ended as "terminated without status" now ends as "terminated with status: <ExceptionCode>", which is a non-success status as it should be.

## Reproduction and verification

Host: macOS arm64, Apple Clang, `--preset=release`, default backend (unicorn, no `EMULATOR_*` env overrides). Guest: 64-bit `test-sample.exe` cross-built from this branch's `test.cpp` with mingw-w64 (GCC 16.1), run in an emulation root containing that binary as `C:\test-sample.exe`.

Before (identical tree with `origin/main`'s `exception.cpp` swapped back in):

```
$ analyzer -e <root> --very-concise 'C:\test-sample.exe' -fail-fast
Executing function: RaiseFailFastException (kernelbase.dll) (0x104177500) via 0x1400e0b60 (test-sample.exe)
Emulation failed at: 0x1800a3194 - Emulation terminated without status
```

```
[ RUN      ] ExceptionTest.SecondChanceExceptionCodeBecomesExitStatus
exception_test.cpp:12: Failure
Value of: (emu).process.exit_status.has_value()
  Actual: false
Expected: true
[  FAILED  ] ExceptionTest.SecondChanceExceptionCodeBecomesExitStatus (697 ms)
```

After (this branch):

```
$ analyzer -e <root> --very-concise 'C:\test-sample.exe' -fail-fast
Executing function: RaiseFailFastException (kernelbase.dll) (0x104177500) via 0x1400e0b60 (test-sample.exe)
Emulation terminated with status: E0001234
```

```
[ RUN      ] ExceptionTest.SecondChanceExceptionCodeBecomesExitStatus
[       OK ] ExceptionTest.SecondChanceExceptionCodeBecomesExitStatus (610 ms)
```

Full `windows-emulator-test` against the rebuilt sample: 51/51 passed (20.4 s). Against the previous prebuilt `test-sample.exe` (no `-fail-fast` support) the other 50 tests still pass; only the new test fails, with `exit_status == 0` vs `0xE0001234`, because a stale sample ignores the argument and exits normally. So the new test needs `test-sample.exe` rebuilt from this branch, which the sample build in CI does.

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Branch was already a single commit (pre_squash_commit_count=1); the squash was a reset --soft + recommit with the same message, content verified byte-identical.
- Remote fork/fix/raise-exception-exit-status previously pointed at abb79958, an older version of this branch based on the integration branch (diff vs HEAD spanned ~130 files); it diverged from the origin/main-based HEAD, so --force-with-lease was used. Now at 14f528ee.
- The new ExceptionTest requires test-sample.exe rebuilt from this branch. The user's shared emulation root (/Users/jack/Documents/Coding/C++/sogen/build/release/artifacts/root/filesys/c/test-sample.exe, md5 b5ae102f...) is a stale prebuilt without -fail-fast, so the test fails there (exit_status 0 vs 0xE0001234) until that binary is replaced. I did NOT replace it; verification used an isolated scratch root with a mingw-w64 cross-build. Upstream CI builds samples from source on Windows runners so this should not affect CI.
- Memory-note claims checked against the diff: exception_test.cpp does exist; the -fail-fast sample repro is present; no extra file changes beyond the 4 listed; 'reveals pre-existing stop behaviour' is accurate (emu.stop() call unchanged, only exit_status added). No discrepancies found on this branch.

</details>

---

## `fix/apiset-zero-value-entries`

Commit: `9d69c8764eaaae279c64ad7c8fd4106e5e3a159e`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:fix/apiset-zero-value-entries

**Commit message:**
```
fix(apiset): write zero-value namespace entries into the cloned map

apiset::clone skipped the rest of the loop body for a namespace entry
with ValueCount == 0, so neither its API_SET_NAMESPACE_ENTRY slot nor
its API_SET_HASH_ENTRY slot was ever written into the guest copy. The
zeroed hash slot breaks the sort order ntdll's binary search relies on,
so unrelated api sets stop resolving and the loader raises
STATUS_DLL_NOT_FOUND.

Write both slots for every entry and clear ValueOffset for entries
without values, since the source offset is meaningless in the
relocated copy. Cover the path with a synthetic map, as no shipped
api-set.bin contains such an entry.
```

**PR body (paste into GitHub):**

```markdown
## Summary

`apiset::clone` (`src/windows-emulator/apiset/apiset.cpp`) `continue`d out of the per-entry loop as soon as it saw a namespace entry with `ValueCount == 0`. That skipped not only the value copying but also the two writes at the end of the loop body, so the guest copy of the map was left with a zero-filled `API_SET_NAMESPACE_ENTRY` slot *and* a zero-filled `API_SET_HASH_ENTRY` slot for that index.

The hash-table slot is the damaging one: ntdll resolves api sets by binary-searching the hash table by `Hash`, so a stray `{Hash = 0, Index = 0}` in the middle of the table breaks the sort invariant and makes the search miss entries that have nothing to do with the empty one. The visible symptom is the loader failing to resolve an unrelated `api-ms-win-*` DLL with `STATUS_DLL_NOT_FOUND`.

## Change

- Restructure the loop so the value copying is conditional on `ValueCount != 0`, while the namespace-entry and hash-entry writes run for every entry.
- For entries without values, set `ValueOffset = 0` instead of carrying the source map's offset into the relocated copy, where it points at nothing meaningful.
- Add `ApiSetTest.CloneWritesEntriesWithoutValues` (`src/windows-emulator-test/apiset_test.cpp`), which builds a synthetic v6 map with three entries (the middle one has no values), clones it into a fresh `memory_manager` region, and checks the middle entry's name/`ValueCount`/`ValueOffset`, its hash slot (`Hash = 0x2000`, `Index = 1`), and that `apiset::get_namespace_table` on the cloned bytes yields the expected two resolvable entries.

The test uses a synthetic map on purpose: I parsed the zstd-compressed `api-set.bin` in the staged root (version 6, 936 entries) and it contains zero entries with `ValueCount == 0` and a correctly sorted hash table, so the shipped map cannot exercise this path. The bug only bites with a map that has such entries (real Windows api-set maps can carry them for retired/unhosted contracts).

## Verification

Host: macOS arm64, `--preset=release` (RelWithDebInfo), Unicorn backend (the test helper's default with no `EMULATOR_*` overrides), `EMULATOR_ROOT` pointing at the staged root; the test runs on the x86-64 emulator and exercises the clone path shared by the 64- and 32-bit loaders.

- **Red on `origin/main`**: swapped in `origin/main`'s `apiset.cpp`, rebuilt `windows-emulator-test`, ran `--gtest_filter=ApiSetTest.*`: 1 test **FAILED** with 3 assertion failures, at `apiset_test.cpp:100` (cloned name reads back as `u""`), `:103` (hash `0` vs expected `8192`/`0x2000`) and `:104` (index `0` vs expected `1`). This is exactly the unwritten namespace-entry slot plus unwritten hash slot.
- **Green on this branch**: same test **passes**.
- **Full suite on this branch**: `gtest_parallel.py ./windows-emulator-test` -> 51/51 passed, exit code 0.

## Notes

- Two files touched: `src/windows-emulator/apiset/apiset.cpp` and the new `src/windows-emulator-test/apiset_test.cpp` (picked up by the existing `GLOB_RECURSE` in the test `CMakeLists.txt`).
- No behavioural change for maps without zero-value entries: the value-copy branch is byte-for-byte the previous logic, only re-indented under the new condition.

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Branch was already a single commit (f42ff21a) ahead of origin/main; the reset --soft + recommit was performed anyway per instructions and produced 9d69c876 with a byte-identical diff (pre/post diff files compared empty).
- The memory note's live end-to-end A/B (real staged api-set.bin with an injected zero-value entry producing NtRaiseHardError C0000135 on x64 and x86 on main) is NOT part of the diff and was NOT reproduced in this pass; the PR body deliberately does not claim it. Only the unit-test red/green and full-suite results are stated as verified.
- Pre-existing remote fork/fix/apiset-zero-value-entries pointed at 4c1ed140 ('fix(apiset): zero ValueOffset for zero-value entries instead of commenting it'), which diverged from HEAD; it was overwritten with --force-with-lease=refs/heads/fix/apiset-zero-value-entries:4c1ed140 (forced update 4c1ed140...9d69c876).

</details>

---

## `fix/bcrypt-provider-parsing`

Commit: `f6dc9321f83c80aa7ac412f1a25b17afac7f3048`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:fix/bcrypt-provider-parsing

**Commit message:**
```
fix(ksecdd): parse the algorithm name from short BCrypt provider requests

bcrypt sizes the KsecDD 0x390400 request to the algorithm name it
carries: 0x40 bytes for "SHA256" and "SHA1", 0x38 for "MD5", "MD4" and
"MD2". Gating the algorithm_name read on the full 0x40-byte struct left
every three-character name unparsed, so the lookup fell through to the
RNG provider and BCryptOpenAlgorithmProvider returned
STATUS_NOT_SUPPORTED for the whole MD family.

The bounded read_memory<T>(address, size) already zero-fills whatever
the guest did not send, so the guard never kept the read in bounds.

Add a "BCrypt Hash" test-sample case that hashes "abc" through MD5
(short request) and SHA256 (full request) and checks the digests, so
both paths through the device stay covered.
```

**PR body (paste into GitHub):**

```markdown
## Problem

`BCryptOpenAlgorithmProvider` fails with `STATUS_NOT_SUPPORTED` (`0xC00000BB`) for every algorithm whose name is three characters long (`MD5`, `MD4`, `MD2`), while `SHA1`/`SHA256` work.

bcrypt.dll sizes the KsecDD `0x390400` provider-lookup request to the name it carries. Measured on the emulated x64 bcrypt.dll by temporarily logging `c.input_buffer_length` in `security_support_provider.cpp`:

| Algorithm | request length |
|---|---|
| `MD5`, `MD4` | `0x38` |
| `SHA1`, `SHA256` | `0x40` |

`ksec_algorithm_request` is `0x40` bytes, and the handler only parsed `algorithm_name` when `input_buffer_length >= sizeof(ksec_algorithm_request)`. Every `0x38`-byte request therefore ended up with an empty name, missed the hash-algorithm table, and fell through to the RNG-provider response, which bcrypt then rejected.

## Fix

`src/windows-emulator/devices/security_support_provider.cpp`: drop the full-struct-size gate and always parse `algorithm_name`. The read already goes through `read_memory<T>(address, size)`, which copies `min(size, sizeof(T))` bytes into a zero-initialised struct, so a short request never reads out of bounds; the guard was only discarding data that had already been read safely. The existing `ksec_algorithm_request_min_size` (`0x30`, the offset of the name) check still rejects requests too short to contain any name at all.

## Test

`src/samples/test-sample/test.cpp` + `CMakeLists.txt`: new `BCrypt Hash` test-sample case (links `bcrypt`) that opens `MD5` (short request) and `SHA256` (full request), hashes `"abc"` via `BCryptHash`, and compares against the known digests, so both request shapes stay covered.

## Verification

All runs: macOS host, x64 `analyzer` from `cmake --build --preset=release`. "main-equivalent" below is this branch's tree with only `security_support_provider.cpp` reverted to `origin/main` (`e8824779`), rebuilt.

**test-sample.exe (x64, built from this branch with mingw-w64), `--backend unicorn`:**
- main-equivalent: 29/30, `Running test 'BCrypt Hash': BCryptOpenAlgorithmProvider(MD5) failed: 0xC00000BB` -> `Fail`, exit 1
- this branch: 30/30, exit 0
- this branch, `--backend fex`: 30/30, exit 0

**Standalone probe (`BCryptOpenAlgorithmProvider` + `BCryptHash("abc")` for MD5/MD4/SHA1/SHA256), before -> after:**
- x64 unicorn: MD5/MD4 `0xC00000BB` -> `0x00000000` with digests `900150983cd24fb0d6963f7d28e17f72` / `a448017aaf21d8525fc10ae87aa6729d`; SHA1/SHA256 unchanged (`0x00000000`, correct digests) in both builds
- x86 (WoW64) unicorn: same before/after result as x64
- x64 fex: same before/after result as x64
- x86 fex: not runnable, backend reports "FEX backend does not support WoW64 (32-bit) processes yet" (pre-existing, unrelated)

**Regression:** `EMULATOR_ROOT=<root> ./windows-emulator-test` on this branch: 50/50 passed. `clang-format --dry-run --Werror` clean on the changed files.

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Branch was already a single commit (f6dc9321) with a conventional-style message, and fork/fix/bcrypt-provider-parsing already pointed at exactly that hash. I kept the existing commit instead of reset --soft + re-commit, since rewriting would only change the hash for identical content and force a needless history rewrite; the plain push reported 'Everything up-to-date'. pre/post diffs are byte-identical (same HEAD).
- Confirmed conflict with feat/gdi-user-syscalls (fork, 32ddfac8): git merge-tree of this branch against it reports CONFLICT (content) in src/samples/test-sample/test.cpp (both add a test function at the same insertion point and a RUN_TEST registration). Only test.cpp conflicts; security_support_provider.cpp vs gdi.cpp do not overlap. Whichever PR merges second needs a rebase.
- The 32-bit test-sample.exe cannot be cross-compiled with i686-w64-mingw32-g++ (pre-existing: SEH __try/__except and a lambda-to-LPTHREAD_START_ROUTINE conversion fail to build), so x86 coverage was done with a standalone bcrypt probe rather than the full test sample. FEX has no WoW64 support, so x86 was only run on unicorn.
- windows-emulator-test runs the pre-existing C:\test-sample.exe in the shared emulation root (built from an older tree, without the new BCrypt Hash case); the new case was exercised via a separately built test-sample.exe as described. No changes to the shared root were left behind.

</details>

---

## `feat/registry-transaction-stubs`

Commit: `04ec5cd47c600f3d4fecee422c22f12b0432c1c2`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:feat/registry-transaction-stubs

**Commit message:**
```
feat(registry): stub NtCreateRegistryTransaction and NtOpenRegistryTransaction

The registry is a plain hive with no Kernel Transaction Manager support,
so both syscalls now return STATUS_NOT_SUPPORTED, matching a system
without transacted-registry support, instead of hitting the
unimplemented-syscall abort. The output handle is left untouched on
failure, as NT does.

Add a "Registry Transaction" section to test-sample that resolves both
ntdll exports and calls them, so the dispatcher's coverage of these two
syscalls is exercised by windows-emulator-test.
```

**PR body (paste into GitHub):**

```markdown
## Summary

Register `NtCreateRegistryTransaction` and `NtOpenRegistryTransaction` (both exported by the Windows 10+ `ntdll.dll` in the emulation root, syscall ids `0xC0` / `0x12F`) as handlers that return `STATUS_NOT_SUPPORTED` (`0xC00000BB`). Sogen's registry is a plain hive with no Kernel Transaction Manager (TxR) support, and `STATUS_NOT_SUPPORTED` is what a Windows system without transacted-registry support returns, so callers take their ordinary non-transacted fallback path. The output handle is left untouched on the failure path, as NT does.

Before this change, the first call aborts the whole emulation through the unimplemented-syscall path.

## Reproduction

Any x64 guest that calls either export. The new `test-sample` section is the minimal one: resolve both from `ntdll.dll` via `GetProcAddress`, call `NtCreateRegistryTransaction(&h, TRANSACTION_ALL_ACCESS, &oa, 0)` and `NtOpenRegistryTransaction(&h, TRANSACTION_ALL_ACCESS, &oa)`.

Without the handlers registered (the two `add_handler` lines removed, everything else identical), `test-sample.exe` (x64, cross-built with the repo's `cmake/toolchain/mingw-w64.cmake`) under `--backend unicorn`:

```
Executing syscall: NtCreateRegistryTransaction (0xC0) at 0x1800a1bf2 via 0x1400e162d (test-sample.exe)
Unimplemented syscall: NtCreateRegistryTransaction - 0xC0 (raw: 0xC0)
Emulation failed at: 0x1800a1bf4 - Emulation terminated without status
```

The analyzer exits with status 1 and guest stdout stops at `Running test 'Registry Transaction': ` with no result; every test after it never runs.

## Verification

Same `test-sample.exe`, this branch:

- `--backend unicorn`: both syscalls dispatched (`NtCreateRegistryTransaction (0xC0)`, `NtOpenRegistryTransaction (0x12F)`), `Emulation terminated with status: 0`, guest stdout shows `Running test 'Registry Transaction': Success`, 30 `Success` / 0 `Fail` across the whole sample.
- `--backend fex`: identical result (both syscalls dispatched, status 0, 30/0).
- `windows-emulator-test` with `EMULATOR_ROOT` pointing at a root containing the rebuilt sample: `50 tests from 11 test suites ran ... [  PASSED  ] 50 tests.` The `EmulationTest.*` cases run the sample end to end, so the new section is exercised on every CI run that rebuilds `test-sample.exe`.
- Host: macOS arm64, `cmake --build --preset=release`.

Note the sample check accepts either `STATUS_NOT_SUPPORTED` or success from the create call (so a future real TxR implementation does not have to touch the test), and only requires the open call to fail, since no transaction object exists to open.

## Files

- `src/windows-emulator/syscalls/registry.cpp` — the two handlers
- `src/windows-emulator/syscalls.cpp` — declarations + `add_handler` registrations
- `src/samples/test-sample/test.cpp` — `test_registry_transaction` + `RUN_TEST` entry, `<winternl.h>` include

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Memory-note check: the prior commit message and memory claims all held up against the diff (3 files, stubs + test-sample section, no extra files). One nuance worth knowing for the PR: the pre-built test-sample.exe in the shared emulation root (build/release/artifacts/root/filesys/c/test-sample.exe, 213K) predates this branch and does not contain the 'Registry Transaction' test, so running windows-emulator-test against that root does NOT exercise the new section; I verified with a freshly mingw-cross-compiled sample instead (CI rebuilds it, so upstream CI will cover it).
- Remote fork/feat/registry-transaction-stubs previously pointed at c9ad36cf (the pre-squash commit) and diverged from the new 04ec5cd4, so --force-with-lease (lease pinned to c9ad36cf) was used as expected; plain push was not possible.
- The tidy preset was not run in this pass (content is unchanged from the already-reviewed branch; only the release preset + tests were re-run).

</details>

---

## `fix/ordinal-only-exports`

Commit: `7cfe2af6bd6af7ad3704a2272d87b3edeeae561f`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:fix/ordinal-only-exports

**Commit message:**
```
fix(module): collect ordinal-only exports in both collect_exports overloads

Both export walkers iterated AddressOfNames and looked up the function
slot through AddressOfNameOrdinals, so an export with no name-table entry
never produced a symbol. Modules that export by ordinal only (profapi.dll
has NumberOfNames == 0) ended up with an empty export list, and mixed
modules silently lost their unnamed ordinals.

Walk AddressOfFunctions instead, skip unused zero slots, and attach the
name through a reverse ordinal-to-name-index map. Exports without a name
are named "#<ordinal>" so find_export and address_names still resolve
them.
```

**PR body (paste into GitHub):**

```markdown
## Problem

Both `collect_exports` overloads in `src/windows-emulator/module/module_mapping.cpp` (the file-buffer walker and the `map_module_from_memory` walker) iterate `AddressOfNames` for `NumberOfNames` entries and reach the function slot through `AddressOfNameOrdinals`. The PE export directory does not require every function slot to have a name-table entry, so:

- A module whose exports are ordinal-only (`NumberOfNames == 0`) produces an **empty** `exports` vector and an empty `address_names` map.
- A mixed module silently drops every export that has no name entry.

Because `mapped_module::address_names` is what `windows-analyzer/analysis.cpp` uses to emit `function_execution_event` / `foreign_code_transition_event`, calls into an ordinal-only DLL are never logged at all, and calls into an unnamed ordinal of a mixed DLL are attributed via `upper_bound` to the nearest *named* export with a wrong offset. `find_export` cannot resolve them either, and the export count reported by `minidump_loader.cpp` is wrong.

## Fix

Walk `AddressOfFunctions` for `NumberOfFunctions` slots, skip zero (unused) slots, and attach the name through a reverse ordinal -> name-index map built from `AddressOfNameOrdinals`. Exports with no name entry get the conventional `#<ordinal>` name so `find_export("#N")` and `address_names` resolve them. Both overloads get the same change so file-mapped and memory-mapped modules produce identical export lists.

## Reproduction

`C:\Windows\System32\profapi.dll` (x64, from the emulation root) has `NumberOfNames == 0` and 17 non-zero slots in `AddressOfFunctions`. The new test `src/windows-emulator-test/module_exports_test.cpp` maps it through `module_manager::map_module` and compares `mod.exports.size()` against an independent walk of the export directory read back from guest memory.

Against `origin/main`'s `module_mapping.cpp` (swapped in, rebuilt, run, then restored):

```
module_exports_test.cpp:55: Failure
Expected equality of these values:
  mod.exports.size()
    Which is: 0
  count_used_export_slots(emu.memory, mod, directory)
    Which is: 17
[  FAILED  ] ModuleExportsTest.OrdinalOnlyExportsAreCollected
[  FAILED  ] ModuleExportsTest.MemoryMappedModuleCollectsSameExportsAsFileMappedModule
```

With this branch:

```
[       OK ] ModuleExportsTest.OrdinalOnlyExportsAreCollected (7 ms)
[       OK ] ModuleExportsTest.MemoryMappedModuleCollectsSameExportsAsFileMappedModule (3 ms)
```

## How widespread this is

Independent survey (a standalone PE parser, not sogen) of the DLLs in the emulation root that carry an export directory:

- `system32` (x64): 158 DLLs. 2 are ordinal-only and previously mapped with **zero** exports (`profapi.dll` 17, `ntasn1.dll` 60). 37 mixed DLLs previously dropped **2,268** unnamed exports in total, e.g. `dxcore.dll` names=1 used=224 (+223), `combase.dll` 461/598 (+137), `comctl32.dll` 119/181 (+62), `dwmapi.dll` 44/101 (+57), `dcomp.dll` 12/32 (+20), `d3d9.dll` 17/23 (+6).
- `syswow64` (x86): 2,339 DLLs. 38 ordinal-only (including every MFC runtime, `mfc140u.dll` alone has 14,482 exports, all unnamed; `edgeIso.dll` 232, `shunimpl.dll` 453). 134 mixed DLLs previously dropped **27,725** unnamed exports.

## Tests

- New `ModuleExportsTest.OrdinalOnlyExportsAreCollected`: asserts `NumberOfNames == 0`, `exports.size()` equals the number of non-zero function slots, every symbol is named `#<ordinal>`, ordinal is within `[Base, Base + NumberOfFunctions)`, `address == image_base + rva`, `find_export(name)` resolves, and `address_names` contains the address.
- New `ModuleExportsTest.MemoryMappedModuleCollectsSameExportsAsFileMappedModule`: `map_module_from_memory<uint64_t>` over the already-mapped image yields the same `exports` (name/ordinal/rva/address, in order) and an equal `address_names` map as the file-mapped module.
- Full `windows-emulator-test` suite via `gtest_parallel.py` with `EMULATOR_ROOT=root`: 52/52 pass, exit code 0 (macOS arm64, `--preset=release`).

## Notes

- No behavioural change for exports that already had a name: named symbols still get their string name; only previously-missing slots are added. `exports.reserve` now uses `NumberOfFunctions` instead of `NumberOfNames`.
- The `#<ordinal>` naming matches the existing `u"#"` convention already used for ordinal-based class/resource lookups elsewhere in the emulator (`process_context.cpp`, `syscalls/user.cpp`).

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Squash was a no-op: the branch was already exactly ONE commit (7cfe2af6) on top of origin/main (e8824779, which is also the merge-base). I deliberately did NOT run `git reset --soft` + re-commit, because that would have produced a new hash for byte-identical content and forced a force-push for no benefit. The existing commit message already matches the repo's conventional style (`fix(module): ...` with a body and the Claude-Session trailer), so it is reported as-is in commit_message.
- Push was a no-op: `git ls-remote fork fix/ordinal-only-exports` already pointed at 7cfe2af6 (== HEAD) before I started; `git push fork HEAD:refs/heads/fix/ordinal-only-exports` reported 'Everything up-to-date'. No --force-with-lease was needed or used.
- Memory-note discrepancy (informational): the note described the branch as only 'module_mapping.cpp, ~61 lines'. The module_mapping.cpp figure is accurate (+52/-9), but the branch ALSO adds a new 91-line test file src/windows-emulator-test/module_exports_test.cpp, which the note did not mention. Verified it is real, picked up by the GLOB_RECURSE in src/windows-emulator-test/CMakeLists.txt, fails on origin/main and passes on the branch. The PR body is grounded in the actual diff.
- Minor style note, NOT changed (content had to stay identical to what was reviewed): the diff adds two comments in module_mapping.cpp — a two-line note above the AddressOfFunctions loop explaining that AddressOfNames only covers part of the function table (arguably qualifies as a non-obvious PE constraint under the repo's comment policy), and `// Unused ordinal slot.` on the `continue` in both overloads (borderline restatement). The maintainer may ask for the latter to be dropped.

</details>

---

## `fix/handle-4-byte-alignment`

Commit: `14760c7ce6a0be7de2f6d8ec3a3f5cdb74b9e6fd`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:fix/handle-4-byte-alignment

**Commit message:**
```
fix(handles): make handle values 4-aligned like real Windows

NT handles are multiples of 4: the kernel ignores the low 2 bits and
user-mode code is free to tag them (rpcrt4 keeps private flags there).
With the id starting at bit 0, a tagged handle aliased a neighbouring
live handle, so NtClose on it closed the wrong object.

Reserve the low 2 bits in handle_value so every store hands out
4-aligned values and lookups ignore the tag bits. That makes the
per-store IndexShift redundant, so drop it and the IndexShift=2 uses on
sections and registry_keys.

user32 indexes the shared aheList by the HANDLE's low 16 bits, not by
the internal id, so route every USER_HANDLEENTRY access (allocate,
set_owner, free, NtUserMapDesktopObject) through the id-to-slot shift
and bound the allocator by the number of ids that still have a slot
inside the table.

Add a test-sample case that opens three files and calls ntdll!NtClose
on the middle one with each low-2-bit pattern, asserting only that
handle is closed.
```

**PR body (paste into GitHub):**

```markdown
## Problem

Sogen hands out kernel handle values with the object id starting at bit 0, so consecutive handles are `0x800002, 0x800003, 0x800004, ...`. Real NT handles are always multiples of 4: the kernel ignores the low two bits, and user-mode code is allowed to (and does) store private tag bits there.

The consequence is not cosmetic. When guest code tags a handle and later passes it to `NtClose`, the emulator resolves the *tagged* value as a different id and closes whichever neighbouring handle happens to live there. Concretely, on current `main` (x64 guest, unicorn backend), with `before=0x800001`, `target=0x800002`, `after=0x800003` all open:

```
NtClose(0x800003)   // target | 3
  -> STATUS_SUCCESS, `target` still open, `after` closed
NtClose(0x800001)   // target | 1
  -> STATUS_SUCCESS, none of the three probe handles closed (some other live handle was)
NtClose(0x800000)   // target & ~3
  -> STATUS_INVALID_HANDLE (0xC0000008)
```

The same happens for a 32-bit guest (`NtClose(0x800005)` with tag 1 closes `after` instead of `target`; tags 2 and 3 fail with `STATUS_INVALID_HANDLE`).

## Fix

- `handles.hpp`: `handle_value` reserves the low 2 bits (`reserved : 2`, `id : 21` instead of `id : 23`). Every `handle_store` therefore hands out 4-aligned values and lookups ignore the tag bits. The per-store `IndexShift` template parameter becomes redundant and is removed.
- `process_context.hpp`: drops the now-nonexistent `IndexShift = 2` argument from `sections` and `registry_keys`. Those two stores previously got 4-alignment by shifting their own index; every store now gets it from the layout.
- `user_handle_table.hpp` / `syscalls/user.cpp`: user32 indexes the shared `aheList` by the HANDLE's low 16 bits, not by our internal id. With the id now at bit 2, the slot the guest computes is `id << 2`, so `allocate`, `set_owner`, `free_index` and `NtUserMapDesktopObject` go through `handle_index_to_ahe_slot()`. The allocator is bounded by `MAX_HANDLE_INDICES = MAX_HANDLES >> 2` (the ids that still have a slot inside the 0xFFFF-entry table); `used_indices_` and the bound checks in `user.cpp` use the same constant.
- `test-sample/test.cpp`: new `Handle Tag Bits` case. It opens three files, calls `ntdll!NtClose` on the middle one with each low-2-bit pattern (0..3), and asserts `STATUS_SUCCESS`, the target closed, and both neighbours still readable.

## Regarding the earlier review on #1234

Igoorx's comment on `user_handle_table.hpp:148` (https://github.com/momo5502/sogen/pull/1234#discussion_r3607483695):

> HMValidateHandle doesn't seem to be broken today, so what is this fixing? Maybe it's an outdated issue? If it's just a cosmetic change, I don't think it's worth it for the number of changes this introduces in the patch.

That was a fair objection to the original patch, which was justified by `HMValidateHandle` and had no test. This PR is not that justification. The bug it fixes is the kernel-handle aliasing above: a tagged `NtClose` closing a *neighbouring* handle returns success and silently destroys an unrelated object, which is a real correctness bug rather than a cosmetic one, and it is now covered by a test that fails on `main` and passes here. The `aheList` slot changes in `user_handle_table.hpp` are the minimum required so the user32 side keeps working once handles are 4-aligned (the guest computes the slot from the HANDLE value, so the shift has to be applied on our side too); they are not an independent `HMValidateHandle` fix. The whole change is +80/-17 across 5 files.

## Verification

Host: macOS arm64 (Apple Clang), `--preset=release`. Guest binaries cross-built with mingw-w64 14.0.0 from this branch's `test.cpp` (x64) plus a standalone copy of the new test body for x86 (`test.cpp` itself only builds with MSVC on 32-bit).

`test-sample.exe` (x64), full run, this branch vs. a build with `origin/main`'s `handles.hpp`, `process_context.hpp`, `user.cpp`, `user_handle_table.hpp` swapped in (same test binary):

| backend | main-equivalent | this branch |
|---|---|---|
| unicorn | `Handle Tag Bits`: **Fail**, exit 1 (28 other tests pass) | 30/30 pass, exit 0 |
| FEX | `Handle Tag Bits`: **Fail**, exit 1 | 30/30 pass, exit 0 |
| icicle | `Handle Tag Bits`: **Fail**, exit 1 (27 other pass; Interrupts self-skips) | 29/29 pass, exit 0 |

Standalone probe (same logic as the new test), unicorn:

| guest | main-equivalent | this branch |
|---|---|---|
| x64 | FAIL (handles `0x800002/03/04`; tag 3 closes `after`, tag 1 closes an unrelated handle, tag 0 -> `0xC0000008`) | PASS (handles `0x800008/0c/10`; all 4 tags close exactly `target`) |
| x86 (WoW64) | FAIL (handles `0x800003/04/05`; tag 1 closes `after`, tags 2-3 -> `0xC0000008`) | PASS (handles `0x80000c/10/14`; all 4 tags close exactly `target`) |

`windows-emulator-test` on this branch: 50/50 passed (29s).

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Branch was already a single commit (44d8ccc6) ahead of origin/main, not the 3 commits the memory note described; a prior pass had evidently squashed it. I still ran reset --soft + recommit per the procedure (new hash 14760c7c, same message, diff byte-identical to pre-squash).
- Could not independently verify the memory note's 'CurrentDirectory.Handle caching collision' claim or the in-code 'rpcrt4 keeps private flags there' claim; neither is evidenced in the diff or reproducible here. The PR body is grounded only in the tag-bit NtClose neighbour-closing bug that the test demonstrates, and does not assert the CurrentDirectory/rpcrt4 specifics beyond noting user-mode tagging is permitted.
- x86 verification could not use test.cpp itself: it only compiles with MSVC on 32-bit (SEH __try/__except, __nop, lambda-to-stdcall conversions fail under i686 mingw-gcc, and no clang/lld mingw toolchain is installed). x86 evidence comes from a standalone probe containing the same test body, disclosed as such in the PR body.
- The remote fork branch previously pointed at 54fc1395 (a 2026-07-20 commit on the old PR #1234 split lineage, not an ancestor); it was overwritten with --force-with-lease pinned to that exact hash, as instructed.
- The root's staged test-sample.exe (build/release/artifacts/root/filesys/c) is byte-identical to the Jul 2 MSVC artifact and does NOT contain the new test, so the suite's emulation tests do not exercise 'Handle Tag Bits'; that case was exercised via direct analyzer runs instead. I did not replace the shared root's test-sample.exe.
- Sibling worktrees (wf_3f0afef6-539-5 and -539-8) were running their own builds/analyzers concurrently against separate scratch roots during my runs; timings above may be slightly inflated but correctness results are unaffected.
- Tidy preset (clang-tidy) was not run in this pass; content is unchanged from the already-reviewed branch.

</details>

---

## `wt/macos-vulkan-bringup`

Commit: `f4db26f663d557e1f67eae821528c592fb997f67`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:wt/macos-vulkan-bringup

**Commit message:**
```
fix(vulkan): run against a portability driver (MoltenVK) on macOS hosts

On Apple Silicon the host Vulkan library is MoltenVK, a portability
driver the Khronos loader hides unless the caller opts in, so
vkCreateInstance returned VK_ERROR_INCOMPATIBLE_DRIVER and no physical
device was ever enumerated.

create_instance enables VK_KHR_portability_enumeration and sets
VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR when the loader
advertises that instance extension. create_device appends
VK_KHR_portability_subset when the physical device advertises it, which
Vulkan requires and which the guest never asks for; whether a device
advertises it is checked once and cached per physical device.
```

**PR body (paste into GitHub):**

```markdown
## Summary

`vulkan_host` could not create a Vulkan instance on a macOS host. The host library there is MoltenVK, a *portability* driver that the Khronos loader deliberately hides from enumeration unless the application opts in, so `vkCreateInstance` failed with `VK_ERROR_INCOMPATIBLE_DRIVER` (-9) and the guest never saw a physical device.

Two changes in `src/windows-emulator/devices/vulkan_host.cpp` (+81 lines), plus a doc paragraph in `docs/gpu-paravirtualization.md`:

- `create_instance`: if the loader advertises `VK_KHR_portability_enumeration`, enable it and set `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`. The extension is checked via `vkEnumerateInstanceExtensionProperties`, so on loaders that don't offer it (older Linux loaders, SwiftShader setups) nothing changes. On loaders that do offer it, enabling it only un-hides non-conformant devices; it never alters a device's reported capabilities.
- `create_device`: if the physical device advertises `VK_KHR_portability_subset`, append it to the enabled device extensions unless the guest already asked for it. The spec requires this extension to be enabled whenever the device exposes it, and a Windows guest never requests it. The per-device check is done once and cached in `physical_device_data` (`std::optional<bool> portability`). `VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME` lives in `vulkan_beta.h` behind `VK_ENABLE_BETA_EXTENSIONS`, so the name is spelled out as a `string_view` constant instead.

No wire-protocol change; the guest shim is untouched.

## Reproduction / verification

Host: Apple M5 Pro, macOS 26.5 (Darwin 25.6.0), Homebrew `vulkan-loader` 1.4.350.1 + `molten-vk` 1.4.1. Guest binaries: `vulkan-shim-test` and the `vulkan-shim` DLL from this tree, cross-compiled with mingw-w64 (i686 and x86_64), staged in the emulation root.

```
DYLD_LIBRARY_PATH=/opt/homebrew/lib \
VK_ICD_FILENAMES=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json \
./analyzer -e root --backend unicorn -s 'C:\vulkan-shim-test-x86.exe' 'C:\vulkan-shim.dll'
```

Before (origin/main `e8824779`, same host, same guest binaries):

```
[shim-test] vkCreateInstance -> -9, instance=00000000
```

After (this commit):

```
[shim-test] vkCreateInstance -> 0, instance=00000001
[shim-test] vkEnumeratePhysicalDevices -> 0, count=1
[shim-test] device[0]: 'Apple M5 Pro' type=1 api=1.3.334
[shim-test] queue families=4, graphics family=0, timestamp family=0
[shim-test] vkCreateDevice -> 0, device=00000003
[shim-test] empty submit -> 0, fence wait -> 0 -> PASS
[shim-test] fill+readback (0xDEADBEEF x64) -> PASS
[shim-test] clear+readback (16x16 -> 0xFF0000FF) wait=0 -> PASS
[shim-test] pipeline cache round trip -> PASS
[shim-test] ok
```

Same result for the x86 guest under `--backend unicorn` (WoW64) and for the x64 guest under both `--backend unicorn` and `--backend fex`: instance + device creation succeed, the GPU fills a buffer and clears a 16x16 image, and the guest reads the expected bytes back through the direct `vkMapMemory` alias. The FEX backend rejects the 32-bit guest (`FEX backend does not support WoW64 (32-bit) processes yet`), which is pre-existing and unrelated.

`windows-emulator-test`: 50/50 passing on this commit (`EMULATOR_ROOT` set). `clang-format --dry-run --Werror` clean on the changed source file.

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Branch was already a single commit ahead of origin/main (a08042ff); the reset --soft/recommit squash was still performed (new hash f4db26f6) and the pre/post diff is byte-identical. The prior commit message was already in repo style, so it was reused verbatim.
- The shared emulation root's staged guest shim (build/release/artifacts/root/filesys/c/windows/syswow64/vulkan-1.dll, dated Jun 27) and vulkan-shim-test-x86.exe (Jul 19) are built against the integration branch's gpu_bridge_protocol, which differs from origin/main by ~550 lines. Against this origin/main-based host they pass instance/device creation but fail at vkAllocateMemory, so clear+readback FAILs. This is the known stale-staged-DLL protocol mismatch, not a defect in this branch; I verified with freshly cross-compiled protocol-matched guest binaries instead (all PASS) and left the staged root files untouched.
- The doc paragraph added by this branch describes the verification as Unicorn-only; I additionally confirmed the x64 guest passes under --backend fex, so the PR body reports both. No factual claim in the doc paragraph was found to be wrong.
- Other sibling worker sessions had analyzer processes running (e.g. wow64-test-sample.exe from worktree wf_3f0afef6-539-22) during my runs; I did not touch them. My own analyzer runs all exited normally.

</details>

---

## `fix/wow64-userconnect-sharedinfo-layout`

Commit: `1e11a8dab3426f39f13eb9c9c15e1aafb40e6bec`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:fix/wow64-userconnect-sharedinfo-layout

**Commit message:**
```
fix(user): write the WoW64 userconnect reply in SHAREDINFO layout

The 32-bit user32!UserClientDllInitialize raw-copies the 0x238-byte
user-connect reply into _gSharedInfo and reads it with the same
8-byte-slotted SHAREDINFO layout as the 64-bit client. Both WoW64
writers (the CSRSS ApiPort reply and NtUserProcessConnect) filled a
separate WIN32K_USERCONNECT32 layout whose message-table fields landed
in the wrong slots, so DefWindowMsgs/DefWindowSpecMsgs stayed null and
user32 faulted on the first DefWindowProc(WM_NULL).

Route both writers through populate_user_shared_info, drop
WIN32K_USERCONNECT32 and its builders, pin sizeof(USER_SHAREDINFO) to
0x238, and update the stale investigation notes in
docs/windows-ui-emulation.md.
```

**PR body (paste into GitHub):**

```markdown
## Problem

A 32-bit (WoW64) guest that creates a window and calls `DefWindowProcA(hwnd, WM_NULL, 0, 0)` faults inside user32 on the first call. Since a typical message loop pumps `WM_NULL`, this hits any 32-bit GUI app that gets as far as a window.

Root cause: the 32-bit `user32!UserClientDllInitialize` raw-copies the 0x238-byte user-connect reply verbatim into `_gSharedInfo` and reads it with the same 8-byte-slotted `SHAREDINFO` layout the 64-bit client uses. Sogen's two WoW64 writers of that reply — the CSRSS ApiPort user-connect reply in `ports/api_port.cpp` and `NtUserProcessConnect` in `syscalls/user.cpp` — both filled a separate, hand-built `WIN32K_USERCONNECT32` struct whose `wndmsg_count`/`wndmsg_table` fields sat in the wrong slots. `SHAREDINFO.DefWindowMsgs` (0x218) and `.DefWindowSpecMsgs` (0x228) therefore stayed null in the 32-bit client, and user32 dereferenced them on the first `DefWindowProc`.

## Fix

- Route both WoW64 writers through the existing `win32k_userconnect::try_write_user_shared_info` / `populate_user_shared_info`, i.e. the same code the native 64-bit connect already uses. WoW64 user32 expects the 64-bit-slotted struct, so no narrowing layer is needed.
- Delete `WIN32K_USERCONNECT32`, its `static_assert`s, `build_wow64_userconnect`, `try_write_wow64_userconnect`, and the now-unused `k_wow64_wndmsg_count` / `k_wow64_ime_msg_count` constants. The `NtUserProcessConnect` writer was a second, independently maintained copy of the same wrong layout; it now shares the one path.
- Add `static_assert(sizeof(USER_SHAREDINFO) == 0x238)` so the reply size user32 copies is pinned in the type itself (the length checks in `api_port.cpp` and `resolve_wow64_destination` now key off `sizeof(USER_SHAREDINFO)`).
- Update `docs/windows-ui-emulation.md`: the section "Fix concept is validated; the clean location is NOT yet found" described this exact investigation as open, with `WIN32K_USERCONNECT32` named as a suspect and a proposed watchpoint-based next step. It is rewritten as resolved (the four globals are `SHAREDINFO.DefWindowMsgs`/`DefWindowSpecMsgs`), and the breadcrumb about the WoW64 path having "no working equivalent" of the message-bit tables is corrected.

Net: +38 / -146 lines across 6 files. No behaviour change for 64-bit guests — `populate_user_shared_info` itself is untouched.

## Reproduction / verification

Minimal 32-bit repro (`i686-w64-mingw32-g++ -static`): `RegisterClassA` + `CreateWindowExA`, then `DefWindowProcA(hwnd, WM_NULL, 0, 0)`, print the result, exit 0 iff it returned 0. Run with `analyzer -s --backend unicorn -e <root> c:/wm-null-repro-x86.exe` (FEX has no WoW64 support, so unicorn is the only applicable backend here).

Before (this branch's five source files swapped to their `origin/main` versions, rebuilt):
```
calling DefWindowProcA(WM_NULL)
Null-pointer call to 0x0; caller return address 0x0 (?+0x0)
```
analyzer exit code 1.

After (this commit, `1e11a8da`):
```
calling DefWindowProcA(WM_NULL)
DefWindowProcA(WM_NULL) returned 0
```
analyzer exit code 0.

`windows-emulator-test` with `EMULATOR_ROOT` set: 50/50 pass on this commit (release preset, macOS arm64 host).

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Branch was already a single commit (54a35234) ahead of origin/main, so pre_squash_commit_count=1; the reset --soft + recommit was still performed and the diff verified byte-identical (new hash 1e11a8da).
- Memory-note claim check: the '~114 lines of duplicate/dead struct-building code deleted' figure does not match the diff; actual stat is +38/-146 across 6 files (about 121 code lines removed, the rest is docs). The other claims (existing try_write_user_shared_info helper reused, second writer in NtUserProcessConnect fixed, static_assert(sizeof(USER_SHAREDINFO)==0x238), docs/windows-ui-emulation.md paragraph corrected) all verified against the diff. No test file is added by this branch (src/samples/test-sample/test.cpp is unchanged) -- the PR body claims none.
- Minor wording discrepancy inside the branch's own docs change: docs/windows-ui-emulation.md now says the pre-fix fault is 'a read of address 0 (the null bit-array pointer)', but under --backend unicorn the observed pre-fix symptom is 'Null-pointer call to 0x0'. The earlier investigation text was WHP-based. Not changed (out of scope for a squash pass); PR body reports the observed output.
- Remote fork branch existed at ab5a42e1 (an older iteration on an older base, diverged); pushed with --force-with-lease pinned to that exact hash, forced update succeeded. Not pushed to origin, no PR opened.

</details>

---

## `feat/gdi-user-syscalls`

Commit: `32ddfac8fbce923bb670645a678e7cc0969afe9d`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:feat/gdi-user-syscalls

**Commit message:**
```
feat(gdi): decode 1/4/8/16bpp DIBs in NtGdiSetDIBitsToDeviceInternal

The handler only accepted 24/32bpp BI_RGB DIBs and returned 0 scanlines for
everything else, leaving the target untouched. Read the colour table for
palettised depths (sized by bit depth, since pixel data may index past
biClrUsed), decode 16bpp as RGB555, and compose every pixel from bytes so
the result is host-endian independent.

The colour table is only decoded when it really is an RGBQUAD array:
DIB_PAL_COLORS (16-bit logical-palette indices) and BITMAPCOREHEADER
(RGBTRIPLEs) DIBs are rejected through the existing unsupported-DIB warning
instead of being read as RGBQUADs.

test-sample gains a "GDI DIB" test that blits a top-down DIB at each of
1/4/8/16bpp onto a 32bpp memory bitmap and reads the pixels back.
```

**PR body (paste into GitHub):**

```markdown
## Summary

`NtGdiSetDIBitsToDeviceInternal` only handled 24/32bpp `BI_RGB` DIBs. Any other depth hit the `unsupported DIB` warning and returned 0 scanlines, so the destination surface was left untouched. This adds 1/4/8bpp (palettised, via the `RGBQUAD` colour table) and 16bpp (RGB555) decoding, and keeps 24/32bpp behaviour unchanged.

Files: `src/windows-emulator/syscalls/gdi.cpp`, `src/samples/test-sample/test.cpp` (+148/-10).

## What changed

- A `dib_pixel_to_bgra32` helper decodes one pixel per bit depth. 1bpp is MSB-first, 4bpp high nibble first, 16bpp is RGB555 (not 565) with 5->8 bit expansion, 24/32bpp read the bytes as B,G,R. Every path composes the pixel from individual bytes, so the result no longer depends on host endianness.
- The colour table is read from `info + biSize` (not a hard-coded 40) and sized by bit depth (`1 << bpp`), not by `biClrUsed`, because pixel data may legally index past `biClrUsed`; unspecified entries decode to opaque black instead of indexing out of range.
- `color_use` (previously ignored) is now honoured for the reject path only: the table is treated as an `RGBQUAD` array only for `DIB_RGB_COLORS` with a `BITMAPINFOHEADER` or later. `DIB_PAL_COLORS` (16-bit logical-palette indices) and `BITMAPCOREHEADER` (`RGBTRIPLE`s) DIBs at <=8bpp still go through the existing warning-and-return-0 path rather than being misread as `RGBQUAD`s. The warning now also prints `usage=`.
- Compression other than `BI_RGB`, `biWidth <= 0`, and bit counts outside {1,4,8,16,24,32} are still rejected as before.

## Test

`test-sample` gains a `GDI DIB` test: a 4x1 top-down DIB at each of 1/4/8/16bpp is blitted with `SetDIBitsToDevice(..., DIB_RGB_COLORS)` onto a 32bpp memory bitmap (`CreateBitmap(4, 1, 1, 32)` selected into a `CreateCompatibleDC`), after which every pixel is read back with `GetPixel` and compared against the expected colour. The pixel patterns are chosen so index order (0,1,2,3 or 0,1,0,1 for 1bpp) and nibble/bit ordering are actually checked, and the 16bpp case encodes pure R/G/B/white in RGB555 so a 565 decode would fail.

## Verification

Host: macOS arm64 (Apple Silicon), `cmake --build --preset=release`, worktree at this commit.

- `windows-emulator-test` (gtest, `EMULATOR_ROOT` set): **50/50 passed**.
- Full `test-sample` cross-compiled from this branch's `test.cpp` with mingw-w64 (x86-64 PE32+), run as `analyzer -s -e root --backend unicorn c:/test-sample-gdi-x86_64.exe`: **30/30 tests `Success` including `GDI DIB`, exit 0, zero `unsupported DIB` warnings** in the log.
- The `GDI DIB` function extracted verbatim into a minimal harness and built for **both i686 (WoW64) and x86-64**:
  - unicorn backend: i686 `GDI DIB: Success` (exit 0), x86-64 `GDI DIB: Success` (exit 0)
  - fex backend: x86-64 `GDI DIB: Success` (exit 0); i686 not runnable, analyzer reports the pre-existing `FEX backend does not support WoW64 (32-bit) processes yet`.
- Negative control (same binaries, same root, `gdi.cpp` temporarily swapped to `origin/main` and analyzer relinked): x86-64 full sample and i686 harness both fail with `NtGdiSetDIBitsToDeviceInternal: unsupported DIB (bpp=1 compression=0 width=4)` followed by `SetDIBitsToDevice(1bpp) copied 0 scanlines`, `GDI DIB: Failure`, exit 1. Restoring this branch's `gdi.cpp` returns them to `Success`.
- Side effect worth noting: on `origin/main` the existing `GDI` test already emits `unsupported DIB (bpp=1 compression=0 width=128)` (a 1bpp DIB it blits internally was silently dropped); with this change that warning disappears and the DIB is drawn.

Note: the `test-sample.exe` shipped in `build/release/artifacts` on this host predates this `test.cpp` and does not contain the `GDI DIB` test, which is why the sample was rebuilt from source with mingw-w64 for verification.

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Context note did not match the branch: the actual diff contains NO pOwner-removal / desktop-window fix and never referenced dcfg32.exe; it is solely 1/4/8/16bpp DIB decoding in NtGdiSetDIBitsToDeviceInternal plus a 'GDI DIB' test-sample test. PR body is grounded only in the observed diff.
- Branch was already exactly ONE commit above origin/main (32ddfac8) with a clean conventional message and the Claude-Session trailer, and fork/feat/gdi-user-syscalls already pointed at that exact hash. A reset --soft + recommit would only have minted a new hash for an identical tree and forced a pointless force-push, so the existing commit was kept as-is; 'git push fork HEAD:refs/heads/feat/gdi-user-syscalls' reported 'Everything up-to-date'. No history rewrite, no force-with-lease.
- The test-sample.exe in build/release/artifacts and in the shared root (identical md5) predates this branch's test.cpp and does not contain the 'GDI DIB' test; the project does not cross-compile the sample on macOS. Verification used an ad-hoc mingw-w64 build of the branch's test.cpp (needed -lgdi32 -luser32 beyond the CMake link list, and no -municode). The full test.cpp does not compile for i686 with mingw due to a pre-existing stdcall-lambda in test_threads_winapi (unrelated to this branch), so 32-bit coverage used a verbatim extraction of the new test function only.
- i686 could not be verified on the FEX backend: pre-existing 'FEX backend does not support WoW64 (32-bit) processes yet'.
- Conflict risk for the next phase: this branch appends test_set_dib_bits_to_device() immediately after test_actctx() in the anonymous namespace and adds a RUN_TEST line right after RUN_TEST(test_gdi, "GDI") at the end of main() in src/samples/test-sample/test.cpp; fix/bcrypt-provider-parsing's test.cpp addition (per the note) will likely conflict in the same two hunks.
- A sibling workflow's analyzer (pid 41633, running c:\wow64-test-sample.exe against the same shared root) was live during my first x86-64 run; it was not mine so I did not kill it. Both runs were short and completed; no analyzer processes remained afterwards.

</details>

---

## `wt/macos-vulkan-dxvk-spoof`

Commit: `e10423e5138dfd5a852ac12d9267fb72a583afa7`

Compare: https://github.com/momo5502/sogen/compare/main...JackTYM:sogen:wt/macos-vulkan-dxvk-spoof

**Commit message:**
```
feat(vulkan): pass DXVK's adapter filter on MoltenVK portability devices

On Apple Silicon the host Vulkan library is MoltenVK, a portability
driver the Khronos loader hides unless the caller opts in, so
vkCreateInstance returned VK_ERROR_INCOMPATIBLE_DRIVER and no physical
device was ever enumerated. create_instance now enables
VK_KHR_portability_enumeration when the loader advertises it, and
create_device appends VK_KHR_portability_subset when the physical
device advertises it (checked once and cached per physical device).

DXVK's shared D3D8-11 adapter filter then requires geometryShader,
shaderCullDistance, VK_EXT_depth_clip_enable/depthClipEnable and
robustness2's robustBufferAccess2/nullDescriptor, all of which MoltenVK
lacks and none of which D3D9 uses. On a device advertising
VK_KHR_portability_subset the host reports those five as supported at
query time and retracts at vkCreateDevice: the synthetic extension is
stripped unless the device implements it and the requested feature
chain is AND-ed against a re-query of the same chain shape. A native
driver sees its real capabilities and the guest's request reaches it
unmodified.
```

**PR body (paste into GitHub):**

```markdown
## Summary

Makes `vulkan_host` usable on a macOS host where the system Vulkan implementation is MoltenVK (a non-conformant "portability" driver), and then gets DXVK's D3D9 front-end past its adapter filter on that driver. Two files: `src/windows-emulator/devices/vulkan_host.cpp` (+173/-1) and `docs/gpu-paravirtualization.md` (+60).

### Dependency / stacking note

This is **stacked on `wt/macos-vulkan-bringup`** and the single squashed commit here **contains bringup's content** (its `VK_KHR_portability_enumeration` / `VK_KHR_portability_subset` handling, ~103 lines) plus the DXVK-specific work on top (~131 lines). Either open this PR against `wt/macos-vulkan-bringup` so only the delta shows, or merge bringup first; merging this on its own would also land bringup's changes. The two halves are separable in the diff: everything under `is_portability_device`, `has_device_extension`, the `create_instance` extension block and the `VK_KHR_portability_subset` append in `create_device` is bringup; the rest is this branch.

### What changes

1. **Bringup half.** `create_instance` enables `VK_KHR_portability_enumeration` + `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` when the loader advertises the instance extension (without it the loader returns `VK_ERROR_INCOMPATIBLE_DRIVER` and never enumerates MoltenVK). `create_device` appends `VK_KHR_portability_subset` when the physical device advertises it, which the spec requires and which no guest ever asks for. Whether a device advertises it is checked once and cached (`std::optional<bool> portability` on `physical_device_data`).

2. **DXVK half.** DXVK applies one required-feature baseline across D3D8/9/10/11: `geometryShader`, `shaderCullDistance`, `VK_EXT_depth_clip_enable` (+ `depthClipEnable`) and `VK_EXT_robustness2`'s `robustBufferAccess2`/`nullDescriptor`. MoltenVK supports none of them; D3D9 uses none of them; DXVK has no config knob to relax the set. On a device advertising `VK_KHR_portability_subset` the host therefore:
   - reports those five as supported in `vkGetPhysicalDeviceFeatures2` and injects a synthetic `VK_EXT_depth_clip_enable` entry into `vkEnumerateDeviceExtensionProperties`;
   - in `vkCreateDevice`, strips `VK_EXT_depth_clip_enable` from the enabled list unless the device really implements it, and AND-s the requested feature chain (base `VkPhysicalDeviceFeatures` and every chained struct, by re-querying the same `sType` chain shape) against what the device actually supports, so MoltenVK is never asked to enable a feature it lacks.

   Gating is **solely** `VK_KHR_portability_subset` (the spec's own definition of a non-conformant device). There is no vendor-ID check and no DXVK detection. A conformant native driver takes none of these paths and the guest's request reaches it unmodified.

### Reproduction and verification

Host: Apple M5 Pro, macOS 26 (Darwin 25.6.0), Apple Clang, Homebrew `vulkan-loader` 1.4.350.1 + `molten-vk` 1.4.1 (reports driver string "MoltenVK 0.2.2209", Vulkan 1.3.334). The loader/ICD were selected via the environment (`DYLD_LIBRARY_PATH=/opt/homebrew/lib VK_ICD_FILENAMES=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json`), not via anything hardcoded. CPU backend: Unicorn (analyzer default, no `--backend` given). Emulation root: a real Windows tree via `-e <root>`.

Guests (all **64-bit**, all built from *this* tree with mingw-w64 14.0.0 so the shim matches the host's bridge protocol):
- `vulkan-shim-test.exe` + `vulkan-shim.dll` (`src/samples/`), run as `analyzer -s -e <root> c:/vulkan-shim-test.exe`
- `d3d9-probe.exe`, a ~50-line program: `Direct3DCreate9` → `GetAdapterCount` → `GetAdapterIdentifier` → `CreateWindowExA` → `CreateDevice(HAL, windowed, SOFTWARE_VERTEXPROCESSING)` → `Clear`, staged next to **DXVK v3.1 x64 `d3d9.dll`** (sha256 `c3ce5c92…52f0d8`) and the shim renamed to `vulkan-1.dll`; run with `--env DXVK_LOG_LEVEL info --env DXVK_LOG_PATH C:\dxvk` so DXVK's own log is captured.

Same guests, same root, same host binary rebuilt three times with only `vulkan_host.cpp` swapped:

| host `vulkan_host.cpp` | `vulkan-shim-test` | DXVK `d3d9-probe` |
|---|---|---|
| `origin/main` (e8824779) | `vkCreateInstance -> -9` (`VK_ERROR_INCOMPATIBLE_DRIVER`), nothing enumerated | DXVK log: `err: DxvkInstance::createInstance: Failed to create Vulkan instance`; process dies with uncaught `dxvk::DxvkError` inside `Direct3DCreate9` |
| bringup only (a08042ff) | `vkCreateInstance -> 0`, `device[0]: 'Apple M5 Pro' api=1.3.334`, `vkCreateDevice -> 0`, all checks PASS | DXVK log: `Found device: Apple M5 Pro (MoltenVK 0.2.2209)` / `Skipping: Device does not support required feature 'geometryShader'` / `warn: DXVK: No adapters found`; same `DxvkError` termination |
| **this branch** (e10423e5) | identical to bringup: `vkCreateDevice -> 0` (with a `VkPhysicalDeviceVulkan13Features` chain, i.e. the new masking path is exercised), `fill+readback (0xDEADBEEF x64) -> PASS`, `clear+readback (16x16 -> 0xFF0000FF) -> PASS`, `pipeline cache round trip -> PASS`, final `ok` | `GetAdapterCount -> 1`, `CreateDevice -> 0x00000000`, `Clear -> 0x00000000`, exit 0. DXVK's log shows it *requested* `VK_EXT_depth_clip_enable`, `VK_EXT_robustness2`, `geometryShader: 1`, `shaderCullDistance: 1` — the host masked those before MoltenVK saw them and `vkCreateDevice` succeeded |

Two things in the DXVK output that are DXVK's, not sogen's: it reports the adapter as `AMD Radeon RX 6700 XT` (0x1002/0x73DF) because of its own "Detected nonclassical vendor ID: 0x106b, hiding actual GPU" fallback; and the shim prints `no implementation for Vulkan function: vkCreateRenderPass2 …` for a number of entry points DXVK probes that the upstream shim doesn't export yet (pre-existing, returns null, did not stop device creation or `Clear`).

`cmake --build --preset=release` is clean on the squashed commit; `windows-emulator-test` **50/50** (the suite needs an emulation root containing the current user's `Users\<name>\AppData\Local` — `SHGetKnownFolderPath` verifies the folder exists — otherwise the six test-sample-based tests fail on `test_apis` regardless of this change). `--preset=tidy` was not run in this pass.

### Things a reviewer should know

- **Blast radius is wider than DXVK.** The spoof is keyed on the *device*, not the client: every guest Vulkan application on a portability device is told those five capabilities exist. A client that really uses one (a geometry shader, cull distance, a D3D12 title via vkd3d) fails at pipeline creation instead of skipping the device at adapter selection. Documented in `docs/gpu-paravirtualization.md` together with two more limitations: depth clip stays at the Vulkan default (matches D3D9's default, so a title that explicitly disables depth clipping misrenders), and `nullDescriptor` relies on MoltenVK tolerating `VK_NULL_HANDLE` descriptors in practice.
- **Hardcoded paths: checked, none in code.** `grep` over the whole diff for `/opt/homebrew`, `/usr/local`, `.dylib`, `rpath`: the only hit is one *documentation* line in the bringup section giving the `DYLD_LIBRARY_PATH=/opt/homebrew/lib VK_ICD_FILENAMES=…` invocation used for the verification run. The loader-name array in `vulkan_host.cpp` is unchanged (`libvulkan.1.dylib`, `libvulkan.dylib`, `libMoltenVK.dylib`). The earlier `feat/macos-vulkan` branch's absolute `/opt/homebrew/lib/...` dlopen fallbacks and its Apple vendor-ID (0x106B) portability heuristic were both dropped from this split.
- The `create_device` change replaces `has_features || feature_tail != &features2` with a single `has_feature_chain` bool that is also used to gate the masking; `feature_tail` is still used to build the chain.
- Not covered by this round: a native (non-portability) Linux/Windows driver was not run — the gating makes every new path a no-op there by construction, but there is no run confirming it; 32-bit (WoW64) guests were not run; nothing past `Clear` (no `Present`/draw) was exercised through DXVK.
- `VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME` lives in `vulkan_beta.h` behind `VK_ENABLE_BETA_EXTENSIONS`, hence the local string constant.

https://claude.ai/code/session_015RnPevLxkXHEaVsaGXubBz
```

<details><summary>Self-corrections / notes from the squash-and-verify pass</summary>

- Branch topology differed from the task description: it was NOT stacked on wt/macos-vulkan-bringup at start. The reflog shows a prior pass had already reset it onto origin/main (e8824779) and recommitted (225c0bd9) with bringup's content folded in, so merge-base with both origin/main and bringup was e8824779 and pre_squash_commit_count was 1. The squash was still performed (reset --soft + recommit with a cleaned message) and verified identical; the resulting single commit contains bringup's ~103 lines plus this branch's ~131-line delta. Disclosed in the PR body as a stacked-PR dependency.
- The hardcoded-path grep was NOT zero hits as the memory note claimed: one docs line (bringup's section) contains `DYLD_LIBRARY_PATH=/opt/homebrew/lib VK_ICD_FILENAMES=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json` as an example invocation. No hits in code. Stated accurately in the PR body rather than as 'none'.
- The shared worktree .claude/worktrees/wf_3f0afef6-539-17 had wt/macos-vulkan-bringup checked out (as warned), so work was done in a pre-existing scratchpad worktree for this branch (created by an earlier pass) instead of /tmp; it was removed with `git worktree remove --force` + `prune` when done. The main repo checkout (integration/fex-perf-plus-d3d9-umd) was not touched.
- windows-emulator-test initially reported 9 then 6 failures purely from the test environment (no emulation root / no user-profile dir in the worktree's artifacts). Resolved to 50/50 by supplying an overlay root with `Users/<profile>/AppData/Local`; these failures are in test-sample's SHGetKnownFolderPath path and are unrelated to this diff.
- Neither wt/macos-vulkan-bringup nor wt/macos-vulkan-dxvk-spoof existed on `fork` before this push; plain push created the branch (no force needed).
- clang-tidy preset (`--preset=tidy`) was not run in this pass.
- Not verified: behaviour on a native non-portability Vulkan driver (Linux/Windows), 32-bit guests, or DXVK rendering beyond Clear.

</details>

---
