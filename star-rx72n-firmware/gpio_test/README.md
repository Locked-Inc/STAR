# GPIO Pin Test (gpio_test)

Automated GPIO verification for the STAR RX72N breakout board using 3x
Digilent Analog Discovery 2 (48 channels total: DIO 0-15 on each AD2).
Implementation is polling-based; no RTOS is used so the MCU boot path
stays trivial.

## Hardware Setup

- **MCU**: R5F572NNHDFB (RX72N, 145-pin LFBGA, lot code 502AZ00)
- **Breakout board**: TOM's custom PCB that exposes 100 GPIO pins from
  the 144-pin BGA package to header pins for AD2 probing
- **Test instruments**: 3x Analog Discovery 2 (Digilent), each with
  IO 0-15 digital channels

### Analog Discovery 2 Devices

Verified 2026-04-15 via `pydwf` 1.1.19 + `libdwf.so` 3.24.3.

These show up in `lsusb` as FT232H (FTDI VID `0403:6014`).

| Device | Serial Number  | Status |
|--------|----------------|--------|
| AD2 #0 | 210321A2AE49   | OK     |
| AD2 #1 | 210321A36AA3   | OK     |
| AD2 #2 | 210321A36AAE   | OK     |

Open by serial number:
```python
from pydwf import DwfLibrary
from pydwf.utilities import openDwfDevice

dwf = DwfLibrary()
dev = openDwfDevice(dwf, serial_number_filter="210321A2AE49")
dev.digitalIO.status()
val = dev.digitalIO.inputStatus()  # 16-bit DIO read
dev.close()
```

## Pin Map

See [pin_map.md](pin_map.md) for the full breakout board pin-to-GPIO
mapping (extracted from `schematic/TOM/TOM_MCU_144Pin.kicad_sch`).

100 GPIO pins broken out across 9 header columns. Pin numbers match the
back silkscreen labels on TOM's PCB.

## AD2 Channel Assignments

Physical BGA pins exposed on the breakout board and their AD2 channel
assignments will be documented here once wiring is confirmed.

Edit `host/gpio_verify.py` `CHANNEL_TO_PIN` dict to map each AD2
DIO channel to its connected GPIO pin name.

```
AD2 #0 (SN: 210321A2AE49)    AD2 #1 (SN: 210321A36AA3)    AD2 #2 (SN: 210321A36AAE)
IO 0  = pin ???               IO 0  = pin ???               IO 0  = pin ???
IO 1  = pin ???               IO 1  = pin ???               IO 1  = pin ???
...                           ...                           ...
IO 15 = pin ???               IO 15 = pin ???               IO 15 = pin ???
```

## Test Strategy

1. **Firmware** (`gpio_test/`): sequentially drives each of 98 GPIO pins
   (all breakout GPIOs except PJ3/PJ5, which are JTAG TMS/TDO and hang
   the MCU when driven while the E2 Lite is attached). Each pin pulses
   HIGH for ~5 ms then LOW for ~5 ms using a nop busy-wait at ICLK 96
   MHz. After a full sweep there is a long quiet gap (~40x one pin
   period) so the host can anchor timing to it.

2. **Host script** (`host/gpio_verify.py`): captures the AD2 digital
   inputs at 1 kHz, detects rising/falling edges, and matches them to
   the known firmware pin sequence.

3. **Report**: pass/fail per pin, identifies dead/shorted/wrong-pin
   connections.

## Files

```
gpio_test/
  README.md       -- this file
  pin_map.md      -- breakout board pin-to-GPIO mapping table
  main.c          -- firmware: sequential GPIO toggle (98 pins)
  Makefile        -- build + flash
  startup.S       -- RX72N startup
  linker.ld       -- linker script
  vectors.S       -- minimal vector table (all slots to _default_isr)
  clock.c         -- HOCO 16 MHz -> PLL 192 MHz clock init
  host/
    gpio_verify.py -- AD2 capture + verify script
```

## Build

```bash
cd star-rx72n-firmware/gpio_test
make            # builds gpio_test.elf/.mot/.hex
make flash      # flash via E2 Lite + rfp-cli
make clean      # remove build artifacts
```

## Usage

```bash
# 1. Flash firmware to the RX72N
make flash

# 2. Wire AD2 probes to breakout board headers
#    Update CHANNEL_TO_PIN in host/gpio_verify.py

# 3. Run verification
cd host
python3 gpio_verify.py --verbose

# Dry run (print config, no capture)
python3 gpio_verify.py --dry-run
```

## Dependencies

- **Firmware**: GNURX toolchain (rx-elf-gcc). Use the dev container:
  `devcontainer exec --workspace-folder <repo> bash -lc 'cd star-rx72n-firmware/gpio_test && make'`.
- **Flash**: macOS-native `rfp-cli` at `~/tools/rfp-cli/bin/rfp-cli`.
- **Host script**: Python 3.8+, pydwf >= 1.1.19, libdwf >= 3.25.1
  (WaveForms runtime installed to `/Library/Frameworks/dwf.framework`).
