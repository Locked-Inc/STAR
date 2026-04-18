#!/usr/bin/env python3
"""
convert_scope_capture.py -- Convert real Keysight DSOX1204G quadrature
captures (dual-scope burst mode) into the Phase A / Phase B CSV format
used by encoder_sim.py.

Input:  capstone/measurements/03_encoder_3min_dual_scope/
          scope_A/burst_NNNN.csv  (columns: Time (s), FL_A, FL_B, FR_A, FR_B)
          scope_B/burst_NNNN.csv  (columns: Time (s), BL_A, BL_B, BR_A, BR_B)
        Each burst: 500,000 samples, 100 ms window, ~0.2 us step, 0-3.3V logic.

Output: encoder_sim_phase_a_real.csv  (timestamp_ms, enc0..enc3 = *_A channels)
        encoder_sim_phase_b_real.csv  (timestamp_ms, enc0..enc3 = *_B channels)

Motor/wheel mapping (matches STAR firmware):
  enc0 = Motor 0 = FL  (Front-Left)   - scope_A CH1 (A), CH2 (B)
  enc1 = Motor 1 = FR  (Front-Right)  - scope_A CH3 (A), CH4 (B)
  enc2 = Motor 2 = BL  (Rear-Left)    - scope_B CH1 (A), CH2 (B)
  enc3 = Motor 3 = BR  (Rear-Right)   - scope_B CH3 (A), CH4 (B)

Processing:
  1. For each of the 15 bursts:
     a. Read scope_A and scope_B CSVs (aligned in time; cross-scope
        start-skew measured at +0.000 ms).
     b. Downsample from 0.2 us to 50 us step (factor 250).
     c. Binarize voltages: LOW if V < threshold, HIGH otherwise.
        Default threshold = 1.65 V (midpoint of 0-3.3 V logic).
  2. Concatenate bursts end-to-end (inter-burst gaps discarded).
     Each burst contributes 100 ms of usable data => 15 * 100 ms = 1500 ms total.
  3. Write Phase A CSV (all *_A columns) and Phase B CSV (all *_B columns).

Usage:
  python3 convert_scope_capture.py                              # default paths
  python3 convert_scope_capture.py --step-us 20                 # higher resolution
  python3 convert_scope_capture.py --threshold 1.5              # different V threshold
  python3 convert_scope_capture.py --bursts 5                   # first 5 bursts only
  python3 convert_scope_capture.py --single-burst 7             # just burst 7

SPDX-License-Identifier: MIT
Copyright (c) 2026 Locked Inc.
"""

import argparse
import csv
import sys
from pathlib import Path

#
# NOTE: the source scope captures (1.2 GB across 15 bursts) are not stored in
# this repo.  The pre-converted encoder_sim_phase_a_real.csv /
# encoder_sim_phase_b_real.csv files in this directory are the output of
# running this script once against those captures.  If you just want to play
# real data through the AD2, use encoder_sim.py directly -- you do not need
# to re-run this converter.
#
DEFAULT_CAPTURE_DIR = Path("./03_encoder_3min_dual_scope")

NUM_MOTORS = 4

# (scope, a_col_idx, b_col_idx) per motor index.
# Column indices are into the CSV row (index 0 = Time, 1..4 = channel voltages).
MOTOR_SOURCE = [
    ("scope_A", 1, 2),  # enc0 FL -> FL_A, FL_B
    ("scope_A", 3, 4),  # enc1 FR -> FR_A, FR_B
    ("scope_B", 1, 2),  # enc2 BL -> BL_A, BL_B
    ("scope_B", 3, 4),  # enc3 BR -> BR_A, BR_B
]


