# AD2 Breakout Board Wiring

Physical flywire map from each Analog Discovery 2 DIO channel to TOM's RX72N
breakout PCB pin numbers (silkscreen labels on the back of the board).

Cross-referenced against `star-rx72n-firmware/gpio_test/pin_map.md` to resolve
each breakout pin number to an RX72N port/pin name.

## Verification status

**AD2 E wired 2026-04-17 -- full-board sweep with all five AD2s not yet
re-run.** Added a fifth unit covering **all of Col 1** plus
**Col 7 Row 1** (PD0). 14 of AD2 E's 16 channels are usable GPIOs;
**DIO 6 (PJ5) and DIO 7 (PJ3) are JTAG TMS/TDO** and are excluded
from the firmware sweep in `main.c` (driving them while E2 Lite has
the debug interface latched hangs the MCU). Total wired-and-verified
pins expected at the next sweep: **76/76**.

**AD2 D wired 2026-04-17.** Covers **Col 9 rows 1-3** (PE2, PE1, PE0)
plus **all of Col 8** (P07, P40-P47, P90-P93).

Re-run `./venv/bin/python3 gpio_verify.py --verbose` to validate all
78 pins end-to-end. AD2 D and AD2 E serials are pending - run
`./venv/bin/python3 gpio_verify.py --auto-map` once with all five
units plugged in, note the two newly-enumerated SNs, and paste them
into `AD2_SERIAL_NUMBERS` in `gpio_verify.py` alongside A/B/C.

**Last full-board sweep: 2026-04-16 -- PASS (46/46 wired pins).** Captured
over 13 s with `gpio_verify.py` against the ThreadX build of gpio_test;
sweep order matched `FIRMWARE_PIN_ORDER`, no noise on the two VCC-rail
channels or the unwired DIO 15 slots.

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
    # AD2 C -- SN 210321A2AE49
    ( 2,  0): "PC3",
    ( 2,  1): "PC4",
    ( 2,  2): "P80",
    ( 2,  3): "P81",
    ( 2,  4): "P82",
    ( 2,  5): "PC5",
    ( 2,  6): "PC6",
    ( 2,  7): "P83",
    ( 2,  8): "P20",
    ( 2,  9): "P17",
    ( 2, 10): "P87",
    ( 2, 11): "P86",
    ( 2, 12): "P15",
    ( 2, 13): "P14",
    ( 2, 14): "P13",
    ( 2, 15): "P12",
    # AD2 D -- SN <pending, run --auto-map>
    ( 3,  0): "PE2",
    ( 3,  1): "PE1",
    ( 3,  2): "PE0",
    ( 3,  3): "P07",
    ( 3,  4): "P40",
    ( 3,  5): "P41",
    ( 3,  6): "P42",
    ( 3,  7): "P43",
    ( 3,  8): "P44",
    ( 3,  9): "P45",
    ( 3, 10): "P46",
    ( 3, 11): "P47",
    ( 3, 12): "P90",
    ( 3, 13): "P91",
    ( 3, 14): "P92",
    ( 3, 15): "P93",
    # AD2 E -- SN <pending, run --auto-map>
    ( 4,  0): "P05",
    ( 4,  1): "P03",
    ( 4,  2): "P02",
    ( 4,  3): "P01",
    ( 4,  4): "P00",
    ( 4,  5): "PF5",
    # ( 4, 6): PJ5 -- JTAG TDO, excluded from firmware sweep
    # ( 4, 7): PJ3 -- JTAG TMS, excluded from firmware sweep
    ( 4,  8): "P33",
    ( 4,  9): "P32",
    ( 4, 10): "P25",
    ( 4, 11): "P24",
    ( 4, 12): "P23",
    ( 4, 13): "P22",
    ( 4, 14): "P21",
    ( 4, 15): "PD0",
}
```

## AD2 C -- SN `210321A2AE49` (index 2)

Wired up Col 3 bottom-to-top for DIO 0-7, then across to Col 2
top-to-bottom for DIO 8-15.

| DIO | Breakout pin | GPIO | Col/Row (pin_map) | Notes |
|-----|--------------|------|-------------------|-------|
|  0  |         67   | PC3  | Col 3, Row 8      | OK    |
|  1  |         66   | PC4  | Col 3, Row 7      | OK    |
|  2  |         65   | P80  | Col 3, Row 6      | OK    |
|  3  |         64   | P81  | Col 3, Row 5      | OK    |
|  4  |         63   | P82  | Col 3, Row 4      | OK    |
|  5  |         62   | PC5  | Col 3, Row 3      | OK    |
|  6  |         61   | PC6  | Col 3, Row 2      | OK    |
|  7  |         58   | P83  | Col 3, Row 1      | OK    |
|  8  |         37   | P20  | Col 2, Row 1      | OK    |
|  9  |         38   | P17  | Col 2, Row 2      | OK    |
| 10  |         39   | P87  | Col 2, Row 3      | OK    |
| 11  |         41   | P86  | Col 2, Row 4      | OK    |
| 12  |         42   | P15  | Col 2, Row 5      | OK    |
| 13  |         43   | P14  | Col 2, Row 6      | OK    |
| 14  |         44   | P13  | Col 2, Row 7      | OK    |
| 15  |         45   | P12  | Col 2, Row 8      | OK    |

### Summary for AD2 C

- 16/16 channels live (no VCC/VSS hits)
- Zero overlap with AD2 A or AD2 B
- Adds Port 8 completion (P80-P83, P86, P87), most of Port C
  (PC3-PC6), plus Port 1 (P12-P17) and P20

### Ready-to-paste CHANNEL_TO_PIN additions

```python
    # AD2 C -- SN 210321A2AE49
    ( 2,  0): "PC3",
    ( 2,  1): "PC4",
    ( 2,  2): "P80",
    ( 2,  3): "P81",
    ( 2,  4): "P82",
    ( 2,  5): "PC5",
    ( 2,  6): "PC6",
    ( 2,  7): "P83",
    ( 2,  8): "P20",
    ( 2,  9): "P17",
    ( 2, 10): "P87",
    ( 2, 11): "P86",
    ( 2, 12): "P15",
    ( 2, 13): "P14",
    ( 2, 14): "P13",
    ( 2, 15): "P12",
