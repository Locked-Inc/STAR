#!/usr/bin/env python3
"""
ad2_verify.py -- capture Pi5 GPIO sweep on 2x Analog Discovery 2 and
identify broken pins.

Pairs with pi5_toggle.py running on the Pi5. The Pi5 drives GPIO 0..27
sequentially (10 ms HIGH, 10 ms LOW each, 2 s quiet gap between cycles).
This script samples both AD2s at 5 kHz for 2 full cycles, finds the
quiet gap as a time anchor, then assigns each DIO channel's first
rising edge after the anchor to a Pi5 GPIO index.

Output:
  * table of AD2 (unit, DIO) -> Pi5 GPIO number (auto-discovered)
  * list of Pi5 GPIOs that were NOT detected anywhere (likely broken
    pin OR unwired DIO slot)
  * list of DIO channels that were silent (unwired, expected for the
    last 4 slots on the 12-channel AD2)

Usage (on the Mac with both AD2s plugged in and Pi5 sweeping):
    python3 ad2_verify.py
    python3 ad2_verify.py --cycles 3 --sample-rate 10000 --verbose
"""

import argparse
import sys
import time
from dataclasses import dataclass

try:
    from pydwf import (DwfLibrary, DwfDigitalInClockSource,
                       DwfAcquisitionMode)
    from pydwf.utilities import openDwfDevice
except ImportError:
    print("ERROR: pydwf not installed. Use the venv at "
          "star-rx72n-firmware/gpio_test/host/venv or `pip install pydwf`.",
          file=sys.stderr)
    sys.exit(1)

# Two AD2 units. Order determines "unit index" in the output.
AD2_SERIAL_NUMBERS = [
    "210321A2AE49",  # unit 0: user said 16 channels wired (DIO 0..15)
    "210321A36AAE",  # unit 1: user said 12 channels wired (DIO 0..11)
]
CHANNELS_PER_AD2 = 16

# Must match pi5_toggle.py: pins claimed by HAT EEPROM, I2C-1 (Hailo),
# SPI0 CS0/CS1. These are never driven by the toggler so the verifier
# must not expect edges on them.
SKIP_PINS = frozenset({0, 1, 2, 3, 7, 8})
SWEEP_ORDER = [g for g in range(28) if g not in SKIP_PINS]
NUM_SWEEP = len(SWEEP_ORDER)

PIN_HIGH_MS = 10
PIN_LOW_MS = 10
PIN_PERIOD_MS = PIN_HIGH_MS + PIN_LOW_MS
CYCLE_GAP_MS = 2000
CYCLE_DURATION_MS = NUM_SWEEP * PIN_PERIOD_MS + CYCLE_GAP_MS


@dataclass
class EdgeEvent:
    t_ms: float
    unit: int
    channel: int
    rising: bool


def open_devices(dwf: DwfLibrary) -> list:
    devs = []
    for i, sn in enumerate(AD2_SERIAL_NUMBERS):
        try:
            d = openDwfDevice(dwf, serial_number_filter=sn)
            devs.append(d)
            print(f"  unit {i} (SN {sn}): opened")
        except Exception as e:
            for d in devs:
                d.close()
            print(f"  unit {i} (SN {sn}): FAILED -- {e}", file=sys.stderr)
            sys.exit(1)
    return devs


def configure(dev, sample_rate_hz: int) -> None:
    dio = dev.digitalIn
    dio.clockSourceSet(DwfDigitalInClockSource.Internal)
    dio.dividerSet(int(100e6 / sample_rate_hz))
    dio.bufferSizeSet(4096)
    dio.acquisitionModeSet(DwfAcquisitionMode.Record)
    dio.sampleFormatSet(16)


def capture(devs: list, sample_rate_hz: int,
            duration_ms: float) -> list[list[int]]:
    samples: list[list[int]] = [[] for _ in devs]
    for d in devs:
        d.digitalIn.configure(False, True)

    total = int(sample_rate_hz * duration_ms / 1000)
    print(f"  capturing ~{total} samples over {duration_ms:.0f} ms ...")

    deadline = time.monotonic() + (duration_ms / 1000.0) + 0.5
    while time.monotonic() < deadline:
        for i, d in enumerate(devs):
            d.digitalIn.status(True)
            avail, _lost, _corrupt = d.digitalIn.statusRecord()
            if avail > 0:
                data = d.digitalIn.statusData(avail, sample_format=16)
                samples[i].extend(int(s) for s in data)
        time.sleep(0.001)

    for d in devs:
        d.digitalIn.configure(False, False)

    for i, s in enumerate(samples):
        print(f"  unit {i}: captured {len(s)} samples")
    return samples


