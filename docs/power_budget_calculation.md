# STAR System Power Budget Calculation

## Component Specifications

### Raspberry Pi 5 (8GB)
- **Idle**: 700-800mA @ 5V (3.5-4W)
- **Typical Load**: 1.5A @ 5V (7.5W)
- **Peak Load**: 3.4A @ 5V (17W max pathological)
- **Power Supply**: 5V rail

### RPLiDAR C1
- **Operating Current**: 230mA @ 5V (1.15W)
- **Power Supply**: 5V rail

### Renesas RX72N Microcontroller
- **Estimated Active Current**: 150mA @ 3.3V (~0.5W)
  - Running at 240MHz with peripherals (RSPI, GPTW, ADC, etc.)
  - Conservative estimate for high-performance 32-bit MCU
- **Sleep Mode**: <10mA @ 3.3V (with peripheral clocks stopped)
- **Power Supply**: 3.3V rail

### DRV8243S Motor Drivers (4 units)
- **Sleep Current**: 5uA per driver @ 6V
- **Quiescent (Active, No Load)**: ~8mA per driver @ 6V (estimated)
- **Operating**: Current pass-through to motors (minimal IC consumption)
- **Power Supply**: 6V motor rail (VM) and 3.3V logic (VDD)

### DC Gearmotors (4 units, 6V rated)
- **Stall Current**: 3A @ 6V per motor (12A total worst case)
- **Typical Running**: 1A @ 6V per motor (4A total)
- **Light Load**: 0.3A @ 6V per motor (1.2A total)
- **Idle (brake/coast)**: 0A
- **Power Supply**: 6V motor rail

---

## Current Budget by Operating Mode

### Mode 1: System Idle (Standby)
**Description**: System powered, no motion, LiDAR off

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| Raspberry Pi 5 (idle) | 5V | 800mA | 4.0W |
| RPLiDAR C1 (off) | 5V | 0mA | 0W |
| RX72N (active, idle) | 3.3V | 150mA | 0.5W |
| DRV8243S (4x, active) | 6V | 32mA | 0.2W |
| Motors (4x, stopped) | 6V | 0mA | 0W |
| **5V Rail Total** | **5V** | **800mA** | **4.0W** |
| **6V Rail Total** | **6V** | **32mA** | **0.2W** |
| **3.3V Rail Total** | **3.3V** | **150mA** | **0.5W** |
| **SYSTEM TOTAL** | - | - | **4.7W** |

---

### Mode 2: Stationary Mapping (LiDAR Active, No Motion)
**Description**: Stationary with LiDAR scanning, motors idle

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| Raspberry Pi 5 (typical) | 5V | 1.5A | 7.5W |
| RPLiDAR C1 | 5V | 230mA | 1.15W |
| RX72N (active) | 3.3V | 150mA | 0.5W |
| DRV8243S (4x, active) | 6V | 32mA | 0.2W |
| Motors (4x, stopped) | 6V | 0mA | 0W |
| **5V Rail Total** | **5V** | **1.73A** | **8.65W** |
| **6V Rail Total** | **6V** | **32mA** | **0.2W** |
| **3.3V Rail Total** | **3.3V** | **150mA** | **0.5W** |
| **SYSTEM TOTAL** | - | - | **9.35W** |

---

### Mode 3: Normal Operation (Driving + Mapping)
**Description**: Typical autonomous navigation, motors at moderate load

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| Raspberry Pi 5 (typical) | 5V | 1.5A | 7.5W |
| RPLiDAR C1 | 5V | 230mA | 1.15W |
| RX72N (active) | 3.3V | 150mA | 0.5W |
| DRV8243S (4x, active) | 6V | 32mA | 0.2W |
| Motors (4x, typical load) | 6V | 4.0A | 24W |
| **5V Rail Total** | **5V** | **1.73A** | **8.65W** |
| **6V Rail Total** | **6V** | **4.03A** | **24.2W** |
| **3.3V Rail Total** | **3.3V** | **150mA** | **0.5W** |
| **SYSTEM TOTAL** | - | - | **33.35W** |

---

### Mode 4: Peak Load (Maximum Acceleration/Climbing)
**Description**: Heavy motor load (climbing, acceleration), RPi5 under load

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| Raspberry Pi 5 (peak) | 5V | 3.4A | 17W |
| RPLiDAR C1 | 5V | 230mA | 1.15W |
| RX72N (active) | 3.3V | 150mA | 0.5W |
| DRV8243S (4x, active) | 6V | 32mA | 0.2W |
| Motors (4x, heavy load) | 6V | 8.0A | 48W |
| **5V Rail Total** | **5V** | **3.63A** | **18.15W** |
| **6V Rail Total** | **6V** | **8.03A** | **48.2W** |
| **3.3V Rail Total** | **3.3V** | **150mA** | **0.5W** |
| **SYSTEM TOTAL** | - | - | **66.85W** |

---

### Mode 5: Absolute Worst Case (Motor Stall)
**Description**: All motors stalled (should trigger fault protection)

