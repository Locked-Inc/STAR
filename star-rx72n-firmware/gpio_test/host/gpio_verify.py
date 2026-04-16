#!/usr/bin/env python3
"""
gpio_verify.py -- AD2 capture and verification for GPIO breakout board test.

Captures digital inputs from 3x Analog Discovery 2 instruments and verifies
that each GPIO pin toggles correctly when the RX72N firmware drives them.

The firmware (gpio_test/main.c) toggles each pin sequentially:
  - HIGH for 50 ms, LOW for 50 ms, then next pin
  - 1 s gap between full cycles

This script:
  1. Opens all 3 AD2s by serial number
  2. Samples digital inputs at 1 kHz for one full cycle (~11 s)
  3. Detects rising/falling edges on each channel
  4. Matches edge timing to the known pin sequence
  5. Reports pass/fail per pin

Requirements:
  - Python 3.8+
  - pydwf >= 1.1.19 (pip install pydwf)
  - libdwf >= 3.24.3 (Digilent WaveForms runtime)

Usage:
  python3 gpio_verify.py [--sample-rate 1000] [--cycles 1] [--verbose]

SPDX-License-Identifier: MIT
Copyright (c) 2026 Locked Inc.
"""

import argparse
import sys
import time
from dataclasses import dataclass, field

try:
    from pydwf import DwfLibrary
    from pydwf.utilities import openDwfDevice
except ImportError:
    print("ERROR: pydwf not installed. Run: pip install pydwf", file=sys.stderr)
    sys.exit(1)

# ============================================================================
# AD2 device configuration
# ============================================================================

AD2_SERIAL_NUMBERS = [
    "210321A36AA3",  # AD2 #0
    "210321A36AAE",  # AD2 #1
]

CHANNELS_PER_AD2 = 16  # DIO 0..15 on each AD2

# ============================================================================
# AD2-channel-to-pin mapping
#
# Fill this in once the AD2 probes are wired to the breakout board headers.
# Each entry maps (ad2_index, dio_channel) -> pin_name.
#
# Example:
#   (0, 0): "P00"   means AD2 #0, DIO 0 is connected to P00 (BGA 8)
#
# Unmapped channels are ignored during verification.
# ============================================================================

CHANNEL_TO_PIN: dict[tuple[int, int], str] = {
    # AD2 #0 (SN: 210321A2AE49)
    # (0, 0): "P05",
    # (0, 1): "P03",
    # (0, 2): "P02",
    # ... fill in when wired ...

    # AD2 #1 (SN: 210321A36AA3)
    # (1, 0): "P20",
    # ... fill in when wired ...

    # AD2 #2 (SN: 210321A36AAE)
    # (2, 0): "PE3",
    # ... fill in when wired ...
}

# ============================================================================
# Firmware pin order -- must match s_pins[] in main.c exactly.
# The index in this list = the firmware's sequential pin index.
# ============================================================================

FIRMWARE_PIN_ORDER = [
    # Port 0
    "P00", "P01", "P02", "P03", "P05", "P07",
    # Port 1
    "P12", "P13", "P14", "P15", "P17",
    # Port 2
    "P20", "P21", "P22", "P23", "P24", "P25",
    # Port 3
    "P32", "P33",
    # Port 4
    "P40", "P41", "P42", "P43", "P44", "P45", "P46", "P47",
    # Port 5
    "P50", "P51", "P52", "P53", "P54", "P55", "P56",
    # Port 6
    "P60", "P61", "P62", "P63", "P64", "P65", "P66", "P67",
    # Port 7
    "P70", "P71", "P72", "P73", "P74", "P75", "P76", "P77",
    # Port 8
    "P80", "P81", "P82", "P83", "P86", "P87",
    # Port 9
    "P90", "P91", "P92", "P93",
    # Port A
    "PA0", "PA1", "PA2", "PA3", "PA4", "PA5", "PA6", "PA7",
    # Port B
    "PB0", "PB1", "PB2", "PB3", "PB4", "PB5",
    # Port C
    "PC0", "PC1", "PC2", "PC3", "PC4", "PC5", "PC6",
    # Port D
    "PD0", "PD1", "PD2", "PD3", "PD4", "PD5", "PD6", "PD7",
    # Port E
    "PE0", "PE1", "PE2", "PE3", "PE4", "PE5", "PE6", "PE7",
    # Port F
    "PF5",
    # Port J
    "PJ3", "PJ5",
]

