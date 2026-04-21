#!/usr/bin/env python3
"""Triggered capture: any DIO7 edge triggers, then capture 5s window.

Runs at lower sample rate (~820 Hz) to cover a 5s window in the 4096-sample
buffer.  Phase bursts are microsecond-scale so they'll alias to single-sample
edges, but edge COUNTS still map to phase numbers.  Triggers on rising OR
falling to work around stuck-high DIO7.
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

# 5 seconds over 4096 samples = 820 Hz.  Phase bursts alias but edge count
# is preserved (edges stretched to one sample each).
seconds = 20.0
sample_hz = n / seconds
divider = max(1, int(round(base.value / sample_hz)))
dwf.FDwfDigitalInDividerSet(hdwf, c_uint(divider))
actual_hz = base.value / divider

dwf.FDwfDigitalInBufferSizeSet(hdwf, c_int(n))
dwf.FDwfDigitalInSampleFormatSet(hdwf, c_int(8))
dwf.FDwfDigitalInAcquisitionModeSet(hdwf, c_int(0))  # Single

# Trigger on ANY edge on DIO7 (rising OR falling), 64 samples pre-trigger
dwf.FDwfDigitalInTriggerSourceSet(hdwf, c_ubyte(11))  # trigsrcDetectorDigitalIn
dwf.FDwfDigitalInTriggerPositionSet(hdwf, c_uint(n - 64))
dwf.FDwfDigitalInTriggerSet(
    hdwf,
    c_uint(0),         # low-level
    c_uint(0),         # high-level
    c_uint(1 << 7),    # rising edge on DIO7
    c_uint(1 << 7),    # falling edge on DIO7
)
dwf.FDwfDigitalInTriggerAutoTimeoutSet(hdwf, c_double(30.0))

dwf.FDwfDigitalInConfigure(hdwf, c_int(1), c_int(1))
print(f"Armed: any DIO7 edge triggers. "
      f"Window = {n} samples @ {actual_hz:.0f} Hz = {n/actual_hz:.1f} s")

sts = c_ubyte()
deadline = time.time() + 50.0
while time.time() < deadline:
    dwf.FDwfDigitalInStatus(hdwf, c_int(1), byref(sts))
    if sts.value == 2:
        break
    time.sleep(0.05)
print(f"status = {sts.value} (2=Done)")

buf = (c_ubyte * n)()
dwf.FDwfDigitalInStatusData(hdwf, buf, c_int(n))

io7 = 1 << 7
rising, falling = [], []
prev = buf[0] & io7
init_level = "HIGH" if prev else "LOW"
final = buf[-1] & io7
final_level = "HIGH" if final else "LOW"
for i in range(1, n):
    cur = buf[i] & io7
    if cur and not prev:
        rising.append(i)
    elif not cur and prev:
        falling.append(i)
    prev = cur

print(f"DIO7 start: {init_level}, end: {final_level}")
print(f"Total edges: {len(rising)} rising, {len(falling)} falling")

if not rising and not falling:
    print("NO EDGES in the whole capture window.")
    dwf.FDwfDeviceClose(hdwf)
    sys.exit(0)

# Group rising edges into bursts (gap > 50 ms)
gap = int(0.050 * actual_hz)
if rising:
    bursts = [[rising[0]]]
    for e in rising[1:]:
        if (e - bursts[-1][-1]) > gap:
            bursts.append([e])
        else:
            bursts[-1].append(e)

    print(f"\n{len(bursts)} burst(s):")
    for i, b in enumerate(bursts):
        t0 = b[0] / actual_hz * 1000.0
        t1 = b[-1] / actual_hz * 1000.0
        print(f"  burst {i+1}: {len(b):3d} pulses   t={t0:8.1f}..{t1:8.1f} ms   dur={t1-t0:7.1f} ms")

    last = bursts[-1]
    if len(last) > 15:
        period_ms = (last[-1] - last[0]) / (len(last) - 1) / actual_hz * 1000.0
        hz = 1000.0 / period_ms if period_ms > 0 else 0.0
        print(f"\nLast burst is continuous ({len(last)} pulses, ~{hz:.1f} Hz aliased)")
        if len(bursts) >= 2:
            print(f"Last phase marker: {len(bursts[-2])} pulses = phase {len(bursts[-2])}")
    else:
        print(f"\nLast burst = phase {len(last)} (firmware hung after this phase)")

dwf.FDwfDeviceClose(hdwf)
