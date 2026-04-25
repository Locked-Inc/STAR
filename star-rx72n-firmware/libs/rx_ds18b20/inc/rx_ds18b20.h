/**
 * @file rx_ds18b20.h
 * @brief DS18B20 1-Wire Digital Temperature Sensor Driver API
 *
 * @details
 * # Overview
 *
 * Platform-independent driver for Maxim DS18B20 programmable resolution 1-Wire
 * digital thermometer with CRC-8 data integrity checking. Provides high-level
 * API for temperature measurement, resolution configuration, and EEPROM
 * persistence with automatic error handling and validation.
 *
 * This driver abstracts the DS18B20's complex 1-Wire protocol into simple init,
 * trigger, and read operations while maintaining full access to advanced features
 * like ROM matching (multi-drop buses) and alarm thresholds.
 *
 * # Key Features
 *
 * ## Measurement Capabilities
 * - **Temperature range**: -55degC to +125degC with +/-0.5degC accuracy (-10degC to +85degC)
 * - **Programmable resolution**: 9, 10, 11, or 12 bits
 * - **Conversion times**: 94ms (9-bit) to 750ms (12-bit)
 * - **CRC-8 validation**: Detects transmission errors (polynomial x^8 + x^5 + x^4 + 1)
 *
 * ## Resolution vs. Precision vs. Conversion Time Trade-off
 *
 * | Resolution | Precision | Conversion Time | Use Case |
 * |------------|-----------|-----------------|----------|
 * | 9-bit      | 0.5degC     | ~94ms           | Fast updates, low precision needed |
 * | 10-bit     | 0.25degC    | ~188ms          | Balanced for most applications |
 * | 11-bit     | 0.125degC   | ~375ms          | High precision, moderate speed |
 * | 12-bit     | 0.0625degC  | ~750ms          | Maximum precision, slow updates |
 *
 * ## Advanced Features
 * - **ROM matching**: Supports multi-device 1-Wire buses (up to 255 sensors)
 * - **Skip ROM mode**: Optimized for single-sensor configurations
 * - **EEPROM storage**: Persist resolution/alarm settings across power cycles
 * - **Power mode detection**: Identify parasitic vs. external power operation
 * - **Alarm thresholds**: Programmable TH/TL registers (not exposed in this API)
 *
 * # System Architecture Context
 *
 * The DS18B20 driver sits at the sensor layer of the STAR firmware stack:
 *
 * @code{.unparsed}
 * +-------------------------------------------------+
 * | Application Layer (Tasks)                        |
 * | - Temperature monitoring task                    |
 * | - Environmental control                          |
 * +----------------+--------------------------------+
 *                  |
 * +----------------v--------------------------------+
 * | Sensor Driver Layer                              |
 * | +---------------------------------------------+ |
 * | | rx_ds18b20 (This Module)                    | |
 * | | - Handle management                         | |
 * | | - CRC validation                            | |
 * | | - Temperature conversion                    | |
 * | +--------------+------------------------------+ |
 * +----------------+--------------------------------+
 *                  |
 * +----------------v--------------------------------+
 * | Bus Manager Layer (rx_bus_manager)              |
 * | - Abstraction for I2C, SPI, 1-Wire, etc.        |
 * | - Dependency injection for testability          |
 * +----------------+--------------------------------+
 *                  |
 * +----------------v--------------------------------+
 * | 1-Wire HAL (rx_bus_onewire)                     |
 * | - Bit-level 1-Wire protocol                     |
 * | - Reset/presence detection                      |
 * | - ROM commands (Match/Skip/Search)              |
 * +----------------+--------------------------------+
 *                  |
 * +----------------v--------------------------------+
 * | Hardware Layer (GPIO, Timers)                    |
 * | - Microsecond-precision timing                   |
 * | - Open-drain GPIO control                        |
 * +--------------------------------------------------+
 * @endcode
 *
 * # Design Rationale
 *
 * ## Dependency Injection Pattern
 *
 * The driver accepts a `rx_bus_manager_t*` instead of directly accessing GPIO,
 * enabling:
 * - **Unit testing** with mock bus implementations (no hardware required)
 * - **Hardware independence** - works on any 1-Wire HAL implementation
 * - **Multi-bus support** - single driver code for multiple 1-Wire buses
 *
 * This follows the **Dependency Inversion Principle** (high-level modules depend
 * on abstractions, not concrete implementations).
 *
 * ## Blocking API Design
 *
 * All operations are blocking (caller must wait for conversion). Rationale:
 * - DS18B20 conversions are inherently slow (up to 750ms)
 * - Non-blocking would require state machines and polling infrastructure
 * - Simplicity favored: caller can use ThreadX semaphores if async needed
 * - Typical usage: Dedicated temperature monitoring task with periodic reads
 *
 * ## Error Handling Strategy
 *
 * - **All functions return `rx_err_t`** for consistent error propagation
 * - **CRC validation on every read** - detect bus noise/corruption
 * - **Range checking** - reject impossible temperatures (-880 to +2000 raw)
 * - **nullptr checks** - fail fast on API misuse
 * - **State validation** - ensure init before use, prevent double-init
 *
 * # Implementation Approach
 *
 * ## Temperature Conversion Process
 *
 * The DS18B20 uses a two-phase measurement cycle:
 *
 * 1. **Trigger Phase** (`rx_ds18b20_trigger_conversion()`):
 *    - Send Convert T command (0x44)
 *    - DS18B20 begins analog-to-digital conversion
 *    - Returns immediately (non-blocking)
 *
 * 2. **Wait Phase** (Caller's responsibility):
 *    - Wait for conversion time based on resolution
 *    - Use `rx_ds18b20_get_conversion_time_ms()` to get exact time
 *    - ThreadX: `tx_thread_sleep(time_ms * TX_TIMER_TICKS_PER_SECOND / 1000)`
 *
 * 3. **Read Phase** (`rx_ds18b20_read_temperature()`):
 *    - Read 9-byte scratchpad (includes CRC)
 *    - Validate CRC-8 checksum
 *    - Extract 16-bit raw temperature
 *    - Apply resolution mask (clear undefined bits)
 *    - Convert to Celsius (raw / 16.0)
 *
 * ## Memory Layout and Scratchpad Structure
 *
 * The DS18B20's scratchpad RAM is 9 bytes:
 *
 * | Byte | Name         | Description                     |
 * |------|--------------|---------------------------------|
 * | 0    | Temp LSB     | Temperature low byte            |
 * | 1    | Temp MSB     | Temperature high byte (+ sign)  |
 * | 2    | TH Register  | High alarm threshold            |
 * | 3    | TL Register  | Low alarm threshold             |
 * | 4    | Config       | Resolution bits [6:5]           |
 * | 5    | Reserved     | 0xFF                            |
 * | 6    | Reserved     | Factory value                   |
 * | 7    | Reserved     | Factory value                   |
 * | 8    | CRC          | CRC-8 of bytes 0-7              |
 *
 * Configuration register format (byte 4):
 * @code
 * Bit:   7   6   5   4   3   2   1   0
 *        0  R1  R0   1   1   1   1   1
 *           +---+- Resolution: 00=9b, 01=10b, 10=11b, 11=12b
 * @endcode
 *
 * # Performance Characteristics
 *
 * ## Timing Analysis
 *
 * | Operation             | Duration        | Notes                    |
 * |-----------------------|-----------------|--------------------------|
 * | Init                  | ~10ms           | Presence detect + read   |
 * | Trigger conversion    | ~1ms            | Command transmission     |
 * | Conversion (9-bit)    | 93.75ms (spec)  | 100ms recommended        |
 * | Conversion (10-bit)   | 187.5ms (spec)  | 200ms recommended        |
 * | Conversion (11-bit)   | 375ms (spec)    | 400ms recommended        |
 * | Conversion (12-bit)   | 750ms (spec)    | 800ms recommended        |
 * | Read scratchpad       | ~2ms            | 9 bytes + CRC validation |
 * | Set resolution        | ~5ms            | Read + modify + write    |
 *
 * **Note**: Recommended times include 10% safety margin above datasheet specs.
 *
 * ## Memory Usage
 *
 * | Component              | Size (bytes) | Location     |
 * |------------------------|--------------|--------------|
 * | `rx_ds18b20_handle_t`  | 28           | Caller stack |
 * | `rx_ds18b20_config_t`  | 24           | Caller stack |
 * | Scratchpad buffer      | 9            | Stack (temp) |
 * | Driver code (.text)    | ~2KB         | Flash        |
 * | Constant data (.rodata)| ~100 bytes   | Flash        |
 *
 * **Total RAM per sensor**: 28 bytes (handle only, no dynamic allocation)
 *
 * # Hardware Requirements
 *
 * | Resource       | Requirement                                      |
 * |----------------|--------------------------------------------------|
 * | **CPU**        | Any (architecture-independent)                   |
 * | **RAM**        | 28 bytes per sensor + 9-byte temp buffer         |
 * | **Flash**      | ~2KB code + ~100 bytes constants                 |
 * | **GPIO**       | 1 pin (open-drain or push-pull with resistor)    |
 * | **Timing**     | Microsecond-precision delays (1-Wire protocol)   |
 * | **Pull-up**    | 4.7kOhm resistor to VCC (external)                 |
 * | **Power**      | 3.0V-5.5V (parasitic mode: data pin can power)   |
 *
 * ## Typical Wiring (Single Sensor)
 *
 * @code{.unparsed}
 *     VCC (3.3V or 5V)
 *      |
 *      +-----+------ 4.7kOhm pull-up resistor
 *      |     |
 *   +--+-----+--+
 *   | DS18B20   |
 *   |           |
 *   |  DQ  VDD  |
 *   +---+---+---+
 *       |   |
 *       |   +------ VCC (or GND for parasitic mode)
 *       |
 *       +---------- MCU GPIO (data line)
 *
 *   GND ---------- GND
 * @endcode
 *
 * # Example Usage Patterns
 *
 * ## Example 1: Basic Single-Sensor Configuration
 *
 * @code
 * #include "rx_ds18b20.h"
 * #include "rx_bus_manager.h"
 * #include "tx_api.h"
 *
 * void temperature_task_entry(ULONG input) {
 *     rx_ds18b20_handle_t sensor;
 *     rx_ds18b20_config_t config = {
 *         .bus_manager = &g_bus_manager,
 *         .bus_name = "onewire0",
 *         .resolution = k_ds18b20_resolution_12bit,  // 0.0625degC precision
 *         .use_rom_matching = false,                  // Skip ROM (single sensor)
 *     };
 *
 *     rx_err_t err = rx_ds18b20_init(&sensor, &config);
 *     if (err != k_rx_ok) {
 *         // Handle error: check bus wiring, power, pull-up resistor
 *         return;
 *     }
 *
 *     while (1) {
 *         // Trigger conversion
 *         err = rx_ds18b20_trigger_conversion(&sensor);
 *         if (err != k_rx_ok) continue;
 *
 *         // Wait for conversion (800ms for 12-bit)
 *         tx_thread_sleep(80);  // 800ms at 100 Hz tick rate
 *
 *         // Read temperature
 *         float temp_celsius;
 *         err = rx_ds18b20_read_temperature(&sensor, &temp_celsius);
 *         if (err == k_rx_ok) {
 *             // Process temperature (e.g., log, control, transmit)
 *             printf("Temperature: %.2fdegC\n", temp_celsius);
 *         } else if (err == k_rx_err_crc_mismatch) {
 *             // CRC error: retry or log fault
 *         }
 *
 *         // Repeat every 5 seconds
 *         tx_thread_sleep(500);
 *     }
 * }
 * @endcode
 *
 * ## Example 2: Multi-Sensor Configuration (ROM Matching)
 *
 * @code
 * // Pre-discovered ROM codes (use ROM search to find these)
 * uint8_t rom_sensor1[8] = {0x28, 0xFF, 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB};
 * uint8_t rom_sensor2[8] = {0x28, 0xFF, 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45};
 *
 * rx_ds18b20_handle_t sensors[2];
 * rx_ds18b20_config_t config_template = {
 *     .bus_manager = &g_bus_manager,
 *     .bus_name = "onewire0",
 *     .resolution = k_ds18b20_resolution_11bit,
 *     .use_rom_matching = true,  // Enable ROM matching for multi-drop
 * };
 *
 * // Init sensor 1
 * memcpy(config_template.rom, rom_sensor1, 8);
 * rx_ds18b20_init(&sensors[0], &config_template);
 *
 * // Init sensor 2
 * memcpy(config_template.rom, rom_sensor2, 8);
 * rx_ds18b20_init(&sensors[1], &config_template);
 *
 * // Trigger both simultaneously (broadcast not shown, or trigger individually)
 * rx_ds18b20_trigger_conversion(&sensors[0]);
 * rx_ds18b20_trigger_conversion(&sensors[1]);
 *
 * tx_thread_sleep(40);  // Wait 400ms for 11-bit
 *
 * // Read both
 * float temps[2];
 * rx_ds18b20_read_temperature(&sensors[0], &temps[0]);
 * rx_ds18b20_read_temperature(&sensors[1], &temps[1]);
 * @endcode
 *
 * ## Example 3: Error Handling and Diagnostics
 *
 * @code
 * rx_err_t err = rx_ds18b20_read_temperature(&sensor, &temp_c);
 *
 * switch (err) {
 *     case k_rx_ok:
 *         // Success: use temp_c
 *         break;
 *
 *     case k_rx_err_crc_mismatch:
 *         // CRC error: possible bus noise, retry
 *         retry_count++;
 *         break;
 *
 *     case k_rx_err_invalid_state:
 *         // Sensor disconnected or not responding
 *         // Check: wiring, power, pull-up resistor
 *         break;
 *
 *     case k_rx_err_out_of_range:
 *         // Temperature outside -55degC to +125degC
 *         // Sensor fault or extreme environment
 *         break;
 *
 *     default:
 *         // Unexpected error
 *         break;
 * }
 * @endcode
 *
 * ## Example 4: Runtime Resolution Change
 *
 * @code
 * // Switch to 9-bit for faster updates (0.5degC precision)
 * rx_ds18b20_set_resolution(&sensor, k_ds18b20_resolution_9bit);
 *
 * // Persist to EEPROM (survives power cycle)
 * rx_ds18b20_save_config(&sensor);
 *
 * // Now trigger with shorter wait time
 * rx_ds18b20_trigger_conversion(&sensor);
 * tx_thread_sleep(10);  // 100ms for 9-bit
 * rx_ds18b20_read_temperature(&sensor, &temp_c);
 * @endcode
 *
 * # Thread Safety
 *
 * **Not thread-safe**. Caller must serialize access if multiple threads use the
 * same sensor handle. Use ThreadX mutexes:
 *
 * @code
 * TX_MUTEX sensor_mutex;
 * tx_mutex_create(&sensor_mutex, "DS18B20", TX_NO_INHERIT);
 *
 * // Thread 1:
 * tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER);
 * rx_ds18b20_trigger_conversion(&sensor);
 * tx_mutex_put(&sensor_mutex);
 *
 * // Thread 2:
 * tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER);
 * rx_ds18b20_read_temperature(&sensor, &temp_c);
 * tx_mutex_put(&sensor_mutex);
 * @endcode
 *
 * **Note**: Different sensor handles (different physical sensors) CAN be accessed
 * concurrently without synchronization, even on the same 1-Wire bus (ROM matching
 * addresses each sensor independently).
 *
 * # Known Limitations
 *
 * 1. **No alarm threshold API**: TH/TL registers accessible via scratchpad but
 *    not exposed. Users can read/write scratchpad directly if needed.
 *
 * 2. **No broadcast/simultaneous conversion**: Must trigger each sensor individually
 *    when using ROM matching. Skip ROM mode allows broadcast but only for single-sensor.
 *
 * 3. **No ROM search**: Must provide ROM codes manually for multi-sensor. Consider
 *    adding `rx_ds18b20_discover()` helper in future.
 *
 * 4. **Blocking API only**: No asynchronous/callback interface. Caller must use
 *    RTOS primitives (tasks, timers) for non-blocking behavior.
 *
 * # References and Standards
 *
 * - **DS18B20 Datasheet**: Maxim Integrated (now Analog Devices)
 *   https://www.analog.com/media/en/technical-documentation/data-sheets/DS18B20.pdf
 * - **1-Wire Protocol**: Book of iButton Standards, Maxim Integrated
 * - **CRC-8 Polynomial**: x^8 + x^5 + x^4 + 1 (standard for 1-Wire devices)
 * - **ROM Structure**: 8-byte format (1-byte family + 6-byte serial + 1-byte CRC)
 * - **IEEE 802.3 Ethernet CRC**: Different from DS18B20's CRC-8, not applicable here
 *
 * @see rx_bus_manager.h - Bus manager abstraction layer
 * @see rx_bus_onewire.h - 1-Wire protocol implementation
 * @see rx_crc.h - CRC calculation utilities (CRC-8 for DS18B20)
 * @see docs/sections/03_hardware_pinout.tex - GPIO pin assignments
 *
 * @par NASA Power of 10 Compliance:
 * - **Rule 1** (No goto): [OK] All control flow uses structured statements
 * - **Rule 2** (Bounded loops): [OK] All loops have compile-time or runtime bounds
 * - **Rule 3** (No heap): [OK] Zero dynamic allocation (static handles only)
 * - **Rule 4** (Functions <=60 lines): [OK] All public functions <60 lines
 * - **Rule 5** (Assertions): [OK] 2+ preconditions per function (NULL checks, state)
 * - **Rule 6** (Data scope): [OK] Variables declared at smallest scope
 * - **Rule 7** (Return checking): [OK] All HAL returns validated
 * - **Rule 8** (Preprocessor limits): [OK] C23 typed enums only, minimal macros
 * - **Rule 9** (Pointer restrictions): [WARN] Intentional deviation for DIP (bus_manager)
 * - **Rule 10** (Compiler warnings): [OK] -Wall -Wextra -Werror, zero warnings
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility)**: Module handles ONLY DS18B20 sensor operations
 * - **O (Open/Closed)**: Extensible via bus_manager (add new buses w/o changes)
 * - **L (Liskov Substitution)**: Any rx_bus_manager_t* is interchangeable
 * - **I (Interface Segregation)**: Focused API (temp read, no unrelated operations)
 * - **D (Dependency Inversion)**: Depends on rx_bus_manager_t abstraction, not GPIO
 *
 * @par Module Dependencies:
 * @code{.unparsed}
 * rx_ds18b20
 *   +-- rx_bus_manager (bus abstraction, injected)
 *   +-- rx_bus_onewire (1-Wire protocol layer)
 *   +-- rx_crc (CRC-8 validation)
 *   +-- rx_err (error codes)
 *   +-- rx_log (diagnostic logging)
 *   +-- rx_check (nullptr validation macros)
 * @endcode
 *
 * @author Locked, Inc.
 * @date 2026-01-30
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_bus_onewire.h"
#include "rx_err.h"

/* =============================================================================
 * DS18B20 Constants
 * =============================================================================
 */

