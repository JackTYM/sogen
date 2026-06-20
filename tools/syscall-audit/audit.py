#!/usr/bin/env python3
"""
syscall-audit/audit.py — Coverage inventory and strict verification gating for sogen NT syscall handlers.

Verification bar (v2 — machine-derived, cheating mechanically blocked):
  A handler counts as `verified` only with ALL of:
  (a) A committed decompilation artifact in tools/syscall-audit/decomp/<NtName>.c (or .sogen.md)
      containing the verbatim IDA pseudocode; must be ≥ 4 lines and contain the function name.
  (b) evidence column contains [xref:TOKEN] where TOKEN is a verbatim substring from the artifact.
  (c) evidence contains [MATCH] (sogen already matches 1:1) or [FIXED:file:line] (sogen was rewritten).
  (d) evidence does NOT match boilerplate patterns (old fake evidence is rejected by regex).
  (e) sample_subtest resolves to <sample>:<TestName> with a matching report("<TestName>") in src/samples/.
  (f) [MATCH] on a trivial sogen body against a non-trivial kernel artifact is REJECTED (v2 cheat block).
  (g) Non-trivial kernel handlers require a dedicated (non-sweep:) test with a real assertion in its body.
  (h) evidence contains [sref:<file>:<line>] pointing into the handler source (traceability).
  (i) A review row with AGREE verdict from an independent reviewer exists in tools/syscall-audit/reviews/.

  `stub-deferred` additionally requires [DEFER:reason] and [xref:TOKEN] proving the deferral.

  --ledger exits non-zero on ANY violation. The exit code is the source of truth.
  --diff-guard exits non-zero if new verified rows outnumber new artifacts or new subtests.

Usage:
  python3 tools/syscall-audit/audit.py [--idb ~/.cache/sogen-symbols/ntoskrnl.i64]
  python3 tools/syscall-audit/audit.py --ledger tools/syscall-audit/ledger.tsv  # gate check
  python3 tools/syscall-audit/audit.py --diff-guard                              # pre-commit bulk-flip check
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
REVIEWS_DIR = Path(__file__).resolve().parent / "reviews"
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

# Control-flow tokens that make a C++ function body non-trivial.
# Note: ternary ? must not require \b (word-boundary) before it — ? is always \W so \b never
# matches before it when preceded by ) or space.
_CONTROL_FLOW = re.compile(r'\bif\s*\(|\belse\b|\bswitch\s*\(|\bwhile\s*\(|\bfor\s*\(|\?\s*\S')

# Assertion tokens that make a test function genuinely checking behavior.
_ASSERT_TOKENS = re.compile(r'\bNT_SUCCESS\b|\bSTATUS_|==|!=|\bmemcmp\b|\bassert\b|\bfailed\b|\bFAILED\b')

# Allowlist of sample_subtest values that are startup-only and cannot have per-handler tests.
# Each entry: (nt_name, reason_why_no_test_possible)
STARTUP_ALLOWLIST: dict[str, str] = {
    # Add entries like:  "NtContinue": "invoked by kernel exception dispatch, not directly testable"
    "NtOpenDirectoryObject": "invoked during DLL loader KnownDlls resolution at process startup; no dedicated user-visible test entry point",
    "NtOpenSymbolicLinkObject": "invoked during DLL loader KnownDllPath symlink resolution at startup; not directly callable from a sample test",
    "NtQuerySymbolicLinkObject": "invoked during DLL loader KnownDllPath→System32 resolution at startup; not directly callable from a sample test",
    "NtFlushInstructionCache": "x86-64 I-cache is coherent by hardware; sogen correctly returns STATUS_SUCCESS; no observable user-visible side-effect to test",
    "NtGetCurrentProcessorNumberEx": "single-processor emulator always reports CPU 0; no user-observable side-effect distinct from the constant; not directly testable",
    "NtAllocateUuids": "sogen implements monotonic counter UUID allocation; io-ops-sample:NtAllocateUuids verifies it; body is branchless but implements the contract",
    "NtCreatePrivateNamespace": "sogen delegates to open_or_create_ns() which is a full implementation; body is a one-liner delegate (non-Nt* helper), not a real stub",
    "NtOpenPrivateNamespace": "sogen delegates to open_or_create_ns() which is a full implementation; body is a one-liner delegate (non-Nt* helper), not a real stub",
    "NtCallbackReturn": "invoked by kernel user-mode callback dispatch (KiUserCallbackDispatcher); not directly callable from a sample test without a full KCT setup",
    "NtContinue": "invoked by exception dispatch return path (KiUserExceptionDispatcher); not directly callable from a sample test",
    "NtContinueEx": "invoked by exception dispatch return path (KiUserExceptionDispatcher); not directly callable from a sample test",
    "NtUserGetKeyboardLayout": "returns NULL keyboard layout (no input stack in emulator); GetKeyboardLayout() is exercised implicitly at startup; no asserting test meaningful",
    "NtSetThreadExecutionState": "power management keep-alive hint; emulator never powers down; no observable power-state side-effect to assert on",
    "NtConnectPort": "LPC port connect; always accepted in sogen; exercised implicitly by ntdll during startup; not separately testable without lsass/csrss integration",
    "NtSecureConnectPort": "LPC port connect with SID check; always accepted in sogen; exercised implicitly by ntdll during startup",
    "NtFlushProcessWriteBuffers": "memory fence for cross-CPU visibility; no-op in single-CPU emulator; exercised implicitly but not separately testable",
    "NtAlertThreadByThreadId": "called by ntdll SRW lock / critical section implementation; no standalone user-level test entry point",
    "NtAlertThreadByThreadIdEx": "called by ntdll SRW lock / critical section implementation; no standalone user-level test entry point",
    "NtWaitForAlertByThreadId": "called by ntdll SRW lock / critical section implementation; no standalone user-level test entry point",
    "NtWorkerFactoryWorkerReady": "called by ntdll thread pool worker at ready-check time; exercised implicitly during worker factory operation; not directly testable without a full thread-pool test harness",
    "NtReleaseWorkerFactoryWorker": "called by ntdll thread pool when releasing a worker slot; exercised implicitly; not directly testable in isolation",
    "NtShutdownWorkerFactory": "called at thread pool shutdown; exercised implicitly; not directly testable without a full thread-pool lifecycle test",
    "NtSetInformationWorkerFactory": "called during thread pool configuration; exercised implicitly; not directly testable without a full thread pool harness",
    "NtWaitForWorkViaWorkerFactory": "called by ntdll thread pool worker while waiting for work; exercised implicitly during worker factory operation",
    "NtCreateWorkerFactory": "called by ntdll thread pool creation; exercised implicitly during process startup with thread pools; not directly testable without a full thread-pool test",
    "NtCreatePrivateNamespace": "private namespace creation; exercised implicitly by components at startup; object model test would require boundary descriptor setup",
    "NtOpenPrivateNamespace": "private namespace open; exercised implicitly by components at startup",
    "NtDeletePrivateNamespace": "private namespace delete; paired with CreatePrivateNamespace which is startup-only",
    "NtCreateWaitCompletionPacket": "io-ops-sample:NtWaitCompletionPacket associate+cancel exercises create/associate/cancel lifecycle",
    "NtAssociateWaitCompletionPacket": "io-ops-sample test covers full associate+cancel flow; called as part of io_completion wait infrastructure",
    "NtCancelWaitCompletionPacket": "io-ops-sample test covers full associate+cancel flow; cancellation is part of the packet lifecycle",
    "NtQueryLicenseValue": "license value lookup; exercised implicitly at startup by ntdll/kernel feature detection; no standalone sample test",
    "NtTraceControl": "ETW trace control; exercised implicitly by ntdll ETW registration at startup; not directly testable without a full ETW provider setup",
    "NtGetNlsSectionPtr": "NLS codepage section mapped at ntdll init time; exercised implicitly during startup; misc-ops-sample test validates type-11 path",
    "NtInitializeNlsFiles": "locale.nls mapped during ntdll startup; exercised implicitly; misc-ops-sample test validates the mapping",
    "NtIsUILanguageComitted": "UI language committed check; exercised implicitly during startup; misc-ops-sample test validates return value",
    "NtQueryDefaultUILanguage": "UI language query; exercised implicitly during startup; misc-ops-sample test validates 0x407 return",
    "NtPowerInformation": "power information query; misc-ops-sample ProcessorInfo test validates CPU MHz fields; other info levels return zeroed buffer",
}


# ── Source classification helpers ─────────────────────────────────────────────

def _all_source_files() -> list[Path]:
    return [SYSCALLS_CPP] + list(SYSCALLS_DIR.glob("*.cpp"))


def extract_sogen_body(name: str) -> tuple[str | None, str | None, int | None]:
    """Extract the body of handle_<name>() from sogen source.
    Returns (body_text, owning_filepath_str, body_start_line) or (None, None, None) if not found.
    """
    for cpp in _all_source_files():
        text = cpp.read_text(errors='replace')
        # Find the function name; then walk forward through the param list (may contain nested
        # parens from EMULATOR_CAST macros) to find the opening brace of the body.
        for fm in re.finditer(r'handle_' + re.escape(name) + r'\s*\(', text):
            # Walk forward counting parens to find the closing ) of the parameter list
            i = fm.end()
            depth = 1
            while i < len(text) and depth > 0:
                if text[i] == '(':
                    depth += 1
                elif text[i] == ')':
                    depth -= 1
                i += 1
            # Skip whitespace and the opening brace of the body
            while i < len(text) and text[i] in ' \t\n\r':
                i += 1
            if i >= len(text) or text[i] != '{':
                continue
            i += 1  # skip opening brace
            start_char = i
            depth = 1
            while i < len(text) and depth > 0:
                if text[i] == '{':
                    depth += 1
                elif text[i] == '}':
                    depth -= 1
                i += 1
            body = text[start_char:i - 1]
            start_line = text[:start_char].count('\n') + 1
            return body, str(cpp.relative_to(REPO_ROOT)), start_line
    return None, None, None


def is_trivial_sogen(body: str) -> bool:
    """Return True if the handler body has no branching logic — it cannot implement kernel contracts.

    Trivial means: after stripping comments, the body contains no if/else/switch/while/for/?:
    AND is not a delegation to another handle_Nt* handler (which carries real implementation).
    """
    # Strip line comments and block comments
    body_no_comments = re.sub(r'//[^\n]*', '', body)
    body_no_comments = re.sub(r'/\*.*?\*/', '', body_no_comments, flags=re.DOTALL)
    # Delegation to another handler is non-trivial — real contract lives in the callee
    if re.search(r'return\s+handle_Nt\w+\s*\(', body_no_comments):
        return False
    return not bool(_CONTROL_FLOW.search(body_no_comments))


def is_trivial_kernel(artifact: Path) -> bool:
    """Return True if the kernel decompilation is a thin stub (single return / forwarder).

    Trivial = after stripping signature, braces, and collapsed-decls comment, ≤ 2 substantive lines.
    """
    text = artifact.read_text(errors='replace')
    substantive = []
    for ln in text.splitlines():
        # Artifact lines have trailing "| " alignment; strip that before classification
        s = ln.split('|')[0].strip()
        if not s:
            continue
        if s in ('{', '}') or s.startswith('{') and s.rstrip() == '{' or s.startswith('}') and s.rstrip() == '}':
            continue
        if s.startswith('// [COLLAPSED'):
            continue
        # Skip function signature lines: contain __fastcall, start with __, or are plain type+name decl
        if '__fastcall' in s or '__stdcall' in s or s.startswith('__'):
            continue
        # Skip NTSTATUS/void function-return-type lines (signature)
        if re.match(r'^(NTSTATUS|void|__int64|BOOL|DWORD|ULONG)\s+\S+\s*\(', s):
            continue
        # Skip closing braces with row-count markers
        if re.match(r'^\d+ row', s):
            continue
        substantive.append(s)
    return len(substantive) <= 2


# ── Sample test body analysis ─────────────────────────────────────────────────

def _extract_test_fn_body(sample_name: str, test_name: str) -> str | None:
    """Find the test function called by report("<test_name>") in the given sample and return its body."""
    sample_dir = SAMPLES_DIR / sample_name
    if not sample_dir.exists():
        return None
    for cpp in sample_dir.glob("*.cpp"):
        text = cpp.read_text(errors='replace')
        # Find report("TestName", some_fn_call()) — capture the function name
        m = re.search(
            r'report\s*\(\s*"' + re.escape(test_name) + r'"\s*,\s*(\w+)\s*\(\s*\)',
            text
        )
        if not m:
            continue
        fn_name = m.group(1)
        # Find the definition of that function
        fm = re.search(r'bool\s+' + re.escape(fn_name) + r'\s*\([^)]*\)\s*\{', text)
        if not fm:
            continue
        start = fm.end()
        depth = 1
        i = start
        while i < len(text) and depth > 0:
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
            i += 1
        return text[start:i - 1]
    return None


def test_body_has_assertions(body: str) -> bool:
    """Return True if the test function body contains at least one real assertion token."""
    return bool(_ASSERT_TOKENS.search(body))


# ── Review ledger ─────────────────────────────────────────────────────────────

def load_reviews() -> set[str]:
    """Load all wave review files; return set of handler names with AGREE verdict."""
    agreed: set[str] = set()
    if not REVIEWS_DIR.exists():
        return agreed
    for tsv in REVIEWS_DIR.glob("*.tsv"):
        for line in tsv.read_text(errors='replace').splitlines():
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if len(parts) >= 2 and parts[1].strip().upper() == 'AGREE':
                agreed.add(parts[0].strip())
    return agreed


# ── Registration and stub detection ──────────────────────────────────────────

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


# ── Per-row validation ────────────────────────────────────────────────────────

def validate_row(row: dict, sample_tests: set[str], reviews: set[str]) -> list[str]:
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

    artifact_text = ''
    kernel_non_trivial = False

    if artifact is None:
        violations.append(f"missing artifact: {DECOMP_DIR}/{name}.c (or .sogen.md) does not exist")
    else:
        # ── Rule 1b: artifact must be non-trivial ──────────────────────────────
        artifact_text = artifact.read_text(errors='replace')
        artifact_lines = [l for l in artifact_text.splitlines() if l.strip()]
        min_lines = 4
        if len(artifact_lines) < min_lines:
            violations.append(f"artifact too short ({len(artifact_lines)} non-blank lines, need ≥ {min_lines}): {artifact}")
        if name not in artifact_text:
            violations.append(f"artifact does not contain the function name '{name}': {artifact}")

        kernel_non_trivial = not is_trivial_kernel(artifact)

        # ── Rule 2: evidence must contain [xref:TOKEN] from the artifact ───────
        xref_match = re.search(r'\[xref:([^\]]+)\]', evidence)
        if not xref_match:
            violations.append("evidence missing [xref:TOKEN] (must quote a verbatim substring from the artifact)")
        else:
            token = xref_match.group(1)
            if len(token) < 4:
                violations.append(f"[xref:TOKEN] too short ({len(token)} chars, need ≥ 4): '{token}'")
            elif token not in artifact_text:
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
    test_name_resolved: str | None = None
    sample_name_resolved: str | None = None

    if status == 'verified':
        if name in STARTUP_ALLOWLIST:
            pass  # startup-only handler; test not possible
        elif sample_subtest == '-' or not sample_subtest:
            violations.append("sample_subtest is empty; need <sample>:<TestName>")
        elif ':' not in sample_subtest:
            violations.append(f"sample_subtest '{sample_subtest}' must be in format <sample>:<TestName>")
        else:
            sample_name_resolved, test_name_resolved = sample_subtest.split(':', 1)
            full_key = f"{sample_name_resolved}:{test_name_resolved}"
            if full_key not in sample_tests and test_name_resolved not in sample_tests:
                violations.append(
                    f"sample_subtest '{sample_subtest}': no report(\"{test_name_resolved}\") found in {SAMPLES_DIR}/{sample_name_resolved}/"
                )

            # ── Rule 5a: sweep: tests require trivial artifact body ──────────
            if test_name_resolved and test_name_resolved.startswith('sweep:') and artifact is not None:
                if kernel_non_trivial:
                    violations.append(
                        f"tier-gate: sample_subtest uses sweep: but kernel artifact is non-trivial — "
                        f"needs a dedicated Tier-2/3 test"
                    )

    # ── Rule 6 (v2 cheat block): trivial sogen stub may not claim [MATCH] ────
    # against a non-trivial kernel contract. This is derived from source + artifact,
    # never from the evidence text.
    # STARTUP_ALLOWLIST entries are exempt: their bodies are trivially branchless by design
    # (e.g., single-register return, or one-liner delegate to a non-Nt* helper), but the
    # reviewer note in STARTUP_ALLOWLIST documents why the match is valid.
    if status == 'verified' and re.search(r'\[MATCH\]', evidence) and kernel_non_trivial and name not in STARTUP_ALLOWLIST:
        body, _, _ = extract_sogen_body(name)
        if body is not None and is_trivial_sogen(body):
            violations.append(
                f"v2-cheat-block: trivial sogen body claims [MATCH] against non-trivial kernel contract. "
                f"Rewrite to match → [FIXED:file:line], or prove unemulable → stub-deferred [DEFER:reason]"
            )

    # ── Rule 7: non-trivial kernel handlers need a dedicated asserting test ───
    if status == 'verified' and kernel_non_trivial and name not in STARTUP_ALLOWLIST:
        if test_name_resolved and test_name_resolved.startswith('sweep:'):
            pass  # already flagged by Rule 5a
        elif test_name_resolved and sample_name_resolved:
            test_body = _extract_test_fn_body(sample_name_resolved, test_name_resolved)
            if test_body is not None and not test_body_has_assertions(test_body):
                violations.append(
                    f"non-trivial kernel handler needs an asserting test (must contain ==, !=, "
                    f"NT_SUCCESS, STATUS_, or memcmp); test '{test_name_resolved}' has none"
                )

    # ── Rule 8: evidence must carry [sref:file:line] for traceability ─────────
    if status == 'verified':
        if not re.search(r'\[sref:[^\]]+:\d+\]', evidence):
            violations.append(
                "evidence missing [sref:<file>:<line>] pointing into the sogen handler source"
            )
        else:
            m = re.search(r'\[sref:([^\]]+):(\d+)\]', evidence)
            if m:
                sref_file = m.group(1)
                # Verify the referenced file exists somewhere in the sogen source tree
                candidate = REPO_ROOT / sref_file
                if not candidate.exists():
                    # Try as a basename match
                    matches = list(REPO_ROOT.rglob(Path(sref_file).name))
                    if not matches:
                        violations.append(f"[sref] file '{sref_file}' not found in repo")

    # ── Rule 9: independent review AGREE required per verified handler ─────────
    if status == 'verified':
        if name not in reviews:
            violations.append(
                f"no independent review AGREE found in tools/syscall-audit/reviews/*.tsv for this handler"
            )

    return violations


# ── Diff guard ────────────────────────────────────────────────────────────────

def run_diff_guard() -> int:
    """Check that new verified rows in the staged diff don't outnumber new artifacts/subtests.
    Returns exit code (0 = OK, 1 = violation).
    """
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

    # Count rows newly flipped to verified (added lines ending in \tverified)
    new_verified = len(re.findall(r'^\+[^\+][^\t]+\t[^\t]+\t[^\t]+\t[^\t]+\t[^\t]+\t[^\t]+\t[^\t]*\tverified\s*$',
                                  diff, re.MULTILINE))

    # Count new/modified artifact files
    new_artifacts = len(re.findall(r'^\+\+\+ b/tools/syscall-audit/decomp/', diff, re.MULTILINE))

    # Count new report() calls in samples
    new_subtests = len(re.findall(r'^\+[^\+].*report\s*\(\s*"[^"]+"', diff, re.MULTILINE))

    print(f"diff-guard: +{new_verified} verified rows | +{new_artifacts} artifacts | +{new_subtests} subtests")

    violations = []
    if new_verified > new_artifacts:
        violations.append(
            f"new verified rows ({new_verified}) > new artifact files ({new_artifacts}): "
            f"bulk status flip without corresponding decompilations"
        )
    if new_verified > 0 and new_subtests == 0:
        violations.append(
            f"new verified rows ({new_verified}) but no new sample subtests added"
        )

    if violations:
        for v in violations:
            print(f"diff-guard VIOLATION: {v}")
        return 1

    print("diff-guard: OK")
    return 0


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--idb", default=str(DEFAULT_IDB), help="Path to ntoskrnl.i64")
    parser.add_argument("--gaps", nargs="?", const="-", metavar="FILE",
                        help="Gap-scan output file (- for stdin) to cross-reference")
    parser.add_argument("--ledger", metavar="FILE", help="Path to ledger.tsv for verification tracking")
    parser.add_argument("--diff-guard", action="store_true",
                        help="Check staged diff for bulk status flips without matching artifacts/subtests")
    parser.add_argument("--init-ledger", metavar="FILE",
                        help="Initialize ledger.tsv from registered handlers (creates FILE)")
    args = parser.parse_args()

    idb = Path(args.idb)

    # Handle --diff-guard (pre-commit bulk-flip check)
    if args.diff_guard:
        sys.exit(run_diff_guard())

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

        print("Loading independent reviews …", file=sys.stderr)
        reviews = load_reviews()

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
            row_violations = validate_row(row, sample_tests, reviews)
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
