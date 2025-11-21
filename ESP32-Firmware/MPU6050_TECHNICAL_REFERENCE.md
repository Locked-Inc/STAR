# MPU6050 6-Axis IMU - Comprehensive Technical Reference for C Driver on ESP32-IDF

## Document Overview
Complete technical documentation for implementing a robust, production-grade MPU6050 driver on ESP32 using ESP-IDF framework. Covers register map, I2C protocol, sensor specifications, configuration, FIFO operation, security considerations, and memory safety practices.

**Device**: InvenSense MPU-6050 (now TDK)
**Specification**: 6-axis Motion Tracking with DMP
**I2C Address**: 0x68 (AD0 LOW) or 0x69 (AD0 HIGH)
**Datasheet**: MPU-6000 and MPU-6050 Register Map (Rev 4.0, 03/09/2012)

---

## 1. REGISTER MAP - COMPLETE REFERENCE

### 1.1 Register Address Space (0x00-0x7F)

#### Self-Test and Factory Calibration (0x00-0x05)
| Address | Register | R/W | Bits | Description |
|---------|----------|-----|------|-------------|
| 0x00 | XGOFFS_TC | R/W | 7:0 | X Gyroscope Offset TC |
| 0x01 | YGOFFS_TC | R/W | 7:0 | Y Gyroscope Offset TC |
| 0x02 | ZGOFFS_TC | R/W | 7:0 | Z Gyroscope Offset TC |
| 0x03 | X_FINE_GAIN | R/W | 7:0 | X Fine Gain |
| 0x04 | Y_FINE_GAIN | R/W | 7:0 | Y Fine Gain |
| 0x05 | Z_FINE_GAIN | R/W | 7:0 | Z Fine Gain |

#### Accelerometer Offset (0x06-0x0B)
| Address | Register | R/W | Bits | Description |
|---------|----------|-----|------|-------------|
| 0x06 | XA_OFFSET_H | R/W | 15:8 | X Accel Offset High Byte |
| 0x07 | XA_OFFSET_L_TC | R/W | 7:0 | X Accel Offset Low Byte + TC[1:0] |
| 0x08 | YA_OFFSET_H | R/W | 15:8 | Y Accel Offset High Byte |
| 0x09 | YA_OFFSET_L_TC | R/W | 7:0 | Y Accel Offset Low Byte + TC[1:0] |
| 0x0A | ZA_OFFSET_H | R/W | 15:8 | Z Accel Offset High Byte |
| 0x0B | ZA_OFFSET_L_TC | R/W | 7:0 | Z Accel Offset Low Byte + TC[1:0] |

#### Self-Test Factory Values (0x0D-0x10)
| Address | Register | R/W | Bits | Description |
|---------|----------|-----|------|-------------|
| 0x0D | SELF_TEST_X | R/O | 7:0 | Self Test X |
| 0x0E | SELF_TEST_Y | R/O | 7:0 | Self Test Y |
| 0x0F | SELF_TEST_Z | R/O | 7:0 | Self Test Z |
| 0x10 | SELF_TEST_A | R/O | 7:0 | Self Test Accel |

#### Gyroscope Offset Registers (0x13-0x18)
| Address | Register | R/W | Bits | Description |
|---------|----------|-----|------|-------------|
| 0x13 | XG_OFFS_USRH | R/W | 15:8 | X Gyro User Offset High |
| 0x14 | XG_OFFS_USRL | R/W | 7:0 | X Gyro User Offset Low |
| 0x15 | YG_OFFS_USRH | R/W | 15:8 | Y Gyro User Offset High |
| 0x16 | YG_OFFS_USRL | R/W | 7:0 | Y Gyro User Offset Low |
| 0x17 | ZG_OFFS_USRH | R/W | 15:8 | Z Gyro User Offset High |
| 0x18 | ZG_OFFS_USRL | R/W | 7:0 | Z Gyro User Offset Low |

#### Configuration Registers (0x19-0x22)
| Address | Register | R/W | Bits | Description |
|---------|----------|-----|------|-------------|
| 0x19 | SMPLRT_DIV | R/W | 7:0 | Sample Rate Divider |
| 0x1A | CONFIG | R/W | 2:0 | Digital Low Pass Filter Config |
| 0x1B | GYRO_CONFIG | R/W | 4:3 | Gyroscope Full Scale Select |
| 0x1C | ACCEL_CONFIG | R/W | 4:3 | Accelerometer Full Scale Select |
| 0x1D | FF_THR | R/W | 7:0 | Free Fall Threshold |
| 0x1E | FF_DUR | R/W | 7:0 | Free Fall Duration |
| 0x1F | MOT_THR | R/W | 7:0 | Motion Threshold |
| 0x20 | MOT_DUR | R/W | 7:0 | Motion Duration |
| 0x21 | ZRMOT_THR | R/W | 7:0 | Zero Motion Threshold |
| 0x22 | ZRMOT_DUR | R/W | 7:0 | Zero Motion Duration |

#### FIFO and I2C Master Control (0x23-0x36)
| Address | Register | R/W | Bits | Description |
|---------|----------|-----|------|-------------|
| 0x23 | FIFO_EN | R/W | 7:0 | FIFO Enable Control |
| 0x24 | I2C_MST_CTRL | R/W | 7:0 | I2C Master Control |
| 0x25 | I2C_SLV0_ADDR | R/W | 7:0 | I2C Slave 0 Address |
| 0x26 | I2C_SLV0_REG | R/W | 7:0 | I2C Slave 0 Register Address |
| 0x27 | I2C_SLV0_CTRL | R/W | 7:0 | I2C Slave 0 Control |
| 0x28 | I2C_SLV1_ADDR | R/W | 7:0 | I2C Slave 1 Address |
| 0x29 | I2C_SLV1_REG | R/W | 7:0 | I2C Slave 1 Register Address |
| 0x2A | I2C_SLV1_CTRL | R/W | 7:0 | I2C Slave 1 Control |
| 0x2B | I2C_SLV2_ADDR | R/W | 7:0 | I2C Slave 2 Address |
| 0x2C | I2C_SLV2_REG | R/W | 7:0 | I2C Slave 2 Register Address |
| 0x2D | I2C_SLV2_CTRL | R/W | 7:0 | I2C Slave 2 Control |
| 0x2E | I2C_SLV3_ADDR | R/W | 7:0 | I2C Slave 3 Address |
| 0x2F | I2C_SLV3_REG | R/W | 7:0 | I2C Slave 3 Register Address |
| 0x30 | I2C_SLV3_CTRL | R/W | 7:0 | I2C Slave 3 Control |
| 0x31 | I2C_SLV4_ADDR | R/W | 7:0 | I2C Slave 4 Address |
| 0x32 | I2C_SLV4_REG | R/W | 7:0 | I2C Slave 4 Register Address |
| 0x33 | I2C_SLV4_DO | R/W | 7:0 | I2C Slave 4 Data Output |
| 0x34 | I2C_SLV4_CTRL | R/W | 7:0 | I2C Slave 4 Control |
| 0x35 | I2C_SLV4_DI | R/O | 7:0 | I2C Slave 4 Data Input |
| 0x36 | I2C_MST_STATUS | R/O | 7:0 | I2C Master Status |

#### Interrupt Configuration (0x37-0x3A)
| Address | Register | R/W | Bits | Description |
|---------|----------|-----|------|-------------|
| 0x37 | INT_PIN_CFG | R/W | 7:0 | Interrupt Pin Configuration |
| 0x38 | INT_ENABLE | R/W | 7:0 | Interrupt Enable Control |
| 0x39 | INT_STATUS | R/O | 7:0 | Interrupt Status |
| 0x3A | INT_STATUS | R/O | 7:0 | Interrupt Status (duplicate reference) |

#### Sensor Data Registers (0x3B-0x48)
| Address | Register | R/W | Bits | Description |
|---------|----------|-----|------|-------------|
| 0x3B | ACCEL_XOUT_H | R/O | 15:8 | Accelerometer X-axis High Byte |
| 0x3C | ACCEL_XOUT_L | R/O | 7:0 | Accelerometer X-axis Low Byte |
| 0x3D | ACCEL_YOUT_H | R/O | 15:8 | Accelerometer Y-axis High Byte |
| 0x3E | ACCEL_YOUT_L | R/O | 7:0 | Accelerometer Y-axis Low Byte |
| 0x3F | ACCEL_ZOUT_H | R/O | 15:8 | Accelerometer Z-axis High Byte |
| 0x40 | ACCEL_ZOUT_L | R/O | 7:0 | Accelerometer Z-axis Low Byte |
| 0x41 | TEMP_OUT_H | R/O | 15:8 | Temperature High Byte |
| 0x42 | TEMP_OUT_L | R/O | 7:0 | Temperature Low Byte |
| 0x43 | GYRO_XOUT_H | R/O | 15:8 | Gyroscope X-axis High Byte |
| 0x44 | GYRO_XOUT_L | R/O | 7:0 | Gyroscope X-axis Low Byte |
| 0x45 | GYRO_YOUT_H | R/O | 15:8 | Gyroscope Y-axis High Byte |
| 0x46 | GYRO_YOUT_L | R/O | 7:0 | Gyroscope Y-axis Low Byte |
| 0x47 | GYRO_ZOUT_H | R/O | 15:8 | Gyroscope Z-axis High Byte |
| 0x48 | GYRO_ZOUT_L | R/O | 7:0 | Gyroscope Z-axis Low Byte |