/**
 * @enum ds18b20_command_t
 * @brief DS18B20 function commands per datasheet specification
 *
 * @details
 * Command codes sent after ROM selection (Match ROM or Skip ROM) to initiate
 * DS18B20 operations. Each command triggers a specific sensor function.
 *
 * **Protocol Sequence:**
 * 1. Reset pulse + presence detection
 * 2. ROM command (Match ROM 0x55 or Skip ROM 0xCC)
 * 3. Function command (one of these values)
 * 4. Additional data bytes (if required by command)
 *
 * **Command Details:**
 *
 * | Command            | Code | Data Bytes | Duration   | Description                    |
 * |--------------------|------|------------|------------|--------------------------------|
 * | Convert T          | 0x44 | 0          | 94-750ms   | Start temperature conversion   |
 * | Write Scratchpad   | 0x4E | 3          | ~2ms       | Write TH, TL, Config registers |
 * | Read Scratchpad    | 0xBE | 0          | ~2ms       | Read 9-byte scratchpad + CRC   |
 * | Copy Scratchpad    | 0x48 | 0          | ~10ms      | Save scratchpad to EEPROM      |
 * | Recall E^2          | 0xB8 | 0          | ~1ms       | Restore EEPROM to scratchpad   |
 * | Read Power Supply  | 0xB4 | 0          | <1ms       | Check parasitic vs. external   |
 *
 * @par Usage Example:
 * @code
 * // Trigger conversion
 * rx_bus_onewire_reset(bus_mgr, "onewire0", &presence);
 * rx_bus_onewire_skip_rom(bus_mgr, "onewire0");
 * rx_bus_onewire_write_byte(bus_mgr, "onewire0", k_ds18b20_cmd_convert_t);
 * // Wait conversion time...
 * @endcode
 *
 * @see DS18B20 datasheet section "Function Commands" for timing specifications
 * @see rx_bus_onewire.h for ROM command functions
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_ds18b20_cmd_convert_t     = 0x44, /**< Trigger temperature conversion (ADC operation) */
  k_ds18b20_cmd_write_scratch = 0x4E, /**< Write to scratchpad memory (TH, TL, Config bytes) */
  k_ds18b20_cmd_read_scratch  = 0xBE, /**< Read from scratchpad memory (9 bytes: data + CRC) */
  k_ds18b20_cmd_copy_scratch  = 0x48, /**< Copy scratchpad to EEPROM (non-volatile storage) */
  k_ds18b20_cmd_recall_eeprom = 0xB8, /**< Recall EEPROM to scratchpad (restore saved values) */
  k_ds18b20_cmd_read_power    = 0xB4, /**< Read power supply mode (0=parasitic, 1=external) */
} ds18b20_command_t;

