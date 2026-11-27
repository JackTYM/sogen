# FEXCore (deps/FEX) — Ready to Open

Scope note: unlike `PR1234_SPLIT_READY.md`, `FEXPERF_CANDIDATES_DIGEST.md`, and
`BRANCH_TRIAGE_PR_READY.md` (all `fork`=`JackTYM/sogen` → `origin`=`momo5502/sogen`), the branches
below live in the **FEXCore submodule's own repo**: `fork`=`JackTYM/FEX` → `origin`=`momo5502/FEX`.

**Correction (2026-09-23, same day):** an earlier pass in this doc downgraded everything to
"not ready," claiming the whole stack was 9 commits stale relative to the real `macos-arm64` tip.
That was wrong — it compared against `JackTYM/FEX`'s own branch that happens to also be named
`macos-arm64` (a separate, unrelated personal branch on this same fork), not momo5502's real one.
Verified directly via `gh api repos/momo5502/FEX/branches/macos-arm64 --jq '.commit.sha'`
(bypassing any local git caching): momo5502's real, current `macos-arm64` tip is `ff4f18b2` — and
the original 6-commit stack was built directly on top of it with zero drift. That said, going
through everything on `JackTYM/FEX` that isn't on momo5502's real branches (per explicit
instruction, since all of it needs to go upstream eventually) turned up **6 more commits** on that
same personal branch — genuine, valuable, never-submitted FEXCore work dating back to July 2026,
one of which (`f790d39`) had real content overlap with this stack's own `ExitFunctionLink` fix.
Everything below reflects the full, reconciled result: **10 independent PR-ready branches**, all
rebuilt directly on `ff4f18b2`, all verified together as one stack (see "Full-stack verification"
below) and individually.

## Full-stack verification

Before splitting into individual branches, all 13 underlying commits (10 PRs' worth, one split
into two — see `feat/embedder-codebuffer-retention-api` below) were reconciled and verified
together as a single stack, directly on top of `ff4f18b2`:

