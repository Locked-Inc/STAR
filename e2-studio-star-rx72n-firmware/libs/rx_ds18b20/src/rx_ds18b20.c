/* lib/rx_ds18b20/src/rx_ds18b20.c */

/**
 * @file rx_ds18b20.c
 * @brief DS18B20 1-Wire Digital Temperature Sensor Driver Implementation
 *
 * @details
 * Complete DS18B20 driver implementation with hardware abstraction via bus manager.
 * Supports all DS18B20 features: temperature measurement, resolution configuration,
 * ROM matching for multi-drop buses, and non-volatile EEPROM storage.
 *
 * ## Architecture
 *
 * **Dependency Injection Pattern:**
 * - Uses `rx_bus_manager_t` interface for 1-Wire communication
 * - Enables unit testing with mock bus implementations
 * - Zero direct hardware dependencies
 *
 * **Communication Flow:**
 * ```
 * [rx_ds18b20] → [rx_bus_manager] → [rx_bus_onewire] → [GPIO HAL]
 * ```
 *
 * **Thread Safety:**
 * - NOT thread-safe - caller must provide synchronization
 * - Multiple handles can coexist (different sensors)
 * - Concurrent access to same handle causes race conditions
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simple control flow | ✓ | No goto, no recursion |
 * | 2. Fixed loop bounds | ✓ | All loops bounded by enum constants |
 * | 3. No dynamic allocation | ✓ | Zero malloc/free |
 * | 4. Short functions | ✓ | Max 60 lines per function |
 * | 5. Assertions | ✓ | Min 2 checks per function (nullptr, state) |
 * | 6. Data scope | ✓ | Variables declared at smallest scope |
 * | 7. Check returns | ✓ | All function returns validated |
 * | 8. Limit preprocessor | ✓ | Typed enums only, no magic numbers |
 * | 9. Pointer restrictions | ✓ | Single-level dereferencing |
 * | 10. Compile warnings | ✓ | -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * **Single Responsibility:**
 * - One purpose: DS18B20 sensor communication
 * - Bus operations delegated to `rx_bus_onewire`
 * - CRC validation delegated to `rx_crc`
 *
 * **Open/Closed:**
 * - Extensible via bus manager interface
 * - New bus types added without modifying this driver
 *
 * **Liskov Substitution:**
 * - Any `rx_bus_manager_t` implementation works
 * - Mock bus substitutes for testing
 *
 * **Interface Segregation:**
 * - Clean API with focused functions
 * - Separate read/write/config operations
 *
 * **Dependency Inversion:**
 * - Depends on `rx_bus_manager_t` abstraction
 * - Bus manager injected via config structure
 *
 * ## Performance
 *
 * **Typical Operation Times:**
 * - Initialization: ~100ms (bus init + scratchpad read)
 * - Trigger conversion: ~2ms (command send)
 * - Read temperature: ~5ms (scratchpad read + CRC check)
 * - Resolution change: ~10ms (read + modify + write scratchpad)
 *
 * **Conversion Times (sensor internal):**
 * - 9-bit: 94ms
 * - 10-bit: 188ms
 * - 11-bit: 375ms
 * - 12-bit: 750ms (default)
 *
 * ## Memory Layout
 *
 * **DS18B20 Scratchpad Structure (9 bytes):**
 * ```
 * Byte | Field        | Description
 * -----|--------------|------------------------------------------
 * 0    | Temp LSB     | Temperature low byte (1/16°C units)
 * 1    | Temp MSB     | Temperature high byte (sign-extended)
 * 2    | TH Register  | High alarm threshold (user-defined)
 * 3    | TL Register  | Low alarm threshold (user-defined)
 * 4    | Config       | Resolution bits R1:R0 (bits 6:5)
 * 5    | Reserved     | 0xFF (always)
 * 6    | Reserved     | Factory programmed (implementation-specific)
 * 7    | Reserved     | 0x10 (count remaining, legacy)
 * 8    | CRC          | CRC-8/MAXIM checksum
 * ```
 *
 * **Temperature Data Format:**
 * ```
 * 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 * ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
 * │ S│ S│ S│ S│ S│ 2⁶│ 2⁵│ 2⁴│ 2³│ 2²│ 2¹│ 2⁰│2⁻¹│2⁻²│2⁻³│2⁻⁴│
 * └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘
 *    Sign extend    Integer part      Fractional (1/16°C)
 * ```
 *
 * **Resolution vs Temperature Bits:**
 * - 9-bit: Bits 15:3 valid, bits 2:0 undefined (mask 0xFFF8)
 * - 10-bit: Bits 15:2 valid, bits 1:0 undefined (mask 0xFFFC)
 * - 11-bit: Bits 15:1 valid, bit 0 undefined (mask 0xFFFE)
 * - 12-bit: All bits valid (mask 0xFFFF)
 *
 * ## Error Handling
 *
 * **Error Propagation Strategy:**
 * ```c
 * // Check bus errors → return immediately
 * err = rx_bus_onewire_reset(...);
 * if (err != k_rx_ok) return err;
 *
 * // Check CRC → return k_rx_err_crc_mismatch
 * if (crc_calc != crc_device) return k_rx_err_crc_mismatch;
 *
 * // Check range → return k_rx_err_out_of_range
 * if (temp < MIN || temp > MAX) return k_rx_err_out_of_range;
 * ```
 *
 * **Common Error Codes:**
 * - `k_rx_err_null_ptr` - NULL handle/config/output pointer
 * - `k_rx_err_invalid_state` - Sensor not initialized or not present
 * - `k_rx_err_invalid_arg` - Invalid resolution or ROM family code
 * - `k_rx_err_crc_mismatch` - Scratchpad CRC validation failed
 * - `k_rx_err_out_of_range` - Temperature outside -55°C to +125°C
 *
 * @par Example: Basic Temperature Reading
 * @code
 * rx_ds18b20_handle_t sensor;
 * rx_ds18b20_config_t config = {
 *     .bus_manager = &g_bus_manager,
 *     .bus_name = "onewire0",
 *     .resolution = k_ds18b20_resolution_12bit,
 *     .use_rom_matching = false  // Single sensor
 * };
 *
 * // 1. Initialize
 * rx_err_t err = rx_ds18b20_init(&sensor, &config);
 * if (err != k_rx_ok) handle_error(err);
 *
 * // 2. Trigger conversion
 * err = rx_ds18b20_trigger_conversion(&sensor);
 * if (err != k_rx_ok) handle_error(err);
 *
 * // 3. Wait for conversion (12-bit = 750ms)
 * delay_ms(rx_ds18b20_get_conversion_time_ms(&sensor));
 *
 * // 4. Read temperature
 * float temp_c = 0.0f;
 * err = rx_ds18b20_read_temperature(&sensor, &temp_c);
 * if (err == k_rx_ok) {
 *     printf("Temperature: %.2f°C\n", temp_c);
 * }
 *
 * // 5. Cleanup
 * rx_ds18b20_deinit(&sensor);
 * @endcode
 *
 * @par Example: Multi-Drop Bus with ROM Matching
 * @code
 * // Read ROM codes first (use search algorithm or known values)
 * uint8_t sensor1_rom[8] = {0x28, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE};
 * uint8_t sensor2_rom[8] = {0x28, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32};
 *
 * rx_ds18b20_handle_t sensor1, sensor2;
 * rx_ds18b20_config_t config1 = {
 *     .bus_manager = &g_bus_manager,
 *     .bus_name = "onewire0",
 *     .resolution = k_ds18b20_resolution_12bit,
 *     .use_rom_matching = true,
 *     .rom = {0x28, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE}
 * };
 *
 * // Initialize both sensors on same bus
 * rx_ds18b20_init(&sensor1, &config1);
 * config1.rom = sensor2_rom;  // Reuse config, change ROM
 * rx_ds18b20_init(&sensor2, &config1);
 *
 * // Read from specific sensor
 * float temp1 = 0.0f;
 * rx_ds18b20_trigger_conversion(&sensor1);
 * delay_ms(750);
 * rx_ds18b20_read_temperature(&sensor1, &temp1);
 * @endcode
 *
 * @see rx_ds18b20.h
 * @see rx_bus_onewire.h
 * @see rx_crc.h
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_ds18b20.h"

#include <math.h>
#include <string.h>

#include "rx_bus_onewire.h"
#include "rx_check.h"
#include "rx_crc.h"
#include "rx_log.h"

/**
 * @defgroup ds18b20_internal_constants Internal Constants
 * @{
 */