```

## AD2 D -- SN `<pending, run --auto-map>` (index 3)

Wired Col 9 top-to-bottom for DIO 0-2 (pins 109/110/111), then across
to Col 8 top-to-bottom for DIO 3-15 (pins 144 down to 127, skipping
the VCC row at 132 and the VSS row at 130 since AD2 D's DIO 12 and
DIO 13 jump over them onto the next GPIO). All 16 channels are live.

| DIO | Breakout pin | GPIO | Col/Row (pin_map) | Notes |
|-----|--------------|------|-------------------|-------|
|  0  |        109   | PE2  | Col 9, Row 1      | OK    |
|  1  |        110   | PE1  | Col 9, Row 2      | OK    |
|  2  |        111   | PE0  | Col 9, Row 3      | OK    |
|  3  |        144   | P07  | Col 8, Row 1      | OK    |
|  4  |        141   | P40  | Col 8, Row 2      | OK    |
|  5  |        139   | P41  | Col 8, Row 3      | OK    |
|  6  |        138   | P42  | Col 8, Row 4      | OK    |
|  7  |        137   | P43  | Col 8, Row 5      | OK    |
|  8  |        136   | P44  | Col 8, Row 6      | OK    |
|  9  |        135   | P45  | Col 8, Row 7      | OK    |
| 10  |        134   | P46  | Col 8, Row 8      | OK    |
| 11  |        133   | P47  | Col 8, Row 9      | OK    |
| 12  |        131   | P90  | Col 8, Row 11     | OK (jumped over VCC pin 132 at Row 10) |
| 13  |        129   | P91  | Col 8, Row 13     | OK (jumped over VSS pin 130 at Row 12) |
| 14  |        128   | P92  | Col 8, Row 14     | OK    |
| 15  |        127   | P93  | Col 8, Row 15     | OK    |

### Summary for AD2 D

- 16/16 channels live (no VCC/VSS hits)
- Zero overlap with AD2 A, B, or C
- Completes Port E (PE0-PE7: PE0/PE1/PE2 here, PE3-PE7 already on AD2 A)
- Completes Port 4 (P40-P47) and the P9x block (P90, P91, P92, P93)
- Adds P07 to round out Port 0 coverage started by Col 1 (still unwired)

### Ready-to-paste CHANNEL_TO_PIN additions

```python
    # AD2 D -- SN <pending>
    ( 3,  0): "PE2",
    ( 3,  1): "PE1",
    ( 3,  2): "PE0",
    ( 3,  3): "P07",
    ( 3,  4): "P40",
    ( 3,  5): "P41",
    ( 3,  6): "P42",
    ( 3,  7): "P43",
    ( 3,  8): "P44",
    ( 3,  9): "P45",
    ( 3, 10): "P46",
    ( 3, 11): "P47",
    ( 3, 12): "P90",
    ( 3, 13): "P91",
    ( 3, 14): "P92",
    ( 3, 15): "P93",
