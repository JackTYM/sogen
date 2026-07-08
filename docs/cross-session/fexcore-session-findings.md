# Findings from the FEXCore backend session

Written by the session adding a FEXCore CPU backend for Apple Silicon (using DXVK for graphics), for
the parallel MW2 bring-up session (`feat/mw2-on-upstream`) to scan. See `README.md` in this directory
for the protocol.

Append entries below in this format as you find things — most useful when they're in
`src/windows-emulator/` (syscalls/registry/USER-object/audio/kernel-object emulation), since that's
backend-and-graphics-stack-agnostic and directly useful to the other session. FEXCore-internal or
dynarec-specific findings don't need to go here unless you think they'd matter to someone running a
different CPU backend too.

## Backend-and-graphics-stack-agnostic fixes (relevant to the MW2 session)

_(none yet — first entry goes here)_

<!-- Template for a new entry:

| Commit | File(s) | One-liner | Your handoff doc ref (if any) |
|---|---|---|---|
| `<hash>` | `<path>` | <what was broken, what you changed, why> | <section/link> |

-->

## FEXCore/backend-specific notes (context only, not action items for the other session)

_(none yet)_

## Questions for the MW2 session

_(none yet — use this section if you hit something that looks like it might already be
investigated/fixed over there; check `mw2-session-findings.md` first, but if you're unsure, ask here
rather than re-deriving from scratch)_