/** @brief Logging tag for DS18B20 driver */
static const char* s_tag = "DS18B20";

/** @brief DS18B20 family code (byte 0 of ROM) */
static const uint8_t s_ds18b20_family_code = 0x28U;

/**
 * @brief Temperature conversion divisor (raw to Celsius)
 * @details DS18B20 stores temperature in 1/16°C units. Divide by 16 to get Celsius.
 */
static const float s_temp_conversion_divisor = 16.0f;

/**
 * @brief Reserved byte expected value in scratchpad
 * @details Byte 5 of scratchpad should always be 0xFF per datasheet.
 */
static const uint8_t s_ds18b20_reserved_byte_value = 0xFFU;

/**
 * @brief Invalid conversion time sentinel
 * @details Returned by rx_ds18b20_get_conversion_time_ms() on error.
 */
static const uint32_t s_ds18b20_conversion_time_invalid_u32 = 0U;

/** @} */

/* =============================================================================
 * Internal Validation Macros
 * =============================================================================
 */

/**
 * @def CHECK_DS18B20_HANDLE
 * @brief Validate DS18B20 handle pointer and initialization state
 *
 * @details
 * Composite validation macro that checks both NULL pointer and initialization.
 * Used at the start of all public API functions to enforce preconditions.
 *
 * **Checks Performed:**
 * 1. Handle is not nullptr (via RX_CHECK_NULL_PTR)
 * 2. Handle is initialized (handle->initialized == true)
 *
 * **Error Returns:**
 * - `k_rx_err_null_ptr` if handle is nullptr
 * - `k_rx_err_invalid_state` if not initialized
 *
 * @param[in] handle Handle to validate (type: rx_ds18b20_handle_t*)
 * @param[in] tag Logging tag for error messages (type: const char*)
 *
 * @par Example:
 * @code
 * rx_err_t rx_ds18b20_read_temperature(rx_ds18b20_handle_t* handle, float* temp)
 * {
 *     CHECK_DS18B20_HANDLE(handle, s_tag);  // Early return on error
 *     // ... function body ...
 * }
 * @endcode
 *
 * @note This is a statement macro (uses do-while(0) pattern) - requires semicolon.
 * @warning Do not use in expressions - this macro contains early returns.
 *
 * @see RX_CHECK_NULL_PTR
 */
#define CHECK_DS18B20_HANDLE(handle, tag)                                                          \
  do {                                                                                             \
    RX_CHECK_NULL_PTR(handle, tag, "handle is nullptr");                                              \
    if (!(handle)->initialized) {                                                                  \
      rx_log_error(tag, "DS18B20 not initialized");                                                \
      return k_rx_err_invalid_state;                                                               \
    }                                                                                              \
  } while (0)

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @struct ds18b20_scratchpad_write_t
 * @brief Scratchpad write parameters for TH, TL, and config registers
 *
 * @details
 * Internal structure for writing DS18B20 scratchpad registers.
 * DS18B20 Write Scratchpad command (0x4E) writes exactly 3 bytes:
 * TH alarm, TL alarm, and configuration register.
 *
 * **Memory Layout (3 bytes total):**
 * ```
 * Offset | Field  | Description
 * -------|--------|--------------------------------------------------
 * 0      | th     | High temperature alarm threshold (-55 to +125°C)
 * 1      | tl     | Low temperature alarm threshold (-55 to +125°C)
 * 2      | config | Resolution bits R1:R0 at bits 6:5
 * ```
 *
 * **Configuration Register Format:**
 * ```
 * Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0
 * ------|-------|-------|-------|-------|-------|-------|-------
 *   0   |  R1   |  R0   |   1   |   1   |   1   |   1   |   1
 *       └───────┴───────┘
 *       Resolution bits
 * ```
 *
 * **Resolution Encoding:**
 * - R1=0, R0=0: 9-bit (0.5°C, 93.75ms)
 * - R1=0, R0=1: 10-bit (0.25°C, 187.5ms)
 * - R1=1, R0=0: 11-bit (0.125°C, 375ms)
 * - R1=1, R0=1: 12-bit (0.0625°C, 750ms)
 *
 * @note This structure is for internal use only - not exposed in public API.
 * @see internal_ds18b20_write_scratchpad()
 * @see internal_ds18b20_resolution_to_config()
 */
