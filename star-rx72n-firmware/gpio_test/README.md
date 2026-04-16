# GPIO Pin Test (gpio_test)

Automated GPIO verification for the STAR RX72N board using 3x Digilent
Analog Discovery 2 (48 channels total: IO 0-15 on each AD2).

## Hardware Setup

- **MCU**: R5F572NNHDFB (RX72N, 145-pin LFBGA, lot code 502AZ00)
- **Breakout board**: Custom board that exposes physical BGA pins to
  header pins for AD2 probing
- **Test instruments**: 3x Analog Discovery 2 (Digilent), each with
  IO 0-15 digital channels

## Pin Map

Physical BGA pins exposed on the breakout board and their AD2 channel
assignments will be documented here once confirmed.

```
AD2 #1 (serial: TODO)     AD2 #2 (serial: TODO)     AD2 #3 (serial: TODO)
IO 0  = pin ???            IO 0  = pin ???            IO 0  = pin ???
IO 1  = pin ???            IO 1  = pin ???            IO 1  = pin ???
...                        ...                        ...
IO 15 = pin ???            IO 15 = pin ???            IO 15 = pin ???
```

## Test Strategy

1. **Firmware** (`gpio_test/`): cycles through each testable GPIO pin,
   driving it HIGH then LOW with a known timing pattern.
2. **Host script** (Python + WaveForms SDK): captures the AD2 digital
   inputs and verifies the expected toggling pattern on each channel.
3. **Report**: pass/fail per pin, identifies dead/shorted/wrong-pin
   connections.

## Files

```
gpio_test/
  README.md       -- this file
  main.c          -- firmware: sequential GPIO toggle (to be written)
  Makefile         -- build + flash (to be written)
  startup.S       -- reuse from blinky_rtos
  linker.ld       -- reuse from blinky_rtos
  vectors.S       -- minimal (CMT0 + SWINT only, no USB)
  clock.c         -- reuse from usb_test (24 MHz EXTAL -> PLL)
  cmt0.c          -- reuse from usb_test (100 Hz tick)
  host/
    gpio_verify.py -- AD2 capture + verify script (to be written)
```
