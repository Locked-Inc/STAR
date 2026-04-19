# STAR RX72N Board Configuration

Authoritative reference for the hardware-level jumpers and DIP switches on
the STAR controller board. Get these wrong and the E2 Lite will fail to
attach, flashing will fail, or the chip will silently run in the wrong mode.

Source of truth: `schematic/STAR_MCU.kicad_sch` and
`docs/sections/03_hardware_pinout.tex` (`op-mode-config` /
`emulator-config` tables).

---

## SW1 -- Operating Mode DIP Switch ("OP_CFG")

Part: Mouser `200-TSW10307FS` (4-position SMT DIP), **only 2 positions are
wired** on this board.

| SW1 pos | RX72N pin | Net     | ON means                   | OFF means                 |
|---------|-----------|---------|----------------------------|---------------------------|
| 1       | 16        | MD/FINED| Pin driven **LOW** (boot mode) | Pin pulled **HIGH** (single-chip run) |
| 2       | 60        | PC7/UB  | Pin driven **LOW**             | Pin pulled **HIGH**       |
| 3       | --         | --       | unused                         | unused                    |
| 4       | --         | --       | unused                         | unused                    |

`MD/FINED` is RX72N mode pin 1. `PC7/UB` is the user-boot select used only
when MD has already put the chip in boot mode.

---

## UPSEL_CFG -- 3-pin Jumper

Separate 3-pin header (NOT part of SW1) that drives `P35/UPSEL/NMI`
(RX72N pin 24). Only meaningful in USB boot mode.

| UPSEL_CFG shunt | Effect                     |
|-----------------|----------------------------|
| `1-2`           | USB boot from bus-powered USB-C port |
| `2-3`           | USB boot from self-powered USB-C port |
| open            | USB boot disabled          |

---

## EMLE -- 3-pin Emulator Jumper

Separate 3-pin header that drives the `EMLE` (Emulator Enable) pin. The
E2 Lite **will refuse to connect** unless this is set correctly.

| EMLE shunt | Effect                                                                                 |
|------------|----------------------------------------------------------------------------------------|
| `1-2`      | **Normal / standard debug** -- E2 Lite attaches cleanly after reset. This is the default. |
| `2-3`      | **Hot plug-in debug** -- E2 Lite attaches without resetting the running target.            |
| open       | **DO NOT SET.** Chip will run but E2 Lite cannot attach, and attempting to will hang.     |

---

## Boot Mode Truth Table (SW1 + UPSEL_CFG combined)

| SW1.1 (MD) | SW1.2 (UB) | UPSEL_CFG | Resulting boot mode |
|------------|------------|-----------|---------------------|
| **OFF**    | don't care | don't care| **Single-chip run** -- normal firmware execution. Use this for `make flash` via E2 Lite and for all runtime operation. |
| ON         | OFF        | don't care| SCI boot (serial Renesas Flash Programmer over SCI1 USB-to-UART). |
| ON         | ON         | `1-2`     | USB boot (bus-powered) -- Renesas Flash Programmer over the USB0 USB-C port. |
| ON         | ON         | `2-3`     | USB boot (self-powered) -- same, but the board provides its own power. |

---

## Recommended settings per use case

### Normal operation + `make flash` via E2 Lite (99% of the time) -- DEFAULT

```
SW1.1    = OFF             (MD HIGH  -> single-chip)
SW1.2    = OFF             (UB HIGH  -- doesn't matter in single-chip mode)
UPSEL    = shorted to VCC  (3.3V, held HIGH)
EMLE     = shorted to GND  (held LOW, emulator disabled in normal run)
```

With this configuration the chip comes out of reset running your flashed
firmware, and the E2 Lite attaches for programming and debugging over FINE.

> **This is the default jumper state on the STAR board.** If you're unsure
> what to set, use these positions.

### SCI boot for Renesas Flash Programmer (bypass the E2 Lite)

```
SW1.1 = ON       (MD LOW   -> boot mode)
SW1.2 = OFF      (UB LOW   -> SCI boot)
UPSEL_CFG = any  (ignored for SCI boot)
EMLE      = 1-2  (or 2-3, both work)
```

### USB boot for Renesas Flash Programmer via USB0

```
SW1.1 = ON       (MD LOW   -> boot mode)
SW1.2 = ON       (UB HIGH  -> USB boot)
UPSEL_CFG = 1-2  (bus-powered) or 2-3 (self-powered)
EMLE      = 1-2
```

This connects the RX72N internal USB-boot ROM to the host. Useful for
sanity-checking that the USB0 PHY and pins are electrically good -- if the
chip enumerates as the Renesas Flash Programmer device in this mode, the
USB0 hardware path is correct and any failure in our firmware is a
software issue.

---

## USB Device Enumeration Quirk

The board has **two USB-C connectors** plus the E2 Lite JTAG header:

| Connector | Routes to | Host device |
|-----------|-----------|-------------|
| **SCI/Debug USB-C** | Cypress CY7C65213 USB-UART bridge (SCI9: PB7=TXD9, PB6=RXD9) | `/dev/ttyACM0` |
| **USB0 USB-C** | RX72N USB0 PHY directly (pins 47/48) | `/dev/ttyACMx` (when CDC firmware runs) |
| **E2 Lite header** | Renesas E2 Emulator Lite (FINE debug) | `045b:82a0` in lsusb |

**The Cypress USB-UART disappears while the E2 Lite holds the board in
reset** (during flashing or when first plugged in before firmware runs).
This is normal -- the Cypress is powered from the board's 3.3V rail, not
from its own USB VBUS, so it loses power when the E2 Lite holds the RX72N
in reset.

**Workaround:** flash first (`make flash`), then the board boots, the
Cypress powers up, and `/dev/ttyACM0` reappears within ~1 second. Both the
E2 Lite and the Cypress will coexist fine once the firmware is running.

If the Cypress never appears after flashing, check:
1. The SCI/Debug USB-C cable is plugged into the correct jack (not the USB0
   jack).
2. The cable supports data (not a charge-only cable).
3. Run `sudo dmesg -w` in another terminal and unplug/replug the cable --
   if dmesg shows nothing at all, the cable or jack is dead.

---

## Checking what you currently have set

- **Power down** the board before changing DIP or jumper positions. Live
  changes can damage the E2 Lite or leave the RX72N in an undefined state.
- After any change, do a full power cycle (not just reset).
- Verify by running `make flash` in `blinky_rtos/` -- if that succeeds the
  jumpers are correct for normal operation.
