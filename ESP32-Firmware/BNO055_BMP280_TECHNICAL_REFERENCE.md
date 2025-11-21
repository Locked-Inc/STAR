# BNO055 9-DOF IMU & BMP280 Pressure Sensor: Comprehensive Technical Reference
## ESP32-IDF C Driver Development Guide

---

## Table of Contents
1. [Register Maps](#register-maps)
2. [Communication Protocols](#communication-protocols)
3. [BNO055 Operation Modes & Calibration](#bno055-operation-modes--calibration)
4. [Quaternion & Euler Angle Outputs](#quaternion--euler-angle-outputs)
5. [BMP280 Pressure & Temperature Measurement](#bmp280-pressure--temperature-measurement)
6. [Altitude Calculation](#altitude-calculation)
7. [Shared Bus Management](#shared-bus-management)
8. [Security & Validation Considerations](#security--validation-considerations)
9. [Memory Safety & Multi-Sensor Synchronization](#memory-safety--multi-sensor-synchronization)
10. [C Driver Implementation Patterns](#c-driver-implementation-patterns)

---

## 1. Register Maps

### 1.1 BNO055 Register Map Overview

The BNO055 uses a dual-page register architecture accessed through I2C/SPI interfaces. Page 0 contains operational and sensor data registers; Page 1 contains calibration and configuration registers.

**Page Selection Register:**
```
Address: 0x07 (PAGE_ID)
- Bit 0: Page select (0 = Page 0, 1 = Page 1)
- Must be checked before register access to ensure reading from correct page
```

#### Page 0 Registers (Operational)

| Address | Name | Type | Purpose |
|---------|------|------|---------|
| 0x00 | CHIP_ID | R | Chip ID (0xA0) |
| 0x01 | ACC_ID | R | Accelerometer ID (0xFB) |
| 0x02 | MAG_ID | R | Magnetometer ID (0x32) |
| 0x03 | GYRO_ID | R | Gyroscope ID (0x0F) |
| 0x04-0x05 | SW_REV_ID | R | Software Revision |
| 0x06 | BL_REV_ID | R | Bootloader Revision |
| 0x08-0x0D | ACC_DATA_X/Y/Z | R | Accelerometer data (LSB @ 0x08/0x0A/0x0C, MSB @ 0x09/0x0B/0x0D) |
| 0x0E-0x13 | MAG_DATA_X/Y/Z | R | Magnetometer data (LSB @ 0x0E/0x10/0x12, MSB @ 0x0F/0x11/0x13) |
| 0x14-0x19 | GYRO_DATA_X/Y/Z | R | Gyroscope data (LSB @ 0x14/0x16/0x18, MSB @ 0x15/0x17/0x19) |
| 0x1A-0x1D | EUL_HEADING/ROLL/PITCH | R | Euler angles (heading @ 0x1A-0x1B, roll @ 0x1C-0x1D, pitch follows) |
| 0x20-0x23 | QUATERNION_W/X/Y/Z | R | Quaternion (W @ 0x20-0x21, X @ 0x22-0x23, Y @ 0x24-0x25, Z @ 0x26-0x27) |
| 0x28-0x2D | LIN_ACCEL_X/Y/Z | R | Linear Acceleration (16-bit, m/s^2) |
| 0x2E-0x33 | GRAVITY_X/Y/Z | R | Gravity Vector (16-bit, m/s^2) |
| 0x34 | TEMP | R | Temperature (signed 8-bit, degrees C) |
| 0x35 | CALIB_STAT | R | Calibration Status (System, Gyro, Accel, Mag) |
| 0x36 | SELFTEST_RESULT | R | Self-test results |
| 0x37 | INTR_STAT | R | Interrupt status |
| 0x38 | SYS_CLK_STATUS | R | System clock status |
| 0x39 | SYS_STAT | R | System status code |
| 0x3A | SYS_ERR | R | System error code |
| 0x3D | OPR_MODE | R/W | Operation Mode (bits 3-0) |
| 0x3E | PWR_MODE | R/W | Power Mode (bits 2-0) |
| 0x3F | SYS_TRIGGER | W | System Trigger Register |
| 0x40-0x41 | TEMP_SOURCE | R/W | Temperature source selection |
| 0x42 | AXIS_MAP_CONFIG | R/W | Axis remapping |
| 0x43 | AXIS_MAP_SIGN | R/W | Axis sign |

**Critical Page 0 Registers:**

**OPR_MODE (0x3D) - Operation Mode**
```
Bits [3:0] - Mode Selection
0x0 = CONFIGMODE (factory reset state)
0x2 = ACCONLY
0x4 = MAGONLY
0x7 = ACCMAG
0x8 = ACCGYRO
0xA = MAGGYRO
0xB = AMG (Accelerometer-Magnetometer-Gyroscope)
0xC = IMU
0xD = COMPASS
0xE = M4G (Magnetometer for Gyroscope)
0xF = NDOF_FMC_OFF (NDOF with Fast Magnetometer Calibration disabled)
0x1C = NDOF (9-DOF with Fast Magnetometer Calibration - FUSION MODE)

Note: Some mode transitions require prior CONFIG mode transition
Firmware bug: 3/3 calibration state causes heading jitter (use 0-2 state instead)
```

**PWR_MODE (0x3E) - Power Mode**
```
Bits [2:0] - Power Mode Selection
0x0 = NORMAL (all sensors on)
0x1 = LOWPOWER (reduced power, may affect sensor accuracy)
0x2 = SUSPEND (minimal power, only clock active)

Access to PWR_MODE requires OPR_MODE = CONFIGMODE first
```

**SYS_TRIGGER (0x3F) - System Trigger**
```
Bit 7: Self-test trigger
Bit 5: System reset (triggers full device reset, 650ms wait required)
Bit 1: Soft reset (triggers soft reset, wait 650ms before I2C access)
Bit 0: Clock external (use external crystal if available)
```

**CALIB_STAT (0x35) - Calibration Status**
```
Bits [7:6]: System calibration status
  00 = Not calibrated
  01 = Calibration in progress
  10 = Calibration completed but unstable
  11 = Calibration completed and stable

Bits [5:4]: Gyroscope calibration status (same encoding)
Bits [3:2]: Accelerometer calibration status (same encoding)
Bits [1:0]: Magnetometer calibration status (same encoding)

Security Note: Firmware bug - 3/3 calibration can cause numerical instability
Recommendation: Use calib_stat <= 2/3 for most applications
```

#### Page 1 Registers (Calibration & Configuration)

| Address | Name | Type | Purpose |
|---------|------|------|---------|
| 0x00 | ACC_OFFSET_X/Y/Z | R/W | Accelerometer offset (LSB/MSB pairs @ 0x00/0x01, 0x02/0x03, 0x04/0x05) |
| 0x06-0x0B | MAG_OFFSET_X/Y/Z | R/W | Magnetometer offset (LSB/MSB pairs @ 0x06/0x07, 0x08/0x09, 0x0A/0x0B) |
| 0x0C-0x11 | GYRO_OFFSET_X/Y/Z | R/W | Gyroscope offset (LSB/MSB pairs @ 0x0C/0x0D, 0x0E/0x0F, 0x10/0x11) |
| 0x12-0x19 | ACCEL_RADIUS | R/W | Accelerometer radius (16-bit) |
| 0x1A-0x1B | MAG_RADIUS | R/W | Magnetometer radius (16-bit) |
| 0x20 | ACC_CONFIG | R/W | Accelerometer configuration (range, bandwidth) |
| 0x21 | MAG_CONFIG | R/W | Magnetometer configuration |
| 0x22 | GYRO_CONFIG_0 | R/W | Gyroscope range configuration |
| 0x23 | GYRO_CONFIG_1 | R/W | Gyroscope bandwidth configuration |

### 1.2 BMP280 Register Map

The BMP280 uses a single register address space with chip ID at 0xD0.

| Address | Name | Type | Purpose |
|---------|------|------|---------|
| 0xD0 | ID | R | Chip ID (0x58) |
| 0xE0 | RESET | W | Soft reset (write 0xB6 to trigger) |
| 0xF3 | STATUS | R | Measurement status (measuring, im_update) |
| 0xF4 | CTRL_MEAS | R/W | Control measurements (mode, oversampling) |
| 0xF5 | CONFIG | R/W | Configuration (filter, standby time) |
| 0xF7-0xF9 | PRESS_MSB/LSB/XLSB | R | Raw pressure data (20-bit) |
| 0xFA-0xFC | TEMP_MSB/LSB/XLSB | R | Raw temperature data (20-bit) |
| 0x88-0x8F | CALIB_T1-T3 | R | Temperature calibration coefficients |
| 0x8E-0xA0 | CALIB_P1-P9 | R | Pressure calibration coefficients |

**CTRL_MEAS (0xF4) - Control Measurements**
```
Bits [7:5]: Pressure oversampling
  000 = x0 (skipped)
  001 = x1
  010 = x2
  011 = x4
  100 = x8
  101 = x16

Bits [4:2]: Temperature oversampling (same encoding)

Bits [1:0]: Power mode
  00 = SLEEP (no measurements)
  01 = FORCED (single measurement then sleep)
  10 = FORCED (alternative notation)
  11 = NORMAL (continuous measurement)
```

**CONFIG (0xF5) - Configuration**
```
Bits [7:5]: Standby duration in NORMAL mode
  000 = 0.5ms
  001 = 62.5ms
  010 = 125ms
  011 = 250ms
  100 = 500ms
  101 = 1000ms
  110 = 2000ms
  111 = 4000ms

Bits [4:2]: IIR filter coefficient
  000 = off
  001 = 2
  010 = 4
  011 = 8
  100 = 16

Bit 0: SPI 3-wire enable
```

**STATUS (0xF3) - Status Register**
```
Bit 3: Measuring (1 = measurement ongoing)
Bit 0: Im_update (1 = image update ongoing, NVM write in progress)

Security: Do not read data while measuring (can cause data corruption)
```

#### BMP280 Calibration Coefficient Address Map

Calibration data occupies 24 bytes (0x88-0xA0):

**Temperature Coefficients:**
- 0x88-0x89: T1 (unsigned 16-bit, [deg C] resolution depends on algorithm)
- 0x8A-0x8B: T2 (signed 16-bit)
- 0x8C-0x8D: T3 (signed 16-bit)

**Pressure Coefficients:**
- 0x8E-0x8F: P1 (unsigned 16-bit)
- 0x90-0x91: P2 (signed 16-bit)
- 0x92-0x93: P3 (signed 16-bit)
- 0x94-0x95: P4 (signed 16-bit)
- 0x96-0x97: P5 (signed 16-bit)
- 0x98-0x99: P6 (signed 16-bit)
- 0x9A-0x9B: P7 (signed 16-bit)
- 0x9C-0x9D: P8 (signed 16-bit)
- 0x9E-0x9F: P9 (signed 16-bit)

---

## 2. Communication Protocols

### 2.1 I2C Protocol Overview

Both BNO055 and BMP280 support I2C communication with the following characteristics:

#### BNO055 I2C Specifications

**Addressing:**
- 7-bit addressing mode
- Default address: 0x29 (with COM3 pin to 3.3V)
- Alternative address: 0x28 (with COM3 pin to GND)
- Configurable at runtime via COM3 pin state

**Speed Support:**
- Standard mode: up to 100 kHz
- Fast mode: up to 400 kHz
- High-speed mode: up to 3.4 MHz (not recommended due to signal integrity)

**Protocol Sequence - Write Operation:**
```
START -> Address (0x29) + Write bit -> ACK -> Register Address -> ACK -> Data Byte(s) -> ACK -> STOP
```

**Protocol Sequence - Read Operation:**
```
START -> Address (0x29) + Read bit -> ACK -> (repeated START) -> Address (0x29) + Write bit -> Register Address -> ACK -> Repeated START -> Address (0x29) + Read bit -> ACK -> Data Byte(s) -> NACK -> STOP
```

Alternative simplified sequence (widely used):
```
Write Register Address -> Repeated START -> Read Data
```

**Timing Requirements:**
- Minimum I2C clock: Not specified by Bosch, but 100 kHz is standard
- Recommended clock: 400 kHz for best performance
- Setup/hold times: Standard I2C spec (100ns typical)

#### BMP280 I2C Specifications

**Addressing:**
- 7-bit addressing mode
- Address 0x77 if SDO pin connected to VDDIO (3.3V)
- Address 0x76 if SDO pin connected to GND

**Speed Support:**
- Standard mode: up to 100 kHz
- Fast mode: up to 400 kHz
- High-speed mode: up to 3.4 MHz

**Timing Requirements:**
- I2C clock typically 100-400 kHz (recommend 400 kHz for responsiveness)
- No special setup/hold requirements beyond I2C standard

**Critical Caveat:**
- Do not read data while measurement is ongoing (STATUS.measuring = 1)
- Burst read recommended for calibration coefficients (0x88-0xA0)
- Datasheet: "It is strongly recommended to use burst read and not address every register individually"

### 2.2 SPI Protocol Overview

Both sensors support SPI, but I2C is recommended for simplicity on shared buses.

#### BNO055 SPI Specifications

**Mode Selection:**
- Default: SPI polarity = 0, phase = 0 (CPOL=0, CPHA=0)
- Alternative: SPI polarity = 1, phase = 1 (CPOL=1, CPHA=1)
- Set via CS pin state during power-up (not recommended - stick with mode 0)

**Speed Support:**
- Maximum clock: 10 MHz
- Recommended: 5 MHz or less for stability

**Addressing:**
- Register address in first byte: 0-127 for read, 128-255 for write
- Format: [RW_bit(1)][Register_Address(7)]

**Frame Structure:**
```
[RW_bit][REG_ADDR(6:0)][DUMMY] -> [DATA]
- Write: 0x00 + register_addr, dummy byte, then data
- Read: 0x80 + register_addr, dummy byte, then data read
```

#### BMP280 SPI Specifications

**Mode Selection:**
- Default: SPI polarity = 0, phase = 0
- Alternative: Supports mode 1 and mode 3

**Speed Support:**
- Maximum clock: 10 MHz
- Recommended: 5 MHz or less

**CS Requirements:**
- Must be held low during entire transaction
- Must be high between transactions

**Data Format:**
- 8-bit read/write operations
- Address in first byte: [0(read)/1(write)][Register_Address(6:0)]

### 2.3 I2C Bus Frequency Recommendations

For multi-sensor shared bus:
- **Standard choice**: 400 kHz (Fast mode)
- **Conservative choice**: 100 kHz (Standard mode)
- **Avoid**: 3.4 MHz (High-speed mode) - increases noise sensitivity

**Pull-up Resistor Guidelines:**
- Recommended: 4.7 kΩ per I2C line (SCL, SDA)
- Minimum combined resistance: 2.2 kΩ (if multiple sensors have built-in pull-ups)
- Total capacitance on bus: less than 400 pF recommended

---

## 3. BNO055 Operation Modes & Calibration

### 3.1 Operation Modes Classification

#### Non-Fusion Modes
These modes output raw or minimally processed sensor data:

- **ACCONLY (0x2)**: Accelerometer only, power-optimized
- **MAGONLY (0x4)**: Magnetometer only
- **GYROONLY (0x6)**: Gyroscope only (implied but not standard)
- **ACCMAG (0x7)**: Accelerometer + Magnetometer, no gyro
- **ACCGYRO (0x8)**: Accelerometer + Gyroscope, no magnetometer
- **MAGGYRO (0xA)**: Magnetometer + Gyroscope, no accelerometer
- **AMG (0xB)**: All three sensors without fusion

#### Fusion Modes
These modes run the BNO055's onboard sensor fusion algorithm:

- **IMU (0xC)**: Accelerometer + Gyroscope fusion (9-axis without magnetometer)
- **COMPASS (0xD)**: Accelerometer + Magnetometer fusion
- **M4G (0xE)**: Magnetometer-for-Gyroscope (gyro-stabilized heading)
- **NDOF_FMC_OFF (0xF)**: 9-DOF without Fast Magnetometer Calibration
- **NDOF (0x1C)**: **Standard 9-DOF** - All sensors with automatic fast magnetometer calibration

#### Special Mode
- **CONFIGMODE (0x0)**: Configuration/initialization mode (factory reset state)

### 3.2 Mode Transitions & Timing

**Key Timing Requirements:**

1. **Power-on/Reset to CONFIG mode**: 650 ms minimum wait
   - After device power-on or soft/hard reset, wait 650ms before I2C access
   - Check system status (0x39) for ready indication

2. **CONFIG mode to Operational mode**: 7 ms minimum
   - API implementations typically use 20-25 ms for safety margin

3. **Power Mode changes**: Must transition through CONFIG mode
   - CONFIG -> write PWR_MODE -> back to operational mode
   - Total sequence: 7ms + PWR_MODE transition + 19ms

4. **Firmware-specific delays**: Use 25-30 ms for all mode transitions
   - API implementations use longer delays than datasheet minimums
   - Empirically verified safer approach

**Mode Transition Sequence:**
```
Current_Mode -> Write CONFIGMODE (0x0 to 0x3D) -> Wait 25ms ->
Write Target_Mode -> Wait 25ms -> Verify OPR_MODE register
```

### 3.3 Calibration Overview

#### Calibration Components

The BNO055 has four independent calibration states:

1. **System Calibration**: Overall fusion algorithm calibration
2. **Gyroscope Calibration**: Gyroscope offset calibration
3. **Accelerometer Calibration**: Accelerometer offset calibration
4. **Magnetometer Calibration**: Magnetometer sensitivity and bias

Each can be in state 0-3:
- **0**: Not calibrated
- **1**: Calibration in progress
- **2**: Calibration completed but unstable
- **3**: Calibration completed and stable

**Read from CALIB_STAT (0x35):**
```
[sys_calib(2-bits)][gyro_calib(2-bits)][accel_calib(2-bits)][mag_calib(2-bits)]
```

#### Automatic vs. Manual Calibration

**Automatic Calibration:**
- Enabled in NDOF and COMPASS modes
- Cannot be disabled - runs in background continuously
- Modifies calibration offsets automatically
- Firmware bug: May overwrite explicitly loaded offsets during operation

**Manual Calibration:**
- Load pre-computed offsets into Page 1 registers (0x00-0x11)
- Use after power-on if known good calibration data available
- May be overwritten by automatic calibration in fusion modes

#### Recommended Calibration Strategy

```
1. Power-on -> Wait 650ms
2. Set mode to NDOF
3. Wait for automatic calibration (monitor CALIB_STAT)
4. When stable (ideally 3/3 or 2/3), read and save offsets:
   - Page 1, registers 0x00-0x11 (6 offset pairs)
5. On subsequent power-cycles:
   - Switch to CONFIG, load saved offsets, return to NDOF
6. Monitor CALIB_STAT continuously for instability
```

**Security/Stability Notes:**
- Firmware bug: 3/3 calibration (all sensors at level 3) can cause heading jitter
- Empirically, maintaining 2/3 calibration is safer
- Rapid sensor motion can drop calibration status unexpectedly
- Magnetometer calibration requires 8-shape motion pattern (not automatic)

#### Calibration Validation

```c
// Recommended validation before using orientation data
typedef struct {
    uint8_t sys_calib;      // 0-3
    uint8_t gyro_calib;     // 0-3
    uint8_t accel_calib;    // 0-3
    uint8_t mag_calib;      // 0-3
} bno055_calib_status_t;

// Safe usage threshold
#define MIN_SYSTEM_CALIB_LEVEL 1    // At least calibration in progress
#define MIN_FUSION_CALIB_LEVEL 2    // Prefer completed over unstable=1

bool is_calibration_valid(bno055_calib_status_t calib) {
    // All sensors should be at least at level 2 for best results
    return (calib.sys_calib >= 2 && calib.gyro_calib >= 2 &&
            calib.accel_calib >= 2 && calib.mag_calib >= 2);
}
```

---

## 4. Quaternion & Euler Angle Outputs

### 4.1 Quaternion Representation

**Format:** 4 consecutive 16-bit signed integers (W, X, Y, Z)

**Register Addresses (Page 0):**
- 0x20-0x21: Quaternion W (signed 16-bit)
- 0x22-0x23: Quaternion X (signed 16-bit)
- 0x24-0x25: Quaternion Y (signed 16-bit)
- 0x26-0x27: Quaternion Z (signed 16-bit)

**Range and Resolution:**
- Each component: -1.0 to 1.0 (normalized unit quaternion)
- 16-bit signed integer maps to: -1.0 to 1.0 range
- Resolution: 1/2^14 ≈ 0.000061 (approximately)
- Representation: Q14 fixed-point (1 sign bit + 14 magnitude bits + implicit normalization)

**Data Format:**
```
Raw register value (int16_t) -> Normalized quaternion component = raw_value / 16384.0f
```

**Memory Layout (Little-Endian on ESP32):**
```
Register 0x20 = W_LSB, 0x21 = W_MSB -> W = (W_MSB << 8) | W_LSB
Register 0x22 = X_LSB, 0x23 = X_MSB -> X = (X_MSB << 8) | X_LSB
... and so on
```

### 4.2 Euler Angles Representation

**Format:** 3 consecutive 16-bit signed integers (Heading, Roll, Pitch)

**Register Addresses (Page 0):**
- 0x1A-0x1B: Euler Heading (signed 16-bit)
- 0x1C-0x1D: Euler Roll (signed 16-bit)
- 0x1E-0x1F: Euler Pitch (signed 16-bit)

**Range and Resolution:**
- Each angle: -180° to +180° (heading 0-360°)
- 16-bit signed: Resolution ≈ 360°/65536 ≈ 0.0055° per LSB
- Actual datasheet scaling: Range ±180° or 0-360° depending on angle type

**Data Format:**
```
Raw register value (int16_t) -> Angle in degrees = raw_value / 16.0f
```

**Axis Convention (Z-Y-X Euler angles):**
- **Heading**: Rotation around Z-axis (0-360° or -180 to +180°)
- **Roll**: Rotation around X-axis (-180° to +180°)
- **Pitch**: Rotation around Y-axis (-90° to +90°)

### 4.3 Quaternion vs. Euler Angles

#### Advantages of Quaternions

1. **No Gimbal Lock**: Unlike Euler angles, quaternions don't suffer from gimbal lock singularities
2. **Numerical Stability**: Better numerical properties for continuous rotations
3. **Smooth Interpolation**: Easier to implement smooth orientation transitions (SLERP)
4. **Smaller Magnitude**: More compact representation for transmission

#### Known Firmware Issues with Euler Angles

**Critical Bug Warning:**
- Euler angle output from BNO055 has documented firmware bugs
- Accuracy degrades significantly when tilted > 20-30° from horizontal
- Heading becomes increasingly distorted at steep angles
- **Recommendation**: Always use quaternions, convert to Euler angles locally if needed

**Affected Scenarios:**
- Quadcopter applications with significant pitch/roll (> 45°)
- Vertical orientation measurements
- Any application requiring >±20° tilt accuracy

### 4.4 Quaternion Normalization & Precision

#### Normalization Requirement

Unit quaternions MUST have magnitude = 1.0:
```
|q| = sqrt(w² + x² + y² + z²) = 1.0
```

#### Numerical Stability Issues

After each quaternion operation (multiplication, integration), precision degrades:

1. **Drift Due to Floating-Point Errors**: Each operation introduces rounding errors
2. **Accumulation Over Time**: Repeated operations cause quaternion norm to drift
3. **Critical Threshold**: Norm drift > 0.001 (magnitude 0.999-1.001) causes noticeable artifacts

#### Renormalization Strategy

```c
// Recommended: Normalize after quaternion multiplication
typedef struct {
    float w, x, y, z;
} quaternion_t;

// Normalization using reciprocal square root (more accurate than division)
void quaternion_normalize(quaternion_t *q) {
    float norm_sq = q->w*q->w + q->x*q->x + q->y*q->y + q->z*q->z;

    // Avoid division by zero
    if (norm_sq < 1e-10f) {
        q->w = 1.0f; q->x = 0.0f; q->y = 0.0f; q->z = 0.0f;
        return;
    }

    // Using reciprocal square root for better accuracy
    float inv_norm = 1.0f / sqrtf(norm_sq);
    q->w *= inv_norm;
    q->x *= inv_norm;
    q->y *= inv_norm;
    q->z *= inv_norm;
}

// Frequency recommendation: Normalize every 10-50 iterations or operations
// Verify periodically: if norm > 1.001 or < 0.999, problem exists
```

#### Precision Warnings

1. **BNO055 Output**: Already normalized, but verify in driver code
2. **Float Precision**: 32-bit float sufficient for most applications
3. **Accumulated Operations**: If rotating same quaternion repeatedly, re-normalize every 50-100 iterations
4. **Tolerance**: Allow ±0.001 norm deviation before forcing recalibration

### 4.5 Conversion: Quaternion to Euler Angles

**Safe Conversion (avoids gimbal lock):**

```c
// Quaternion to Euler angles (Z-Y-X convention)
void quaternion_to_euler(float w, float x, float y, float z,
                        float *heading, float *roll, float *pitch) {
    float heading_rad, roll_rad, pitch_rad;

    // Roll (rotation around X-axis)
    float sinr_cosp = 2 * (w * x + y * z);
    float cosr_cosp = 1 - 2 * (x*x + y*y);
    roll_rad = atan2f(sinr_cosp, cosr_cosp);

    // Pitch (rotation around Y-axis)
    float sinp = 2 * (w * y - z * x);
    // Clamp for numerical safety
    if (fabsf(sinp) >= 1)
        pitch_rad = copysignf(M_PI / 2, sinp);  // 90 or -90 degrees
    else
        pitch_rad = asinf(sinp);

    // Heading (rotation around Z-axis)
    float siny_cosp = 2 * (w * z + x * y);
    float cosy_cosp = 1 - 2 * (y*y + z*z);
    heading_rad = atan2f(siny_cosp, cosy_cosp);

    // Convert to degrees
    *heading = heading_rad * 180.0f / M_PI;
    *roll = roll_rad * 180.0f / M_PI;
    *pitch = pitch_rad * 180.0f / M_PI;
}
```

---

## 5. BMP280 Pressure & Temperature Measurement

### 5.1 Raw Data Acquisition

**Raw Pressure Data:**
- Registers: 0xF7 (MSB), 0xF8 (LSB), 0xF9 (XLSB)
- Format: 20-bit unsigned integer (3 bytes)
- Extraction: `adc_P = (MSB << 12) | (LSB << 4) | (XLSB >> 4)`

**Raw Temperature Data:**
- Registers: 0xFA (MSB), 0xFB (LSB), 0xFC (XLSB)
- Format: 20-bit unsigned integer (3 bytes)
- Extraction: `adc_T = (MSB << 12) | (LSB << 4) | (XLSB >> 4)`

### 5.2 Compensation Algorithm

The BMP280 requires manufacturer-specific calibration coefficients for accurate readings.

**Calibration Coefficients (from Page 0, addresses 0x88-0xA0):**

```c
typedef struct {
    // Temperature calibration
    uint16_t T1;    // 0x88-0x89
    int16_t T2;     // 0x8A-0x8B
    int16_t T3;     // 0x8C-0x8D

    // Pressure calibration
    uint16_t P1;    // 0x8E-0x8F
    int16_t P2;     // 0x90-0x91
    int16_t P3;     // 0x92-0x93
    int16_t P4;     // 0x94-0x95
    int16_t P5;     // 0x96-0x97
    int16_t P6;     // 0x98-0x99
    int16_t P7;     // 0x9A-0x9B
    int16_t P8;     // 0x9C-0x9D
    int16_t P9;     // 0x9E-0x9F
} bmp280_calib_t;
```

**Critical Note:** Read calibration coefficients as burst read (0x88-0xA0), not individually.

**Temperature Compensation (32-bit recommended):**

```c
// Returns t_fine (used in pressure compensation)
// Temperature in Celsius = t_fine / 5120.0
int32_t bmp280_compensate_temperature(int32_t adc_T, bmp280_calib_t *calib) {
    int32_t var1, var2, t_fine;

    var1 = (((adc_T >> 3) - ((int32_t)calib->T1 << 1))) * ((int32_t)calib->T2) >> 11;
    var2 = (((adc_T >> 4) - ((int32_t)calib->T1)) *
            ((adc_T >> 4) - ((int32_t)calib->T1))) >> 12;
    var2 = ((var2) * ((int32_t)calib->T3)) >> 14;

    t_fine = var1 + var2;
    return t_fine;  // Save for pressure compensation
}

float bmp280_get_temperature_celsius(int32_t t_fine) {
    return (t_fine / 5120.0f);  // or (t_fine * 100.0f / 512000.0f)
}
```

**Pressure Compensation (32-bit recommended):**

```c
// MUST call temperature compensation first to get t_fine
uint32_t bmp280_compensate_pressure(int32_t adc_P, int32_t t_fine,
                                    bmp280_calib_t *calib) {
    int64_t var1, var2, press;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib->P6;
    var2 = var2 + ((var1 * (int64_t)calib->P5) << 17);
    var2 = var2 + (((int64_t)calib->P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib->P3) >> 8) +
           ((var1 * (int64_t)calib->P2) << 12);
    var1 = (((int64_t)1 << 47) + var1) * ((int64_t)calib->P1) >> 33;

    if (var1 == 0) return 0;  // Avoid exception

    press = 1048576 - adc_P;
    press = (((press << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib->P9) * (press >> 13) * (press >> 13)) >> 25;
    var2 = (((int64_t)calib->P8) * press) >> 19;
    press = ((press + var1 + var2) >> 8) + (((int64_t)calib->P7) << 4);

    return (uint32_t)press;  // Pressure in Pa
}

float bmp280_get_pressure_hpa(uint32_t pressure_pa) {
    return (pressure_pa / 100.0f);
}
```

**32-bit Alternative (if 64-bit unavailable):**
```
Reference: BMP280 datasheet Appendix 8.2
Accuracy: Typically 1 Pa (1-sigma)
```

### 5.3 Measurement Modes

**Power Modes (CTRL_MEAS 0xF4, bits [1:0]):**

- **0x0 - SLEEP**: No measurements, lowest power
- **0x1 - FORCED**: Single measurement cycle, then returns to SLEEP
- **0x3 - NORMAL**: Continuous measurement with configurable standby

**Measurement Sequencing:**

1. **FORCED mode (recommended for low power):**
   ```
   1. Write mode = FORCED
   2. Poll STATUS.measuring until 0
   3. Read pressure and temperature registers
   4. Sensor automatically returns to SLEEP
   5. Repeat from step 1
   ```

2. **NORMAL mode (continuous, lowest latency):**
   ```
   1. Write standby duration (CONFIG bits [7:5])
   2. Write mode = NORMAL
   3. Continuously poll and read registers
   4. Sensor measures on interval set by standby duration
   ```

**Sampling Timing:**

- Measurement duration depends on oversampling settings (0-5 samples)
- Typical duration: 10-100ms depending on oversampling
- Standby duration in NORMAL mode: 0.5ms to 4000ms (8 options)

### 5.4 Oversampling Configuration

**Pressure and Temperature Oversampling (CTRL_MEAS bits [7:2]):**

| Setting | Samples | Approx. Duration | Noise |
|---------|---------|------------------|-------|
| 0x0 | 0 (skipped) | 0ms | N/A |
| 0x1 | 1x | 2ms | High |
| 0x2 | 2x | 3ms | Medium-high |
| 0x3 | 4x | 5ms | Medium |
| 0x4 | 8x | 8ms | Medium-low |
| 0x5 | 16x | 15ms | Low |

**Recommended Settings:**

- **Power optimization**: 2x oversampling (4ms cycle time)
- **Standard**: 4x oversampling (5ms cycle time)
- **High accuracy**: 16x oversampling (15ms cycle time)

**Oversampling Mismatch Risk:**
- Pressure measurement depends on temperature compensation
- ALWAYS measure temperature first, then pressure
- If only pressure is needed, temperature still must be read (algorithm requirement)

### 5.5 Data Validity & Synchronization

**Critical Timing Rules:**

1. **Do NOT read while measuring** (STATUS.measuring = 1)
   - Reading during measurement can cause data corruption
   - Always check STATUS register before read operations

2. **Temperature must precede pressure**
   - Pressure compensation requires t_fine from temperature
   - If only pressure is needed, still read temperature first

3. **Burst Read for Calibration**
   - Read 0x88-0xA0 as single burst operation
   - Individual register reads can cause partial corruption

**Safe Read Pattern:**
```c
// Wait for measurement to complete
while (bmp280_read_status() & 0x08) {  // STATUS.measuring bit
    vTaskDelay(1);  // Wait 1ms
}

// Read temperature first (generates t_fine)
int32_t adc_T = bmp280_read_temperature_raw();
int32_t t_fine = bmp280_compensate_temperature(adc_T, &calib);

// Read pressure (uses t_fine)
int32_t adc_P = bmp280_read_pressure_raw();
uint32_t pressure = bmp280_compensate_pressure(adc_P, t_fine, &calib);
```

---

## 6. Altitude Calculation

### 6.1 Barometric Altitude Formula

**International Barometric Formula (simplified):**

```
h = 44330 * (1.0 - (p / p0)^(1/5.255))

Where:
h = altitude (meters)
p = pressure at measurement point (Pa or hPa)
p0 = reference pressure at sea level (Pa or hPa)
```

**For pressure in hPa:**
```
h = 44330 * (1.0 - (p_hpa / p0_hpa)^0.1903)
```

### 6.2 Reference Pressure Determination

**Critical Parameter: Sea Level Pressure**

The reference pressure p0 MUST match the atmospheric pressure at sea level for YOUR LOCATION on the MEASUREMENT DAY.

**Why it matters:**
- Barometric pressure at sea level changes DAILY based on weather
- Error of ±1 hPa → altitude error of ±10 meters (approximately)
- Fixed sea level pressure produces systematic altitude errors

**Typical Values:**
- Standard sea level pressure: 1013.25 hPa (ISA)
- Real-world range: 980-1050 hPa depending on weather
- Weather forecast sources: Provide daily sea level pressure

**Obtaining p0:**

1. **Online weather data**: Use weather API for current location
2. **GPS reference**: If GPS available, use known reference elevation
3. **Calibration reference**: Place sensor at known elevation, read pressure as p0 reference
4. **Worst case**: Assume 1013.25 hPa (±10-15m error typical)

**Implementation:**
```c
float bmp280_calculate_altitude(float pressure_hpa, float sea_level_pressure_hpa) {
    float altitude = 44330.0f * (1.0f - powf(pressure_hpa / sea_level_pressure_hpa, 0.1903f));
    return altitude;  // meters
}
```

### 6.3 Altitude Accuracy Considerations

**Best-Case Accuracy:** ±1 meter (with precise reference pressure)
**Typical Accuracy:** ±5-10 meters
**Worst-Case Accuracy:** ±20+ meters (without accurate reference pressure)

**Factors Affecting Accuracy:**

1. **Reference Pressure Error**: Dominant source of error (±10m per ±1 hPa)
2. **Temperature Compensation**: Already handled by BMP280 algorithm
3. **Oversampling**: Higher oversampling = lower noise = better accuracy
4. **Sensor Drift**: Negligible over short term (hours), may require recalibration daily
5. **Weather Variability**: Pressure changes with weather fronts (±5-10 hPa/day typical)

**Dynamic Adjustment:**

For applications requiring high accuracy:
```c
// Periodic recalibration
typedef struct {
    float altitude;
    float reference_pressure_hpa;
    uint32_t last_calibration_time;
} altitude_tracker_t;

void recalibrate_altitude_reference(altitude_tracker_t *tracker,
                                    float current_pressure_hpa,
                                    float known_reference_elevation_m) {
    // Solve backwards for reference pressure at sea level
    // p0 = p / (1 - h/44330)^(1/0.1903)

    float exponent = 1.0f / 0.1903f;
    tracker->reference_pressure_hpa = current_pressure_hpa /
        powf(1.0f - (known_reference_elevation_m / 44330.0f), exponent);

    tracker->altitude = known_reference_elevation_m;
    tracker->last_calibration_time = get_current_time_ms();
}
```

---

## 7. Shared Bus Management

### 7.1 I2C Bus Architecture for Multi-Sensor

**Physical Configuration:**
```
ESP32
├── GPIO 21 (SDA) ──────┬──── BNO055 SDA (addr 0x29)
│                       └──── BMP280 SDA (addr 0x77)
└── GPIO 22 (SCL) ──────┬──── BNO055 SCL
                        └──── BMP280 SCL
```

**Pull-up Resistor Sizing:**
- Each sensor may have internal 10-100kΩ pull-ups
- External pull-ups: 4.7kΩ recommended
- Total combined resistance > 2.2kΩ required
- Typical design: 4.7kΩ external on each line

### 7.2 Device Addressing

**BNO055 Address Selection:**
- Default (COM3 = 3.3V): 0x29
- Alternative (COM3 = GND): 0x28
- Configurable at power-up via physical pin state

**BMP280 Address Selection:**
- Address 0x77 if SDO pin = 3.3V (VDDIO)
- Address 0x76 if SDO pin = GND
- Configurable at power-up via physical pin state

**Address Conflict Resolution:**
If sensors have conflicting addresses, use:
1. I2C address bridge/multiplexer (TCA9548A)
2. One sensor on GPIO-based bit-bang I2C bus
3. Use both I2C bus controllers (ESP32 has 2 I2C buses)

### 7.3 Sequential vs. Multiplexed Access

**Sequential Access (Simplest, Recommended):**
```c
// Single I2C bus, access sensors one at a time
void sensor_read_task(void) {
    while (1) {
        // Read BNO055 (may take 5-20ms)
        bno055_read_orientation();

        // Read BMP280 (may take 2-10ms)
        bmp280_read_pressure();

        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms loop
    }
}
```

**Advantages:**
- Simple implementation
- No race conditions
- Single I2C driver instance needed
- Predictable timing

**Disadvantages:**
- Higher latency between readings of different sensors
- Data age mismatch (BNO055 data is 10ms older than BMP280 data)

### 7.4 Thread Safety & Synchronization

#### ESP32-IDF I2C Thread Safety

**Important Facts:**

1. **Legacy I2C driver (ESP-IDF < v5.2)**: NOT thread-safe
   - Use mutex protection for multi-task access
   - Or designate single task as I2C owner

2. **New I2C driver (ESP-IDF v5.2+)**: Partially thread-safe
   - i2c_new_master_bus() creates thread-safe bus object
   - Still recommend mutex for application safety

#### Mutex Protection Pattern

```c
// Static mutex for I2C bus protection
static SemaphoreHandle_t i2c_mutex = NULL;

void i2c_init_mutex(void) {
    i2c_mutex = xSemaphoreCreateMutex();
    configASSERT(i2c_mutex != NULL);
}

// Wrapper for BNO055 read
esp_err_t bno055_protected_read(uint8_t reg, uint8_t *data, size_t len) {
    esp_err_t ret = ESP_FAIL;

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ret = i2c_master_read_slave_register(BNO055_ADDR, reg, data, len);
        xSemaphoreGive(i2c_mutex);
    }

    return ret;
}

// Wrapper for BMP280 read
esp_err_t bmp280_protected_read(uint8_t reg, uint8_t *data, size_t len) {
    esp_err_t ret = ESP_FAIL;

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ret = i2c_master_read_slave_register(BMP280_ADDR, reg, data, len);
        xSemaphoreGive(i2c_mutex);
    }

    return ret;
}
```

#### Multi-Bus Approach

For demanding applications, use both I2C buses:

```c
// I2C_NUM_0: BNO055 (GPIO 21/22)
// I2C_NUM_1: BMP280 (GPIO 25/26)

void init_dual_i2c(void) {
    // Initialize I2C bus 0 for BNO055
    i2c_config_t conf0 = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &conf0);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    // Initialize I2C bus 1 for BMP280
    i2c_config_t conf1 = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_25,
        .scl_io_num = GPIO_NUM_26,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_1, &conf1);
    i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0);
}

// Now BNO055 on bus 0, BMP280 on bus 1 - no contention
void bno055_read_task(void) {
    while (1) {
        bno055_read_orientation_on_bus(I2C_NUM_0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void bmp280_read_task(void) {
    while (1) {
        bmp280_read_pressure_on_bus(I2C_NUM_1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### 7.5 Data Synchronization & Timestamping

#### Synchronized Read Pattern

```c
typedef struct {
    uint32_t timestamp_ms;
    float heading, roll, pitch;    // BNO055
    float pressure_hpa;
    float temperature_celsius;      // BMP280
    uint8_t bno_calib_status;
} sensor_data_t;

void synchronized_sensor_read(sensor_data_t *data) {
    // Record timestamp at start
    data->timestamp_ms = esp_timer_get_time() / 1000;  // milliseconds

    // Read both sensors within critical section
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Read BNO055 orientation (faster, ~5ms)
        bno055_read_quaternion(&q);
        bno055_read_calib_status(&data->bno_calib_status);

        // Read BMP280 pressure (slower, ~10ms with oversampling)
        bmp280_read_temperature_pressure(&data->temperature_celsius,
                                        &data->pressure_hpa);

        xSemaphoreGive(i2c_mutex);
    }

    // Convert quaternion to Euler angles outside critical section
    quaternion_to_euler(q.w, q.x, q.y, q.z,
                       &data->heading, &data->roll, &data->pitch);
}
```

#### Data Age Tracking

```c
typedef struct {
    sensor_data_t current;
    uint32_t age_ms;  // Milliseconds since last successful read
} sensor_snapshot_t;

void update_sensor_age(sensor_snapshot_t *snap) {
    uint32_t now_ms = esp_timer_get_time() / 1000;
    snap->age_ms = now_ms - snap->current.timestamp_ms;
}

// Use in safety checks
bool is_sensor_data_fresh(sensor_snapshot_t *snap, uint32_t max_age_ms) {
    update_sensor_age(snap);
    return snap->age_ms < max_age_ms;
}
```

---

## 8. Security & Validation Considerations

### 8.1 Invalid Mode Transitions

#### Mode Transition State Machine

```
┌──────────────┐
│  CONFIGMODE  │  (Initial state after reset)
└──────┬───────┘
       │
       ├─► (Set to any operational mode)
       │   - Non-fusion: ACCONLY, MAGONLY, ACCMAG, ACCGYRO, AMG, IMU, COMPASS, M4G
       │   - Fusion: NDOF_FMC_OFF, NDOF
       │
       └─► (Set to CONFIGMODE for PWR_MODE changes)
           └─► (Modify PWR_MODE register)
               └─► (Return to operational mode)

Timing Requirements:
- CONFIG -> Any mode: wait 7ms (API uses 25ms for safety)
- PWR_MODE change sequence: CONFIG (wait 25ms) -> PWR_MODE change -> Mode (wait 25ms)
- After reset: ALWAYS wait 650ms before I2C operations
```

#### Invalid Transition Detection

```c
typedef enum {
    BNO_MODE_CONFIGMODE = 0x0,
    BNO_MODE_ACCONLY = 0x2,
    BNO_MODE_MAGONLY = 0x4,
    BNO_MODE_ACCMAG = 0x7,
    BNO_MODE_ACCGYRO = 0x8,
    BNO_MODE_MAGGYRO = 0xA,
    BNO_MODE_AMG = 0xB,
    BNO_MODE_IMU = 0xC,
    BNO_MODE_COMPASS = 0xD,
    BNO_MODE_M4G = 0xE,
    BNO_MODE_NDOF_FMC_OFF = 0xF,
    BNO_MODE_NDOF = 0x1C,
    BNO_MODE_INVALID = 0xFF,
} bno055_mode_t;

// Validate mode transition
esp_err_t bno055_validate_mode_transition(bno055_mode_t current, bno055_mode_t target) {
    // Valid transitions
    static const bool valid_transitions[12][12] = {
        // From CONFIG -> To:  [ACC] [MAG] [ACCMAG] [ACCGYRO] [MAGGYRO] [AMG] [IMU] [COMPASS] [M4G] [NDOF_FMC] [NDOF] [CONFIG]
        [0] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},  // From CONFIG: all valid
        [1] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},  // From ACCONLY: only to CONFIG or CONFIGMODE for mode change
        // ... etc for all modes
    };

    if (target == BNO_MODE_INVALID) {
        return ESP_ERR_INVALID_ARG;
    }

    if (current == target) {
        return ESP_OK;  // No change needed
    }

    if (current == BNO_MODE_CONFIGMODE || target == BNO_MODE_CONFIGMODE) {
        return ESP_OK;  // Always allow transitions to/from CONFIGMODE
    }

    // Direct mode-to-mode transitions are not allowed
    // Must go through CONFIGMODE
    return ESP_ERR_INVALID_STATE;
}
```

#### Mode Transition Safety Pattern

```c
esp_err_t bno055_safe_set_mode(bno055_mode_t target_mode) {
    bno055_mode_t current_mode;

    // Get current mode
    esp_err_t ret = bno055_read_opr_mode(&current_mode);
    if (ret != ESP_OK) return ret;

    // If already in target mode, no action needed
    if (current_mode == target_mode) return ESP_OK;

    // Validate transition
    ret = bno055_validate_mode_transition(current_mode, target_mode);
    if (ret != ESP_OK) return ret;

    // If not in CONFIG mode and target is not CONFIG, go to CONFIG first
    if (current_mode != BNO_MODE_CONFIGMODE && target_mode != BNO_MODE_CONFIGMODE) {
        ret = bno055_write_register(0x3D, BNO_MODE_CONFIGMODE);  // OPR_MODE
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(25));  // Wait for mode change
    }

    // Write target mode
    ret = bno055_write_register(0x3D, target_mode);  // OPR_MODE
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(25));  // Wait for mode change

    // Verify mode was set
    ret = bno055_read_opr_mode(&current_mode);
    if (ret != ESP_OK) return ret;

    if (current_mode != target_mode) {
        return ESP_ERR_INVALID_STATE;  // Mode transition failed
    }

    return ESP_OK;
}
```

### 8.2 Calibration Validation

#### Calibration Status Monitoring

```c
typedef struct {
    uint8_t sys_calib;      // 0-3
    uint8_t gyro_calib;     // 0-3
    uint8_t accel_calib;    // 0-3
    uint8_t mag_calib;      // 0-3
    bool is_stable;         // All sensors >= 2
    bool is_fully_calibrated;  // All sensors == 3 (but unreliable!)
} bno055_calib_status_t;

esp_err_t bno055_read_calib_status(bno055_calib_status_t *status) {
    uint8_t calib_stat;
    esp_err_t ret = bno055_read_register(0x35, &calib_stat);  // CALIB_STAT

    if (ret != ESP_OK) return ret;

    status->sys_calib = (calib_stat >> 6) & 0x3;
    status->gyro_calib = (calib_stat >> 4) & 0x3;
    status->accel_calib = (calib_stat >> 2) & 0x3;
    status->mag_calib = calib_stat & 0x3;

    status->is_stable = (status->sys_calib >= 2 &&
                         status->gyro_calib >= 2 &&
                         status->accel_calib >= 2 &&
                         status->mag_calib >= 2);

    status->is_fully_calibrated = (status->sys_calib == 3 &&
                                   status->gyro_calib == 3 &&
                                   status->accel_calib == 3 &&
                                   status->mag_calib == 3);

    return ESP_OK;
}

// Recommended: Wait for stable (>= 2/2/2/2) rather than fully calibrated (3/3/3/3)
bool bno055_wait_calibration_stable(uint32_t timeout_ms) {
    uint32_t start = esp_timer_get_time() / 1000;
    bno055_calib_status_t status;

    while ((esp_timer_get_time() / 1000 - start) < timeout_ms) {
        if (bno055_read_calib_status(&status) == ESP_OK) {
            if (status.is_stable && !status.is_fully_calibrated) {
                // Safe state: calibrated but not at dangerous level 3
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    return false;  // Timeout
}
```

#### Calibration Offset Management

```c
typedef struct {
    int16_t acc_offset_x, acc_offset_y, acc_offset_z;
    int16_t mag_offset_x, mag_offset_y, mag_offset_z;
    int16_t gyro_offset_x, gyro_offset_y, gyro_offset_z;
} bno055_calibration_offsets_t;

// Save current calibration offsets to structure
esp_err_t bno055_save_calibration(bno055_calibration_offsets_t *offsets) {
    // Switch to Page 1 (calibration page)
    bno055_write_register(0x07, 0x01);  // PAGE_ID = 1
    vTaskDelay(pdMS_TO_TICKS(1));

    // Read calibration registers (6 offset pairs)
    uint8_t calib_data[12];
    esp_err_t ret = bno055_read_registers(0x00, calib_data, 12);

    if (ret == ESP_OK) {
        // Parse 16-bit offsets (LSB/MSB pairs)
        offsets->acc_offset_x = (calib_data[1] << 8) | calib_data[0];
        offsets->acc_offset_y = (calib_data[3] << 8) | calib_data[2];
        offsets->acc_offset_z = (calib_data[5] << 8) | calib_data[4];

        offsets->mag_offset_x = (calib_data[7] << 8) | calib_data[6];
        offsets->mag_offset_y = (calib_data[9] << 8) | calib_data[8];
        offsets->mag_offset_z = (calib_data[11] << 8) | calib_data[10];
    }

    // Switch back to Page 0
    bno055_write_register(0x07, 0x00);  // PAGE_ID = 0

    return ret;
}

// Load calibration offsets
esp_err_t bno055_load_calibration(const bno055_calibration_offsets_t *offsets) {
    // Must be in CONFIGMODE
    bno055_safe_set_mode(BNO_MODE_CONFIGMODE);
    vTaskDelay(pdMS_TO_TICKS(25));

    // Switch to Page 1
    bno055_write_register(0x07, 0x01);  // PAGE_ID = 1
    vTaskDelay(pdMS_TO_TICKS(1));

    // Pack calibration data
    uint8_t calib_data[12];
    calib_data[0] = offsets->acc_offset_x & 0xFF;
    calib_data[1] = (offsets->acc_offset_x >> 8) & 0xFF;
    // ... repeat for other offsets

    // Write calibration registers
    esp_err_t ret = bno055_write_registers(0x00, calib_data, 12);

    // Switch back to Page 0
    bno055_write_register(0x07, 0x00);  // PAGE_ID = 0

    return ret;
}
```

### 8.3 Fusion State Race Conditions

#### Data Read Integrity

```c
// Problem: Reading quaternion across 8 bytes (0x20-0x27)
// Sensor may update mid-read, giving inconsistent values

// Solution: Atomic read with retry

typedef struct {
    float w, x, y, z;
} quaternion_data_t;

esp_err_t bno055_read_quaternion_safe(quaternion_data_t *quat) {
    uint8_t buf[8];
    int16_t qw, qx, qy, qz;
    int16_t qw_verify, qx_verify;

    const uint8_t MAX_RETRIES = 3;

    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        // Read all 8 bytes
        esp_err_t ret = bno055_read_registers(0x20, buf, 8);
        if (ret != ESP_OK) return ret;

        // Extract values
        qw = (buf[1] << 8) | buf[0];
        qx = (buf[3] << 8) | buf[2];
        qy = (buf[5] << 8) | buf[4];
        qz = (buf[7] << 8) | buf[6];

        // Immediate re-read first 4 bytes to detect mid-read update
        uint8_t verify_buf[4];
        ret = bno055_read_registers(0x20, verify_buf, 4);
        if (ret != ESP_OK) return ret;

        qw_verify = (verify_buf[1] << 8) | verify_buf[0];
        qx_verify = (verify_buf[3] << 8) | verify_buf[2];

        // If values changed significantly, sensor was updated mid-read
        if (qw == qw_verify && qx == qx_verify) {
            // Confidence: data is consistent
            quat->w = qw / 16384.0f;
            quat->x = qx / 16384.0f;
            quat->y = qy / 16384.0f;
            quat->z = qz / 16384.0f;
            return ESP_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(1));  // Brief delay before retry
    }

    return ESP_ERR_TIMEOUT;  // Failed consistency check
}
```

#### Sensor Fusion Status Validation

```c
// Monitor system status for errors during operation
esp_err_t bno055_read_system_status(uint8_t *status) {
    return bno055_read_register(0x39, status);  // SYS_STAT
}

// System status codes:
// 0 = Idle, 1 = System Error, 2 = Initializing, 3 = Running, 4 = Running (IMU Mode), etc.

// System error register
esp_err_t bno055_read_system_error(uint8_t *error) {
    return bno055_read_register(0x3A, error);  // SYS_ERR
}

// Error codes:
// 0x00 = No error
// 0x01 = Out of range (sensor value overflow)
// 0x02 = Regmap value error
// 0x03 = Register map address error
// 0x04 = Register map write error
// 0x05 = BNO low power error
// 0x06 = Accelerometer power mode not applicable
// 0x07 = Fusion algorithm configuration error
// 0x08 = Sensor configuration error

bool bno055_check_system_health(void) {
    uint8_t status, error;

    if (bno055_read_system_status(&status) != ESP_OK) return false;
    if (bno055_read_system_error(&error) != ESP_OK) return false;

    // Running state is good (0x03)
    if (status != 0x03 && status != 0x04) {
        // ESP_LOGE(TAG, "BNO055 not running: status=%d", status);
        return false;
    }

    if (error != 0x00) {
        // ESP_LOGE(TAG, "BNO055 system error: %d", error);
        return false;
    }

    return true;
}
```

### 8.4 Pressure Overflow Prevention

#### BMP280 Pressure Range Monitoring

```c
// BMP280 pressure measurement range: 300-1100 hPa

#define BMP280_MIN_PRESSURE_HPA 300.0f
#define BMP280_MAX_PRESSURE_HPA 1100.0f

bool bmp280_validate_pressure(float pressure_hpa) {
    return (pressure_hpa >= BMP280_MIN_PRESSURE_HPA &&
            pressure_hpa <= BMP280_MAX_PRESSURE_HPA);
}

// Altitude saturation check
float bmp280_calculate_altitude_safe(float pressure_hpa, float ref_pressure_hpa) {
    // Validate inputs
    if (!bmp280_validate_pressure(pressure_hpa) ||
        !bmp280_validate_pressure(ref_pressure_hpa)) {
        return NAN;  // Invalid measurement
    }

    // Check pressure ratio sanity
    float ratio = pressure_hpa / ref_pressure_hpa;
    if (ratio <= 0 || ratio > 2.0f) {
        return NAN;  // Unrealistic pressure ratio
    }

    // Calculate altitude with bounds checking
    float altitude = 44330.0f * (1.0f - powf(ratio, 0.1903f));

    // Altitude realism check (±5000m is extreme)
    if (fabsf(altitude) > 10000.0f) {
        return NAN;  // Unrealistic altitude
    }

    return altitude;
}

// Outlier detection with running average
typedef struct {
    float values[10];
    uint8_t index;
    bool filled;
} altitude_filter_t;

float altitude_filter_update(altitude_filter_t *filter, float new_altitude) {
    // Skip NAN values
    if (isnan(new_altitude)) {
        return NAN;
    }

    filter->values[filter->index] = new_altitude;
    filter->index = (filter->index + 1) % 10;

    if (!filter->filled && filter->index == 0) {
        filter->filled = true;
    }

    // Calculate median or mean
    float sum = 0;
    int count = filter->filled ? 10 : filter->index;
    for (int i = 0; i < count; i++) {
        sum += filter->values[i];
    }

    return sum / count;
}
```

---

## 9. Memory Safety & Multi-Sensor Synchronization

### 9.1 Quaternion Data Handling

#### Alignment and Size Guarantees

```c
// Ensure proper alignment for fast access
typedef struct {
    // 16 bytes total, aligned to 4-byte boundary
    int16_t w, x, y, z;
} __attribute__((packed)) bno055_quaternion_raw_t;

_Static_assert(sizeof(bno055_quaternion_raw_t) == 8,
               "Quaternion raw size must be 8 bytes");
_Static_assert(alignof(bno055_quaternion_raw_t) >= 2,
               "Quaternion must be at least 2-byte aligned");

typedef struct {
    float w, x, y, z;
} __attribute__((aligned(4))) bno055_quaternion_float_t;

_Static_assert(sizeof(bno055_quaternion_float_t) == 16,
               "Quaternion float must be 16 bytes");
```

#### Safe Quaternion Reading Pattern

```c
// Thread-safe quaternion read with consistency check
esp_err_t bno055_read_quaternion_consistent(bno055_quaternion_float_t *quat) {
    bno055_quaternion_raw_t raw1, raw2;

    // First read
    esp_err_t ret = bno055_read_registers(0x20, (uint8_t*)&raw1, sizeof(raw1));
    if (ret != ESP_OK) return ret;

    // Second read to verify consistency
    ret = bno055_read_registers(0x20, (uint8_t*)&raw2, sizeof(raw2));
    if (ret != ESP_OK) return ret;

    // Check consistency (allow small jitter, not full change)
    if (abs(raw1.w - raw2.w) > 100 ||
        abs(raw1.x - raw2.x) > 100 ||
        abs(raw1.y - raw2.y) > 100 ||
        abs(raw1.z - raw2.z) > 100) {
        return ESP_ERR_TIMEOUT;  // Data changed mid-read
    }

    // Use average of both reads (poor-man's noise filtering)
    quat->w = ((raw1.w + raw2.w) / 2.0f) / 16384.0f;
    quat->x = ((raw1.x + raw2.x) / 2.0f) / 16384.0f;
    quat->y = ((raw1.y + raw2.y) / 2.0f) / 16384.0f;
    quat->z = ((raw1.z + raw2.z) / 2.0f) / 16384.0f;

    return ESP_OK;
}

// Post-read validation and normalization
esp_err_t bno055_read_and_validate_quaternion(bno055_quaternion_float_t *quat) {
    esp_err_t ret = bno055_read_quaternion_consistent(quat);
    if (ret != ESP_OK) return ret;

    // Verify quaternion is normalized (magnitude ~= 1.0)
    float magnitude_sq = quat->w*quat->w + quat->x*quat->x +
                         quat->y*quat->y + quat->z*quat->z;

    if (magnitude_sq < 0.9f || magnitude_sq > 1.1f) {
        ESP_LOGW(TAG, "Quaternion magnitude out of range: %f", sqrtf(magnitude_sq));
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Re-normalize for safety
    float inv_mag = 1.0f / sqrtf(magnitude_sq);
    quat->w *= inv_mag;
    quat->x *= inv_mag;
    quat->y *= inv_mag;
    quat->z *= inv_mag;

    return ESP_OK;
}
```

### 9.2 Calibration Buffer Management

#### Calibration Data Storage

```c
typedef struct {
    // BNO055 calibration offsets (Page 1)
    int16_t bno_acc_offset[3];   // X, Y, Z
    int16_t bno_mag_offset[3];   // X, Y, Z
    int16_t bno_gyro_offset[3];  // X, Y, Z

    // BMP280 calibration coefficients
    uint16_t bmp_T1;
    int16_t bmp_T2, bmp_T3;
    uint16_t bmp_P1;
    int16_t bmp_P2, bmp_P3, bmp_P4, bmp_P5, bmp_P6, bmp_P7, bmp_P8, bmp_P9;

    // Metadata
    uint32_t timestamp_seconds;  // Unix timestamp of calibration
    uint16_t crc16;              // CRC for integrity checking
} __attribute__((packed)) sensor_calibration_data_t;

// CRC16 calculation for integrity
uint16_t calculate_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

// NVS (Non-Volatile Storage) operations
esp_err_t save_calibration_to_nvs(const sensor_calibration_data_t *calib) {
    nvs_handle_t nvs_handle;

    esp_err_t ret = nvs_open("sensor_calib", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;

    // Calculate CRC and set timestamp
    sensor_calibration_data_t calib_with_meta = *calib;
    calib_with_meta.timestamp_seconds = time(NULL);
    calib_with_meta.crc16 = calculate_crc16(
        (uint8_t*)&calib_with_meta,
        offsetof(sensor_calibration_data_t, crc16));

    ret = nvs_set_blob(nvs_handle, "sensor_calib",
                       &calib_with_meta, sizeof(calib_with_meta));
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}

// Load calibration with validation
esp_err_t load_calibration_from_nvs(sensor_calibration_data_t *calib) {
    nvs_handle_t nvs_handle;

    esp_err_t ret = nvs_open("sensor_calib", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) return ret;

    size_t required_size = sizeof(sensor_calibration_data_t);
    ret = nvs_get_blob(nvs_handle, "sensor_calib", calib, &required_size);
    nvs_close(nvs_handle);

    if (ret != ESP_OK) return ret;

    // Verify CRC
    uint16_t expected_crc = calculate_crc16(
        (uint8_t*)calib,
        offsetof(sensor_calibration_data_t, crc16));

    if (expected_crc != calib->crc16) {
        return ESP_ERR_INVALID_CRC;  // Corrupted calibration data
    }

    return ESP_OK;
}
```

### 9.3 Multi-Sensor Synchronization

#### Time-Synchronized Data Struct

```c
typedef struct {
    // Timestamp (microseconds since boot)
    uint64_t timestamp_us;

    // BNO055 data (rotation)
    struct {
        int16_t w, x, y, z;      // Raw quaternion
        uint8_t calib_status;     // Calibration status
        uint8_t sys_status;       // System status
        uint32_t read_time_us;    // Time taken to read
    } bno055;

    // BMP280 data (pressure/temperature)
    struct {
        int32_t adc_pressure;     // Raw ADC value
        int32_t adc_temperature;  // Raw ADC value
        uint32_t read_time_us;    // Time taken to read
    } bmp280;

} __attribute__((packed)) synchronized_sensor_snapshot_t;

// Ring buffer for sensor history
#define SENSOR_HISTORY_SIZE 100

typedef struct {
    synchronized_sensor_snapshot_t samples[SENSOR_HISTORY_SIZE];
    uint32_t write_index;
    uint32_t total_samples;  // Monotonic counter
    SemaphoreHandle_t mutex;
} sensor_history_ring_buffer_t;

// Initialize ring buffer
esp_err_t sensor_history_init(sensor_history_ring_buffer_t *buf) {
    buf->write_index = 0;
    buf->total_samples = 0;
    buf->mutex = xSemaphoreCreateMutex();
    return (buf->mutex != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

// Add sample to ring buffer (thread-safe)
esp_err_t sensor_history_push(sensor_history_ring_buffer_t *buf,
                             const synchronized_sensor_snapshot_t *sample) {
    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    buf->samples[buf->write_index] = *sample;
    buf->write_index = (buf->write_index + 1) % SENSOR_HISTORY_SIZE;
    buf->total_samples++;

    xSemaphoreGive(buf->mutex);
    return ESP_OK;
}

// Get most recent sample
esp_err_t sensor_history_get_latest(sensor_history_ring_buffer_t *buf,
                                    synchronized_sensor_snapshot_t *sample) {
    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (buf->total_samples == 0) {
        xSemaphoreGive(buf->mutex);
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t latest_index = (buf->write_index - 1 + SENSOR_HISTORY_SIZE) % SENSOR_HISTORY_SIZE;
    *sample = buf->samples[latest_index];

    xSemaphoreGive(buf->mutex);
    return ESP_OK;
}
```

#### Synchronized Read Implementation

```c
// Read both sensors with synchronized timing
esp_err_t read_synchronized_sensors(synchronized_sensor_snapshot_t *snapshot) {
    esp_err_t ret;
    uint64_t start_time_us = esp_timer_get_time();

    // Acquire I2C bus
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uint64_t read_start = esp_timer_get_time();

    // Read BNO055 (faster)
    ret = bno055_read_registers(0x20, (uint8_t*)&snapshot->bno055.w, 8);
    if (ret != ESP_OK) {
        xSemaphoreGive(i2c_mutex);
        return ret;
    }
    ret = bno055_read_register(0x35, &snapshot->bno055.calib_status);
    if (ret != ESP_OK) {
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    uint64_t bno_read_time = esp_timer_get_time() - read_start;

    // Read BMP280 temperature first (required for pressure compensation)
    read_start = esp_timer_get_time();
    ret = bmp280_read_registers(0xFA, (uint8_t*)&snapshot->bmp280.adc_temperature, 3);
    if (ret != ESP_OK) {
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    // Read BMP280 pressure
    ret = bmp280_read_registers(0xF7, (uint8_t*)&snapshot->bmp280.adc_pressure, 3);
    if (ret != ESP_OK) {
        xSemaphoreGive(i2c_mutex);
        return ret;
    }

    uint64_t bmp_read_time = esp_timer_get_time() - read_start;

    xSemaphoreGive(i2c_mutex);

    // Record timing
    snapshot->timestamp_us = esp_timer_get_time();
    snapshot->bno055.read_time_us = bno_read_time;
    snapshot->bmp280.read_time_us = bmp_read_time;

    // Total read time for diagnostics
    uint64_t total_read_time = snapshot->timestamp_us - start_time_us;
    if (total_read_time > 20000) {  // > 20ms is suspicious
        ESP_LOGW(TAG, "Slow sensor read: %llu us", total_read_time);
    }

    return ESP_OK;
}

// Process synchronized snapshot
void process_sensor_snapshot(const synchronized_sensor_snapshot_t *snap) {
    // Convert raw ADC values
    bno055_quaternion_float_t quat;
    quat.w = snap->bno055.w / 16384.0f;
    quat.x = snap->bno055.x / 16384.0f;
    quat.y = snap->bno055.y / 16384.0f;
    quat.z = snap->bno055.z / 16384.0f;

    // Extract calibration info
    uint8_t sys_calib = (snap->bno055.calib_status >> 6) & 0x3;
    uint8_t gyro_calib = (snap->bno055.calib_status >> 4) & 0x3;
    uint8_t accel_calib = (snap->bno055.calib_status >> 2) & 0x3;
    uint8_t mag_calib = snap->bno055.calib_status & 0x3;

    // Verify calibration before trusting data
    if (sys_calib < 2 || gyro_calib < 2 || accel_calib < 2 || mag_calib < 2) {
        ESP_LOGW(TAG, "BNO055 not fully calibrated: %d/%d/%d/%d",
                 sys_calib, gyro_calib, accel_calib, mag_calib);
        return;
    }

    // Compensate BMP280
    int32_t t_fine = bmp280_compensate_temperature(snap->bmp280.adc_temperature, &bmp_calib);
    uint32_t pressure_pa = bmp280_compensate_pressure(snap->bmp280.adc_pressure,
                                                      t_fine, &bmp_calib);

    float temperature_c = t_fine / 5120.0f;
    float pressure_hpa = pressure_pa / 100.0f;
    float altitude_m = bmp280_calculate_altitude_safe(pressure_hpa, reference_pressure_hpa);

    // Now use synchronized data: quat, temperature, pressure, altitude
    // All with same timestamp: snap->timestamp_us
}
```

---

## 10. C Driver Implementation Patterns

### 10.1 Basic Driver Structure

```c
// File: bno055_driver.h
#ifndef BNO055_DRIVER_H
#define BNO055_DRIVER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"
#include <stdint.h>
#include <stdbool.h>

// BNO055 I2C address options
#define BNO055_I2C_ADDR_DEFAULT 0x29  // COM3 = 3.3V
#define BNO055_I2C_ADDR_ALT 0x28      // COM3 = GND

// Register addresses (Page 0)
#define BNO055_CHIP_ID_ADDR 0x00
#define BNO055_OPR_MODE_ADDR 0x3D
#define BNO055_PWR_MODE_ADDR 0x3E
#define BNO055_SYS_TRIGGER_ADDR 0x3F
#define BNO055_CALIB_STAT_ADDR 0x35
#define BNO055_QUAT_W_ADDR 0x20
#define BNO055_PAGE_ID_ADDR 0x07

// Operation modes
typedef enum {
    BNO055_MODE_CONFIGMODE = 0x0,
    BNO055_MODE_NDOF = 0x1C,
    // ... other modes
} bno055_operation_mode_t;

// Driver handle
typedef struct {
    i2c_master_dev_handle_t i2c_dev;
    uint8_t i2c_addr;
    SemaphoreHandle_t mutex;
    bool initialized;
} bno055_driver_handle_t;

// Function declarations
esp_err_t bno055_init(bno055_driver_handle_t *handle,
                      i2c_master_bus_handle_t i2c_bus,
                      uint8_t i2c_addr);
esp_err_t bno055_deinit(bno055_driver_handle_t *handle);
esp_err_t bno055_set_mode(bno055_driver_handle_t *handle,
                          bno055_operation_mode_t mode);
esp_err_t bno055_read_quaternion(bno055_driver_handle_t *handle,
                                float *w, float *x, float *y, float *z);

#endif // BNO055_DRIVER_H
```

```c
// File: bno055_driver.c
#include "bno055_driver.h"
#include "esp_log.h"

static const char *TAG = "BNO055";

esp_err_t bno055_init(bno055_driver_handle_t *handle,
                      i2c_master_bus_handle_t i2c_bus,
                      uint8_t i2c_addr) {
    if (!handle || !i2c_bus) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create I2C device handle
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_7,
        .device_address = i2c_addr,
        .scl_speed_hz = 400000,  // 400 kHz
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &handle->i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %d", ret);
        return ret;
    }

    handle->i2c_addr = i2c_addr;
    handle->mutex = xSemaphoreCreateMutex();
    if (!handle->mutex) {
        return ESP_ERR_NO_MEM;
    }

    // Wait for device to be ready
    vTaskDelay(pdMS_TO_TICKS(650));

    // Verify chip ID
    uint8_t chip_id;
    ret = bno055_read_register(handle, BNO055_CHIP_ID_ADDR, &chip_id);
    if (ret != ESP_OK || chip_id != 0xA0) {
        ESP_LOGE(TAG, "Invalid chip ID: 0x%02X", chip_id);
        return ESP_ERR_INVALID_RESPONSE;
    }

    handle->initialized = true;
    ESP_LOGI(TAG, "BNO055 initialized at address 0x%02X", i2c_addr);

    return ESP_OK;
}

esp_err_t bno055_read_register(bno055_driver_handle_t *handle,
                              uint8_t reg_addr,
                              uint8_t *data) {
    if (!handle || !handle->initialized || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(handle->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = i2c_master_transmit_receive(handle->i2c_dev,
                                                &reg_addr, 1,
                                                data, 1,
                                                -1);

    xSemaphoreGive(handle->mutex);
    return ret;
}
```

### 10.2 Complete Code Example: BNO055 + BMP280 Driver

Due to length constraints, here's a summary structure. Full implementation would include:

1. **BNO055 Driver Module**
   - Initialization, mode management
   - Quaternion/Euler reading
   - Calibration save/load
   - Status monitoring

2. **BMP280 Driver Module**
   - Initialization, measurement mode
   - Temperature/pressure compensation
   - Altitude calculation
   - Status checking

3. **Sensor Fusion Module**
   - Synchronized reads
   - Data validation
   - Timestamp management
   - Ring buffer storage

4. **Main Application**
   - Task creation
   - Periodic reads
   - Data logging/transmission

### 10.3 Error Handling Best Practices

```c
// Comprehensive error handling pattern
esp_err_t read_sensor_with_recovery(void) {
    esp_err_t ret;
    int retry_count = 0;
    const int MAX_RETRIES = 3;

    do {
        ret = perform_sensor_read();

        if (ret == ESP_OK) {
            return ESP_OK;
        }

        switch (ret) {
            case ESP_ERR_TIMEOUT:
                ESP_LOGW(TAG, "I2C timeout, retrying (%d/%d)",
                        retry_count + 1, MAX_RETRIES);
                vTaskDelay(pdMS_TO_TICKS(10));
                break;

            case ESP_ERR_INVALID_RESPONSE:
                ESP_LOGW(TAG, "Invalid sensor response");
                break;

            case ESP_ERR_INVALID_STATE:
                ESP_LOGE(TAG, "Sensor in invalid state, attempting reset");
                perform_sensor_reset();
                vTaskDelay(pdMS_TO_TICKS(650));
                break;

            default:
                ESP_LOGE(TAG, "Unexpected error: %s", esp_err_to_name(ret));
                break;
        }

        retry_count++;

    } while (retry_count < MAX_RETRIES);

    return ret;
}
```

---

## References & Additional Resources

### Official Datasheets
- BNO055: Bosch Sensortec BST-BNO055-DS000-14 (Rev 1.4, June 2016)
- BMP280: Bosch Sensortec BST-BMP280-DS001 (Rev 1.26, October 2021)

### Key Implementation Notes

1. **Firmware Bugs (BNO055)**
   - Euler angles distort > 20-30° from horizontal (use quaternions)
   - 3/3 calibration can cause heading jitter (use 2/3 instead)
   - Automatic calibration may overwrite manually-loaded offsets

2. **Temperature Dependency (BMP280)**
   - Pressure compensation requires temperature measurement first
   - For altitude accuracy, use precise sea-level reference pressure

3. **Thread Safety (ESP32-IDF)**
   - Always use mutex for multi-task I2C access
   - New I2C driver (v5.2+) provides better thread safety
   - Consider dual I2C buses for high-performance applications

4. **Data Integrity**
   - Re-normalize quaternions after operations
   - Verify calibration status before trusting orientation data
   - Check measurement status before reading BMP280

---

**Document Version**: 1.0
**Last Updated**: 2024
**Audience**: ESP32-IDF C Developers
**Target**: Production-Ready Driver Implementation