NUM_PINS = len(FIRMWARE_PIN_ORDER)

# Firmware timing (seconds)
PIN_HIGH_MS = 50
PIN_LOW_MS = 50
PIN_PERIOD_MS = PIN_HIGH_MS + PIN_LOW_MS
CYCLE_GAP_MS = 1000
CYCLE_DURATION_MS = NUM_PINS * PIN_PERIOD_MS + CYCLE_GAP_MS


@dataclass
class EdgeEvent:
    """A detected rising or falling edge on an AD2 channel."""
    timestamp_ms: float
    ad2_index: int
    channel: int
    rising: bool


@dataclass
class PinResult:
    """Verification result for a single GPIO pin."""
    pin_name: str
    firmware_index: int
    expected_window_ms: tuple[float, float]  # (start, end) of expected HIGH
    ad2_index: int = -1
    channel: int = -1
    detected: bool = False
    edge_count: int = 0
    verdict: str = "NOT_WIRED"


def open_ad2_devices(dwf: DwfLibrary) -> list:
    """Open all 3 AD2 devices by serial number."""
    devices = []
    for i, sn in enumerate(AD2_SERIAL_NUMBERS):
        try:
            dev = openDwfDevice(dwf, serial_number_filter=sn)
            devices.append(dev)
            print(f"  AD2 #{i} (SN: {sn}): opened")
        except Exception as e:
            print(f"  AD2 #{i} (SN: {sn}): FAILED -- {e}", file=sys.stderr)
            for d in devices:
                d.close()
            sys.exit(1)
    return devices


def configure_digital_input(dev, sample_rate_hz: int) -> None:
    """Configure AD2 digital input for continuous acquisition."""
    dio = dev.digitalIn

    # Use internal clock, sample rate
    dio.clockSourceSet(0)  # internal
    dio.dividerSet(int(100e6 / sample_rate_hz))  # 100 MHz base clock

    # Enable all 16 channels
    dio.bufferSizeSet(4096)

    # Single acquisition mode -- we will poll manually
    dio.acquisitionModeSet(0)  # single

    # 16-bit sample format
    dio.sampleFormatSet(16)


def capture_cycle(devices: list, sample_rate_hz: int,
                  duration_ms: float) -> list[list[list[int]]]:
    """
    Capture digital samples from all AD2s for the specified duration.

    Returns: list of per-AD2 sample buffers.
    Each buffer is a list of 16-bit values (one per sample, each bit = one DIO).
    """
    import ctypes

    total_samples = int(sample_rate_hz * duration_ms / 1000)
    all_samples: list[list[int]] = [[] for _ in range(len(devices))]

    # Start acquisition on all devices
    for dev in devices:
        dio = dev.digitalIn
        dio.configure(False, True)  # reconfigure=False, start=True

    print(f"  Capturing {total_samples} samples over {duration_ms:.0f} ms ...")

    start_time = time.monotonic()
    deadline = start_time + (duration_ms / 1000.0) + 0.5  # 500ms margin

    while time.monotonic() < deadline:
        for i, dev in enumerate(devices):
            dio = dev.digitalIn
            status = dio.status(True)  # read data

            # Read available samples
            available = dio.statusSamplesValid()
            if available > 0:
                buf = (ctypes.c_uint16 * available)()
                dio.statusData(buf, available)
                all_samples[i].extend(buf)

        time.sleep(0.001)  # 1 ms poll interval

    # Stop acquisition
    for dev in devices:
        dev.digitalIn.configure(False, False)

    for i, samples in enumerate(all_samples):
        print(f"  AD2 #{i}: captured {len(samples)} samples")

    return all_samples


