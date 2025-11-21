# PCA9685 16-Channel PWM Controller: Comprehensive Technical Reference for ESP32-IDF

**Document Date:** November 2025
**Target Platform:** ESP32 with ESP-IDF
**Language:** C
**Device Manufacturer:** NXP Semiconductors
**Official Datasheet:** https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf

## Executive Summary

The PCA9685 is a 16-channel, 12-bit PWM Fm+ I2C-bus LED controller that operates from 2.3V to 5.5V with programmable PWM frequencies from 24 Hz to 1526 Hz. This document provides comprehensive technical specifications for C driver implementation on ESP32-IDF, including complete register maps, security considerations, and memory safety analysis.

---

## 1. DEVICE SPECIFICATIONS

### 1.1 Core Features
- **Channels:** 16 independent PWM outputs
- **Resolution:** 12-bit (4096 steps)
- **Frequency Range:** 24 Hz to 1526 Hz
- **Default I2C Address:** 0x40 (configurable via A0-A5 pins)
- **Maximum Addressable Devices:** 62 on single I2C bus
- **Internal Oscillator:** 25 MHz (typical)
- **I2C Speed:** Supports Fm+ (1 Mbps)
- **Supply Voltage:** 2.3V to 5.5V
- **Output Current:** 25 mA sink (open-drain) / 10 mA source, 25 mA sink (totem pole)

### 1.2 I2C Bus Characteristics
- **Bus Voltage:** 2.3V to 5.5V
- **I2C Address Bits:** 7-bit addressing
- **Slave Address Format:**
  - Default: `0100 000X` (0x40 base address)
  - Configurable: `0100 A5-A0 + RW bit`
- **Addressing Range:** 0x40 to 0x7E (62 unique addresses)
- **Address Pin Connections:** A0-A5 must be tied to VCC or GND (no internal pullups)

---

## 2. COMPLETE REGISTER MAP

### 2.1 Register Address Layout (Hexadecimal)

| Address | Register Name | Purpose | Size | Notes |
|---------|---------------|---------|------|-------|
| 0x00 | MODE1 | Device mode and control | 1 byte | Contains RESTART, EXTCLK, AI, SLEEP, SUB3, SUB2, SUB1, ALLCALL |
| 0x01 | MODE2 | Output configuration | 1 byte | Contains INVRT, OUTDRV, OUTNE[1:0] |
| 0x02 | SUBADR1 | I2C subaddress 1 | 1 byte | Default: 0xE2, disabled at power-up |
| 0x03 | SUBADR2 | I2C subaddress 2 | 1 byte | Default: 0xE4, disabled at power-up |
| 0x04 | SUBADR3 | I2C subaddress 3 | 1 byte | Default: 0xE8, disabled at power-up |
| 0x05 | ALLCALLADR | All Call address | 1 byte | Default: 0xE0, enabled at power-up |
| 0x06-0x45 | LED0-LED15 ON/OFF | PWM control registers | 64 bytes | 4 bytes per channel (ON_L, ON_H, OFF_L, OFF_H) |
| 0xFA | ALL_LED_ON_L | Broadcast ON low byte | 1 byte | Affects all 16 channels simultaneously |
| 0xFB | ALL_LED_ON_H | Broadcast ON high byte | 1 byte | Affects all 16 channels simultaneously |
| 0xFC | ALL_LED_OFF_L | Broadcast OFF low byte | 1 byte | Affects all 16 channels simultaneously |
| 0xFD | ALL_LED_OFF_H | Broadcast OFF high byte | 1 byte | Affects all 16 channels simultaneously |
| 0xFE | PRE_SCALE | PWM frequency prescaler | 1 byte | Range: 3-255; only writable when SLEEP=1 |
| 0xFF | TestMode | Test mode register | 1 byte | Reserved for manufacturer testing |

### 2.2 LED Channel Register Mapping

Each of 16 channels (LED0-LED15) uses 4 consecutive registers:

```
Channel 0 (LED0):   0x06 (ON_L), 0x07 (ON_H), 0x08 (OFF_L), 0x09 (OFF_H)
Channel 1 (LED1):   0x0A (ON_L), 0x0B (ON_H), 0x0C (OFF_L), 0x0D (OFF_H)
Channel 2 (LED2):   0x0E (ON_L), 0x0F (ON_H), 0x10 (OFF_L), 0x11 (OFF_H)
Channel 3 (LED3):   0x12 (ON_L), 0x13 (ON_H), 0x14 (OFF_L), 0x15 (OFF_H)
Channel 4 (LED4):   0x16 (ON_L), 0x17 (ON_H), 0x18 (OFF_L), 0x19 (OFF_H)
Channel 5 (LED5):   0x1A (ON_L), 0x1B (ON_H), 0x1C (OFF_L), 0x1D (OFF_H)
Channel 6 (LED6):   0x1E (ON_L), 0x1F (ON_H), 0x20 (OFF_L), 0x21 (OFF_H)
Channel 7 (LED7):   0x22 (ON_L), 0x23 (ON_H), 0x24 (OFF_L), 0x25 (OFF_H)
Channel 8 (LED8):   0x26 (ON_L), 0x27 (ON_H), 0x28 (OFF_L), 0x29 (OFF_H)
Channel 9 (LED9):   0x2A (ON_L), 0x2B (ON_H), 0x2C (OFF_L), 0x2D (OFF_H)
Channel 10 (LED10): 0x2E (ON_L), 0x2F (ON_H), 0x30 (OFF_L), 0x31 (OFF_H)
Channel 11 (LED11): 0x32 (ON_L), 0x33 (ON_H), 0x34 (OFF_L), 0x35 (OFF_H)
Channel 12 (LED12): 0x36 (ON_L), 0x37 (ON_H), 0x38 (OFF_L), 0x39 (OFF_H)
Channel 13 (LED13): 0x3A (ON_L), 0x3B (ON_H), 0x3C (OFF_L), 0x3D (OFF_H)
Channel 14 (LED14): 0x3E (ON_L), 0x3F (ON_H), 0x40 (OFF_L), 0x41 (OFF_H)
Channel 15 (LED15): 0x42 (ON_L), 0x43 (ON_H), 0x44 (OFF_L), 0x45 (OFF_H)
```

**Calculation:** `base_address = 0x06 + (channel_number * 4)`

---

## 3. MODE1 REGISTER (Address 0x00) - DETAILED BIT MAPPING

### 3.1 Bit Definitions