| Component | Voltage | Current | Power |
|-----------|---------|---------|-------|
| Raspberry Pi 5 (peak) | 5V | 3.4A | 17W |
| RPLiDAR C1 | 5V | 230mA | 1.15W |
| RX72N (active) | 3.3V | 150mA | 0.5W |
| DRV8243S (4x, active) | 6V | 32mA | 0.2W |
| Motors (4x, **STALL**) | 6V | **12.0A** | **72W** |
| **5V Rail Total** | **5V** | **3.63A** | **18.15W** |
| **6V Rail Total** | **6V** | **12.03A** | **72.2W** |
| **3.3V Rail Total** | **3.3V** | **150mA** | **0.5W** |
| **SYSTEM TOTAL** | - | - | **90.85W** |

**NOTE**: DRV8243S has overcurrent protection and should limit current before reaching full stall. This mode should be transient (<100ms).

---

## Power Supply Recommendations

### Minimum Specifications

#### 5V Rail (Raspberry Pi 5 + LiDAR)
- **Continuous**: 2A minimum (1.73A typical + margin)
- **Peak**: 4A recommended (for RPi5 peak loads)
- **Recommended**: USB-C PD 5V/5A (25W) power supply

#### 6V Motor Rail
- **Continuous**: 5A minimum (4A typical + 25% margin)
- **Peak**: 10A recommended (for transient high loads)
- **Absolute Max**: 15A (with current limiting/protection)
- **Recommended**: 6V 10A buck converter or dedicated motor battery with 10A continuous rating

#### 3.3V Logic Rail
- **Continuous**: 200mA (150mA + margin)
- **Source**: LDO from 5V rail or dedicated buck converter

### Battery Recommendations (if battery powered)

#### Option 1: Single Battery with Buck Converters
- **Battery**: 2S LiPo (7.4V nominal, 8.4V max) or 2S LiFePO4 (6.4V nominal)
- **Capacity**: 5000mAh minimum for ~30 minutes runtime at normal operation
- **Discharge Rating**: 10C minimum (50A burst capability for safety margin)
- **Buck Converters**:
  - 5V @ 5A for Raspberry Pi 5 and LiDAR
  - 6V @ 10A for motors (or use battery voltage directly if 2S LiFePO4)
  - 3.3V @ 500mA for RX72N logic

#### Option 2: Dual Battery System
- **5V Rail**: Dedicated 5V USB power bank (10000mAh, 3A output)
- **Motor Rail**: 2S LiPo 5000mAh 30C for 6V motors

### Estimated Runtime (Normal Operation Mode: 33.35W)

| Battery Capacity | Voltage | Energy | Runtime |
|------------------|---------|--------|---------|
| 5000mAh 2S LiPo | 7.4V | 37Wh | ~1.1 hours |
| 10000mAh 2S LiPo | 7.4V | 74Wh | ~2.2 hours |
| 5000mAh 3S LiPo | 11.1V | 55.5Wh | ~1.7 hours |

**Note**: Runtime calculations assume 80% battery usable capacity and 85% converter efficiency.

---

## Current Limiting and Protection

### Recommended Protection Features

1. **Motor Rail (6V)**:
   - Electronic fuse or current limiter set to 12A
   - Per-motor current limiting via DRV8243S IPROPI configuration
   - Software current monitoring via motor driver current sense

2. **5V Rail**:
   - 5A fuse or PTC resettable fuse
   - Raspberry Pi 5 has built-in power management

3. **3.3V Rail**:
   - 500mA fuse or current-limited LDO

### Firmware Current Monitoring
- DRV8243S IPROPI pins provide motor current feedback to RX72N ADC
- Implement per-motor current limits in firmware (e.g., 2.5A continuous limit)
- Trigger fault state if any motor exceeds limit for >100ms

---

## Summary Table

| Operating Mode | Total Power | 5V Current | 6V Current | Typical Use Case |
|----------------|-------------|------------|------------|------------------|
| **Idle** | 4.7W | 800mA | 32mA | System on, waiting |
| **Stationary Mapping** | 9.35W | 1.73A | 32mA | LiDAR scan only |
| **Normal Operation** | 33.35W | 1.73A | 4.03A | Autonomous navigation |
| **Peak Load** | 66.85W | 3.63A | 8.03A | Climbing/acceleration |
| **Motor Stall (fault)** | 90.85W | 3.63A | 12.03A | Protection should trip |

---

## References

- [Raspberry Pi 5 Power Consumption Benchmarks](https://www.pidramble.com/wiki/benchmarks/power-consumption)
- [Reducing Raspberry Pi 5's power consumption by 140x](https://www.jeffgeerling.com/blog/2023/reducing-raspberry-pi-5s-power-consumption-140x)
- [RPLiDAR C1 Specifications - Waveshare Wiki](https://www.waveshare.com/wiki/RPLIDAR_C1)
- [DRV8243-Q1 Datasheet - Texas Instruments](https://www.ti.com/lit/ds/symlink/drv8243-q1.pdf)
- [RX72N Group Datasheet - Renesas](https://www.renesas.com/en/document/dst/rx72n-group-datasheet)

---

**Last Updated**: 2025-12-26
**Author**: STAR Project Power Analysis
