#!/usr/bin/env python3
"""
Capture PB5 / AD2 IO7 phase pulses emitted by cdc_test firmware.

The firmware pulses PB5 N times at each phase milestone:
  1 pulse  = clock_init() returned
  2 pulses = cmt0_init() returned (about to enter ThreadX)
  3 pulses = cdc_test_task() entered
  4 pulses = rx_usb_init() about to be called
  5 pulses = rx_usb_init() returned
  continuous = main loop is toggling PB5 every ~50 ms (= 20 Hz)

The last burst observed tells us where the firmware hangs.  If we also see
the continuous 20 Hz toggle after burst 5, we know the task is alive.

Wiring: AD2 IO7 -> RX72N pin 80 (PB5), AD2 GND -> board GND.
"""

import ctypes
import sys
import time
from ctypes import byref, c_int, c_uint, c_double, c_ushort, c_ubyte


def main() -> int:
    dwf = ctypes.cdll.LoadLibrary("/Library/Frameworks/dwf.framework/dwf")

    hdwf = c_int()
    if dwf.FDwfDeviceOpen(c_int(-1), byref(hdwf)) == 0 or hdwf.value == 0:
        err = ctypes.create_string_buffer(512)
        dwf.FDwfGetLastErrorMsg(err)
        print(f"ERROR: FDwfDeviceOpen failed: {err.value.decode()}")
        return 1
    print(f"Opened AD2 (hdwf={hdwf.value})")

    try:
        seconds   = float(sys.argv[1]) if len(sys.argv) > 1 else 6.0
        # AD2 DigitalIn buffer is only 4096 samples in Single mode.
        # Pick sample rate so (max_buf / sample_rate) >= requested seconds.
        max_buf = c_int()
        dwf.FDwfDigitalInBufferSizeInfo(hdwf, byref(max_buf))
        n_samples = max_buf.value
        sample_hz = n_samples / seconds
        print(f"Max buffer {n_samples} samples; sample rate {sample_hz/1000:.1f} kHz "
              f"to cover {seconds:.1f} s")

        dwf.FDwfDigitalInInternalClockInfo.restype = c_int
        base_hz = c_double()
        dwf.FDwfDigitalInInternalClockInfo(hdwf, byref(base_hz))
        divider = max(1, int(round(base_hz.value / sample_hz)))
        dwf.FDwfDigitalInDividerSet(hdwf, c_uint(divider))
        actual_hz = base_hz.value / divider
        print(f"Base clk {base_hz.value/1e6:.1f} MHz, divider {divider}, "
              f"actual sample rate {actual_hz/1e6:.3f} MHz")

        dwf.FDwfDigitalInSampleFormatSet(hdwf, c_int(8))
        dwf.FDwfDigitalInBufferSizeSet(hdwf, c_int(n_samples))
        dwf.FDwfDigitalInTriggerPositionSet(hdwf, c_uint(n_samples))
        dwf.FDwfDigitalInAcquisitionModeSet(hdwf, c_int(0))

        dwf.FDwfDigitalInConfigure(hdwf, c_int(0), c_int(1))
        print(f"Capturing {seconds:.1f} s ({n_samples} samples) on DIO0..7...")

        deadline = time.time() + seconds + 3.0
        sts = c_ubyte()
        while time.time() < deadline:
            if dwf.FDwfDigitalInStatus(hdwf, c_int(1), byref(sts)) == 0:
                print("ERROR: FDwfDigitalInStatus failed")
                return 1
            if sts.value == 2:
                break
            time.sleep(0.05)
        else:
            print(f"WARN: capture did not reach Done state (sts={sts.value})")

        buf = (c_ubyte * n_samples)()
        dwf.FDwfDigitalInStatusData(hdwf, buf, c_int(n_samples))

        io7_bit = 1 << 7
        prev = buf[0] & io7_bit
        edges_rising = []
        edges_falling = []
        for i in range(1, n_samples):
            cur = buf[i] & io7_bit
            if cur and not prev:
                edges_rising.append(i)
            elif not cur and prev:
                edges_falling.append(i)
            prev = cur

        print(f"\nEdges on IO7: {len(edges_rising)} rising, "
              f"{len(edges_falling)} falling")

        if not edges_rising:
            print("No activity on IO7 -- firmware never toggled PB5.")
            print("Likely hang BEFORE phase 1 (clock_init did not return,")
            print("or PB5 was never configured as output).")
            return 0

        burst_gap_us = 20_000.0
        gap_samples  = int((burst_gap_us / 1e6) * actual_hz)
        bursts = []
        cur_burst = [edges_rising[0]]
        for e in edges_rising[1:]:
            if (e - cur_burst[-1]) > gap_samples:
                bursts.append(cur_burst)
                cur_burst = [e]
            else:
                cur_burst.append(e)
        bursts.append(cur_burst)

        print(f"Detected {len(bursts)} bursts (gap >= {burst_gap_us/1000:.0f} ms):")
        for idx, b in enumerate(bursts):
            t0 = b[0] / actual_hz
            t1 = b[-1] / actual_hz
            print(f"  burst {idx+1}: {len(b):3d} pulses, "
                  f"t={t0*1000:7.2f} ms .. {t1*1000:7.2f} ms, "
                  f"duration {(t1-t0)*1000:6.2f} ms")

        last = bursts[-1]
        if len(last) > 20:
            period_s = (last[-1] - last[0]) / ((len(last) - 1) * actual_hz)
            freq_hz  = 1.0 / period_s if period_s > 0 else 0.0
            print(f"\nFinal burst looks like a continuous toggle "
                  f"({len(last)} pulses, ~{freq_hz:.1f} Hz).")
            if len(bursts) >= 2:
                last_phase_burst = bursts[-2]
                print(f"Last phase marker: {len(last_phase_burst)} pulses "
                      f"-- firmware reached phase {len(last_phase_burst)}")
                if len(last_phase_burst) == 5:
                    print("PHASE 5 reached: rx_usb_init() returned, "
                          "main loop is alive.")
        else:
            print(f"\nLast burst had {len(last)} pulses -- "
                  f"firmware reached phase {len(last)} then hung.")
            phase_names = {
                1: "clock_init done (hang in sci9_debug_init or cmt0_init)",
                2: "cmt0_init done (hang in tx_kernel_enter / scheduler)",
                3: "task entered (hang in sci9_debug_puts or pb5_mark_phase(4))",
                4: "before rx_usb_init (hang INSIDE rx_usb_init())",
                5: "rx_usb_init returned (hang in main loop -- unexpected)",
            }
            if len(last) in phase_names:
                print(f"  -> {phase_names[len(last)]}")

        return 0
    finally:
        dwf.FDwfDeviceClose(hdwf)


if __name__ == "__main__":
    sys.exit(main())