#### External Sensor Data (0x49-0x60)
| Address | Register | R/W | Bits | Description |
|---------|----------|-----|------|-------------|
| 0x49 | EXT_SENS_DATA_00 | R/O | 7:0 | External Sensor Data 00 |
| ... | ... | ... | ... | (14 more registers) |
| 0x60 | EXT_SENS_DATA_23 | R/O | 7:0 | External Sensor Data 23 |

#### Control Registers (0x6A-0x75)
| Address | Register | R/W | Bits | Description |
|---------|----------|-----|------|-------------|
| 0x6A | USER_CTRL | R/W | 7:0 | User Control Register |
| 0x6B | PWR_MGMT_1 | R/W | 7:0 | Power Management 1 |
| 0x6C | PWR_MGMT_2 | R/W | 7:0 | Power Management 2 |
| 0x72 | FIFO_COUNTH | R/O | 12:8 | FIFO Count High Byte |
| 0x73 | FIFO_COUNTL | R/O | 7:0 | FIFO Count Low Byte |
| 0x74 | FIFO_R_W | R/W | 7:0 | FIFO Read/Write |
| 0x75 | WHO_AM_I | R/O | 7:0 | Device ID (0x68) |

### 1.2 Critical Register Details

#### CONFIG Register (0x1A) - DLPF Configuration
```
Bit  7  6  5  4  3  2  1  0
      0  0  0  0 EFS_SEL[1:0] DLPF_CFG[2:0]
```
- **Bits 2:0 (DLPF_CFG)**: Digital Low Pass Filter Configuration
  - 0: 260Hz acc / 256Hz gyro, 8kHz sampling
  - 1: 184Hz acc / 188Hz gyro, 1kHz sampling
  - 2: 94Hz acc / 98Hz gyro, 1kHz sampling
  - 3: 44Hz acc / 42Hz gyro, 1kHz sampling
  - 4: 21Hz acc / 20Hz gyro, 1kHz sampling
  - 5: 10Hz acc / 10Hz gyro, 1kHz sampling
  - 6: 5Hz acc / 5Hz gyro, 1kHz sampling
  - 7: Reserved

#### GYRO_CONFIG Register (0x1B)
```
Bit  7  6  5  4  3  2  1  0
   XSTF YSTF ZSTF  FS_SEL[1:0]    0
```
- **Bit 7**: Self-test X gyroscope
- **Bit 6**: Self-test Y gyroscope
- **Bit 5**: Self-test Z gyroscope
- **Bits 4:3 (FS_SEL)**: Full-Scale Select
  - 0: ±250°/s (131 LSB/°/s)
  - 1: ±500°/s (65.5 LSB/°/s)
  - 2: ±1000°/s (32.8 LSB/°/s)
  - 3: ±2000°/s (16.4 LSB/°/s)

#### ACCEL_CONFIG Register (0x1C)
```
Bit  7  6  5  4  3  2  1  0
   XSTF YSTF ZSTF AFS_SEL[1:0]    0
```
- **Bit 7**: Self-test X accelerometer
- **Bit 6**: Self-test Y accelerometer
- **Bit 5**: Self-test Z accelerometer
- **Bits 4:3 (AFS_SEL)**: Full-Scale Select
  - 0: ±2g (16,384 LSB/g)
  - 1: ±4g (8,192 LSB/g)
  - 2: ±8g (4,096 LSB/g)
  - 3: ±16g (2,048 LSB/g)

#### INT_ENABLE Register (0x38)
```
Bit  7  6  5  4  3  2  1  0
   FIFO MOT ZMOT FLF INT_I2C   SLV  DMP
   OVF         CWO            x
```
- **Bit 4**: FIFO Overflow Interrupt
- **Bit 3**: Motion Interrupt
- **Bit 2**: Zero Motion Interrupt
- **Bit 1**: Free Fall Interrupt
- **Bit 0**: Data Ready Interrupt

#### FIFO_EN Register (0x23)
```
Bit  7  6  5  4  3  2  1  0
   TEMP XG YG ZG ACCEL  SLV
   _EN  _EN _EN _EN     2 1 0
```
- **Bit 7**: Temperature FIFO Enable
- **Bit 6**: Gyro X FIFO Enable
- **Bit 5**: Gyro Y FIFO Enable
- **Bit 4**: Gyro Z FIFO Enable
- **Bit 3**: Accel FIFO Enable
- **Bits 2:0**: Slave data FIFO Enable

#### USER_CTRL Register (0x6A)
```
Bit  7  6  5  4  3  2  1  0
   FIFO I2C FIFO SIG_I DMP FIFO   I2C
   _RST MST _EN  COND  EN  OVER   EN
        EN         ITION
```
- **Bit 7**: FIFO Reset
- **Bit 6**: I2C Master Enable
- **Bit 5**: FIFO Enable
- **Bit 4**: Signal Conditioning (DMP only)
- **Bit 3**: DMP Enable
- **Bit 2**: FIFO Overflow Interrupt (read-only)
- **Bit 1**: I2C Interface Enable (always 1 for I2C operation)

#### PWR_MGMT_1 Register (0x6B)
```
Bit  7  6  5  4  3  2  1  0
   RST SLEEP CYCLE    TEMP   CLKSEL[2:0]
      _MODE _EN          _EN
```
- **Bit 7**: Reset
- **Bit 6**: Sleep Mode
- **Bit 5**: Cycle Mode
- **Bit 3**: Temperature Sensor Enable
- **Bits 2:0 (CLKSEL)**: Clock Source Selection
  - 0: Internal 8MHz oscillator
  - 1: PLL with X gyro reference
  - 2: PLL with Y gyro reference
  - 3: PLL with Z gyro reference
  - 4: PLL with external 32.768kHz reference
  - 5: PLL with external 19.2MHz reference
  - 6-7: Reserved

---

## 2. I2C PROTOCOL SPECIFICATIONS

### 2.1 I2C Electrical Characteristics
```
Parameter              Min    Typ    Max    Unit
Supply Voltage (VDD)   2.375  3.3    3.46   V
I/O Voltage (VLOGIC)   1.8    3.3    5.0    V
I2C Bus Speed          100    400    1000   kHz (fast mode recommended)
SDA/SCL Pull-up        4.7    10     47     kOhm
```

**ESP32-IDF Recommended Configuration**:
- Standard Mode: 100 kHz (highly reliable)
- Fast Mode: 400 kHz (most common production use)
- Maximum: 1 MHz (verify device tolerance)

### 2.2 I2C Slave Address Selection
```
MPU6050 Slave Address = 110100X (7 bits)
Where X = AD0 pin level:

AD0 = GND:  0x68 (1101000)
AD0 = VDD:  0x69 (1101001)
AD0 = SDA:  0x68 (floating high with pull-up)
AD0 = SCL:  0x69 (floating high with pull-up)
```

### 2.3 I2C Communication Protocol

#### Single Byte Read
```
Master: [START] [ADDR+R] [ACK] [READ BYTE] [NACK] [STOP]
Slave:           [ACK]          [DATA]
```

#### Multiple Byte Read (Burst Read)
```
Master: [START] [ADDR+R] [ACK] [READ] [ACK] [READ] [ACK] ... [READ] [NACK] [STOP]
Slave:          [ACK]    [DATA]      [DATA]      [DATA]      [DATA]
```

**Note**: The MPU6050 auto-increments the register address during burst reads. This is critical for reading multi-byte values (acceleration, gyroscope, temperature).

#### Single Byte Write
```
Master: [START] [ADDR+W] [ACK] [WRITE BYTE] [STOP]
Slave:          [ACK]         [ACK]
```

#### Multiple Byte Write (Burst Write)
```
Master: [START] [ADDR+W] [ACK] [WRITE] [ACK] [WRITE] [ACK] ... [STOP]
Slave:          [ACK]    [ACK] [ACK]  [ACK]
```

### 2.4 I2C Timing Specifications (T=25°C, VDD=3.3V)
```
Parameter                  Min    Max    Unit
SCL Frequency              100    400    kHz
START Condition Setup       600    -      ns
STOP Condition Hold         600    -      ns
SDA Hold Time               0      -      ns
SCL Low Time                4.7    -      µs (at 100kHz)
SCL Low Time                1.3    -      µs (at 400kHz)
SDA Setup Time              100    -      ns
Data Valid Time            -       900    ns
```