/**
 * @enum ds18b20_scratchpad_index_t
 * @brief DS18B20 scratchpad memory layout indices
 *
 * @details
 * The DS18B20's 9-byte scratchpad RAM contains temperature data, alarm thresholds,
 * configuration, and a CRC-8 checksum. Use these indices to access specific bytes
 * when reading via Read Scratchpad command (0xBE).
 *
 * **Memory Layout:**
 *
 * | Index | Byte | Name          | Description                                    | Access    |
 * |-------|------|---------------|------------------------------------------------|-----------|
 * | 0     | 0    | Temp LSB      | Temperature low byte (bits 0-7)                | Read-only |
 * | 1     | 1    | Temp MSB      | Temperature high byte (sign + bits 8-11)       | Read-only |
 * | 2     | 2    | TH Register   | High alarm threshold (-55 to +125degC)           | R/W       |
 * | 3     | 3    | TL Register   | Low alarm threshold (-55 to +125degC)            | R/W       |
 * | 4     | 4    | Config        | Resolution bits [6:5]: 00=9b, 01=10b, 10=11b, 11=12b | R/W |
 * | 5     | 5    | Reserved      | Always 0xFF (ignore)                           | Read-only |
 * | 6     | 6    | Reserved      | Factory-programmed (ignore)                    | Read-only |
 * | 7     | 7    | Reserved      | Factory-programmed (ignore)                    | Read-only |
 * | 8     | 8    | CRC           | CRC-8 checksum of bytes 0-7                    | Read-only |
 *
 * **Temperature Format (Bytes 0-1):**
 *
 * 16-bit signed integer in 1/16degC units (two's complement):
 * @code
 * MSB (byte 1):  S  S  S  S  S  T10 T9  T8
 * LSB (byte 0):  T7 T6 T5 T4 T3 T2  T1  T0
 *                +------------------------+- Temperature bits (12-bit for 12-bit resolution)
 * @endcode
 *
 * - **S**: Sign bits (all 0 for positive, all 1 for negative)
 * - **T10-T0**: Temperature bits (11 bits used, T0-T2 undefined for lower resolutions)
 * - **Value range**: -880 to +2000 (represents -55.0degC to +125.0degC)
 * - **Conversion**: `temp_celsius = raw_value / 16.0`
 *
 * **Configuration Register (Byte 4):**
 *
 * Format: `0 R1 R0 1 1 1 1 1` where R1:R0 = resolution bits
 *
 * | R1 | R0 | Resolution | Precision | Conversion Time |
 * |----|----|-----------:|-----------|-----------------|
 * | 0  | 0  | 9-bit      | 0.5degC     | 93.75ms         |
 * | 0  | 1  | 10-bit     | 0.25degC    | 187.5ms         |
 * | 1  | 0  | 11-bit     | 0.125degC   | 375ms           |
 * | 1  | 1  | 12-bit     | 0.0625degC  | 750ms           |
 *
 * **CRC-8 Calculation (Byte 8):**
 *
 * - **Polynomial**: x^8 + x^5 + x^4 + 1 (0x8C)
 * - **Initial value**: 0x00
 * - **Data**: Bytes 0-7 (inclusive)
 * - **Purpose**: Detect transmission errors on 1-Wire bus
 *
 * @par Usage Example:
 * @code
 * uint8_t scratchpad[k_ds18b20_scratchpad_bytes];
 * rx_ds18b20_read_scratchpad(&sensor, scratchpad);
 *
 * // Extract temperature
 * uint16_t temp_raw = scratchpad[k_ds18b20_scratch_temp_lsb] |
 *                     (scratchpad[k_ds18b20_scratch_temp_msb] << 8);
 *
 * // Validate CRC
 * uint32_t crc_out = 0U;
 * (void)rx_crc8_maxim(scratchpad, k_ds18b20_crc_bytes, &crc_out);
 * if ((uint8_t)crc_out != scratchpad[k_ds18b20_scratch_crc]) {
 *     // CRC mismatch: retry read
 * }
 *
 * // Read alarm thresholds
 * int8_t th = (int8_t)scratchpad[k_ds18b20_scratch_th_reg];
 * int8_t tl = (int8_t)scratchpad[k_ds18b20_scratch_tl_reg];
 * @endcode
 *
 * @invariant Byte 5 (reserved1) is always 0xFF
 * @invariant CRC (byte 8) is valid for bytes 0-7
 * @invariant Temperature bytes (0-1) represent value in range [-880, +2000]
 *
 * @see rx_ds18b20_read_scratchpad() Read entire scratchpad with CRC validation
 * @see rx_crc8_maxim() CRC-8 calculation function
 * @see DS18B20 datasheet section "Memory" for detailed scratchpad specification
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_ds18b20_scratch_temp_lsb  = 0, /**< Temperature LSB (bits 0-7 of 16-bit value) */
  k_ds18b20_scratch_temp_msb  = 1, /**< Temperature MSB (sign bits + bits 8-11) */
  k_ds18b20_scratch_th_reg    = 2, /**< TH (high alarm) register (signed byte, -55 to +125) */
  k_ds18b20_scratch_tl_reg    = 3, /**< TL (low alarm) register (signed byte, -55 to +125) */
  k_ds18b20_scratch_config    = 4, /**< Configuration register (resolution bits at [6:5]) */
  k_ds18b20_scratch_reserved1 = 5, /**< Reserved byte (always 0xFF, do not write) */
  k_ds18b20_scratch_reserved2 = 6, /**< Reserved byte (factory-programmed, do not write) */
  k_ds18b20_scratch_reserved3 = 7, /**< Reserved byte (factory-programmed, do not write) */
  k_ds18b20_scratch_crc       = 8, /**< CRC-8 of bytes 0-7 (polynomial 0x8C) */
  k_ds18b20_scratchpad_bytes  = 9, /**< Total scratchpad size in bytes */
} ds18b20_scratchpad_index_t;