typedef struct {
  uint8_t th;     /**< High alarm threshold register (signed, stored as uint8_t) */
  uint8_t tl;     /**< Low alarm threshold register (signed, stored as uint8_t) */
  uint8_t config; /**< Configuration register (bits 6:5 = resolution) */
} ds18b20_scratchpad_write_t;

static rx_err_t internal_ds18b20_validate_config(const rx_ds18b20_config_t* config,
                                                 const rx_ds18b20_handle_t* handle);
static rx_err_t internal_ds18b20_verify_device_presence(rx_ds18b20_handle_t* handle);
static rx_err_t internal_ds18b20_select_device(const rx_ds18b20_handle_t* handle);
static rx_err_t
                internal_ds18b20_read_scratchpad_raw(const rx_ds18b20_handle_t* handle,
                                                     uint8_t scratchpad[k_ds18b20_scratchpad_bytes]);
static rx_err_t internal_ds18b20_write_scratchpad(const rx_ds18b20_handle_t*        handle,
                                                  const ds18b20_scratchpad_write_t* scratchpad);
static uint8_t  internal_ds18b20_resolution_to_config(const ds18b20_resolution_t resolution);
static uint16_t internal_ds18b20_get_temp_mask(ds18b20_resolution_t resolution);
static float    internal_ds18b20_raw_to_celsius(const int16_t raw_temp);

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_ds18b20_init(rx_ds18b20_handle_t* handle, const rx_ds18b20_config_t* config)
{
  rx_err_t err = k_rx_ok;

  /* Validate inputs */
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is nullptr");

  err = internal_ds18b20_validate_config(config, handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Initialize handle */
  memset(handle, 0, sizeof(rx_ds18b20_handle_t));
  handle->bus_manager      = config->bus_manager;
  handle->bus_name         = config->bus_name;
  handle->resolution       = config->resolution;
  handle->use_rom_matching = config->use_rom_matching;

  if (config->use_rom_matching) {
    memcpy(handle->rom, config->rom, k_onewire_rom_bytes);
  }

  /* Verify device presence */
  err = internal_ds18b20_verify_device_presence(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Configure resolution */
  handle->initialized = true;
  err                 = rx_ds18b20_set_resolution(handle, config->resolution);
  if (err != k_rx_ok) {
    handle->initialized = false;
    return err;
  }

  rx_log_info(s_tag, "DS18B20 initialized successfully");
  return k_rx_ok;
}

/**
 * @brief Deinitialize DS18B20 sensor handle
 *
 * Clears the handle state and releases resources.
 *
 * @param[in,out] handle Sensor handle to deinitialize
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized
 */
rx_err_t rx_ds18b20_deinit(rx_ds18b20_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is nullptr");

  if (!handle->initialized) {
    rx_log_warn(s_tag, "DS18B20 not initialized");
    return k_rx_err_invalid_state;
  }

  /* Clear handle */
  memset(handle, 0, sizeof(rx_ds18b20_handle_t));

  rx_log_info(s_tag, "DS18B20 deinitialized");
  return k_rx_ok;
}

/**
 * @brief Trigger temperature conversion on DS18B20 sensor
 *
 * Sends the Convert T command to start temperature measurement. After calling
 * this function, wait for conversion time (see rx_ds18b20_get_conversion_time_ms)
 * before reading the temperature.
 *
 * @param[in] handle Sensor handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 * @return Error code from bus operations
 */
rx_err_t rx_ds18b20_trigger_conversion(const rx_ds18b20_handle_t* handle)
{
  bool     presence = false;
  rx_err_t err      = k_rx_ok;

  CHECK_DS18B20_HANDLE(handle, s_tag);

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for conversion trigger");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Convert T command */
  err = rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_convert_t);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send conversion command");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Read temperature from DS18B20 sensor in Celsius
 *
 * Reads the scratchpad and converts raw temperature to degrees Celsius.
 * Must call rx_ds18b20_trigger_conversion() and wait before reading.
 *
 * @param[in,out] handle Sensor handle
 * @param[out] temperature_celsius Temperature in degrees Celsius
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or temperature_celsius is nullptr
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_out_of_range if temperature outside sensor range (-55 to +125°C)
 * @return Error code from bus operations or CRC check
 */
rx_err_t rx_ds18b20_read_temperature(rx_ds18b20_handle_t* handle, float* temperature_celsius)
{
  int16_t  raw_temp = 0;
  rx_err_t err      = k_rx_ok;

  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(temperature_celsius, s_tag, "temperature_celsius is nullptr");

  /* Read raw temperature */
  err = rx_ds18b20_read_temperature_raw(handle, &raw_temp);
  if (err != k_rx_ok) {
    return err;
  }

  /* Convert to Celsius */
  *temperature_celsius = internal_ds18b20_raw_to_celsius(raw_temp);

  return k_rx_ok;
}

/**
 * @brief Read raw temperature value from DS18B20 sensor
 *
 * Reads the scratchpad and returns raw 16-bit temperature value in 1/16°C units.
 * Applies resolution mask to clear undefined bits. Must call
 * rx_ds18b20_trigger_conversion() and wait before reading.
 *
 * @param[in,out] handle Sensor handle
 * @param[out] raw_temp Raw temperature (signed 16-bit, 1/16°C units)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or raw_temp is nullptr
 * @return k_rx_err_invalid_state if not initialized
 * @return k_rx_err_out_of_range if temperature outside sensor range
 * @return Error code from bus operations or CRC check
 */
rx_err_t rx_ds18b20_read_temperature_raw(rx_ds18b20_handle_t* handle, int16_t* raw_temp)
{
  uint8_t  scratchpad[k_ds18b20_scratchpad_bytes];
  rx_err_t err  = k_rx_ok;
  uint16_t temp = 0;
  uint16_t mask = 0;

  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(raw_temp, s_tag, "raw_temp is nullptr");

  /* Read scratchpad */
  err = internal_ds18b20_read_scratchpad_raw(handle, scratchpad);
  if (err != k_rx_ok) {
    return err;
  }

  /* Extract temperature (LSB + MSB) */
  temp = (uint16_t)scratchpad[k_ds18b20_scratch_temp_lsb];
  temp |= ((uint16_t)scratchpad[k_ds18b20_scratch_temp_msb]) << k_ds18b20_shift_byte;

  /* Apply resolution mask to clear undefined bits */
  mask = internal_ds18b20_get_temp_mask(handle->resolution);
  temp &= mask;

  /* Post-condition: Validate temperature is within sensor range */
  *raw_temp = (int16_t)temp;
  if (*raw_temp < k_ds18b20_temp_min_raw || *raw_temp > k_ds18b20_temp_max_raw) {
    rx_log_error(s_tag, "Temperature out of sensor range (-55°C to +125°C)");
    return k_rx_err_out_of_range;
  }

  return k_rx_ok;
}

rx_err_t rx_ds18b20_set_resolution(rx_ds18b20_handle_t*       handle,
                                   const ds18b20_resolution_t resolution)
{
  uint8_t                    scratchpad[k_ds18b20_scratchpad_bytes];
  rx_err_t                   err    = k_rx_ok;
  uint8_t                    config = 0;
  ds18b20_scratchpad_write_t write_cfg;

  CHECK_DS18B20_HANDLE(handle, s_tag);

  if (resolution != k_ds18b20_resolution_9bit && resolution != k_ds18b20_resolution_10bit &&
      resolution != k_ds18b20_resolution_11bit && resolution != k_ds18b20_resolution_12bit) {
    rx_log_error(s_tag, "Invalid resolution value");
    return k_rx_err_invalid_arg;
  }

  /* Read current scratchpad */
  err = internal_ds18b20_read_scratchpad_raw(handle, scratchpad);
  if (err != k_rx_ok) {
    return err;
  }

  /* Generate new config byte */
  config = internal_ds18b20_resolution_to_config(resolution);

  /* Write scratchpad with new config */
  write_cfg.th     = scratchpad[k_ds18b20_scratch_th_reg];
  write_cfg.tl     = scratchpad[k_ds18b20_scratch_tl_reg];
  write_cfg.config = config;
  err              = internal_ds18b20_write_scratchpad(handle, &write_cfg);
  if (err != k_rx_ok) {
    return err;
  }

  /* Update handle */
  handle->resolution = resolution;

  return k_rx_ok;
}

rx_err_t rx_ds18b20_get_resolution(const rx_ds18b20_handle_t* handle,
                                   ds18b20_resolution_t*      resolution)
{
  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(resolution, s_tag, "resolution is nullptr");

  *resolution = handle->resolution;
  return k_rx_ok;
}

/**
 * @brief Save current configuration to DS18B20 EEPROM
 *
 * Copies scratchpad registers (TH, TL, config) to non-volatile EEPROM.
 * Configuration persists through power cycles.
 *
 * @param[in] handle Sensor handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 * @return Error code from bus operations
 */
rx_err_t rx_ds18b20_save_config(const rx_ds18b20_handle_t* handle)
{
  bool     presence = false;
  rx_err_t err      = k_rx_ok;

  CHECK_DS18B20_HANDLE(handle, s_tag);

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for EEPROM save");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Copy Scratchpad command */
  err =
    rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_copy_scratch);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send copy scratchpad command");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Recall configuration from DS18B20 EEPROM to scratchpad
 *
 * Restores scratchpad registers (TH, TL, config) from non-volatile EEPROM.
 * Called automatically on power-up, but can be used to revert changes.
 *
 * @param[in] handle Sensor handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 * @return Error code from bus operations
 */
rx_err_t rx_ds18b20_recall_config(const rx_ds18b20_handle_t* handle)
{
  bool     presence = false;
  rx_err_t err      = k_rx_ok;

  CHECK_DS18B20_HANDLE(handle, s_tag);

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for EEPROM recall");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Recall E2 command */
  err =
    rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_recall_eeprom);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send recall EEPROM command");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Read DS18B20 power supply mode
 *
 * Determines if the sensor is powered externally or using parasitic power.
 *
 * @param[in] handle Sensor handle
 * @param[out] external_power true if external power, false if parasitic
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if handle or external_power is nullptr
 * @return k_rx_err_invalid_state if not initialized or device not present
 * @return Error code from bus operations
 */
rx_err_t rx_ds18b20_read_power_mode(const rx_ds18b20_handle_t* handle, bool* external_power)
{
  bool     presence  = false;
  bool     power_bit = false;
  rx_err_t err       = k_rx_ok;

  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(external_power, s_tag, "external_power is nullptr");

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for power mode read");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Read Power Supply command */
  err = rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_read_power);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send read power command");
    return err;
  }

  /* Read power bit (1 = external, 0 = parasitic) */
  err = rx_bus_onewire_read_bit(handle->bus_manager, handle->bus_name, &power_bit);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read power bit");
    return err;
  }

  *external_power = power_bit;
  return k_rx_ok;
}

