# Remaining Drivers Implementation Plans

**Document Purpose:** Consolidated planning for Items 8, 9, and 10 from todo.md
**Status:** 🔴 NOT STARTED - Medium Priority peripheral drivers
**Total Estimated Effort:** 24-32 hours (all three drivers combined)

---

## Table of Contents

1. [I2C Driver for BMS (Item 8)](#1-i2c-driver-for-bms-item-8)
2. [ADC Driver for Motor Current Sensing (Item 9)](#2-adc-driver-for-motor-current-sensing-item-9)
3. [DS18B20 Temperature Sensor Driver (Item 10)](#3-ds18b20-temperature-sensor-driver-item-10)

---

# 1. I2C Driver for BMS (Item 8)

**Status:** 🔴 NOT STARTED - No BMS communication
**Priority:** MEDIUM - Required for battery monitoring
**Estimated Effort:** 10-12 hours
**Dependencies:** RX72N Manual Ch55 (RIIC), BMS chip datasheet

## Problem Statement

The STAR robot needs **Battery Management System (BMS)** monitoring for:
- Battery voltage measurement (6S LiPo = 22.2V nominal)
- Cell voltage balancing status
- Battery current draw (charge/discharge)
- Temperature monitoring
- Low voltage warning (prevent over-discharge)
- State of Charge (SoC) estimation

**Current state:** NO I2C driver exists, NO BMS communication possible.

## Pin Assignments

| Function | Pin | RX72N Function | Notes |
|----------|-----|---------------|-------|
| **SCL (Clock)** | P12 (pin 34) | RIIC0 SMBC0 | FM+ mode (1 MHz) |
| **SDA (Data)** | P13 (pin 33) | RIIC0 SMBD0 | Open-drain + pull-up |
| **I2C Speed** | - | Fast Mode Plus | 1 MHz (supports standard 100 kHz, fast 400 kHz) |

## I2C HAL Design

### API Interface (`lib/rx_hal/inc/rx_i2c.h`)

```c
/**
 * @enum rx_i2c_channel_t
 * @brief I2C channel identifiers
 */
typedef enum : uint8_t {
    k_rx_i2c0 = 0,  /**< RIIC0 (P12/P13) - BMS */
    k_rx_i2c1 = 1,  /**< RIIC1 (not used) */
    k_rx_i2c2 = 2,  /**< RIIC2 (not used) */
} rx_i2c_channel_t;

/**
 * @enum rx_i2c_speed_t
 * @brief I2C communication speed modes
 */
typedef enum : uint32_t {
    k_i2c_speed_standard = 100000,   /**< 100 kHz standard mode */
    k_i2c_speed_fast = 400000,       /**< 400 kHz fast mode */
    k_i2c_speed_fast_plus = 1000000, /**< 1 MHz fast mode plus */
} rx_i2c_speed_t;

/**
 * @struct rx_i2c_config_t
 * @brief I2C controller configuration
 */
typedef struct {
    rx_i2c_channel_t channel;  /**< I2C channel (RIIC0/1/2) */
    rx_i2c_speed_t speed;      /**< Bus speed (100k, 400k, 1M Hz) */
    uint8_t address_bits;      /**< 7-bit or 10-bit addressing (typically 7) */
    uint16_t timeout_ms;       /**< Transaction timeout in milliseconds */
} rx_i2c_config_t;

/**
 * @brief Initialize I2C controller
 *
 * @param[in,out] handle I2C handle
 * @param[in] config I2C configuration (channel, speed, addressing)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, I2C ready
 * @retval k_rx_err_hardware I2C initialization failed
 *
 * @pre RIIC clock enabled (MSTPCRB bit 21)
 * @post I2C controller configured and ready
 */
rx_err_t rx_i2c_init(rx_i2c_handle_t* handle, const rx_i2c_config_t* config);

/**
 * @brief Write data to I2C peripheral
 *
 * @param[in] handle I2C handle
 * @param[in] device_addr 7-bit I2C peripheral address
 * @param[in] reg_addr Register address to write
 * @param[in] data Data buffer
 * @param[in] length Data length in bytes
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, data written
 * @retval k_rx_err_timeout I2C transaction timeout
 * @retval k_rx_err_nack Peripheral NACK (not responding)
 */
rx_err_t rx_i2c_write_reg(rx_i2c_handle_t* handle, uint8_t device_addr,
                           uint8_t reg_addr, const uint8_t* data, size_t length);

/**
 * @brief Read data from I2C peripheral
 *
 * @param[in] handle I2C handle
 * @param[in] device_addr 7-bit I2C peripheral address
 * @param[in] reg_addr Register address to read
 * @param[out] data Data buffer (must be allocated)
 * @param[in] length Data length in bytes
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, data read
 * @retval k_rx_err_timeout I2C transaction timeout
 * @retval k_rx_err_nack Peripheral NACK (not responding)
 */
rx_err_t rx_i2c_read_reg(rx_i2c_handle_t* handle, uint8_t device_addr,
                          uint8_t reg_addr, uint8_t* data, size_t length);
```

### Implementation Notes

**Ch55 (RIIC) Key Registers:**
- **ICCR1:** I2C Control Register 1 (enable, start/stop conditions)
- **ICCR2:** I2C Control Register 2 (interrupts, clock stretching)
- **ICMR1-3:** I2C Mode Registers (speed, noise filter, digital filter)
- **ICFER:** I2C Function Enable Register (master/peripheral select, SCL synchronization)
- **ICBRL/ICBRH:** I2C Bit Rate Low/High Registers (clock speed configuration)
- **ICDRT:** I2C Data Transmit Register
- **ICDRR:** I2C Data Receive Register
- **ICSR1-2:** I2C Status Registers (ACK/NACK, arbitration, stop detection)

**Clock configuration** (1 MHz Fast Mode Plus):
```
PCLK = 60 MHz (after divider)
I2C Clock = PCLK / (ICBRL + ICBRH + 2)
For 1 MHz: ICBRL = 28, ICBRH = 28 (60M / 58 ≈ 1.03 MHz)
```

## BMS Integration

### Typical BMS Chip Examples

**Option 1: BQ76920 (Texas Instruments)**
- 3-5 cell LiPo monitor
- I2C address: 0x08 or 0x18 (configurable)
- Cell voltage measurement: 12-bit ADC
- Temperature monitoring: 2 external NTC inputs
- Overcurrent/overvoltage protection

**Option 2: MAX17320 (Maxim)**
- Fuel gauge IC with I2C
- SoC estimation, voltage, current, temperature
- I2C address: 0x36
- High accuracy (±1% SoC)

### BMS Driver API (`lib/rx_hal/inc/rx_bms.h`)

```c
/**
 * @struct bms_telemetry_t
 * @brief BMS telemetry data
 */
typedef struct {
    float battery_voltage_v;     /**< Total battery voltage (V) */
    float cell_voltages_v[6];    /**< Individual cell voltages (V) [0-5 for 6S] */
    float current_a;             /**< Battery current (A, positive = discharge) */
    float temperature_c;         /**< Battery temperature (°C) */
    float soc_percent;           /**< State of Charge (0-100%) */
    uint16_t cycle_count;        /**< Charge/discharge cycle count */
    bool fault_overvoltage;      /**< Overvoltage fault flag */
    bool fault_undervoltage;     /**< Undervoltage fault flag */
    bool fault_overcurrent;      /**< Overcurrent fault flag */
    bool fault_overtemperature;  /**< Overtemperature fault flag */
} bms_telemetry_t;

/**
 * @brief Initialize BMS communication
 *
 * @param[in,out] handle BMS handle
 * @param[in] i2c_handle I2C bus handle
 * @param[in] device_addr BMS I2C address (e.g., 0x08)
 *
 * @return rx_err_t Error code
 */
rx_err_t bms_init(bms_handle_t* handle, rx_i2c_handle_t* i2c_handle, uint8_t device_addr);

/**
 * @brief Read BMS telemetry (voltage, current, temperature, SoC)
 *
 * @param[in] handle BMS handle
 * @param[out] telemetry Telemetry data structure
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, telemetry updated
 * @retval k_rx_err_timeout I2C timeout (BMS not responding)
 */
rx_err_t bms_read_telemetry(bms_handle_t* handle, bms_telemetry_t* telemetry);

/**
 * @brief Check for BMS faults (overvoltage, undervoltage, overcurrent, temperature)
 *
 * @param[in] handle BMS handle
 * @param[out] faults Bitmask of active faults
 *
 * @return rx_err_t Error code
 */
rx_err_t bms_check_faults(bms_handle_t* handle, uint8_t* faults);
```

## Implementation Phases

### Phase 1: I2C HAL (5-6 hours)
- [ ] Create `rx72n_i2c_regs.h` with RIIC0 register definitions
- [ ] Verify register addresses against Ch55
- [ ] Implement `rx_i2c.c` HAL (init, write, read)
- [ ] Configure clock speed (1 MHz Fast Mode Plus)
- [ ] Test I2C loopback (read/write to EEPROM or I2C peripheral)

### Phase 2: BMS Driver (3-4 hours)
- [ ] Select BMS chip (BQ76920, MAX17320, or other)
- [ ] Obtain BMS datasheet and register map
- [ ] Implement `rx_bms.c` driver
- [ ] Implement telemetry reading (voltage, current, temp, SoC)
- [ ] Implement fault detection

### Phase 3: Integration (1-2 hours)
- [ ] Create BMS monitoring task (1 Hz update rate)
- [ ] Integrate with LED error indicators (LED 1 - low battery)
- [ ] Add telemetry to Protocol Buffer messages (send to RPi5)
- [ ] Test on hardware with actual BMS

### Phase 4: Testing and Documentation (1-2 hours)
- [ ] Unit tests (mock I2C, test BMS register parsing)
- [ ] Hardware integration test
- [ ] Comprehensive Doxygen documentation
- [ ] Update README.md

## Success Criteria

- ✅ I2C communication working at 1 MHz
- ✅ BMS telemetry readable (voltage, current, SoC)
- ✅ Fault detection operational
- ✅ Low battery warning integrates with LED indicator
- ✅ NASA Power of 10 compliant

## References

- **RX72N Manual Ch55:** RIIC (I2C Bus Interface)
- **BQ76920 Datasheet:** Texas Instruments 3-5 cell battery monitor
- **MAX17320 Datasheet:** Maxim fuel gauge IC

---

# 2. ADC Driver for Motor Current Sensing (Item 9)

**Status:** 🔴 NOT STARTED - No current monitoring
**Priority:** MEDIUM - Required for overcurrent protection and current limiting
**Estimated Effort:** 8-10 hours
**Dependencies:** RX72N Manual Ch56 (12-bit ADC), DRV8243 current sense amplifiers

## Problem Statement

The STAR robot needs **motor current sensing** for:
- **Overcurrent protection:** Detect stalled motors (>3A per motor)
- **Current limiting:** Reduce PWM duty cycle when approaching limits
- **Power monitoring:** Calculate total power consumption
- **Diagnostics:** Detect wiring faults (short circuits, open circuits)
- **PID tuning:** Use current feedback for torque control

**Current state:** NO ADC driver, NO current monitoring.

## Pin Assignments

| Motor | Current Sense Pin | ADC Channel | Notes |
|-------|------------------|-------------|-------|
| **Front Left** | P40/AN000 (pin 95) | ADC0 Ch0 | DRV8243 IPROPI output |
| **Front Right** | P41/AN001 (pin 93) | ADC0 Ch1 | DRV8243 IPROPI output |
| **Rear Left** | P42/AN002 (pin 92) | ADC0 Ch2 | DRV8243 IPROPI output |
| **Rear Right** | P43/AN003 (pin 91) | ADC0 Ch3 | DRV8243 IPROPI output |

**DRV8243 Current Sense Output:**
- **IPROPI:** Current proportional output (voltage = K × I_motor)
- **Gain:** K ≈ 0.38 V/A (typical, verify with datasheet)
- **Range:** 0-3.3V → 0-8.7A motor current
- **RX72N ADC:** 12-bit (0-4095 counts), Vref = 3.3V
- **Resolution:** 3.3V / 4096 = 0.8 mV per count → 2.1 mA current resolution

## ADC HAL Design

### API Interface (`lib/rx_hal/inc/rx_adc.h`)

```c
/**
 * @enum rx_adc_channel_t
 * @brief ADC channel identifiers
 */
typedef enum : uint8_t {
    k_rx_adc_ch0 = 0,   /**< AN000 - Front Left Motor Current */
    k_rx_adc_ch1 = 1,   /**< AN001 - Front Right Motor Current */
    k_rx_adc_ch2 = 2,   /**< AN002 - Rear Left Motor Current */
    k_rx_adc_ch3 = 3,   /**< AN003 - Rear Right Motor Current */
    k_rx_adc_ch_count = 4,
} rx_adc_channel_t;

/**
 * @enum rx_adc_mode_t
 * @brief ADC scan modes
 */
typedef enum : uint8_t {
    k_adc_mode_single = 0,      /**< Single-shot conversion */
    k_adc_mode_continuous = 1,  /**< Continuous scan mode */
    k_adc_mode_group = 2,       /**< Group scan (4 channels simultaneously) */
} rx_adc_mode_t;

/**
 * @struct rx_adc_config_t
 * @brief ADC configuration
 */
typedef struct {
    rx_adc_mode_t mode;         /**< Scan mode (single, continuous, group) */
    uint8_t channels_mask;      /**< Bitmask of channels to scan (0x0F = all 4) */
    uint16_t sample_time_us;    /**< Sample time in microseconds (default 10µs) */
    bool use_dma;               /**< Use DMA for results (recommended for continuous mode) */
} rx_adc_config_t;

/**
 * @brief Initialize ADC peripheral
 *
 * @param[in,out] handle ADC handle
 * @param[in] config ADC configuration (mode, channels, sample time)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, ADC ready
 * @retval k_rx_err_hardware ADC initialization failed
 *
 * @pre ADC clock enabled (MSTPCRA bit 17)
 * @post ADC configured for selected channels and mode
 */
rx_err_t rx_adc_init(rx_adc_handle_t* handle, const rx_adc_config_t* config);

/**
 * @brief Read ADC channel (blocking)
 *
 * @param[in] handle ADC handle
 * @param[in] channel ADC channel to read
 * @param[out] value 12-bit ADC value (0-4095)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, value read
 * @retval k_rx_err_timeout ADC conversion timeout
 *
 * @note Blocking (waits for conversion complete)
 * @note Conversion time: ~50µs @ 240 MHz
 */
rx_err_t rx_adc_read_channel(rx_adc_handle_t* handle, rx_adc_channel_t channel, uint16_t* value);

/**
 * @brief Read all ADC channels simultaneously (group scan)
 *
 * @param[in] handle ADC handle
 * @param[out] values Array of 4 ADC values (indexed by channel)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, all channels read
 *
 * @note Uses simultaneous sampling (all channels captured at same instant)
 * @note Conversion time: ~50µs total for all 4 channels
 */
rx_err_t rx_adc_read_all_channels(rx_adc_handle_t* handle, uint16_t values[4]);
```

### Motor Current Conversion

**Convert ADC counts to motor current (Amperes):**

```c
/**
 * @brief Convert ADC value to motor current (Amperes)
 *
 * @param[in] adc_value 12-bit ADC reading (0-4095)
 * @param[out] current_a Motor current in Amperes
 *
 * @details
 * DRV8243 IPROPI gain: 0.38 V/A (typical)
 * ADC resolution: 3.3V / 4096 = 0.8 mV per count
 * Current = (ADC_value × 0.8mV) / 380mV/A = ADC_value / 475
 *
 * @return rx_err_t Error code
 */
rx_err_t motor_current_from_adc(uint16_t adc_value, float* current_a) {
    const float vref = 3.3f;          // ADC reference voltage (V)
    const uint16_t adc_max = 4095;    // 12-bit ADC max value
    const float gain = 0.38f;         // DRV8243 current sense gain (V/A)

    float voltage = ((float)adc_value / (float)adc_max) * vref;
    *current_a = voltage / gain;

    return k_rx_ok;
}
```

## Overcurrent Protection

### Thresholds

| Condition | Threshold (A) | Action |
|-----------|--------------|--------|
| **Normal** | 0 - 2.0 A | Full motor control |
| **Warning** | 2.0 - 2.5 A | Log warning, no action |
| **Limit** | 2.5 - 3.0 A | Reduce PWM duty cycle by 50% |
| **Critical** | > 3.0 A | **Emergency stop motor** (stall detected) |

### Integration with Motor Control

**In motor control task** (1 kHz loop):

```c
void motor_control_task_1khz(void) {
    // Read motor currents (all 4 channels)
    uint16_t adc_values[4];
    rx_adc_read_all_channels(&adc_handle, adc_values);

    // Convert to currents
    float currents_a[4];
    for (int i = 0; i < 4; i++) {
        motor_current_from_adc(adc_values[i], &currents_a[i]);
    }

    // Check overcurrent on each motor
    for (int i = 0; i < 4; i++) {
        if (currents_a[i] > 3.0f) {
            // CRITICAL: Emergency stop motor
            motor_emergency_stop(i);
            rx_log_error("MOTOR", "Motor %d overcurrent: %.2f A", i, currents_a[i]);
        } else if (currents_a[i] > 2.5f) {
            // LIMIT: Reduce PWM by 50%
            motor_set_pwm_limit(i, 0.5f);
        } else if (currents_a[i] > 2.0f) {
            // WARNING: Log only
            rx_log_warn("MOTOR", "Motor %d high current: %.2f A", i, currents_a[i]);
        }
    }

    // Continue with PID control...
}
```

## Implementation Phases

### Phase 1: ADC HAL (4-5 hours)
- [ ] Create `rx72n_adc_regs.h` with ADC register definitions
- [ ] Verify register addresses against Ch56
- [ ] Implement `rx_adc.c` HAL (init, single read, group scan)
- [ ] Test ADC with known voltage (3.3V Vref, GND)
- [ ] Verify 12-bit resolution

### Phase 2: Motor Current Integration (2-3 hours)
- [ ] Implement ADC-to-current conversion function
- [ ] Add current monitoring to motor control task
- [ ] Implement overcurrent thresholds (warning, limit, critical)
- [ ] Test on hardware with actual motors

### Phase 3: Testing (1-2 hours)
- [ ] Unit tests (ADC conversion, current calculation)
- [ ] Hardware test (measure known currents with multimeter)
- [ ] Overcurrent protection test (stall motor intentionally)
- [ ] Comprehensive Doxygen documentation

### Phase 4: Documentation (1 hour)
- [ ] Document current sensing circuit
- [ ] Update motor control documentation
- [ ] Add to README.md

## Success Criteria

- ✅ ADC reads motor currents accurately (±50 mA)
- ✅ Overcurrent protection triggers at 3A
- ✅ Current limiting reduces PWM at 2.5A
- ✅ All 4 motor currents monitored simultaneously
- ✅ NASA Power of 10 compliant

## References

- **RX72N Manual Ch56:** 12-bit ADC
- **DRV8243 Datasheet:** Current sense amplifier specs
- **Motor Control Task:** `/workspaces/STAR/e2-studio-star-rx72n-firmware/src/tasks/motor_control_task.c`

---

# 3. DS18B20 Temperature Sensor Driver (Item 10)

**Status:** 🔴 NOT STARTED - No temperature monitoring
**Priority:** MEDIUM - Required for thermal management
**Estimated Effort:** 6-8 hours
**Dependencies:** RX72N Manual Ch23 (GPIO), DS18B20 datasheet (1-Wire protocol)

## Problem Statement

The STAR robot needs **temperature monitoring** for:
- **Ambient temperature:** Environment sensing
- **Motor driver temperature:** Thermal throttling to prevent overheating
- **Battery temperature:** Safety (LiPo overheating → fire risk)
- **System diagnostics:** Detect cooling issues

**Current state:** NO 1-Wire driver, NO temperature sensors.

## Pin Assignment

| Function | Pin | Notes |
|----------|-----|-------|
| **1-Wire DQ** | P05 (pin 100) | Single data line (bidirectional) |
| **Pull-up** | External 4.7kΩ | Required for 1-Wire bus |

**DS18B20 Features:**
- **Temperature Range:** -55°C to +125°C
- **Accuracy:** ±0.5°C (-10°C to +85°C)
- **Resolution:** 9-12 bits (0.5°C to 0.0625°C)
- **Conversion Time:** 750ms @ 12-bit resolution
- **Protocol:** 1-Wire (single data line, bidirectional)
- **Addressing:** Each sensor has unique 64-bit ROM code

## 1-Wire Protocol Overview

**1-Wire timing-critical operations:**

1. **Reset Pulse:** Master pulls DQ LOW for 480µs, then releases
2. **Presence Pulse:** Sensor pulls DQ LOW for 60-240µs to acknowledge
3. **Write Bit 0:** Master pulls DQ LOW for 60-120µs
4. **Write Bit 1:** Master pulls DQ LOW for 1-15µs, releases
5. **Read Bit:** Master pulls DQ LOW for 1-15µs, samples at 15µs

**Microsecond-accurate timing required** (use CMT timer or software delay).

## 1-Wire HAL Design

### API Interface (`lib/rx_hal/inc/rx_onewire.h`)

```c
/**
 * @enum onewire_result_t
 * @brief 1-Wire operation results
 */
typedef enum : uint8_t {
    k_onewire_ok = 0,            /**< Success */
    k_onewire_no_presence = 1,   /**< No sensor detected on bus */
    k_onewire_crc_error = 2,     /**< CRC mismatch */
    k_onewire_timeout = 3,       /**< Operation timeout */
} onewire_result_t;

/**
 * @struct rx_onewire_handle_t
 * @brief 1-Wire bus handle
 */
typedef struct {
    rx_port_pin_t dq_pin;  /**< Data pin (P05) */
    bool initialized;      /**< Initialization flag */
} rx_onewire_handle_t;

/**
 * @brief Initialize 1-Wire bus
 *
 * @param[in,out] handle 1-Wire handle
 * @param[in] dq_pin Data pin (must have external pull-up)
 *
 * @return rx_err_t Error code
 */
rx_err_t rx_onewire_init(rx_onewire_handle_t* handle, rx_port_pin_t dq_pin);

/**
 * @brief Reset 1-Wire bus and check for presence pulse
 *
 * @param[in] handle 1-Wire handle
 *
 * @return onewire_result_t Result
 * @retval k_onewire_ok Sensor present
 * @retval k_onewire_no_presence No sensor detected
 */
onewire_result_t rx_onewire_reset(rx_onewire_handle_t* handle);

/**
 * @brief Write byte to 1-Wire bus
 *
 * @param[in] handle 1-Wire handle
 * @param[in] byte Data byte to write
 *
 * @return rx_err_t Error code
 */
rx_err_t rx_onewire_write_byte(rx_onewire_handle_t* handle, uint8_t byte);

/**
 * @brief Read byte from 1-Wire bus
 *
 * @param[in] handle 1-Wire handle
 * @param[out] byte Data byte read
 *
 * @return rx_err_t Error code
 */
rx_err_t rx_onewire_read_byte(rx_onewire_handle_t* handle, uint8_t* byte);
```

## DS18B20 Driver API

### Interface (`lib/rx_hal/inc/rx_ds18b20.h`)

```c
/**
 * @struct ds18b20_handle_t
 * @brief DS18B20 temperature sensor handle
 */
typedef struct {
    rx_onewire_handle_t* onewire;  /**< 1-Wire bus handle */
    uint8_t rom_code[8];           /**< 64-bit ROM code (unique ID) */
    bool initialized;              /**< Initialization flag */
} ds18b20_handle_t;

/**
 * @brief Initialize DS18B20 sensor
 *
 * @param[in,out] handle DS18B20 handle
 * @param[in] onewire 1-Wire bus handle
 *
 * @return rx_err_t Error code
 */
rx_err_t ds18b20_init(ds18b20_handle_t* handle, rx_onewire_handle_t* onewire);

/**
 * @brief Start temperature conversion (non-blocking)
 *
 * @param[in] handle DS18B20 handle
 *
 * @return rx_err_t Error code
 *
 * @note Conversion takes 750ms @ 12-bit resolution
 * @note Call ds18b20_read_temperature() after 750ms to get result
 */
rx_err_t ds18b20_start_conversion(ds18b20_handle_t* handle);

/**
 * @brief Read temperature (after conversion complete)
 *
 * @param[in] handle DS18B20 handle
 * @param[out] temperature_c Temperature in Celsius
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, temperature valid
 * @retval k_rx_err_crc CRC mismatch
 */
rx_err_t ds18b20_read_temperature(ds18b20_handle_t* handle, float* temperature_c);
```

### Usage Pattern

```c
// Initialize
rx_onewire_handle_t onewire = {0};
rx_onewire_init(&onewire, k_rx_p0_5);

ds18b20_handle_t temp_sensor = {0};
ds18b20_init(&temp_sensor, &onewire);

// Periodic temperature reading (every 1 second)
void temperature_task_1hz(void) {
    static bool conversion_started = false;
    static uint32_t start_time_ms = 0;

    if (!conversion_started) {
        // Start conversion
        ds18b20_start_conversion(&temp_sensor);
        start_time_ms = get_milliseconds();
        conversion_started = true;
    } else {
        // Check if conversion complete (750ms elapsed)
        uint32_t elapsed_ms = get_milliseconds() - start_time_ms;
        if (elapsed_ms >= 750) {
            // Read temperature
            float temperature_c;
            rx_err_t err = ds18b20_read_temperature(&temp_sensor, &temperature_c);
            if (err == k_rx_ok) {
                printf("Temperature: %.2f °C\n", temperature_c);

                // Check for overtemperature
                if (temperature_c > 60.0f) {
                    // Thermal throttling: reduce motor power
                    motor_set_thermal_limit(0.7f);  // 70% max power
                }
            }

            conversion_started = false;  // Restart cycle
        }
    }
}
```

## Thermal Throttling

**Temperature thresholds:**

| Temperature (°C) | Action |
|------------------|--------|
| < 40°C | Normal operation (100% motor power) |
| 40-50°C | Warning (log message) |
| 50-60°C | Throttle motors to 70% power |
| > 60°C | Emergency stop (prevent thermal damage) |

## Implementation Phases

### Phase 1: 1-Wire HAL (3-4 hours)
- [ ] Implement `rx_onewire.c` (reset, write bit, read bit, write byte, read byte)
- [ ] Use CMT timer for microsecond delays
- [ ] Test 1-Wire timing with oscilloscope (verify 480µs reset pulse)
- [ ] Test with actual DS18B20 sensor

### Phase 2: DS18B20 Driver (2-3 hours)
- [ ] Implement `rx_ds18b20.c` (init, start conversion, read temperature)
- [ ] Implement CRC-8 validation (DS18B20 includes CRC in data)
- [ ] Test temperature reading accuracy (compare with thermometer)

### Phase 3: Integration (1 hour)
- [ ] Create temperature monitoring task (1 Hz)
- [ ] Implement thermal throttling
- [ ] Add temperature to telemetry (Protocol Buffers)

### Phase 4: Documentation (1 hour)
- [ ] Comprehensive Doxygen documentation
- [ ] 1-Wire timing diagram
- [ ] Update README.md

## Success Criteria

- ✅ DS18B20 temperature readable (±0.5°C accuracy)
- ✅ 1-Wire protocol timing correct (verified with oscilloscope)
- ✅ Thermal throttling works (reduces motor power at 50°C)
- ✅ NASA Power of 10 compliant

## References

- **DS18B20 Datasheet:** Maxim Integrated 1-Wire temperature sensor
- **1-Wire Protocol:** https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
- **RX72N Manual Ch23:** GPIO configuration for open-drain output

---

## Summary

These three drivers (I2C BMS, ADC motor current, DS18B20 temperature) provide critical monitoring and safety features for the STAR robot:

1. **Battery monitoring** (I2C BMS) - Prevent over-discharge, estimate SoC
2. **Overcurrent protection** (ADC) - Detect stalled motors, current limiting
3. **Thermal management** (DS18B20) - Prevent overheating, thermal throttling

**Total estimated effort:** 24-32 hours for all three drivers.

**Next steps:** Prioritize based on hardware availability and project timeline.

---

**Document Version:** 1.0
**Last Updated:** 2026-02-05
**Author:** STAR Development Team
**Status:** Ready for Implementation
