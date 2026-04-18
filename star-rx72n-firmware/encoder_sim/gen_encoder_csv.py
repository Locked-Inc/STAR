#!/usr/bin/env python3
"""
gen_encoder_csv.py -- Generate Phase A and Phase B quadrature encoder CSVs
for AD2 playback on the STAR encoder simulation bench.

Produces two CSV files:
  encoder_sim_phase_a.csv -- A-channel state for all 4 encoders over time
  encoder_sim_phase_b.csv -- B-channel state for all 4 encoders over time

The two files together define full quadrature waveforms.  Feed them into
encoder_sim.py which replays them through AD2 DigitalOut to the TOM PCB.

Quadrature state machine (forward rotation):
  step: 0 -> 1 -> 2 -> 3 -> 0 -> ...
     A: 0    1    1    0
     B: 0    0    1    1

Reverse rotation is the mirror: 0 -> 3 -> 2 -> 1 -> 0 -> ...

Hardware constants:
  PPR = 341   (encoder pulses per revolution)
  4x counting: each A/B edge = 1 count => 4 state transitions per pulse => 1364 counts/rev
  Max RPM = 210

Usage:
  python3 gen_encoder_csv.py                         # default: 210 RPM all motors
  python3 gen_encoder_csv.py --profile phase_b       # motor 2 degrades at t=5s
  python3 gen_encoder_csv.py --rpm 100 150 200 210   # custom per-motor RPM
  python3 gen_encoder_csv.py --duration 10000        # 10 second profile

SPDX-License-Identifier: MIT
Copyright (c) 2026 Locked Inc.
"""

import argparse
import csv
import math
import sys
from pathlib import Path

PPR        = 341
NUM_MOTORS = 4
MAX_RPM    = 210.0

# Quadrature state machine: index = step (0..3), value = (A, B)
_QUAD_FWD = [(0, 0), (1, 0), (1, 1), (0, 1)]
_QUAD_REV = [(0, 0), (0, 1), (1, 1), (1, 0)]


def rpm_to_period_ms(rpm: float) -> float:
    """Convert RPM to quadrature state-change period in milliseconds.

    At 341 PPR with 4x counting, one revolution = 4 * 341 = 1364 state
    transitions.  The time between transitions at a given RPM is:

      period = 60_000 ms/min / (RPM * 1364 transitions/rev)
    """
    if rpm <= 0.0:
        return float("inf")
    return 60_000.0 / (rpm * PPR * 4.0)


def build_profile_constant(rpm_list: list[float], duration_ms: float) -> dict:
    """Constant-RPM profile for all 4 motors.

    Returns {t_ms: [rpm0, rpm1, rpm2, rpm3]} with a single segment.
    """
    return {0.0: list(rpm_list), duration_ms: list(rpm_list)}


def build_profile_phase_b(base_rpm: float, duration_ms: float) -> dict:
    """Phase B test profile: motor 2 degrades to 60% at t=5000 ms.

    Simulates a worn/failing motor that slows down mid-run.  All healthy
    motors should converge to motor 2's degraded speed via the sync layer.
    """
    degraded = base_rpm * 0.6
    return {
        0.0:      [base_rpm, base_rpm, base_rpm,  base_rpm],
        5000.0:   [base_rpm, base_rpm, degraded,  base_rpm],
        duration_ms: [base_rpm, base_rpm, degraded, base_rpm],
    }


def _interpolate_rpms(profile: dict, t_ms: float) -> list[float]:
    """Linearly interpolate per-motor RPMs from the profile at time t_ms."""
    times  = sorted(profile.keys())
    if t_ms <= times[0]:
        return list(profile[times[0]])
    if t_ms >= times[-1]:
        return list(profile[times[-1]])
    for i in range(len(times) - 1):
        t0, t1 = times[i], times[i + 1]
        if t0 <= t_ms <= t1:
            alpha = (t_ms - t0) / (t1 - t0)
            r0 = profile[t0]
            r1 = profile[t1]
            return [r0[m] + alpha * (r1[m] - r0[m]) for m in range(NUM_MOTORS)]
    return list(profile[times[-1]])


