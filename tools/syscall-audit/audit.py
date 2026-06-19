#!/usr/bin/env python3
"""
syscall-audit/audit.py — Coverage inventory for sogen NT syscall handlers.

Outputs three lists:
  (a) Registered and non-stub (properly implemented)
  (b) Registered but likely stub (zero-arg declaration, or only returns STATUS_NOT_SUPPORTED)
  (c) Declared in ntoskrnl but not registered at all

Usage:
  python3 tools/syscall-audit/audit.py [--idb ~/.cache/sogen-symbols/ntoskrnl.i64]

Optional: pipe a gap-scan run's output as stdin to cross-reference with live gaps:
  tools/gap-scan.sh C:\\my.exe | python3 tools/syscall-audit/audit.py --gaps -
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SYSCALLS_CPP = REPO_ROOT / "src/windows-emulator/syscalls.cpp"
SYSCALLS_DIR = REPO_ROOT / "src/windows-emulator/syscalls"
IDASQL = Path.home() / "idasql/q"
DEFAULT_IDB = Path.home() / ".cache/sogen-symbols/ntoskrnl.i64"


def registered_handlers(syscalls_cpp: Path) -> dict[str, str]:
    """Extract names and owning files by finding handle_NtXxx implementations.
    Returns a dict mapping NtXxx -> owning_file."""
    handlers = {}

    # Find all handle_NtXxx implementations in syscalls and its subdirectories
    all_cpp_files = [syscalls_cpp] + list(SYSCALLS_DIR.glob("*.cpp"))

    for cpp_file in all_cpp_files:
        text = cpp_file.read_text()
        # Match function definitions: NTSTATUS handle_NtXxx(...) or static NTSTATUS handle_NtXxx(...)
        for m in re.finditer(r'(?:static\s+)?NTSTATUS\s+handle_(Nt\w+)\s*\(', text):
            nt_name = m.group(1)
            handlers[nt_name] = cpp_file.name

    # Also check for any add_handler calls to catch handlers that might not be defined yet
    text = syscalls_cpp.read_text()
    for m in re.finditer(r'\badd_handler\(\s*(Nt\w+)\s*\)', text):
        nt_name = m.group(1)
        if nt_name not in handlers:
            # Check if defined in any specialized file
            found = False
            for cpp in SYSCALLS_DIR.glob("*.cpp"):
                if re.search(rf'(?:static\s+)?NTSTATUS\s+handle_{nt_name}\s*\(', cpp.read_text()):
                    handlers[nt_name] = cpp.name
                    found = True
                    break
            if not found:
                # Assume it's in syscalls.cpp if not found elsewhere
                handlers[nt_name] = "syscalls.cpp"

    return handlers


def stub_handlers(syscalls_cpp: Path) -> set[str]:
    """
    Identify stubs by two heuristics:
    1. Zero-argument declarations: NTSTATUS handle_NtXxx();
    2. Handlers whose only body line is 'return STATUS_NOT_SUPPORTED;'
       (check all *.cpp files in the syscalls directory).
    """
    stubs: set[str] = set()

    # Heuristic 1: zero-arg declarations in the umbrella file
    text = syscalls_cpp.read_text()
    for m in re.finditer(r'NTSTATUS\s+handle_(Nt\w+)\s*\(\s*\)', text):
        stubs.add(m.group(1))

    # Heuristic 2: function bodies that consist solely of STATUS_NOT_SUPPORTED
    # Pattern: handle_NtXxx(...) { ... return STATUS_NOT_SUPPORTED; ... }
    # We use a simplified heuristic: scan each cpp file for the pair
    # "handle_NtXxx" followed within ~10 lines by only STATUS_NOT_SUPPORTED.
    for cpp in [syscalls_cpp] + list(SYSCALLS_DIR.glob("*.cpp")):
        body_text = cpp.read_text()
        # Find function definition starts
        for fm in re.finditer(r'handle_(Nt\w+)\s*\([^)]*\)\s*\n?\s*\{', body_text):
            name = fm.group(1)
            start = fm.end()
            # Extract up to the matching closing brace (simple depth counter)
            depth = 1
            i = start
            while i < len(body_text) and depth > 0:
                if body_text[i] == '{':
                    depth += 1
                elif body_text[i] == '}':
                    depth -= 1
                i += 1
            body = body_text[start:i - 1]
            # Strip comments and whitespace
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
        # idasql table rows: "| NtXxx |"
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
            if len(parts) >= 7:
                nt_name = parts[0]
                rows[nt_name] = {
                    'nt_name': parts[0],
                    'owning_file': parts[1],
                    'idb_module': parts[2],
                    'tier': parts[3],
                    'sample_subtest': parts[4],
                    'evidence': parts[5],
                    'status': parts[6],
                }
    return rows


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
            f.write("# nt_name\towning_file\tidb_module\ttier\tsample_subtest\tevidence\tstatus\n")
            for nt_name in sorted(registered_set):
                owning_file = registered[nt_name]
                is_stub = nt_name in stubs
                status = "unverified"  # All start unverified
                tier = ""
                sample_subtest = "-"
                evidence = ""
                idb_module = ""  # Will be filled in during verification

                # Guess IDB module based on owning file
                if "user.cpp" in owning_file:
                    idb_module = "win32kbase/win32kfull"
                elif "gdi.cpp" in owning_file:
                    idb_module = "gdi32full/win32kbase"
                elif "port.cpp" in owning_file:
                    idb_module = "ntoskrnl/afd"
                else:
                    idb_module = "ntoskrnl"

                f.write(f"{nt_name}\t{owning_file}\t{idb_module}\t{tier}\t{sample_subtest}\t{evidence}\t{status}\n")
        print(f"Created {ledger_path} with {len(registered_set)} entries", file=sys.stderr)
        return

    print(f"Querying ntoskrnl IDB at {idb} …", file=sys.stderr)
    kernel_names = ntoskrnl_syscalls(idb)
    kernel_set = set(kernel_names)

    # Cross-reference with live gap signals if provided
    live_gaps: set[str] = set()
    if args.gaps:
        src = sys.stdin if args.gaps == "-" else open(args.gaps)
        for line in src:
            m = re.search(r'Unimplemented syscall:\s*(Nt\w+)', line)
            if m:
                live_gaps.add(m.group(1))

    # Load ledger if provided
    ledger = {}
    if args.ledger:
        ledger_path = Path(args.ledger)
        ledger = load_ledger(ledger_path)

        # Check for orphans and stale entries
        orphans = set(registered_set) - set(ledger.keys())
        stale = set(ledger.keys()) - set(registered_set)

        if orphans:
            print(f"ERROR: Orphan handlers (registered but not in ledger): {sorted(orphans)}", file=sys.stderr)
        if stale:
            print(f"ERROR: Stale ledger rows (no longer registered): {sorted(stale)}", file=sys.stderr)

        # Count verification status
        verified_count = sum(1 for r in ledger.values() if r['status'] == 'verified')
        deferred_count = sum(1 for r in ledger.values() if r['status'] == 'stub-deferred')
        unverified_count = sum(1 for r in ledger.values() if r['status'] == 'unverified')

        pct = (verified_count + deferred_count) * 100 // (verified_count + deferred_count + unverified_count) if (verified_count + deferred_count + unverified_count) > 0 else 0

        print(f"\n=== Verification Report ===")
        print(f"Verified {verified_count}/{len(ledger)} ({pct}%) | Deferred {deferred_count} | Unverified {unverified_count}")
        if orphans:
            print(f"Orphan handlers: {len(orphans)}")
        if stale:
            print(f"Stale rows: {len(stale)}")

        # Print unverified by file for work queue
        unverified_by_file = {}
        for row in ledger.values():
            if row['status'] == 'unverified':
                f = row['owning_file']
                if f not in unverified_by_file:
                    unverified_by_file[f] = []
                unverified_by_file[f].append(row['nt_name'])

        if unverified_by_file:
            print(f"\n--- Unverified handlers by file (work queue) ---")
            for fname in sorted(unverified_by_file.keys()):
                names = unverified_by_file[fname]
                print(f"  {fname} ({len(names)})")
                for name in sorted(names)[:5]:
                    print(f"    {name}")
                if len(names) > 5:
                    print(f"    ... and {len(names) - 5} more")

        return

    # Classify
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