def detect_edges(samples: list[int], unit: int,
                 sample_rate_hz: int) -> list[EdgeEvent]:
    edges: list[EdgeEvent] = []
    if len(samples) < 2:
        return edges
    period_ms = 1000.0 / sample_rate_hz
    prev = samples[0]
    for idx in range(1, len(samples)):
        curr = samples[idx]
        diff = prev ^ curr
        if diff:
            t = idx * period_ms
            for ch in range(CHANNELS_PER_AD2):
                bit = 1 << ch
                if diff & bit:
                    edges.append(EdgeEvent(t, unit, ch, bool(curr & bit)))
        prev = curr
    return edges


def per_channel_stats(edges: list[EdgeEvent]) -> dict:
    stats: dict[tuple[int, int], dict] = {}
    for e in edges:
        key = (e.unit, e.channel)
        s = stats.setdefault(key, {"rising": [], "falling": []})
        (s["rising"] if e.rising else s["falling"]).append(e.t_ms)
    return stats


def classify(stats: dict, num_cycles: int) -> dict:
    out = {}
    noise_threshold = max(num_cycles * 4, 20)
    for key, s in stats.items():
        n = len(s["rising"])
        if n == 0:
            verdict = "silent"
        elif n > noise_threshold:
            verdict = "noise"
        else:
            verdict = "signal"
        out[key] = {"verdict": verdict, "n_rising": n}
    return out


def auto_map(edges: list[EdgeEvent],
             num_cycles: int) -> tuple[dict[tuple[int, int], int], dict]:
    stats = per_channel_stats(edges)
    cls = classify(stats, num_cycles)

    sig = sum(1 for v in cls.values() if v["verdict"] == "signal")
    noi = sum(1 for v in cls.values() if v["verdict"] == "noise")
    sil = sum(1 for v in cls.values() if v["verdict"] == "silent")
    print(f"  channels: signal={sig} noise={noi} silent={sil}")

    if sig == 0:
        print("  no signal channels -- Pi5 toggler may not be running")
        return {}, cls

    # Find cycle gap by looking for the longest dead window across all
    # signal-channel edges (rising + falling pooled).
    all_t: list[float] = []
    for key, s in stats.items():
        if cls[key]["verdict"] == "signal":
            all_t.extend(s["rising"])
            all_t.extend(s["falling"])
    all_t.sort()
    if len(all_t) < 2:
        return {}, cls

    gaps = [(all_t[i + 1] - all_t[i], all_t[i + 1])
            for i in range(len(all_t) - 1)]
    gap_dur, t_after_gap = max(gaps, key=lambda g: g[0])

    min_gap = max(CYCLE_GAP_MS * 0.5, PIN_PERIOD_MS * 3.0)
    if gap_dur < min_gap:
        print(f"  longest quiet window is {gap_dur:.0f} ms "
              f"(need >= {min_gap:.0f} ms) -- capture too short or "
              f"toggler not idle between cycles")
        return {}, cls

    # Measure pin period empirically if we captured 2+ cycles.
    cycle_boundaries = sorted([t for dur, t in gaps if dur >= min_gap])
    pin_period_ms = float(PIN_PERIOD_MS)
    if len(cycle_boundaries) >= 2:
        lens = [cycle_boundaries[i + 1] - cycle_boundaries[i]
                for i in range(len(cycle_boundaries) - 1)]
        cycle_len = sum(lens) / len(lens)
        sweep_len = cycle_len - gap_dur
        if sweep_len > 0:
            pin_period_ms = sweep_len / NUM_SWEEP

    # t_after_gap is the first edge after the quiet window, i.e. the
    # rising edge of SWEEP_ORDER[0] (or whichever wired GPIO in the
    # sweep is earliest).
    t_anchor = t_after_gap
    print(f"  anchor t = {t_anchor:.1f} ms, pin_period = "
          f"{pin_period_ms:.2f} ms, gap = {gap_dur:.0f} ms")

    mapping: dict[tuple[int, int], int] = {}
    cycle_end = t_anchor + NUM_SWEEP * pin_period_ms
    for key, s in stats.items():
        if cls[key]["verdict"] != "signal":
            continue
        after = [t for t in s["rising"]
                 if t_anchor - pin_period_ms < t < cycle_end]
        if not after:
            continue
        t_first = min(after)
        sweep_idx = round((t_first - t_anchor) / pin_period_ms)
        if 0 <= sweep_idx < NUM_SWEEP:
            mapping[key] = SWEEP_ORDER[sweep_idx]
    return mapping, cls