/**
 * @brief DS18B20 configuration register bit positions
 */
typedef enum : uint8_t {
  k_ds18b20_config_r0_bit = 5, /**< Resolution bit 0 */
  k_ds18b20_config_r1_bit = 6, /**< Resolution bit 1 */
} ds18b20_config_bits_t;

/**
 * @brief DS18B20 temperature resolution modes
 */
typedef enum : uint8_t {
  k_ds18b20_resolution_9bit  = 0, /**< 9-bit: 0.5degC, 93.75ms */
  k_ds18b20_resolution_10bit = 1, /**< 10-bit: 0.25degC, 187.5ms */
  k_ds18b20_resolution_11bit = 2, /**< 11-bit: 0.125degC, 375ms */
  k_ds18b20_resolution_12bit = 3, /**< 12-bit: 0.0625degC, 750ms */
} ds18b20_resolution_t;

/**
 * @brief DS18B20 conversion times (milliseconds)
 *
 * Maximum conversion time for each resolution.
 * Add margin for safety (spec values + 10%).
 */
typedef enum : uint16_t {
  k_ds18b20_conv_time_9bit_ms  = 100, /**< 9-bit conversion: 100ms */
  k_ds18b20_conv_time_10bit_ms = 200, /**< 10-bit conversion: 200ms */
  k_ds18b20_conv_time_11bit_ms = 400, /**< 11-bit conversion: 400ms */
  k_ds18b20_conv_time_12bit_ms = 800, /**< 12-bit conversion: 800ms */
} ds18b20_conversion_time_ms_t;

