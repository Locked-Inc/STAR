# KiCad Netclass Color Reference

This document describes the color scheme mapping between the LaTeX documentation and KiCad netclass configuration.

## Color Calculation Formula

LaTeX uses the notation `color!X` where:
- X% is the base color intensity
- (100-X)% is white (255, 255, 255)
- Formula: `RGB = (base_color * X/100) + (255 * (100-X)/100)`

## Color Mappings

### 1. Debug/Programming (Yellow 15%)
- **LaTeX**: `yellow!15`
- **Base RGB**: (255, 255, 0)
- **Calculated RGB**: (255, 249, 217)
- **Signals**: SCI9_TXD9, SCI9_RXD9, USB0_*, EMU_* (JTAG/Debug)
- **KiCad Netclass**: `Debug/Programming`

### 2. Power/Ground (Gray 15%)
- **LaTeX**: `gray!15`
- **Base RGB**: (128, 128, 128)
- **Calculated RGB**: (217, 217, 217)
- **Signals**: GND, VCC, +3V3, +5V
- **KiCad Netclass**: `Power/Ground`

### 3. RPi5 SPI (Orange 20%)
- **LaTeX**: `orange!20`
- **Base RGB**: (255, 165, 0)
- **Calculated RGB**: (255, 204, 153)
- **Signals**: RPI5_RSPIA_SCLK, RPI5_RSPIA_COPI, RPI5_RSPIA_CIPO, RPI5_RSPIA_CS
- **KiCad Netclass**: `RPi5 SPI`

### 4. Motor PWM Control (Blue 20%)
- **LaTeX**: `blue!20`
- **Base RGB**: (0, 0, 255)
- **Calculated RGB**: (204, 204, 255)
- **Signals**: MOTOR0-3_PH, MOTOR0-3_EN (GPTW PWM signals)
- **KiCad Netclass**: `Motor PWM Control`

### 5. Motor Fault Signals (Lime 10%)
- **LaTeX**: `lime!10`
- **Base RGB**: (128, 255, 128)
- **Calculated RGB**: (230, 255, 230)
- **Signals**: MOTOR0-3_nFAULT (DRV8243S fault outputs)
- **KiCad Netclass**: `Motor Fault Signals`

### 6. Motor Current Sensing (Red 15%)
- **LaTeX**: `red!15`
- **Base RGB**: (255, 0, 0)
- **Calculated RGB**: (255, 217, 217)
- **Signals**: MOTOR0-3_IPROPI (ADC current sensing)
- **KiCad Netclass**: `Motor Current Sensing`

### 7. Encoders (Green 20%)
- **LaTeX**: `green!20`
- **Base RGB**: (0, 255, 0)
- **Calculated RGB**: (204, 255, 204)
- **Signals**: ENCODER0-3_PHASE_A, ENCODER0-3_PHASE_B (MTU quadrature)
- **KiCad Netclass**: `Encoders`

### 8. Motor Driver SPI Config (Purple 20%)
- **LaTeX**: `purple!20`
- **Base RGB**: (128, 0, 128)
- **Calculated RGB**: (229, 204, 229)
- **Signals**: RSPIC_SCLK, RSPIC_COPI, RSPIC_CIPO, RSPIC_MOTOR0-3_nCS
- **KiCad Netclass**: `Motor Driver SPI Config`
- **Note**: Actual calculated color (229, 204, 229) adjusted to (229, 204, 255) for better visual distinction

### 9. I2C Charger (Teal 20%)
- **LaTeX**: `teal!20`
- **Base RGB**: (0, 255, 255)
- **Calculated RGB**: (204, 255, 255)
- **Signals**: CHARGER_I2C_CLOCK, CHARGER_I2C_DATA (RIIC1)
- **KiCad Netclass**: `I2C Charger`

### 10. SMBus BMS (Violet 15%)
- **LaTeX**: `violet!15`
- **Base RGB**: (148, 0, 211)
- **Calculated RGB**: (238, 217, 238)
- **Signals**: BMS_SMBUS_CLK, BMS_SMBUS_DATA (RIIC0 FM+)
- **KiCad Netclass**: `SMBus BMS`

