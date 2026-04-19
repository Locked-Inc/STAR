# RX72N USB0 Bring-up -- SUCCESS (macOS *and* Linux)

**Status**: RX72N USB0 enumerates as VID `0x1209` / PID `0x0001`, Full-Speed USB 2.0,
on both macOS and Linux (Raspberry Pi 5, Ubuntu 24.04, kernel 6.8.0-1051-raspi).

**Working firmware**: `star-rx72n-firmware/usb_test/hoco_pid_fix.c`

**macOS proof (`ioreg -p IOUSB`)**:
```
"Device Speed" = 1       (Full-Speed USB 2.0)
"idVendor"     = 4617    (0x1209, pid.codes test VID)
"idProduct"    = 1       (0x0001)
"kUSBAddress"  = 6       (SET_ADDRESS completed by host)
```

**Linux proof (`lsusb`)**:
```
Bus 001 Device 035: ID 1209:0001 Generic pid.codes Test PID
```

`lsusb -v -d 1209:0001` returns the full device + config + interface descriptors with
no errors (`bcdUSB 2.00`, `bMaxPacketSize0 64`, `wTotalLength 0x0012`,
`bNumInterfaces 1`, `bMaxPower 100mA`).

---

## The 10 Bugs

Bugs 1-9 were found and fixed during the macOS bring-up (2026-04 and earlier).
Bug 10 was found and fixed during the Pi5/Linux bring-up (2026-04-16) -- macOS
silently tolerated the malformed USB transfer that Linux's host stack rejects
with `-75 EOVERFLOW`.

Bugs 1-7 were clock/register issues blocking *any* USB activity. Bugs 8-9 were
the final blockers preventing enumeration from completing on macOS. Bug 10
prevented enumeration on Linux only. All register/bit references cite the
RX72N Hardware User Manual (`docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf`).

### 1. PLLCR at wrong address

`diag_flash.c` wrote the PLL Control Register to `0x8002C`, which is reserved
space. The actual PLLCR is at offset `0x28` = `0x80028`. The PLL was never
configured for MOSC as the source, so `PLOVF` never set.

```c
/* WRONG */   REG16(0x8002C) = 0x1300U;
/* RIGHT */   REG16(0x80028) = 0x1300U;
```

### 2. PLLCR2 at wrong address

Same file wrote PLLCR2 to `0x8002F` (reserved) instead of `0x8002A`. The PLL
was never properly started or stopped via software.

```c
/* WRONG */   REG8(0x8002F) = 0x01U;
/* RIGHT */   REG8(0x8002A) = 0x01U;
```

### 3. USBADDR at wrong offset

SET_ADDRESS handler wrote to `0xA006C`, which is `PIPEMAXP` (Pipe Maximum
Packet Size). The real USBADDR is at offset `0x50` = `0xA0050`.

```c
/* WRONG */   REG16(0xA006C) = val & 0x7FU;
/* RIGHT */   REG16(0xA0050) = val & 0x7FU;
```

### 4. PACKCR.UPLLSEL at bit 12 (not bit 0)

Header comments said UPLLSEL was at bit 0. Manual page 365 shows it is at
**bit 12**. Bit 0 is a reserved-must-be-1 bit.

```c
/* Select PPLL for USB clock */
REG16(0x80044) = (1U << 12) | 1U;   /* UPLLSEL=1 (b12), reserved b0=1 */
```

### 5. PPLLCR3 default wrong for 48 MHz

Default after reset is `0x01` (div-by-2 = 96 MHz from 192 MHz PPLL). For USB we
need 48 MHz, so write `0x03` (div-by-4):

```c
REG8(0x8004B) = 0x03U;   /* PPLLCR3: div-by-4 for USB 48 MHz */
```

### 6. PLLSRCSEL locked while PLL/PPLL running

Manual page 345: *"Writing to PLLCR.PLLSRCSEL while PLLCR2.PLLEN is 0 (PLL is
operating) or PPLLCR2.PPLLEN is 0 (PPLL is operating) is prohibited."*

Previous firmware started HOCO PLL first, which locked PLLSRCSEL=HOCO. All
subsequent attempts to switch to MOSC were silently ignored. Fix: write PLLCR
with the desired source **before starting** either PLL (both are stopped at
reset, so the write is allowed).