### 2.5 ESP32-IDF I2C Configuration Best Practices

#### Clock Stretching Handling
- **Enable on slave**: MPU6050 may hold SCL low during internal processing
- **Master timeout**: Set to 200ms minimum (default often insufficient)
- **Detection**: Monitor for frequent timeout conditions

#### Read/Write Implementation
```c
// Safe I2C read with error checking
esp_err_t mpu6050_read_register(i2c_port_t port, uint8_t reg, uint8_t *data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | 0, true);  // Write mode
    i2c_master_write_byte(cmd, reg, true);  // Register address
    i2c_master_start(cmd);  // Restart
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | 1, true);  // Read mode
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);  // Read with NACK
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}
```

---

## 3. SENSOR SPECIFICATIONS AND RANGES

### 3.1 Accelerometer Specifications

#### Full-Scale Ranges and Sensitivity
```
Range  Resolution  LSB/g      Min_Code  Max_Code  Typ Offset (mg)
±2g    16-bit      16,384     -32,768   +32,767   ±50
±4g    16-bit      8,192      -32,768   +32,767   ±100
±8g    16-bit      4,096      -32,768   +32,767   ±200
±16g   16-bit      2,048      -32,768   +32,767   ±400
```

#### Temperature Effects
```
Parameter                      Min    Typ    Max    Unit
Sensitivity Change             -3     0      +3     %
Zero-g Offset Change           -1     0      +1     mg/°C
```

#### Data Conversion Formula
```
Acceleration (g) = Raw_Value / LSB_Sensitivity
Acceleration (m/s²) = Raw_Value / LSB_Sensitivity * 9.81
Acceleration (mg) = Raw_Value / (LSB_Sensitivity / 1000)

Examples:
±2g range: a_g = raw / 16384
±4g range: a_g = raw / 8192
```

### 3.2 Gyroscope Specifications

#### Full-Scale Ranges and Sensitivity
```
Range      Resolution  LSB/(°/s)  Min_Code  Max_Code  Typ Offset (°/s)
±250°/s    16-bit      131        -32,768   +32,767   ±5
±500°/s    16-bit      65.5       -32,768   +32,767   ±10
±1000°/s   16-bit      32.8       -32,768   +32,767   ±20
±2000°/s   16-bit      16.4       -32,768   +32,767   ±40
```

#### Temperature Effects
```
Parameter                      Min    Typ    Max    Unit
Sensitivity Change             -2     0      +2     %
Zero Rate Offset Change        -2     0      +2     °/s/100°C
```

#### Data Conversion Formula
```
Angular Velocity (°/s) = Raw_Value / LSB_Sensitivity
Angular Velocity (rad/s) = Raw_Value / LSB_Sensitivity * π/180
Angular Velocity (°/s) = Raw_Value / (LSB_Sensitivity / 1000)

Examples:
±250°/s range: omega = raw / 131
±500°/s range: omega = raw / 65.5
```

### 3.3 Temperature Sensor

#### Specifications
```
Parameter                      Value      Unit
Temperature Range              -40 to +85 °C
Sensitivity                    340        LSB/°C
Reference Temperature          36.53      °C (at 0 LSB)
Temperature Accuracy           ±3         °C
```

#### Data Conversion Formula
```
Temperature (°C) = (Raw_Value / 340) + 36.53
Temperature (°C) = Raw_Value / 340.0 + 36.53
```

### 3.4 Critical Sensor Specifications Summary

#### Bandwidth and Noise
```
Parameter              ±2g Range  ±4g Range  ±8g Range  ±16g Range
Noise Density (Accel)  220 µg/√Hz 220 µg/√Hz 220 µg/√Hz 220 µg/√Hz
Noise Density (Gyro)   4 m°/s/√Hz 4 m°/s/√Hz 4 m°/s/√Hz 4 m°/s/√Hz
```

#### Sample Rate
- **Output Data Rate (ODR)**: Configurable via SMPLRT_DIV (0x19)
- **Maximum Internal Sample Rate**: 8 kHz (DLPF disabled)
- **Recommended**: 1 kHz with DLPF enabled

---

## 4. DIGITAL LOW-PASS FILTER (DLPF) CONFIGURATION

### 4.1 DLPF Settings and Characteristics

| DLPF_CFG | Accel BW | Accel Delay | Gyro BW | Gyro Delay | Output Rate | Notes |
|----------|----------|-------------|---------|------------|-------------|-------|
| 0 | 260 Hz | 0 ms | 256 Hz | 0.98 ms | 8 kHz | Minimal filtering |
| 1 | 184 Hz | 2.0 ms | 188 Hz | 1.9 ms | 1 kHz | Good balance |
| 2 | 94 Hz | 3.0 ms | 98 Hz | 2.8 ms | 1 kHz | Moderate filtering |
| 3 | 44 Hz | 4.9 ms | 42 Hz | 4.8 ms | 1 kHz | Strong filtering |
| 4 | 21 Hz | 8.5 ms | 20 Hz | 8.3 ms | 1 kHz | Very strong |
| 5 | 10 Hz | 13.8 ms | 10 Hz | 13.4 ms | 1 kHz | Aggressive |
| 6 | 5 Hz | 19.0 ms | 5 Hz | 18.6 ms | 1 kHz | Maximum |
| 7 | Reserved | - | Reserved | - | - | N/A |

### 4.2 DLPF Selection Guidance

**For Motion Tracking Applications**:
- DLPF_CFG = 1 or 2: Recommended for most IMU applications
- Provides good noise rejection without excessive delay
- 1 kHz output rate sufficient for control loops up to 500 Hz

**For High-Frequency Applications (Drones, Sports)**:
- DLPF_CFG = 0: Minimal delay, maximum bandwidth
- Accept higher noise in exchange for responsiveness
- Suitable for 8 kHz processing

**For Low-Power or Slow Applications**:
- DLPF_CFG = 5 or 6: Aggressive filtering
- Significantly reduces power consumption
- Acceptable 13-19ms latency

**For Accelerometer-Only Use**:
- DLPF_CFG = 2: 94Hz bandwidth balances noise and response
- Accidental g bias from vibration is filtered

### 4.3 Sample Rate Divider Configuration

```
Output_Sample_Rate = Internal_Sample_Rate / (1 + SMPLRT_DIV)

DLPF_CFG = 0:
  Internal_Sample_Rate = 8000 Hz
  For 1000 Hz output: SMPLRT_DIV = 7
  For 100 Hz output: SMPLRT_DIV = 79
  For 50 Hz output: SMPLRT_DIV = 159

DLPF_CFG = 1-6:
  Internal_Sample_Rate = 1000 Hz
  For 500 Hz output: SMPLRT_DIV = 1
  For 100 Hz output: SMPLRT_DIV = 9
  For 50 Hz output: SMPLRT_DIV = 19
```

### 4.4 Complete Configuration Example

```c
// Configure for 500 Hz sampling with moderate filtering
// 1. Set DLPF to mode 1 (184Hz BW, 1kHz internal rate)
mpu6050_write_register(i2c_port, 0x1A, 0x01);  // CONFIG: DLPF_CFG = 1

// 2. Set sample rate divider for 500 Hz
mpu6050_write_register(i2c_port, 0x19, 1);  // SMPLRT_DIV = 1 (1000/(1+1) = 500)

// 3. Set accelerometer range to ±4g
mpu6050_write_register(i2c_port, 0x1C, 0x08);  // ACCEL_CONFIG: AFS_SEL = 1

// 4. Set gyroscope range to ±500°/s
mpu6050_write_register(i2c_port, 0x1B, 0x08);  // GYRO_CONFIG: FS_SEL = 1

// 5. Enable gyro and accel in FIFO
mpu6050_write_register(i2c_port, 0x23, 0x78);  // FIFO_EN: all sensors enabled
```

---

## 5. SAMPLE RATE AND TIMING CONTROL

### 5.1 Sample Rate Formula

```
Sample_Rate = INTERNAL_CLOCK / (1 + SMPLRT_DIV)

Where:
  INTERNAL_CLOCK = 8000 Hz (if DLPF_CFG = 0)
  INTERNAL_CLOCK = 1000 Hz (if DLPF_CFG = 1-6)
  SMPLRT_DIV = value written to register 0x19 (0-255)
```

### 5.2 Recommended Sample Rates and Divider Values

#### For DLPF_CFG = 0 (8 kHz internal clock)
```
Desired Rate  SMPLRT_DIV  Actual Rate  Error
8000 Hz       0           8000 Hz      0%
4000 Hz       1           4000 Hz      0%
2000 Hz       3           2000 Hz      0%
1000 Hz       7           1000 Hz      0%
500 Hz        15          500 Hz       0%
250 Hz        31          250 Hz       0%
125 Hz        63          125 Hz       0%
100 Hz        79          100 Hz       0%
50 Hz         159         50 Hz        0%
25 Hz         319         25 Hz        0%
10 Hz         799         10 Hz        0%
```

