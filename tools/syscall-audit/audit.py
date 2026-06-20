#!/usr/bin/env python3
"""
syscall-audit/audit.py — Coverage inventory and strict verification gating for sogen NT syscall handlers.

Verification bar (per the approved plan):
  A handler counts as `verified` only with ALL of:
  (a) A committed decompilation artifact in tools/syscall-audit/decomp/<NtName>.c (or .sogen.md)
      containing the verbatim IDA pseudocode; must be ≥ 8 lines and contain the function name.
  (b) evidence column contains [xref:TOKEN] where TOKEN is a verbatim substring from the artifact.
  (c) evidence contains [MATCH] (sogen already matches 1:1) or [FIXED:file:line] (sogen was rewritten).
  (d) evidence does NOT match boilerplate patterns (old fake evidence is rejected by regex).
  (e) sample_subtest resolves to <sample>:<TestName> with a matching report("<TestName>") in src/samples/.

  `stub-deferred` additionally requires [DEFER:reason] and [xref:TOKEN] proving the deferral.

  --ledger exits non-zero on ANY violation. The exit code is the source of truth.

Usage:
  python3 tools/syscall-audit/audit.py [--idb ~/.cache/sogen-symbols/ntoskrnl.i64]
  python3 tools/syscall-audit/audit.py --ledger tools/syscall-audit/ledger.tsv  # gate check
  python3 tools/syscall-audit/audit.py --init-ledger tools/syscall-audit/ledger.tsv  # init fresh
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SYSCALLS_CPP = REPO_ROOT / "src/windows-emulator/syscalls.cpp"
SYSCALLS_DIR = REPO_ROOT / "src/windows-emulator/syscalls"
SAMPLES_DIR = REPO_ROOT / "src/samples"
DECOMP_DIR = Path(__file__).resolve().parent / "decomp"
IDASQL = Path.home() / "idasql/q"
DEFAULT_IDB = Path.home() / ".cache/sogen-symbols/ntoskrnl.i64"

# Boilerplate evidence patterns that indicate a fake/bulk verification — always rejected.
BOILERPLATE_PATTERNS = re.compile(
    r'Tested via'
    r'|: Verified$'
    r'|Kernel handler verified'
    r'|Kernel timer object verified'
    r'|^Verified$'
    r'|^IDB 0x[0-9A-Fa-f]+: Verified$'
    r'|^IDB: 0x[0-9A-Fa-f]+ — (Verified|Kernel handler verified|Decompiled, awaiting)'
    r'|Decompiled, awaiting contract verification'
    r'|IDB \w+: (Verified|[0-9A-Fa-f]+ — Verified)'
)

# Allowlist of sample_subtest values that are startup-only and cannot have per-handler tests.
# Each entry: (nt_name, reason_why_no_test_possible)
STARTUP_ALLOWLIST: dict[str, str] = {
    # Add entries like:  "NtContinue": "invoked by kernel exception dispatch, not directly testable"
}


def registered_handlers(syscalls_cpp: Path) -> dict[str, str]:
    """Extract names and owning files by finding handle_NtXxx implementations.
    Returns a dict mapping NtXxx -> owning_file."""
    handlers = {}

    all_cpp_files = [syscalls_cpp] + list(SYSCALLS_DIR.glob("*.cpp"))

    for cpp_file in all_cpp_files:
        text = cpp_file.read_text()
        for m in re.finditer(r'(?:static\s+)?NTSTATUS\s+handle_(Nt\w+)\s*\(', text):
            nt_name = m.group(1)
            handlers[nt_name] = cpp_file.name

    text = syscalls_cpp.read_text()
    for m in re.finditer(r'\badd_handler\(\s*(Nt\w+)\s*\)', text):
        nt_name = m.group(1)
        if nt_name not in handlers:
            found = False
            for cpp in SYSCALLS_DIR.glob("*.cpp"):
                if re.search(rf'(?:static\s+)?NTSTATUS\s+handle_{nt_name}\s*\(', cpp.read_text()):
                    handlers[nt_name] = cpp.name
                    found = True
                    break
            if not found:
                handlers[nt_name] = "syscalls.cpp"

    return handlers


def stub_handlers(syscalls_cpp: Path) -> set[str]:
    """Identify stubs by two heuristics."""
    stubs: set[str] = set()

    text = syscalls_cpp.read_text()
    for m in re.finditer(r'NTSTATUS\s+handle_(Nt\w+)\s*\(\s*\)', text):
        stubs.add(m.group(1))

    for cpp in [syscalls_cpp] + list(SYSCALLS_DIR.glob("*.cpp")):
        body_text = cpp.read_text()
        for fm in re.finditer(r'handle_(Nt\w+)\s*\([^)]*\)\s*\n?\s*\{', body_text):
            name = fm.group(1)
            start = fm.end()
            depth = 1
            i = start
            while i < len(body_text) and depth > 0:
                if body_text[i] == '{':
                    depth += 1
                elif body_text[i] == '}':
                    depth -= 1
                i += 1
            body = body_text[start:i - 1]
            body_stripped = re.sub(r'//[^\n]*', '', body).strip()
            if body_stripped in ('return STATUS_NOT_SUPPORTED;', 'return STATUS_NOT_SUPPORTED ;'):
                stubs.add(name)

    return stubs


def ntoskrnl_syscalls(idb: Path) -> list[str]:
    """Query ntoskrnl.i64 for all Nt* function names via idasql."""
    if not IDASQL.exists():
        print(f"warning: idasql not found at {IDASQL}, skipping kernel universe", file=sys.stderr)
        return []
    result = subprocess.run(
        [str(IDASQL), "-s", str(idb), "-q",
         "SELECT name FROM funcs WHERE name LIKE 'Nt%' ORDER BY name"],
        capture_output=True, text=True, timeout=120
    )
    names = []
    for line in result.stdout.splitlines():
        m = re.match(r'\|\s*(Nt\w+)\s*\|', line)
        if m:
            names.append(m.group(1))
    return names


def load_ledger(ledger_path: Path) -> dict[str, dict]:
    """Load the verification ledger; return dict mapping nt_name -> ledger row."""
    if not ledger_path.exists():
        return {}
    rows = {}
    with open(ledger_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if len(parts) >= 8:
                nt_name = parts[0]
                rows[nt_name] = {
                    'nt_name': parts[0],
                    'owning_file': parts[1],
                    'idb_module': parts[2],
                    'tier': parts[3],
                    'sample_subtest': parts[4],
                    'evidence': parts[5],
                    'decompile_addr': parts[6],
                    'status': parts[7],
                }
            elif len(parts) >= 7:
                nt_name = parts[0]
                rows[nt_name] = {
                    'nt_name': parts[0],
                    'owning_file': parts[1],
                    'idb_module': parts[2],
                    'tier': parts[3],
                    'sample_subtest': parts[4],
                    'evidence': parts[5],
                    'decompile_addr': '',
                    'status': parts[6],
                }
    return rows


def load_sample_tests() -> set[str]:
    """Return the set of all report("<TestName>") strings found in src/samples/**/*.cpp.
    Returns a set of "<sample-dir>:<TestName>" and also "<TestName>" (bare) for flexible matching.
    """
    tests: set[str] = set()
    if not SAMPLES_DIR.exists():
        return tests
    for cpp in SAMPLES_DIR.rglob("*.cpp"):
        sample_name = cpp.parent.name
        text = cpp.read_text(errors='replace')
        for m in re.finditer(r'report\s*\(\s*"([^"]+)"', text):
            test_name = m.group(1)
            tests.add(f"{sample_name}:{test_name}")
            tests.add(test_name)  # bare name for allowlist checks
    return tests


def validate_row(row: dict, sample_tests: set[str]) -> list[str]:
    """Validate a single ledger row according to the strict gating rules.
    Returns a list of violation strings (empty = OK).
    """
    name = row['nt_name']
    status = row['status']
    evidence = row.get('evidence', '')
    sample_subtest = row.get('sample_subtest', '-')

    violations = []

    if status == 'unverified':
        return []  # No requirements on unverified rows.

    # ── Rule 1: artifact must exist ────────────────────────────────────────────
    artifact_c = DECOMP_DIR / f"{name}.c"
    artifact_md = DECOMP_DIR / f"{name}.sogen.md"
    artifact = None
    if artifact_c.exists():
        artifact = artifact_c
    elif artifact_md.exists():
        artifact = artifact_md

    if artifact is None:
        violations.append(f"missing artifact: {DECOMP_DIR}/{name}.c (or .sogen.md) does not exist")
    else:
        # ── Rule 1b: artifact must be non-trivial ──────────────────────────────
        artifact_text = artifact.read_text(errors='replace')
        artifact_lines = [l for l in artifact_text.splitlines() if l.strip()]
        if len(artifact_lines) < 8:
            violations.append(f"artifact too short ({len(artifact_lines)} non-blank lines, need ≥ 8): {artifact}")
        if name not in artifact_text:
            violations.append(f"artifact does not contain the function name '{name}': {artifact}")

        # ── Rule 2: evidence must contain [xref:TOKEN] from the artifact ───────
        xref_match = re.search(r'\[xref:([^\]]+)\]', evidence)
        if not xref_match:
            violations.append("evidence missing [xref:TOKEN] (must quote a verbatim substring from the artifact)")
        else:
            token = xref_match.group(1)
            if len(token) < 4:
                violations.append(f"[xref:TOKEN] too short ({len(token)} chars, need ≥ 4): '{token}'")
            elif artifact and token not in artifact.read_text(errors='replace'):
                violations.append(f"[xref:{token!r}] not found verbatim in {artifact}")

    # ── Rule 3: evidence must have outcome tag ─────────────────────────────────
    if status == 'verified':
        if not re.search(r'\[MATCH\]|\[FIXED:[^\]]+\]', evidence):
            violations.append("evidence missing [MATCH] or [FIXED:file:line] outcome tag")
    elif status == 'stub-deferred':
        if not re.search(r'\[DEFER:[^\]]+\]', evidence):
            violations.append("evidence missing [DEFER:reason] tag for stub-deferred handler")

    # ── Rule 4: evidence must not be boilerplate ───────────────────────────────
    if BOILERPLATE_PATTERNS.search(evidence):
        violations.append(f"evidence matches boilerplate blocklist (bulk/fake evidence not accepted): {evidence!r}")

    # ── Rule 5: verified rows need a real sample sub-test ─────────────────────
    if status == 'verified':
        if name in STARTUP_ALLOWLIST:
            pass  # startup-only handler; test not possible
        elif sample_subtest == '-' or not sample_subtest:
            violations.append("sample_subtest is empty; need <sample>:<TestName>")
        elif ':' not in sample_subtest:
            violations.append(f"sample_subtest '{sample_subtest}' must be in format <sample>:<TestName>")
        else:
            sample_name, test_name = sample_subtest.split(':', 1)
            full_key = f"{sample_name}:{test_name}"
            if full_key not in sample_tests and test_name not in sample_tests:
                violations.append(
                    f"sample_subtest '{sample_subtest}': no report(\"{test_name}\") found in {SAMPLES_DIR}/{sample_name}/"
                )

    return violations


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--idb", default=str(DEFAULT_IDB), help="Path to ntoskrnl.i64")
    parser.add_argument("--gaps", nargs="?", const="-", metavar="FILE",
                        help="Gap-scan output file (- for stdin) to cross-reference")
    parser.add_argument("--ledger", metavar="FILE", help="Path to ledger.tsv for verification tracking")
    parser.add_argument("--init-ledger", metavar="FILE",
                        help="Initialize ledger.tsv from registered handlers (creates FILE)")
    args = parser.parse_args()

    idb = Path(args.idb)

    print("Reading registered handlers from syscalls.cpp …", file=sys.stderr)
    registered = registered_handlers(SYSCALLS_CPP)
    registered_set = set(registered.keys())

    print("Identifying stubs …", file=sys.stderr)
    stubs = stub_handlers(SYSCALLS_CPP)

    # Handle --init-ledger
    if args.init_ledger:
        ledger_path = Path(args.init_ledger)
        print(f"Initializing ledger at {ledger_path} …", file=sys.stderr)
        with open(ledger_path, 'w') as f:
            f.write("# nt_name\towning_file\tidb_module\ttier\tsample_subtest\tevidence\tdecompile_addr\tstatus\n")
            for nt_name in sorted(registered_set):
                owning_file = registered[nt_name]
                status = "unverified"
                tier = ""
                sample_subtest = "-"
                evidence = ""
                decompile_addr = ""
                if "user.cpp" in owning_file:
                    idb_module = "win32kbase/win32kfull"
                elif "gdi.cpp" in owning_file:
                    idb_module = "gdi32full/win32kbase"
                elif "port.cpp" in owning_file:
                    idb_module = "ntoskrnl/afd"
                else:
                    idb_module = "ntoskrnl"
                f.write(f"{nt_name}\t{owning_file}\t{idb_module}\t{tier}\t{sample_subtest}\t{evidence}\t{decompile_addr}\t{status}\n")
        print(f"Created {ledger_path} with {len(registered_set)} entries", file=sys.stderr)
        return

    # Handle --ledger (strict gate mode)
    if args.ledger:
        ledger_path = Path(args.ledger)
        ledger = load_ledger(ledger_path)

        print("Loading sample tests from src/samples/ …", file=sys.stderr)
        sample_tests = load_sample_tests()

        print(f"Querying ntoskrnl IDB at {idb} …", file=sys.stderr)

        # Structural checks: orphans and stale rows
        orphans = set(registered_set) - set(ledger.keys())
        stale = set(ledger.keys()) - set(registered_set)

        all_violations: list[tuple[str, str]] = []  # (handler_name, violation_message)

        if orphans:
            for name in sorted(orphans):
                all_violations.append((name, "orphan: registered in source but missing from ledger"))

        if stale:
            for name in sorted(stale):
                all_violations.append((name, "stale: in ledger but no longer registered in source"))

        # Per-row strict validation
        for name, row in sorted(ledger.items()):
            row_violations = validate_row(row, sample_tests)
            for v in row_violations:
                all_violations.append((name, v))

        # Summary counts
        verified_count = sum(1 for r in ledger.values() if r['status'] == 'verified')
        deferred_count = sum(1 for r in ledger.values() if r['status'] == 'stub-deferred')
        unverified_count = sum(1 for r in ledger.values() if r['status'] == 'unverified')
        total = len(ledger)

        print(f"\n=== Verification Report ===")
        print(f"Verified {verified_count}/{total} | Deferred {deferred_count} | Unverified {unverified_count}")

        if all_violations:
            print(f"\n{'='*60}")
            print(f"VIOLATIONS ({len(all_violations)} total) — audit FAILED")
            print(f"{'='*60}")
            for handler, msg in all_violations:
                print(f"  {handler}: {msg}")
            print(f"\naudit.py: {len(all_violations)} violation(s). Fix all before marking handlers verified.")
            sys.exit(1)
        else:
            print(f"\naudit.py: OK — 0 violations.")
            if unverified_count == 0:
                print(f"audit.py: 100% coverage achieved ({verified_count} verified + {deferred_count} deferred).")
            sys.exit(0)

    # Default mode: coverage inventory
    print(f"Querying ntoskrnl IDB at {idb} …", file=sys.stderr)
    kernel_names = ntoskrnl_syscalls(idb)
    kernel_set = set(kernel_names)

    live_gaps: set[str] = set()
    if args.gaps:
        src = sys.stdin if args.gaps == "-" else open(args.gaps)
        for line in src:
            m = re.search(r'Unimplemented syscall:\s*(Nt\w+)', line)
            if m:
                live_gaps.add(m.group(1))

    non_stub = sorted(n for n in registered if n not in stubs)
    stub_list = sorted(n for n in registered if n in stubs)
    missing = sorted(kernel_set - registered_set)

    print(f"\n=== Syscall Coverage Report ===")
    print(f"  ntoskrnl Nt* functions : {len(kernel_set)}")
    print(f"  Registered handlers    : {len(registered_set)}")
    print(f"    (a) Non-stub         : {len(non_stub)}")
    print(f"    (b) Stubs            : {len(stub_list)}")
    print(f"  (c) Missing entirely   : {len(missing)}")
    if live_gaps:
        live_missing = live_gaps - registered_set
        live_stub = live_gaps & set(stub_list)
        print(f"\n  Live gap signals (from run): {len(live_gaps)}")
        print(f"    → Missing handlers : {sorted(live_missing)}")
        print(f"    → Stub handlers    : {sorted(live_stub)}")

    print(f"\n--- (b) Stub handlers ({len(stub_list)}) ---")
    for n in stub_list:
        tag = " [LIVE GAP]" if n in live_gaps else ""
        print(f"  {n}{tag}")

    print(f"\n--- (c) Missing entirely ({len(missing)}) ---")
    for n in missing:
        tag = " [LIVE GAP]" if n in live_gaps else ""
        print(f"  {n}{tag}")

    print(f"\n--- (a) Non-stub handlers ({len(non_stub)}) ---")
    for n in non_stub:
        print(f"  {n}")


if __name__ == "__main__":
    main()
