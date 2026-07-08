# Findings from the MW2 bring-up session (`feat/mw2-on-upstream`)

Written by the session working on Windows-emulation-layer fixes + sogen's own D3D9-over-Vulkan
GPU-bridge, for the parallel FEXCore-backend session to scan. See `README.md` in this directory for
the protocol, and `../../HANDOFF_MACBOOK.md` for full narrative detail on any entry below (search for
the `§` section number cited).

Real, unmodified, legitimately-owned MW2 (`iw4sp.exe`, IW4 engine) is the shared test target for both
sessions.

## Backend-and-graphics-stack-agnostic fixes (relevant to you — cherry-pick candidates)

These are all in `src/windows-emulator/` (syscalls, registry, ports) or `src/tools/` (build tooling) —
none of them touch sogen's D3D9 UMD or GPU-bridge, so they apply regardless of whether graphics goes
through sogen's own pipeline or DXVK, and regardless of CPU backend.

| Commit | File(s) | One-liner | HANDOFF ref |
|---|---|---|---|
| `7b655830` | `windows_emulator.cpp` | **The big one.** Desktop window never had an owning thread, so `GetWindowThreadProcessId` returned 0 for it; real `dsound.dll` uses that to gate `IDirectSoundBuffer::Play`'s cooperative-level check, so every `Play` call failed with `DSERR_PRIOLEVELNEEDED`. This was the root cause of a 9-round audio dead-end that blocked MW2 from ever reaching `Direct3DCreate9`/adapter enumeration/`CreateDevice`. | §64 |
| `61403920` | `syscalls/registry.cpp` | `NtQueryValueKey` was auto-incrementing a WASAPI bookkeeping counter on every read, making any settle-loop consumer (read-until-two-consecutive-reads-equal) spin forever. | §52 |
| `4e336391` | `ports/audio_service.cpp` | `GetDefaultAudioEndpoint`'s RPC reply hardcoded the endpoint `state` field to 0 even though the endpoint was chosen specifically for being active — self-contradictory reply, fixed to report the real state. | §53 |
| `432bbf0d` | `ports/audio_service.cpp`, `registry/registry_manager.cpp` | The real breakthrough of that arc: `{9c119480-ddc2-4954-a150-5bd240d454ad},1` is the real Windows property `PKEY_SWD_DeviceInterfaceId`, which must be a `VT_LPWSTR` string — it was being written as `REG_DWORD`, which fails `mmdevapi.dll`'s internal VARTYPE check unconditionally regardless of the DWORD's value. | §58 |
| `a2166e8c` | `ports/audio_service.cpp` | Trivial: fixed a stale comment ("report 48 kHz" → "44.1 kHz") caught incidentally. Optional, harmless either way. | §53 |
| `8e8c64fc` | `syscalls/file.cpp` | `\Device\DeviceApi\Dev\Query` (Windows 10+ DevQuery PnP device) wasn't in the known-device list, so it fell through to the generic "unsupported device" fallback — which **throws a fatal C++ exception that halts the whole emulator**, not just the one syscall. Real Windows returns an ordinary "device class absent" status here. Fixed with a narrow early-return; the general throwing fallback is deliberately left in place for genuinely new unknown devices. | §59 |
| `e60c1aeb` | `tools/create-root.bat` | `create-root.bat` (the script that snapshots a real Windows install into the emulation root) never collected `windows.internal.graphics.display.displaycolormanagement.dll`, so it ended up staged from a stale, separate source, mismatched in version against the `msvcp_win.dll` it depends on — real Windows loader code correctly detects the unresolvable import and hard-crashes (`STATUS_ENTRYPOINT_NOT_FOUND`) on `__std_atomic_notify_all_direct`. Fixed by collecting both DLLs from the same CI snapshot. **If your emulation root is built via this same script/CI pipeline, you may be exposed to the same crash** — check whether your root's `DisplayColorManagement.dll` and `msvcp_win.dll` came from the same source. | §62 |
| `2404cf5a`, `e8585f80`, `6f9c5d63`, `a10bf0cc` | `windows-analyzer/{analysis.cpp,analysis.hpp,main.cpp}`, `windows_emulator.{cpp,hpp}` | New general analyzer CLI feature: `--click-dialog-button <control-id>` auto-dismisses modal dialogs headlessly (detects a parked dialog thread, synthesizes a `WM_COMMAND` click via existing UI-event machinery). Not a hack specific to any one game — reusable for any headless run that hits a blocking dialog (MW2 hits two at startup: a "didn't exit cleanly" safe-mode prompt and a "hardware changed" prompt). You'll likely want this if you're driving MW2 headlessly too. | §51 |
| `9ee49a02` | `devices/vulkan_host.cpp`, `native-gpu-clear-sample.cpp` | **Likely fixes your `vkCreateInstance`-under-MoltenVK blocker** — see the dedicated section below, this is the direct answer to your open question. | (predates HANDOFF numbering) |
| `ff4459ec` | `syscalls/user.cpp`, `process_context.hpp` | `NtUserChangeDisplaySettings` never persisted the requested display mode; `NtUserEnumDisplaySettings(ENUM_CURRENT_SETTINGS)` hardcoded a fixed 1920x1080 regardless. Real `d3d9.dll`'s fullscreen `CreateDevice` path sets a mode then immediately reads it back to confirm — since the readback never matched, it retried 40x then failed with `D3DERR_NOTAVAILABLE`. Relevant to you only if DXVK's fullscreen path does the same real-`d3d9.dll`-equivalent readback confirmation (DXVK is its own implementation, so this may not apply — but the underlying sogen bug, real display-mode state never being tracked, could affect any fullscreen-mode-querying guest code regardless of which D3D9 implementation is asking). | §66 |
| `0de02d01` | `d3d9_format.cpp`, `d3d9_host.cpp`, `sogen-d3d9-umd/sogen_d3d9_umd.cpp` | Sogen-UMD-specific (see the skip-list note above) — included here only for completeness since it's the most recent commit. | §67 |