def generate_csvs(
    profile:     dict,
    duration_ms: float,
    out_dir:     Path,
    step_us:     float = 50.0,
) -> tuple[Path, Path]:
    """Generate Phase A and Phase B CSV files from an RPM profile.

    Parameters
    ----------
    profile:     {t_ms: [rpm0..rpm3]} piecewise-linear RPM profile
    duration_ms: total waveform duration in milliseconds
    out_dir:     directory to write the two CSV files
    step_us:     time resolution in microseconds (default 50 us = 20 kHz)

    Returns
    -------
    (path_a, path_b) -- absolute paths to the two written files
    """
    step_ms   = step_us / 1000.0
    n_samples = int(math.ceil(duration_ms / step_ms)) + 1

    # Per-motor quadrature state and time-to-next-transition accumulators.
    # next_trans starts at the first period so step 0 is held for one full period.
    quad_step  = [0] * NUM_MOTORS
    init_rpms  = _interpolate_rpms(profile, 0.0)
    next_trans = [rpm_to_period_ms(abs(init_rpms[m])) for m in range(NUM_MOTORS)]

    path_a = out_dir / "encoder_sim_phase_a.csv"
    path_b = out_dir / "encoder_sim_phase_b.csv"

    with open(path_a, "w", newline="") as fa, open(path_b, "w", newline="") as fb:
        writer_a = csv.writer(fa)
        writer_b = csv.writer(fb)

        header = ["timestamp_ms"] + [f"enc{m}" for m in range(NUM_MOTORS)]
        writer_a.writerow(header)
        writer_b.writerow(header)

        a_state = [0] * NUM_MOTORS
        b_state = [0] * NUM_MOTORS

        for i in range(n_samples):
            t_ms = i * step_ms

            rpms = _interpolate_rpms(profile, t_ms)

            for m in range(NUM_MOTORS):
                rpm = rpms[m]
                if rpm <= 0.0:
                    continue

                period_ms = rpm_to_period_ms(abs(rpm))
                direction = _QUAD_FWD if rpm > 0.0 else _QUAD_REV

                while t_ms >= next_trans[m]:
                    quad_step[m] = (quad_step[m] + 1) % 4
                    a_state[m], b_state[m] = direction[quad_step[m]]
                    next_trans[m] += rpm_to_period_ms(abs(rpm))

            ts_str = f"{t_ms:.3f}"
            writer_a.writerow([ts_str] + a_state)
            writer_b.writerow([ts_str] + b_state)

    return path_a, path_b


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate Phase A/B quadrature encoder CSVs for AD2 playback"
    )
    parser.add_argument(
        "--profile",
        choices=["constant", "phase_b"],
        default="constant",
        help="RPM profile type (default: constant)",
    )
    parser.add_argument(
        "--rpm",
        type=float,
        nargs=4,
        default=[MAX_RPM] * NUM_MOTORS,
        metavar=("RPM0", "RPM1", "RPM2", "RPM3"),
        help=f"Per-motor RPM for constant profile (default: {MAX_RPM} all)",
    )
    parser.add_argument(
        "--base-rpm",
        type=float,
        default=MAX_RPM,
        help="Base RPM for phase_b profile (default: 210)",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=10_000.0,
        metavar="MS",
        help="Total waveform duration in milliseconds (default: 10000)",
    )
    parser.add_argument(
        "--step-us",
        type=float,
        default=50.0,
        metavar="US",
        help="Timestamp resolution in microseconds (default: 50)",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path(__file__).parent,
        help="Output directory for CSV files (default: same as this script)",
    )
    args = parser.parse_args()

    if args.profile == "constant":
        profile = build_profile_constant(args.rpm, args.duration)
    else:
        profile = build_profile_phase_b(args.base_rpm, args.duration)

    args.out_dir.mkdir(parents=True, exist_ok=True)

    print(f"Generating {args.profile!r} profile, {args.duration:.0f} ms, "
          f"step={args.step_us:.0f} us...")

    path_a, path_b = generate_csvs(
        profile=profile,
        duration_ms=args.duration,
        out_dir=args.out_dir,
        step_us=args.step_us,
    )

    n_samples = int(args.duration / (args.step_us / 1000.0)) + 1
    print(f"  Phase A: {path_a}  ({n_samples} rows)")
    print(f"  Phase B: {path_b}  ({n_samples} rows)")

    period_ms = rpm_to_period_ms(max(args.rpm) if args.profile == "constant" else args.base_rpm)
    print(f"  Min state-transition period @ {max(args.rpm):.0f} RPM: {period_ms:.3f} ms")
    print(f"  Step resolution: {args.step_us / 1000.0:.3f} ms  "
          f"({'OK' if args.step_us / 1000.0 <= period_ms / 2.0 else 'WARNING: too coarse'})")

    return 0


if __name__ == "__main__":
    sys.exit(main())