- `git cherry-pick` in dependency order; 2 real conflicts, both resolved by hand (documented under
  the affected branches below: `feat/embedder-codebuffer-retention-api`'s split-out diagnostic
  commit, and `build/ios-accept-as-supported-platform`'s platform-check merge).
- `ninja FEXCore_shared` (both `asan` and `release` presets, in the ExternalProject's own build
  dir — the outer `cmake --build` doesn't reliably pick up submodule source changes on its own).
- `cmake --build --preset=asan --target fex-emulator analyzer`, then 3 consecutive clean runs of
  the WoW64 gate-crossing repro that originally surfaced this whole investigation (`EXIT=0`,
  `wow64-test-sample: ok` every time).
- `cmake --build --preset=release --target windows-emulator-test analyzer` — clean build.
- `EMULATOR_ROOT=<repo>/build/release/artifacts/root ./build/release/artifacts/windows-emulator-test`
  — **69/69 tests passed**, 15 suites.
- Release-mode `analyzer` run of the same WoW64 repro — `EXIT=0`, `wow64-test-sample: ok`.

This combined stack lives at `macos-arm64-sogen-fixes` (tip `07b7c9eb`) — sogen's own submodule pin
for local use (not itself a PR candidate; retire it once the individual PRs below land upstream).
`feat/wow64-fex-reported` (the sogen-side WoW64 feature branch) now pins there.

## Recommended opening order

9 of the 10 are mutually independent (no shared files, no logical dependency) — open in any order,
or in parallel. One real dependency:

- **`feat/lookup-host-address-for-guest-rip` stacks on `feat/embedder-codebuffer-retention-api`** —
  it calls the retention API the other branch adds, so it only makes sense (and only compiles) with
  that PR's content present. Open `feat/embedder-codebuffer-retention-api` first, or open
  `feat/lookup-host-address-for-guest-rip` as a stacked PR against it directly.

| Branch | Depends on |
|---|---|
| `feat/embedder-codebuffer-retention-api` | — |
| `fix/unconditional-pending-interrupt-check` | — |
| `fix/apple-clang-preserve-all-miscompile` | — |
| `feat/lookup-host-address-for-guest-rip` | `feat/embedder-codebuffer-retention-api` |
| `fix/wow64-tso-rsp-esp-64bit-gate` | — |
| `feat/dispatcher-jitpointers-literals` | — |
| `feat/allocator-pages-replaced-hook` | — |
| `perf/lookupcache-populate-l2-on-compile` | — |
| `build/ios-accept-as-supported-platform` | — |
| `build/jemalloc-static-fexcore-linking` | — |

---

## `feat/embedder-codebuffer-retention-api`

**PR title:** `feat(context): add embedder-facing CodeBuffer retention API`

**Branch:** `feat/embedder-codebuffer-retention-api` on `fork` (`https://github.com/JackTYM/FEX.git`)
**Commit:** `2c6b550f504fbc4efe769b990892e5cec30a7de7`
**Base:** `momo5502/FEX:macos-arm64` @ `ff4f18b2` (real tip, confirmed via GitHub API)

3 commits, squashed for review here but kept distinct in spirit:
1. Add `RetainCodeBufferAt`/`ReleaseCodeBufferAt` to the public `Context` interface.
2. Fix `ExitFunctionLink`'s validity guard to consult the full `CodeBufferList`, not just the
   current buffer (`IsAddressInCodeBuffer` → also check `FindCodeBufferContaining`).
3. **Recovered from a separate, never-submitted personal branch and merged in during this
   review**: enrich `ExitFunctionLink`'s bail-out diagnostic to report the current code buffer's
   range alongside the stale pointer. The original form of this (commit `f790d39`, July 26) predates
   commit 2 above (September) and touches the exact same lines — reconciled by hand: kept commit
   2's real functional fix (`FindCodeBufferContaining`) and layered `f790d39`'s diagnostic
   enrichment on top as an explicit follow-up commit, rather than losing either.

### Build/test results

See "Full-stack verification" above — this is part of that same verified stack. Per-branch checks
in this review:
- `clang-format --dry-run -Werror` on all 3 touched files: identical violation counts to the
  unmodified `ff4f18b2` baseline for 2 of them (`Context.h` ×2, pre-existing, unrelated to this
  change), 0 for the third (`Context.cpp`) — zero new violations.
- Comment density: 19/67 ≈ 28.4%. Judged against `Context.h`'s own Doxygen-heavy convention (every
  neighboring public method carries a comparable `@brief`/`@param` block for an equally short
  declaration), not sogen's blanket guideline, per CLAUDE.md's "match existing style" rule.
- Upstream overlap: `gh pr list --repo momo5502/FEX --state open` (empty) and `--state closed`
  (one historical PR, Android host support, unrelated).

### Rubric review

Single coherent feature (an embedder-facing lifecycle API for pinning code buffers across a
context switch, plus the diagnostic fix that made the underlying bug diagnosable in the first
place) even though it spans 3 commits — kept as 3 for reviewability rather than squashed to 1,
since each is independently understandable. No ABI-narration comments (the Doxygen blocks describe
the *contract*, not the edit). No workaround-in-shared-code smell — this is a clean new interface
addition mirroring the pattern of every other `Context` method. No debug hooks in the final commits
(temporary trace logging used during the original investigation was removed before any of this was
committed).

### PR body (ready for `gh pr create` against `momo5502/FEX`, base `macos-arm64`)

```markdown
## Problem

An embedder that multiplexes several logical execution contexts onto a single shared
`InternalThreadState`/`CPUBackend` (e.g. sogen's WoW64 backend, switching between a 32-bit and
64-bit engine on gate crossings) needs to keep a parked context's code buffer alive even after
FEXCore's own `CurrentCodeBuffer` bookkeeping moves on to a newer buffer for whichever other
context runs next. There's no way to express that today.

Separately, `ExitFunctionLink`'s validity guard only checked `IsAddressInCodeBuffer` against the
*current* code buffer — a `Record` pointer into a buffer that's still alive but no longer current
(exactly the case an embedder pinning old buffers needs to handle) was incorrectly treated as
corrupt, bailing to the dispatcher loop top instead of resuming correctly. And when that bail-out
diagnostic does fire for a genuinely corrupt `Record`, it only printed the stale pointer, with no
way to correlate it against the buffer that's actually current right now.

## Fix

- Add `Context::RetainCodeBufferAt(HostAddress)`/`ReleaseCodeBufferAt(HostAddress)`: refcounted,
  no-op on an address outside any known buffer, so safe to call speculatively.
- `ExitFunctionLink`'s validity check now also consults `FindCodeBufferContaining` — the full list
  of known buffers, not just the current one — before concluding a `Record` is corrupt.
- Enrich the bail-out diagnostic to print the current code buffer's range alongside the stale
  `Record` pointer, making cross-generation callret-stack staleness directly diagnosable.

## Verification

- Built `FEXCore_shared` (both ASan and release presets) with this change.
- Rebuilt sogen's `fex-emulator`/`analyzer`/`windows-emulator-test` against it.
- `windows-emulator-test`: 69/69 passed, 15 suites.
- 3 consecutive clean runs of a WoW64 gate-crossing repro that originally surfaced the
  `ExitFunctionLink` validity-check bug this fixes.
- `clang-format --dry-run -Werror` on all 3 touched files — zero new violations vs. baseline.
```

### Blocking concerns / caveats

None. Not opened via `gh pr create`, only pushed to `fork` — manual PR opening still required.

---

## `fix/unconditional-pending-interrupt-check`

**PR title:** `fix(jit): emit the pending-interrupt check before a direct conditional-branch backward edge`

**Branch:** `fix/unconditional-pending-interrupt-check` on `fork` (`https://github.com/JackTYM/FEX.git`)
**Commit:** `fb7f4642bde147e01ff8a3f58e09dcf9034b787b`
**Base:** `momo5502/FEX:macos-arm64` @ `ff4f18b2`

### Build/test results

Part of the verified full stack (see above). `clang-format --dry-run -Werror` on both touched
files — zero new violations. Comment density: **found and fixed** — first draft was 4/8 ≈ 50%
(the `#if` condition's comment explained both the mechanism and the reason in full prose); trimmed
to 2/6 ≈ 33% while keeping the one fact that matters (embedders outside real WOW64 rely on the same
preemption mechanism). Still slightly over the ~30% guideline for a diff this tiny, but every
further cut lost the actual reason for the change — same class of judgment call as this project's
own `peb-critsect-timeout`/`fix/apple-clang-preserve-all-miscompile` precedents.

### Rubric review

Two commits, same underlying mechanism (`NeedsPendingInterruptFaultCheck`, FEXCore's cooperative
preemption signal): one enables the flag unconditionally instead of only under real Windows WOW64,
the other emits the actual interrupt check before a direct conditional-branch backward edge (a gap
in coverage the first change alone doesn't close — a tight loop's backward edge could otherwise
never re-check the flag). Kept as 2 commits since they're genuinely separable, even though related.
No ABI-narration, no debug hooks, no workaround smell. Upstream overlap: none (same `gh pr list`
check as above).

### PR body (ready for `gh pr create` against `momo5502/FEX`, base `macos-arm64`)

```markdown
## Problem

`NeedsPendingInterruptFaultCheck` gates whether the JIT emits the per-block-entry store that lets
`stop()`'s cross-thread preemption (protecting `InterruptFaultPage` to force a fault) actually
work. It was only enabled under real Windows WOW64 (`#if defined(_WIN32) && !defined(ARCHITECTURE_arm64ec)`).
An embedder using the exact same cooperative-preemption mechanism outside a real WOW64 host
process (e.g. on macOS) never gets the check emitted at all, so the preemption signal is silently
never observed.

Separately, even with the check enabled, a tight loop whose only backward edge is a direct
(unconditional) conditional-branch could still run indefinitely without ever re-checking the flag,
since the check was only emitted at other points, not before that specific edge shape.

## Fix

- Enable `NeedsPendingInterruptFaultCheck` for any non-`arm64ec` build, not just real Windows WOW64.
- Emit the pending-interrupt check before a direct conditional-branch backward edge in `BranchOps.cpp`,
  closing the tight-loop gap.

## Verification

- Built `FEXCore_shared` (both ASan and release presets) with this change.
- Rebuilt sogen's `fex-emulator`/`analyzer`/`windows-emulator-test` against it.
- `windows-emulator-test`: 69/69 passed.
- 3 consecutive clean runs of the WoW64 gate-crossing repro that exercises this preemption path.
- `clang-format --dry-run -Werror` — zero new violations.
```

### Blocking concerns / caveats

Comment density (33%) is slightly over the ~30% guideline for this small a diff — judged
acceptable since the alternative is losing the actual reason for the change; flagged rather than
silently justified. Not opened via `gh pr create` — manual opening still required.

---

## `fix/apple-clang-preserve-all-miscompile`

**PR title:** `fix(build): disable clang::preserve_all on Apple/Clang`

**Branch:** `fix/apple-clang-preserve-all-miscompile` on `fork` (`https://github.com/JackTYM/FEX.git`)
**Commit:** `293917a3a4885f47d145b9953c3e49dce3f48bbf`
**Base:** `momo5502/FEX:macos-arm64` @ `ff4f18b2`

Unchanged from the original review earlier today — see git history of this doc for the full
writeup (LLVM's `preserve_all` frame-lowering miscompilation on Apple/upstream Clang 21+, disabled
via a `CMakeLists.txt elseif (APPLE)` branch matching the existing `MINGW` exclusion's shape).
Re-confirmed as part of the full-stack verification above: still clean, no new conflicts, no
change needed.

### Blocking concerns / caveats

None. Not opened via `gh pr create` — manual opening still required.

---

## `feat/lookup-host-address-for-guest-rip`

**PR title:** `feat(context): add FindHostAddressForGuestRIP lookup for embedders`

**Branch:** `feat/lookup-host-address-for-guest-rip` on `fork` (`https://github.com/JackTYM/FEX.git`)
**Commit:** `728877beba4f010106569d2a86f9a89ca647fe84`
**Base:** `feat/embedder-codebuffer-retention-api` @ `2c6b550` — **stacked, not independent** (calls
the retention API that branch adds; the earlier claim that this was independent of the retention
API was wrong — GitHub's own merge-base diff against `macos-arm64` would include it either way)

### Build/test results

Part of the verified full stack (see above). `clang-format --dry-run -Werror` on this branch's own
new file (`Core.cpp`) plus the 3 files shared with `feat/embedder-codebuffer-retention-api` —
zero new violations beyond that branch's own baseline. Comment density of just this branch's own
incremental addition (`FindHostAddressForGuestRIP`'s declaration + doc comment, on top of the
retention API branch): 35/42 ≈ 83% — matches this exact file's own established convention (the
sibling `RetainCodeBufferAt`/`ReleaseCodeBufferAt` carry equally long Doxygen blocks for equally
short declarations), not a red flag.

### Rubric review

Single self-contained addition: one new public virtual method, a thin wrapper over the existing
internal `LookupCache::FindBlock`. No new build-system surface, no debug hooks (temporary trace
logging used to verify the fix during the original investigation was removed before commit).
Upstream overlap: none.

### PR body (ready for `gh pr create` against `momo5502/FEX`, base `macos-arm64` — or stacked
against `feat/embedder-codebuffer-retention-api` if that PR is still open)

```markdown
## Problem

`RetainCodeBufferAt`/`ReleaseCodeBufferAt` (this repo's other open PR) take a *host* `CodeBuffer`
address. An embedder parking a logical execution context only ever has that context's *guest* RIP
at the point it needs to pin or release a buffer. Passing the guest RIP straight through silently
no-ops both calls forever — a guest address essentially never coincides with a real host
`CodeBuffer` address — with no error, no assertion, nothing to signal the caller did it wrong.

`LookupCache::FindBlock(Thread, GuestAddress)` already does exactly this translation internally,
but it lives in an internal header, invisible to embedders.

## Fix

Add `Context::FindHostAddressForGuestRIP(Thread, GuestRIP)` to the public `Context` interface,
implemented as a thin wrapper over the existing internal `LookupCache::FindBlock`. Returns 0 on a
miss, matching `RetainCodeBufferAt`/`ReleaseCodeBufferAt`'s own no-op-on-zero contract.

## Verification

- Built `FEXCore_shared` (both ASan and release presets) with this change.
- Verified functional correctness (not just compilation): temporarily instrumented
  `RetainCodeBufferAt`/`ReleaseCodeBufferAt` and an embedder's call sites to resolve through this
  method first — retains/releases that previously always no-op'd now correctly find and track real
  buffers, refcounts incrementing/decrementing in matched pairs. Instrumentation removed before commit.
- 3 consecutive clean runs of the embedder's own repro.
- `clang-format --dry-run -Werror` — zero new violations.
```

### Blocking concerns / caveats

Depends on `feat/embedder-codebuffer-retention-api` landing (or being open as a stacked base) —
not independently useful without it. Not opened via `gh pr create` — manual opening still required.

---

## `fix/wow64-tso-rsp-esp-64bit-gate`

**PR title:** `fix(fexcore): gate RSP/ESP TSO exemption to 64-bit`

**Branch:** `fix/wow64-tso-rsp-esp-64bit-gate` on `fork` (`https://github.com/JackTYM/FEX.git`)
**Commit:** `abcdd44a33cb1da49bb53974418f2dea47b58174`
**Base:** `momo5502/FEX:macos-arm64` @ `ff4f18b2` (independently re-confirmed live via
`gh api repos/momo5502/FEX/branches/macos-arm64`, and cross-checked against `momo5502/sogen`'s own
`.gitmodules` pin for `deps/FEX`, which matches exactly)

### Build/test results

- Verified via sogen's embedding: disposable worktree, `deps/FEX` pointed at this commit.
- `cmake --preset=release --target windows-emulator-test` — clean; FEXCore picked up the change
  directly, no ExternalProject re-build quirk needed this time.
- `windows-emulator-test`: **71/71 tests passed**.
- `clang-format --dry-run -Werror` on `OpcodeDispatcher.h`: 7 pre-existing violations (lines
  1299/1300/1328/1329/2710/2715/2720), confirmed byte-identical at the unmodified base — zero
  violations from this diff.
- No FEXCore-native test suite run — no visibility into `momo5502/FEX`'s own CI.

### Rubric review

Single self-contained fix, 2 lines, one file. Correctness verified independently: `Is64BitMode` is
a real member of `OpDispatchBuilder` (same class as `IsNonTSOReg`); all 3 call sites
(`OpcodeDispatcher.cpp:4428,4436,4480`) are inside member functions of that same class, so removing
`static` needed no call-site changes — confirmed via a repo-wide `git grep`, no other references
exist. 0% comment density (no comments touched). Native 64-bit path provably unchanged (only
`Is64BitMode == false` code gets new behavior). No overlap on `momo5502/FEX` (open empty, only the
historical Android PR closed).

### PR body (ready for `gh pr create` against `momo5502/FEX`, base `macos-arm64`)

```markdown
## Problem

`IsNonTSOReg` unconditionally exempted RSP/ESP-relative memory accesses from TSO ordering, on the
assumption that stack memory is thread-private. That holds for native Linux code, but not for
WoW64: 32-bit guest code can have stack-allocated structures (SEH exception records, SRW-lock wait
blocks) legitimately shared across threads via pointers taken to stack memory. Skipping TSO on
those accesses under weaker host memory ordering can surface real cross-thread visibility bugs
that don't occur on real x86 hardware, where the guest assumed full TSO everywhere, including the
stack.

## Fix

Gate the RSP exemption to `Is64BitMode` in `IsNonTSOReg`. Native 64-bit code keeps the existing
fast path; 32-bit/WoW64 code now gets full TSO ordering on RSP/ESP-relative accesses. The method
changes from `static` to a regular member to read `Is64BitMode`; all 3 existing call sites are
already inside member functions of the same class, so no call-site changes were needed.

## Verification

- Built and ran sogen's `windows-emulator-test` against this commit — 71/71 passed.
- Confirmed by inspection that `Is64BitMode` is a genuine member of the enclosing class and that
  removing `static` doesn't break any existing call site.

No FEXCore-native test suite was run (no visibility into `momo5502/FEX`'s own CI) —
verification is end-to-end through the actual embedder.
```

### Blocking concerns / caveats

No targeted regression test for the specific WoW64 shared-stack race this fixes — the 71-test pass
is a no-regression signal, not a repro of the original bug. Not opened via `gh pr create`.

---

## `feat/dispatcher-jitpointers-literals`

**PR title:** `dispatcher: move CTX/Sleep/CompileBlock/CompileSingleStep literals into JITPointers slots`

**Branch:** `feat/dispatcher-jitpointers-literals` on `fork` (`https://github.com/JackTYM/FEX.git`)
**Commit:** `841811cd74c3e3413f3c35c61aacf736293a84dd`
**Base:** `momo5502/FEX:macos-arm64` @ `ff4f18b2` (independently re-confirmed live via `gh api`)

### Build/test results

- Verified via sogen's embedding: clean build, no ExternalProject quirk this run.
- `windows-emulator-test`: **71/71 passed**, including `SerializationTest` and suites that exercise
  the dispatcher's hot path directly (`CompileBlock`/`CompileSingleStep` fire on every JIT compile).
- Correctness traced directly, not just trusted: `JITPointers` already holds several other
  process-wide-constant fields per-thread (e.g. `CPUIDObj`, `SyscallHandlerObj`); the 4 new fields
  match that shape exactly. Confirmed no other file references the removed `l_CTX`/`l_Sleep`/
  `l_CompileBlock`/`l_CompileSingleStep` labels, and no pre-existing field name collides. Values
  loaded are identical to what the removed literals held — only the storage/access mechanism
  changed. "No behavior change for in-process execution" holds.
- `clang-format --dry-run -Werror` on both files — clean.
- `Claude-Session:` trailer confirmed stripped (`git log --format=%B -1 <commit>` — no match).

### Rubric review

Single self-contained mechanical relocation, 2 files, 22/-23 lines. Comment density ~14%
(3/22), consistent with `CoreState.h`'s own light section-comment convention for this struct. No
debug hooks, no ABI-narration beyond a brief "why these need to be rewritable slots" clause (kept —
explains an external constraint, embedder interposability, not the edit itself). No overlap on
`momo5502/FEX`.

### PR body (ready for `gh pr create` against `momo5502/FEX`, base `macos-arm64`)

```markdown
## Problem

The emitted dispatcher's host-call targets for `ContextImpl::CompileBlock`, `CompileSingleStep`,
`SleepThread`, and the `CTX` object itself were PC-relative literals baked directly into the
dispatcher's code buffer. Every other host-call target is instead loaded from
`CpuStateFrame::Pointers` (a `JITPointers` struct), which an embedder driving the JIT from outside
the normal in-process call path can rewrite. These four were the only exception, so an embedder
that needs to interpose them (e.g. sogen's HVF vCPU spike, driving FEXCore's JIT from an external
executor) had no way to.

## Fix

Add `CTXObj`, `SleepFunc`, `CompileBlockFunc`, and `CompileSingleStepFunc` to `JITPointers`,
populated in `InitThreadPointers` with the exact same values the removed literals held. Replace
the dispatcher's four PC-relative literal loads with loads from these new slots, matching the
pattern already used for every other host callback. Purely mechanical: same values, loaded through
the existing per-thread-frame indirection instead of code-embedded literals. No behavior change for
in-process execution.

## Verification

- Built and ran sogen's `windows-emulator-test` against this commit — 71/71 passed, including
  tests that exercise the JIT compile/dispatch path directly.
- Confirmed no other reference to the removed labels exists, and no field-name collision.

No FEXCore-native test suite was run — verification is end-to-end through the actual embedder that
motivated this change.
```

### Blocking concerns / caveats

Not opened via `gh pr create` — manual opening still required.

---

## `feat/allocator-pages-replaced-hook`

**PR title:** `allocator: add PagesReplaced hook for embedders mirroring memory into a second mapping domain`

**Branch:** `feat/allocator-pages-replaced-hook` on `fork` (`https://github.com/JackTYM/FEX.git`)
**Commit:** `433acb2a6b6d58db3e969d1281f28f716d8468e7`
**Base:** `momo5502/FEX:macos-arm64` @ `ff4f18b2` (independently re-confirmed live via `gh api`)

### Build/test results

- Verified via a genuinely clean sogen-embedding build (fresh worktree, fresh submodule init,
  fresh configure) — `FEXCore_shared` and `windows-emulator-test` linked with **zero
  undefined-symbol errors**, confirming the earlier "undefined symbol" scare hit during this
  session's own live investigation was purely a stale-rebuild artifact (a prior build not picking
  up the submodule bump), not a real missing-export problem.
- `windows-emulator-test`: **71/71 passed**.
- `clang-format --dry-run -Werror`: `AllocatorHooks.h` clean; `Allocator.cpp` flags 2 violations
  (lines 316-317), confirmed byte-identical at the base commit, checked in-place (not a `/tmp`
  copy, which loses `.clang-format` scope and produces a misleading result — hit and corrected
  during this review).

### Rubric review

Single self-contained addition (one hook, 10 lines, 2 files), matching the exact existing pattern
of the sibling `mmap`/`munmap`/`VirtualTHPControl` hooks (null-initialized global, no
`FEX_DEFAULT_VISIBILITY` at the definition site — confirmed consistent with `VirtualTHPControl`'s
own definition — `extern` only at the header declaration, null-checked before calling). Comment
density 40% (4/10), heavier than the sibling hooks' own 0% baseline, but justified: documents a
genuinely non-obvious external constraint (a `Hypervisor.framework` `hv_vm_map` association
surviving a physical-page swap) not recoverable from the signature alone. Zero behavior change
when left null (the default). No overlap on `momo5502/FEX`.

### PR body (ready for `gh pr create` against `momo5502/FEX`, base `macos-arm64`)

```markdown
## Problem

`VirtualDontNeed`'s Apple path can't rely on `madvise(MADV_DONTNEED)` the way Linux does, so for
page-aligned regions it instead replaces the physical pages backing a live VA range in place via
`mmap(MAP_FIXED)`. An embedder that mirrors FEXCore-visible memory into a second mapping domain —
e.g. a Hypervisor.framework guest, where an `hv_vm_map` association is keyed to the original
physical pages and survives this kind of in-place replacement — keeps translating to the old,
now-detached pages afterward, with no signal that anything changed.

## Fix

Add a `PagesReplaced(void*, size_t)` hook to `FEXCore::Allocator`, following the exact same shape
as the existing `mmap`/`munmap` hooks: a null-initialized global, called (only if non-null) right
after `VirtualDontNeed`'s Apple-specific replacement. Embedders that don't set it see no behavior
change; one that does can re-establish its own mapping for the affected range.

## Verification

- Built `FEXCore_shared` and sogen's `windows-emulator-test` against this commit in a fully clean
  worktree/submodule/configure — no undefined-symbol or link errors.
- 71/71 tests passed.

No FEXCore-native test suite was run — verification is end-to-end through sogen's embedding.
```

### Blocking concerns / caveats

No consuming embedder code is included in this PR — this is the FEXCore-side hook in isolation.
Not opened via `gh pr create` — manual opening still required.

---

## `perf/lookupcache-populate-l2-on-compile`

**PR title:** `perf(lookupcache): populate L2 on initial block compile, not just L1`

**Branch:** `perf/lookupcache-populate-l2-on-compile` on `fork` (`https://github.com/JackTYM/FEX.git`)
**Commit:** `e6bbb2c33290cd7e14cf52444265712237cddbc2` (comment trimmed post-verification — see below)
**Base:** `momo5502/FEX:macos-arm64` @ `ff4f18b2` (independently re-confirmed live via `gh api`)

### Build/test results

- Verified via sogen's embedding: clean build, `windows-emulator-test` **71/71 passed**, including
  `SerializationTest`'s repeated emulate/reset/reserialize cycles that stress the block cache.
- `clang-format --dry-run -Werror` on `LookupCache.h`: 2 pre-existing violations (lines 32/41,
  lock-token constructors), confirmed identical at the unmodified base — zero new violations.
- **Correctness traced directly**: `ClearCache`/`InvalidateCacheRange`'s `CachedCodePages`
  bookkeeping runs unconditionally, before any `L1Only` branch — it already tracked every address
  regardless of whether L2 was populated eagerly or lazily. Full-cache clears
  (`ClearL2Cache`/`ClearThreadLocalCaches`) unmap the entire page-backed region regardless of how
  individual entries were written. So eagerly writing L2 on first compile is invalidated through
  the exact same paths as the pre-existing lazy L2-populate-on-L3-hit path — no new staleness
  window, no bypassed invalidation.

### Rubric review

- **Found and fixed (comment density):** the agent's initial verification measured 3 comment
  lines / 1 code line = 75%, well over the ~30% guideline, and flagged the `L1Only=false:` lead-in
  as narrating the parameter value rather than explaining the external constraint. Per this
  project's own established precedent (`peb-critsect-timeout`, `fix/unconditional-pending-interrupt-check`
  — a justified comment doesn't exempt it from trimming), tightened to the substantive fact only:
  "L1 eviction forces a locked L3 fallback lookup even though L2 caching is enabled; populate both
  on first insert." Now 2 comment lines / 1 code line = 67% — still elevated for a 1-line code
  change, but every further cut loses the actual "why" (the same class of irreducible case as the
  other two branches in this doc with above-30% density).
- Single self-contained fix: one call-site flag flip (`L1Only=true` → `false`), one file, 3/-2 lines
  after the trim. Diff matches the commit message's claim exactly.
- No workaround-in-shared-code smell — exercises `CacheBlockMapping`'s existing `L1Only` parameter
  as designed. `DisableL2Cache()` config flag still fully respected.
- Native/default path unchanged: only cache-population *timing* changes, not externally observable
  behavior.
- No overlap on `momo5502/FEX`.

### PR body (ready for `gh pr create` against `momo5502/FEX`, base `macos-arm64`)

```markdown
## Problem

`LookupCache::AddBlockMapping` inserted a freshly compiled block into L1 only. L2 caching for that
block was left to happen lazily, only if/when a later lookup missed L1 and fell through to the
locked L3 path (`GuestToHostMap::FindBlock`). In practice this means every block's first L1
eviction forces a fully locked L3 lookup to repopulate L2, even though L2 caching is enabled and
the block was already fully resolved at compile time.

## Fix

Change the initial-insert call in `AddBlockMapping` to populate both L1 and L2 on first insert
(`CacheBlockMapping` already supports this — `FindBlock`'s own L3-hit path uses the same call
shape); this just does it eagerly at compile time instead of lazily on first L1 miss.
`CachedCodePages` invalidation bookkeeping runs unconditionally regardless of this flag, so
invalidation correctness is unaffected — an evicted-then-rereferenced block now hits L2 directly
instead of falling through to the locked L3 fallback.

## Verification

- Built and ran sogen's `windows-emulator-test` (71 tests, including `SerializationTest`'s repeated
  emulate/reset/reserialize cycles) against this build — all pass.
- Traced `ClearCache`/`InvalidateCacheRange` to confirm the new eager-L2-populate path is
  invalidated through the exact same bookkeeping as the pre-existing lazy path.

No FEXCore-native test suite was run — verification is end-to-end through the actual embedder.
```

### Blocking concerns / caveats

Comment density (67%) remains above the ~30% guideline even after trimming — the remaining content
is judged irreducible (the "why" for a 1-line change). Not opened via `gh pr create`.

---

## `build/ios-accept-as-supported-platform`

**PR title:** `fix(ios): use the real JIT-write-protect toggle on iOS Simulator, not just macOS`

**Branch:** `build/ios-accept-as-supported-platform` on `fork` (`https://github.com/JackTYM/FEX.git`)
**Commit:** `75dbab7b3c049a5da4984688cca5b8b9654ca443` (2 commits: the original platform-support
addition, plus a real Simulator-correctness fix folded in during this review — see below)
**Base:** `momo5502/FEX:macos-arm64` @ `ff4f18b2` (independently re-confirmed live via `gh api`)

### Build/test results

- Verified via sogen's embedding: clean build (166/166 FEXCore objects), `windows-emulator-test`
  **71/71 passed**, including `EmulationTest` (exercises the JIT/`JITWriteScope` path directly,
  confirming the macOS host branch is functionally unchanged).
- **Merge-conflict resolution verified correct**: this branch required resolving a real conflict
  when rebasing onto `ff4f18b2` (which had independently added `Android` to the same
  `CMakeLists.txt` platform check this commit adds `iOS` to). Read the resolved file directly —
  both `Android` and `iOS` are present in the final `elseif` chain, nothing lost from either side.
- `clang-format --dry-run -Werror`: all 5 originally-touched files flagged only pre-existing
  violations (confirmed byte-identical at the base commit); the new file this review added
  (`AllocatorHooks.h`) is clean.

### Rubric review

- **Found and fixed — a real correctness defect, not a style nit.** The original commit's own
  comment claimed: "the Simulator needs no protection at all. No-op here is correct on both
  [device and Simulator]." This is empirically wrong: the iOS Simulator is a plain macOS process
  subject to the exact same per-thread `MAP_JIT` W^X model as desktop macOS — skipping the toggle
  SIGBUSes on the very first JIT write. A fix for exactly this already existed elsewhere in this
  same fork's history (`fde1849`, dated 5 days later, part of the completed iOS real-device
  bring-up effort — see "Outstanding iOS work" note at the end of this document) but was never
  folded into this branch or its lineage. Folded it in during this review: resolves
  `pthread_jit_write_protect_np` via `dlsym` (since the iOS SDK headers mark it `unavailable` for
  iOS targets even though it links and works on the Simulator) and calls it there; naturally
  no-ops on real device, where the symbol genuinely doesn't exist. Re-verified after folding in:
  clean build, 71/71 (the macOS host path this can be tested on is provably unaffected — the new
  code only activates under `TARGET_OS_IPHONE`).