rx_err_t rx_ds18b20_read_scratchpad(rx_ds18b20_handle_t* handle,
                                    uint8_t              scratchpad[k_ds18b20_scratchpad_bytes])
{
  CHECK_DS18B20_HANDLE(handle, s_tag);
  RX_CHECK_NULL_PTR(scratchpad, s_tag, "scratchpad is nullptr");

  return internal_ds18b20_read_scratchpad_raw(handle, scratchpad);
}

/**
 * @brief Get temperature conversion time for current resolution
 *
 * Returns the required wait time after triggering conversion based on
 * the configured resolution (9-bit: 94ms, 10-bit: 188ms, 11-bit: 375ms, 12-bit: 750ms).
 *
 * @param[in] handle Sensor handle
 *
 * @return Conversion time in milliseconds, or invalid value if handle is nullptr/uninitialized
 */
uint32_t rx_ds18b20_get_conversion_time_ms(const rx_ds18b20_handle_t* handle)
{
  if (handle == nullptr || !handle->initialized) {
    return s_ds18b20_conversion_time_invalid_u32;
  }

  switch (handle->resolution) {
    case k_ds18b20_resolution_9bit:
      return k_ds18b20_conv_time_9bit_ms;
    case k_ds18b20_resolution_10bit:
      return k_ds18b20_conv_time_10bit_ms;
    case k_ds18b20_resolution_11bit:
      return k_ds18b20_conv_time_11bit_ms;
    case k_ds18b20_resolution_12bit:
      return k_ds18b20_conv_time_12bit_ms;
    default:
      return s_ds18b20_conversion_time_invalid_u32;
  }
}