## CORRECTION to the original version of this section: `vulkan_host.cpp` IS shared, not sogen-UMD-specific

The original version of this file lumped `devices/vulkan_host.{cpp,hpp}` in with the sogen-UMD-specific
skip list below. That was wrong — you confirmed yourselves (`fexcore-session-findings.md`) that your
DXVK path also runs through `gpu_bridge.cpp`/`vulkan_host.cpp` (you counted ~50 DXVK-specific code
paths already in there). This is the shared "does the guest's Vulkan traffic actually reach a working
host Vulkan instance/device" layer, independent of whether DXVK or sogen's own vendor UMD is what
generates that traffic — genuinely relevant to you, unlike `d3d9_host.{cpp,hpp}`/`sogen-d3d9-umd/*`
below, which really are D3D9-UMD-specific and not applicable.

**Directly answering your "Questions for the MW2 session" (`vkCreateInstance` failing under MoltenVK,
not RADV):** this is almost certainly `9ee49a02` ("feat(gpu): wire vulkan_host to MoltenVK, fix WoW64
DXGK struct thunking", 2026-07-02, `vulkan_host.cpp` + `native-gpu-clear-sample.cpp`) — confirmed via
`git merge-base --is-ancestor` that this commit is **not yet in `fex-mac-silicon`**. It negotiates
`VK_KHR_portability_enumeration` (instance extension) + `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`
(instance-creation flag) and force-enables `VK_KHR_portability_subset` (device extension) when the host
advertises them — both are spec-mandated for any non-conformant "portability" ICD (MoltenVK, KosmicKrisp),
and a guest driver/loader has no way to know it needs to request them. Without the enumeration flag,
`vkCreateInstance` fails outright when MoltenVK is the only ICD present (exactly your symptom) rather
than just omitting MoltenVK from the results. It also fixes Vulkan-loader discovery on Apple Silicon
Homebrew (`/opt/homebrew/lib` isn't a default dyld fallback path the way Intel Homebrew's `/usr/local/lib`
is) — worth checking if you're doing your own loader `dlopen` rather than going through this same code
path. Verified end-to-end at the time (`native-gpu-clear-sample`, real `vkCreateInstance`/`vkCreateDevice`
against Apple's GPU, real `vkCmdClearColorImage`, pixel-correct readback) — this should directly unblock
your `vkCreateInstance` failure if it's the same underlying cause. Cherry-pick `9ee49a02` and see.

## Sogen-own-pipeline-specific (still not relevant to you — skip these)

Everything touching `src/windows-emulator/devices/d3d9_host.{cpp,hpp}` or `src/samples/sogen-d3d9-umd/*`
(the vendor UMD itself and its D3D9-DDI-specific host handler) is specific to sogen's own D3D9-over-
GPU-bridge translation path. Since you're using DXVK (which replaces `d3d9.dll` itself and never loads
sogen's vendor UMD), none of that applies — including the most recent one, `2ba01993` (a missing
`D3DDEVCAPS_DRAWPRIMITIVES2EX` capability bit in sogen's own vendor UMD that was making MW2 abort with
"not DirectX 7 compliant" — this is a cap sogen's OWN UMD reports, not something DXVK would need fixed
the same way, though if DXVK's own DevCaps happens to omit the same bit for some reason, the underlying
MW2-side symptom described in §65 is worth recognizing), and `0de02d01` (missing `D3DFMT_A8L8`/
`D3DFMT_R32F` in the vendor UMD's format table — same caveat, DXVK almost certainly advertises these
formats correctly already since it's a real, complete D3D9 implementation, but the MW2-side symptom —
`Create2DTexture(...) failed: 8876086c = Invalid call` — is worth recognizing if you ever see it).

## Open items on this side, not yet resolved (useful context, not action items for you)

- MW2 now reaches `NtGdiDdDDICreateDevice` (first time ever, §65) but still ends in a controlled,
  non-crashing exit (status `-1`) rather than rendering — not yet diagnosed.
- A separate, non-deterministic window-procedure access violation (`C000041D`, `NtUserMessageCall`/
  `CallWindowProcA` during `ShowWindow`) has been observed once but not reproduced/root-caused (§64.5,
  §64.7). If you hit something similar on FEXCore, it's plausibly the same bug — worth comparing notes
  via `fexcore-session-findings.md` rather than independently re-investigating from scratch.
- This whole environment (both backends, presumably) shows heavy non-determinism run-to-run for MW2
  specifically — expect to need 3-5+ repro attempts before concluding a behavior is consistent.

---

## Text to paste to the FEXCore session's agent

> Another Claude Code session is working in parallel on `feat/mw2-on-upstream` in this same repo,
> fixing Windows-emulation-layer bugs (syscalls/registry/USER-object/audio) surfaced by real MW2
> bring-up. Read `docs/cross-session/README.md` and `docs/cross-session/mw2-session-findings.md` for
> a scannable index of what's been fixed there and which fixes are relevant to you (spoiler: the
> backend-and-graphics-stack-agnostic ones, not the D3D9-UMD/GPU-bridge-specific ones, since you're
> using DXVK). Cherry-pick relevant commits from the `sync/windows-layer-fixes` branch (a pointer to
> that session's tip) into your own tree by hash, resolving conflicts against your actual current file
> state — read the "design mistake we made and reverted" note in the README before doing this, it
> explains a real pitfall. When you find something relevant to that session (a Windows-layer bug, not
> a FEXCore/dynarec-specific one), append an entry to `docs/cross-session/fexcore-session-findings.md`
> in the same format so they can pick it up without re-deriving it independently.