def load_burst(burst_dir: Path, burst_idx: int) -> tuple[list[float], list[list[float]]]:
    """Load one burst's scope CSVs and return time + 8-channel voltage matrix.

    Returns
    -------
    t_ms : list of float  (relative to burst start, in milliseconds)
    v    : list of 8 lists (one per channel, in MOTOR_SOURCE order:
           [FL_A, FL_B, FR_A, FR_B, BL_A, BL_B, BR_A, BR_B])
    """
    paths = {
        "scope_A": burst_dir / "scope_A" / f"burst_{burst_idx:04d}.csv",
        "scope_B": burst_dir / "scope_B" / f"burst_{burst_idx:04d}.csv",
    }
    for p in paths.values():
        if not p.exists():
            raise FileNotFoundError(f"Missing burst CSV: {p}")

    # Read time column from scope A (both scopes are barrier-synced; skew <1 ms).
    t_ms: list[float] = []
    rows_a: list[list[float]] = []
    with open(paths["scope_A"]) as f:
        reader = csv.reader(f)
        next(reader)  # header
        for row in reader:
            t_ms.append(float(row[0]) * 1000.0)
            rows_a.append([float(row[1]), float(row[2]),
                           float(row[3]), float(row[4])])

    rows_b: list[list[float]] = []
    with open(paths["scope_B"]) as f:
        reader = csv.reader(f)
        next(reader)
        for row in reader:
            rows_b.append([float(row[1]), float(row[2]),
                           float(row[3]), float(row[4])])

    if len(rows_a) != len(rows_b):
        raise ValueError(
            f"Burst {burst_idx}: scope_A has {len(rows_a)} rows, "
            f"scope_B has {len(rows_b)}"
        )

    # Normalize: t_ms[0] -> 0
    t0 = t_ms[0]
    t_ms = [t - t0 for t in t_ms]

    # Interleave into 8-channel vector ordered as
    # [FL_A, FL_B, FR_A, FR_B, BL_A, BL_B, BR_A, BR_B]
    v: list[list[float]] = [[] for _ in range(8)]
    for i in range(len(rows_a)):
        v[0].append(rows_a[i][0])
        v[1].append(rows_a[i][1])
        v[2].append(rows_a[i][2])
        v[3].append(rows_a[i][3])
        v[4].append(rows_b[i][0])
        v[5].append(rows_b[i][1])
        v[6].append(rows_b[i][2])
        v[7].append(rows_b[i][3])

    return t_ms, v


def downsample_and_binarize(
    t_ms:       list[float],
    v_channels: list[list[float]],
    step_ms:    float,
    threshold:  float,
) -> tuple[list[float], list[list[int]]]:
    """Downsample to uniform step_ms grid and binarize against threshold."""
    if len(t_ms) < 2:
        return [], [[] for _ in v_channels]

    src_step_ms = t_ms[1] - t_ms[0]
    stride      = max(1, int(round(step_ms / src_step_ms)))

    t_ds: list[float] = []
    v_ds: list[list[int]] = [[] for _ in v_channels]

    for i in range(0, len(t_ms), stride):
        t_ds.append(t_ms[i])
        for c, v in enumerate(v_channels):
            v_ds[c].append(1 if v[i] >= threshold else 0)

    return t_ds, v_ds


def process_bursts(
    capture_dir: Path,
    burst_indices: list[int],
    step_ms:   float,
    threshold: float,
) -> tuple[list[float], list[list[int]]]:
    """Load, downsample, and concatenate the listed bursts end-to-end."""
    all_t: list[float] = []
    all_v: list[list[int]] = [[] for _ in range(8)]
    offset_ms = 0.0

    for bi in burst_indices:
        print(f"  burst {bi:02d}...", end="", flush=True)
        t_ms, v = load_burst(capture_dir, bi)
        t_ds, v_ds = downsample_and_binarize(t_ms, v, step_ms, threshold)

        for i, t in enumerate(t_ds):
            all_t.append(t + offset_ms)
        for c in range(8):
            all_v[c].extend(v_ds[c])

        if t_ds:
            offset_ms = all_t[-1] + step_ms

        print(f" {len(t_ds)} samples ({t_ds[-1]:.1f} ms burst duration)")

    return all_t, all_v


