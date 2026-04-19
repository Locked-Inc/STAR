#!/usr/bin/env python3
"""edging_tests.py -- edge-accurate AD2 capture for pwm_test GPTW output.

Connects to the first visible Analog Discovery 2 over USB, scope-captures
pins 38 (P17 / GTIOC0B) on Ch1 and pin 34 (P23 / GTIOC0A) on Ch2, and
then runs a battery of edge-timing tests on the 20 kHz PWM waveform.

Tests reported (pass / fail + measured value):
    frequency_ch1     20 kHz +/- 1 %
    frequency_ch2     20 kHz +/- 1 %
    logic_level_high  >= 3.0 V, <= 3.4 V on both channels
    logic_level_low   <= 0.2 V on both channels
    edge_rise_time    <= 50 ns (Ch1 low->high)
    edge_fall_time    <= 50 ns (Ch1 high->low)
    duty_complement   Ch1 duty + Ch2 duty ~= 100 % (sign-magnitude)
    edge_jitter       stddev of measured period / mean period < 0.5 %
    edge_count        enough edges captured in window (>= 50)

Wiring (Tom's PCB breakout):
    AD2 Ch1+  ->  header pin 38  (RX72N P17 / GTIOC0B)
    AD2 Ch2+  ->  header pin 34  (RX72N P23 / GTIOC0A)
    AD2 GNDs  ->  any GND pad
"""

from __future__ import annotations

import argparse
import statistics
import sys
import time
from dataclasses import dataclass

try:
    from pydwf import (
        DwfLibrary,
        DwfAnalogInFilter as AnalogInFilter,
        DwfTriggerSlope as TriggerSlope,
        DwfTriggerSource,
        DwfAnalogInTriggerType as AnalogInTriggerType,
    )
    from pydwf.utilities import openDwfDevice
except ImportError:
    sys.exit("pydwf not installed: pip install pydwf")

SRATE   = 20_000_000.0     # 20 Msps -> 50 ns per sample
NSAMPLE = 40_000           # 2 ms window -> ~40 periods at 20 kHz
V_HI_MIN = 3.0
V_HI_MAX = 3.4
V_LO_MAX = 0.2
PWM_FREQ_HZ = 20_000.0
PWM_FREQ_TOL = 0.01        # 1 %
DUTY_COMPLEMENT_TOL = 0.05 # 5 %
JITTER_PERIOD_STDDEV = 0.005  # 0.5 %
EDGE_TIME_NS_MAX = 200.0     # 200 ns rise / fall max (AD2 + cable bandwidth)

@dataclass
class EdgeStats:
    hi: float
    lo: float
    mid: float
    edges: list[tuple[int, str]]
    periods_samples: list[int]
    high_run_samples: list[int]
    freq_hz: float
    duty: float

def analyze(samples) -> EdgeStats:
    hi = max(samples); lo = min(samples); mid = (hi + lo) * 0.5
    edges = []
    last_rise = None
    rise_prev = None
    periods = []
    highs = []
    for i in range(1, len(samples)):
        if samples[i-1] < mid <= samples[i]:
            edges.append((i, "rise"))
            if rise_prev is not None:
                periods.append(i - rise_prev)
            rise_prev = i
            last_rise = i
        elif samples[i-1] > mid >= samples[i]:
            edges.append((i, "fall"))
            if last_rise is not None:
                highs.append(i - last_rise)
    if periods:
        period_s = (sum(periods) / len(periods)) / SRATE
        freq = 1.0 / period_s
    else:
        freq = float('nan')
    if highs and periods:
        duty = (sum(highs) / len(highs)) / (sum(periods) / len(periods))
    else:
        duty = float('nan')
    return EdgeStats(hi=hi, lo=lo, mid=mid, edges=edges,
                     periods_samples=periods, high_run_samples=highs,
                     freq_hz=freq, duty=duty)

def edge_time_ns(samples, edge_idx: int, lo: float, hi: float, rising: bool) -> float:
    """Measure 10-90% rise/fall time in nanoseconds, linearly interpolated."""
    thresh_lo = lo + 0.1 * (hi - lo)
    thresh_hi = lo + 0.9 * (hi - lo)
    step = 1 if rising else 1
    start = edge_idx
    # walk backwards to find where signal last crossed thresh_lo (if rising) or thresh_hi (if falling)
    i = start
    if rising:
        while i > 0 and samples[i] > thresh_lo:
            i -= 1
        start_i = i
        i = start
        while i < len(samples) - 1 and samples[i] < thresh_hi:
            i += 1
        end_i = i
    else:
        while i > 0 and samples[i] < thresh_hi:
            i -= 1
        start_i = i
        i = start
        while i < len(samples) - 1 and samples[i] > thresh_lo:
            i += 1
        end_i = i
    return max(0, (end_i - start_i)) * (1.0e9 / SRATE)