### 11. PMOD JB (Brown 35%)
- **LaTeX**: `brown!35`
- **Base RGB**: (165, 42, 42)
- **Calculated RGB**: (194, 166, 128)
- **Signals**: PMOD_JB_* (I2C Sensor Hub)
- **KiCad Netclass**: `PMOD JB`

### 12. PMOD JA (Olive 30%)
- **LaTeX**: `olive!30`
- **Base RGB**: (128, 128, 0)
- **Calculated RGB**: (206, 229, 179)
- **Signals**: PMOD_JA_* (High-Speed SPI/Display)
- **KiCad Netclass**: `PMOD JA`

### 13. PMOD JC (Cyan 35%)
- **LaTeX**: `cyan!35`
- **Base RGB**: (0, 255, 255)
- **Calculated RGB**: (166, 255, 255)
- **Signals**: PMOD_JC_* (UART/Wireless)
- **KiCad Netclass**: `PMOD JC`

### 14. Status LEDs (Magenta 15%)
- **LaTeX**: `magenta!15`
- **Base RGB**: (255, 0, 255)
- **Calculated RGB**: (255, 217, 238)
- **Signals**: LED_* (Activity indicators)
- **KiCad Netclass**: `Status LEDs`

## Netclass Pattern Matching

The KiCad project uses wildcard patterns to automatically assign nets to netclasses:

| Pattern | Netclass | Description |
|---------|----------|-------------|
| `GND`, `VCC`, `/+3V3`, `/+5V` | Power/Ground | Power rails |
| `/SCI9_*`, `/USB0_*`, `/EMU_*` | Debug/Programming | Debug interfaces |
| `/RPI5_RSPIA_*` | RPi5 SPI | Main SPI bus |
| `/MOTOR[0-3]_PH`, `/MOTOR[0-3]_EN` | Motor PWM Control | PWM outputs |
| `/MOTOR[0-3]_nFAULT` | Motor Fault Signals | Fault inputs |
| `/MOTOR[0-3]_IPROPI` | Motor Current Sensing | ADC inputs |
| `/ENCODER[0-3]_PHASE_[AB]` | Encoders | Quadrature inputs |
| `/RSPIC_*` | Motor Driver SPI Config | DRV8243S config |
| `/CHARGER_I2C_*` | I2C Charger | Battery charger |
| `/BMS_SMBUS_*` | SMBus BMS | Battery management |
| `/PMOD_JA_*` | PMOD JA | SPI expansion |
| `/PMOD_JB_*` | PMOD JB | I2C expansion |
| `/PMOD_JC_*` | PMOD JC | UART expansion |
| `/LED_*` | Status LEDs | Indicators |

## Priority Order

Netclass priorities (lower number = higher priority):
1. Debug/Programming
2. Power/Ground
3. RPi5 SPI
4. Motor PWM Control
5. Motor Fault Signals
6. Motor Current Sensing
7. Encoders
8. Motor Driver SPI Config
9. I2C Charger
10. SMBus BMS
11. PMOD JB
12. PMOD JA
13. PMOD JC
14. Status LEDs

Higher priority netclasses take precedence when patterns conflict.

## Visual Consistency

This color scheme ensures:
- **Documentation-to-schematic traceability**: Colors match between hardware pinout tables and KiCad schematics
- **Functional grouping**: Related signals use similar colors (e.g., all motor control signals in blue/green/red/lime spectrum)
- **Visual distinction**: Each functional group has a unique, easily distinguishable color
- **Accessibility**: Colors chosen for sufficient contrast and printability

## Reference Files

- LaTeX Source: `/Users/bsikar/Documents/git/STAR/docs/sections/03_hardware_pinout.tex`
- KiCad Project: `/Users/bsikar/Documents/git/STAR/schematic/STAR_MCU.kicad_pro`
- JSON Config: `/Users/bsikar/Documents/git/STAR/schematic/netclass_config.json`
