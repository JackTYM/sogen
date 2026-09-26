# `zero-deps.bin` / `one-dep.bin`

Real `ACTIVATION_CONTEXT_DATA` blobs captured from two custom trivial exes
(`src/tools/dump-actctx-blob/test-exes/`), built and captured on a real Windows Server 2025
GitHub Actions runner (`windows-latest`, OS build `10.0.26100.33296`), used to differentially
reverse-engineer the format across multiple independent real captures rather than trusting a
single sample. See `docs/superpowers/specs/2026-09-24-activation-context-data-format.md`'s
"Differential analysis" section for the full methodology and findings.

- `zero-deps.bin`: `actctx-test-app-zero-deps.exe`'s manifest declares only its own identity
  (`name="T"`), no external dependency. 1 total assembly.
- `one-dep.bin`: `actctx-test-app-one-dep.exe`'s manifest declares its own identity
  (`name="Sogen.ActCtxTestApp.OneDependency"`) plus a dependency on the same
  `Microsoft.Windows.Common-Controls` v6 identity as `common_controls_v6_amd64.bin`. 3 total
  assemblies once the auto-pulled `.Resources` satellite is counted (matches
  `common_controls_v6_amd64.bin`'s own count exactly).

**Known, deliberately-preserved anomaly:** `one-dep.bin`'s TOC entry for id=1 has a garbage
`length` field (confirmed real, reproducible, not a capture artifact - ruled out timing races,
name length, manifest completeness, assembly count, and CI OS-image drift; see the format doc).
This fixture is kept specifically *because* it exercises that case - a parser/generator that
only ever sees "clean" data would never be tested against it. Do not "fix" this fixture by
re-capturing until it looks clean; the anomaly is real and load-bearing for this plan's tests.

Captured via the same disposable-CI-job pattern as `common_controls_v6_amd64.bin` (see that
fixture's own README); the job has since been removed from CI once these were captured.
