# Raspberry Pi 5 Voltage Levels and Safety Guide

## ⚠️ CRITICAL: ALL GPIO PINS ARE 3.3V ONLY!

The Raspberry Pi 5 operates at **3.3V logic levels** for all GPIO pins.

**DO NOT connect 5V signals directly to any GPIO pin - this will damage the chip!**

---

## Voltage Summary

| Interface | GPIO Logic Level | 5V Available? | Notes |
|-----------|-----------------|---------------|-------|
| **40-Pin GPIO Header** | 3.3V | Yes (pins 2,4) | GPIO NOT 5V tolerant! |
| **USB Ports** | USB standard | Yes | Isolated, safe |
| **Ethernet** | Isolated | No | Safe connection |

---

## 40-Pin GPIO Header

### Power Pins:
- **Pin 1, 17**: 3.3V output (~300mA max from GPIO header) ✅
- **Pin 2, 4**: 5V output (from USB-C power supply) ✅
- **Pins 6, 9, 14, 20, 25, 30, 34, 39**: GND

### GPIO Pins:
- **ALL GPIO**: 3.3V logic ONLY! ⚠️
- **NOT 5V tolerant** - will damage Raspberry Pi if 5V applied
- **Maximum current**: 16mA per pin recommended, 50mA absolute maximum

### Safe Connections:
✅ ESP32 (3.3V logic) - **SAFE**
✅ Standard Raspberry Pi HATs (3.3V) - **SAFE**
✅ Most modern sensors (3.3V) - **SAFE**
✅ 3.3V UART adapters (FTDI, CP2102 set to 3.3V mode) - **SAFE**

### Dangerous Connections:
❌ 5V Arduino signals - **WILL DAMAGE CHIP**
❌ 5V UART adapters without level shifter - **WILL DAMAGE CHIP**
❌ 5V I2C devices without level shifter - **WILL DAMAGE CHIP**
❌ Old Arduino shields (5V) - **WILL DAMAGE CHIP**

---

### GPIO:
- **ALL Arduino digital pins**: 3.3V logic
- **NOT compatible with standard 5V Arduino shields!**

### Important:
The Raspberry Pi 5 Arduino connector uses **3.3V logic**, unlike standard Arduinos which use 5V.

**Arduino shields designed for 5V will NOT work safely!**

### Safe Arduino Shields:
✅ Shields explicitly rated for 3.3V
✅ Shields with level shifters
✅ Logic-only shields (no powered components)

### Unsafe:
❌ Standard 5V Arduino shields
❌ Shields that output 5V signals
❌ 5V servos/motors without external power

---

## PMOD Connectors (A and B)

### Standard PMOD Specification:
- **Logic Level**: 3.3V
- **VCC Pin**: 3.3V (~100mA per PMOD max)
- **All signals**: 3.3V

### Pinout (both PMOD A and B):
```
Pin 1-4 (top row):    Pin 5-8 (bottom row):
1: Signal             5: GND
2: Signal             6: VCC (3.3V)
3: Signal             7: Signal
4: Signal             8: Signal
```

### Safe PMOD Modules:
✅ Any module labeled "PMOD" or "Digilent PMOD"
✅ 3.3V I2C/SPI modules
✅ Standard PMOD UART, GPIO modules

---

## ESP32 Connection (Your Use Case)

ESP32 is **FULLY COMPATIBLE** with Raspberry Pi 5! Both use 3.3V logic.

### Recommended Connection: 40-Pin Header

```
Raspberry Pi 5 40-Pin    ESP32
────────────────  ─────────
Pin 8  (TXD)  ->  RX (GPIO3)
Pin 10 (RXD)  <-  TX (GPIO1)
Pin 6  (GND)  --  GND
Pin 1  (3.3V) ->  3.3V (if powering ESP32)
```

### Alternative: Arduino Shield

```
Raspberry Pi 5 Arduino   ESP32
────────────────  ─────────
AR0 (TX)      ->  RX
AR1 (RX)      <-  TX
GND           --  GND
3.3V          ->  3.3V
```

**Both connections are 3.3V and completely safe! ✅**

---

## UART-to-USB Adapter Safety

### ✅ Safe Adapters (3.3V mode):
- FTDI FT232RL with 3.3V jumper set to 3.3V
- CP2102 (check if yours has 5V/3.3V jumper)
- CH340G with 3.3V output

### ⚠️ Check Before Connecting:
Most USB-UART adapters have a jumper or switch:
- **Set to 3.3V mode** before connecting!
- **Never use 5V mode** on Zynq GPIO pins

### Adapter Pinout:
```
USB-UART Adapter    Raspberry Pi 5
────────────────    ───────────
RX  (input)     <-  TXD (output)
TX  (output)    ->  RXD (input)
GND             --  GND
VCC (DO NOT CONNECT unless powering external device)
```