- The three `#include <sys/user.h>` removals are genuine dead-include cleanup (grepped each file
  for any symbol that header could provide — none found; page-size logic already goes through
  `FEXCore::Utils::FEX_PAGE_SIZE`) needed because the iOS SDK doesn't ship that header at all.
- Comment density (both commits combined): elevated but judged justified — documents a real,
  non-obvious external constraint (iOS has no per-thread JIT-write-protect API; the Simulator
  needs the real toggle despite SDK availability annotations saying otherwise) matching this
  file's own established convention for comparable mechanisms.
- Confirmed via `git show ff4f18b2:CMakeLists.txt` that the real base has zero existing iOS
  awareness — this is a genuinely new platform-support addition. No overlap on `momo5502/FEX`.

### PR body (ready for `gh pr create` against `momo5502/FEX`, base `macos-arm64`)

```markdown
## Problem

FEXCore's top-level `CMakeLists.txt` platform check rejects any `CMAKE_SYSTEM_NAME` other than
Linux/Android/Windows/Darwin, so an iOS build fails immediately at configure time.

`JITWriteScope` (the RAII guard around `MAP_JIT` W^X toggling used by JIT code emission) treated
all of iOS the same as real device (no toggle at all) — but the iOS *Simulator* is a plain macOS
process subject to Apple Silicon's per-thread W^X model exactly like desktop macOS. Skipping the
toggle there faults the very first JIT write with EXC_BAD_ACCESS/SIGBUS. Real device genuinely has
no per-thread JIT-write-protect API at all; write/execute control there needs a separate
breakpoint-protocol split-mapping technique (out of scope for this PR).

## Fix

- `CMakeLists.txt`: extend the platform-support check to also accept `CMAKE_SYSTEM_NAME STREQUAL
  "iOS"`, alongside the existing Android/Darwin exceptions.
- `JITWriteScope`: on iOS, resolve `pthread_jit_write_protect_np` via `dlsym` rather than calling
  it directly (the SDK headers mark it `unavailable` for iOS targets even though it links and
  works on the Simulator) and call it there. This naturally no-ops on real device, where the
  symbol genuinely doesn't exist, while giving the Simulator the real toggle it actually needs.
- Drop three unused `#include <sys/user.h>` (not available in the iOS SDK, not referenced by any
  symbol in those files on any platform).

