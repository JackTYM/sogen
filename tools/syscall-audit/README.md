# Syscall Audit — Ground-Truth Correctness Mirror

## Purpose

Track coverage and correctness of sogen's NT syscall handlers against the real Windows kernel.
Each handler is verified against the actual ntoskrnl/driver decompilation so fixes mirror the
kernel's exact contract rather than guessing from documentation.

## Tools

### `audit.py` — Coverage inventory and verification tracking

#### Coverage mode (default)

```bash
# From repo root
python3 tools/syscall-audit/audit.py

# Cross-reference with a live gap scan
cd build/release/artifacts
tools/gap-scan.sh C:\\my-binary.exe | python3 ../../tools/syscall-audit/audit.py --gaps -
```

Outputs three lists:
- **(a) Non-stub** — registered and substantively implemented
- **(b) Stubs** — registered but trivially stubbed (zero-arg or unconditional STATUS_NOT_SUPPORTED)
- **(c) Missing** — present in ntoskrnl but not registered at all

Re-generate each session so coverage grows over time.

#### Verification ledger mode

Initialize the verification ledger (tracks which handlers are verified vs unverified):

```bash
python3 tools/syscall-audit/audit.py --init-ledger tools/syscall-audit/ledger.tsv
```

Check verification progress:

```bash
python3 tools/syscall-audit/audit.py --ledger tools/syscall-audit/ledger.tsv
```

Outputs:
- Overall completion percentage (`verified / (verified + stub-deferred + unverified)`)
- List of orphan handlers (newly added, not in ledger) — **must be added manually**
- Work queue: unverified handlers grouped by owning file

### `ledger.tsv` — Verification ledger

One row per registered handler. Columns:
- `nt_name` — handler name (e.g., NtCreateFile)
- `owning_file` — source file (e.g., file.cpp, syscalls.cpp)
- `idb_module` — IDB module owning the real implementation (ntoskrnl, npfs, afd, etc.)
- `tier` — complexity tier (1=trivial, 2=struct/info-class, 3=stateful/driver)
- `sample_subtest` — sample exe that exercises this handler (e.g., `file-io-sample:NtCreateFile`)
- `evidence` — proof of verification (offsets matched, status codes confirmed, etc.)
- `status` ∈ `unverified` | `verified` | `stub-deferred`

When a handler is verified:
1. Update `status` to `verified`
2. Fill `evidence` with struct offsets/IOSB values matched
3. Fill `sample_subtest` with the sample that tests it
4. Fill `tier` with the complexity level

## Per-Handler Correction Procedure

Apply this procedure to each handler targeted for ground-truth correction:

### Step 1 — Decompile the real implementation

```bash
# ntoskrnl owns core Nt* calls
~/idasql/q -s ~/.cache/sogen-symbols/ntoskrnl.i64 \
  -q "SELECT line_num, line FROM pseudocode WHERE func_addr = (SELECT address FROM funcs WHERE name = 'NtCreateFile') ORDER BY line_num"

# Driver-owned handlers (pipes → npfs, sockets → afd, GPU → dxgkrnl)
~/idasql/q -s ~/.cache/sogen-symbols/npfs.i64 \
  -q "SELECT line_num, line FROM pseudocode WHERE func_addr = (SELECT address FROM funcs WHERE name = 'NpFsdCreate') ORDER BY line_num"
```

IDB ownership map:

| Handler family | IDB |
|---|---|
| Core `Nt*` (process, thread, memory, object, registry) | `ntoskrnl.i64` |
| Named pipes (`NtCreateNamedPipeFile`, `NtReadFile` on pipe) | `npfs.i64` |
| Sockets (`NtDeviceIoControlFile` on Afd handles) | `afd.i64` |
| Win32/GDI kernel side | `win32kbase.i64`, `win32kfull.i64` |
| GPU IOCTLs | `dxgkrnl.i64` |
| Win32 API wrappers | `kernelbase.i64` |

### Step 2 — Extract the kernel contract

From the decompilation, note:
- Parameter validation order and conditions
- Every `switch (info_class)` case: its required buffer length, output struct layout, STATUS_* return
- `IOSB.Information` value per outcome (FILE_CREATED=2, FILE_OPENED=1, bytes-transferred, etc.)
- Every STATUS_* return path (STATUS_BUFFER_TOO_SMALL, STATUS_INVALID_INFO_CLASS, …)

### Step 3 — Compare against sogen's handler

Find the handler in `src/windows-emulator/syscalls/*.cpp`. Compare struct field offsets, info-class
cases, and IOSB semantics against the ground truth.

### Step 4 — Fix and static-assert

Apply the correction. For any local mirror struct sogen defines, add:
```cpp
static_assert(offsetof(MyStruct, field) == 0xNN);
static_assert(sizeof(MyStruct) == 0xNN);
```
Match the existing `fn_*` / file-info patterns in the codebase.

### Step 5 — Verify

```bash
cd /path/to/sogen
cmake --build --preset=release
cd build/release/artifacts
./analyzer -e root -s "C:\\exercising-sample.exe" 2>&1 | grep -E "Unimplemented|Unsupported|Error"
```

The specific gap-ledger line must be gone. Add a regression test to
`src/windows-emulator-test/sample_test.cpp` when a sample newly passes.
Then: `cmake --build --preset=tidy` — must be clean for touched files.

## Work Queue Priority

Run `audit.py` to get the current queue. Work (b) stubs before (c) missing, in order of:
1. Syscalls that appear as `[LIVE GAP]` in a `gap-scan.sh` run on the target corpus
2. Syscalls in families already partially implemented (sweep neighboring info-classes)
3. High-frequency startup paths (process/thread/object/memory)

## Notes

- Never commit debug probes. Track them in the `project-temp-debug-to-remove` memory entry.
- Guard high-frequency probes by `thread.id` to avoid pipe backpressure timeouts.
- For kernel module IDBs, query by function name — no image-base math needed.
- For user-mode DLLs (ntdll, user32, gdi32full, win32u), image base is `0x180000000`.