def print_report(mapping: dict[tuple[int, int], int],
                 cls: dict, verbose: bool) -> None:
    print("\n" + "=" * 72)
    print("Pi5 GPIO VERIFICATION REPORT")
    print("=" * 72)

    print("\n--- DIO -> GPIO mapping (auto-detected) ---")
    for key in sorted(mapping):
        unit, ch = key
        print(f"  unit {unit} DIO{ch:2d}  ->  GPIO{mapping[key]:2d}")

    detected_gpios = set(mapping.values())
    missing = [g for g in SWEEP_ORDER if g not in detected_gpios]

    if missing:
        print(f"\n--- NOT DETECTED ({len(missing)}) ---")
        for g in missing:
            print(f"  GPIO{g:2d}  (no DIO channel saw a valid edge)")
    else:
        print("\n--- NOT DETECTED (0) ---")
        print(f"  all {NUM_SWEEP} swept GPIOs showed valid rising edges")
    if SKIP_PINS:
        print(f"  (skipped by toggler: "
              f"{sorted(SKIP_PINS)} -- HAT/I2C/SPI claimed)")

    if verbose:
        print("\n--- per-channel detail ---")
        for key in sorted(cls):
            unit, ch = key
            v = cls[key]
            gpio = mapping.get(key, None)
            gpio_str = f"GPIO{gpio:2d}" if gpio is not None else "   -"
            print(f"  unit {unit} DIO{ch:2d}  rising={v['n_rising']:4d}  "
                  f"[{v['verdict']:6s}]  {gpio_str}")

    print("\n" + "=" * 72)
    print(f"GPIO detected: {len(detected_gpios)}/{NUM_SWEEP}   "
          f"missing: {len(missing)}")
    print("=" * 72)


def main() -> None:
    p = argparse.ArgumentParser(description="Pi5 GPIO sweep verifier (AD2)")
    p.add_argument("--sample-rate", type=int, default=5000,
                   help="AD2 sample rate in Hz (default: 5000)")
    p.add_argument("--cycles", type=int, default=2,
                   help="Number of firmware cycles to capture (default: 2)")
    p.add_argument("--verbose", action="store_true",
                   help="Print per-channel edge counts")
    args = p.parse_args()

    print("Pi5 GPIO verification via 2x AD2")
    print(f"  sample rate: {args.sample_rate} Hz")
    print(f"  cycles:      {args.cycles}")
    print(f"  cycle_ms:    {CYCLE_DURATION_MS}")
    print()

    print("Opening AD2s ...")
    dwf = DwfLibrary()
    devs = open_devices(dwf)

    try:
        print("Configuring digital inputs ...")
        for d in devs:
            configure(d, args.sample_rate)

        capture_ms = float(args.cycles * CYCLE_DURATION_MS + 500)
        samples = capture(devs, args.sample_rate, capture_ms)

        print("Detecting edges ...")
        edges: list[EdgeEvent] = []
        for i, s in enumerate(samples):
            e = detect_edges(s, i, args.sample_rate)
            edges.extend(e)
            print(f"  unit {i}: {len(e)} edges")

        mapping, cls = auto_map(edges, args.cycles)
        print_report(mapping, cls, args.verbose)

        if set(mapping.values()) != set(SWEEP_ORDER):
            sys.exit(1)
    finally:
        for d in devs:
            d.close()
        print("\nAD2 devices closed.")


if __name__ == "__main__":
    main()
