#!/usr/bin/env python3
"""Capture DIO7 for 20s and group edges using a 250 ms gap threshold.

Tuned for `pb5_mark_phase_slow()` in cdc_test_main.c which emits pulses at
~130 ms cadence with ~300 ms gaps between phase bursts.  Edges are counted
as rising-edge pulses within a burst; each burst prints its pulse count so
register-dump values are directly readable.
"""
import ctypes, sys, time
from ctypes import byref, c_int, c_uint, c_double, c_ubyte

dwf = ctypes.cdll.LoadLibrary("/Library/Frameworks/dwf.framework/dwf")
hdwf = c_int()
dwf.FDwfDeviceOpen(c_int(-1), byref(hdwf))

base = c_double()
dwf.FDwfDigitalInInternalClockInfo(hdwf, byref(base))

max_buf = c_int()
dwf.FDwfDigitalInBufferSizeInfo(hdwf, byref(max_buf))
n = max_buf.value

seconds = 30.0
sample_hz = n / seconds
divider = max(1, int(round(base.value / sample_hz)))
dwf.FDwfDigitalInDividerSet(hdwf, c_uint(divider))
actual_hz = base.value / divider

dwf.FDwfDigitalInBufferSizeSet(hdwf, c_int(n))
dwf.FDwfDigitalInSampleFormatSet(hdwf, c_int(8))
dwf.FDwfDigitalInAcquisitionModeSet(hdwf, c_int(0))

dwf.FDwfDigitalInTriggerSourceSet(hdwf, c_ubyte(11))
dwf.FDwfDigitalInTriggerPositionSet(hdwf, c_uint(n - 64))
dwf.FDwfDigitalInTriggerSet(
    hdwf, c_uint(0), c_uint(0),
    c_uint(1 << 7), c_uint(1 << 7),
)
dwf.FDwfDigitalInTriggerAutoTimeoutSet(hdwf, c_double(30.0))

dwf.FDwfDigitalInConfigure(hdwf, c_int(1), c_int(1))
print(f"Armed @ {actual_hz:.0f} Hz ({n/actual_hz:.1f} s window)")

sts = c_ubyte()
deadline = time.time() + 50.0
while time.time() < deadline:
    dwf.FDwfDigitalInStatus(hdwf, c_int(1), byref(sts))
    if sts.value == 2:
        break
    time.sleep(0.05)

buf = (c_ubyte * n)()
dwf.FDwfDigitalInStatusData(hdwf, buf, c_int(n))

io7 = 1 << 7
rising = []
prev = buf[0] & io7
for i in range(1, n):
    cur = buf[i] & io7
    if cur and not prev:
        rising.append(i)
    prev = cur

print(f"Total rising edges: {len(rising)}")
if not rising:
    dwf.FDwfDeviceClose(hdwf)
    sys.exit(0)

# Group edges with >150 ms gap as separate phase bursts.
gap = int(0.400 * actual_hz)
bursts = [[rising[0]]]
for e in rising[1:]:
    if (e - bursts[-1][-1]) > gap:
        bursts.append([e])
    else:
        bursts[-1].append(e)

print(f"\n{len(bursts)} phase burst(s) (>250 ms gap):")
for i, b in enumerate(bursts):
    t0 = b[0] / actual_hz * 1000.0
    t1 = b[-1] / actual_hz * 1000.0
    print(f"  burst {i+1:3d}: {len(b):3d} pulses   t={t0:8.1f}..{t1:8.1f} ms   dur={t1-t0:7.1f} ms")

dwf.FDwfDeviceClose(hdwf)