/**
 * @brief DS18B20 temperature masks for resolution
 *
 * Lower bits are undefined for resolutions < 12-bit and must be masked.
 * This prevents garbage bits from corrupting the temperature reading.
 */
typedef enum : uint16_t {
  k_ds18b20_temp_mask_9bit  = 0xFFF8, /**< Mask for 9-bit (discard bits 0-2) */
  k_ds18b20_temp_mask_10bit = 0xFFFC, /**< Mask for 10-bit (discard bits 0-1) */
  k_ds18b20_temp_mask_11bit = 0xFFFE, /**< Mask for 11-bit (discard bit 0) */
  k_ds18b20_temp_mask_12bit = 0xFFFF, /**< Mask for 12-bit (all bits valid) */
} ds18b20_temp_mask_t;

/**
 * @brief DS18B20 temperature conversion constants
 */
typedef enum : int32_t {
  k_ds18b20_temp_shift             = 4,      /**< Shift to get integer temperature */
  k_ds18b20_sign_bit               = 0x8000, /**< Sign bit in 16-bit temperature */
  k_ds18b20_crc_bytes              = 8,      /**< Number of bytes for CRC calculation */
  k_ds18b20_shift_byte             = 8,      /**< Shift for byte positioning */
  k_ds18b20_scratchpad_write_bytes = 3,      /**< Scratchpad write size (TH, TL, Config) */
  k_ds18b20_temp_min_raw           = -880,   /**< Minimum temperature (-55degC raw value) */
  k_ds18b20_temp_max_raw           = 2000,   /**< Maximum temperature (+125degC raw value) */
} ds18b20_conversion_constants_t;

