# Build 26100 (Windows Server 2025) MW2 investigation — state as of 2026-07-17

## Context

The `fex-mac-silicon` PR (momo5502/sogen#1234) boots MW2/iw4sp.exe to the main menu when the
emulation root is generated from CI's **Windows 2022** (build 10.0.20348) runner leg. It does
**not** currently boot cleanly against CI's **Windows 2025** (build 10.0.26100) runner leg — two
separate, real issues were found there, one fixed, one still open. This branch (`mw2-server2025`,
worktree at `.worktrees/fex-mac-silicon-26100`) exists to get build 26100 working too, since CI
produces both root variants and both should ideally work.

Paused here per the user's direction to first address the automated Claude review findings on the
open PR (see PR #1234 comments, 2026-07-17 09:54 UTC) with individual, non-squashed commits on the
`fex-mac-silicon`/`fex-backend` branch, tested against the known-good Windows 2022 root before each
commit. Resume this build-26100 work after that.

## How the mismatch was found

1. The original local `fex-mac-silicon` emulation root (confirmed booting to menu by the user
   directly) was accidentally destroyed (`rm -rf build/release` while chasing an unrelated FEX-fork
   submodule-pruning test). No backup existed (no Time Machine, no local APFS snapshots).
2. Reconstructed a substitute root by copying `mw2/`+`ProgramData/steam` from a *different* worktree
   (`~/Documents/Coding/C++/sogen`, on `feat/mw2-on-upstream`) and manually copying 5 missing audio
   DLLs from that same worktree into a freshly-pulled CI "Windows 2025 Emulation Root" artifact.
   This reproduced a Miles audio failure — but it turned out to be a **build-mismatch artifact of
   the reconstruction itself**: the main worktree's DLLs are build 10.0.20348.5139 (verified via
   `pefile` reading `ntdll.dll`'s VS_VERSION_INFO resource), not build 26100. Mixing 20348 audio
   DLLs into a 26100 root reproduced the same symptom as having no audio DLLs at all.
3. Fixed `create-root.bat` to actually collect the 5 missing audio/COM DLLs
   (`audioeng.dll`, `audiokse.dll`, `audioses.dll`, `avrt.dll`, `twinapi.appcore.dll`) — this gap
   was real and independently confirmed by three other local branches
   (`ci-collect-audio-dlls`, `sync/windows-layer-fixes`, `ci/vkd3d-shader-bridge-fix`) that had
   already made the same fix for the same reason. Pushed as branch `test/create-root-audio-dlls`.
4. Tested a **single-build-consistent** Windows 2025 root (all DLLs from the same CI run, including
   the now-collected audio DLLs): the Miles audio issue was gone (zero `[audiosrv]` RPC calls logged
   — succeeded silently), but MW2 progressed further and hit a **new, unrelated** blocker:
   `Unimplemented syscall: NtDeleteWnfStateName (0xE1)`, called by `wow64.dll` itself (not game code)
   as part of a `DeviceApi\CMNotify` device-change-notification teardown sequence (subscribe →
   2×ALPC → delete-state-name). Confirmed via `grep` that this syscall genuinely isn't implemented
   anywhere in `windows-emulator/`.
5. Tested the **Windows 2022** root (build 20348, matching the other worktree's DLLs) with the same
   audio-DLL-fixed `create-root.bat`: MW2 reached the actual main menu, stably, with zero WNF/audio
   failures. Confirmed via screenshot (self-captured with `screencapture` + read directly — no user
   involvement needed to detect success/failure states; window titles readable via
   `osascript -e 'tell application "System Events" to tell process "analyzer" to get name of every
   window'`, but window *text content* is not accessible that way — sogen's UI is custom-rendered,
   not native AppKit controls, so a real screenshot is needed to read dialog text).
6. Confirmed via `pefile`-based PE version-resource inspection that the Windows-2022-leg CI root is
   build 10.0.20348.5139 — exactly matching what's in the other worktree, and (per the user) likely
   matching what the original destroyed root was built from too.

## Root cause split (two independent things, don't conflate them)

- **Audio DLL collection gap**: real, `create-root.bat`-level, already fixed on
  `test/create-root-audio-dlls` (needs folding into the real PR — not yet done, on hold per current
  review-fix work).
- **Build 26100 vs 20348 behavioral divergence**: NOT a DLL-collection problem — this is windows-
  emulator code that either (a) doesn't implement a syscall build-26100's `wow64.dll` happens to
  call ((`NtDeleteWnfStateName`), fixed below, or (b) has synthetic RPC response content that
  build-26100's `audioses.dll` validates more strictly than build-20348's does (open, see below).

## Fix applied on this branch (`mw2-server2025`)

Added a plain no-op `handle_NtDeleteWnfStateName()` returning `STATUS_SUCCESS` in
`src/windows-emulator/syscalls.cpp`, registered via `add_handler(NtDeleteWnfStateName)` — matches
the exact existing style of every sibling WNF syscall (`NtSubscribeWnfStateChange`,
`NtUnsubscribeWnfStateChange`, `NtSetWnfProcessNotificationEvent`, all plain
`return STATUS_SUCCESS` stubs). Verified: syscall-number→name resolution was already correct
(that's how the original log line printed "NtDeleteWnfStateName" for raw opcode 0xE1 in the first
place) — only the handler itself was missing. Regression pair (native `test-sample.exe`, FEX
`wow64-test-sample.exe`) passes after this change. This got MW2 *past* the WNF crash on build
26100, into deeper audio initialization than any prior build-26100 attempt reached.

## Still open: build-26100-specific Miles/audio abort (NOT yet root-caused, NOT fixed)

With the `NtDeleteWnfStateName` fix in place, on the build-26100 root, MW2 now:
1. Calls `AudioSrv`'s `GetDefaultAudioEndpoint` (opnum 25) — succeeds, returns a real endpoint id
   (traced: `{0.0.0.00000000}.{6C26BA7D-F0B2-4225-B422-8168C5261E45}`, the standard RDP-redirected
   render endpoint aliased into `MMDevices\Audio\Render` by `registry_manager::
   alias_remote_audio_endpoints`).
2. Opens the `AudioClientRpc` interface and calls, in order: `GetMixFormat` (opnum 0, succeeds —
   76-byte reply, the hardcoded 44.1kHz/2ch/32-bit-float WAVEFORMATEXTENSIBLE from
   `audio_service.cpp`'s `handle_get_mix_format`), then two more calls (48-byte replies each,
   almost certainly `IsFormatSupported` opnum 1 and `GetDevicePeriod` opnum 2, both also
   hardcoded-success stubs in `audio_service.cpp`) — all succeed with `HRESULT=S_OK`.
3. **Never reaches `OpenStream` (opnum 4) or `CreateRemoteStream` (opnum 7)** — instead, Miles
   immediately starts unmapping its own plugin DLLs (`c:\mw2\miles\mssmp3.asi`, `mssvoice.asi`,
   `mssds3d.flt`, `mileseq.flt`, `mssdsp.flt`, `msseax.flt`) — i.e. it decides to abort and tear
   itself down right after the three initial format/period queries, before ever trying to actually
   open the stream.
4. This is **exactly reproducible byte-for-byte** across two independent fresh-process runs (same
   88/304/76/48/48-byte RPC exchange both times, same plugin-unmap sequence both times) — ruled out
   as ASLR-related flakiness (this codebase's one well-documented flaky class); this is
   deterministic given the current code + build-26100 guest DLLs.
5. The *exact same* `audio_service.cpp` code, returning identical response bytes, works fine against
   build 20348's `audioses.dll` (reaches the menu). So build 26100's `audioses.dll` is doing some
   additional validation on the `GetMixFormat`/`IsFormatSupported`/`GetDevicePeriod` responses (or on
   something else entirely between those calls and the point where it gives up) that build 20348's
   doesn't do, or is more strict about, and our current hardcoded stub responses don't satisfy it.

### Next steps when resuming this branch

- This is **not** a missing-syscall problem — implementing more syscalls won't fix it. It's a
  response-*content* problem in code that already exists and already "succeeds" at the RPC layer.
- Don't guess at what build-26100's `audioses.dll` wants without evidence. Good next steps, in
  rough order of effort:
  - Use `EMULATOR_LOG_RPC=1` (already wired up, `port.cpp`'s debug hook — flagged in the Claude PR
    review as a "leftover debug hook" that may get cleaned up/gated during the review-fix pass; if
    it's removed or changed, this investigation will need an equivalent replacement) to capture the
    *exact* request bytes for each of the three calls and hand-decode/diff them against what a real
    Windows 11/Server 2025 WASAPI trace would produce for the same sequence, to check for a format
    field build-26100 might validate differently (e.g. a stricter channel-mask/sub-format check, a
    period-value bounds check, or a required extra field this NDR struct our code doesn't populate).
  - Consider capturing a real ALPC trace on build 26100 hardware/VM (the existing `alpc_capture.py`
    tooling mentioned in `audio_service.cpp`'s comments was used to derive the current hardcoded
    reply bytes from a *build-20348* capture — a build-26100-specific capture may be needed instead
    of assuming universality).
  - `idasql` against a build-26100 `audioses.dll` could help identify exactly what it checks between
    `GetDevicePeriod` returning and the decision to abort, without needing a live Windows 11/Server
    2025 machine to trace against.
- Given the existing regression pair only covers native/wow64-sample (not real audio), no CI
  coverage currently protects this path at all on either build — worth keeping in mind when deciding
  how invasive a fix to attempt.