## Verification

- Full release build of FEXCore + the embedding test target from a clean worktree/submodule/configure.
- `windows-emulator-test`: 71/71 passed, including the JIT-exercising `EmulationTest`, confirming
  macOS/Linux/Android behavior is unaffected (the Simulator-specific code only activates under
  `TARGET_OS_IPHONE`).
- iOS device/Simulator runtime behavior itself was not exercised (no iOS toolchain in this
  environment) — the Simulator fix's own correctness was previously confirmed empirically
  elsewhere (SIGBUS reproduced and resolved) rather than in this specific verification pass.
```

### Blocking concerns / caveats

- iOS device/Simulator behavior itself is unverifiable from this macOS host — no iOS
  SDK/Simulator toolchain exercised here. The Simulator fix's correctness rests on the prior
  empirical finding recorded in `fde1849`'s own commit message, not on independent re-verification
  in this pass.
- Not opened via `gh pr create` — manual opening still required.
- **See "Outstanding iOS work" below** — this branch is the first 2 of a much larger, already-
  completed body of iOS real-device work (10 more commits) that was not folded into this PR.

---

## `build/jemalloc-static-fexcore-linking`

**PR title:** `build: link JemallocLibs into the static FEXCore target too, not just FEXCore_shared`

**Branch:** `build/jemalloc-static-fexcore-linking` on `fork` (`https://github.com/JackTYM/FEX.git`)
**Commit:** `4ddd36f70e7c5dde096a9b3f5b57d31197982106`
**Base:** `momo5502/FEX:macos-arm64` @ `ff4f18b2` (re-confirmed live via `gh api` at verification time)