| Bit | Name | Type | Reset | Description |
|-----|------|------|-------|-------------|
| 7 | RESTART | R/W | 0 | Oscillator restart. 0=Restart disabled. 1=Restart enabled |
| 6 | EXTCLK | R/W | 0 | External clock input. 0=Internal 25MHz. 1=External clock on EXTCLK pin |
| 5 | AI | R/W | 0 | Auto-increment. 0=Disabled. 1=Enabled (increments register address on sequential reads/writes) |
| 4 | SLEEP | R/W | 1 | Low power mode. 0=Normal operation. 1=Sleep mode (disables oscillator) |
| 3 | SUB3 | R/W | 0 | Subaddress 3 acknowledgment. 1=Responds to SUBADR3 address |
| 2 | SUB2 | R/W | 0 | Subaddress 2 acknowledgment. 1=Responds to SUBADR2 address |
| 1 | SUB1 | R/W | 0 | Subaddress 1 acknowledgment. 1=Responds to SUBADR1 address |
| 0 | ALLCALL | R/W | 1 | All Call address acknowledgment. 1=Responds to ALLCALLADR (0xE0) |

### 3.2 MODE1 Initialization Sequence

**Critical:** Must follow this exact sequence to ensure proper operation:

```c
// Step 1: Write MODE1 with SLEEP=1 (prevents immediate oscillator operation)
uint8_t mode1 = (1 << 4);  // Set SLEEP bit
i2c_write_register(PCA9685_ADDR, 0x00, mode1);
vTaskDelay(pdMS_TO_TICKS(1));  // Wait minimum 500us

// Step 2: Clear SLEEP bit to enable oscillator
mode1 &= ~(1 << 4);  // Clear SLEEP bit
i2c_write_register(PCA9685_ADDR, 0x00, mode1);
vTaskDelay(pdMS_TO_TICKS(1));

// Step 3: Set RESTART bit
mode1 |= (1 << 7);  // Set RESTART bit
i2c_write_register(PCA9685_ADDR, 0x00, mode1);
vTaskDelay(pdMS_TO_TICKS(1));

// Step 4: Enable auto-increment for multi-byte operations
mode1 |= (1 << 5);  // Set AI bit
i2c_write_register(PCA9685_ADDR, 0x00, mode1);
```

### 3.3 Critical Timing Notes

- **Tbuf (I2C bus free time):** Minimum 4.7 microseconds
- **Oscillator startup time:** Add 500 microseconds delay after clearing SLEEP
- **RESTART bit behavior:** Self-clearing; writing 1 resets internal counters
- **PRE_SCALE modification:** Can ONLY be changed when SLEEP=1

---

## 4. MODE2 REGISTER (Address 0x01) - DETAILED BIT MAPPING

### 4.1 Bit Definitions

| Bit | Name | Type | Reset | Description |
|-----|------|------|-------|-------------|
| 7-5 | Reserved | R | 000 | Unused, always read as 0 |
| 4 | INVRT | R/W | 0 | Output inversion. 0=Normal. 1=Inverted (GPIO high = PWM off) |
| 3 | Reserved | R | 0 | Unused |
| 2 | OUTDRV | R/W | 0 | Output driver configuration. 0=Open-drain. 1=Totem-pole |
| 1-0 | OUTNE[1:0] | R/W | 00 | Output enable configuration (see 4.2) |

### 4.2 OUTNE[1:0] Configuration (Output Enable)

| OUTNE[1:0] | OE Pin | Behavior |
|-----------|--------|----------|
| 00 | 1 (disabled) | LEDn = 0 when OE=1 (outputs pulled low) |
| 01 | 1 (disabled) | LEDn follows individual PWM or ALL_LED settings |
| 10 | 1 (disabled) | LEDn = 1 (outputs pulled high) - typically invalid |
| 11 | 1 (disabled) | Reserved for future use |

### 4.3 Output Mode Selection

**For Open-Drain Configuration (most common for LED control):**
```c
uint8_t mode2 = 0x00;  // OUTDRV = 0 (open-drain)
mode2 |= 0x01;         // OUTNE = 01 (PWM-controlled)
i2c_write_register(PCA9685_ADDR, 0x01, mode2);
```

**For Totem-Pole Configuration (direct MOSFET drive):**
```c
uint8_t mode2 = 0x04;  // OUTDRV = 1 (totem-pole)
mode2 |= 0x01;         // OUTNE = 01 (PWM-controlled)
i2c_write_register(PCA9685_ADDR, 0x01, mode2);
```

**For Inverted Output:**
```c
uint8_t mode2 = 0x14;  // INVRT = 1, OUTDRV = 1
mode2 |= 0x01;         // OUTNE = 01
i2c_write_register(PCA9685_ADDR, 0x01, mode2);
```

---

## 5. PWM LED OUTPUT REGISTERS - DETAILED OPERATION

### 5.1 Register Structure for Each Channel

Each PWM channel consists of 4 registers forming two 12-bit values:

**ON Time Registers (2 registers per channel):**
- `LEDn_ON_L` (lower 8 bits of ON counter)
- `LEDn_ON_H` (upper 4 bits of ON counter, bits 7-4 unused)

**OFF Time Registers (2 registers per channel):**
- `LEDn_OFF_L` (lower 8 bits of OFF counter)
- `LEDn_OFF_H` (upper 4 bits of OFF counter, bits 7-4 unused, bit 4 = Full ON control)

### 5.2 12-Bit Counter Mechanism

The PCA9685 internally runs a 12-bit counter continuously cycling from 0x000 to 0xFFF (0-4095):

```
Counter Cycle: 0x000 -> 0x001 -> 0x002 -> ... -> 0xFFF -> 0x000 (repeat)
               (4096 total steps per cycle)
```

**PWM Logic:**
- When counter < ON value: LED output = low
- When counter >= ON value AND counter < OFF value: LED output = high
- When counter >= OFF value: LED output = low
- Duty cycle = (OFF - ON) / 4096 * 100%

### 5.3 Full ON/OFF Control (Special Bits)

**LEDn_ON_H Bit 4 (Full ON):**
- Set to 1: Channel always on (ignores OFF time)
- Value: 0x1000 (4096 in decimal) in the 12-bit value

**LEDn_OFF_H Bit 4 (Full OFF):**
- Set to 1: Channel always off (ignores ON time)
- Value: 0x1000 (4096 in decimal) in the 12-bit value

### 5.4 Reading/Writing 12-Bit Values

**Writing a 12-bit ON value (example: 0x305 = 773 decimal):**
```c
uint16_t on_value = 0x305;
uint8_t on_l = on_value & 0xFF;        // Lower 8 bits = 0x05
uint8_t on_h = (on_value >> 8) & 0x0F; // Upper 4 bits = 0x03

i2c_write_register(PCA9685_ADDR, 0x06, on_l);  // Write ON_L
i2c_write_register(PCA9685_ADDR, 0x07, on_h);  // Write ON_H
```