### 7. PLL STC multiplication formula

Header comments said `multiply = STC + 1`. Manual page 345 says
`multiply = (STC + 1) / 2` for both PLL and PPLL.

| STC (dec) | Formula `(STC+1)/2` | Manual table |
|-----------|---------------------|--------------|
| 19 (0x13) | x 10.0              | x 10.0 OK    |
| 23 (0x17) | x 12.0              | x 12.0 OK    |

Using `STC+1` would give x20 and x24, double the correct value.

### 8. PID = NAK after SETUP reception (THE first enumeration blocker)

**Manual page 2017, section 40.3.4.6 (4)**:
> "When the function controller is selected: NAK setting -- PID[1:0] = 00b (NAK)
> is set and NAK is returned in response to transactions: When the SETUP token
> is received normally (DCP only)."

The hardware automatically sets `DCPCTR.PID` to NAK after *every* SETUP token.
If firmware then writes response data to CFIFO and sets BVAL without first
restoring PID to BUF, the USB module will return NAK to every subsequent IN
token. The host sees endless NAK or an EPROTO (-71) and eventually gives up.

```c
/* In the CTRT handler, after clearing VALID: */
REG16(DCPCTR) |= 0x0001U;   /* PID = BUF -- hardware forced it to NAK on SETUP */
```

### 9. CCPL on GET_DESCRIPTOR to complete Control Read status stage

For Control Read transfers (GET_DESCRIPTOR), the control transfer sequence is:

```
  SETUP         (host -> device)
  DATA stage    (device -> host, descriptor bytes)
  STATUS stage  (host -> device, zero-length OUT)
```

After firmware writes descriptor data to CFIFO and sets BVAL, it must also set
`DCPCTR.CCPL` so the hardware accepts the zero-length OUT packet in the
status stage. Without CCPL, the host's status-stage OUT is NAK'd, the host
times out, and `DVSQ` stays stuck at Address (2) instead of progressing to
Configured (3).

```c
/* In GET_DESCRIPTOR handler: */
cfifo_write(src, s);           /* write descriptor to CFIFO, sets BVAL */
REG16(DCPCTR) |= (1U << 2);    /* CCPL -- accept zero-length status OUT */
```

### 10. CFIFO 16-bit MBW pads odd-length transfers (Linux-only blocker)

**Manual page 1947, section 40.2.16 (CFIFOSEL.MBW)**:
> "MBW = 0: Byte access (8-bit width); MBW = 1: Word access (16-bit width).
> When word access is selected, two-byte data is transferred per access of CFIFO."

**Manual page 1949, section 40.2.18 (CFIFOCTR.DTLN)**:
> "Receive Data Length: indicates the length of the receive data... For CPU-write
> direction, DTLN is automatically incremented by the access width per write."

The original `cfifo_write()` ran in 16-bit MBW mode and looped `i += 2`,
synthesizing a final padded word for odd-length transfers:

```c
/* WRONG: 16-bit MBW with byte-pair loop */
REG16(CFIFOSEL) = (1U << 5) | (1U << 10);   /* ISEL=1, MBW=16 */
for (uint16_t i = 0; i < len; i += 2U) {
    uint16_t w = data[i];
    if (i + 1U < len) { w |= (uint16_t)data[i + 1U] << 8; }
    REG16(CFIFO) = w;                       /* DTLN += 2 each write */
}
REG16(CFIFOCTR) |= (1U << 15);              /* BVAL: send DTLN bytes */
```

