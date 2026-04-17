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

## AD2 A -- SN `210321A36AA3` (index 0)

Identified by unplugging the second unit and re-running `pydwf`
enumeration: only `210321A36AA3` remained. Maps to index `0` in
`AD2_SERIAL_NUMBERS` in `gpio_verify.py`.

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

### Ready-to-paste CHANNEL_TO_PIN

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

## AD2 B -- SN `210321A36AAE` (index 1)

Wired down Col 5 (DIO 0-10, pins 90-80) then jumped across to Col 3
(DIO 11-15, pins 72-68, top-of-stack first). Plug AD2 B back in
before running `gpio_verify.py` so both devices enumerate.

| DIO | Breakout pin | GPIO | Col/Row (pin_map) | Notes |
|-----|--------------|------|-------------------|-------|
|  0  |         90   | PA5  | Col 5, Row 4      | OK    |
|  1  |         89   | PA6  | Col 5, Row 5      | OK    |
|  2  |         88   | PA7  | Col 5, Row 6      | OK    |
|  3  |         87   | PB0  | Col 5, Row 7      | OK    |
|  4  |         86   | P71  | Col 5, Row 8      | OK    |
|  5  |         85   | P72  | Col 5, Row 9      | OK    |
|  6  |         84   | PB1  | Col 5, Row 10     | OK    |
|  7  |         83   | PB2  | Col 5, Row 11     | OK    |
|  8  |         82   | PB3  | Col 5, Row 12     | OK    |
|  9  |         81   | PB4  | Col 5, Row 13     | OK    |
| 10  |         80   | PB5  | Col 5, Row 14     | OK    |
| 11  |         72   | P74  | Col 3, Row 13     | OK    |
| 12  |         71   | P75  | Col 3, Row 12     | OK    |
| 13  |         70   | PC2  | Col 3, Row 11     | OK    |
| 14  |         69   | P76  | Col 3, Row 10     | OK    |
| 15  |         68   | P77  | Col 3, Row 9      | OK    |

### Summary for AD2 B

- 16/16 channels hit real GPIOs (no VCC/VSS rails)
- Zero overlap with AD2 A's pins -- 30 unique GPIOs covered between
  the two AD2s (14 from A + 16 from B)

### Ready-to-paste CHANNEL_TO_PIN additions

Merge these entries into the `CHANNEL_TO_PIN` dict alongside the AD2 A
entries above.

```python
    # AD2 B
    ( 1,  0): "PA5",
    ( 1,  1): "PA6",
    ( 1,  2): "PA7",
    ( 1,  3): "PB0",
    ( 1,  4): "P71",
    ( 1,  5): "P72",
    ( 1,  6): "PB1",
    ( 1,  7): "PB2",
    ( 1,  8): "PB3",
    ( 1,  9): "PB4",
    ( 1, 10): "PB5",
    ( 1, 11): "P74",
    ( 1, 12): "P75",
    ( 1, 13): "PC2",
    ( 1, 14): "P76",
    ( 1, 15): "P77",
```

## Combined CHANNEL_TO_PIN (paste into gpio_verify.py)

```python
CHANNEL_TO_PIN = {
    # AD2 A -- SN 210321A36AA3
    ( 0,  0): "PE3",
    ( 0,  1): "PE4",
    ( 0,  2): "PE5",
    # ( 0,  3): VCC -- dead channel, skip
    ( 0,  4): "P70",
    # ( 0,  5): VCC -- dead channel, skip
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
    # AD2 B -- SN 210321A36AAE
    ( 1,  0): "PA5",
    ( 1,  1): "PA6",
    ( 1,  2): "PA7",
    ( 1,  3): "PB0",
    ( 1,  4): "P71",
    ( 1,  5): "P72",
    ( 1,  6): "PB1",
    ( 1,  7): "PB2",
    ( 1,  8): "PB3",
    ( 1,  9): "PB4",
    ( 1, 10): "PB5",
    ( 1, 11): "P74",
    ( 1, 12): "P75",
    ( 1, 13): "PC2",
    ( 1, 14): "P76",
    ( 1, 15): "P77",
}
```

## AD2 C -- SN `210321A2AE49` (index 2)

Third unit, located after AD2 A and AD2 B were mapped. Not yet wired.
Gives us 48 total DIO channels, enough to cover ~half of the firmware's
100 pins per capture pass.

## Identification method used

Unplugged the second AD2, re-enumerated with `pydwf`. Whichever SN
stayed enumerated is AD2 A. `210321A36AA3` was the one that remained,
so the unplugged unit is `210321A36AAE` (AD2 B). AD2 C
(`210321A2AE49`) was added after both A and B were already identified.
