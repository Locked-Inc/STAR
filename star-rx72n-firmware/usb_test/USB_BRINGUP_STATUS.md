# USB0 Bring-up Status

**Date**: 2026-04-16
**Board**: RX72N with 24 MHz crystal (Board 2)
**File**: `star-rx72n-firmware/usb_test/mosc_pll_fix.c` (ready to flash after reboot)

## Root Cause Found: PID=NAK After SETUP

**Hardware Manual p2017, Section 40.3.4.6(4)**:

> "When the function controller is selected... PID[1:0] = 00b (NAK) is set
> when the SETUP token is received normally (DCP only)."

After **every** SETUP token, the RX72N hardware automatically forces the
Default Control Pipe (DCP) response PID to NAK. This means:

1. Host sends SETUP (GET_DESCRIPTOR)
2. Hardware ACKs the SETUP and sets PID = NAK
3. CTRT fires, firmware reads SETUP data, writes descriptor to CFIFO, sets BVAL
4. Host sends IN token for data stage
5. **USB module returns NAK** (because PID=NAK) instead of sending descriptor data
6. Host retries IN, gets NAK again, eventually reports -71 (EPROTO) or -110 (timeout)

**Fix**: After clearing VALID in the CTRT handler, restore PID=BUF:
```c
REG16(DCPCTR) |= 0x0001U;  /* PID = BUF */
```

This fix is already coded in `mosc_pll_fix.c` but untested (USB bus crashed).

The same bug exists in `usb_min.c` -- neither the ISR version nor the polling
version restores PID=BUF after SETUP reception.

## All Bugs Found (8 total)

### 1. PLLCR at wrong address (diag_flash.c)
- **Was**: `REG16(0x8002C)` (offset 0x2C = reserved space)
- **Fix**: `REG16(0x80028)` (offset 0x28 = PLLCR)
- **Impact**: PLL was never configured. PLOVF never set for MOSC source.

### 2. PLLCR2 at wrong address (diag_flash.c)
- **Was**: `REG8(0x8002F)` (offset 0x2F = reserved space)
- **Fix**: `REG8(0x8002A)` (offset 0x2A = PLLCR2)
- **Impact**: PLL was never started/stopped.

### 3. USBADDR at wrong offset
- **Was**: `REG16(0xA006C)` (offset 0x6C = PIPEMAXP)
- **Fix**: `REG16(0xA0050)` (offset 0x50 = USBADDR)
- **Impact**: SET_ADDRESS wrote to pipe config instead of device address register.

### 4. PACKCR.UPLLSEL at wrong bit position
- **Header claimed**: bit 0
- **Manual (p365)**: **bit 12**
- **Fix**: `REG16(0x80044) = (1U << 12) | 1U` for PPLL, or leave default `0x0001` for PLL
- **Impact**: PPLL was never actually selected for USB clock.

### 5. PPLLCR3 default wrong for 48 MHz
- **Default after reset**: 0x01 (div 2 = 96 MHz from 192 MHz PPLL)
- **Fix**: Write 0x03 (div 4 = 48 MHz) to 0x8004B
- **Impact**: If using PPLL path, USB clock was 96 MHz instead of 48 MHz.

### 6. PLLSRCSEL locked while PLL/PPLL running
- **Manual (p345)**: "Writing to PLLCR.PLLSRCSEL while PLLCR2.PLLEN=0 (PLL
  running) or PPLLCR2.PPLLEN=0 (PPLL running) is prohibited."
- **Fix**: Stop both PLL and PPLL before writing PLLCR. Write PLLCR with desired
  source BEFORE starting either PLL.
- **Impact**: Previous firmware started HOCO PLL first, locking PLLSRCSEL=HOCO.
  All subsequent attempts to switch to MOSC were silently ignored.

### 7. PLL STC multiplication formula
- **Header comment claimed**: multiply = STC + 1
- **Manual (p345)**: multiply = **(STC + 1) / 2** for both PLL and PPLL
- **Example**: STC=19 (0x13) gives x10, NOT x20. PLLCR=0x1300 with 24 MHz = 240 MHz.
- **Impact**: Wrong STC values would give wrong PLL frequency.

### 8. PID=NAK after SETUP (THE critical enumeration bug)
- **Manual (p2017)**: Hardware sets PID=NAK when SETUP token received on DCP
- **Fix**: `REG16(DCPCTR) |= 0x0001U` after clearing VALID in CTRT handler
- **Impact**: Device never sends data in response to GET_DESCRIPTOR.

## Additional Manual Findings

### Data Toggle (p2018, sec 40.3.4.7)
"When the function controller has been selected and control transfer is used,
the USB automatically sets the sequence bit when a stage transition is made.
**Software settings are not required.**"

SQCLR/SQSET are NOT needed for control transfers in device mode. The hardware
handles DATA0/DATA1 toggling automatically.

### SCKCR2 UCK Duty Cycle (p343)
- UCK /3: duty cycle 2:1
- UCK /5: duty cycle 3:2
- UCK /2 and /4: 50/50 duty cycle

Use /4 (not /5) for clean USB signaling.

### MEMWAIT (p341)
"Set MEMWAIT to 1 if ICLK > 120 MHz." At exactly 120 MHz, MEMWAIT=0 is fine.