#### For DLPF_CFG = 1-6 (1 kHz internal clock)
```
Desired Rate  SMPLRT_DIV  Actual Rate  Error
1000 Hz       0           1000 Hz      0%
500 Hz        1           500 Hz       0%
250 Hz        3           250 Hz       0%
200 Hz        4           200 Hz       0%
100 Hz        9           100 Hz       0%
50 Hz         19          50 Hz        0%
25 Hz         39          25 Hz        0%
20 Hz         49          20 Hz        0%
10 Hz         99          10 Hz        0%
5 Hz          199         5 Hz         0%
1 Hz          999         1 Hz         0%
```

### 5.3 Data Ready Interrupt for Precise Timing

```c
// Enable data ready interrupt (most reliable timing method)
mpu6050_write_register(i2c_port, 0x38, 0x01);  // INT_ENABLE: INTR_RDY = 1

// Attach GPIO interrupt to INT pin
gpio_set_intr_type(INT_GPIO, GPIO_INTR_NEGEDGE);  // Active low
gpio_install_isr_service(0);
gpio_isr_handler_add(INT_GPIO, isr_handler, (void*)i2c_port);

// In ISR: read all 14 bytes (6 accel + 2 temp + 6 gyro) atomically
void IRAM_ATTR isr_handler(void *arg) {
    i2c_port_t port = (i2c_port_t)arg;
    // Signal task to read data
    xTaskResumeFromISR(data_read_task_handle);
}
```

---

## 6. INTERRUPT CONFIGURATION

### 6.1 Interrupt Sources

| Bit | Register 0x38 | Description | Register |
|-----|---------------|-------------|----------|
| 0 | DATA_RDY_EN | New data ready for reading | 0x3B-0x48 |
| 1 | I2C_MST_INT_EN | I2C master interrupt (slave only) | 0x36 |
| 2 | FIFO_OFLOW_EN | FIFO overflow interrupt | 0x72-0x73 |
| 3 | ZMOT_EN | Zero motion interrupt | 0x20-0x22 |
| 4 | MOT_EN | Motion interrupt | 0x1F-0x20 |
| 5 | FF_EN | Free fall interrupt | 0x1D-0x1E |
| 6-7 | Reserved | - | - |

### 6.2 Interrupt Pin Configuration (Register 0x37)

```
Bit  Description                    Values
0    INT_LEVEL                      0=active high, 1=active low
1    INT_OPEN                       0=push-pull, 1=open-drain
2    LATCH_INT_EN                   0=pulse, 1=hold until cleared
3    INT_RD_CLEAR                   0=manual clear, 1=cleared on read
4    FSYNC_INT_LEVEL                0=active high, 1=active low
5    FSYNC_INT_MODE_EN              0=disabled, 1=enabled
6    I2C_BYPASS_EN                  0=disabled, 1=bypasses I2C master
7    CLKOUT_EN                      0=disabled, 1=outputs sample clock
```

### 6.3 Interrupt Setup Example

```c
// Configure interrupt pin for active-low, latch mode
uint8_t int_pin_cfg = 0x80;  // LATCH_INT_EN | INT_LEVEL (active low)
mpu6050_write_register(i2c_port, 0x37, int_pin_cfg);

// Enable data ready interrupt
mpu6050_write_register(i2c_port, 0x38, 0x01);  // DATA_RDY_EN

// Read interrupt status to clear
uint8_t int_status;
mpu6050_read_register(i2c_port, 0x3A, &int_status);

// On INT pin edge: read data immediately
if (int_status & 0x01) {  // DATA_RDY bit
    // Read 14 bytes atomically: accel(6) + temp(2) + gyro(6)
    uint8_t data[14];
    mpu6050_read_registers(i2c_port, 0x3B, 14, data);
}
```

---

## 7. FIFO OPERATION AND MANAGEMENT

### 7.1 FIFO Architecture

**Capacity**: 1024 bytes (4 KB maximum)
**Data Width**: 8 bits per access
**Auto-increment**: Address auto-increments on burst reads
**Reset**: Manual reset via USER_CTRL register bit 7

### 7.2 FIFO Data Packet Format

When FIFO_EN is configured with sensors, each sample is stored as consecutive bytes:

```
Packet Structure (if all 3 sensors + temp enabled):
Byte  Content          Bits   Format
0-1   ACCEL_XOUT       15:0   Signed 16-bit
2-3   ACCEL_YOUT       15:0   Signed 16-bit
4-5   ACCEL_ZOUT       15:0   Signed 16-bit
6-7   TEMP_OUT         15:0   Signed 16-bit
8-9   GYRO_XOUT        15:0   Signed 16-bit
10-11 GYRO_YOUT        15:0   Signed 16-bit
12-13 GYRO_ZOUT        15:0   Signed 16-bit

Total: 14 bytes per sample
Max samples: 1024 / 14 = 73 samples
```

### 7.3 FIFO Enable Register (0x23)

```
Bit  Name              Effect
0    SLV_0_FIFO_EN     Enable slave 0 data to FIFO
1    SLV_1_FIFO_EN     Enable slave 1 data to FIFO
2    SLV_2_FIFO_EN     Enable slave 2 data to FIFO
3    ACCEL_FIFO_EN     Enable accelerometer to FIFO
4    GYRO_ZOUT_FIFO_EN Enable gyro Z to FIFO
5    GYRO_YOUT_FIFO_EN Enable gyro Y to FIFO
6    GYRO_XOUT_FIFO_EN Enable gyro X to FIFO
7    TEMP_FIFO_EN      Enable temperature to FIFO
```

### 7.4 FIFO Count Registers (0x72-0x73)

```
Register  Name          Bits   Max Value
0x72      FIFO_COUNTH   12:8   0x03 (upper 5 bits only)
0x73      FIFO_COUNTL   7:0    0xFF

Total FIFO Count = (FIFO_COUNTH << 8) | FIFO_COUNTL
Max FIFO Count = 1024 bytes
```

### 7.5 FIFO Overflow and Protection

**Overflow Condition**:
- FIFO is full (count = 1024) and new data arrives
- Writing continues to FIFO, oldest data is discarded
- FIFO overflow flag is set in INT_STATUS register (0x3A, bit 4)

**Protection Strategy** (CRITICAL for memory safety):

```c
#define FIFO_SIZE 1024
#define MAX_FIFO_SAMPLES 73  // 1024 / 14 bytes per sample
#define SAMPLE_SIZE 14

typedef struct {
    int16_t accel[3];   // 6 bytes
    int16_t temp;       // 2 bytes
    int16_t gyro[3];    // 6 bytes
} IMU_Sample_t;

// Safe FIFO read with bounds checking
esp_err_t mpu6050_read_fifo_safe(i2c_port_t port, IMU_Sample_t *samples,
                                  uint16_t max_count, uint16_t *read_count) {
    uint8_t fifo_h, fifo_l;

    // Read FIFO count
    mpu6050_read_register(port, 0x72, &fifo_h);
    mpu6050_read_register(port, 0x73, &fifo_l);

    // CRITICAL: Convert to uint16_t to prevent overflow
    uint16_t fifo_count = ((uint16_t)fifo_h << 8) | (uint16_t)fifo_l;

    // Validate bounds
    if (fifo_count > FIFO_SIZE) {
        // ERROR: FIFO corrupted, reset it
        mpu6050_write_register(port, 0x6A, 0x84);  // FIFO_RST
        mpu6050_write_register(port, 0x6A, 0x04);  // Enable FIFO
        *read_count = 0;
        return ESP_ERR_INVALID_STATE;
    }

    // Calculate how many complete samples we can read
    uint16_t num_samples = fifo_count / SAMPLE_SIZE;
    if (num_samples > max_count) {
        // Too many samples, reset FIFO to prevent overflow
        mpu6050_write_register(port, 0x6A, 0x84);  // FIFO_RST
        return ESP_ERR_INVALID_SIZE;
    }

    // Read samples safely
    uint8_t buffer[SAMPLE_SIZE];
    for (int i = 0; i < num_samples; i++) {
        mpu6050_read_registers(port, 0x74, SAMPLE_SIZE, buffer);
        samples[i].accel[0] = (int16_t)((buffer[0] << 8) | buffer[1]);
        samples[i].accel[1] = (int16_t)((buffer[2] << 8) | buffer[3]);
        samples[i].accel[2] = (int16_t)((buffer[4] << 8) | buffer[5]);
        samples[i].temp = (int16_t)((buffer[6] << 8) | buffer[7]);
        samples[i].gyro[0] = (int16_t)((buffer[8] << 8) | buffer[9]);
        samples[i].gyro[1] = (int16_t)((buffer[10] << 8) | buffer[11]);
        samples[i].gyro[2] = (int16_t)((buffer[12] << 8) | buffer[13]);
    }

    *read_count = num_samples;
    return ESP_OK;
}
```