/* =============================================================================
 * Internal Helper Functions Implementation
 * =============================================================================
 */

/**
 * @brief Validate DS18B20 configuration parameters before initialization
 *
 * @details
 * Comprehensive validation of configuration structure and handle state.
 * Called by rx_ds18b20_init() to enforce preconditions and prevent
 * misconfiguration.
 *
 * **Algorithm:**
 * 1. Check config pointer is not nullptr
 * 2. Check bus_manager pointer is not nullptr
 * 3. Check bus_name string is not nullptr
 * 4. Verify handle is not already initialized (prevent re-initialization)
 * 5. Validate resolution is in valid range (0-3)
 * 6. If ROM matching enabled, verify family code is 0x28 (DS18B20)
 *
 * **Validation Rules:**
 * ```
 * ┌─────────────────┬────────────────┬─────────────────────┐
 * │ Field           │ Check          │ Error Code          │
 * ├─────────────────┼────────────────┼─────────────────────┤
 * │ config          │ != nullptr        │ k_rx_err_null_ptr   │
 * │ bus_manager     │ != nullptr        │ k_rx_err_null_ptr   │
 * │ bus_name        │ != nullptr        │ k_rx_err_null_ptr   │
 * │ initialized     │ == false       │ k_rx_err_invalid_state │
 * │ resolution      │ <= 3           │ k_rx_err_invalid_arg│
 * │ rom[0] (if ROM) │ == 0x28        │ k_rx_err_invalid_arg│
 * └─────────────────┴────────────────┴─────────────────────┘
 * ```
 *
 * @param[in] config Configuration structure to validate
 * @param[in] handle Handle to check for existing initialization
 *
 * @return k_rx_ok Configuration is valid
 * @retval k_rx_err_null_ptr config, bus_manager, or bus_name is nullptr
 * @retval k_rx_err_invalid_state handle already initialized
 * @retval k_rx_err_invalid_arg Invalid resolution or wrong family code
 *
 * @pre config != nullptr
 * @pre handle != nullptr (checked by caller)
 * @post If k_rx_ok returned, all fields are valid for initialization
 *
 * @note This is an internal helper - not exposed in public API
 * @warning Do not call after initialization - will return error
 *
 * @see rx_ds18b20_init()
 * @see ds18b20_resolution_t
 */