**Reading a 12-bit ON value (with auto-increment):**
```c
uint8_t on_l, on_h;
i2c_read_register(PCA9685_ADDR, 0x06, &on_l);  // Read ON_L
i2c_read_register(PCA9685_ADDR, 0x07, &on_h);  // Read ON_H
uint16_t on_value = ((on_h & 0x0F) << 8) | on_l;
```

### 5.5 Invalid Register Combinations (Must Avoid)

**CRITICAL HARDWARE LIMITATION:**
> "LEDn_ON and LEDn_OFF count registers should NEVER be programmed with identical values."

This causes undefined PWM behavior. Specific invalid combinations:
- `ON = OFF` (any value)
- `ON = 0x000, OFF = 0x000`
- `ON = 0x001, OFF = 0x001`
- And so forth for all 4096 values

**Prevention in Driver:**
```c
esp_err_t pca9685_set_pwm(uint8_t channel, uint16_t on_value, uint16_t off_value) {
    // Safety check 1: Validate ON and OFF are different
    if (on_value == off_value) {
        return ESP_ERR_INVALID_ARG;  // MUST reject identical values
    }

    // Safety check 2: Validate channel number
    if (channel > 15 && channel != 16) {  // 16 = ALL_LED
        return ESP_ERR_INVALID_ARG;
    }

    // Safety check 3: Validate 12-bit range
    if (on_value > 0x0FFF || off_value > 0x0FFF) {
        return ESP_ERR_INVALID_ARG;
    }

    // ... proceed with write ...
}
```

---

## 6. PRE_SCALE REGISTER (Address 0xFE) - FREQUENCY CONTROL

### 6.1 Register Definition

| Aspect | Detail |
|--------|--------|
| Address | 0xFE |
| Width | 8 bits |
| Valid Range | 3-255 |
| Default Value | 0x1E (30 decimal = 200 Hz) |
| Writable | Only when MODE1 SLEEP bit = 1 |
| Reset | After RESTART bit set |

### 6.2 Prescaler to Frequency Mapping

**Formula:**
```
frequency_hz = 25,000,000 / (4096 * (prescale + 1))
prescale = round(25,000,000 / (4096 * frequency_hz)) - 1
```

**Boundary Conditions:**
```
Prescale = 3   → frequency = 25,000,000 / (4096 * 4) = 1,525.88 Hz
Prescale = 255 → frequency = 25,000,000 / (4096 * 256) = 23.84 Hz
```

### 6.3 Common Frequency Settings

| Desired Frequency | Prescale Value | Actual Frequency | Error |
|------------------|----------------|------------------|-------|
| 50 Hz (servo) | 0x79 (121) | 50.08 Hz | +0.16% |
| 100 Hz | 0x3C (60) | 100.16 Hz | +0.16% |
| 200 Hz (default) | 0x1E (30) | 200.32 Hz | +0.16% |
| 1000 Hz | 0x06 (6) | 997.67 Hz | -0.23% |
| 1526 Hz (max) | 0x03 (3) | 1525.88 Hz | -0.01% |

### 6.4 Prescaler Calculation with Overflow Protection

**CRITICAL: Integer Overflow Risk**

Raw calculation can exceed uint8_t range at low frequencies:
```
25,000,000 / (4096 * 24) = 254.4 (fits in uint8_t)
25,000,000 / (4096 * 20) = 305.2 (OVERFLOWS uint8_t!)
```

**Safe Implementation:**
```c
esp_err_t pca9685_set_frequency(uint16_t frequency_hz) {
    // Input validation
    if (frequency_hz < 24 || frequency_hz > 1526) {
        return ESP_ERR_INVALID_ARG;
    }

    // Use floating-point to avoid overflow
    float prescale_float = (25000000.0f / (4096.0f * frequency_hz)) - 1.0f;

    // Clamp to valid range [3, 255]
    if (prescale_float < 3.0f) {
        prescale_float = 3.0f;  // Cap at max frequency
    }
    if (prescale_float > 255.0f) {
        prescale_float = 255.0f;  // Cap at min frequency
    }

    // Round to nearest integer
    uint8_t prescale = (uint8_t)(prescale_float + 0.5f);

    // MUST set SLEEP bit before modifying PRE_SCALE
    uint8_t mode1;
    i2c_read_register(PCA9685_ADDR, 0x00, &mode1);
    mode1 |= (1 << 4);  // Set SLEEP
    i2c_write_register(PCA9685_ADDR, 0x00, mode1);
    vTaskDelay(pdMS_TO_TICKS(1));

    // Write prescaler
    i2c_write_register(PCA9685_ADDR, 0xFE, prescale);
    vTaskDelay(pdMS_TO_TICKS(1));

    // Clear SLEEP bit to resume operation
    mode1 &= ~(1 << 4);  // Clear SLEEP
    i2c_write_register(PCA9685_ADDR, 0x00, mode1);
    vTaskDelay(pdMS_TO_TICKS(1));

    return ESP_OK;
}
```

---

## 7. I2C PROTOCOL DETAILS

### 7.1 I2C Address Configuration

**Default 7-Bit Slave Address:** `0100 000` (0x40)

**Configurable with Hardware Pins A0-A5:**
```
Address = 0x40 + (A5*32 + A4*16 + A3*8 + A2*4 + A1*2 + A0*1)
```

**Examples:**
```
A5=0, A4=0, A3=0, A2=0, A1=0, A0=0 → 0x40
A5=0, A4=0, A3=0, A2=0, A1=0, A0=1 → 0x41
A5=0, A4=0, A3=0, A2=0, A1=1, A0=0 → 0x42
A5=1, A1=1, A0=1 (all others=0) → 0x76
```

### 7.2 Special I2C Addresses

| Address | Name | Purpose | Enabled |
|---------|------|---------|---------|
| 0xE0 (1110 0000) | ALL_CALL | Broadcast to ALL PCA9685 on bus | At power-up (changeable) |
| 0xE2 (1110 0010) | SUBADR1 | Subaddress 1 (if enabled) | Disabled at power-up |
| 0xE4 (1110 0100) | SUBADR2 | Subaddress 2 (if enabled) | Disabled at power-up |
| 0xE8 (1110 1000) | SUBADR3 | Subaddress 3 (if enabled) | Disabled at power-up |

### 7.3 Single-Byte Read/Write Sequence

**Master to Slave (Write Register):**
```
START → SLAVE_ADDR (0x40) + W → ACK → REGISTER_ADDR → ACK → DATA_BYTE → ACK → STOP
```

**Master to Slave (Read Register):**
```
START → SLAVE_ADDR (0x40) + W → ACK → REGISTER_ADDR → ACK → RESTART
START → SLAVE_ADDR (0x40) + R → ACK → DATA_BYTE → NACK → STOP
```