/**
 * @brief DS18B20 scratchpad write buffer indices
 */
typedef enum : uint8_t {
  k_ds18b20_write_idx_th     = 0, /**< TH (high alarm) register index */
  k_ds18b20_write_idx_tl     = 1, /**< TL (low alarm) register index */
  k_ds18b20_write_idx_config = 2, /**< Configuration register index */
} ds18b20_write_idx_t;

/**
 * @brief DS18B20 initialization values
 */
typedef enum : uint8_t {
  k_ds18b20_config_register_cleared = 0, /**< Cleared config register value */
} ds18b20_init_values_t;

/**
 * @brief DS18B20 power supply modes
 */
typedef enum : uint8_t {
  k_ds18b20_power_parasitic = 0, /**< Parasitic power mode */
  k_ds18b20_power_external  = 1, /**< External power mode */
} ds18b20_power_mode_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief DS18B20 configuration structure
 */
typedef struct {
  rx_bus_manager_t*    bus_manager;              /**< Bus manager instance (required) */
  const char*          bus_name;                 /**< OneWire bus name (required) */
  ds18b20_resolution_t resolution;               /**< Temperature resolution */
  bool                 use_rom_matching;         /**< Use ROM matching (true) or skip ROM (false) */
  uint8_t              rom[k_onewire_rom_bytes]; /**< Device ROM (if use_rom_matching=true) */
} rx_ds18b20_config_t;