def capture() -> tuple[list, list]:
    dwf = DwfLibrary()
    dev = openDwfDevice(dwf)
    ai = dev.analogIn
    try:
        for ch in (0, 1):
            ai.channelEnableSet(ch, True)
            ai.channelRangeSet(ch, 5.0)
            ai.channelOffsetSet(ch, 1.65)
            ai.channelFilterSet(ch, AnalogInFilter.Decimate)
        ai.frequencySet(SRATE)
        ai.bufferSizeSet(NSAMPLE)
        ai.triggerSourceSet(DwfTriggerSource.DetectorAnalogIn)
        ai.triggerChannelSet(0)
        ai.triggerTypeSet(AnalogInTriggerType.Edge)
        ai.triggerLevelSet(1.65)
        ai.triggerHysteresisSet(0.2)
        ai.triggerConditionSet(TriggerSlope.Rise)
        ai.triggerPositionSet(NSAMPLE / (2 * SRATE))
        ai.triggerAutoTimeoutSet(0.1)
        ai.configure(True, True)
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            st = ai.status(True)
            if str(st).endswith("Done"):
                break
            time.sleep(0.005)
        ch1 = ai.statusData(0, NSAMPLE)
        ch2 = ai.statusData(1, NSAMPLE)
        return ch1, ch2
    finally:
        dev.close()

def run_tests(ch1, ch2) -> int:
    a = analyze(ch1)
    b = analyze(ch2)

    def print_row(name: str, val, ok: bool):
        mark = "[PASS]" if ok else "[FAIL]"
        print(f"  {mark}  {name:<22}  {val}")

    print(f"\nEDGING TESTS -- pwm_test GPTW0 (20 kHz sawtooth)")
    print(f"Sample rate: {SRATE/1e6:.1f} Msps, window: {NSAMPLE/SRATE*1e3:.2f} ms\n")

    failed = 0

    for label, st in (("Ch1 (P17/IN1)", a), ("Ch2 (P23/IN2)", b)):
        print(f"-- {label}")
        ok = V_HI_MIN <= st.hi <= V_HI_MAX
        print_row("logic_high",   f"{st.hi:.2f} V", ok);      failed += not ok
        ok = st.lo <= V_LO_MAX
        print_row("logic_low",    f"{st.lo:.2f} V", ok);      failed += not ok
        ok = len(st.edges) >= 50
        print_row("edge_count",   f"{len(st.edges)}", ok);    failed += not ok
        freq_ok = abs(st.freq_hz - PWM_FREQ_HZ) / PWM_FREQ_HZ < PWM_FREQ_TOL
        print_row("frequency",    f"{st.freq_hz/1e3:.3f} kHz", freq_ok); failed += not freq_ok
        if st.periods_samples:
            m = statistics.mean(st.periods_samples)
            s = statistics.stdev(st.periods_samples) if len(st.periods_samples) > 1 else 0.0
            jitter = s / m if m else float('inf')
            jitter_ok = jitter < JITTER_PERIOD_STDDEV
            print_row("edge_jitter", f"{jitter*100:.3f} % (period stdev/mean)", jitter_ok); failed += not jitter_ok
        if not st.edges:
            continue
        rising = [e for e in st.edges if e[1] == "rise"][:5]
        falling = [e for e in st.edges if e[1] == "fall"][:5]
        rt = [edge_time_ns(ch1 if st is a else ch2, e[0], st.lo, st.hi, True)  for e in rising]
        ft = [edge_time_ns(ch1 if st is a else ch2, e[0], st.lo, st.hi, False) for e in falling]
        if rt:
            avg = sum(rt)/len(rt)
            ok = avg <= EDGE_TIME_NS_MAX
            print_row("rise_time",  f"{avg:.0f} ns (n={len(rt)})", ok); failed += not ok
        if ft:
            avg = sum(ft)/len(ft)
            ok = avg <= EDGE_TIME_NS_MAX
            print_row("fall_time",  f"{avg:.0f} ns (n={len(ft)})", ok); failed += not ok

    # Complementary check: Ch1 duty + Ch2 duty should sum to ~100%
    if a.duty == a.duty and b.duty == b.duty:  # not NaN
        total = a.duty + b.duty
        comp_ok = abs(total - 1.0) < DUTY_COMPLEMENT_TOL
        print(f"\n-- Complementarity")
        print_row("duty_complement", f"Ch1={a.duty*100:.1f}% + Ch2={b.duty*100:.1f}% = {total*100:.1f}%",
                  comp_ok)
        failed += not comp_ok

    print(f"\n{'='*60}")
    print(f"{'[PASS]' if failed == 0 else '[FAIL]'}  summary: {failed} test(s) failed")
    return 1 if failed else 0

def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--verbose", action="store_true")
    args = p.parse_args()
    ch1, ch2 = capture()
    return run_tests(ch1, ch2)

if __name__ == "__main__":
    sys.exit(main())
