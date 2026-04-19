#!/usr/bin/env python3
"""
encoder_sim.py -- Replay Phase A/B quadrature encoder CSVs through an
Analog Discovery 2 DigitalOut to simulate motor encoder signals on the TOM PCB.

Wiring (AD2 B, SN 210321A36AAE):
  DIO 0 -> P24 / MTCLKA  (Motor 0 Phase A, MTU1)
  DIO 1 -> P25 / MTCLKB  (Motor 0 Phase B, MTU1)
  DIO 2 -> PA1 / MTCLKC  (Motor 1 Phase A, MTU2)
  DIO 3 -> PC5 / MTCLKD  (Motor 1 Phase B, MTU2)
  DIO 4 -> PC2 / TCLKA   (Motor 2 Phase A, TPU1)
  DIO 5 -> PA3 / TCLKB   (Motor 2 Phase B, TPU1)
  DIO 6 -> PC0 / TCLKC   (Motor 3 Phase A, TPU2)
  DIO 7 -> PB3 / TCLKD   (Motor 3 Phase B, TPU2)
  GND   -> TOM PCB GND

AD2 DIO logic level: 3.3V -- directly compatible with RX72N 3.3V GPIO.

The AD2 Pattern Generator runs at CLOCK_HZ (default 1 MHz).  Each CSV row
is held for a number of ticks equal to the time delta to the next row.
Maximum waveform duration is limited by the AD2 sample buffer (~16 M samples).

Usage:
  python3 encoder_sim.py                             # replay from default CSVs, loop
  python3 encoder_sim.py --phase-a my_a.csv --phase-b my_b.csv
  python3 encoder_sim.py --serial 210321A36AAE       # target specific AD2
  python3 encoder_sim.py --no-loop                   # play once and exit
  python3 encoder_sim.py --clock-hz 500000           # 500 kHz pattern clock

Requirements:
  pydwf >= 1.1.19  (pip install pydwf)
  WaveForms runtime >= 3.24.3

SPDX-License-Identifier: MIT
Copyright (c) 2026 Locked Inc.
"""

import argparse
import csv
import ctypes
import sys
import time
from pathlib import Path

try:
    from pydwf import (
        DwfLibrary,
        DwfDigitalOutOutput,
        DwfDigitalOutType,
        DwfDigitalOutIdle,
        DwfState,
    )
    from pydwf.utilities import openDwfDevice
except ImportError:
    print("ERROR: pydwf not installed. Run: pip install pydwf", file=sys.stderr)
    sys.exit(1)

# AD2 B serial number (has DIO 0-15 all active, confirmed in ad2_wiring.md)
DEFAULT_SERIAL = "210321A36AAE"

# DIO channel assignments: (phase_a_channel, phase_b_channel) per motor
MOTOR_CHANNELS: list[tuple[int, int]] = [
    (0, 1),  # Motor 0: P24/MTCLKA, P25/MTCLKB
    (2, 3),  # Motor 1: PA1/MTCLKC, PC5/MTCLKD
    (4, 5),  # Motor 2: PC2/TCLKA,  PA3/TCLKB
    (6, 7),  # Motor 3: PC0/TCLKC,  PB3/TCLKD
]

NUM_MOTORS   = 4
NUM_CHANNELS = 8  # DIO 0..7


def _load_csv(path: Path) -> tuple[list[float], list[list[int]]]:
    """Load a Phase A or Phase B CSV.

    Returns
    -------
    timestamps : list of float  (milliseconds)
    states     : list of list[int]  (one list per row, NUM_MOTORS values)
    """
    timestamps: list[float] = []
    states:     list[list[int]] = []

    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            timestamps.append(float(row["timestamp_ms"]))
            states.append([int(row[f"enc{m}"]) for m in range(NUM_MOTORS)])

    if not timestamps:
        raise ValueError(f"CSV is empty: {path}")

    return timestamps, states


def _build_pattern(
    ts_a: list[float],
    st_a: list[list[int]],
    ts_b: list[float],
    st_b: list[list[int]],
    clock_hz: int,
) -> tuple[list[list[int]], int]:
    """Merge Phase A and Phase B samples into an 8-channel pattern array.

    The AD2 Pattern Generator requires a fixed-rate sample array.  We compute
    the least-common sample grid from both CSVs (they should have identical
    timestamps) and convert millisecond timestamps to clock ticks.

    Returns
    -------
    channel_bits : list of 8 lists, each a sequence of 0/1 values per tick
    n_ticks      : total number of ticks in the pattern
    """
    if len(ts_a) != len(ts_b):
        raise ValueError(
            f"Phase A has {len(ts_a)} rows but Phase B has {len(ts_b)} rows -- "
            "they must be generated together with gen_encoder_csv.py"
        )

    ticks_per_ms = clock_hz / 1000.0
    n_rows = len(ts_a)

    # Build list of (tick_start, tick_end, [ch0..ch7]) per CSV row
    segments: list[tuple[int, int, list[int]]] = []
    for i in range(n_rows):
        tick_start = int(round(ts_a[i] * ticks_per_ms))
        tick_end   = (
            int(round(ts_a[i + 1] * ticks_per_ms)) if i < n_rows - 1
            else tick_start + 1
        )
        if tick_end <= tick_start:
            tick_end = tick_start + 1

        ch = [0] * NUM_CHANNELS
        for m, (ca, cb) in enumerate(MOTOR_CHANNELS):
            ch[ca] = st_a[i][m]
            ch[cb] = st_b[i][m]

        segments.append((tick_start, tick_end, ch))

    total_ticks = segments[-1][1]

    channel_bits: list[list[int]] = [[] for _ in range(NUM_CHANNELS)]
    seg_idx = 0
    for tick in range(total_ticks):
        while seg_idx < len(segments) - 1 and tick >= segments[seg_idx + 1][0]:
            seg_idx += 1
        ch = segments[seg_idx][2]
        for c in range(NUM_CHANNELS):
            channel_bits[c].append(ch[c])

    return channel_bits, total_ticks