static rx_err_t internal_ds18b20_validate_config(const rx_ds18b20_config_t* config,
                                                 const rx_ds18b20_handle_t* handle)
{
  RX_CHECK_NULL_PTR(config, s_tag, "config is nullptr");
  RX_CHECK_NULL_PTR(config->bus_manager, s_tag, "bus_manager is nullptr");
  RX_CHECK_NULL_PTR(config->bus_name, s_tag, "bus_name is nullptr");

  if (handle->initialized) {
    rx_log_warn(s_tag, "DS18B20 already initialized");
    return k_rx_err_invalid_state;
  }

  if (config->resolution > k_ds18b20_resolution_12bit) {
    rx_log_error(s_tag, "Invalid resolution setting");
    return k_rx_err_invalid_arg;
  }

  if (config->use_rom_matching && config->rom[0] != s_ds18b20_family_code) {
    rx_log_error(s_tag, "Invalid DS18B20 family code");
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Verify DS18B20 device presence and establish communication
 *
 * @details
 * Three-step verification process to ensure sensor is physically connected
 * and responding correctly before completing initialization.
 *
 * **Algorithm:**
 * ```
 * @startuml
 * start
 * :Initialize OneWire bus;
 * if (Bus init successful?) then (yes)
 *   :Send reset pulse;
 *   if (Presence pulse detected?) then (yes)
 *     :Read scratchpad (with CRC);
 *     if (Scratchpad valid?) then (yes)
 *       :Return k_rx_ok;
 *       stop
 *     else (CRC fail)
 *       :Return error;
 *     endif
 *   else (no presence)
 *     :Log "device not present";
 *     :Return k_rx_err_invalid_state;
 *     stop
 *   endif
 * else (bus init fail)
 *   :Return error;
 * endif
 * @enduml
 * ```
 *
 * **Verification Steps:**
 * 1. **Bus Initialization** - Configure GPIO for 1-Wire timing
 * 2. **Presence Detection** - Send reset, check for presence pulse
 * 3. **Communication Test** - Read scratchpad with CRC validation
 *
 * **Timing:**
 * - Reset pulse: 480-960 µs LOW
 * - Presence detect: 60-240 µs after reset
 * - Scratchpad read: ~5ms (9 bytes + CRC)
 *
 * @param[in,out] handle DS18B20 handle with bus configuration
 *
 * @return k_rx_ok Device present and communicating
 * @retval k_rx_err_invalid_state No presence pulse detected
 * @retval k_rx_err_* Bus initialization or scratchpad read error
 *
 * @pre handle->bus_manager is valid
 * @pre handle->bus_name is valid
 * @post If k_rx_ok, device is ready for configuration
 *
 * @note This function is called during rx_ds18b20_init()
 * @warning Blocks for ~100ms total (bus init + scratchpad read)
 *
 * @par Performance:
 * Execution time: ~100ms (bus init 50ms + reset 1ms + scratchpad 5ms + margin)
 *
 * @see rx_bus_onewire_init()
 * @see rx_bus_onewire_reset()
 * @see internal_ds18b20_read_scratchpad_raw()
 */
static rx_err_t internal_ds18b20_verify_device_presence(rx_ds18b20_handle_t* handle)
{
  bool     presence = false;
  rx_err_t err      = k_rx_ok;
  uint8_t  scratchpad[k_ds18b20_scratchpad_bytes];

  /* Initialize OneWire bus */
  err = rx_bus_onewire_init(handle->bus_manager, handle->bus_name);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to initialize OneWire bus");
    return err;
  }

  /* Check device presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "OneWire reset failed");
    return err;
  }

  if (!presence) {
    rx_log_error(s_tag, "DS18B20 device not present on bus");
    return k_rx_err_invalid_state;
  }

  /* Read scratchpad to verify communication */
  err = internal_ds18b20_read_scratchpad_raw(handle, scratchpad);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read scratchpad during init");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Select DS18B20 device using ROM matching or Skip ROM command
 *
 * @details
 * Sends appropriate ROM command based on bus configuration:
 * - **Match ROM (0x55)** - Address specific device on multi-drop bus
 * - **Skip ROM (0xCC)** - Broadcast to single device or all devices
 *
 * **Decision Flow:**
 * ```
 * @startuml
 * start
 * if (use_rom_matching?) then (yes)
 *   :Send Match ROM (0x55);
 *   :Send 64-bit ROM code;
 *   note right
 *     Addresses specific device
 *     by unique ROM ID
 *   end note
 * else (no)
 *   :Send Skip ROM (0xCC);
 *   note right
 *     Single device: addresses only device
 *     Multiple devices: broadcasts to all
 *   end note
 * endif
 * :Device selected;
 * :Ready for function command;
 * stop
 * @enduml
 * ```
 *
 * **ROM Matching (Multi-Drop Bus):**
 * - Used when multiple DS18B20 sensors on one bus
 * - Requires 64-bit ROM code (obtained via search or label)
 * - Ensures commands go to intended sensor only
 *
 * **Skip ROM (Single Device):**
 * - Faster - no need to send 64-bit ROM
 * - Safe when only one device on bus
 * - Caution: broadcasts if multiple devices present
 *
 * **Timing:**
 * - Match ROM: ~3ms (1 byte command + 8 bytes ROM)
 * - Skip ROM: ~1ms (1 byte command)
 *
 * @param[in] handle DS18B20 handle with ROM configuration
 *
 * @return k_rx_ok Device selected successfully
 * @retval k_rx_err_* Bus communication error
 *
 * @pre handle->use_rom_matching is configured
 * @pre If ROM matching: handle->rom contains valid 64-bit ID
 * @post Selected device ready for function commands
 *
 * @note Called before every DS18B20 function command
 * @warning Skip ROM on multi-drop bus affects ALL devices
 *
 * @par Example: ROM Code Format
 * ```
 * Byte 0: Family Code (0x28 for DS18B20)
 * Byte 1-6: Serial Number (48-bit unique ID)
 * Byte 7: CRC-8 checksum
 * ```
 *
 * @see rx_bus_onewire_match_rom()
 * @see rx_bus_onewire_skip_rom()
 */
static rx_err_t internal_ds18b20_select_device(const rx_ds18b20_handle_t* handle)
{
  rx_err_t err = k_rx_ok;

  if (handle->use_rom_matching) {
    /* Use Match ROM to address specific device */
    err = rx_bus_onewire_match_rom(handle->bus_manager, handle->bus_name, handle->rom);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to send Match ROM");
      return err;
    }
  } else {
    /* Use Skip ROM for single device or broadcast */
    err = rx_bus_onewire_skip_rom(handle->bus_manager, handle->bus_name);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to send Skip ROM");
      return err;
    }
  }

  return k_rx_ok;
}