def detect_edges(samples: list[int], ad2_index: int,
                 sample_rate_hz: int) -> list[EdgeEvent]:
    """Detect rising and falling edges on all 16 channels of one AD2."""
    edges: list[EdgeEvent] = []
    if len(samples) < 2:
        return edges

    sample_period_ms = 1000.0 / sample_rate_hz
    prev = samples[0]

    for idx in range(1, len(samples)):
        curr = samples[idx]
        changed = prev ^ curr
        if changed != 0:
            t_ms = idx * sample_period_ms
            for ch in range(CHANNELS_PER_AD2):
                bit = 1 << ch
                if changed & bit:
                    rising = bool(curr & bit)
                    edges.append(EdgeEvent(
                        timestamp_ms=t_ms,
                        ad2_index=ad2_index,
                        channel=ch,
                        rising=rising,
                    ))
        prev = curr

    return edges


def build_expected_windows() -> list[PinResult]:
    """Build expected HIGH windows for each pin based on firmware timing."""
    results = []
    for i, pin_name in enumerate(FIRMWARE_PIN_ORDER):
        start_ms = i * PIN_PERIOD_MS
        end_ms = start_ms + PIN_HIGH_MS
        results.append(PinResult(
            pin_name=pin_name,
            firmware_index=i,
            expected_window_ms=(float(start_ms), float(end_ms)),
        ))
    return results


def verify_edges(results: list[PinResult], all_edges: list[EdgeEvent],
                 verbose: bool) -> None:
    """Match detected edges to expected pin windows and set verdicts."""

    # Build reverse map: pin_name -> (ad2_index, channel)
    pin_to_channel: dict[str, tuple[int, int]] = {}
    for (ad2_idx, ch), pin_name in CHANNEL_TO_PIN.items():
        pin_to_channel[pin_name] = (ad2_idx, ch)

    # Group edges by (ad2_index, channel)
    edge_map: dict[tuple[int, int], list[EdgeEvent]] = {}
    for e in all_edges:
        key = (e.ad2_index, e.channel)
        edge_map.setdefault(key, []).append(e)

    tolerance_ms = 20.0  # +/- 20 ms timing tolerance

    for result in results:
        if result.pin_name not in pin_to_channel:
            result.verdict = "NOT_WIRED"
            continue

        ad2_idx, ch = pin_to_channel[result.pin_name]
        result.ad2_index = ad2_idx
        result.channel = ch

        key = (ad2_idx, ch)
        edges = edge_map.get(key, [])
        result.edge_count = len(edges)

        if len(edges) == 0:
            result.verdict = "FAIL_NO_EDGES"
            continue

        # Look for a rising edge near the expected HIGH start
        exp_start, exp_end = result.expected_window_ms
        found_rising = False
        found_falling = False

        for e in edges:
            if e.rising and abs(e.timestamp_ms - exp_start) < tolerance_ms:
                found_rising = True
            if not e.rising and abs(e.timestamp_ms - exp_end) < tolerance_ms:
                found_falling = True

        if found_rising and found_falling:
            result.detected = True
            result.verdict = "PASS"
        elif found_rising:
            result.verdict = "FAIL_NO_FALLING"
        elif found_falling:
            result.verdict = "FAIL_NO_RISING"
        else:
            result.verdict = "FAIL_TIMING"

        if verbose and result.verdict != "PASS":
            print(f"  [{result.pin_name}] edges: {edges[:4]}")


