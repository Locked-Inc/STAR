/* lib/rx_bus/inc/rx_bus_adc.h */

/**
 * @file rx_bus_adc.h
 * @brief ADC (Analog-to-Digital Converter) Bus Abstraction for RX72N
 *
 * @details
 * Provides bus manager integration for Renesas S12ADFa (Successive Approximation
 * ADC Fast) peripheral operations. Wraps the low-level ADC HAL with the bus
 * abstraction pattern for consistent API across all bus types (I2C, SPI, UART,
 * GPIO, 1-Wire, ADC) with mutex-protected access.
 *
 * @par System Architecture
 * @dot
 * digraph adc_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_api {
 *     label="ADC Bus API (This Module)";
 *     style=filled;
 *     color=lightyellow;
 *     init [label="rx_bus_adc_init()"];
 *     read [label="rx_bus_adc_read()"];
 *     readv [label="rx_bus_adc_read_voltage_mv()"];
 *   }
 *
 *   subgraph cluster_bus {
 *     label="Bus Manager";
 *     style=filled;
 *     color=lightblue;
 *     manager [label="rx_bus_manager_t\nMutex protection\nBus lookup"];
 *     config [label="rx_bus_config_t\nUnit, Channel\nResolution, Vref"];
 *   }
 *
 *   subgraph cluster_hal {
 *     label="S12ADFa HAL Layer";
 *     style=filled;
 *     color=lightgreen;
 *     adc [label="adc_init()\nadc_start()\nadc_read()"];
 *     regs [label="S12AD0/1\nRegister Access"];
 *   }
 *
 *   subgraph cluster_hw {
 *     label="ADC Hardware";
 *     style=filled;
 *     color=lightcoral;
 *     current [label="Motor Current\nDRV8243 IPROPI"];
 *     voltage [label="Battery Voltage\nDivider 1/11"];
 *     temp [label="Temperature\nInternal Sensor"];
 *     analog [label="Analog Sensors\n0-3.3V"];
 *   }
 *
 *   init -> manager;
 *   read -> manager;
 *   readv -> manager;
 *   manager -> config;
 *   config -> adc;
 *   adc -> regs;
 *   regs -> current;
 *   regs -> voltage;
 *   regs -> temp;
 *   regs -> analog;
 * }
 * @enddot
 *
 * @par ADC Overview
 * The S12ADFa (Successive Approximation ADC Fast) is a 12-bit ADC peripheral:
 * - **12-bit resolution**: 0-4095 counts (4096 levels)
 * - **Multiple units**: S12AD0 and S12AD1 (28 total channels)
 * - **Sampling time**: 0.5-16.5 µs configurable
 * - **Conversion time**: ~1.5 µs @ 12-bit resolution
 * - **Input range**: 0V to VREFH (typically 3.3V)
 * - **Trigger modes**: Software, hardware, periodic
 * - **Averaging**: 2, 4, 8, 16, or 32 samples
 *
 * @par Conversion Process
 * @verbatim
 * ADC Conversion Sequence (Software Trigger):
 *
 * 1. Sample Phase (ADCS = sampling time):
 *    Input voltage charges internal sample & hold capacitor
 *    Duration: 0.5 - 16.5 µs (configurable)
 *
 * 2. Conversion Phase (12 clock cycles):
 *    Successive approximation algorithm
 *    Binary search from MSB to LSB
 *    Duration: ~1.5 µs @ 25 MHz PCLKD
 *
 * 3. Result Ready:
 *    12-bit result available in ADDRn register
 *    Conversion end interrupt (ADIE) if enabled
 *
 * Total Time = Sampling Time + Conversion Time
 * Example: 3 µs + 1.5 µs = 4.5 µs per channel
 * @endverbatim
 *
 * @par Resolution and Precision
 * | Resolution | Counts | LSB @ 3.3V | LSB @ 5V | Use Case |
 * |------------|--------|------------|----------|----------|
 * | 12-bit | 4096 | 0.805 mV | 1.221 mV | Standard (default) |
 * | 10-bit | 1024 | 3.223 mV | 4.883 mV | Faster sampling |
 * | 8-bit | 256 | 12.89 mV | 19.53 mV | High-speed |
 *
 * @par ADC Formula
 * The ADC conversion follows this equation:
 * @f[
 * ADC_{raw} = \frac{V_{in}}{V_{REF}} \times (2^{bits} - 1)
 * @f]
 *
 * Where:
 * - @f$ V_{in} @f$ = input voltage (0 to VREF)
 * - @f$ V_{REF} @f$ = reference voltage (typically 3.3V or 5V)
 * - @f$ bits @f$ = resolution (8, 10, or 12)
 *
 * Voltage conversion (reverse):
 * @f[
 * V_{in} = \frac{ADC_{raw} \times V_{REF}}{2^{bits} - 1}
 * @f]
 *
 * Example for 12-bit @ 3.3V VREF:
 * @f[
 * V_{mV} = \frac{ADC_{raw} \times 3300}{4095}
 * @f]
 *
 * @par Sampling Time Calculation
 * Sampling time depends on PCLKD and ADCS register:
 * @f[
 * T_{sample} = \frac{ADCS + 1}{PCLKD}
 * @f]
 *
 * Where:
 * - ADCS = sampling state count (0-255)
 * - PCLKD = peripheral clock D (60 MHz on RX72N)
 *
 * Example: ADCS = 74 @ 60 MHz:
 * @f[
 * T_{sample} = \frac{74 + 1}{60 \times 10^6} = 1.25 \mu s
 * @f]
 *
 * @par Performance Characteristics
 * | Metric | Value | Notes |
 * |--------|-------|-------|
 * | Init time | ~50 µs | One-time setup |
 * | Single conversion | 4.5 µs | 3 µs sample + 1.5 µs conversion |
 * | Maximum rate | 222 kSPS | @ minimum sampling time |
 * | Practical rate | 100 kSPS | With adequate settling time |
 * | INL error | ± 2 LSB | Integral non-linearity |
 * | DNL error | ± 1 LSB | Differential non-linearity |
 * | Offset error | ± 10 mV | Typical @ 25°C |
 * | Gain error | ± 1% | Typical |
 *
 * @par Memory Usage
 * | Component | Size | Description |
 * |-----------|------|-------------|
 * | Code (.text) | ~900 bytes | All API functions |
 * | Stack | 48 bytes | Maximum call depth |
 * | No heap | 0 | Static allocation only |
 *
 * @par Hardware Requirements
 * | Requirement | Specification |
 * |-------------|---------------|
 * | VREFH | 3.3V or 5V (reference voltage) |
 * | VREFL | 0V (ground) |
 * | V_IN range | VREFL to VREFH |
 * | Source impedance | < 10 kΩ (for fast settling) |
 * | Bypass cap | 0.1 µF ceramic on VREFH |
 * | Input cap | 10 nF on each analog input |
 *
 * @par Channel Assignments (STAR Project - 144-pin LFQFP)
 * | Unit | Channel | Pin | Signal | Range | Notes |
 * |------|---------|-----|--------|-------|-------|
 * | S12AD0 | AN007 | P47 | Motor 0 Current | 0-3.3V | DRV8243 IPROPI |
 * | S12AD0 | AN006 | P46 | Motor 1 Current | 0-3.3V | DRV8243 IPROPI |
 * | S12AD0 | AN005 | P45 | Motor 2 Current | 0-3.3V | DRV8243 IPROPI |
 * | S12AD0 | AN004 | P44 | Motor 3 Current | 0-3.3V | DRV8243 IPROPI |
 *
 * @par Current Sensing (DRV8243 IPROPI)
 * The DRV8243 motor driver outputs a current proportional voltage on IPROPI:
 * - **Current ratio**: 1000:1 (1mA output per 1A load current)
 * - **Sense resistor**: 4990 Ω (1% tolerance)
 * - **Conversion**: I_motor = (V_IPROPI / 4990 Ω) × 1000
 *
 * Example: V_IPROPI = 1.65V (mid-scale):
 * @f[
 * I_{motor} = \frac{1.65}{4990} \times 1000 = 0.331 A = 331 mA
 * @f]
 *
 * @par Battery Voltage Measurement
 * Battery voltage uses a 1:11 resistor divider to scale down to ADC range:
 * - **Divider ratio**: 1/11 (100kΩ / 10kΩ)
 * - **Input range**: 0-36.3V battery → 0-3.3V ADC
 * - **Conversion**: V_battery = V_ADC × 11
 *
 * @par Error Sources and Mitigation
 * | Error Source | Magnitude | Mitigation |
 * |--------------|-----------|------------|
 * | Quantization | ± 0.5 LSB | Averaging (2-32 samples) |
 * | Noise | ± 2 LSB | Low-pass filter on input |
 * | Offset drift | ± 5 mV/°C | Temperature compensation |
 * | Gain error | ± 1% | Calibration lookup table |
 * | Source impedance | Variable | Buffer amplifier if > 10 kΩ |
 *
 * @par Thread Safety
 * All functions are thread-safe when used with the bus manager:
 * - Bus manager provides mutex protection per ADC operation
 * - Timeout on mutex acquisition prevents deadlock
 * - Safe to call from multiple ThreadX threads
 * - Conversion started after mutex acquired (atomic)
 *
 * @par Module Dependencies
 * - rx_bus_manager.h: Bus abstraction and mutex protection
 * - rx_err.h: Error code definitions
 * - rx_adc.h: Low-level S12ADFa hardware driver
 * - rx_mpc.h: Pin function selection (AN pins)
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: ✓ No goto, setjmp, or recursion
 * - Rule 2: ✓ All loops bounded (conversion timeout)
 * - Rule 3: ✓ No dynamic memory allocation
 * - Rule 4: ✓ Functions under 60 lines
 * - Rule 5: ✓ Input validation via RX_CHECK_NULL_PTR
 * - Rule 6: ✓ Variables at smallest scope
 * - Rule 7: ✓ All return values checked
 * - Rule 8: ✓ Limited preprocessor (typed enums preferred)
 * - Rule 9: ✓ Function pointers only for bus abstraction (DIP)
 * - Rule 10: ✓ Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single Responsibility - ADC analog input operations only
 * - **O**: Open/Closed - Extensible via bus manager config
 * - **L**: Liskov Substitution - Implements rx_bus_interface_t
 * - **I**: Interface Segregation - Minimal 3-function API
 * - **D**: Dependency Inversion - Depends on abstractions (bus manager)
 *
 * @par Usage Example - Motor Current Monitoring
 * @code
 * // Initialize bus manager
 * rx_bus_manager_t manager;
 * rx_bus_manager_init(&manager);
 *
 * // Register ADC bus for motor 0 current sensing
 * rx_bus_config_t current_config = {
 *     .type = k_rx_bus_type_adc,
 *     .adc = {
 *         .unit = 0,          // S12AD0
 *         .channel = 0,       // AN000
 *         .resolution = 12,   // 12-bit
 *         .vref_mv = 3300,    // 3.3V reference
 *     }
 * };
 * rx_bus_register(&manager, "motor0_current", &current_config);
 *
 * // Initialize ADC
 * rx_err_t err = rx_bus_adc_init(&manager, "motor0_current");
 * if (err != k_rx_ok) {
 *     return err;
 * }
 *
 * // Read motor current
 * uint32_t voltage_mv = 0;
 * err = rx_bus_adc_read_voltage_mv(&manager, "motor0_current", &voltage_mv);
 * if (err == k_rx_ok) {
 *     // Convert IPROPI voltage to motor current (mA)
 *     // IPROPI = I_motor / 1000 * 4990 Ω
 *     uint32_t current_ma = (voltage_mv * 1000) / 4990;
 *
 *     if (current_ma > 5000) {  // 5A limit
 *         // Overcurrent - disable motor
 *         motor_emergency_stop();
 *     }
 * }
 * @endcode
 *
 * @par Usage Example - Battery Voltage Monitoring
 * @code
 * // Register ADC for battery voltage (1:11 divider)
 * rx_bus_config_t battery_config = {
 *     .type = k_rx_bus_type_adc,
 *     .adc = {
 *         .unit = 0,
 *         .channel = 4,       // AN004
 *         .resolution = 12,
 *         .vref_mv = 3300,
 *     }
 * };
 * rx_bus_register(&manager, "battery_voltage", &battery_config);
 * rx_bus_adc_init(&manager, "battery_voltage");
 *
 * // Read battery voltage
 * uint32_t adc_voltage_mv = 0;
 * err = rx_bus_adc_read_voltage_mv(&manager, "battery_voltage", &adc_voltage_mv);
 * if (err == k_rx_ok) {
 *     // Scale up by divider ratio (1:11)
 *     uint32_t battery_voltage_mv = adc_voltage_mv * 11;
 *
 *     if (battery_voltage_mv < 14000) {  // 14V low battery
 *         rx_log_warn("POWER", "Low battery: %d mV", battery_voltage_mv);
 *     }
 * }
 * @endcode
 *
 * @par Usage Example - Raw ADC Reading
 * @code
 * // Read raw ADC counts (no voltage conversion)
 * uint16_t raw_value = 0;
 * err = rx_bus_adc_read(&manager, "sensor_input", &raw_value);
 * if (err == k_rx_ok) {
 *     // raw_value is 0-4095 for 12-bit ADC
 *     // Convert to voltage manually
 *     uint32_t voltage_mv = (raw_value * 3300UL) / 4095;
 *
 *     // Or use for lookup table
 *     uint32_t temperature_c = ntc_lookup_table[raw_value];
 * }
 * @endcode
 *
 * @see rx_bus_manager.h Bus manager API for registration and lookup
 * @see rx_bus_types.h Bus type definitions and configurations
 * @see rx_adc.h Low-level S12ADFa hardware driver
 *
 * @version 1.0.0
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ADC Bus Operations
 * =============================================================================
 */