### 7.4 Multi-Byte Read/Write (Auto-Increment)

Requires MODE1[5] (AI bit) = 1:

**Master to Slave (Write 4 consecutive registers):**
```
START → SLAVE_ADDR + W → ACK → START_REG_ADDR → ACK
  → DATA_BYTE_1 → ACK → DATA_BYTE_2 → ACK → DATA_BYTE_3 → ACK → DATA_BYTE_4 → ACK → STOP
```

**Register address auto-increments after each byte (register auto-address advance)**

### 7.5 ESP32-IDF I2C Implementation Example

```c
#include "driver/i2c.h"
#include "esp_err.h"

#define I2C_MASTER_SCL_IO    22
#define I2C_MASTER_SDA_IO    21
#define I2C_MASTER_FREQ_HZ   100000  // 100 kHz standard mode (PCA9685 supports up to 1 Mbps)
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0

esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_NUM_0, &conf);
    if (err != ESP_OK) {
        return err;
    }

    return i2c_driver_install(I2C_NUM_0, conf.mode,
                             I2C_MASTER_RX_BUF_DISABLE,
                             I2C_MASTER_TX_BUF_DISABLE, 0);
}

esp_err_t i2c_write_register(uint8_t addr, uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t i2c_read_register(uint8_t addr, uint8_t reg, uint8_t *value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        return ret;
    }

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}

esp_err_t i2c_write_registers_burst(uint8_t addr, uint8_t start_reg, uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, start_reg, true);

    for (size_t i = 0; i < len; i++) {
        i2c_master_write_byte(cmd, data[i], true);
    }

    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}
```

---

## 8. PWM FREQUENCY AND DUTY CYCLE CALCULATIONS

### 8.1 Frequency Calculation

**System Clock:** 25 MHz internal oscillator
**Counter Resolution:** 12-bit (0-4095 = 4096 steps)
**Counter Frequency:** 25,000,000 / (prescale + 1)
**PWM Frequency:** (25,000,000 / (prescale + 1)) / 4096

### 8.2 Duty Cycle Calculation

Given ON and OFF counter values:

```
Period = 4096 counts
Pulse Width = OFF - ON
Duty Cycle (%) = ((OFF - ON) / 4096) * 100
```

**Example: 50% Duty Cycle**
```
ON = 0, OFF = 2048
Duty = (2048 - 0) / 4096 * 100 = 50%
```

**Example: 25% Duty Cycle with Phase Shift**
```
ON = 512, OFF = 1536
Duty = (1536 - 512) / 4096 * 100 = 25%
Phase Shift = (512 / 4096) * 100 = 12.5%
```

### 8.3 Microsecond-Based PWM Configuration (Servo Control)

For servo control, often specified in microseconds (e.g., 1.5ms center position):

```c
/**
 * Convert pulse width in microseconds to ON/OFF register values
 *
 * @param frequency_hz    PWM frequency in Hz
 * @param pulse_us        Pulse width in microseconds
 * @param on_value        Pointer to store ON register value (0-4095)
 * @param off_value       Pointer to store OFF register value (0-4095)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid input
 */
esp_err_t pca9685_pulse_to_registers(uint16_t frequency_hz, uint16_t pulse_us,
                                     uint16_t *on_value, uint16_t *off_value) {
    if (frequency_hz < 24 || frequency_hz > 1526) {
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate period in microseconds
    uint32_t period_us = 1000000 / frequency_hz;

    // Check pulse width validity
    if (pulse_us > period_us) {
        return ESP_ERR_INVALID_ARG;
    }

    // Convert pulse width to counter steps (0-4095)
    uint16_t pulse_steps = (pulse_us * 4096) / period_us;

    // Clamp to valid range
    if (pulse_steps > 4095) {
        pulse_steps = 4095;
    }

    // Set ON time to 0, OFF time to pulse width
    *on_value = 0;
    *off_value = pulse_steps;

    // Validate that ON != OFF
    if (*on_value == *off_value) {
        // Adjust slightly to avoid hardware limitation
        if (*off_value < 4095) {
            (*off_value)++;
        } else {
            return ESP_ERR_INVALID_ARG;
        }
    }

    return ESP_OK;
}
```

**Common Servo Pulse Widths (50 Hz / 20ms period):**
```
1000 µs (0°)     → 204 steps
1500 µs (90°)    → 307 steps
2000 µs (180°)   → 409 steps
```

---

## 9. SECURITY CONSIDERATIONS

### 9.1 I2C Address Conflicts

**Risk:** Multiple devices at same address cause bus contention

**Mitigation:**
```c
esp_err_t pca9685_verify_address(uint8_t addr) {
    uint8_t mode1;
    esp_err_t ret = i2c_read_register(addr, 0x00, &mode1);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Address 0x%02X not responding", addr);
        return ESP_ERR_NOT_FOUND;
    }

    // Verify by reading a known register pattern
    uint8_t pre_scale;
    ret = i2c_read_register(addr, 0xFE, &pre_scale);
    if (ret != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Device found at 0x%02X, MODE1=0x%02X, PRE_SCALE=0x%02X",
             addr, mode1, pre_scale);

    return ESP_OK;
}
```

### 9.2 I2C Bus Conflicts

**Risk:** Multiple masters on I2C bus can cause data corruption

**Mitigation:**
- Single master architecture (ESP32 is master, PCA9685 is slave)
- Use I2C bus arbitration if multiple masters required
- Add CRC checking for critical commands