### 7.6 FIFO Enable/Disable Procedure

```c
// Enable FIFO
void mpu6050_fifo_enable(i2c_port_t port) {
    // 1. Disable FIFO first
    mpu6050_write_register(port, 0x6A, 0x04);  // USER_CTRL: FIFO disabled

    // 2. Reset FIFO
    mpu6050_write_register(port, 0x6A, 0x84);  // USER_CTRL: FIFO_RST

    // 3. Configure data to go into FIFO
    mpu6050_write_register(port, 0x23, 0x78);  // FIFO_EN: accel + gyro

    // 4. Enable FIFO
    mpu6050_write_register(port, 0x6A, 0x44);  // USER_CTRL: FIFO_EN

    // 5. Enable FIFO overflow interrupt
    mpu6050_write_register(port, 0x38, 0x15);  // INT_ENABLE: FIFO_OFLOW_EN | DATA_RDY_EN
}

// Disable FIFO
void mpu6050_fifo_disable(i2c_port_t port) {
    // Disable sensors from FIFO
    mpu6050_write_register(port, 0x23, 0x00);  // FIFO_EN: nothing

    // Disable FIFO
    mpu6050_write_register(port, 0x6A, 0x04);  // USER_CTRL: FIFO disabled

    // Clear FIFO
    mpu6050_write_register(port, 0x6A, 0x84);  // USER_CTRL: FIFO_RST
}

// Clear FIFO on overflow
void mpu6050_clear_fifo_overflow(i2c_port_t port) {
    // Read and clear INT_STATUS
    uint8_t int_status;
    mpu6050_read_register(port, 0x3A, &int_status);

    if (int_status & 0x10) {  // FIFO_OFLOW bit
        // Reset FIFO
        mpu6050_write_register(port, 0x6A, 0x84);
        mpu6050_write_register(port, 0x6A, 0x44);  // Re-enable FIFO
    }
}
```

---

## 8. SELF-TEST AND CALIBRATION

### 8.1 Self-Test Procedure

Self-test applies a self-generated signal to the sensors and compares the output to factory values.

**Registers Involved**:
```
0x13-0x18  Gyroscope User Offset Trim
0x06-0x0B  Accelerometer User Offset Trim
0x0D-0x10  Self-Test Factory Values
0x1B       GYRO_CONFIG (self-test enable bits)
0x1C       ACCEL_CONFIG (self-test enable bits)
```

### 8.2 Self-Test Implementation

```c
typedef struct {
    float accel[3];
    float gyro[3];
} SelfTestResult_t;

// Perform factory self-test
esp_err_t mpu6050_self_test(i2c_port_t port, SelfTestResult_t *result) {
    uint8_t raw_data[4];
    float factory_trim[6];
    float st_response[6];

    // 1. Read factory trim values
    mpu6050_read_register(port, 0x0D, &raw_data[0]);  // SELF_TEST_X
    mpu6050_read_register(port, 0x0E, &raw_data[1]);  // SELF_TEST_Y
    mpu6050_read_register(port, 0x0F, &raw_data[2]);  // SELF_TEST_Z
    mpu6050_read_register(port, 0x10, &raw_data[3]);  // SELF_TEST_A

    // Extract self-test trim values
    factory_trim[0] = (float)(2620.0 * pow(1.01, (float)(raw_data[0] - 1)));  // X accel
    factory_trim[1] = (float)(2620.0 * pow(1.01, (float)(raw_data[1] - 1)));  // Y accel
    factory_trim[2] = (float)(2620.0 * pow(1.01, (float)(raw_data[2] - 1)));  // Z accel
    factory_trim[3] = (float)(2620.0 * pow(1.01, (float)(raw_data[3] - 1)));  // X gyro
    factory_trim[4] = (float)(2620.0 * pow(1.01, (float)(raw_data[3] - 1)));  // Y gyro
    factory_trim[5] = (float)(2620.0 * pow(1.01, (float)(raw_data[3] - 1)));  // Z gyro

    // 2. Disable FIFO and set high sample rate
    mpu6050_write_register(port, 0x6A, 0x04);  // Disable FIFO
    mpu6050_write_register(port, 0x19, 0x00);  // SMPLRT_DIV = 0 (8kHz)
    mpu6050_write_register(port, 0x1A, 0x00);  // DLPF = 0
    mpu6050_write_register(port, 0x1C, 0x08);  // ACCEL ±4g
    mpu6050_write_register(port, 0x1B, 0x08);  // GYRO ±500°/s

    // 3. Measure normal response (self-test disabled)
    vTaskDelay(200 / portTICK_RATE_MS);  // Wait for settling
    int16_t accel_normal[3], gyro_normal[3];
    mpu6050_read_accel(port, accel_normal);
    mpu6050_read_gyro(port, gyro_normal);

    // 4. Enable self-test
    mpu6050_write_register(port, 0x1C, 0xE8);  // ACCEL: all self-test enabled
    mpu6050_write_register(port, 0x1B, 0xE8);  // GYRO: all self-test enabled

    vTaskDelay(200 / portTICK_RATE_MS);  // Wait for settling
    int16_t accel_st[3], gyro_st[3];
    mpu6050_read_accel(port, accel_st);
    mpu6050_read_gyro(port, gyro_st);

    // 5. Calculate self-test response
    st_response[0] = (float)(accel_st[0] - accel_normal[0]);
    st_response[1] = (float)(accel_st[1] - accel_normal[1]);
    st_response[2] = (float)(accel_st[2] - accel_normal[2]);
    st_response[3] = (float)(gyro_st[0] - gyro_normal[0]);
    st_response[4] = (float)(gyro_st[1] - gyro_normal[1]);
    st_response[5] = (float)(gyro_st[2] - gyro_normal[2]);

    // 6. Compare with factory trim (acceptance: 50% to 150%)
    bool accel_test_pass = true, gyro_test_pass = true;

    for (int i = 0; i < 3; i++) {
        float ratio = (st_response[i] / factory_trim[i]);
        if (ratio < 0.5 || ratio > 1.5) {
            result->accel[i] = ratio;
            accel_test_pass = false;
        }
    }

    for (int i = 0; i < 3; i++) {
        float ratio = (st_response[i+3] / factory_trim[i+3]);
        if (ratio < 0.5 || ratio > 1.5) {
            result->gyro[i] = ratio;
            gyro_test_pass = false;
        }
    }

    // 7. Restore normal configuration
    mpu6050_write_register(port, 0x1C, 0x08);  // ACCEL: disable self-test
    mpu6050_write_register(port, 0x1B, 0x08);  // GYRO: disable self-test

    return (accel_test_pass && gyro_test_pass) ? ESP_OK : ESP_FAIL;
}
```

### 8.3 Gyroscope Offset Calibration

```c
typedef struct {
    int16_t offset[3];
    float sensitivity;
} GyroCalibration_t;

// Perform gyroscope offset calibration (zero-rate offset)
esp_err_t mpu6050_calibrate_gyro(i2c_port_t port, GyroCalibration_t *calib) {
    int32_t sum[3] = {0, 0, 0};
    int16_t raw[3];

    // Collect 100 samples while sensor is stationary
    for (int i = 0; i < 100; i++) {
        mpu6050_read_gyro(port, raw);
        sum[0] += raw[0];
        sum[1] += raw[1];
        sum[2] += raw[2];
        vTaskDelay(10 / portTICK_RATE_MS);  // 10ms between reads
    }

    // Calculate average offset
    calib->offset[0] = sum[0] / 100;
    calib->offset[1] = sum[1] / 100;
    calib->offset[2] = sum[2] / 100;

    // Store in device trim registers (register 0x13-0x18)
    mpu6050_write_register(port, 0x13, (calib->offset[0] >> 8) & 0xFF);
    mpu6050_write_register(port, 0x14, calib->offset[0] & 0xFF);
    mpu6050_write_register(port, 0x15, (calib->offset[1] >> 8) & 0xFF);
    mpu6050_write_register(port, 0x16, calib->offset[1] & 0xFF);
    mpu6050_write_register(port, 0x17, (calib->offset[2] >> 8) & 0xFF);
    mpu6050_write_register(port, 0x18, calib->offset[2] & 0xFF);

    return ESP_OK;
}

// Perform accelerometer offset calibration
esp_err_t mpu6050_calibrate_accel(i2c_port_t port, int16_t accel_offset[3]) {
    int32_t sum[3] = {0, 0, 0};
    int16_t raw[3];

    // Collect 100 samples with Z facing up
    for (int i = 0; i < 100; i++) {
        mpu6050_read_accel(port, raw);
        sum[0] += raw[0];
        sum[1] += raw[1];
        sum[2] += raw[2] - 16384;  // Subtract 1g from Z axis
        vTaskDelay(10 / portTICK_RATE_MS);
    }

    // Calculate average offset
    accel_offset[0] = sum[0] / 100;
    accel_offset[1] = sum[1] / 100;
    accel_offset[2] = sum[2] / 100;

    // Store in device trim registers (register 0x06-0x0B)
    // Note: Lower 8 bits only, upper bits stored elsewhere
    int16_t offset_h, offset_l;
    for (int i = 0; i < 3; i++) {
        offset_h = (accel_offset[i] >> 8) & 0xFF;
        offset_l = (accel_offset[i] & 0xFF) >> 3;  // Only 5 LSBs used
        mpu6050_write_register(port, 0x06 + (i*2), offset_h);
        mpu6050_write_register(port, 0x07 + (i*2), offset_l);
    }

    return ESP_OK;
}
```