def write_phase_csvs(
    t_ms:       list[float],
    v_channels: list[list[int]],
    out_dir:    Path,
    suffix:     str,
) -> tuple[Path, Path]:
    """Write Phase A (indices 0,2,4,6) and Phase B (1,3,5,7) CSVs."""
    path_a = out_dir / f"encoder_sim_phase_a{suffix}.csv"
    path_b = out_dir / f"encoder_sim_phase_b{suffix}.csv"

    a_idx = [0, 2, 4, 6]  # FL_A, FR_A, BL_A, BR_A
    b_idx = [1, 3, 5, 7]  # FL_B, FR_B, BL_B, BR_B

    header = ["timestamp_ms", "enc0", "enc1", "enc2", "enc3"]

    with open(path_a, "w", newline="") as fa, open(path_b, "w", newline="") as fb:
        wa = csv.writer(fa)
        wb = csv.writer(fb)
        wa.writerow(header)
        wb.writerow(header)

        for i, t in enumerate(t_ms):
            ts = f"{t:.3f}"
            wa.writerow([ts] + [v_channels[ci][i] for ci in a_idx])
            wb.writerow([ts] + [v_channels[ci][i] for ci in b_idx])

    return path_a, path_b


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert Keysight scope captures to encoder_sim CSV format"
    )
    parser.add_argument(
        "--capture-dir", type=Path, default=DEFAULT_CAPTURE_DIR,
        help=f"Path to 03_encoder_3min_dual_scope directory (default: {DEFAULT_CAPTURE_DIR})",
    )
    parser.add_argument(
        "--step-us", type=float, default=50.0,
        help="Output timestamp resolution in microseconds (default: 50)",
    )
    parser.add_argument(
        "--threshold", type=float, default=1.65,
        help="Binarization threshold in volts (default: 1.65, midpoint of 0..3.3 V logic)",
    )
    parser.add_argument(
        "--bursts", type=int, default=15, metavar="N",
        help="Use first N bursts (default: 15 = all)",
    )
    parser.add_argument(
        "--single-burst", type=int, default=None, metavar="IDX",
        help="Process only burst IDX (overrides --bursts)",
    )
    parser.add_argument(
        "--out-dir", type=Path, default=Path(__file__).parent,
        help="Output directory (default: same as this script)",
    )
    parser.add_argument(
        "--suffix", default="_real",
        help="Suffix for output filenames (default: _real)",
    )
    args = parser.parse_args()

    if not args.capture_dir.exists():
        print(f"ERROR: capture dir not found: {args.capture_dir}", file=sys.stderr)
        return 1

    if args.single_burst is not None:
        burst_indices = [args.single_burst]
    else:
        burst_indices = list(range(args.bursts))

    step_ms = args.step_us / 1000.0

    print(f"Converting {len(burst_indices)} burst(s) from {args.capture_dir}")
    print(f"  step = {args.step_us:.0f} us, threshold = {args.threshold:.2f} V")

    all_t, all_v = process_bursts(
        args.capture_dir, burst_indices, step_ms, args.threshold
    )

    if not all_t:
        print("ERROR: no samples produced", file=sys.stderr)
        return 1

    print(f"\nTotal samples: {len(all_t)}  ({all_t[-1]:.1f} ms timeline)")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    path_a, path_b = write_phase_csvs(all_t, all_v, args.out_dir, args.suffix)

    # Per-channel edge count sanity check
    def count_edges(bits: list[int]) -> int:
        return sum(1 for i in range(1, len(bits)) if bits[i] != bits[i-1])

    print("\nEdge counts per channel (rising + falling):")
    names = ["FL_A", "FL_B", "FR_A", "FR_B", "BL_A", "BL_B", "BR_A", "BR_B"]
    for c, n in enumerate(names):
        print(f"  {n:5s} : {count_edges(all_v[c])}")

    print(f"\nOutputs:")
    print(f"  Phase A: {path_a}")
    print(f"  Phase B: {path_b}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