### Build/test results

- Confirmed the claim by reading the actual target definitions: `FEXCore_shared` already carried
  the `JemallocLibs` link line (unchanged); the plain static `FEXCore` target had no equivalent
  line before this fix, despite real code in `FEXCore_object` (`DebugData`'s
  `FEXAllocOperators`) calling `FEXCore::Allocator::malloc`/`free` unconditionally.
- **Directly reproduced the bug mechanism**: built a temporary scratch executable linking only
  against the static `FEXCore` target. With the fix reverted: link fails with exactly
  `Undefined symbols ... FEXCore::Allocator::malloc(unsigned long)` / `free(void*)`. With the fix
  restored: links and runs cleanly. Scratch target removed before finishing.
- `windows-emulator-test`: **71/71 passed** — confirms the `FEXCore_shared` path (what sogen
  actually consumes) has no regression, as expected since this only adds a link edge to a
  previously-unused static target.

### Rubric review

Single self-contained fix, one `target_link_libraries` line, one file. Comment density (~86% of
net-inserted lines) checked against this exact file's own local precedent for the identical class
of comment (the `AddLibrary` function's existing static/shared-link-gap explanation immediately
above, ~95% density for the 1-line fix it justifies) — proportionate, if anything leaner than the
sibling precedent it directly extends; no trim needed. No overlap on `momo5502/FEX`.

