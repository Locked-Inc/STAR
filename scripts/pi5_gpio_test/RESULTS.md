# Pi5 GPIO sweep -- first run

Date: 2026-04-18

Setup:
- 2x Analog Discovery 2 on the Mac
  - unit 0 (SN 210321A2AE49): DIO 0..15 wired
  - unit 1 (SN 210321A36AAE): DIO 0..11 wired, DIO 12..15 floating
- Pi5 running `scripts/pi5_gpio_test/pi5_toggle.py` on a monitor
- HAT on top: Hailo AI Hat+
- Toggler skips GPIO 0, 1, 2, 3, 7, 8 (claimed by HAT EEPROM / I2C / SPI)
- 22 GPIOs should sweep; 28 wires plugged onto the 40-pin header

## Results

### Detected (15 of 22)

| GPIO | unit | DIO |
|------|------|-----|
|   4  | 0    | 12  |
|   5  | 0    |  5  |
|   6  | 0    |  6  |
|   9  | 0    |  0  |
|  10  | 1    | 11  |
|  11  | 0    |  2  |
|  12  | 1    |  4  |
|  13  | 0    | 11  |
|  16  | 1    |  3  |
|  17  | 0    |  1  |
|  19  | 0    |  4  |
|  20  | 1    |  0  |
|  21  | 1    |  1  |
|  22  | 0    |  9  |
|  26  | 0    |  7  |

### Not detected (7 of 22)

| GPIO | Header pin | Notes                                           |
|------|------------|-------------------------------------------------|
|  14  | 8          | UART0 TXD (free -- see "Serial console" below)  |
|  15  | 10         | UART0 RXD (free -- see "Serial console" below)  |
|  18  | 12         |                                                 |
|  23  | 16         |                                                 |
|  24  | 18         |                                                 |
|  25  | 22         |                                                 |
|  27  | 13         |                                                 |

### Serial console state (verified on this Pi5)

- `/boot/firmware/cmdline.txt`: `console=tty1` only (no `serial0`/`ttyAMA0`)
- `/boot/firmware/config.txt`: no `enable_uart=1`, no UART overlay
- `systemctl`: no `serial-getty@` unit active
- `gpioinfo gpiochip4`: lines 14 and 15 report `unused`

So UART0 is not claiming GPIO 14/15. The missing edges on those pins
are a wiring issue, not a kernel one.

### Crosstalk artifact

unit 1 DIO 12 was supposed to be unwired. It showed 1 rising edge (vs
2 expected per cycle) at the same time as GPIO 10 on unit 1 DIO 11.
This is capacitive pickup from the adjacent wired DIO 11, not a real
connection. Passive wiring check earlier confirmed DIO 12..15 are
floating.

## Interpretation

28 wires on the header, 22 sweepable GPIOs. 15 confirmed working,
13 wires unaccounted for. The 13 are almost certainly landing on the
40-pin header's non-GPIO positions:

- 2x 3V3 (pins 1, 17)
- 2x 5V (pins 2, 4)
- 8x GND (pins 6, 9, 14, 20, 25, 30, 34, 39)
- 6x HAT-claimed GPIOs (0, 1, 2, 3, 7, 8 -- on pins 27, 28, 3, 5, 26, 24)

That is 18 non-sweepable pins total, 7 of which would explain the
missing GPIOs if wires meant for them ended up on power/ground/HAT
pins instead.

## Next step

Move wires off power/ground/HAT pins onto the 7 missing GPIOs:

| GPIO | Header pin position |
|------|---------------------|
|  14  | 8                   |
|  15  | 10                  |
|  18  | 12                  |
|  27  | 13                  |
|  23  | 16                  |
|  24  | 18                  |
|  25  | 22                  |

Re-run:

```
# On the Pi5 (if not already looping):
sudo python3 scripts/pi5_gpio_test/pi5_toggle.py

# On the Mac:
star-rx72n-firmware/gpio_test/host/venv/bin/python3 \
    scripts/pi5_gpio_test/ad2_verify.py --verbose
```

Serial console is already off (see "Serial console state" above), so
GPIO 14/15 should come up with wiring alone. If they still miss after
re-wiring, re-check the probe, not the kernel config.