### MOSC PLL + SCKCR3 switch crashes CPU
Consistently reproducible. HOCO PLL (192 MHz) + SCKCR3 switch works fine.
MOSC PLL (192 MHz, same frequency) + SCKCR3 switch crashes. Root cause unknown
but may be related to PLLSRCSEL not actually changing (bug #6). Workaround: keep
CPU on HOCO, use PLL only for USB clock via SCKCR2.

## Register Quick Reference

| Register | Address | Key Bits |
|----------|---------|----------|
| PRCR | 0x803FE | 0xA50F=unlock all, 0xA500=lock |
| PLLCR | 0x80028 | STC[13:8], PLLSRCSEL[4], PLIDIV[1:0] |
| PLLCR2 | 0x8002A | PLLEN[0] (0=run, 1=stop, inverted!) |
| PPLLCR | 0x80048 | Same layout as PLLCR |
| PPLLCR2 | 0x8004A | Same as PLLCR2 |
| PPLLCR3 | 0x8004B | PPLCK[3:0]: 01=/2, 02=/3, 03=/4, 04=/5 |
| MEMWAIT | 0x8101C | bit 0: 0=no wait, 1=one wait (for ICLK>120MHz) |
| SCKCR | 0x80020 | ICK[27:24], FCK[31:28], PCKB[11:8], etc |
| SCKCR2 | 0x80024 | UCK[7:4] (div N+1), reserved b0=1 |
| SCKCR3 | 0x80026 | CKSEL[10:8]: 0=LOCO,1=HOCO,2=MOSC,4=PLL |
| PACKCR | 0x80044 | UPLLSEL[12] (0=PLL, 1=PPLL), reserved b0=1 |
| OSCOVFSR | 0x8003C | MOOVF[0], PLOVF[2], HCOVF[3], PPLOVF[5] |
| MOFCR | 0x8C293 | MODRV2[5:4]: 00=20-24MHz (default, correct) |
| SYSCFG | 0xA0000 | USBE[0], DPRPU[4], SCKE[10] |
| DCPCTR | 0xA0060 | PID[1:0], CCPL[2], SQCLR[8] |
| USBADDR | 0xA0050 | USBADDR[6:0], writable when DVCHG=1 |
| INTSTS0 | 0xA0040 | CTSQ[2:0], VALID[3], BRDY[8], CTRT[11], DVST[12] |
| CFIFO | 0xA0014 | 16-bit FIFO data port |
| CFIFOSEL | 0xA0020 | CURPIPE[3:0], ISEL[5], MBW[10] |
| CFIFOCTR | 0xA0022 | FRDY[13], BCLR[14], BVAL[15] |

## Current Firmware State

`mosc_pll_fix.c` is ready to flash with:
- MOSC crystal -> PLL x8 = 192 MHz (PLLSRCSEL set before PLL starts)
- CPU stays on HOCO (16 MHz) -- MOSC PLL SCKCR3 switch crashes
- USB clock: PLL 192 MHz / 4 = 48 MHz via SCKCR2
- PID=BUF restored after each SETUP reception (THE critical fix)
- Production-matching init sequence (USBE before SCKE, 10ms delays)
- VBUS pin MPC configured (P1.6 = USB0_VBUS)

## Current Build Status (macOS)

Firmware BUILT with PID=NAK fix at:
`/Users/bsikar/Documents/github/STAR/star-rx72n-firmware/usb_test/mosc_pll_fix.mot` (4572 bytes)

Built via Docker container `keen_gates` which has the GNU RX toolchain at `/opt/gnurx/`.

**Blocker**: Can't flash from macOS.
- No native macOS rfp-cli (Renesas provides Linux/Windows only)
- Docker Desktop on macOS doesn't expose USB to containers
- E2 Lite (0x045B:0x82a0) visible on macOS host (via ioreg) but no driver

## Build Command (macOS via Docker)

```bash
docker exec keen_gates bash -c 'cd /workspaces/STAR/star-rx72n-firmware/usb_test && \
  /opt/gnurx/bin/rx-elf-gcc -mcpu=rx72t -misa=v3 -mlittle-endian-data \
    -std=gnu23 -O0 -g3 -Wall -Wextra -Wno-unused-parameter \
    -nostartfiles -Wl,-e_PowerON_Reset_PC -T linker.ld \
    startup.S mosc_pll_fix.c -o mosc_pll_fix.elf && \
  /opt/gnurx/bin/rx-elf-objcopy -O srec mosc_pll_fix.elf mosc_pll_fix.mot'
```

## Flash Options

1. **Plug E2 Lite into a Linux machine and flash**:
   ```bash
   sudo /opt/rfp/linux-x64/rfp-cli -d RX72N -t e2l -if fine -run \
     -auth id FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -a mosc_pll_fix.mot
   ```

2. **Linux VM with USB passthrough** (Parallels/VMware/UTM).
   UTM is free and handles USB passthrough well.

3. **e2 studio IDE** if installed on macOS -- supports E2 Lite flashing.

## Verification After Flash

```bash
# macOS -- check for RX72N USB device (VID 0x1209, PID 0x0001)
system_profiler SPUSBDataType 2>/dev/null | grep -A3 "1209"
ioreg -p IOUSB -l -w 0 | grep -B2 -A2 "4617"  # 0x1209 decimal
```

## Files Modified

- `usb_test/mosc_pll_fix.c` -- standalone bare-metal USB firmware (primary test file)
- `usb_test/clock.c` -- modified but may need cleanup (was testing MOSC PLL)
- `usb_test/diag_flash.c` -- original firmware with bugs #1 and #2 (address errors)