/**
 * @brief Initialize ADC channel through bus manager
 *
 * @details
 * Configures the S12ADFa ADC peripheral for analog voltage measurement.
 * This function must be called before any read operations. The initialization
 * process:
 *
 * 1. Acquires bus manager mutex (thread-safe)
 * 2. Looks up bus configuration by name
 * 3. Validates bus type is ADC
 * 4. Configures MPC for analog input pin function
 * 5. Initializes S12AD unit with resolution and sampling time
 * 6. Enables ADC channel
 * 7. Performs initial conversion (discard for settling)
 * 8. Releases mutex
 *
 * @par Initialization Sequence
 * @dot
 * digraph init_flow {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="rx_bus_adc_init()", shape=ellipse];
 *   mutex [label="Acquire mutex"];
 *   lookup [label="Lookup bus by name"];
 *   validate [label="Validate bus type"];
 *   mpc [label="Configure MPC\nAnalog pin mode"];
 *   adc [label="Initialize S12AD\nResolution, Sampling"];
 *   enable [label="Enable channel"];
 *   settle [label="Dummy conversion\n(discard)"];
 *   release [label="Release mutex"];
 *   done [label="Return", shape=ellipse];
 *
 *   start -> mutex;
 *   mutex -> lookup;
 *   lookup -> validate;
 *   validate -> mpc;
 *   mpc -> adc;
 *   adc -> enable;
 *   enable -> settle;
 *   settle -> release;
 *   release -> done;
 * }
 * @enddot
 *
 * @par ADC Configuration:
 * The initialization configures:
 * - **Resolution**: 12-bit (4096 counts, 0.805 mV/LSB @ 3.3V)
 * - **Sampling time**: 3 µs (adequate for most sensors)
 * - **Conversion time**: 1.5 µs (12 clocks @ 25 MHz)
 * - **Reference**: VREFH pin (typically 3.3V or 5V)
 * - **Trigger**: Software (manual start per conversion)
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized via rx_bus_manager_init()
 *   - Must have ADC bus registered via rx_bus_register()
 * @param[in] bus_name ADC bus name
 *   - Null-terminated string
 *   - Must match name used in rx_bus_register()
 *   - Maximum 31 characters
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, ADC channel initialized and ready
 * @retval k_rx_err_null_ptr NULL pointer in manager or bus_name
 * @retval k_rx_err_not_found Bus name not found in manager
 * @retval k_rx_err_invalid_arg Bus is not ADC type
 * @retval k_rx_err_timeout Mutex acquisition timeout (default 1000ms)
 * @retval k_rx_err_hw_init_failed ADC hardware initialization failed
 *
 * @pre manager must be initialized via rx_bus_manager_init()
 * @pre Bus must be registered via rx_bus_register() with type k_rx_bus_type_adc
 * @pre Analog input pin must not be driven externally above VREFH
 *
 * @post S12AD unit configured and enabled
 * @post Analog pin configured for ADC input
 * @post Bus state set to initialized
 * @post Initial dummy conversion performed (settling time)
 *
 * @note Thread-safe via bus manager mutex
 * @note Call only once per channel; subsequent calls return k_rx_err_invalid_state
 * @note First conversion after init should be discarded (settling time)
 *
 * @warning Do not exceed VREFH on analog input (damage possible)
 * @warning Do not call from interrupt context (mutex wait)
 * @warning Ensure source impedance < 10 kΩ for accurate readings
 *
 * @par Performance:
 * - Execution time: ~50 µs typical (ADC setup + dummy conversion)
 * - Mutex timeout: 1000 ms (configurable via bus manager)
 *
 * @par Example - Current Sensor Initialization:
 * @code
 * rx_bus_config_t current_config = {
 *     .type = k_rx_bus_type_adc,
 *     .adc = {
 *         .unit = 0,
 *         .channel = 0,       // AN000 - motor current
 *         .resolution = 12,
 *         .vref_mv = 3300,
 *     }
 * };
 * rx_bus_register(&manager, "motor_current", &current_config);
 *
 * rx_err_t err = rx_bus_adc_init(&manager, "motor_current");
 * if (err != k_rx_ok) {
 *     rx_log_error("ADC", "Failed to init current sensor");
 *     return err;
 * }
 * @endcode
 *
 * @see rx_bus_manager_init() Initialize bus manager first
 * @see rx_bus_register() Register bus before initialization
 * @see rx_bus_adc_read() Read raw ADC value
 * @see rx_bus_adc_read_voltage_mv() Read voltage in millivolts
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_adc_init(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Read ADC raw value through bus manager
 *
 * @details
 * Performs a single ADC conversion and returns the raw digital value (0-4095
 * for 12-bit). Does not perform voltage scaling - use rx_bus_adc_read_voltage_mv()
 * for automatic voltage conversion.
 *
 * @par Algorithm Steps:
 * 1. Validate all pointers (manager, bus_name, value)
 * 2. Acquire mutex with timeout
 * 3. Lookup bus configuration
 * 4. Validate bus initialized
 * 5. Start ADC conversion (software trigger)
 * 6. Wait for conversion complete (poll ADCSR.ADST bit)
 * 7. Read result from ADDRn register
 * 8. Release mutex
 * 9. Return raw value
 *
 * @par Conversion Timing:
 * @verbatim
 * Timeline for single ADC read:
 *
 * |<--- Sampling --->|<- Conversion ->|
 * |     3.0 µs       |     1.5 µs     |
 * |                                   |
 * Start                           Result Ready
 *
 * Total: 4.5 µs typical @ 12-bit resolution
 * @endverbatim
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name ADC bus name
 *   - Must match registered bus
 * @param[out] value Pointer to store raw ADC value
 *   - Range: 0-4095 (12-bit resolution)
 *   - 0 = 0V input, 4095 = VREF input
 *   - Valid only if k_rx_ok returned
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, value contains raw ADC reading
 * @retval k_rx_err_null_ptr NULL pointer in any parameter
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_timeout Conversion timeout or mutex timeout
 *
 * @pre Bus must be initialized via rx_bus_adc_init()
 * @pre value pointer must be valid
 * @pre Analog input voltage must be within 0 to VREFH
 *
 * @post value contains raw 12-bit ADC result
 * @post ADC ready for next conversion
 *
 * @invariant Bus state remains initialized
 * @invariant value range: 0-4095 for 12-bit ADC
 *
 * @note Thread-safe via bus manager mutex
 * @note Blocking call (waits for conversion ~4.5 µs)
 * @note For voltage conversion, use rx_bus_adc_read_voltage_mv() instead
 *
 * @warning value undefined if return != k_rx_ok
 * @warning Analog input must not exceed VREFH (damage possible)
 *
 * @par Performance:
 * - Execution time: ~10 µs (4.5 µs conversion + overhead)
 * - Conversion timeout: 100 µs (configurable)
 *
 * @par Example - Raw ADC Reading:
 * @code
 * uint16_t raw_adc = 0;
 * rx_err_t err = rx_bus_adc_read(&manager, "sensor_input", &raw_adc);
 * if (err == k_rx_ok) {
 *     // Convert to voltage manually
 *     uint32_t voltage_mv = (raw_adc * 3300UL) / 4095;
 *
 *     // Or use in lookup table
 *     float temperature_c = ntc_thermistor_table[raw_adc];
 * }
 * @endcode
 *
 * @par Example - Multiple Readings for Averaging:
 * @code
 * // Average 16 readings for noise reduction
 * uint32_t sum = 0;
 * for (uint8_t i = 0; i < 16; i++) {
 *     uint16_t reading = 0;
 *     err = rx_bus_adc_read(&manager, "noisy_signal", &reading);
 *     if (err == k_rx_ok) {
 *         sum += reading;
 *     }
 * }
 * uint16_t average = sum / 16;
 * @endcode
 *
 * @see rx_bus_adc_init() Initialize ADC first
 * @see rx_bus_adc_read_voltage_mv() Read with automatic voltage conversion
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_bus_adc_read(rx_bus_manager_t* manager, const char* bus_name, uint16_t* value);

/**
 * @brief Read ADC voltage in millivolts through bus manager
 *
 * @details
 * Performs a single ADC conversion and automatically converts the raw value
 * to millivolts based on the configured VREF. This is the preferred function
 * for most analog voltage measurements.
 *
 * @par Algorithm Steps:
 * 1. Validate all pointers (manager, bus_name, voltage_mv)
 * 2. Acquire mutex with timeout
 * 3. Lookup bus configuration (get VREF)
 * 4. Validate bus initialized
 * 5. Start ADC conversion
 * 6. Wait for conversion complete
 * 7. Read raw ADC value
 * 8. Convert to millivolts: V_mV = (raw × VREF_mV) / 4095
 * 9. Release mutex
 * 10. Return voltage in millivolts
 *
 * @par Voltage Conversion Formula:
 * The conversion uses the configured VREF from bus configuration:
 * @f[
 * V_{mV} = \frac{ADC_{raw} \times V_{REF\_mV}}{2^{12} - 1} = \frac{ADC_{raw} \times V_{REF\_mV}}{4095}
 * @f]
 *
 * Example @ 3.3V VREF:
 * - ADC = 0 → 0 mV
 * - ADC = 2048 → 1651 mV (mid-scale)
 * - ADC = 4095 → 3300 mV (full-scale)
 *
 * @par Precision:
 * | VREF | LSB | Effective Bits | Notes |
 * |------|-----|----------------|-------|
 * | 3.3V | 0.805 mV | ~12 bits | Standard configuration |
 * | 5.0V | 1.221 mV | ~12 bits | Higher voltage sensors |
 *
 * @param[in] manager Bus manager instance
 *   - Must be initialized
 * @param[in] bus_name ADC bus name
 *   - Must match registered bus
 * @param[out] voltage_mv Pointer to store voltage in millivolts
 *   - Range: 0 to VREF_mV (typically 0-3300 mV)
 *   - Resolution: 0.805 mV @ 3.3V VREF
 *   - Valid only if k_rx_ok returned
 *
 * @return rx_err_t Error code indicating result
 * @retval k_rx_ok Success, voltage_mv contains reading in millivolts
 * @retval k_rx_err_null_ptr NULL pointer in any parameter
 * @retval k_rx_err_not_found Bus name not found
 * @retval k_rx_err_invalid_state Bus not initialized
 * @retval k_rx_err_timeout Conversion timeout or mutex timeout
 *
 * @pre Bus must be initialized via rx_bus_adc_init()
 * @pre voltage_mv pointer must be valid
 * @pre Bus config must have valid vref_mv value
 * @pre Analog input voltage must be within 0 to VREFH
 *
 * @post voltage_mv contains input voltage in millivolts
 * @post ADC ready for next conversion
 *
 * @invariant Bus state remains initialized
 * @invariant voltage_mv range: 0 to vref_mv
 *
 * @note Thread-safe via bus manager mutex
 * @note Blocking call (waits for conversion ~4.5 µs)
 * @note Uses VREF from bus configuration (set in rx_bus_register)
 * @note Preferred over rx_bus_adc_read() for direct voltage measurement
 *
 * @warning voltage_mv undefined if return != k_rx_ok
 * @warning Analog input must not exceed VREFH (damage possible)
 * @warning External voltage dividers must be accounted for separately
 *
 * @par Performance:
 * - Execution time: ~12 µs (4.5 µs conversion + calculation + overhead)
 * - Conversion timeout: 100 µs (configurable)
 *
 * @par Example - Motor Current Measurement:
 * @code
 * // Read motor current via DRV8243 IPROPI output
 * uint32_t ipropi_mv = 0;
 * rx_err_t err = rx_bus_adc_read_voltage_mv(&manager, "motor0_current", &ipropi_mv);
 * if (err == k_rx_ok) {
 *     // IPROPI = I_motor / 1000 * 4990 Ω
 *     // I_motor = IPROPI / 4990 * 1000 (in mA)
 *     uint32_t current_ma = (ipropi_mv * 1000) / 4990;
 *
 *     if (current_ma > 5000) {  // 5A overcurrent limit
 *         motor_emergency_stop();
 *         rx_log_error("MOTOR", "Overcurrent: %d mA", current_ma);
 *     }
 * }
 * @endcode
 *
 * @par Example - Battery Voltage with Divider:
 * @code
 * // Read battery voltage through 1:11 resistor divider
 * uint32_t adc_voltage_mv = 0;
 * err = rx_bus_adc_read_voltage_mv(&manager, "battery_voltage", &adc_voltage_mv);
 * if (err == k_rx_ok) {
 *     // Scale up by divider ratio
 *     uint32_t battery_mv = adc_voltage_mv * 11;
 *
 *     if (battery_mv < 14000) {  // 14V low battery warning
 *         rx_log_warn("POWER", "Low battery: %d.%d V",
 *                     battery_mv / 1000, (battery_mv % 1000) / 100);
 *     }
 * }
 * @endcode
 *
 * @par Example - Temperature Sensor (Linear):
 * @code
 * // LM35 temperature sensor: 10 mV/°C, 0°C = 0V
 * uint32_t sensor_mv = 0;
 * err = rx_bus_adc_read_voltage_mv(&manager, "temperature", &sensor_mv);
 * if (err == k_rx_ok) {
 *     // Convert to temperature (10 mV/°C)
 *     int32_t temp_celsius = sensor_mv / 10;
 *
 *     if (temp_celsius > 85) {
 *         // Overtemperature warning
 *         rx_log_warn("TEMP", "High temperature: %d °C", temp_celsius);
 *     }
 * }
 * @endcode
 *
 * @see rx_bus_adc_init() Initialize ADC first
 * @see rx_bus_adc_read() Read raw ADC value (no conversion)
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_bus_adc_read_voltage_mv(rx_bus_manager_t* manager, const char* bus_name, uint32_t* voltage_mv);

#ifdef __cplusplus
}
#endif