For an odd `len = 9` (Linux's first `GET_DESCRIPTOR(CONFIGURATION)`), this
issues 5 word writes -> hardware DTLN = 10. When the host requested 9 bytes,
the device returned 10. The xHCI host controller flags this as **babble**
(more bytes than the SETUP `wLength`) and the kernel reports
`-75 EOVERFLOW`:

```
usb 1-1.2.1: new full-speed USB device number 29 using xhci_hcd
usb 1-1.2.1: unable to read config index 0 descriptor/start: -75
usb 1-1.2.1: can't read configurations, error -75
```

**Why macOS never hit this**: the macOS USB stack's first `GET_DESCRIPTOR`
on a CONFIGURATION descriptor uses `wLength = 8` (or 18), both even.
Linux uses `wLength = 9` -- exactly the descriptor header length per the USB
spec. The 16-bit MBW path is correct for even transfers and works for the
18-byte device descriptor, so macOS sailed through.

**Fix:** use 8-bit MBW so each write contributes exactly one byte to DTLN
regardless of total length. The DCP can stream individual bytes; only the
non-DCP pipes need 16-bit mode for throughput.

```c
/* RIGHT: 8-bit MBW, one byte per write */
REG16(CFIFOSEL) = (1U << 5);                /* ISEL=1, MBW=0 (8-bit) */
for (uint16_t i = 0; i < len; i++) {
    REG8(CFIFO) = data[i];                  /* DTLN += 1 each write */
}
REG16(CFIFOCTR) |= (1U << 15);
```

---

## How We Found the Final Two (the fun part)

On Linux, the firmware progressed to "device not responding to setup address"
with error `-71` but never fully enumerated. On macOS, nothing appeared in
`ioreg` at all -- the OS silently drops devices that fail enumeration badly.

With no `dmesg`-style visibility into USB protocol errors on macOS, we used
the **Analog Discovery 2** (Digilent FT232H, SN `210321A2AE49`) wired to
RX72N pin 82 (PB3 / AD2 IO7) to observe firmware internal state via GPIO.

### Encoding DVSQ on PB3

`INTSTS0[6:4]` is the Device State Flag (`DVSQ`), reflecting USB enumeration state:

| DVSQ | Meaning       | Set when                              |
|------|---------------|----------------------------------------|
| 0    | Powered       | VBUS present, no bus reset yet         |
| 1    | Default       | Host sent USB bus reset                |
| 2    | Address       | SET_ADDRESS completed successfully     |
| 3    | Configured    | SET_CONFIGURATION completed            |

Firmware encoded DVSQ as a duty cycle on PB3:

```c
uint8_t dvsq = (uint8_t)((REG16(INTSTS0) >> 4) & 0x7U);
if (dvsq >= 3U) { PB3_HIGH(); }                  /* CONFIGURED -- goal state */
else if (dvsq == 2U) { REG8(0x8C02B) ^= 0x08U; } /* toggle -- at Address */
else { PB3_LOW(); }
```

### AD2 capture script

```python
from pydwf import DwfLibrary
from pydwf.utilities import openDwfDevice
import time, numpy as np

dwf = DwfLibrary()
dev = openDwfDevice(dwf, serial_number_filter='210321A2AE49')
dio = dev.digitalIO
dio.outputEnableSet(0x0000)   # all inputs

samples = []
start = time.time()
while time.time() - start < 6.0:
    dio.status()
    samples.append((time.time() - start, (dio.inputStatus() >> 7) & 1))
    time.sleep(0.002)

# avg = 0.0  -> DVSQ < 2  (not enumerating)
# avg ~ 0.5 -> DVSQ = 2   (stuck at Address, GET_DESCRIPTOR failing)
# avg = 1.0 -> DVSQ >= 3  (CONFIGURED, enumeration complete)
```

### What we saw

1. Before fix 8: `avg = 0.5` -- DVSQ stuck at Address state. Host completed
   SET_ADDRESS (hardware auto-responds) but GET_DESCRIPTOR was returning NAK.
2. After fix 8 alone: still `avg = 0.5`. The firmware now had PID=BUF so it
   was *sending* data, but the Control Read status stage never completed.
3. After fix 9 (added CCPL): device appeared in `ioreg` with the correct
   VID/PID, kUSBAddress=6. Done.

---

## How We Found Bug 10 (Linux bring-up, 2026-04-16)

After moving firmware development to a Pi5 (Ubuntu 24.04), the macOS-validated
`hoco_pid_fix.c` would not enumerate. PB3 read `avg = 1.000` -- the *device*
believed it was Configured -- yet `lsusb | grep 1209` returned nothing and
`dmesg` showed:

```
usb 1-1.2.1: new full-speed USB device number 29 using xhci_hcd
usb 1-1.2.1: unable to read config index 0 descriptor/start: -75
usb 1-1.2.1: can't read configurations, error -75
usb 1-1.2-port1: attempt power cycle
usb 1-1.2-port1: unable to enumerate USB device
```

PB3 said the firmware reached DVSQ=3 because the previous enumeration *attempt*
had transiently succeeded through SET_CONFIGURATION on the device's hardware
state machine -- but the host then rejected the next descriptor read and gave up.

The smoking gun came from `usbmon` (kernel USB sniffer):

```bash
sudo modprobe usbmon
sudo cat /sys/kernel/debug/usb/usbmon/1u > /tmp/usbmon.log &
# trigger re-enumeration via re-flash
rfp-cli -d RX72N -t e2l -if fine -run \
  -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a hoco_pid_fix.mot
```

Decoded transactions for one of the failed attempts (device address 29):

```
S Ci:1:029:0 s 80 06 0100 0000 0012 18 <          GET_DESCRIPTOR(DEVICE, 18)
C Ci:1:029:0 0 18 = 12010002 ff000040 09120100 00010000 0001
                                                   ^^^^^^^^ device descriptor OK

S Ci:1:029:0 s 80 06 0600 0000 000a 10 <          GET_DESCRIPTOR(DEV_QUALIFIER, 10)
C Ci:1:029:0 -32 0                                 -32 = STALL (firmware correctly STALLs)

S Ci:1:029:0 s 80 06 0200 0000 0009 9 <           GET_DESCRIPTOR(CONFIG, 9)  <-- ODD!
C Ci:1:029:0 -75 0                                 -75 EOVERFLOW: device sent more than 9
```

`wLength = 9` is the killer. The 16-bit MBW write loop turns 9 bytes into 5
word writes (10 bytes in the FIFO), so DTLN = 10 when the host wanted 9. The
xHCI controller flags this as babble and surfaces it as `-75 EOVERFLOW`.

macOS's xHCI stack (different driver, different first-read length) never asks
with `wLength = 9`, so the same firmware enumerated cleanly there.

After applying bug-10's 8-bit MBW fix and reflashing, the same usbmon capture
shows the 9-byte CONFIG read completing with exactly 9 bytes, then Linux
asks again with `wLength = 18` and the full descriptor returns:

```
S Ci:1:035:0 s 80 06 0200 0000 0009 9 <
C Ci:1:035:0 0 9 = 09021200 01010080 32       <-- exactly 9 bytes
                   ^^ ^^ ^^^^^         ^^
                   |  |  |             bMaxPower (0x32 = 100mA)
                   |  |  wTotalLength = 0x0012 = 18
                   |  bDescriptorType = 2 (CONFIGURATION)
                   bLength = 9
```

The host reads `wTotalLength = 0x0012 = 18`, then issues a second
`GET_DESCRIPTOR(CONFIG, 18)` to fetch the full 18-byte configuration
(9-byte config header + 9-byte interface descriptor).

`lsusb | grep 1209` -> `Bus 001 Device 035: ID 1209:0001 Generic pid.codes Test PID`.

---

## macOS Toolchain Setup

Everything below assumes the board is connected via E2 Lite + USB-C to USB0 port.

### rfp-cli (flash programmer)

Apple Silicon native:

```bash
# Install once (from RFP_CLI_macOS_V32200_arm64.zip downloaded from Renesas)
mkdir -p ~/opt/rfp
unzip RFP_CLI_macOS_V32200_arm64.zip -d /tmp/rfp_install
cp -R /tmp/rfp_install/macos-arm64/* ~/opt/rfp/
chmod +x ~/opt/rfp/rfp-cli

# Flash:
~/opt/rfp/rfp-cli -d RX72N -t e2l -if fine -run \
  -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \
  -a /Users/bsikar/Documents/github/STAR/star-rx72n-firmware/usb_test/hoco_pid_fix.mot
```

### GNU RX toolchain (via Docker)

Docker Desktop for Mac does **not** pass host USB through to containers, so
Docker is only used for building. The container `keen_gates` has the GNU RX
toolchain at `/opt/gnurx/`.

```bash
docker exec keen_gates bash -c 'cd /workspaces/STAR/star-rx72n-firmware/usb_test && \
  /opt/gnurx/bin/rx-elf-gcc -mcpu=rx72t -misa=v3 -mlittle-endian-data \
    -std=gnu23 -O0 -g3 -Wall -Wextra -Wno-unused-parameter \
    -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
    startup.S hoco_pid_fix.c -o hoco_pid_fix.elf && \
  /opt/gnurx/bin/rx-elf-objcopy -O srec hoco_pid_fix.elf hoco_pid_fix.mot'
```

### AD2 via pydwf

```bash
pip3 install --user --break-system-packages pydwf
```

Waveforms SDK comes from the `WaveForms.app` in `/Applications`.

---

## Verify Enumeration

```bash
# Should print a 6-line block with idVendor=4617 (=0x1209) if firmware works
ioreg -p IOUSB -l -w 0 | grep -B1 -A5 '"idVendor" = 4617'
```

---

## Register Quick Reference

| Register  | Address     | Notes                                                    |
|-----------|-------------|----------------------------------------------------------|
| `PRCR`    | `0x803FE`   | 0xA50F = unlock all, 0xA500 = lock                       |
| `PLLCR`   | `0x80028`   | STC[13:8], PLLSRCSEL[4], PLIDIV[1:0]                     |
| `PLLCR2`  | `0x8002A`   | PLLEN[0]: 0=run, 1=stop (inverted!)                      |
| `PPLLCR`  | `0x80048`   | Same layout as PLLCR                                     |
| `PPLLCR2` | `0x8004A`   | Same as PLLCR2                                           |
| `PPLLCR3` | `0x8004B`   | PPLCK[3:0]: 01=/2, 02=/3, 03=/4, 04=/5                   |
| `MEMWAIT` | `0x8101C`   | bit 0: 1 wait-state required for ICLK > 120 MHz          |
| `SCKCR`   | `0x80020`   | ICK[27:24], FCK[31:28], PCKB[11:8], ...                  |
| `SCKCR2`  | `0x80024`   | UCK[7:4] = /(N+1); reserved b0 must be 1                 |
| `SCKCR3`  | `0x80026`   | CKSEL[10:8]: 0=LOCO 1=HOCO 2=MOSC 4=PLL                  |
| `PACKCR`  | `0x80044`   | UPLLSEL[12]: 0=PLL via SCKCR2, 1=PPLL via PPLLCR3        |
| `OSCOVFSR`| `0x8003C`   | MOOVF[0], PLOVF[2], HCOVF[3], PPLOVF[5]                  |
| `MOFCR`   | `0x8C293`   | MODRV2[5:4]: 00=20-24 MHz (default, correct for our xtal)|
| `SYSCFG`  | `0xA0000`   | USBE[0], DPRPU[4], SCKE[10]                              |
| `DCPCTR`  | `0xA0060`   | PID[1:0], CCPL[2], SQCLR[8]                              |
| `USBADDR` | `0xA0050`   | USBADDR[6:0] (writable only when DVCHG=1)                |
| `INTSTS0` | `0xA0040`   | CTSQ[2:0], VALID[3], VBSTS[7], BRDY[8], CTRT[11], DVST[12], DVSQ[6:4] |
| `CFIFO`   | `0xA0014`   | 16-bit FIFO port                                         |
| `CFIFOSEL`| `0xA0020`   | CURPIPE[3:0], ISEL[5], MBW[10] (0=8-bit, 1=16-bit)       |
| `CFIFOCTR`| `0xA0022`   | FRDY[13], BCLR[14], BVAL[15]                             |

---

## Files

- `usb_test/hoco_pid_fix.c` -- **working firmware** (HOCO PLL x12 = 192 MHz, UCK /4 = 48 MHz, PID=BUF + CCPL fixes)
- `usb_test/mosc_pll_fix.c` -- earlier MOSC-crystal variant (also works but CPU-on-PLL-from-MOSC has stability issues; HOCO PLL is the safer choice)
- `usb_test/diag_vbus.c` -- diagnostic-only firmware that encodes DVSQ/VBSTS on PB3 for AD2 capture
- `usb_test/usb_min.c` -- earlier ISR-based attempt (same PID=NAK bug -- would also need the fixes to work)
- `usb_test/clock.c` -- HOCO PLL clock init used by the ThreadX build
- `usb_test/diag_flash.c` -- historical; had bugs 1 and 2 (wrong PLLCR/PLLCR2 addresses)