def print_report(results: list[PinResult]) -> None:
    """Print a summary report of all pin test results."""
    passed = sum(1 for r in results if r.verdict == "PASS")
    not_wired = sum(1 for r in results if r.verdict == "NOT_WIRED")
    failed = sum(1 for r in results if r.verdict.startswith("FAIL"))

    print("\n" + "=" * 72)
    print("GPIO BREAKOUT BOARD VERIFICATION REPORT")
    print("=" * 72)

    # Print results grouped by verdict
    for verdict_filter in ["PASS", "NOT_WIRED", "FAIL_NO_EDGES",
                           "FAIL_NO_RISING", "FAIL_NO_FALLING",
                           "FAIL_TIMING"]:
        group = [r for r in results if r.verdict == verdict_filter]
        if not group:
            continue
        print(f"\n--- {verdict_filter} ({len(group)}) ---")
        for r in group:
            loc = (f"AD2#{r.ad2_index} DIO{r.channel}"
                   if r.ad2_index >= 0 else "unmapped")
            print(f"  [{r.firmware_index:3d}] {r.pin_name:4s}  {loc:14s}"
                  f"  edges={r.edge_count}")

    print(f"\n{'=' * 72}")
    print(f"TOTAL: {NUM_PINS} pins | "
          f"PASS: {passed} | FAIL: {failed} | NOT_WIRED: {not_wired}")
    print("=" * 72)

    if failed > 0:
        sys.exit(1)


def auto_discover_mapping(all_edges: list[EdgeEvent]) -> dict[tuple[int, int], str]:
    """
    Figure out which AD2 channel maps to which firmware pin without any
    prior wiring information.

    How it works:
      1. The firmware toggles pins in the known order in FIRMWARE_PIN_ORDER,
         each pin HIGH for 50 ms then LOW for 50 ms (100 ms per pin).
      2. Between cycles there is a 1000 ms quiet gap (no edges).
      3. We find the longest edge-free interval across all channels; the
         edge immediately AFTER that gap is pin index 0's rising edge.
      4. Every other channel's rising edge timestamp, relative to that
         anchor, tells us which pin index it's wired to:
             pin_index = round((t_edge - t_anchor) / 100)
      5. Look up FIRMWARE_PIN_ORDER[pin_index] for the pin name.
    """
    rising_times = sorted(e.timestamp_ms for e in all_edges if e.rising)
    if len(rising_times) < 2:
        print("  auto-map: not enough rising edges to infer mapping")
        return {}

    # Find the longest gap between consecutive rising edges across all
    # channels. The firmware cycle gap (~1000 ms) is the only interval
    # longer than the 100 ms pin period, so it wins.
    gaps = [(rising_times[i + 1] - rising_times[i], rising_times[i + 1])
            for i in range(len(rising_times) - 1)]
    gap_duration, t_anchor = max(gaps, key=lambda x: x[0])

    if gap_duration < 500.0:
        print(f"  auto-map: longest gap is only {gap_duration:.0f} ms "
              f"(expected ~1000 ms). Capture too short or firmware not running?")
        return {}

    print(f"  auto-map: cycle boundary at t = {t_anchor:.0f} ms "
          f"(gap = {gap_duration:.0f} ms)")

    # Group rising edges per (ad2, channel); first edge after anchor is that
    # channel's pin.
    per_channel: dict[tuple[int, int], list[float]] = {}
    for e in all_edges:
        if e.rising:
            per_channel.setdefault((e.ad2_index, e.channel), []).append(
                e.timestamp_ms)

    mapping: dict[tuple[int, int], str] = {}
    cycle_end = t_anchor + NUM_PINS * PIN_PERIOD_MS
    for key, times in per_channel.items():
        after = [t for t in times if t_anchor - 20.0 <= t < cycle_end]
        if not after:
            continue
        t_edge = min(after)
        pin_idx = round((t_edge - t_anchor) / PIN_PERIOD_MS)
        if 0 <= pin_idx < NUM_PINS:
            mapping[key] = FIRMWARE_PIN_ORDER[pin_idx]
    return mapping


