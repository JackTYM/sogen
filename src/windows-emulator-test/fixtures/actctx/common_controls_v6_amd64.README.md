# `common_controls_v6_amd64.bin`

A real `ACTIVATION_CONTEXT_DATA` blob, captured directly from `PEB->ActivationContextData` of
a real, suspended Windows process — not hand-built, not a reconstruction. Used as a
byte-for-byte correctness oracle for the host-side blob generator this project builds (see
`docs/superpowers/plans/2026-09-24-comctl32-sxs-activation-context.md`, Tasks 5–6).

## How it was captured

- **Tool:** `src/tools/dump-actctx-blob/dump_actctx_blob.cpp` (committed alongside this
  fixture). Launches a target exe `CREATE_SUSPENDED`, reads its PEB via
  `NtQueryInformationProcess`, reads `PEB->ActivationContextData` and dumps the blob.
- **Target exe:** `C:\Windows\System32\mspaint.exe` (a stock Windows exe with a
  Common-Controls-v6 manifest — confirmed by this capture itself; no separate check was
  needed since a null `ActivationContextData` would have made the tool fail outright, and the
  script fell through to try `notepad.exe`/`explorer.exe` next in that case, but mspaint
  succeeded on the first attempt).
- **Runner:** GitHub Actions `windows-latest` runner, via a temporary scratch CI job
  (`capture-actctx-fixture` in `.github/workflows/ci-reusable.yml`, since removed — see git
  history around this commit if it needs to be re-added for a re-capture).
- **Date:** 2026-09-24 (CI run `36077492460` on `JackTYM/sogen`, branch
  `feat/comctl32-sxs-activation-context`).
- **Trimming:** the tool over-reads a generous 1 MiB (since the real blob's exact size wasn't
  known ahead of time) — raw capture was 12298 bytes, with a real `TotalSize` field at offset
  `0xC` reading `0x209C` (8348). Bytes `8348..12298` were confirmed all-zero (page-boundary
  padding, not real data) and trimmed off. This file is the trimmed 8348-byte result.
- **`PEB->ProcessAssemblyStorageMap`** was `0x0` (null) for this real, live capture —
  confirms it does not need to be populated for this case (an open question noted during
  design review).

## What's actually in it (confirmed by inspection, not assumed)

- Real assembly version: `6.0.26100.33438` (amd64) — matches the same version independently
  found via the WinSxS-collection POC earlier in this investigation (`create-root.bat`
  collecting from the same class of Windows Server 2022/2025 runner).
- DLL redirection: `comctl32.dll` / `comctl32.dll.mui`.
- **Window-class redirection is present** (e.g. `Button` → `6.0.26100.33438!Button`,
  `SysListView32` → `6.0.26100.33438!SysListView32`, and ~25 more) — confirms design review
  risk R7 is real, not speculative. Whether the generator needs to reproduce this section is
  still an open call for Task 5/6 to make empirically (does DLL redirection alone suffice for
  correct Notepad++ behavior?).
- **`dpiAware` setting text is present** in the application-settings section — confirms R6
  (populating this PEB field exposes more than just DLL redirection) is real, not speculative.
- Also present but not yet inventoried in detail: a reference to
  `Microsoft.Windows.Common-Controls.Resources` (the satellite localization assembly, a
  *separate* identity from the main Common-Controls assembly) — Task 5's format-parsing work
  should account for this if it shows up as its own roster entry, not conflate it with the
  main assembly.

## Re-capturing

If the format assumptions in Task 5 turn out wrong, or a different assembly/architecture
needs its own golden fixture, re-add the `capture-actctx-fixture` job (see git history) or
build+run `dump-actctx-blob.exe` directly on any real Windows machine.
