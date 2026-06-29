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

**Always run builds and analyzer invocations in the foreground** — never pass `run_in_background: true` to the Bash tool. Background runs leave lingering shells and the timeout mechanism doesn't work reliably for them. Use the Bash tool's built-in `timeout` parameter (milliseconds) instead of the shell `timeout` command, and always capture and read the output yourself rather than asking the user to run commands.

## Smoke tests

Execute `analyzer.exe -s test-sample.exe` for smoke tests using the cmd in the directory of the built preset.
E.g.: `cmd /c "cd build\release\artifacts\ && analyzer.exe -s test-sample.exe"`

Other applications can also be executed in the emulator:
`cmd /c "cd build\release\artifacts\ && analyzer.exe -s path/to/binary.exe arg1 arg2 ..."`

## Development notes

- Run clang-format on changed files to ensure consistent formatting
- Do not generate code comments unless they add important information not otherwise deducible from code itself
- Do not use shortcuts or workarounds. Always prefer clean and idiomatic solutions.