**Important**: Connect RX to TX and TX to RX (crossover)!

---

## Current Limits

### 3.3V Power Output:
- **40-pin header**: ~500mA total (pins 1, 17)
- **PMOD A/B**: ~100mA per connector
- **Arduino shield**: Limited by board design

**Do not exceed these limits!** Use external power for:
- Motors
- Servos
- High-power sensors
- ESP32 with WiFi active (can draw 300mA peaks)

### Powering ESP32:
**Option 1**: External power (recommended)
- USB power to ESP32
- Separate 3.3V regulator

**Option 2**: From Raspberry Pi (only for testing)
- Use 40-pin Pin 1 (3.3V)
- **Only** if ESP32 WiFi is disabled or low power mode
- Monitor current draw

---

## Level Shifters (When Needed)

If you must connect 5V devices, use bidirectional level shifters:

### Recommended Level Shifters:
- **BSS138** based (I2C)
- **TXS0108E** (8-channel, bidirectional)
- **74LVC245** (unidirectional, 8-bit)

### Wiring with Level Shifter:
```
Raspberry Pi 5       Level         5V Device
(3.3V)        Shifter       (5V)
─────────     ───────       ─────────
GPIO  ---->   LV -> HV  --> Device Input
GPIO  <----   LV <- HV  <-- Device Output
3.3V  ---->   VCCL
GND   ------> GND   <----- GND
                     <----- 5V to VCCH
```

---

## Voltage Testing (Before Connecting Unknown Devices)

### Use Multimeter:
1. Power on your device
2. **Measure voltage** between signal pin and GND
3. **If > 3.6V**: Use level shifter!
4. **If < 3.3V**: Direct connection OK

### Common Device Voltages:
| Device | Logic Level | Safe? |
|--------|-------------|-------|
| ESP32 | 3.3V | ✅ YES |
| ESP8266 | 3.3V | ✅ YES |
| Arduino Uno | 5V | ❌ NO - needs level shifter |
| Arduino Due | 3.3V | ✅ YES |
| Raspberry Pi | 3.3V | ✅ YES |
| STM32 (most) | 3.3V | ✅ YES |
| Nordic nRF52 | 3.3V | ✅ YES |
| TI CC2650 | 3.3V | ✅ YES |
| Most I2C sensors | 3.3V or 5V | ⚠️ Check datasheet |
| HC-05 Bluetooth | 3.3V | ✅ YES |
| HC-SR04 Ultrasonic | 5V | ❌ NO - needs level shifter |

---

## Summary: What's Safe?

### ✅ SAFE (3.3V):
- ESP32 (your main use case)
- ESP8266
- Most modern microcontrollers
- Raspberry Pi accessories
- Standard PMOD modules
- Modern sensors (check datasheet)

### ❌ UNSAFE (5V) - Need Level Shifter:
- Standard Arduino (Uno, Mega, Nano)
- 5V Arduino shields
- Some older sensors
- 5V motor controllers
- 5V relay modules
- HC-SR04 ultrasonic sensor

### ⚠️ CHECK FIRST:
- Unknown sensors or modules
- Generic breakout boards
- Chinese modules (often unlabeled)
- Old peripherals

**When in doubt, MEASURE with multimeter!**

---

## Quick Reference Card

```
╔══════════════════════════════════════════════════════╗
║       Raspberry Pi 5 VOLTAGE SAFETY CARD                   ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║  ALL GPIO PINS: 3.3V LOGIC ONLY                     ║
║                                                      ║
║  ✅ SAFE:                                           ║
║     • ESP32 / ESP8266                               ║
║     • 3.3V UART adapters                            ║
║     • Raspberry Pi accessories                      ║
║     • Standard PMOD modules                         ║
║                                                      ║
║  ❌ DANGER (will damage chip):                      ║
║     • 5V Arduino signals                            ║
║     • 5V UART without level shifter                 ║
║     • Standard Arduino shields                      ║
║                                                      ║
║  Power Available:                                   ║
║     • 3.3V: 500mA (40-pin header)                   ║
║     • 5V: Available (40-pin pins 2,4)               ║
║                                                      ║
║  MEASURE FIRST if unsure!                           ║
║                                                      ║
╚══════════════════════════════════════════════════════╝
```

---

## Your STAR Robot Project

For your specific hardware:

| Device | Voltage | Connection | Safe? |
|--------|---------|------------|-------|
| **ESP32** | 3.3V | 40-pin or Arduino shield | ✅ **YES** |
| **SICK TiM561 Lidar** | - | Ethernet (isolated) | ✅ **YES** |
| **USB Camera** | - | USB (isolated) | ✅ **YES** |

All your planned connections are **3.3V safe**! ✅