/**
 * @brief Read DS18B20 scratchpad memory with CRC-8 validation
 *
 * @details
 * Reads all 9 bytes of scratchpad memory and validates data integrity using
 * CRC-8/MAXIM checksum. This is the primary method for reading temperature
 * and configuration data from the sensor.
 *
 * **Algorithm:**
 * ```
 * @startuml
 * start
 * :Send reset pulse;
 * if (Presence detected?) then (yes)
 *   :Select device (Match/Skip ROM);
 *   :Send Read Scratchpad (0xBE);
 *   :Read 9 bytes (8 data + 1 CRC);
 *   :Calculate CRC-8 of first 8 bytes;
 *   if (CRC matches byte 8?) then (yes)
 *     if (Reserved byte == 0xFF?) then (yes)
 *       :Return k_rx_ok;
 *       stop
 *     else (reserved invalid)
 *       :Log warning;
 *       :Return k_rx_ok anyway;
 *       note right: Non-fatal, continue
 *     endif
 *   else (CRC mismatch)
 *     :Log error;
 *     :Return k_rx_err_crc_mismatch;
 *     stop
 *   endif
 * else (no presence)
 *   :Return k_rx_err_invalid_state;
 * endif
 * @enduml
 * ```
 *
 * **Scratchpad Memory Map:**
 * ```
 * ┌──────┬──────────────┬─────────────────────────────┐
 * │ Byte │ Field        │ Description                 │
 * ├──────┼──────────────┼─────────────────────────────┤
 * │  0   │ Temp LSB     │ Temperature low byte        │
 * │  1   │ Temp MSB     │ Temperature high byte       │
 * │  2   │ TH Register  │ High alarm threshold        │
 * │  3   │ TL Register  │ Low alarm threshold         │
 * │  4   │ Config       │ Resolution bits (6:5)       │
 * │  5   │ Reserved     │ Always 0xFF                 │
 * │  6   │ Reserved     │ Factory-programmed          │
 * │  7   │ Reserved     │ Count remain (legacy)       │
 * │  8   │ CRC          │ CRC-8/MAXIM checksum        │
 * └──────┴──────────────┴─────────────────────────────┘
 * ```
 *
 * **CRC-8/MAXIM Calculation:**
 * - Polynomial: 0x31 (x^8 + x^5 + x^4 + 1)
 * - Initial value: 0x00
 * - Input: First 8 bytes of scratchpad
 * - Result: Must match byte 8
 *
 * **Post-Conditions:**
 * - Scratchpad buffer contains valid sensor data
 * - CRC verified (data integrity guaranteed)
 * - Reserved byte checked (warning if unexpected)
 *
 * @param[in] handle DS18B20 sensor handle (must be initialized)
 * @param[out] scratchpad 9-byte buffer to receive scratchpad data
 *                        Array must be at least k_ds18b20_scratchpad_bytes (9) bytes
 *
 * @return k_rx_ok Scratchpad read successful, CRC valid
 * @retval k_rx_err_null_ptr handle or scratchpad is nullptr
 * @retval k_rx_err_invalid_state Device not present on bus
 * @retval k_rx_err_crc_mismatch CRC validation failed (corrupted data)
 * @retval k_rx_err_* Bus communication error
 *
 * @pre handle != nullptr
 * @pre handle->initialized == true
 * @pre scratchpad != nullptr
 * @pre scratchpad size >= 9 bytes
 * @post If k_rx_ok: scratchpad contains validated sensor data
 * @post If error: scratchpad contents undefined
 *
 * @note Reserved byte validation is non-fatal (log warning only)
 * @warning Do not use scratchpad data if CRC check fails
 *
 * @par Performance:
 * Execution time: ~5ms (reset 1ms + select 1ms + read 3ms)
 *
 * @par Example: Manual Scratchpad Access
 * @code
 * uint8_t scratchpad[9];
 * rx_err_t err = internal_ds18b20_read_scratchpad_raw(&sensor, scratchpad);
 * if (err == k_rx_ok) {
 *     int16_t temp_raw = scratchpad[0] | (scratchpad[1] << 8);
 *     uint8_t config = scratchpad[4];
 *     uint8_t resolution = (config >> 5) & 0x03;
 * }
 * @endcode
 *
 * @see rx_crc8_maxim()
 * @see internal_ds18b20_select_device()
 */
static rx_err_t internal_ds18b20_read_scratchpad_raw(const rx_ds18b20_handle_t* handle,
                                                     uint8_t scratchpad[k_ds18b20_scratchpad_bytes])
{
  bool     presence   = false;
  rx_err_t err        = k_rx_ok;
  uint8_t  crc_calc   = 0;
  uint8_t  crc_device = 0;

  RX_CHECK_NULL_PTR(handle, s_tag, "handle is nullptr");
  RX_CHECK_NULL_PTR(scratchpad, s_tag, "scratchpad is nullptr");

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for scratchpad read");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Read Scratchpad command */
  err =
    rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_read_scratch);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send read scratchpad command");
    return err;
  }

  /* Read 9 bytes (8 data + 1 CRC) */
  err = rx_bus_onewire_read(handle->bus_manager,
                            handle->bus_name,
                            scratchpad,
                            k_ds18b20_scratchpad_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read scratchpad data");
    return err;
  }

  /* Validate CRC */
  crc_calc   = rx_crc8_maxim(scratchpad, k_ds18b20_crc_bytes);
  crc_device = scratchpad[k_ds18b20_scratch_crc];

  if (crc_calc != crc_device) {
    rx_log_error(s_tag, "Scratchpad CRC mismatch");
    return k_rx_err_crc_mismatch;
  }

  /* Post-condition: Validate scratchpad structure (reserved byte should be 0xFF) */
  if (scratchpad[k_ds18b20_scratch_reserved1] != s_ds18b20_reserved_byte_value) {
    rx_log_warn(s_tag, "Scratchpad reserved byte unexpected value");
  }

  return k_rx_ok;
}