def print_auto_mapping(mapping: dict[tuple[int, int], str]) -> None:
    """Print a paste-ready CHANNEL_TO_PIN dict for the user."""
    if not mapping:
        print("\nAuto-map produced no results. Check wiring and re-run.")
        return
    print("\n" + "=" * 72)
    print("AUTO-DETECTED CHANNEL_TO_PIN MAPPING")
    print("Paste this into gpio_verify.py to replace the current dict.")
    print("=" * 72)
    print("CHANNEL_TO_PIN = {")
    last_ad2 = -1
    for (ad2, ch) in sorted(mapping):
        if ad2 != last_ad2:
            print(f"    # AD2 #{ad2}")
            last_ad2 = ad2
        print(f'    ({ad2}, {ch:2d}): "{mapping[(ad2, ch)]}",')
    print("}")
    print("=" * 72)
    print(f"Mapped {len(mapping)} channels total.\n")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="GPIO breakout board verification via AD2")
    parser.add_argument("--sample-rate", type=int, default=1000,
                        help="AD2 sample rate in Hz (default: 1000)")
    parser.add_argument("--cycles", type=int, default=1,
                        help="Number of firmware cycles to capture (default: 1)")
    parser.add_argument("--verbose", action="store_true",
                        help="Print detailed edge info for failures")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print config and exit without capturing")
    parser.add_argument("--auto-map", action="store_true",
                        help="Auto-discover which channel is wired to which "
                             "pin. Captures 2 cycles and prints a paste-ready "
                             "CHANNEL_TO_PIN dict. Skip verification step.")
    args = parser.parse_args()

    # Auto-map forces 2 capture cycles so the cycle-gap anchor is always in
    # range regardless of when the capture starts relative to firmware boot.
    if args.auto_map and args.cycles < 2:
        args.cycles = 2

    print(f"GPIO Breakout Board Verification")
    print(f"  Pins:        {NUM_PINS}")
    print(f"  Sample rate: {args.sample_rate} Hz")
    print(f"  Cycle time:  {CYCLE_DURATION_MS} ms")
    print(f"  Mode:        {'AUTO-MAP' if args.auto_map else 'VERIFY'}")
    if not args.auto_map:
        print(f"  Mapped channels: {len(CHANNEL_TO_PIN)}")
    print()

    if not args.auto_map and len(CHANNEL_TO_PIN) == 0:
        print("WARNING: No AD2 channels mapped to pins yet.")
        print("Either edit CHANNEL_TO_PIN in this script or re-run with "
              "--auto-map to discover the mapping automatically.")
        if not args.dry_run:
            print("Proceeding with capture anyway (edge detection only).\n")

    if args.dry_run:
        results = build_expected_windows()
        print_report(results)
        return

    # Open AD2 devices
    print("Opening AD2 devices ...")
    dwf = DwfLibrary()
    devices = open_ad2_devices(dwf)

    try:
        # Configure digital inputs
        print("Configuring digital inputs ...")
        for dev in devices:
            configure_digital_input(dev, args.sample_rate)

        # Capture
        capture_ms = float(args.cycles * CYCLE_DURATION_MS + 500)
        all_samples = capture_cycle(devices, args.sample_rate, capture_ms)

        # Detect edges
        print("Detecting edges ...")
        all_edges: list[EdgeEvent] = []
        for i, samples in enumerate(all_samples):
            edges = detect_edges(samples, i, args.sample_rate)
            all_edges.extend(edges)
            print(f"  AD2 #{i}: {len(edges)} edges detected")

        if args.auto_map:
            mapping = auto_discover_mapping(all_edges)
            print_auto_mapping(mapping)
            return

        # Verify
        print("Verifying pin toggles ...")
        results = build_expected_windows()
        verify_edges(results, all_edges, args.verbose)

        # Report
        print_report(results)

    finally:
        for dev in devices:
            dev.close()
        print("\nAD2 devices closed.")


if __name__ == "__main__":
    main()
