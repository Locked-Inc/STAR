#!/usr/bin/env python3
"""
ad2_passive.py -- read idle state of every AD2 channel without driving.

Use to sanity-check wiring before the toggle test: tells you which
probes are pinned HIGH (likely a 3V3 rail or input with pull-up),
pinned LOW (GND or pull-down), or floating (noisy, no contact).

Does NOT identify broken GPIOs -- the Pi5 has to be toggling for that.
"""

import sys
import time

try:
    from pydwf import (DwfLibrary, DwfDigitalInClockSource,
                       DwfAcquisitionMode)
    from pydwf.utilities import openDwfDevice
except ImportError:
    print("ERROR: pydwf not installed", file=sys.stderr)
    sys.exit(1)

AD2_SERIAL_NUMBERS = ["210321A2AE49", "210321A36AAE"]
CHANNELS_PER_AD2 = 16
SAMPLE_RATE_HZ = 5000
CAPTURE_MS = 500


def configure(dev):
    dio = dev.digitalIn
    dio.clockSourceSet(DwfDigitalInClockSource.Internal)
    dio.dividerSet(int(100e6 / SAMPLE_RATE_HZ))
    dio.bufferSizeSet(4096)
    dio.acquisitionModeSet(DwfAcquisitionMode.Record)
    dio.sampleFormatSet(16)


def capture(dev) -> list[int]:
    dev.digitalIn.configure(False, True)
    samples: list[int] = []
    deadline = time.monotonic() + (CAPTURE_MS / 1000.0) + 0.3
    while time.monotonic() < deadline:
        dev.digitalIn.status(True)
        avail, _l, _c = dev.digitalIn.statusRecord()
        if avail > 0:
            data = dev.digitalIn.statusData(avail, sample_format=16)
            samples.extend(int(s) for s in data)
        time.sleep(0.001)
    dev.digitalIn.configure(False, False)
    return samples


def classify(samples: list[int], channel: int) -> tuple[str, float, int]:
    bit = 1 << channel
    if not samples:
        return ("no-data", 0.0, 0)
    high = sum(1 for s in samples if s & bit)
    total = len(samples)
    duty = high / total
    transitions = sum(1 for i in range(1, total)
                      if bool(samples[i] & bit) != bool(samples[i - 1] & bit))
    if transitions > total * 0.05:
        verdict = "floating"
    elif duty > 0.95:
        verdict = "HIGH"
    elif duty < 0.05:
        verdict = "LOW"
    else:
        verdict = f"unstable({duty:.0%})"
    return verdict, duty, transitions


def main() -> None:
    print(f"Passive wiring check: {CAPTURE_MS} ms capture at "
          f"{SAMPLE_RATE_HZ} Hz, no Pi5 driving required")
    dwf = DwfLibrary()
    devs = []
    for i, sn in enumerate(AD2_SERIAL_NUMBERS):
        try:
            devs.append(openDwfDevice(dwf, serial_number_filter=sn))
            print(f"  unit {i} (SN {sn}): opened")
        except Exception as e:
            for d in devs:
                d.close()
            print(f"  unit {i}: FAILED -- {e}", file=sys.stderr)
            sys.exit(1)

    try:
        for d in devs:
            configure(d)
        captures = [capture(d) for d in devs]
        for i, s in enumerate(captures):
            print(f"  unit {i}: {len(s)} samples")

        print("\n" + "=" * 60)
        print("CHANNEL STATE")
        print("=" * 60)
        for u, s in enumerate(captures):
            for ch in range(CHANNELS_PER_AD2):
                verdict, duty, tr = classify(s, ch)
                print(f"  unit {u} DIO{ch:2d}  {verdict:12s}  "
                      f"duty={duty:.2%}  transitions={tr}")
            print()
    finally:
        for d in devs:
            d.close()


if __name__ == "__main__":
    main()