---

## 9. SECURITY AND RELIABILITY CONSIDERATIONS

### 9.1 FIFO Overflow Handling

**Vulnerability**: FIFO overflow can cause data loss or buffer overruns

**Mitigation Strategy**:
```c
// 1. Use uint16_t for FIFO count to prevent integer overflow
uint16_t fifo_count = ((uint16_t)count_h << 8) | (uint16_t)count_l;

// 2. Implement bounds checking before reading
#define MAX_SAFE_FIFO_SAMPLES 70  // Leave margin for safety
if (fifo_count > (MAX_SAFE_FIFO_SAMPLES * 14)) {
    // Reset FIFO to prevent overflow
    mpu6050_write_register(port, 0x6A, 0x84);  // FIFO_RST
    return ESP_ERR_INVALID_STATE;
}

// 3. Monitor FIFO overflow interrupt
uint8_t int_status;
mpu6050_read_register(port, 0x3A, &int_status);
if (int_status & 0x10) {  // FIFO overflow bit
    // Handle overflow: reset FIFO and resume
    mpu6050_write_register(port, 0x6A, 0x84);
    mpu6050_write_register(port, 0x6A, 0x44);
    // Log error for diagnostics
    ESP_LOGE("MPU6050", "FIFO overflow detected");
}
```

### 9.2 Invalid Range Detection

**Vulnerability**: Sensor reads may return invalid ranges indicating hardware failure

**Detection Code**:
```c
#define ACCEL_MAX_VALID 32767   // Max int16_t
#define GYRO_MAX_VALID 32767

esp_err_t mpu6050_validate_data(int16_t accel[3], int16_t gyro[3], int16_t temp) {
    // Check for invalid values (all 0xFF or all 0x00)
    if ((accel[0] == 0x7FFF && accel[1] == 0x7FFF && accel[2] == 0x7FFF) ||
        (accel[0] == 0 && accel[1] == 0 && accel[2] == 0)) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    if ((gyro[0] == 0x7FFF && gyro[1] == 0x7FFF && gyro[2] == 0x7FFF) ||
        (gyro[0] == 0 && gyro[1] == 0 && gyro[2] == 0)) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Check temperature in valid range (-40°C to +85°C)
    int16_t temp_min = (int16_t)((-40 - 36.53) * 340);  // ~-26000
    int16_t temp_max = (int16_t)((85 - 36.53) * 340);   // ~16500
    if (temp < temp_min || temp > temp_max) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}
```

### 9.3 I2C Clock Stretching and Timeouts

**Vulnerability**: I2C bus can hang during clock stretching or communication errors

**Safe Implementation**:
```c
// Configure I2C with extended timeout for clock stretching
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400000,  // 400 kHz
    .clk_flags = 0,
};

// Critical: Set slave timeout to allow clock stretching
i2c_param_config(I2C_NUM_0, &conf);
i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

// Set slave response timeout - allows for sensor processing delays
i2c_set_timeout(I2C_NUM_0, 20000);  // 20ms timeout (MPU6050 may need 5-10ms)

// Safe I2C transaction with error recovery
esp_err_t mpu6050_safe_read(i2c_port_t port, uint8_t reg, uint8_t *data) {
    for (int retry = 0; retry < 3; retry++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | 0, true);
        i2c_master_write_byte(cmd, reg, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | 1, true);
        i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            return ESP_OK;
        }
        if (ret == ESP_ERR_TIMEOUT) {
            // Clock stretching or bus hang - try reset
            if (retry < 2) {
                vTaskDelay(10 / portTICK_RATE_MS);
            }
        }
    }
    return ESP_FAIL;
}
```

### 9.4 Interrupt Race Conditions

**Vulnerability**: Data may be read while sensor is writing to output registers

**Solution: Atomic Reads**:
```c
// Read all 14 bytes atomically in burst to prevent tearing
esp_err_t mpu6050_read_all_atomic(i2c_port_t port, uint8_t *data_14bytes) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | 0, true);
    i2c_master_write_byte(cmd, 0x3B, true);  // ACCEL_XOUT_H
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | 1, true);

    // Read 14 bytes with ACK for all but last byte (NACK)
    for (int i = 0; i < 13; i++) {
        i2c_master_read_byte(cmd, &data_14bytes[i], I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &data_14bytes[13], I2C_MASTER_NACK);

    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

// Use data ready interrupt to trigger atomic reads
void IRAM_ATTR mpu6050_isr(void *arg) {
    // Signal task to read 14 bytes immediately
    BaseType_t higher_priority_woken = pdFALSE;
    xTaskResumeFromISRNotify(data_task_handle, NULL, &higher_priority_woken);
    if (higher_priority_woken) {
        portYIELD_FROM_ISR();
    }
}
```

---

## 10. MEMORY SAFETY FOR C DRIVERS

### 10.1 FIFO Buffer Management

**Risk**: Buffer overflow when reading more data than allocated

```c
#define FIFO_MAX_SAMPLES 73
#define SAMPLE_BYTES 14

typedef struct {
    int16_t accel[3];
    int16_t temp;
    int16_t gyro[3];
} IMU_Sample_t;

typedef struct {
    IMU_Sample_t samples[FIFO_MAX_SAMPLES];
    uint16_t count;
    uint16_t head;
} FIFO_Buffer_t;

// Safe buffer read with strict bounds
esp_err_t fifo_buffer_read(FIFO_Buffer_t *fifo, i2c_port_t port) {
    uint8_t fifo_h, fifo_l;

    // Read FIFO count with validation
    if (mpu6050_read_register(port, 0x72, &fifo_h) != ESP_OK ||
        mpu6050_read_register(port, 0x73, &fifo_l) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t byte_count = ((uint16_t)fifo_h << 8) | (uint16_t)fifo_l;

    // Strict validation
    if (byte_count > FIFO_SIZE || byte_count % SAMPLE_BYTES != 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint16_t sample_count = byte_count / SAMPLE_BYTES;
    if (sample_count > FIFO_MAX_SAMPLES) {
        return ESP_ERR_INVALID_SIZE;
    }

    // Read with bounds checking
    uint8_t buffer[SAMPLE_BYTES];
    for (uint16_t i = 0; i < sample_count; i++) {
        // Check buffer bounds before write
        if (fifo->count >= FIFO_MAX_SAMPLES) {
            return ESP_ERR_NO_MEM;
        }

        if (mpu6050_read_registers(port, 0x74, SAMPLE_BYTES, buffer) != ESP_OK) {
            return ESP_ERR_INVALID_RESPONSE;
        }

        // Parse with explicit casting for safety
        fifo->samples[fifo->count].accel[0] =
            (int16_t)(((int16_t)buffer[0] << 8) | (int16_t)buffer[1]);
        fifo->samples[fifo->count].accel[1] =
            (int16_t)(((int16_t)buffer[2] << 8) | (int16_t)buffer[3]);
        fifo->samples[fifo->count].accel[2] =
            (int16_t)(((int16_t)buffer[4] << 8) | (int16_t)buffer[5]);

        fifo->samples[fifo->count].temp =
            (int16_t)(((int16_t)buffer[6] << 8) | (int16_t)buffer[7]);

        fifo->samples[fifo->count].gyro[0] =
            (int16_t)(((int16_t)buffer[8] << 8) | (int16_t)buffer[9]);
        fifo->samples[fifo->count].gyro[1] =
            (int16_t)(((int16_t)buffer[10] << 8) | (int16_t)buffer[11]);
        fifo->samples[fifo->count].gyro[2] =
            (int16_t)(((int16_t)buffer[12] << 8) | (int16_t)buffer[13]);

        fifo->count++;
    }

    return ESP_OK;
}
```

### 10.2 Signed vs Unsigned Data Conversion

**Critical Issue**: MPU6050 returns signed 16-bit data; incorrect conversion causes sign inversion