```c
#define CRC_POLYNOMIAL 0xA6

uint8_t calculate_crc8(uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ CRC_POLYNOMIAL;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

### 9.3 Invalid Register Writes

**Risk:** Writing to invalid registers or invalid values causes undefined behavior

**Mitigation Strategy 1: Input Validation**
```c
esp_err_t pca9685_set_channel_pwm(uint8_t channel, uint16_t on, uint16_t off) {
    // Validate channel
    if (channel > 15) {
        ESP_LOGE(TAG, "Invalid channel: %d (valid: 0-15)", channel);
        return ESP_ERR_INVALID_ARG;
    }

    // Validate PWM values
    if (on > 0x0FFF) {
        ESP_LOGE(TAG, "ON value out of range: 0x%04X (max: 0x0FFF)", on);
        return ESP_ERR_INVALID_ARG;
    }

    if (off > 0x0FFF) {
        ESP_LOGE(TAG, "OFF value out of range: 0x%04X (max: 0x0FFF)", off);
        return ESP_ERR_INVALID_ARG;
    }

    // CRITICAL: Prevent identical ON/OFF (hardware limitation)
    if (on == off) {
        ESP_LOGE(TAG, "ON and OFF cannot be identical: 0x%04X == 0x%04X", on, off);
        return ESP_ERR_INVALID_ARG;
    }

    // Write is safe, proceed...
}
```

**Mitigation Strategy 2: Read-Modify-Write Pattern**
```c
esp_err_t pca9685_set_mode1_bit(uint8_t bit_pos, uint8_t value) {
    uint8_t mode1;

    // Read current value
    esp_err_t ret = i2c_read_register(PCA9685_ADDR, 0x00, &mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    // Modify only target bit
    if (value) {
        mode1 |= (1 << bit_pos);
    } else {
        mode1 &= ~(1 << bit_pos);
    }

    // Write back
    return i2c_write_register(PCA9685_ADDR, 0x00, mode1);
}
```

### 9.4 Frequency Overflow

**Risk:** Prescale value calculation overflows uint8_t at low frequencies

**Mitigation:**
```c
esp_err_t pca9685_set_frequency_safe(uint16_t frequency_hz) {
    // Input validation
    if (frequency_hz < 24) {
        ESP_LOGW(TAG, "Frequency too low (%d Hz), clamping to 24 Hz", frequency_hz);
        frequency_hz = 24;
    }
    if (frequency_hz > 1526) {
        ESP_LOGW(TAG, "Frequency too high (%d Hz), clamping to 1526 Hz", frequency_hz);
        frequency_hz = 1526;
    }

    // Use float to avoid overflow
    float prescale_f = (25000000.0f / (4096.0f * (float)frequency_hz)) - 1.0f;

    // Explicit bounds clamping
    if (prescale_f < 3.0f) {
        prescale_f = 3.0f;
        ESP_LOGW(TAG, "Prescale clamped to minimum 3 (max frequency)");
    }
    if (prescale_f > 255.0f) {
        prescale_f = 255.0f;
        ESP_LOGW(TAG, "Prescale clamped to maximum 255 (min frequency)");
    }

    // Round to nearest integer with explicit cast
    uint8_t prescale = (uint8_t)(prescale_f + 0.5f);

    ESP_LOGI(TAG, "Setting frequency: %d Hz → prescale: %d (actual: %.2f Hz)",
             frequency_hz, prescale,
             25000000.0f / (4096.0f * (float)(prescale + 1)));

    // ... proceed with prescale write ...
}
```

### 9.5 Physical I2C Bus Attacks

**Risk:** Unencrypted I2C bus allows snooping of register reads/writes

**Mitigation:**
- Add address validation
- Monitor for excessive I2C errors indicating active tampering
- Use watchdog timer to detect bus lockup

```c
#define MAX_I2C_ERRORS_PER_SECOND 5

typedef struct {
    uint32_t error_count;
    uint32_t last_reset_tick;
} pca9685_error_tracking_t;

pca9685_error_tracking_t error_tracker = {0};

esp_err_t pca9685_check_error_rate(void) {
    uint32_t current_tick = xTaskGetTickCount();
    uint32_t elapsed = current_tick - error_tracker.last_reset_tick;

    if (elapsed > pdMS_TO_TICKS(1000)) {
        // Reset counter every second
        if (error_tracker.error_count > MAX_I2C_ERRORS_PER_SECOND) {
            ESP_LOGE(TAG, "Excessive I2C errors detected: %d/sec",
                     error_tracker.error_count);
            error_tracker.error_count = 0;
            return ESP_ERR_TIMEOUT;
        }
        error_tracker.error_count = 0;
        error_tracker.last_reset_tick = current_tick;
    }

    return ESP_OK;
}

void pca9685_record_error(void) {
    error_tracker.error_count++;
    pca9685_check_error_rate();
}
```

---

## 10. MEMORY SAFETY ISSUES & MITIGATIONS

### 10.1 Buffer Overflow Risks

**Risk 1: Register Burst Read with Incorrect Size**
```c
// UNSAFE: No bounds checking
void pca9685_read_led_regs_unsafe(uint8_t channel, uint8_t *buffer) {
    // What if buffer is not 4 bytes? → Buffer overflow!
    i2c_read_registers_burst(PCA9685_ADDR, 0x06 + channel*4, buffer, 4);
}

// SAFE: Explicit size verification
esp_err_t pca9685_read_led_regs_safe(uint8_t channel, uint8_t *buffer, size_t buffer_size) {
    // Validate buffer size
    if (buffer_size < 4) {
        ESP_LOGE(TAG, "Buffer too small: %d (need: 4)", buffer_size);
        return ESP_ERR_INVALID_ARG;
    }

    // Validate channel
    if (channel > 15) {
        ESP_LOGE(TAG, "Invalid channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t start_reg = 0x06 + (channel * 4);
    return i2c_read_registers_burst(PCA9685_ADDR, start_reg, buffer, 4);
}
```

**Risk 2: Array Out-of-Bounds Write**
```c
// UNSAFE: No bounds checking on array
void pca9685_set_all_pwm_unsafe(uint16_t *pwm_values) {
    for (int i = 0; i < 20; i++) {  // BUG: PCA9685 has only 16 channels!
        uint16_t on = pwm_values[i];
        uint16_t off = pwm_values[i + 1];
        // Writing to non-existent channels!
    }
}

// SAFE: Explicit bounds checking
esp_err_t pca9685_set_all_pwm_safe(const uint16_t *pwm_values, size_t count) {
    if (pwm_values == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (count > 16) {
        ESP_LOGE(TAG, "Too many PWM values: %d (max: 16)", count);
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < count; i++) {
        if (pwm_values[i] > 0x0FFF) {
            ESP_LOGE(TAG, "PWM value out of range at index %d: 0x%04X",
                     i, pwm_values[i]);
            return ESP_ERR_INVALID_ARG;
        }
    }

    // Now safe to proceed...
}
```

### 10.2 Integer Overflow Issues

**Risk 1: Frequency Calculation Overflow**
```c
// UNSAFE: Integer overflow in prescaler calculation
uint8_t pca9685_freq_to_prescale_unsafe(uint16_t freq_hz) {
    // BUG: 25000000 / (4096 * freq) can overflow uint32!
    uint32_t temp = 25000000 / (4096 * freq_hz);  // Overflow if freq < ~150
    return (uint8_t)(temp - 1);
}

// SAFE: Use floating-point
uint8_t pca9685_freq_to_prescale_safe(uint16_t freq_hz) {
    // Use float to prevent overflow
    float prescale_f = (25000000.0f / (4096.0f * (float)freq_hz)) - 1.0f;

    // Explicit bounds clamping
    if (prescale_f < 3.0f) return 3;
    if (prescale_f > 255.0f) return 255;

    return (uint8_t)(prescale_f + 0.5f);
}
```

**Risk 2: Register Address Calculation Overflow**
```c
// UNSAFE: Channel * 4 can cause unexpected register address
uint8_t pca9685_get_register_unsafe(uint8_t channel) {
    // BUG: If channel > 63, will wrap around!
    return 0x06 + (channel * 4);  // No bounds check
}

// SAFE: Explicit bounds validation
esp_err_t pca9685_get_register_safe(uint8_t channel, uint8_t *out_reg) {
    if (channel > 15) {
        ESP_LOGE(TAG, "Invalid channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    // Now safe: max is 0x06 + (15 * 4) = 0x42
    *out_reg = 0x06 + (channel * 4);
    return ESP_OK;
}
```

**Risk 3: 16-bit ON/OFF Value Overflow**
```c
// UNSAFE: No validation of 12-bit constraint
void pca9685_set_pwm_unsafe(uint16_t on, uint16_t off) {
    // If on or off > 0x0FFF, higher bits will be truncated!
    uint8_t on_l = on & 0xFF;
    uint8_t on_h = (on >> 8);  // Should mask to 0x0F!
}

// SAFE: Explicit 12-bit masking
esp_err_t pca9685_set_pwm_safe(uint16_t on, uint16_t off) {
    // Validate 12-bit range
    if (on > 0x0FFF || off > 0x0FFF) {
        ESP_LOGE(TAG, "PWM values must be ≤ 0x0FFF (12-bit)");
        return ESP_ERR_INVALID_ARG;
    }

    // Extract with explicit masking
    uint8_t on_l = on & 0xFF;
    uint8_t on_h = (on >> 8) & 0x0F;  // Explicitly mask to 4 bits
    uint8_t off_l = off & 0xFF;
    uint8_t off_h = (off >> 8) & 0x0F;

    // Now safe to write...
}
```

### 10.3 NULL Pointer Dereference

**Risk: Function receives NULL pointer**
```c
// UNSAFE: No NULL check
void pca9685_init_unsafe(pca9685_device_t *dev) {
    dev->addr = 0x40;  // Crash if dev == NULL!
}

// SAFE: Validate pointer
esp_err_t pca9685_init_safe(pca9685_device_t *dev) {
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    dev->addr = 0x40;
    return ESP_OK;
}
```

### 10.4 Use-After-Free

**Risk: Accessing freed device structure**
```c
// UNSAFE: No flag to indicate freed state
typedef struct {
    uint8_t addr;
    uint8_t mode1;
} pca9685_t;

pca9685_t *g_pca9685;

void pca9685_free(pca9685_t *dev) {
    free(dev);
    // g_pca9685 still points to freed memory!
}

void pca9685_set_pwm(uint8_t channel, uint16_t on) {
    g_pca9685->addr;  // Use-after-free!
}

// SAFE: Clear pointer and use validation
#define PCA9685_FLAG_INITIALIZED (1 << 0)

typedef struct {
    uint8_t flags;
    uint8_t addr;
} pca9685_t;

void pca9685_free(pca9685_t *dev) {
    if (dev == NULL) return;
    dev->flags &= ~PCA9685_FLAG_INITIALIZED;
    free(dev);
    dev = NULL;
}

esp_err_t pca9685_set_pwm(pca9685_t *dev, uint8_t channel, uint16_t on) {
    if (dev == NULL || !(dev->flags & PCA9685_FLAG_INITIALIZED)) {
        return ESP_ERR_INVALID_STATE;
    }
    // Safe to use...
}
```

### 10.5 Double-Free

**Risk: Freeing same pointer twice**
```c
// UNSAFE: No guard against double-free
pca9685_t *dev = malloc(sizeof(pca9685_t));
// ...
free(dev);
free(dev);  // Double-free crash!

// SAFE: Set to NULL after free
pca9685_t *dev = malloc(sizeof(pca9685_t));
// ...
free(dev);
dev = NULL;
if (dev != NULL) {
    free(dev);  // Will be skipped on second call
}
```

---

## 11. RECOMMENDED C DRIVER STRUCTURE

### 11.1 Header File (pca9685.h)

```c
#ifndef PCA9685_H
#define PCA9685_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

// Device Configuration Constants
#define PCA9685_I2C_ADDR_BASE       0x40
#define PCA9685_NUM_CHANNELS        16
#define PCA9685_MAX_PWM_VALUE       0x0FFF
#define PCA9685_MIN_FREQUENCY_HZ    24
#define PCA9685_MAX_FREQUENCY_HZ    1526
#define PCA9685_OSC_FREQUENCY       25000000  // 25 MHz

// Register Addresses
#define PCA9685_REG_MODE1           0x00
#define PCA9685_REG_MODE2           0x01
#define PCA9685_REG_SUBADR1         0x02
#define PCA9685_REG_SUBADR2         0x03
#define PCA9685_REG_SUBADR3         0x04
#define PCA9685_REG_ALLCALLADR      0x05
#define PCA9685_REG_LED0_ON_L       0x06
#define PCA9685_REG_PRE_SCALE       0xFE
#define PCA9685_REG_ALLLED_ON_L     0xFA
#define PCA9685_REG_ALLLED_OFF_H    0xFD

// MODE1 Bits
#define PCA9685_MODE1_RESTART       (1 << 7)
#define PCA9685_MODE1_EXTCLK        (1 << 6)
#define PCA9685_MODE1_AI            (1 << 5)
#define PCA9685_MODE1_SLEEP         (1 << 4)
#define PCA9685_MODE1_SUB3          (1 << 3)
#define PCA9685_MODE1_SUB2          (1 << 2)
#define PCA9685_MODE1_SUB1          (1 << 1)
#define PCA9685_MODE1_ALLCALL       (1 << 0)

// MODE2 Bits
#define PCA9685_MODE2_INVRT         (1 << 4)
#define PCA9685_MODE2_OUTDRV        (1 << 2)
#define PCA9685_MODE2_OUTNE_MASK    0x03

// Device Handle
typedef struct {
    i2c_port_t i2c_port;
    uint8_t address;
    uint16_t current_frequency;
    bool initialized;
} pca9685_t;

// Public API
esp_err_t pca9685_init(pca9685_t *dev, i2c_port_t i2c_port, uint8_t address);
esp_err_t pca9685_deinit(pca9685_t *dev);
esp_err_t pca9685_set_frequency(pca9685_t *dev, uint16_t frequency_hz);
esp_err_t pca9685_set_pwm(pca9685_t *dev, uint8_t channel, uint16_t on, uint16_t off);
esp_err_t pca9685_set_pwm_pulse(pca9685_t *dev, uint8_t channel, uint16_t pulse_us);
esp_err_t pca9685_get_pwm(pca9685_t *dev, uint8_t channel, uint16_t *on, uint16_t *off);

#ifdef __cplusplus
}
#endif

#endif  // PCA9685_H
```

### 11.2 Implementation File (pca9685.c) - Core Functions

```c
#include "pca9685.h"
#include "esp_log.h"

static const char *TAG = "PCA9685";

static esp_err_t pca9685_write_register(pca9685_t *dev, uint8_t reg, uint8_t value) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write register 0x%02X: %s", reg, esp_err_to_name(ret));
    }

    return ret;
}

static esp_err_t pca9685_read_register(pca9685_t *dev, uint8_t reg, uint8_t *value) {
    if (dev == NULL || !dev->initialized || value == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to address register 0x%02X: %s", reg, esp_err_to_name(ret));
        return ret;
    }

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02X: %s", reg, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t pca9685_init(pca9685_t *dev, i2c_port_t i2c_port, uint8_t address) {
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (address < PCA9685_I2C_ADDR_BASE || address > 0x7E) {
        ESP_LOGE(TAG, "Invalid I2C address: 0x%02X (range: 0x40-0x7E)", address);
        return ESP_ERR_INVALID_ARG;
    }

    dev->i2c_port = i2c_port;
    dev->address = address;
    dev->current_frequency = 200;  // Default
    dev->initialized = true;

    // Verify device presence
    uint8_t mode1;
    esp_err_t ret = pca9685_read_register(dev, PCA9685_REG_MODE1, &mode1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device not responding at address 0x%02X", address);
        dev->initialized = false;
        return ret;
    }

    // Initialize MODE1: Set SLEEP, enable AI, clear other bits
    uint8_t init_mode1 = PCA9685_MODE1_SLEEP | PCA9685_MODE1_AI;
    ret = pca9685_write_register(dev, PCA9685_REG_MODE1, init_mode1);
    if (ret != ESP_OK) {
        dev->initialized = false;
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    // Initialize MODE2: Open-drain, PWM mode
    uint8_t init_mode2 = PCA9685_MODE2_OUTNE_MASK & 0x01;
    ret = pca9685_write_register(dev, PCA9685_REG_MODE2, init_mode2);
    if (ret != ESP_OK) {
        dev->initialized = false;
        return ret;
    }

    // Set default frequency (200 Hz)
    ret = pca9685_set_frequency(dev, 200);
    if (ret != ESP_OK) {
        dev->initialized = false;
        return ret;
    }

    ESP_LOGI(TAG, "Initialized PCA9685 at address 0x%02X", address);
    return ESP_OK;
}

esp_err_t pca9685_set_frequency(pca9685_t *dev, uint16_t frequency_hz) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Validate frequency
    if (frequency_hz < PCA9685_MIN_FREQUENCY_HZ) {
        ESP_LOGW(TAG, "Frequency too low: %d Hz, clamping to %d Hz",
                 frequency_hz, PCA9685_MIN_FREQUENCY_HZ);
        frequency_hz = PCA9685_MIN_FREQUENCY_HZ;
    }
    if (frequency_hz > PCA9685_MAX_FREQUENCY_HZ) {
        ESP_LOGW(TAG, "Frequency too high: %d Hz, clamping to %d Hz",
                 frequency_hz, PCA9685_MAX_FREQUENCY_HZ);
        frequency_hz = PCA9685_MAX_FREQUENCY_HZ;
    }

    // Calculate prescaler using floating-point to avoid overflow
    float prescale_f = ((float)PCA9685_OSC_FREQUENCY / (4096.0f * (float)frequency_hz)) - 1.0f;

    // Clamp prescale to valid range [3, 255]
    if (prescale_f < 3.0f) {
        prescale_f = 3.0f;
    }
    if (prescale_f > 255.0f) {
        prescale_f = 255.0f;
    }

    uint8_t prescale = (uint8_t)(prescale_f + 0.5f);

    // Must set SLEEP bit before modifying PRE_SCALE
    uint8_t mode1;
    esp_err_t ret = pca9685_read_register(dev, PCA9685_REG_MODE1, &mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    mode1 |= PCA9685_MODE1_SLEEP;
    ret = pca9685_write_register(dev, PCA9685_REG_MODE1, mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    // Write prescaler
    ret = pca9685_write_register(dev, PCA9685_REG_PRE_SCALE, prescale);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    // Clear SLEEP bit to resume operation
    mode1 &= ~PCA9685_MODE1_SLEEP;
    ret = pca9685_write_register(dev, PCA9685_REG_MODE1, mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    // Set RESTART bit
    mode1 |= PCA9685_MODE1_RESTART;
    ret = pca9685_write_register(dev, PCA9685_REG_MODE1, mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    dev->current_frequency = frequency_hz;
    float actual_freq = (float)PCA9685_OSC_FREQUENCY / (4096.0f * (float)(prescale + 1));
    ESP_LOGI(TAG, "Set frequency: %d Hz (prescale: %d, actual: %.2f Hz)",
             frequency_hz, prescale, actual_freq);

    return ESP_OK;
}

esp_err_t pca9685_set_pwm(pca9685_t *dev, uint8_t channel, uint16_t on, uint16_t off) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Validate channel
    if (channel >= PCA9685_NUM_CHANNELS) {
        ESP_LOGE(TAG, "Invalid channel: %d (range: 0-%d)", channel, PCA9685_NUM_CHANNELS - 1);
        return ESP_ERR_INVALID_ARG;
    }

    // Validate PWM values (12-bit)
    if (on > PCA9685_MAX_PWM_VALUE) {
        ESP_LOGE(TAG, "ON value out of range: 0x%04X (max: 0x%04X)", on, PCA9685_MAX_PWM_VALUE);
        return ESP_ERR_INVALID_ARG;
    }

    if (off > PCA9685_MAX_PWM_VALUE) {
        ESP_LOGE(TAG, "OFF value out of range: 0x%04X (max: 0x%04X)", off, PCA9685_MAX_PWM_VALUE);
        return ESP_ERR_INVALID_ARG;
    }

    // CRITICAL: Prevent ON == OFF (hardware limitation)
    if (on == off && on != 0x1000) {  // 0x1000 = full ON or OFF special case
        ESP_LOGE(TAG, "ON and OFF values cannot be identical: 0x%04X", on);
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate register addresses
    uint8_t on_l_reg = PCA9685_REG_LED0_ON_L + (channel * 4);
    uint8_t on_h_reg = on_l_reg + 1;
    uint8_t off_l_reg = on_l_reg + 2;
    uint8_t off_h_reg = on_l_reg + 3;

    // Extract bytes
    uint8_t on_l = on & 0xFF;
    uint8_t on_h = (on >> 8) & 0x0F;
    uint8_t off_l = off & 0xFF;
    uint8_t off_h = (off >> 8) & 0x0F;

    // Write registers
    esp_err_t ret;

    ret = pca9685_write_register(dev, on_l_reg, on_l);
    if (ret != ESP_OK) return ret;

    ret = pca9685_write_register(dev, on_h_reg, on_h);
    if (ret != ESP_OK) return ret;

    ret = pca9685_write_register(dev, off_l_reg, off_l);
    if (ret != ESP_OK) return ret;

    ret = pca9685_write_register(dev, off_h_reg, off_h);
    if (ret != ESP_OK) return ret;

    ESP_LOGV(TAG, "Channel %d: ON=0x%04X, OFF=0x%04X", channel, on, off);

    return ESP_OK;
}

esp_err_t pca9685_set_pwm_pulse(pca9685_t *dev, uint8_t channel, uint16_t pulse_us) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Calculate period in microseconds
    uint32_t period_us = 1000000 / dev->current_frequency;

    // Validate pulse width
    if (pulse_us > period_us) {
        ESP_LOGE(TAG, "Pulse width too long: %d us (period: %d us)", pulse_us, period_us);
        return ESP_ERR_INVALID_ARG;
    }

    // Convert to counter steps
    uint16_t pulse_steps = (pulse_us * 4096) / period_us;

    // Clamp to valid range
    if (pulse_steps > 4095) {
        pulse_steps = 4095;
    }

    // Set ON to 0, OFF to pulse width
    uint16_t on = 0;
    uint16_t off = pulse_steps;

    // Ensure ON != OFF
    if (on == off && off < 4095) {
        off++;
    }

    return pca9685_set_pwm(dev, channel, on, off);
}

esp_err_t pca9685_deinit(pca9685_t *dev) {
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->initialized) {
        return ESP_OK;
    }

    // Set all channels to OFF
    pca9685_write_register(dev, PCA9685_REG_ALLLED_OFF_H, 0x10);

    dev->initialized = false;
    ESP_LOGI(TAG, "Deinitialized PCA9685 at address 0x%02X", dev->address);

    return ESP_OK;
}
```

---

## 12. TESTING & VALIDATION CHECKLIST

### 12.1 Register Access Tests
- [ ] Read/write MODE1 register successfully
- [ ] Read/write MODE2 register successfully
- [ ] Read/write PRE_SCALE register (with SLEEP bit handling)
- [ ] Read/write individual LED ON/OFF registers
- [ ] Burst write to all LED registers using auto-increment
- [ ] Verify 12-bit register split across two 8-bit registers

### 12.2 Frequency Setting Tests
- [ ] Set frequency to 50 Hz (servo standard)
- [ ] Set frequency to 1526 Hz (maximum)
- [ ] Set frequency to 24 Hz (minimum)
- [ ] Verify prescaler clamping prevents overflow
- [ ] Verify actual frequency within 1% of requested

### 12.3 PWM Output Tests
- [ ] Set 0% duty cycle (OFF=0, ON=4095 or similar)
- [ ] Set 50% duty cycle
- [ ] Set 100% duty cycle (ON=0, OFF=4095)
- [ ] Set phase-shifted PWM (ON != 0)
- [ ] Reject identical ON/OFF values
- [ ] Verify output on oscilloscope

### 12.4 I2C Communication Tests
- [ ] Detect device presence at correct address
- [ ] Handle I2C timeouts gracefully
- [ ] Retry failed I2C transactions
- [ ] Verify data integrity with CRC
- [ ] Test at maximum I2C bus speed (1 Mbps)

### 12.5 Safety & Security Tests
- [ ] Reject invalid channel numbers
- [ ] Reject PWM values > 0x0FFF
- [ ] Reject invalid frequencies
- [ ] Detect address conflicts
- [ ] Monitor I2C error rates
- [ ] Handle NULL pointers safely
- [ ] Verify no buffer overflows

### 12.6 Edge Case Tests
- [ ] Frequency boundary values (24, 1526 Hz)
- [ ] Maximum PWM resolution (0x0000, 0x0FFF)
- [ ] Prescaler boundary values (3, 255)
- [ ] Multiple device addressing
- [ ] Bus lockup recovery
- [ ] Brownout/reset handling

---

## 13. QUICK REFERENCE TABLES

### 13.1 Register Summary

| Operation | Register | Bits | Valid Range | Notes |
|-----------|----------|------|-------------|-------|
| Mode Control | MODE1 | 0x00 | 0x00-0xFF | Only SLEEP=1 before PRE_SCALE write |
| Output Config | MODE2 | 0x01 | 0x00-0xFF | Configure OUTDRV before operation |
| PWM Frequency | PRE_SCALE | 0xFE | 3-255 | Writable only when SLEEP=1 |
| LED ON Time | LED0_ON_L/H | 0x06-0x07 | 0x000-0x0FFF | 12-bit value split across 2 regs |
| LED OFF Time | LED0_OFF_L/H | 0x08-0x09 | 0x000-0x0FFF | 12-bit value split across 2 regs |

### 13.2 I2C Address Summary

| Address | Name | Used For | Default |
|---------|------|----------|---------|
| 0x40-0x7E | Individual | Per-device addressing | Via A0-A5 pins |
| 0xE0 | ALL_CALL | Broadcast to all | Enabled at power-up |
| 0xE2 | SUBADR1 | Group addressing | Disabled at power-up |
| 0xE4 | SUBADR2 | Group addressing | Disabled at power-up |
| 0xE8 | SUBADR3 | Group addressing | Disabled at power-up |

### 13.3 Frequency / Prescaler Reference

| Frequency (Hz) | Prescale | Accuracy |
|---|---|---|
| 1526 | 3 | +0.01% |
| 1000 | 6 | -0.23% |
| 500 | 12 | +0.16% |
| 200 | 30 (default) | +0.16% |
| 100 | 61 | -0.16% |
| 50 | 121 | +0.16% |
| 24 | 255 | -1.33% |

---

## 14. ADDITIONAL RESOURCES

**Official Documentation:**
- NXP PCA9685 Datasheet: https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf
- Adafruit PCA9685 Guide: https://learn.adafruit.com/16-channel-pwm-servo-driver/

**Open Source Implementations:**
- esp-idf-lib: https://github.com/UncleRus/esp-idf-lib/tree/master/components/pca9685
- kimsniper/pca9685: https://github.com/kimsniper/pca9685

**ESP32-IDF Documentation:**
- I2C Driver: https://docs.espressif.com/projects/esp-idf/latest/esp32/api-reference/peripherals/i2c.html

---

**Document Version:** 1.0
**Last Updated:** November 2025
**Author:** Technical Research - Anthropic Claude Code

