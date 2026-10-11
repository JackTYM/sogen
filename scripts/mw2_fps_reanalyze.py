#!/usr/bin/env python3
"""Re-analyze already-captured MW2 FPS benchmark logs with a different measurement
window, and report full-log stall/freeze events (gaps between consecutive GPU present
prints exceeding a threshold), without re-running the game.
"""

import argparse
import glob
import os
import re

FPS_LINE_RE = re.compile(
    r"\[\s*([\d.]+)s\]\s*GPU present: ([\d.]+) FPS \((\d+) frames / ([\d.]+)s\)"
)


def load_log(path):
    entries = []
    with open(path) as f:
        for line in f:
            m = FPS_LINE_RE.search(line)
            if m:
                elapsed, fps, frames, win_s = m.groups()
                entries.append((float(elapsed), float(fps), int(frames), float(win_s)))
    return entries


def analyze_window(entries, window_start, window_end):
    total_frames = 0
    total_elapsed_s = 0.0
    fps_values = []
    for elapsed, fps, frames, win_s in entries:
        if not (window_start <= elapsed < window_end):
            continue
        total_frames += frames
        total_elapsed_s += win_s
        fps_values.append(fps)
    weighted = (total_frames / total_elapsed_s) if total_elapsed_s > 0 else None
    return weighted, len(fps_values), total_frames, total_elapsed_s


def find_stalls(entries, threshold_s):
    return [(elapsed, win_s) for elapsed, fps, frames, win_s in entries if win_s >= threshold_s]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log-dir", default="/private/tmp/claude-501/-Users-jack-Documents-Coding-C---sogen/9696d14d-d600-4b90-a4b1-3ebb901f36cc/scratchpad/mw2_fps_logs")
    parser.add_argument("--window-start", type=float, default=120.0)
    parser.add_argument("--window-end", type=float, default=170.0)
    parser.add_argument("--stall-threshold-s", type=float, default=5.0)
    args = parser.parse_args()

    for path in sorted(glob.glob(os.path.join(args.log_dir, "*.log"))):
        name = os.path.basename(path)
        entries = load_log(path)
        if not entries:
            print(f"{name}: no GPU present lines found")
            continue
        weighted, n, frames, elapsed_s = analyze_window(entries, args.window_start, args.window_end)
        stalls = find_stalls(entries, args.stall_threshold_s)
        last_elapsed = entries[-1][0]
        stall_str = f"STALLS at {[f'{e:.1f}s(+{w:.1f}s)' for e, w in stalls]}" if stalls else "no stalls"
        if weighted is not None:
            print(f"{name}: window[{args.window_start:.0f},{args.window_end:.0f}) FPS={weighted:6.2f} "
                  f"(n={n}, {frames}f/{elapsed_s:.1f}s) | last_sample_at={last_elapsed:.1f}s | {stall_str}")
        else:
            print(f"{name}: no samples in window[{args.window_start:.0f},{args.window_end:.0f}) "
                  f"| last_sample_at={last_elapsed:.1f}s | {stall_str}")


if __name__ == "__main__":
    main()