### PR body (ready for `gh pr create` against `momo5502/FEX`, base `macos-arm64`)

```markdown
## Problem

`FEXCore::Allocator::malloc/calloc/memalign/free/etc.` are declared in `AllocatorHooks.h` as "to
be defined externally," defined in `AllocatorHooks.cpp` (compiled into the `JemallocLibs` static
lib). Real FEXCore code calls them unconditionally (e.g. `FEXAllocOperators`, used by `DebugData`
in the JIT). `target_link_libraries(${PROJECT_NAME}_shared PRIVATE JemallocLibs)` already resolves
this for the shared library; the plain static `FEXCore` target never got the same treatment —
nothing linked it directly until sogen's iOS work started consuming it statically. A strict static
consumer (no undefined symbols tolerated, e.g. Xcode's ld64) hits unresolved
`FEXCore::Allocator::malloc`/`memalign`/etc. at its own final link time.

## Fix

Add `target_link_libraries(${PROJECT_NAME} PRIVATE JemallocLibs)` next to the existing `_shared`
line.

## Verification

- Directly reproduced the gap: linking a minimal executable against only the static `FEXCore`
  target and calling `FEXCore::Allocator::malloc`/`free` fails with the exact undefined-symbol
  error without this fix, and links/runs cleanly with it.
- Built and ran sogen's full `windows-emulator-test` (consumes `FEXCore_shared`, unaffected): 71/71
  passed — no regression on the existing consumer.
```