```

## AD2 E -- SN `<pending, run --auto-map>` (index 4)

Wired down Col 1 top-to-bottom for DIO 0-14 (pins 2, 4, 6, 7, 8, 9,
11, 13, 26, 27, 32, 33, 34, 35, 36), then across to Col 7 Row 1 for
DIO 15 (pin 126). All 16 channels are live.

Breakout pins 14-25 are dead rows on Col 1 (VCC/VSS interposed on the
silkscreen between rows 8 and 9) so the DIO 7 -> DIO 8 jump crosses
the physical gap without a separate flywire.

| DIO | Breakout pin | GPIO | Col/Row (pin_map) | Notes |
|-----|--------------|------|-------------------|-------|
|  0  |          2   | P05  | Col 1, Row 1      | OK    |
|  1  |          4   | P03  | Col 1, Row 2      | OK    |
|  2  |          6   | P02  | Col 1, Row 3      | OK    |
|  3  |          7   | P01  | Col 1, Row 4      | OK    |
|  4  |          8   | P00  | Col 1, Row 5      | OK    |
|  5  |          9   | PF5  | Col 1, Row 6      | OK    |
|  6  |         11   | PJ5  | Col 1, Row 7      | **DEAD (JTAG TDO, excluded from firmware sweep)** |
|  7  |         13   | PJ3  | Col 1, Row 8      | **DEAD (JTAG TMS, excluded from firmware sweep)** |
|  8  |         26   | P33  | Col 1, Row 9      | OK (jumped physical gap after Row 8) |
|  9  |         27   | P32  | Col 1, Row 10     | OK    |
| 10  |         32   | P25  | Col 1, Row 11     | OK    |
| 11  |         33   | P24  | Col 1, Row 12     | OK    |
| 12  |         34   | P23  | Col 1, Row 13     | OK    |
| 13  |         35   | P22  | Col 1, Row 14     | OK    |
| 14  |         36   | P21  | Col 1, Row 15     | OK    |
| 15  |        126   | PD0  | Col 7, Row 1      | OK (anchors the Col 7 block) |

### Summary for AD2 E

- 14 of 16 channels carry real GPIO signals during the sweep
- 2 dead channels: **DIO 6 (PJ5)** and **DIO 7 (PJ3)** are JTAG
  TMS/TDO and `main.c` intentionally skips them to avoid hanging the
  MCU while E2 Lite is connected. Verification script logs them as
  "not in FIRMWARE_PIN_ORDER" and they should be left out of
  CHANNEL_TO_PIN. Flywires stay connected so the slot is accounted
  for physically.
- Zero overlap with AD2 A, B, C, or D
- Completes Port 0 lower half (P00-P05; P07 already on AD2 D)
- Completes Port 2 upper half (P21-P25; P20 already on AD2 C)
- Adds Port 3 (P32, P33), PF5
- First pin on Port D (PD0); PD1-PD7 still unwired

### Option: enabling PJ3/PJ5 in the firmware (future work)

If the team wants to reclaim DIO 6 and DIO 7 as real capture channels,
it is possible IF the E2 Lite is physically disconnected before the
sweep starts:

1. Add PJ3 and PJ5 entries to `s_pins[]` in `star-rx72n-firmware/gpio_test/main.c`.
2. Add `"PJ3"` and `"PJ5"` to `FIRMWARE_PIN_ORDER` in
   `gpio_verify.py`.
3. Un-skip `(4, 6)` and `(4, 7)` in `CHANNEL_TO_PIN`.
4. Unplug E2 Lite before running `gpio_verify.py --verbose`.

Not doing that by default because the existing comment in `main.c`
(lines 24-25 and 129-132) is explicit about the hang risk, and the
team's current workflow keeps E2 Lite connected during sweeps for
faster iteration.

### Ready-to-paste CHANNEL_TO_PIN additions

```python
    # AD2 E -- SN <pending>
    ( 4,  0): "P05",
    ( 4,  1): "P03",
    ( 4,  2): "P02",
    ( 4,  3): "P01",
    ( 4,  4): "P00",
    ( 4,  5): "PF5",
    # ( 4, 6): PJ5 -- JTAG TDO, excluded from firmware sweep
    # ( 4, 7): PJ3 -- JTAG TMS, excluded from firmware sweep
    ( 4,  8): "P33",
    ( 4,  9): "P32",
    ( 4, 10): "P25",
    ( 4, 11): "P24",
    ( 4, 12): "P23",
    ( 4, 13): "P22",
    ( 4, 14): "P21",
    ( 4, 15): "PD0",
```

## Identification method used

Unplugged the second AD2, re-enumerated with `pydwf`. Whichever SN
stayed enumerated is AD2 A. `210321A36AA3` was the one that remained,
so the unplugged unit is `210321A36AAE` (AD2 B). AD2 C
(`210321A2AE49`) was added after both A and B were already identified.
AD2 D and AD2 E's serials are pending the next `gpio_verify.py
--auto-map` run with all five units plugged in simultaneously.
