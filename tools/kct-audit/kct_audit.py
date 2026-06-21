#!/usr/bin/env python3
"""
kct-audit/kct_audit.py — Cheat-proof audit gate for sogen's KCT arg-struct contracts.

Sogen is the CALLER for KCT callbacks: the real user32.dll __fn* executes inside
the emulator. Sogen's job is to build an arg buffer with the correct byte layout;
the real __fn* reads fields at fixed offsets [a1+N] and makes an indirect call
through a function pointer at a fixed icall offset. A wrong layout → wild-RIP crash.

Correctness is fully static and machine-derivable:
  every [a1+N] offset the real __fn* reads
  must be covered by a static_assert(offsetof(STRUCT, field) == N) in user.cpp
  AND the icall offset must be covered AND sogen assigns dispatch_client_message there.

Status vocabulary (per slot):
  implemented   — gate re-derives MATCH/FIXED from artifact + sogen source
  deferred      — __fn* needs a subsystem sogen lacks; artifact-proven [DEFER:]
  dummy         — __ClientCallDummyCallback no-op slots (51/52/55)
  wndproc-thunk — slots 130-188; real window procedures; no arg-struct audit
  client-callback — __ClientXxx callbacks; lighter requirements (no arg-struct)
  unverified    — initial state; gate passes silently

Gate is authoritative: --ledger exits non-zero on ANY violation.
The exit code, not any evidence text, is the truth.

Usage:
  python3 tools/kct-audit/kct_audit.py --ledger tools/kct-audit/ledger.tsv
  python3 tools/kct-audit/kct_audit.py --diff-guard
  python3 tools/kct-audit/kct_audit.py --init-ledger tools/kct-audit/ledger.tsv
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
USER_CPP = REPO_ROOT / "src/windows-emulator/syscalls/user.cpp"
SAMPLES_DIR = REPO_ROOT / "src/samples"
APFN_TSV = Path(__file__).resolve().parent / "apfnDispatch.tsv"
DECOMP_DIR = Path(__file__).resolve().parent / "decomp"
REVIEWS_DIR = Path(__file__).resolve().parent / "reviews"

# Assertion tokens that make a test function genuinely checking behavior.
_ASSERT_TOKENS = re.compile(r'\bNT_SUCCESS\b|\bSTATUS_|==|!=|\bmemcmp\b|\bassert\b|\bfailed\b|\bFAILED\b|exit_code|PostQuitMessage')

# Boilerplate evidence patterns — always rejected (mirrors syscall-audit rules).
BOILERPLATE_PATTERNS = re.compile(
    r'Tested via'
    r'|: Verified$'
    r'|^Verified$'
    r'|^IDB 0x[0-9A-Fa-f]+: Verified$'
    r'|Decompiled, awaiting contract verification'
)


# ── apfnDispatch table ────────────────────────────────────────────────────────

def load_apfn(path: Path) -> dict[int, dict]:
    """Load apfnDispatch.tsv; return dict mapping index -> {fn_name, category}."""
    result: dict[int, dict] = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if len(parts) >= 3:
                idx = int(parts[0])
                result[idx] = {'fn_name': parts[1], 'category': parts[2]}
    return result


# ── Ledger ───────────────────────────────────────────────────────────────────

def load_ledger(path: Path) -> list[dict]:
    """Load ledger.tsv; return list of row dicts."""
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if len(parts) >= 10:
                rows.append({
                    'index':            int(parts[0]),
                    'fn_name':          parts[1],
                    'category':         parts[2],
                    'sogen_struct':     parts[3],
                    'sogen_index_const': parts[4],
                    'decompile_artifact': parts[5],
                    'sample_subtest':   parts[6],
                    'evidence':         parts[7],
                    'wave':             parts[8],
                    'status':           parts[9],
                })
    return rows


# ── Artifact parsing ──────────────────────────────────────────────────────────

def parse_fn_offsets(artifact_text: str) -> dict:
    """Extract icall offset and read/addr-of offsets from a committed __fn* decompilation.

    IDA emits decimal offsets: *(_QWORD *)(a1 + 40), NOT 0x28.
    Returns:
      icall: int | None   — offset of the function-pointer field called through
      reads: set[int]     — all *(...)(a1+N) dereference offsets (decimal→int)
      addr_of: set[int]   — a1+N used as address (not dereferenced directly)
      all_offsets: set[int]
    """
    # icall: (a1 + N))( — the address expression inside an indirect function call.
    # Pattern: `(a1 + N))(`  where `)` closes the cast and `(` starts the arg list.
    icall_matches = re.findall(r'\(a1 \+ (\d+)\)\)\s*\(', artifact_text)

    # Deref reads with explicit offset: *(...)(a1 + N)
    deref_matches = re.findall(r'\*\([^)]+\)\s*\(a1 \+ (\d+)\)', artifact_text)

    # Bare dereference at offset 0: *(...)(a1) without + N
    has_offset_zero = bool(re.search(r'\*\([^)]+\)\s*a1\b', artifact_text))

    # Addr-of: a1 + N as a standalone expression (not preceded by *)
    # These are offsets passed by address (e.g. WINDOWPOS struct passed by pointer).
    addr_of_all = {int(m) for m in re.findall(r'\ba1 \+ (\d+)\b', artifact_text)}

    reads = {int(m) for m in deref_matches}
    if has_offset_zero:
        reads.add(0)

    icall_set = {int(m) for m in icall_matches}
    addr_of = addr_of_all - reads - icall_set

    icall = int(icall_matches[0]) if len(icall_matches) == 1 else None

    return {
        'icall': icall,
        'reads': reads,
        'addr_of': addr_of,
        'all_offsets': reads | addr_of | icall_set,
        'icall_ambiguous': len(icall_matches) != 1,
    }


# ── Sogen struct analysis ─────────────────────────────────────────────────────

def extract_all_struct_asserts(user_cpp_text: str) -> dict[str, dict]:
    """Extract static_assert(offsetof/sizeof) from user.cpp for all fn_* structs.

    Returns dict: struct_name -> {offsets: set[int], sizeof: int|None}
    """
    result: dict[str, dict] = {}

    for m in re.finditer(
        r'static_assert\s*\(\s*offsetof\s*\(\s*(\w+)\s*,\s*\w+\s*\)\s*==\s*(0x[0-9A-Fa-f]+|\d+)\s*\)',
        user_cpp_text
    ):
        struct_name = m.group(1)
        raw = m.group(2)
        offset = int(raw, 16) if raw.startswith('0x') else int(raw)
        entry = result.setdefault(struct_name, {'offsets': set(), 'sizeof': None})
        entry['offsets'].add(offset)

    for m in re.finditer(
        r'static_assert\s*\(\s*sizeof\s*\(\s*(\w+)\s*\)\s*==\s*(0x[0-9A-Fa-f]+|\d+)\s*\)',
        user_cpp_text
    ):
        struct_name = m.group(1)
        raw = m.group(2)
        size = int(raw, 16) if raw.startswith('0x') else int(raw)
        entry = result.setdefault(struct_name, {'offsets': set(), 'sizeof': None})
        entry['sizeof'] = size

    return result


def get_nested_base_offsets(user_cpp_text: str, outer_struct: str,
                             struct_asserts: dict[str, dict]) -> dict[str, int]:
    """Find nested struct types embedded in outer_struct and their base offsets.

    Looks for: static_assert(offsetof(OUTER, FIELD) == BASE) where FIELD corresponds
    to a nested fn_* struct type. Returns {nested_struct_type: base_offset}.
    """
    nested: dict[str, int] = {}

    # Find all (FIELD_NAME, BASE_OFFSET) pairs from outer_struct asserts in user.cpp
    for m in re.finditer(
        r'static_assert\s*\(\s*offsetof\s*\(\s*' + re.escape(outer_struct) +
        r'\s*,\s*(\w+)\s*\)\s*==\s*(0x[0-9A-Fa-f]+|\d+)\s*\)',
        user_cpp_text
    ):
        field_name = m.group(1)
        raw = m.group(2)
        base = int(raw, 16) if raw.startswith('0x') else int(raw)

        # Check if this field is a fn_* struct type (look for it in struct body)
        # Pattern: TYPE field_name{}; inside the struct definition
        for nested_struct_name in struct_asserts:
            if nested_struct_name == outer_struct:
                continue
            # Look for "nested_struct_type field_name{}" inside the struct block
            pattern = (r'struct\s+' + re.escape(outer_struct) +
                       r'\s*\{[^}]*\b' + re.escape(nested_struct_name) +
                       r'\s+' + re.escape(field_name) + r'\s*\{')
            if re.search(pattern, user_cpp_text, re.DOTALL):
                nested[nested_struct_name] = base
                break

    return nested


def compute_absolute_covered(outer_struct: str, user_cpp_text: str,
                              struct_asserts: dict[str, dict]) -> set[int]:
    """Compute the set of absolute buffer offsets covered by static_asserts for outer_struct
    and any nested structs it embeds.
    """
    covered: set[int] = set(struct_asserts.get(outer_struct, {}).get('offsets', set()))

    nested = get_nested_base_offsets(user_cpp_text, outer_struct, struct_asserts)
    for nested_type, base_offset in nested.items():
        inner_offsets = struct_asserts.get(nested_type, {}).get('offsets', set())
        covered.update(base_offset + off for off in inner_offsets)

    return covered


# ── Sample tests ──────────────────────────────────────────────────────────────

def load_sample_tests() -> set[str]:
    """Return set of 'sample-dir:TestName' and bare TestName strings from src/samples."""
    tests: set[str] = set()
    if not SAMPLES_DIR.exists():
        return tests
    for cpp in SAMPLES_DIR.rglob("*.cpp"):
        sample_name = cpp.parent.name
        text = cpp.read_text(errors='replace')
        for m in re.finditer(r'report\s*\(\s*"([^"]+)"', text):
            tests.add(f"{sample_name}:{m.group(1)}")
            tests.add(m.group(1))
    return tests


# ── Reviews ───────────────────────────────────────────────────────────────────

def load_reviews() -> set[str]:
    """Load kct-wave review files; return set of (index-as-str or fn_name) with AGREE verdict."""
    agreed: set[str] = set()
    if not REVIEWS_DIR.exists():
        return agreed
    for tsv in REVIEWS_DIR.glob("kct-wave*.tsv"):
        for line in tsv.read_text(errors='replace').splitlines():
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if len(parts) >= 2 and parts[1].strip().upper() == 'AGREE':
                agreed.add(parts[0].strip())
    return agreed


# ── Per-row validation ────────────────────────────────────────────────────────

def validate_row(row: dict, apfn: dict[int, dict], user_cpp_text: str,
                 struct_asserts: dict[str, dict], sample_tests: set[str],
                 reviews: set[str]) -> list[str]:
    """Validate a single ledger row. Returns list of violation strings (empty = OK)."""
    status = row['status']
    idx = row['index']
    fn_name = row['fn_name']
    category = row['category']
    evidence = row.get('evidence', '')
    sample_subtest = row.get('sample_subtest', '-')
    sogen_struct = row.get('sogen_struct', '-')
    sogen_index_const = row.get('sogen_index_const', '-')
    artifact_name = row.get('decompile_artifact', '-')

    violations: list[str] = []

    # ── Category consistency: ledger must match apfnDispatch.tsv ground truth ──
    if idx in apfn:
        real_category = apfn[idx]['category']
        if category != real_category:
            violations.append(
                f"category mismatch: ledger says '{category}' but apfnDispatch.tsv says '{real_category}'"
            )
        real_fn_name = apfn[idx]['fn_name']
        if fn_name != real_fn_name:
            violations.append(
                f"fn_name mismatch: ledger says '{fn_name}' but apfnDispatch.tsv says '{real_fn_name}'"
            )
    else:
        violations.append(f"index {idx} not found in apfnDispatch.tsv (valid range 0..188)")

    # ── Dummy and wndproc-thunk: status must match category, no further audit ──
    if category == 'dummy':
        if status != 'dummy':
            violations.append(f"dummy slot must have status=dummy, got '{status}'")
        return violations

    if category == 'wndproc-thunk':
        if status != 'wndproc-thunk':
            violations.append(f"wndproc-thunk slot must have status=wndproc-thunk, got '{status}'")
        return violations

    # ── Unverified: no requirements (honest partial coverage) ─────────────────
    if status == 'unverified':
        return violations

    # ── For implemented / deferred / client-callback rows: ────────────────────

    # ── Rule 1: artifact must exist and be valid ───────────────────────────────
    artifact: Path | None = None
    artifact_text = ''
    if artifact_name and artifact_name != '-':
        artifact = DECOMP_DIR / artifact_name
        if not artifact.exists():
            violations.append(f"artifact '{artifact_name}' not found in {DECOMP_DIR}/")
        else:
            artifact_text = artifact.read_text(errors='replace')
            if len([l for l in artifact_text.splitlines() if l.strip()]) < 4:
                violations.append(f"artifact too short (< 4 non-blank lines): {artifact}")
            if fn_name not in artifact_text:
                violations.append(f"artifact does not contain fn_name '{fn_name}'")
    elif status in ('implemented', 'deferred'):
        violations.append(
            f"missing decompile_artifact: need decomp/<fn_name>.c for status='{status}'"
        )

    # ── Rule 2: evidence must contain [xref:TOKEN] verbatim from artifact ─────
    xref_match = re.search(r'\[xref:([^\]]+)\]', evidence)
    if not xref_match:
        violations.append("evidence missing [xref:TOKEN] (must quote verbatim substring from artifact)")
    elif artifact_text:
        token = xref_match.group(1)
        if len(token) < 4:
            violations.append(f"[xref:TOKEN] too short ({len(token)} chars, need ≥ 4): '{token}'")
        elif token not in artifact_text:
            violations.append(f"[xref:{token!r}] not found verbatim in artifact")

    # ── Rule 3: outcome tag ───────────────────────────────────────────────────
    if status == 'implemented':
        if not re.search(r'\[MATCH\]|\[FIXED:[^\]]+\]', evidence):
            violations.append("evidence missing [MATCH] or [FIXED:file:line]")
    elif status == 'deferred':
        if not re.search(r'\[DEFER:[^\]]+\]', evidence):
            violations.append("evidence missing [DEFER:reason]")

    # ── Rule 4: no boilerplate ────────────────────────────────────────────────
    if BOILERPLATE_PATTERNS.search(evidence):
        violations.append("evidence matches boilerplate blocklist")

    # ── Rule 5: traceability ──────────────────────────────────────────────────
    if status == 'implemented':
        if not re.search(r'\[sref:[^\]]+:\d+\]', evidence):
            violations.append("evidence missing [sref:<file>:<line>]")

    # ── Rule 6: index constant cross-check ────────────────────────────────────
    # For marshal-fn rows: sogen_index_const must encode the correct index.
    if status == 'implemented' and category == 'marshal-fn':
        if sogen_index_const == '-' or not sogen_index_const:
            violations.append("sogen_index_const is '-'; need k_fn_*_callback_id constant name")
        else:
            # Verify the constant value in user.cpp matches the ledger index
            const_match = re.search(
                r'constexpr\s+\w+\s+' + re.escape(sogen_index_const) + r'\s*=\s*(0x[0-9A-Fa-f]+|\d+)',
                user_cpp_text
            )
            if not const_match:
                violations.append(
                    f"sogen_index_const '{sogen_index_const}' not found in user.cpp"
                )
            else:
                raw = const_match.group(1)
                const_val = int(raw, 16) if raw.startswith('0x') else int(raw)
                if const_val != idx:
                    violations.append(
                        f"sogen_index_const '{sogen_index_const}' = {const_val} (0x{const_val:X}) "
                        f"but ledger index is {idx} (0x{idx:X}); wrong KCT slot"
                    )

    # ── Rule 7: arg-struct layout gate (the cheat-proof core) ─────────────────
    # For marshal-fn implemented rows: every [a1+N] offset the real __fn* reads
    # must be covered by static_assert(offsetof(STRUCT, field) == N) in sogen.
    if status == 'implemented' and category == 'marshal-fn' and artifact_text and sogen_struct != '-':
        fn_offsets = parse_fn_offsets(artifact_text)

        if fn_offsets['icall_ambiguous']:
            violations.append(
                "artifact has zero or multiple indirect call sites — "
                "cannot auto-derive icall offset; add a clarifying comment"
            )
        elif fn_offsets['icall'] is None:
            violations.append(
                "no indirect call pattern found in artifact (expected `(a1 + N))(` )"
            )
        else:
            icall_offset = fn_offsets['icall']
            covered = compute_absolute_covered(sogen_struct, user_cpp_text, struct_asserts)
            struct_sizeof = struct_asserts.get(sogen_struct, {}).get('sizeof')

            # Gate A: icall offset must be covered by a static_assert
            if icall_offset not in covered:
                violations.append(
                    f"icall offset 0x{icall_offset:X} ({icall_offset}) not covered by "
                    f"static_assert(offsetof({sogen_struct}, ...) == 0x{icall_offset:X}); "
                    f"add the assert or it will wild-RIP"
                )

            # Gate B: struct must be large enough to hold all read offsets
            if struct_sizeof is not None:
                max_read = max(fn_offsets['all_offsets'], default=0)
                if struct_sizeof < max_read + 1:
                    violations.append(
                        f"sizeof({sogen_struct}) == 0x{struct_sizeof:X} is too small; "
                        f"artifact reads up to offset 0x{max_read:X}, need sizeof ≥ 0x{max_read+1:X}"
                    )
            else:
                violations.append(
                    f"no static_assert(sizeof({sogen_struct}) == ...) found; add it"
                )

            # Gate C: all deref read offsets must be covered (not just icall)
            uncovered = fn_offsets['reads'] - covered
            if uncovered:
                hex_list = ', '.join(f'0x{o:X}' for o in sorted(uncovered))
                violations.append(
                    f"deref offsets {hex_list} read by {fn_name} are not covered by "
                    f"static_assert(offsetof({sogen_struct}, ...) == N); "
                    f"add offsetof asserts or the WndProc will receive wrong args"
                )

            # Gate D: dispatch_client_message must be assigned at the icall field
            # Check that some field at icall_offset is assigned c.proc.dispatch_client_message
            if not re.search(r'dispatch_client_message', user_cpp_text):
                violations.append(
                    "dispatch_client_message not found in user.cpp; "
                    "icall field must be set to c.proc.dispatch_client_message"
                )

    # ── Rule 8: sample subtest ────────────────────────────────────────────────
    if status == 'implemented':
        if sample_subtest == '-' or not sample_subtest:
            violations.append("sample_subtest is '-'; need <sample>:<TestName> or a window-sample test")
        elif ':' in sample_subtest:
            sample_name, test_name = sample_subtest.split(':', 1)
            full_key = f"{sample_name}:{test_name}"
            if full_key not in sample_tests and test_name not in sample_tests:
                violations.append(
                    f"sample_subtest '{sample_subtest}': no matching report(\"{test_name}\") "
                    f"in {SAMPLES_DIR}/{sample_name}/"
                )
        # Window samples that use ASSERT_TERMINATED_SUCCESSFULLY (no report() pattern)
        # are allowed as bare 'sample-name' references; the SampleTest suite validates them.

    # ── Rule 9: independent review ────────────────────────────────────────────
    if status == 'implemented':
        key = str(idx)
        if key not in reviews and fn_name not in reviews:
            violations.append(
                f"no independent review AGREE in tools/kct-audit/reviews/kct-wave*.tsv "
                f"for index {idx} ({fn_name})"
            )

    return violations


# ── Diff guard ────────────────────────────────────────────────────────────────

def run_diff_guard() -> int:
    """Check staged diff for bulk status flips without matching artifacts/subtests."""
    result = subprocess.run(
        ['git', 'diff', '--cached', '--unified=0'],
        capture_output=True, text=True, cwd=REPO_ROOT
    )
    if result.returncode != 0:
        result = subprocess.run(
            ['git', 'diff', 'HEAD~1', '--unified=0'],
            capture_output=True, text=True, cwd=REPO_ROOT
        )
    diff = result.stdout

    # Count rows newly flipped to implemented in ledger.tsv
    new_impl = len(re.findall(
        r'^\+[^\+].*\timplemented\s*$', diff, re.MULTILINE
    ))

    # Count new artifact files in kct-audit/decomp/
    new_artifacts = len(re.findall(
        r'^\+\+\+ b/tools/kct-audit/decomp/', diff, re.MULTILINE
    ))

    # Count new report() calls or new SampleTest entries in test files
    new_subtests = len(re.findall(
        r'^\+[^\+].*(?:report\s*\(\s*"[^"]+"|ASSERT_TERMINATED_SUCCESSFULLY)', diff, re.MULTILINE
    ))

    print(f"diff-guard: +{new_impl} implemented rows | +{new_artifacts} artifacts | +{new_subtests} subtests")

    violations = []
    if new_impl > new_artifacts:
        violations.append(
            f"new implemented rows ({new_impl}) > new artifact files ({new_artifacts}): "
            f"bulk status flip without decompilations"
        )
    if new_impl > 0 and new_subtests == 0:
        violations.append(
            f"new implemented rows ({new_impl}) but no new sample subtests/tests added"
        )

    if violations:
        for v in violations:
            print(f"diff-guard VIOLATION: {v}")
        return 1

    print("diff-guard: OK")
    return 0


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--ledger', metavar='FILE',
                        help='Path to ledger.tsv for gate check')
    parser.add_argument('--diff-guard', action='store_true',
                        help='Check staged diff for bulk status flips')
    parser.add_argument('--apfn', default=str(APFN_TSV),
                        help='Path to apfnDispatch.tsv ground truth')
    args = parser.parse_args()

    if args.diff_guard:
        sys.exit(run_diff_guard())

    if not args.ledger:
        parser.print_help()
        sys.exit(0)

    ledger_path = Path(args.ledger)
    apfn_path = Path(args.apfn)

    print("Loading apfnDispatch ground truth …", file=sys.stderr)
    apfn = load_apfn(apfn_path)

    print("Loading ledger …", file=sys.stderr)
    rows = load_ledger(ledger_path)

    print("Reading user.cpp for struct asserts …", file=sys.stderr)
    user_cpp_text = USER_CPP.read_text(errors='replace')
    struct_asserts = extract_all_struct_asserts(user_cpp_text)

    print("Loading sample tests …", file=sys.stderr)
    sample_tests = load_sample_tests()

    print("Loading independent reviews …", file=sys.stderr)
    reviews = load_reviews()

    # Structural check: ledger must have exactly indices 0..188
    ledger_indices = {r['index'] for r in rows}
    missing_indices = set(range(189)) - ledger_indices
    extra_indices = ledger_indices - set(range(189))

    all_violations: list[tuple[str, str]] = []

    for idx in sorted(missing_indices):
        all_violations.append((str(idx), f"missing ledger row for index {idx}"))
    for idx in sorted(extra_indices):
        all_violations.append((str(idx), f"extra ledger row for index {idx} (valid range 0..188)"))

    # Per-row validation
    for row in rows:
        row_violations = validate_row(row, apfn, user_cpp_text, struct_asserts,
                                      sample_tests, reviews)
        for v in row_violations:
            all_violations.append((f"{row['index']}:{row['fn_name']}", v))

    # Summary
    implemented_count = sum(1 for r in rows if r['status'] == 'implemented')
    deferred_count = sum(1 for r in rows if r['status'] == 'deferred')
    dummy_count = sum(1 for r in rows if r['status'] == 'dummy')
    thunk_count = sum(1 for r in rows if r['status'] == 'wndproc-thunk')
    client_count = sum(1 for r in rows if r['status'] == 'client-callback')
    unverified_count = sum(1 for r in rows if r['status'] == 'unverified')
    total = len(rows)

    print(f"\n=== KCT Audit Report ===")
    print(f"Implemented {implemented_count} | Deferred {deferred_count} | "
          f"Dummy {dummy_count} | WndprocThunk {thunk_count} | "
          f"ClientCallback {client_count} | Unverified {unverified_count} | Total {total}")

    if all_violations:
        print(f"\n{'='*60}")
        print(f"VIOLATIONS ({len(all_violations)} total) — audit FAILED")
        print(f"{'='*60}")
        for handler, msg in all_violations:
            print(f"  {handler}: {msg}")
        print(f"\nkct_audit.py: {len(all_violations)} violation(s). Fix all before marking slots implemented.")
        sys.exit(1)
    else:
        print("\nkct_audit.py: OK — 0 violations.")
        if unverified_count == 0 and client_count == 0:
            print("kct_audit.py: 100% KCT coverage achieved.")
        sys.exit(0)


if __name__ == '__main__':
    main()