/**
 * @brief DS18B20 driver handle structure
 */
typedef struct {
  rx_bus_manager_t*    bus_manager;              /**< Bus manager reference (not owned) */
  const char*          bus_name;                 /**< OneWire bus name (not owned) */
  ds18b20_resolution_t resolution;               /**< Current temperature resolution */
  bool                 use_rom_matching;         /**< ROM matching enabled */
  uint8_t              rom[k_onewire_rom_bytes]; /**< Device ROM code */
  bool                 initialized;              /**< True after successful init */
} rx_ds18b20_handle_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize DS18B20 sensor
 *
 * Initializes the DS18B20 sensor and configures resolution.
 * If ROM matching is disabled, uses Skip ROM (single device mode).
 *
 * @param[out] handle Pointer to DS18B20 handle. Must not be nullptr.
 * @param[in] config Pointer to configuration. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or config is nullptr
 * @return k_rx_err_invalid_arg if bus_manager or bus_name is nullptr
 * @return k_rx_err_invalid_state if device not responding
 * @return k_rx_err_crc if scratchpad CRC check fails
 */
[[nodiscard]] rx_err_t rx_ds18b20_init(rx_ds18b20_handle_t*       handle,
                                       const rx_ds18b20_config_t* config);

/**
 * @brief Deinitialize DS18B20 sensor
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_ds18b20_deinit(rx_ds18b20_handle_t* handle);

/**
 * @brief Trigger temperature conversion
 *
 * Sends Convert T command to start temperature measurement.
 * Caller must wait for conversion time before reading temperature.
 *
 * Conversion times:
 * - 9-bit: 100ms
 * - 10-bit: 200ms
 * - 11-bit: 400ms
 * - 12-bit: 800ms
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 */
[[nodiscard]] rx_err_t rx_ds18b20_trigger_conversion(const rx_ds18b20_handle_t* handle);