def _bits_to_bytes(bits: list[int]) -> bytes:
    """Pack a list of 0/1 values into bytes, LSB first (pydwf customSet format)."""
    n = len(bits)
    n_bytes = (n + 7) // 8
    out = bytearray(n_bytes)
    for i, b in enumerate(bits):
        if b:
            out[i // 8] |= 1 << (i % 8)
    return bytes(out)


def run(
    path_a:   Path,
    path_b:   Path,
    serial:   str,
    clock_hz: int,
    loop:     bool,
) -> None:
    print(f"Loading Phase A: {path_a}")
    ts_a, st_a = _load_csv(path_a)
    print(f"Loading Phase B: {path_b}")
    ts_b, st_b = _load_csv(path_b)

    duration_ms = ts_a[-1]
    print(f"Profile duration: {duration_ms:.1f} ms  ({len(ts_a)} rows)")

    print("Building pattern array...")
    channel_bits, n_ticks = _build_pattern(ts_a, st_a, ts_b, st_b, clock_hz)
    print(f"  Pattern: {n_ticks} ticks @ {clock_hz/1e3:.0f} kHz = "
          f"{n_ticks / clock_hz * 1000:.1f} ms")

    channel_bytes = [_bits_to_bytes(channel_bits[c]) for c in range(NUM_CHANNELS)]

    dwf = DwfLibrary()
    print(f"Opening AD2 (SN {serial!r})...")
    with openDwfDevice(dwf, serial_number_filter=serial) as dev:
        dout = dev.digitalOut

        for ch in range(NUM_CHANNELS):
            dout.enableSet(ch, True)
            dout.outputSet(ch, DwfDigitalOutOutput.PushPull)
            dout.typeSet(ch, DwfDigitalOutType.Custom)
            dout.idleSet(ch, DwfDigitalOutIdle.Low)
            dout.customSet(ch, channel_bytes[ch])

        dout.runSet(n_ticks / clock_hz)
        dout.repeatSet(0 if loop else 1)
        dout.internalClockSet(clock_hz)

        print(f"Starting playback (loop={loop})...")
        dout.configure(True)

        if loop:
            print("Running continuously. Press Ctrl+C to stop.")
            try:
                while True:
                    time.sleep(0.5)
            except KeyboardInterrupt:
                print("\nStopping.")
        else:
            target = duration_ms / 1000.0 + 0.5
            deadline = time.monotonic() + target
            while time.monotonic() < deadline:
                state = dout.status()
                if state == DwfState.Done:
                    break
                time.sleep(0.05)
            print("Playback complete.")

        dout.configure(False)


def main() -> int:
    default_dir = Path(__file__).parent

    parser = argparse.ArgumentParser(
        description="Replay encoder CSVs through AD2 DigitalOut to TOM PCB"
    )
    parser.add_argument(
        "--phase-a",
        type=Path,
        default=default_dir / "encoder_sim_phase_a.csv",
        help="Path to Phase A CSV (default: encoder_sim_phase_a.csv)",
    )
    parser.add_argument(
        "--phase-b",
        type=Path,
        default=default_dir / "encoder_sim_phase_b.csv",
        help="Path to Phase B CSV (default: encoder_sim_phase_b.csv)",
    )
    parser.add_argument(
        "--serial",
        default=DEFAULT_SERIAL,
        help=f"AD2 serial number (default: {DEFAULT_SERIAL})",
    )
    parser.add_argument(
        "--clock-hz",
        type=int,
        default=1_000_000,
        help="Pattern generator clock frequency in Hz (default: 1000000)",
    )
    parser.add_argument(
        "--no-loop",
        action="store_true",
        help="Play the profile once then exit (default: loop continuously)",
    )
    args = parser.parse_args()

    if not args.phase_a.exists():
        print(f"ERROR: Phase A CSV not found: {args.phase_a}", file=sys.stderr)
        print("Run gen_encoder_csv.py first.", file=sys.stderr)
        return 1
    if not args.phase_b.exists():
        print(f"ERROR: Phase B CSV not found: {args.phase_b}", file=sys.stderr)
        print("Run gen_encoder_csv.py first.", file=sys.stderr)
        return 1

    run(
        path_a=args.phase_a,
        path_b=args.phase_b,
        serial=args.serial,
        clock_hz=args.clock_hz,
        loop=not args.no_loop,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