### Blocking concerns / caveats

Not opened via `gh pr create` — manual opening still required.

---

## Outstanding iOS work (not included in this doc — scoping question)

While reconciling `build/ios-accept-as-supported-platform`, discovered that `JackTYM/FEX:main`
contains **10 more commits**, all past the `fde1849` Simulator fix already folded in above,
implementing a complete iOS real-device JIT bring-up: a self-contained "JIT26" breakpoint-protocol
client (`c5f504d`), symbol-collision fix (`fb6b7c3`), routing FEXCore's JIT allocator through it on
real device (`a52e4c8`), signal-safety/guard-page fixes (`121780f`), `sys_icache_invalidate` for
`ClearICache` on Apple platforms (`b817d0e`), two diagnostic commits (`c916ae4`, `90121d5`), a
poll-based JIT block-entry stop check for real device (`7a39158`), and three more real-device
correctness fixes (`10bdffd`, `f0923f5`, `fb70314`). This matches this project's own memory record
of a completed "iOS FEX real-device bring-up" effort (2026-09-16).

This is a qualitatively larger, more complex, more platform-specific body of work than anything else
in this document — not a handful of small self-contained fixes, but an entire subsystem (a custom
breakpoint-protocol JIT client) that would need its own dedicated review pass (build verification
on a real device/Simulator is not possible from this macOS host at all, unlike the other branches
here which could at least be verified not to regress the macOS path). Deliberately **not** folded
into this PR-readiness pass — flagging for a scoping decision rather than silently expanding
further.
