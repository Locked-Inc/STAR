# AD2 Breakout Board Wiring

Physical flywire map from each Analog Discovery 2 DIO channel to TOM's RX72N
breakout PCB pin numbers (silk-screen labels on the back of the board).

Cross-referenced against `star-rx72n-firmware/gpio_test/pin_map.md` to resolve
each breakout pin number to an RX72N port/pin name.

> **Source of truth: TOM's physical breakout PCB silkscreen, not the KiCad
> schematic.** TOM's breakout is an independent board whose layout may drift
> from anything in `Schematic/*.kicad_sch`. If in doubt, trust the silkscreen
> on the actual board and `pin_map.md` (which was produced from that
> silkscreen). Do not cross-check against the KiCad schematic -- it can be
> slightly different.

## AD2 A (unit identity TBD)

Wired first. Serial number not yet confirmed -- run `gpio_verify.py --auto-map`
after the firmware is flashed to match this bundle to either SN
`210321A36AA3` (AD2 #0) or `210321A36AAE` (AD2 #1).

| DIO | Breakout pin | GPIO   | Col/Row (pin_map) | Notes                    |
|-----|--------------|--------|-------------------|--------------------------|
|  0  |        108   | PE3    | Col 6, Row 1      | OK                       |
|  1  |        107   | PE4    | Col 6, Row 2      | OK                       |
|  2  |        106   | PE5    | Col 6, Row 3      | OK                       |
|  3  |        105   | **VCC**| Col 6, Row 4      | **power rail, no signal**|
|  4  |        104   | P70    | Col 6, Row 5      | OK                       |
|  5  |        103   | **VCC**| Col 6, Row 6      | **power rail, no signal**|
|  6  |        102   | PE6    | Col 6, Row 7      | OK                       |
|  7  |        101   | PE7    | Col 6, Row 8      | OK                       |
|  8  |        100   | P65    | Col 6, Row 9      | OK                       |
|  9  |         99   | P66    | Col 6, Row 10     | OK                       |
| 10  |         98   | P67    | Col 6, Row 11     | OK                       |
| 11  |         97   | PA0    | Col 6, Row 12     | OK                       |
| 12  |         96   | PA1    | Col 6, Row 13     | OK                       |
| 13  |         95   | PA2    | Col 6, Row 14     | OK                       |
| 14  |         94   | PA3    | Col 6, Row 15     | OK                       |
| 15  |         92   | PA4    | Col 5, Row 2      | OK (top of Col 5)        |

### Summary for AD2 A

- 14 usable GPIO signals captured
- 2 dead channels: **DIO 3** and **DIO 5** are wired to VCC rails
  (pins 105 and 103 on the breakout are labelled `VCC`, not GPIOs)
- To recover DIO 3 and DIO 5: unplug those flywires and move them to any
  unused GPIO header. Good nearby candidates:
  - Pin  109 = PE2 (Col 9 row 1)
  - Pin  110 = PE1 (Col 9 row 2)
  - Pin  111 = PE0 (Col 9 row 3)

### Ready-to-paste CHANNEL_TO_PIN (once AD2 identity is known)

Assuming this is AD2 index 0 (first in `AD2_SERIAL_NUMBERS`). If auto-map
reports index 1 instead, change the first tuple element from `0` to `1`.

```python
CHANNEL_TO_PIN = {
    ( 0,  0): "PE3",
    ( 0,  1): "PE4",
    ( 0,  2): "PE5",
    # (0, 3): "VCC" -- not a GPIO, skip
    ( 0,  4): "P70",
    # (0, 5): "VCC" -- not a GPIO, skip
    ( 0,  6): "PE6",
    ( 0,  7): "PE7",
    ( 0,  8): "P65",
    ( 0,  9): "P66",
    ( 0, 10): "P67",
    ( 0, 11): "PA0",
    ( 0, 12): "PA1",
    ( 0, 13): "PA2",
    ( 0, 14): "PA3",
    ( 0, 15): "PA4",
}
```

## AD2 B (not yet wired)

TBD.

## How to identify which physical AD2 is which

Three options:

1. **Read the label** on the back of the AD2 -- the serial number matches
   one of `210321A36AA3` or `210321A36AAE`.
2. **Run auto-map** after flashing the firmware:
   `./venv/bin/python3 gpio_verify.py --auto-map`. Whichever AD2 index
   shows rising edges on DIO 0-2, 4, 6-15 (not 3 or 5) is AD2 A.
3. **Blink test**: in WaveForms desktop, open Logic Analyser for one AD2
   at a time; when the firmware is running you'll see toggles only on
   the AD2 that's physically wired.
