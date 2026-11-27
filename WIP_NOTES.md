# wip/uncommitted-integration-2026-11-27

Snapshot of the uncommitted working-tree state found on `integration/fex-perf-plus-d3d9-umd` (base
ecd42da39) on 2026-11-26. Nothing here was reviewed, probed or built as a unit; it is kept so the work is
not lost and so each theme can be reviewed and carved (or dropped) on its own.

Not a PR candidate as a whole. Native-UMD content: none. Contents by theme (from the diff):

- Steam-launched game support: `src/windows-emulator/steam_drm_ipc.{cpp,hpp}` (acknowledges the
  STEAM_DIPC_CONSUME / STEAM_START_ACK_EVENT handshake that Proton's steam_helper normally plays),
  `semaphore.cpp` hook, `windows_emulator.hpp::get_application_settings()`, SteamExe lookup in `process.cpp`.
  Maintainer #1238 prefers guest-exposed drivers/steam bridge as a plugin: likely fork-only.
- Windows-host fidelity of file IDs and stat: `common/utils/stat.*` (compat_find_path_by_inode, macOS
  fsgetpath), `file_system.hpp::find_by_file_id`, `platform/file_management.hpp`, `kernel_mapped.hpp`,
  `status.hpp`, `handles.hpp`, `syscalls/file.cpp` (FILE_ATTRIBUTE_ARCHIVE on ordinary files, probed on
  Windows 11 26100).
- Registry hive big-data (`db`) values: `registry/hive_parser.cpp`.
- Memory/system information classes: `syscalls/memory.cpp` (MemoryWorkingSetExInformation,
  MemoryImageExtensionInformation), `syscalls/system.cpp` (class 0xC0 NLS init).
- Object security descriptors: `syscalls/object.cpp` (+190 lines: SID/ACL builders).
- Volume device name query for bare drives: `syscalls.cpp` (mountmgr MOUNTDEV_NAME).
- create-root.bat: `mui` directories; `event.cpp`: STATUS_OBJECT_NAME_NOT_FOUND.
- Docs carried along: `FEXCORE_PR_READY.md`, `FEXPERF_CANDIDATES_DIGEST.md` (old FEX/perf PR triage notes,
  superseded); `PR1234_SPLIT_READY.md` deleted here (still in history).

Left out on purpose: `v86.txt` (a 2778-line analyzer log of probe p155_x86), `.playwright-mcp/`, the deps/FEX
submodule dirty flag.

Follow-up: every theme goes through the carve queue (~/sogen-split/QUEUE_2026-11-27.md): probe against real
Windows where behaviour is involved, carve onto origin/main, Opus review, fork CI.
