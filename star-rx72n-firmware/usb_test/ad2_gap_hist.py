#!/usr/bin/env python3
"""Capture 20s, print histogram of rising-edge gaps for threshold tuning."""
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

seconds = 20.0
sample_hz = n / seconds
divider = max(1, int(round(base.value / sample_hz)))
dwf.FDwfDigitalInDividerSet(hdwf, c_uint(divider))
actual_hz = base.value / divider

dwf.FDwfDigitalInBufferSizeSet(hdwf, c_int(n))
dwf.FDwfDigitalInSampleFormatSet(hdwf, c_int(8))
dwf.FDwfDigitalInAcquisitionModeSet(hdwf, c_int(0))

dwf.FDwfDigitalInTriggerSourceSet(hdwf, c_ubyte(11))
dwf.FDwfDigitalInTriggerPositionSet(hdwf, c_uint(n - 64))
dwf.FDwfDigitalInTriggerSet(hdwf, c_uint(0), c_uint(0), c_uint(1 << 7), c_uint(1 << 7))
dwf.FDwfDigitalInTriggerAutoTimeoutSet(hdwf, c_double(30.0))
dwf.FDwfDigitalInConfigure(hdwf, c_int(1), c_int(1))
print(f"Armed @ {actual_hz:.0f} Hz")

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

print(f"{len(rising)} rising edges")
if len(rising) < 2:
    sys.exit(0)

gaps_ms = [(rising[i+1] - rising[i]) / actual_hz * 1000.0 for i in range(len(rising)-1)]
gaps_ms_sorted = sorted(gaps_ms)

buckets = [(0, 30), (30, 60), (60, 100), (100, 150), (150, 200), (200, 300),
           (300, 500), (500, 800), (800, 1500), (1500, 5000)]
print(f"\nGap histogram (ms):")
for lo, hi in buckets:
    count = sum(1 for g in gaps_ms if lo <= g < hi)
    bar = "#" * min(count, 60)
    print(f"  [{lo:5.0f}..{hi:5.0f})  {count:3d}  {bar}")

print(f"\nMin gap: {min(gaps_ms):.1f} ms   Max gap: {max(gaps_ms):.1f} ms")
print(f"Median: {gaps_ms_sorted[len(gaps_ms_sorted)//2]:.1f} ms")
dwf.FDwfDeviceClose(hdwf)
