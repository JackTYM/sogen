# Cross-session coordination: MW2 bring-up (this branch) ↔ FEXCore backend (parallel worktree)

Two parallel Claude Code sessions are working on this project at the same time, both currently
testing against the same real, legitimately-owned Call of Duty: Modern Warfare 2 install:

- **This session** (`feat/mw2-on-upstream`): Windows-emulation-layer + sogen's own D3D9-over-Vulkan
  GPU-bridge/vendor-UMD graphics pipeline, running on the existing WHP/Unicorn CPU backends.
- **The FEXCore session** (a separate worktree, branch name TBD by that session): adding a FEXCore
  CPU backend for Apple Silicon, using **DXVK** to solve graphics (replacing the real `d3d9.dll` +
  vendor UMD with DXVK's own `d3d9.dll`, which talks directly to Vulkan — it does not use sogen's
  GPU-bridge/vendor-UMD at all).

These two efforts sit in genuinely different subsystems (CPU backend + graphics translation vs.
Windows syscall/kernel-object emulation), so they aren't merged into one coordinating session — see
the reasoning below. Instead they sync via **files, not live conversation**.

## Why not just merge into one session

- Each session carries deep, hard-won context (this session: 10+ rounds of MW2 audio/rendering RE,
  documented in `../../HANDOFF_MACBOOK.md`; the FEXCore session: presumably similarly deep dynarec/
  ARM64 backend context). Combining into one coordinator means re-deriving or summarizing both,
  which is expensive and lossy.
- The actual code these two efforts touch is mostly disjoint: this session's fixes and features live
  under `src/windows-emulator/` (syscalls, registry, ports) and `src/samples/sogen-d3d9-umd/`; the
  FEXCore session's work lives in a new CPU-backend implementation and doesn't touch the D3D9
  UMD/GPU-bridge at all (since it uses DXVK instead).
- **The one thing that DOES matter across both**: bugs found in `src/windows-emulator/`'s syscall/
  registry/USER-object/audio-RPC emulation are backend-and-graphics-stack-agnostic — they affect any
  CPU backend and any graphics solution, since they're about correctly emulating Windows itself (NT
  kernel objects, window management, the registry, WASAPI), not about translating D3D9 or executing
  x86 instructions. These are exactly the fixes worth syncing across sessions.

## The three files in this directory

| File | Written by | Read by | Purpose |
|---|---|---|---|
| `mw2-session-findings.md` | This session | FEXCore session | Scannable index of bugs/fixes found here, tagged by whether they're relevant to FEXCore's stack |
| `fexcore-session-findings.md` | FEXCore session | This session | Same, in the other direction |
| `README.md` (this file) | Either, rarely | Both | The protocol itself — update only if the protocol changes |

Each findings file is a **short, scannable index**, not a duplicate of the full narrative log. The
full detail stays in each session's own long-form handoff doc (this session's is
`../../HANDOFF_MACBOOK.md`) — link to a section/commit there instead of copying prose over.

**Update cadence:** whenever a finding lands that's tagged "relevant to the other session" below,
append an entry before moving on to the next task — don't batch it up for later, since the whole
point is the other session can pick up a fix as soon as it exists rather than independently
re-deriving it.

## The sync branch: `sync/windows-layer-fixes`

A branch that simply tracks this session's `feat/mw2-on-upstream` tip (a fast-forward pointer, not a
separately-curated/rebased branch — see "a design mistake we made and reverted" below for why not).
Its only purpose is to give the FEXCore session a stable, short name to fetch from instead of typing
out `feat/mw2-on-upstream`. The FEXCore session should **not** merge this branch wholesale — it
contains all of this session's D3D9-GPU-bridge/vendor-UMD work too, which is irrelevant noise for a
DXVK-based approach and would make for a needlessly large, conflict-prone merge. Instead:

1. Fetch it: `git fetch origin sync/windows-layer-fixes` (or add this repo/fork as a remote first if
   not already configured).
2. Look up the specific commit hash(es) you want from `mw2-session-findings.md`.
3. `git cherry-pick <hash>` each one individually into your own tree, resolving any conflicts against
   your own current file state (completely normal — don't expect a clean no-conflict pick just
   because the commit was clean in its original context).

### A design mistake we made and reverted (read this before cherry-picking)

We first tried building `sync/windows-layer-fixes` as a *separately rebased* branch — cherry-picking
just the "fix" commits onto the old common ancestor with `origin/main`, to give a minimal, clean diff.
This **failed silently in a dangerous way**: several of these fixes (e.g. the registry
auto-increment-counter fix) are fixes to bugs that were themselves introduced by earlier, larger
"bring-up" commits (audio/registry/win32k emulation added specifically for this fork's MW2 work,
commits like `df8ae3ad`/`0421c654`) that don't exist yet at that old ancestor. Cherry-picking the fix
onto a base that never had the bug produces a **silent no-op merge conflict resolution**, not an
error — it looks like it worked but the "fix" has nothing to fix. **Do not try to construct an
artificially minimal base for these commits.** Cherry-pick them into your actual current tree (which
presumably already has equivalent audio/registry/win32k emulation, since you're also running MW2),
and treat any conflict as real signal to look at, not noise to blindly resolve.

## Instructions for whoever is briefing the FEXCore session's agent

See the bottom of `mw2-session-findings.md` for the exact text to paste to that agent.
