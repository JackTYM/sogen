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

## Smoke tests

Execute `analyzer.exe -s test-sample.exe` for smoke tests using the cmd in the directory of the built preset.
E.g.: `cmd /c "cd build\release\artifacts\ && analyzer.exe -s test-sample.exe"`

Other applications can also be executed in the emulator:
`cmd /c "cd build\release\artifacts\ && analyzer.exe -s path/to/binary.exe arg1 arg2 ..."`

## Development notes

- Run clang-format on changed files to ensure consistent formatting
- Do not generate code comments unless they add important information not otherwise deducible from code itself
- Do not use shortcuts or workarounds. Always prefer clean and idiomatic solutions.

## Working style for this WoW64/FEX effort

- The goal is the full implementation working end-to-end with a real game (mw2/iw4sp, a 32-bit
  WoW64 title) running correctly on the FEX/macOS-ARM64 backend - not just individual phases or
  synthetic test binaries passing. Keep that as the actual finish line.
- Do not pause to check in or ask whether to continue. Keep working through the task list (Phase
  A/B/C/D, the wild-branch/AddBlockLink follow-ups, the GS-segment/TEB layout conflict, etc.) and
  whatever new blockers are found along the way, until either a genuine question needs the user's
  input (an ambiguous decision only they can make, not just "this needs more design work") or the
  entire implementation actually works correctly with the game.
- When a deep architectural conflict is found (e.g. the TEB64/TEB32 layout vs. per-context rebasing
  conflict), make the best-reasoned engineering call yourself, document the reasoning and the
  alternatives considered in the plan file, and keep moving - don't stop and wait for approval on
  design choices that a senior engineer would just decide and proceed with.
- Keep the plan file (`~/.claude/plans/swift-painting-avalanche.md`) updated as the durable record of
  findings, fixes, and open threads - treat it as the source of truth to resume from, not just a log.