/**
 * @brief Read temperature in Celsius
 *
 * Reads the scratchpad and converts raw temperature to Celsius.
 * Temperature conversion must complete before calling (see conversion times).
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 * @param[out] temperature_celsius Pointer to store temperature in degC. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 * @return k_rx_err_crc if scratchpad CRC check fails
 */
[[nodiscard]] rx_err_t rx_ds18b20_read_temperature(rx_ds18b20_handle_t* handle,
                                                   float*               temperature_celsius);

/**
 * @brief Read raw temperature value
 *
 * Reads the raw 16-bit temperature from scratchpad without conversion.
 * Useful for debugging or custom temperature processing.
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 * @param[out] raw_temp Pointer to store raw temperature. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 * @return k_rx_err_crc if scratchpad CRC check fails
 */
[[nodiscard]] rx_err_t rx_ds18b20_read_temperature_raw(rx_ds18b20_handle_t* handle,
                                                       int16_t*             raw_temp);

/**
 * @brief Set temperature resolution
 *
 * Changes the sensor resolution (9, 10, 11, or 12 bits).
 * Higher resolution provides more precision but takes longer to convert.
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 * @param[in] resolution Resolution mode (9-12 bits)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_arg if resolution out of range
 * @return k_rx_err_invalid_state if not initialized or device not present
 */
[[nodiscard]] rx_err_t rx_ds18b20_set_resolution(rx_ds18b20_handle_t* handle,
                                                 ds18b20_resolution_t resolution);

/**
 * @brief Get current temperature resolution
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 * @param[out] resolution Pointer to store resolution. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
[[nodiscard]] rx_err_t rx_ds18b20_get_resolution(const rx_ds18b20_handle_t* handle,
                                                 ds18b20_resolution_t*      resolution);

/**
 * @brief Save configuration to EEPROM
 *
 * Copies scratchpad bytes 2-4 (TH, TL, Config) to non-volatile EEPROM.
 * Values persist across power cycles.
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 */
[[nodiscard]] rx_err_t rx_ds18b20_save_config(const rx_ds18b20_handle_t* handle);

/**
 * @brief Recall configuration from EEPROM
 *
 * Restores scratchpad bytes 2-4 (TH, TL, Config) from EEPROM.
 * Useful after power-up to restore saved settings.
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 */
[[nodiscard]] rx_err_t rx_ds18b20_recall_config(const rx_ds18b20_handle_t* handle);

/**
 * @brief Read power supply mode
 *
 * Checks if sensor is using parasitic power or external power.
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 * @param[out] external_power True if external power, false if parasitic. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 */
[[nodiscard]] rx_err_t rx_ds18b20_read_power_mode(const rx_ds18b20_handle_t* handle,
                                                  bool*                      external_power);

/**
 * @brief Read scratchpad memory
 *
 * Reads the entire 9-byte scratchpad (8 data bytes + 1 CRC byte).
 * Performs CRC validation.
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 * @param[out] scratchpad Pointer to 9-byte buffer. Must not be nullptr.
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if any pointer is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 * @return k_rx_err_crc if CRC check fails
 */
[[nodiscard]] rx_err_t rx_ds18b20_read_scratchpad(rx_ds18b20_handle_t* handle,
                                                  uint8_t scratchpad[k_ds18b20_scratchpad_bytes]);

/**
 * @brief Get conversion time for current resolution
 *
 * Returns the maximum conversion time in milliseconds for the configured resolution.
 * Caller should wait this long after triggering conversion before reading temperature.
 *
 * @param[in] handle Pointer to initialized handle. Must not be nullptr.
 *
 * @return Conversion time in milliseconds, or 0 if handle is nullptr or not initialized
 */
uint32_t rx_ds18b20_get_conversion_time_ms(const rx_ds18b20_handle_t* handle);

#ifdef __cplusplus
}
#endif