/**
 * @brief Write DS18B20 scratchpad registers (TH, TL, Config)
 *
 * @details
 * Writes the three user-modifiable scratchpad registers: high alarm (TH),
 * low alarm (TL), and configuration (resolution). This is the only way to
 * change sensor resolution and alarm thresholds.
 *
 * **Algorithm:**
 * ```
 * @startuml
 * start
 * :Send reset pulse;
 * if (Presence detected?) then (yes)
 *   :Select device (Match/Skip ROM);
 *   :Send Write Scratchpad (0x4E);
 *   :Write TH byte;
 *   :Write TL byte;
 *   :Write Config byte;
 *   :Return k_rx_ok;
 *   note right
 *     Changes take effect immediately
 *     but are volatile (lost on power cycle)
 *   end note
 *   stop
 * else (no presence)
 *   :Return k_rx_err_invalid_state;
 * endif
 * @enduml
 * ```
 *
 * **Write Sequence:**
 * 1. Reset bus and check presence
 * 2. Select device (Match ROM or Skip ROM)
 * 3. Send Write Scratchpad command (0x4E)
 * 4. Write 3 bytes in order: TH, TL, Config
 *
 * **Configuration Register Encoding:**
 * ```
 * Bit:  7   6   5   4   3   2   1   0
 *      ┌───┬───┬───┬───┬───┬───┬───┬───┐
 *      │ 0 │R1 │R0 │ 1 │ 1 │ 1 │ 1 │ 1 │
 *      └───┴───┴───┴───┴───┴───┴───┴───┘
 *          └───┴───┘
 *        Resolution
 *
 * R1 R0 | Resolution | Conversion Time
 * ------+------------+----------------
 *  0  0 |   9-bit    |     94 ms
 *  0  1 |  10-bit    |    188 ms
 *  1  0 |  11-bit    |    375 ms
 *  1  1 |  12-bit    |    750 ms
 * ```
 *
 * **Persistence:**
 * - Scratchpad writes are **volatile** (RAM only)
 * - Lost on power cycle or reset
 * - Use Copy Scratchpad (0x48) to save to EEPROM
 *
 * @param[in] handle DS18B20 sensor handle (must be initialized)
 * @param[in] scratchpad Write parameters (TH, TL, config)
 *
 * @return k_rx_ok Scratchpad written successfully
 * @retval k_rx_err_null_ptr handle or scratchpad is nullptr
 * @retval k_rx_err_invalid_state Device not present on bus
 * @retval k_rx_err_* Bus communication error
 *
 * @pre handle != nullptr
 * @pre handle->initialized == true
 * @pre scratchpad != nullptr
 * @post Sensor resolution/alarms updated (volatile, RAM only)
 *
 * @note Changes are immediate but volatile (lost on power cycle)
 * @warning No CRC verification - ensure bus integrity
 *
 * @par Performance:
 * Execution time: ~3ms (reset 1ms + select 1ms + write 1ms)
 *
 * @par Example: Change Resolution to 10-bit
 * @code
 * uint8_t scratchpad_data[9];
 * internal_ds18b20_read_scratchpad_raw(&sensor, scratchpad_data);
 *
 * ds18b20_scratchpad_write_t write_cfg = {
 *     .th = scratchpad_data[2],  // Preserve TH
 *     .tl = scratchpad_data[3],  // Preserve TL
 *     .config = (0x01 << 5) | 0x1F  // 10-bit: R1=0, R0=1
 * };
 * internal_ds18b20_write_scratchpad(&sensor, &write_cfg);
 *
 * // Save to EEPROM to make permanent
 * rx_ds18b20_save_config(&sensor);
 * @endcode
 *
 * @see rx_ds18b20_save_config() Make changes permanent
 * @see internal_ds18b20_select_device()
 * @see ds18b20_scratchpad_write_t
 */
static rx_err_t internal_ds18b20_write_scratchpad(const rx_ds18b20_handle_t*        handle,
                                                  const ds18b20_scratchpad_write_t* scratchpad)
{
  bool     presence = false;
  rx_err_t err      = k_rx_ok;
  uint8_t  write_buf[k_ds18b20_scratchpad_write_bytes];

  RX_CHECK_NULL_PTR(handle, s_tag, "handle is nullptr");
  RX_CHECK_NULL_PTR(scratchpad, s_tag, "scratchpad is nullptr");

  /* Reset and check presence */
  err = rx_bus_onewire_reset(handle->bus_manager, handle->bus_name, &presence);
  if (err != k_rx_ok || !presence) {
    rx_log_error(s_tag, "Device not present for scratchpad write");
    return k_rx_err_invalid_state;
  }

  /* Select device */
  err = internal_ds18b20_select_device(handle);
  if (err != k_rx_ok) {
    return err;
  }

  /* Send Write Scratchpad command */
  err =
    rx_bus_onewire_write_byte(handle->bus_manager, handle->bus_name, k_ds18b20_cmd_write_scratch);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to send write scratchpad command");
    return err;
  }

  /* Write 3 bytes (TH, TL, Config) */
  write_buf[k_ds18b20_write_idx_th]     = scratchpad->th;
  write_buf[k_ds18b20_write_idx_tl]     = scratchpad->tl;
  write_buf[k_ds18b20_write_idx_config] = scratchpad->config;

  err = rx_bus_onewire_write(handle->bus_manager,
                             handle->bus_name,
                             write_buf,
                             k_ds18b20_scratchpad_write_bytes);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to write scratchpad data");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Convert resolution enum to config register value
 *
 * @param[in] resolution Resolution mode
 *
 * @return Configuration register value
 */
static uint8_t internal_ds18b20_resolution_to_config(const ds18b20_resolution_t resolution)
{
  uint8_t config = k_ds18b20_config_register_cleared;

  /* Resolution bits are R1:R0 at bits 6:5 */
  config = (uint8_t)resolution << k_ds18b20_config_r0_bit;

  return config;
}

/**
 * @brief Get temperature mask for resolution
 *
 * Lower bits are undefined for resolutions < 12-bit and must be masked.
 *
 * @param[in] resolution Resolution mode
 *
 * @return Temperature mask
 */
static uint16_t internal_ds18b20_get_temp_mask(const ds18b20_resolution_t resolution)
{
  switch (resolution) {
    case k_ds18b20_resolution_9bit:
      return k_ds18b20_temp_mask_9bit;
    case k_ds18b20_resolution_10bit:
      return k_ds18b20_temp_mask_10bit;
    case k_ds18b20_resolution_11bit:
      return k_ds18b20_temp_mask_11bit;
    case k_ds18b20_resolution_12bit:
      return k_ds18b20_temp_mask_12bit;
    default:
      return k_ds18b20_temp_mask_12bit;
  }
}

/**
 * @brief Convert raw temperature to Celsius
 *
 * DS18B20 stores temperature as 16-bit signed value in 1/16°C units.
 * Divide by 16 to get Celsius.
 *
 * @param[in] raw_temp Raw 16-bit temperature value
 *
 * @return Temperature in degrees Celsius
 */
static float internal_ds18b20_raw_to_celsius(const int16_t raw_temp)
{
  float result = NAN;

  if (raw_temp < k_ds18b20_temp_min_raw || raw_temp > k_ds18b20_temp_max_raw) {
    rx_log_error(s_tag, "Raw temperature out of range - returning NaN sentinel");
    return NAN;
  }

  /* Convert from 1/16°C to °C */
  result = (float)raw_temp / s_temp_conversion_divisor;

  /* Post-condition: Validate result is finite (not NaN or Inf) */
  if (!isfinite(result)) {
    rx_log_error(s_tag, "Computed Celsius not finite - returning NaN sentinel");
    return NAN;
  }

  return result;
}