```c
// INCORRECT: Creates unsigned interpretation (0-65535 instead of -32768 to +32767)
uint16_t wrong_raw = (buffer[0] << 8) | buffer[1];
float wrong_value = wrong_raw / 16384.0;  // Results in 0 to 4g instead of -2 to +2g

// CORRECT: Explicitly cast to signed
int16_t correct_raw = (int16_t)((buffer[0] << 8) | buffer[1]);
float correct_value = correct_raw / 16384.0;  // Results in -2 to +2g

// Safe function with explicit typing
void parse_imu_data(const uint8_t *raw_data, int16_t *accel, int16_t *gyro, int16_t *temp) {
    // All intermediate calculations are signed int16_t
    for (int i = 0; i < 3; i++) {
        // Explicitly cast high byte to int16_t first
        int16_t high = (int16_t)raw_data[i*2];
        uint8_t low = raw_data[i*2 + 1];
        // Shift high byte, then OR with low byte
        accel[i] = (high << 8) | low;
    }

    // Same for temperature (signed)
    int16_t temp_high = (int16_t)raw_data[6];
    uint8_t temp_low = raw_data[7];
    *temp = (temp_high << 8) | temp_low;

    // Same for gyroscope
    for (int i = 0; i < 3; i++) {
        int16_t high = (int16_t)raw_data[8 + i*2];
        uint8_t low = raw_data[8 + i*2 + 1];
        gyro[i] = (high << 8) | low;
    }
}
```

### 10.3 16-bit Data Assembly and Byte Order

**Important**: All 16-bit data is Big-Endian (MSB first)

```c
// Proper 16-bit assembly from MSB/LSB pair
static inline int16_t assemble_int16(uint8_t msb, uint8_t lsb) {
    return (int16_t)((msb << 8) | lsb);
}

// Version with explicit sign handling
static inline int16_t assemble_int16_safe(uint8_t msb, uint8_t lsb) {
    // Step 1: Combine bytes
    uint16_t combined = ((uint16_t)msb << 8) | (uint16_t)lsb;

    // Step 2: Reinterpret as signed (2's complement)
    return (int16_t)combined;
}

// Macro for consistency
#define ASSEMBLE_INT16(msb, lsb) ((int16_t)(((msb) << 8) | (lsb)))

// Array conversion
void convert_bytes_to_int16(const uint8_t *bytes, int16_t *values, uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        values[i] = ASSEMBLE_INT16(bytes[i*2], bytes[i*2 + 1]);
    }
}
```

### 10.4 Register Access Bounds Checking

```c
#define MPU6050_REG_MIN 0x00
#define MPU6050_REG_MAX 0x75

esp_err_t mpu6050_read_register_safe(i2c_port_t port, uint8_t reg, uint8_t *value) {
    // Validate register address
    if (reg < MPU6050_REG_MIN || reg > MPU6050_REG_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    // Validate pointer
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return mpu6050_read_register(port, reg, value);
}

esp_err_t mpu6050_read_registers_safe(i2c_port_t port, uint8_t reg_start,
                                       uint16_t count, uint8_t *buffer) {
    // Validate starting address
    if (reg_start < MPU6050_REG_MIN || reg_start > MPU6050_REG_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    // Validate read doesn't exceed address space
    if (reg_start + count > (MPU6050_REG_MAX + 1)) {
        return ESP_ERR_INVALID_SIZE;
    }

    // Validate buffer pointer
    if (buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return mpu6050_read_registers(port, reg_start, count, buffer);
}
```

### 10.5 Stack Allocation Safety

**Best Practice**: Avoid large stack allocations; use static buffers

```c
// BAD: Stack allocation may cause stack overflow
void read_fifo_bad(i2c_port_t port) {
    uint8_t buffer[1024];  // 1KB on stack - risky!
    mpu6050_read_registers(port, 0x74, 1024, buffer);
}

// GOOD: Static or heap allocation
static uint8_t fifo_buffer[1024];  // Static buffer

void read_fifo_good(i2c_port_t port) {
    mpu6050_read_registers(port, 0x74, 1024, fifo_buffer);
}

// BEST: Use context structure with pre-allocated buffers
typedef struct {
    uint8_t fifo_buffer[1024];
    uint8_t sample_buffer[14];
    i2c_port_t port;
} MPU6050_Context_t;

esp_err_t mpu6050_init_context(MPU6050_Context_t *ctx, i2c_port_t port) {
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ctx->port = port;
    return ESP_OK;
}
```

---

## 11. COMPLETE DRIVER EXAMPLE

```c
// mpu6050_driver.h
#ifndef MPU6050_DRIVER_H
#define MPU6050_DRIVER_H

#include <stdint.h>
#include <string.h>
#include "driver/i2c.h"
#include "esp_err.h"

#define MPU6050_ADDR_AD0_LOW  0x68
#define MPU6050_ADDR_AD0_HIGH 0x69

// Register addresses
#define REG_SMPLRT_DIV   0x19
#define REG_CONFIG       0x1A
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_INT_ENABLE   0x38
#define REG_INT_STATUS   0x3A
#define REG_ACCEL_XOUT_H 0x3B
#define REG_TEMP_OUT_H   0x41
#define REG_GYRO_XOUT_H  0x43
#define REG_USER_CTRL    0x6A
#define REG_PWR_MGMT_1   0x6B
#define REG_FIFO_COUNTH  0x72
#define REG_FIFO_COUNTL  0x73
#define REG_FIFO_R_W     0x74
#define REG_WHO_AM_I     0x75

// Sensor data structure
typedef struct {
    int16_t accel[3];  // X, Y, Z
    int16_t temp;
    int16_t gyro[3];   // X, Y, Z
} mpu6050_data_t;

// Configuration structure
typedef struct {
    uint8_t dlpf_cfg;
    uint8_t sample_rate_div;
    uint8_t gyro_range;    // 0=250, 1=500, 2=1000, 3=2000
    uint8_t accel_range;   // 0=2g, 1=4g, 2=8g, 3=16g
} mpu6050_config_t;

// Driver context
typedef struct {
    i2c_port_t port;
    uint8_t addr;
    mpu6050_config_t config;
} mpu6050_handle_t;

// Function prototypes
esp_err_t mpu6050_init(mpu6050_handle_t *handle, i2c_port_t port, uint8_t addr);
esp_err_t mpu6050_configure(mpu6050_handle_t *handle, const mpu6050_config_t *config);
esp_err_t mpu6050_read_data(mpu6050_handle_t *handle, mpu6050_data_t *data);
esp_err_t mpu6050_read_accel(mpu6050_handle_t *handle, int16_t accel[3]);
esp_err_t mpu6050_read_gyro(mpu6050_handle_t *handle, int16_t gyro[3]);
esp_err_t mpu6050_read_temp(mpu6050_handle_t *handle, int16_t *temp);
esp_err_t mpu6050_fifo_enable(mpu6050_handle_t *handle);
esp_err_t mpu6050_fifo_read(mpu6050_handle_t *handle, mpu6050_data_t *samples,
                             uint16_t max_count, uint16_t *read_count);

#endif  // MPU6050_DRIVER_H
```

