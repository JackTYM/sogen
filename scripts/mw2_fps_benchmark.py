#!/usr/bin/env python3
"""Controlled MW2 FPS benchmark comparing --vcpus counts under the HVF backend.

Fairness methodology: every run uses the identical --click-dialog-button rules and
--send-input script (the same scripted wall-clock waits regardless of vcpu count), so
the only thing that varies between configs is --vcpus. Because level-load duration is
CPU-throughput-bound (it varies with vcpu count) while in-game simulation time tracks
real wall-clock time once loaded, a single fixed wall-clock deadline (measured from
process launch) is used as the synchronization point: it is chosen with a large margin
past the slowest observed loading-transition end, so every config is guaranteed to be
well into steady, settled in-mission rendering by the time the deadline is reached. FPS
is then measured only in a fixed-length window starting at that deadline, with no
further scripted input during the window (a static scripted scene, not the player
moving through arbitrary content).

Usage: python3 scripts/mw2_fps_benchmark.py --vcpus 1 2 4 --repeats 5
"""

import argparse
import json
import os
import re
import selectors
import signal
import subprocess
import sys
import time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ARTIFACTS = os.path.join(REPO_ROOT, "build", "release", "artifacts")
EMULATION_ROOT = os.path.join(ARTIFACTS, "root")

SEND_INPUT_SCRIPT = "wait:70000; key:enter; wait:10000; key:enter; wait:8000; key:enter"

FPS_LINE_RE = re.compile(
    r"GPU present: ([\d.]+) FPS \((\d+) frames / ([\d.]+)s\) \| "
    r"frame time p50=([\d.]+)ms p95=([\d.]+)ms p99=([\d.]+)ms"
)


def launch(vcpus: int, env_overrides: dict[str, str], emulation_root: str, hvf: bool) -> subprocess.Popen:
    env = os.environ.copy()
    env["EMULATOR_FEX"] = "1"
    if hvf:
        env["EMULATOR_FEX_HVF"] = "1"
    env["EMULATOR_FPS_COUNTER"] = "1"
    env.update(env_overrides)

    cmd = [
        "./analyzer",
        "-e", emulation_root,
        "-r", os.path.join(emulation_root, "registry"),
        "--vcpus", str(vcpus),
        "--click-dialog-button", "Safe Mode", "6",
        "--click-dialog-button", "Optimal Settings", "6",
        "--send-input", SEND_INPUT_SCRIPT,
        "-s", "c:/mw2/iw4sp.exe",
    ]
    return subprocess.Popen(cmd, cwd=ARTIFACTS, env=env, stdout=subprocess.DEVNULL,
                             stderr=subprocess.PIPE, text=True, start_new_session=True)


def run_once(vcpus: int, deadline_s: float, window_s: float, buffer_s: float,
             log_path: str, env_overrides: dict[str, str], emulation_root: str, hvf: bool) -> dict:
    total_runtime = deadline_s + window_s + buffer_s
    proc = launch(vcpus, env_overrides, emulation_root, hvf)
    start = time.monotonic()

    sel = selectors.DefaultSelector()
    sel.register(proc.stderr, selectors.EVENT_READ)

    lines = []
    end_time = start + total_runtime
    crashed_at = None
    returncode = None

    with open(log_path, "w") as logf:
        while True:
            now = time.monotonic()
            if now >= end_time:
                break
            if proc.poll() is not None:
                returncode = proc.returncode
                crashed_at = now - start
                break
            for key, _ in sel.select(timeout=1.0):
                line = key.fileobj.readline()
                if not line:
                    continue
                elapsed = time.monotonic() - start
                logf.write(f"[{elapsed:8.2f}s] {line}")
                logf.flush()
                lines.append((elapsed, line.rstrip()))

        if proc.poll() is None:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except ProcessLookupError:
                pass
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                pass

    reached_deadline = crashed_at is None or crashed_at >= (deadline_s + window_s)
    return {
        "vcpus": vcpus,
        "crashed": crashed_at is not None,
        "crashed_at_s": crashed_at,
        "returncode": returncode,
        "reached_deadline": reached_deadline,
        "lines": lines,
        "log_path": log_path,
    }


