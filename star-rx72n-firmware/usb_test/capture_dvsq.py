#!/usr/bin/env python3
"""
Capture PB3 (AD2 IO7) for ~6 seconds to decode RX72N USB DVSQ state.

Per USB_BRINGUP_STATUS.md, hoco_pid_fix.c encodes DVSQ on PB3 such that:
    avg ~ 0.0  -> DVSQ < 2  (not enumerating; PB3 held LOW)
    avg ~ 0.5  -> DVSQ = 2  (stuck at Address; PB3 toggling)
    avg ~ 1.0  -> DVSQ >= 3 (CONFIGURED; PB3 held HIGH -- success)
"""
import time

from pydwf import DwfLibrary
from pydwf.utilities import openDwfDevice

dwf = DwfLibrary()
dev = openDwfDevice(dwf, serial_number_filter="210321A2AE49")
dio = dev.digitalIO
dio.outputEnableSet(0x0000)  # All inputs.

samples = []
start = time.time()
while time.time() - start < 6.0:
    dio.status()
    samples.append((dio.inputStatus() >> 7) & 1)
    time.sleep(0.002)

avg = sum(samples) / len(samples)
print(f"Samples: {len(samples)}, avg PB3 = {avg:.3f}")
if avg >= 0.95:
    print("  -> DVSQ >= 3 (CONFIGURED) -- enumeration complete")
elif avg <= 0.05:
    print("  -> DVSQ < 2 -- not enumerating (Powered or Default)")
elif 0.3 < avg < 0.7:
    print("  -> DVSQ = 2 (Address) -- stuck after SET_ADDRESS, GET_DESCRIPTOR or status stage failing")
else:
    print(f"  -> Mixed state, transitions are happening")