```c
// mpu6050_driver.c
#include "mpu6050_driver.h"

static const char *TAG = "MPU6050";

// Helper: Write register
static esp_err_t mpu6050_write_register(mpu6050_handle_t *handle, uint8_t reg,
                                        uint8_t value) {
    uint8_t write_buf[2] = {reg, value};

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, 2, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(handle->port, cmd,
                                         1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

// Helper: Read register
static esp_err_t mpu6050_read_register(mpu6050_handle_t *handle, uint8_t reg,
                                       uint8_t *value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(handle->port, cmd,
                                         1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

// Helper: Read multiple registers
static esp_err_t mpu6050_read_registers(mpu6050_handle_t *handle, uint8_t reg_start,
                                        uint16_t count, uint8_t *buffer) {
    if (buffer == NULL || count == 0 || count > 128) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_start, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->addr << 1) | I2C_MASTER_READ, true);

    for (uint16_t i = 0; i < count - 1; i++) {
        i2c_master_read_byte(cmd, &buffer[i], I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &buffer[count - 1], I2C_MASTER_NACK);

    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(handle->port, cmd,
                                         1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}

// Initialize driver
esp_err_t mpu6050_init(mpu6050_handle_t *handle, i2c_port_t port, uint8_t addr) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->port = port;
    handle->addr = addr;

    // Check WHO_AM_I
    uint8_t who_am_i;
    if (mpu6050_read_register(handle, REG_WHO_AM_I, &who_am_i) != ESP_OK) {
        return ESP_FAIL;
    }

    if (who_am_i != 0x68 && who_am_i != 0x71) {  // 0x68 for AD0=0, 0x71 for MPU6000
        return ESP_ERR_NOT_FOUND;
    }

    // Wake up from sleep mode
    if (mpu6050_write_register(handle, REG_PWR_MGMT_1, 0x00) != ESP_OK) {
        return ESP_FAIL;
    }

    vTaskDelay(100 / portTICK_RATE_MS);

    // Default configuration
    handle->config.dlpf_cfg = 1;        // 184Hz BW
    handle->config.sample_rate_div = 9; // 100Hz output
    handle->config.gyro_range = 1;      // ±500°/s
    handle->config.accel_range = 1;     // ±4g

    return mpu6050_configure(handle, &handle->config);
}

// Configure sensor
esp_err_t mpu6050_configure(mpu6050_handle_t *handle, const mpu6050_config_t *config) {
    if (handle == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Validate ranges
    if (config->dlpf_cfg > 6 || config->gyro_range > 3 || config->accel_range > 3) {
        return ESP_ERR_INVALID_ARG;
    }

    // Set DLPF
    if (mpu6050_write_register(handle, REG_CONFIG, config->dlpf_cfg & 0x07) != ESP_OK) {
        return ESP_FAIL;
    }

    // Set gyroscope range
    uint8_t gyro_cfg = (config->gyro_range & 0x03) << 3;
    if (mpu6050_write_register(handle, REG_GYRO_CONFIG, gyro_cfg) != ESP_OK) {
        return ESP_FAIL;
    }

    // Set accelerometer range
    uint8_t accel_cfg = (config->accel_range & 0x03) << 3;
    if (mpu6050_write_register(handle, REG_ACCEL_CONFIG, accel_cfg) != ESP_OK) {
        return ESP_FAIL;
    }

    // Set sample rate
    if (mpu6050_write_register(handle, REG_SMPLRT_DIV, config->sample_rate_div) != ESP_OK) {
        return ESP_FAIL;
    }

    memcpy(&handle->config, config, sizeof(mpu6050_config_t));
    return ESP_OK;
}

// Read all sensor data
esp_err_t mpu6050_read_data(mpu6050_handle_t *handle, mpu6050_data_t *data) {
    if (handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[14];
    if (mpu6050_read_registers(handle, REG_ACCEL_XOUT_H, 14, buffer) != ESP_OK) {
        return ESP_FAIL;
    }

    // Parse data (signed 16-bit values)
    data->accel[0] = (int16_t)((buffer[0] << 8) | buffer[1]);
    data->accel[1] = (int16_t)((buffer[2] << 8) | buffer[3]);
    data->accel[2] = (int16_t)((buffer[4] << 8) | buffer[5]);
    data->temp     = (int16_t)((buffer[6] << 8) | buffer[7]);
    data->gyro[0]  = (int16_t)((buffer[8] << 8) | buffer[9]);
    data->gyro[1]  = (int16_t)((buffer[10] << 8) | buffer[11]);
    data->gyro[2]  = (int16_t)((buffer[12] << 8) | buffer[13]);

    return ESP_OK;
}

// Read accelerometer only
esp_err_t mpu6050_read_accel(mpu6050_handle_t *handle, int16_t accel[3]) {
    if (handle == NULL || accel == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[6];
    if (mpu6050_read_registers(handle, REG_ACCEL_XOUT_H, 6, buffer) != ESP_OK) {
        return ESP_FAIL;
    }

    accel[0] = (int16_t)((buffer[0] << 8) | buffer[1]);
    accel[1] = (int16_t)((buffer[2] << 8) | buffer[3]);
    accel[2] = (int16_t)((buffer[4] << 8) | buffer[5]);

    return ESP_OK;
}

// Read gyroscope only
esp_err_t mpu6050_read_gyro(mpu6050_handle_t *handle, int16_t gyro[3]) {
    if (handle == NULL || gyro == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[6];
    if (mpu6050_read_registers(handle, REG_GYRO_XOUT_H, 6, buffer) != ESP_OK) {
        return ESP_FAIL;
    }

    gyro[0] = (int16_t)((buffer[0] << 8) | buffer[1]);
    gyro[1] = (int16_t)((buffer[2] << 8) | buffer[3]);
    gyro[2] = (int16_t)((buffer[4] << 8) | buffer[5]);

    return ESP_OK;
}

// Read temperature only
esp_err_t mpu6050_read_temp(mpu6050_handle_t *handle, int16_t *temp) {
    if (handle == NULL || temp == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[2];
    if (mpu6050_read_registers(handle, REG_TEMP_OUT_H, 2, buffer) != ESP_OK) {
        return ESP_FAIL;
    }

    *temp = (int16_t)((buffer[0] << 8) | buffer[1]);
    return ESP_OK;
}

// Enable FIFO
esp_err_t mpu6050_fifo_enable(mpu6050_handle_t *handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Reset FIFO
    if (mpu6050_write_register(handle, REG_USER_CTRL, 0x84) != ESP_OK) {
        return ESP_FAIL;
    }

    vTaskDelay(10 / portTICK_RATE_MS);

    // Configure FIFO for accel + gyro + temp
    if (mpu6050_write_register(handle, REG_USER_CTRL, 0x40) != ESP_OK) {
        return ESP_FAIL;
    }

    // Enable overflow interrupt
    if (mpu6050_write_register(handle, REG_INT_ENABLE, 0x15) != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Read FIFO data safely
esp_err_t mpu6050_fifo_read(mpu6050_handle_t *handle, mpu6050_data_t *samples,
                             uint16_t max_count, uint16_t *read_count) {
    if (handle == NULL || samples == NULL || read_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *read_count = 0;

    uint8_t fifo_h, fifo_l;
    if (mpu6050_read_register(handle, REG_FIFO_COUNTH, &fifo_h) != ESP_OK ||
        mpu6050_read_register(handle, REG_FIFO_COUNTL, &fifo_l) != ESP_OK) {
        return ESP_FAIL;
    }

    uint16_t fifo_count = ((uint16_t)fifo_h << 8) | (uint16_t)fifo_l;

    // Validate FIFO count
    if (fifo_count > 1024) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t num_samples = fifo_count / 14;
    if (num_samples > max_count) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t buffer[14];
    for (uint16_t i = 0; i < num_samples; i++) {
        if (mpu6050_read_registers(handle, REG_FIFO_R_W, 14, buffer) != ESP_OK) {
            return ESP_FAIL;
        }

        samples[i].accel[0] = (int16_t)((buffer[0] << 8) | buffer[1]);
        samples[i].accel[1] = (int16_t)((buffer[2] << 8) | buffer[3]);
        samples[i].accel[2] = (int16_t)((buffer[4] << 8) | buffer[5]);
        samples[i].temp     = (int16_t)((buffer[6] << 8) | buffer[7]);
        samples[i].gyro[0]  = (int16_t)((buffer[8] << 8) | buffer[9]);
        samples[i].gyro[1]  = (int16_t)((buffer[10] << 8) | buffer[11]);
        samples[i].gyro[2]  = (int16_t)((buffer[12] << 8) | buffer[13]);
    }

    *read_count = num_samples;
    return ESP_OK;
}
```

---

## 12. RECOMMENDED CONFIGURATION PROFILES

### Profile 1: Low Power (1 Hz, Minimal Processing)
```c
mpu6050_config_t low_power = {
    .dlpf_cfg = 6,            // 5Hz BW, 18.6ms delay
    .sample_rate_div = 199,   // 1000/(1+199) = 5Hz actual
    .gyro_range = 0,          // ±250°/s
    .accel_range = 0,         // ±2g
};
```

### Profile 2: Balanced (100 Hz, Good Filtering)
```c
mpu6050_config_t balanced = {
    .dlpf_cfg = 1,            // 184Hz BW, 2ms delay
    .sample_rate_div = 9,     // 1000/(1+9) = 100Hz
    .gyro_range = 1,          // ±500°/s
    .accel_range = 1,         // ±4g
};
```

### Profile 3: High Performance (500 Hz, Minimal Filtering)
```c
mpu6050_config_t high_perf = {
    .dlpf_cfg = 0,            // 260Hz BW, 0ms delay
    .sample_rate_div = 15,    // 8000/(1+15) = 500Hz
    .gyro_range = 3,          // ±2000°/s
    .accel_range = 3,         // ±16g
};
```

---

## 13. REFERENCES

- **Official Datasheet**: MPU-6000 and MPU-6050 Register Map and Descriptions, Revision 4.0, InvenSense/TDK
- **I2C Specification**: I2C Bus Specification and User Manual, NXP Semiconductors
- **ESP32-IDF Documentation**: https://docs.espressif.com/projects/esp-idf/
- **Reference Implementations**:
  - esp-idf-lib: https://github.com/espressif/esp-idf-lib
  - i2cdevlib: https://github.com/jrowberg/i2cdevlib
  - Adafruit MPU6050: https://github.com/adafruit/Adafruit_MPU6050

---

**Document Version**: 1.0
**Last Updated**: 2025-11-20
**Target Platform**: ESP32 with ESP-IDF Framework