def analyze_window(lines, window_start: float, window_end: float, stall_threshold_s: float = 5.0) -> dict:
    total_frames = 0
    total_elapsed_s = 0.0
    instantaneous_fps = []
    n_samples = 0
    max_gap_s = 0.0
    for elapsed, line in lines:
        if not (window_start <= elapsed < window_end):
            continue
        m = FPS_LINE_RE.search(line)
        if not m:
            continue
        fps, frames, win_s, p50, p95, p99 = m.groups()
        total_frames += int(frames)
        total_elapsed_s += float(win_s)
        instantaneous_fps.append(float(fps))
        n_samples += 1
        max_gap_s = max(max_gap_s, float(win_s))

    weighted_fps = (total_frames / total_elapsed_s) if total_elapsed_s > 0 else 0.0
    return {
        "weighted_fps": weighted_fps,
        "stalled": max_gap_s >= stall_threshold_s,
        "max_gap_s": max_gap_s,
        "n_samples": n_samples,
        "total_frames": total_frames,
        "total_elapsed_s": total_elapsed_s,
        "min_instant_fps": min(instantaneous_fps) if instantaneous_fps else None,
        "max_instant_fps": max(instantaneous_fps) if instantaneous_fps else None,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vcpus", type=int, nargs="+", default=[1, 2])
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--max-attempts-per-repeat", type=int, default=3)
    parser.add_argument("--deadline-s", type=float, default=180.0)
    parser.add_argument("--window-s", type=float, default=60.0)
    parser.add_argument("--buffer-s", type=float, default=15.0)
    parser.add_argument("--root", default=EMULATION_ROOT)
    parser.add_argument("--no-hvf", action="store_true")
    parser.add_argument("--env", action="append", default=[], metavar="KEY=VALUE")
    parser.add_argument("--log-dir", default="/private/tmp/claude-501/-Users-jack-Documents-Coding-C---sogen/9696d14d-d600-4b90-a4b1-3ebb901f36cc/scratchpad/mw2_fps_logs")
    parser.add_argument("--results-json", default="/private/tmp/claude-501/-Users-jack-Documents-Coding-C---sogen/9696d14d-d600-4b90-a4b1-3ebb901f36cc/scratchpad/mw2_fps_results.json")
    args = parser.parse_args()

    os.makedirs(args.log_dir, exist_ok=True)
    env_overrides = dict(item.split("=", 1) for item in args.env)

    results = []
    if os.path.exists(args.results_json):
        with open(args.results_json) as f:
            results = json.load(f)

    for vcpus in args.vcpus:
        successes = sum(1 for r in results if r["vcpus"] == vcpus and r["success"])
        attempt_idx = sum(1 for r in results if r["vcpus"] == vcpus)
        while successes < args.repeats:
            attempts_this_repeat = 0
            while attempts_this_repeat < args.max_attempts_per_repeat:
                attempt_idx += 1
                attempts_this_repeat += 1
                log_path = os.path.join(args.log_dir, f"vcpus{vcpus}_attempt{attempt_idx}.log")
                print(f"=== vcpus={vcpus} attempt={attempt_idx} (successes so far: {successes}/{args.repeats}) ===",
                      file=sys.stderr, flush=True)
                run = run_once(vcpus, args.deadline_s, args.window_s, args.buffer_s, log_path,
                               env_overrides, args.root, not args.no_hvf)

                record = {
                    "vcpus": vcpus,
                    "attempt": attempt_idx,
                    "crashed": run["crashed"],
                    "crashed_at_s": run["crashed_at_s"],
                    "log_path": log_path,
                }

                if run["reached_deadline"] and not run["crashed"]:
                    window = analyze_window(run["lines"], args.deadline_s, args.deadline_s + args.window_s)
                    pre_window = analyze_window(run["lines"], max(0, args.deadline_s - 30), args.deadline_s)
                    record.update({
                        "weighted_fps": window["weighted_fps"],
                        "stalled": window["stalled"],
                        "max_gap_s": window["max_gap_s"],
                        "n_samples": window["n_samples"],
                        "total_frames": window["total_frames"],
                        "total_elapsed_s": window["total_elapsed_s"],
                        "min_instant_fps": window["min_instant_fps"],
                        "max_instant_fps": window["max_instant_fps"],
                        "pre_window_fps": pre_window["weighted_fps"],
                    })
                    if window["stalled"]:
                        record["success"] = False
                        print(f"  STALLED (multi-second freeze, max_gap={window['max_gap_s']:.1f}s) during "
                              f"measurement window -- discarding, not counted as a clean success: "
                              f"contaminated weighted FPS was {window['weighted_fps']:.2f}",
                              file=sys.stderr, flush=True)
                        results.append(record)
                    else:
                        record["success"] = True
                        print(f"  SUCCESS: weighted FPS={window['weighted_fps']:.2f} "
                              f"(n={window['n_samples']} samples, {window['total_frames']} frames / "
                              f"{window['total_elapsed_s']:.1f}s), pre-window FPS={pre_window['weighted_fps']:.2f}",
                              file=sys.stderr, flush=True)
                        successes += 1
                        results.append(record)
                        break
                else:
                    record["success"] = False
                    print(f"  FAILED: crashed_at={run['crashed_at_s']}", file=sys.stderr, flush=True)
                    results.append(record)

            else:
                print(f"  Giving up on this repeat for vcpus={vcpus} after "
                      f"{args.max_attempts_per_repeat} failed attempts", file=sys.stderr, flush=True)
                break

            with open(args.results_json, "w") as f:
                json.dump(results, f, indent=2)

    with open(args.results_json, "w") as f:
        json.dump(results, f, indent=2)

    print("\n=== SUMMARY ===")
    for vcpus in args.vcpus:
        succ = [r for r in results if r["vcpus"] == vcpus and r["success"]]
        fail = [r for r in results if r["vcpus"] == vcpus and not r["success"]]
        if succ:
            fps_values = [r["weighted_fps"] for r in succ]
            print(f"vcpus={vcpus}: {len(succ)} successful / {len(succ)+len(fail)} attempted runs; "
                  f"FPS values = {[f'{v:.1f}' for v in fps_values]}; "
                  f"mean = {sum(fps_values)/len(fps_values):.2f}")
        else:
            print(f"vcpus={vcpus}: 0 successful runs out of {len(fail)} attempts")


if __name__ == "__main__":
    main()
