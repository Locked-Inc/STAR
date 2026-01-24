/* lib/rx_bq4050/src/rx_bq4050.c */

/**
 * @file rx_bq4050.c
 * @brief BQ4050 Battery Fuel Gauge Driver Implementation
 *
 * Implementation of BQ4050 fuel gauge driver using SMBus protocol.
 * The BQ4050 implements the Smart Battery System (SBS) 1.1 specification
 * with Texas Instruments manufacturer-specific extensions.
 *
 * SBS Register Map (Standard Commands):
 * - 0x00: ManufacturerAccess (word, manufacturer-specific commands)
 * - 0x08: Temperature (word, 0.1K units)
 * - 0x09: Voltage (word, mV)
 * - 0x0A: Current (word, mA, signed)
 * - 0x0B: AverageCurrent (word, mA, signed)
 * - 0x0D: RelativeStateOfCharge (word, %)
 * - 0x0E: AbsoluteStateOfCharge (word, %)
 * - 0x0F: RemainingCapacity (word, mAh)
 * - 0x10: FullChargeCapacity (word, mAh)
 * - 0x11: RunTimeToEmpty (word, minutes)
 * - 0x13: AverageTimeToFull (word, minutes)
 * - 0x17: CycleCount (word, cycles)
 * - 0x18: DesignCapacity (word, mAh)
 * - 0x3C: CellVoltage4 (word, mV, BQ4050-specific)
 * - 0x3D: CellVoltage3 (word, mV, BQ4050-specific)
 * - 0x3E: CellVoltage2 (word, mV, BQ4050-specific)
 * - 0x3F: CellVoltage1 (word, mV, BQ4050-specific)
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_bq4050.h"

#include <stdio.h>

#include "rx_bq4050_constants.h"
#include "rx_bus_smbus.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BQ4050";

typedef enum : uint8_t {
  s_bq4050_min_cells     = 1,
  s_bq4050_smbus_address = 11U,
} bq4050_init_constants_t;

typedef enum : uint16_t {
  k_bq4050_smbus_addr_hex_width       = 2,  /**< SMBus address width in hex digits */
  k_bq4050_log_msg_size               = 80, /**< Buffer size for formatted log messages */
  k_bq4050_snprintf_success_threshold = 0,  /**< Minimum snprintf return value for success */
  k_bq4050_snprintf_init              = 0,  /**< snprintf result initialization value */
  k_bq4050_loop_init                  = 0,  /**< Loop counter initialization value */
} bq4050_log_constants_t;

/**
 * @brief Temperature conversion constants
 *
 * SBS temperature format: 0.1 Kelvin units (uint16_t)
 * Conversion: temp_0_1c = (temp_0.1k - 2731), then temp_c = temp_0_1c / 10
 * Input range: 0K to 65535 (0.1K units)
 * Intermediate range: -2731 to 62804 (0.1°C units after offset subtraction)
 * Output range: -273 to 6280 (whole degrees Celsius)
 */
typedef enum : int32_t {
  k_temp_kelvin_offset     = 2731,   /**< Offset: 0.1K to 0.1°C (273.15K × 10) */
  k_temp_decimal_scale     = 10,     /**< Scale: 0.1 units to whole units */
  k_temp_min_valid_0_1k    = 0,      /**< Minimum temperature in 0.1K (absolute zero) */
  k_temp_max_valid_0_1k    = 65535,  /**< Maximum temperature in 0.1K (uint16_t max) */
  k_temp_min_0_1c          = -2731,  /**< Min intermediate (0 - 2731): −273.1 in 0.1°C units */
  k_temp_max_0_1c          = 62804,  /**< Max intermediate (65535 - 2731): 6280.4 in 0.1°C units */
  k_temp_celsius_min_int16 = -32768, /**< int16_t minimum */
  k_temp_celsius_max_int16 = 32767,  /**< int16_t maximum */
} bq4050_temp_constants_t;

/**
 * @brief State of Charge (SOC) limits
 *
 * SBS specification allows SOC > 100% in some conditions (e.g., overcharge).
 * We clamp to valid 0-100% range for application use.
 */
typedef enum : uint8_t {
  k_soc_min_percent = 0,   /**< Minimum SOC percentage */
  k_soc_max_percent = 100, /**< Maximum SOC percentage */
} bq4050_soc_limits_t;

/**
 * @brief Cell voltage array indices
 *
 * Named indices for accessing cell voltage register map array.
 */
typedef enum : uint8_t {
  k_cell_idx_1 = 0, /**< Cell 1 index */
  k_cell_idx_2 = 1, /**< Cell 2 index */
  k_cell_idx_3 = 2, /**< Cell 3 index */
  k_cell_idx_4 = 3, /**< Cell 4 index */
} bq4050_cell_index_t;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Convert temperature from 0.1K units to degrees Celsius (NASA Rule 5: Validation)
 *
 * SBS temperature format: 16-bit unsigned integer in 0.1 Kelvin units
 * Conversion: temp_c = (temp_0.1k - 2731) / 10 degrees Celsius
 *
 * Validates both intermediate and final ranges to ensure safe conversion.
 * Pre-condition: Verify input temp_0_1k is within valid bounds (0-65535 0.1K)
 * Post-condition: Verify output fits in int16_t range after division
 *
 * @param[in] temp_0_1k Temperature in 0.1K units (uint16_t range)
 * @param[out] temp_celsius_out Pointer to store converted temperature in degrees Celsius
 *
 * @return k_rx_ok if conversion succeeds
 * @return k_rx_err_null_ptr if temp_celsius_out is NULL
 * @return k_rx_err_out_of_range if intermediate or final result exceeds int16_t bounds
 */
static rx_err_t internal_convert_temperature(const uint16_t temp_0_1k, int16_t* temp_celsius_out)
{
  int32_t temp_0_1c;
  int32_t temp_celsius_unchecked;

  /* Pre-condition: Validate output pointer (Rule 5: Check all parameters) */
  RX_CHECK_NULL_PTR(temp_celsius_out, s_tag, "temp_celsius_out pointer is NULL");

  /* Note: Input range check omitted - temp_0_1k is uint16_t which inherently has range [0, 65535],
   * matching [k_temp_min_valid_0_1k, k_temp_max_valid_0_1k]. Explicit checks would trigger
   * -Wtype-limits warnings since comparisons are always true/false for this type. */

  /* Perform intermediate conversion: 0.1K to 0.1°C (Rule 5: Track intermediate values) */
  temp_0_1c = (int32_t)temp_0_1k - k_temp_kelvin_offset;

  /* Post-condition: Verify intermediate result is within valid conversion range (Rule 5) */
  if (temp_0_1c < k_temp_min_0_1c || temp_0_1c > k_temp_max_0_1c) {
    rx_log_error(s_tag, "Temperature intermediate value exceeds valid conversion range");
    return k_rx_err_out_of_range;
  }

  /* Convert from 0.1°C to whole degrees Celsius */
  temp_celsius_unchecked = temp_0_1c / k_temp_decimal_scale;

  /* Post-condition: Verify final result fits in int16_t (Rule 5: Bounds check before assignment) */
  if (temp_celsius_unchecked < k_temp_celsius_min_int16 ||
      temp_celsius_unchecked > k_temp_celsius_max_int16) {
    rx_log_error(s_tag, "Temperature final result exceeds int16_t range");
    return k_rx_err_out_of_range;
  }

  /* Safe to assign: both intermediate and final checks passed */
  *temp_celsius_out = (int16_t)temp_celsius_unchecked;

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize BQ4050 fuel gauge
 *
 * See rx_bq4050.h for full documentation.
 */
rx_err_t
rx_bq4050_init(rx_bus_manager_t* manager, const char* bus_name, const rx_bq4050_config_t* config)
{
  rx_err_t err;
  uint16_t voltage_mv;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  (void)config; /* Configuration stored in BQ4050 data flash, minimal init needed */

  rx_log_info(s_tag, "Initializing BQ4050 fuel gauge");

  /* Initialize SMBus */
  err = rx_bus_smbus_init(manager, bus_name);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "SMBus initialization failed");
    return err;
  }

  /* Verify communication by reading voltage */
  err = rx_bq4050_read_voltage(manager, bus_name, &voltage_mv);
  if (err != k_rx_ok) {
    rx_log_error_hex(s_tag,
                     "Failed to communicate with BQ4050 at address",
                     s_bq4050_smbus_address,
                     k_bq4050_smbus_addr_hex_width);
    return err;
  }

  rx_log_info_hex(s_tag,
                  "BQ4050 initialized successfully at address",
                  s_bq4050_smbus_address,
                  k_bq4050_smbus_addr_hex_width);

  return k_rx_ok;
}

/**
 * @brief Read battery pack voltage
 *
 * See rx_bq4050.h for full documentation.
 */
rx_err_t
rx_bq4050_read_voltage(rx_bus_manager_t* manager, const char* bus_name, uint16_t* voltage_mv)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(voltage_mv, s_tag, "voltage_mv pointer is NULL");

  return rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_voltage, voltage_mv);
}

/**
 * @brief Read individual cell voltages
 *
 * See rx_bq4050.h for full documentation.
 */
rx_err_t rx_bq4050_read_cell_voltages(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint16_t*         cell_voltages,
                                      const uint8_t     num_cells)
{
  char                 log_msg[k_bq4050_log_msg_size];
  int                  snprintf_result                    = k_bq4050_snprintf_init;
  uint8_t              i                                  = k_bq4050_loop_init;
  rx_err_t             err                                = k_rx_ok;
  static const uint8_t s_cell_reg_map[k_bq4050_max_cells] = {
    [k_cell_idx_1] = k_sbs_cell_voltage_1, /* Cell 1 at 0x3F */
    [k_cell_idx_2] = k_sbs_cell_voltage_2, /* Cell 2 at 0x3E */
    [k_cell_idx_3] = k_sbs_cell_voltage_3, /* Cell 3 at 0x3D */
    [k_cell_idx_4] = k_sbs_cell_voltage_4, /* Cell 4 at 0x3C */
  };

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(cell_voltages, s_tag, "cell_voltages pointer is NULL");

  if ((num_cells < s_bq4050_min_cells) || (num_cells > k_bq4050_max_cells)) {
    snprintf_result = snprintf(log_msg,
                               sizeof(log_msg),
                               "num_cells out of range: %u (allowed %u..%u)",
                               (unsigned)num_cells,
                               (unsigned)s_bq4050_min_cells,
                               (unsigned)k_bq4050_max_cells);
    if (snprintf_result > k_bq4050_snprintf_success_threshold &&
        (uint32_t)snprintf_result < sizeof(log_msg)) {
      rx_log_error(s_tag, log_msg);
    }
    return k_rx_err_invalid_arg;
  }

  for (i = k_cell_idx_1; i < k_bq4050_max_cells; i++) {
    if (i >= num_cells) {
      break;
    }
    err = rx_bus_smbus_read_word_data(manager, bus_name, s_cell_reg_map[i], &cell_voltages[i]);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to read cell voltage");
      return err;
    }
  }

  return k_rx_ok;
}

/**
 * @brief Read battery current
 *
 * See rx_bq4050.h for full documentation.
 */
rx_err_t
rx_bq4050_read_current(rx_bus_manager_t* manager, const char* bus_name, int16_t* current_ma)
{
  uint16_t raw_current;
  rx_err_t err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(current_ma, s_tag, "current_ma pointer is NULL");

  /* Read as unsigned, interpret as signed (SBS current is signed 16-bit) */
  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_current, &raw_current);

  if (err == k_rx_ok) {
    *current_ma = (int16_t)raw_current;
  }

  return err;
}

/**
 * @brief Read average battery current
 *
 * See rx_bq4050.h for full documentation.
 */
rx_err_t rx_bq4050_read_average_current(rx_bus_manager_t* manager,
                                        const char*       bus_name,
                                        int16_t*          avg_current_ma)
{
  uint16_t raw_current;
  rx_err_t err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(avg_current_ma, s_tag, "avg_current_ma pointer is NULL");

  /* Read as unsigned, interpret as signed */
  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_average_current, &raw_current);

  if (err == k_rx_ok) {
    *avg_current_ma = (int16_t)raw_current;
  }

  return err;
}

/**
 * @brief Read relative state of charge
 *
 * See rx_bq4050.h for full documentation.
 */
rx_err_t
rx_bq4050_read_relative_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc_percent)
{
  uint16_t soc_word;
  rx_err_t err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(soc_percent, s_tag, "soc_percent pointer is NULL");

  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_relative_state_of_charge, &soc_word);

  if (err == k_rx_ok) {
    /* Clamp to 0-100 range (SBS allows > 100% in some conditions) */
    if (soc_word > k_soc_max_percent) {
      *soc_percent = k_soc_max_percent;
    } else {
      *soc_percent = (uint8_t)soc_word;
    }
  }

  return err;
}

/**
 * @brief Read absolute state of charge
 *
 * See rx_bq4050.h for full documentation.
 */
rx_err_t
rx_bq4050_read_absolute_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc_percent)
{
  uint16_t soc_word;
  rx_err_t err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(soc_percent, s_tag, "soc_percent pointer is NULL");

  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_absolute_state_of_charge, &soc_word);

  if (err == k_rx_ok) {
    /* Clamp to 0-100 range */
    if (soc_word > k_soc_max_percent) {
      *soc_percent = k_soc_max_percent;
    } else {
      *soc_percent = (uint8_t)soc_word;
    }
  }

  return err;
}

/**
 * @brief Read battery temperature
 *
 * See rx_bq4050.h for full documentation.
 */
rx_err_t
rx_bq4050_read_temperature(rx_bus_manager_t* manager, const char* bus_name, int16_t* temperature_c)
{
  rx_err_t err;
  uint16_t temp_0_1k;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(temperature_c, s_tag, "temperature_c pointer is NULL");

  /* Read temperature from BQ4050 in 0.1K units */
  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_temperature, &temp_0_1k);
  if (err != k_rx_ok) {
    return err;
  }

  /* Convert temperature with validation (NASA Rule 5: Check result of conversion) */
  err = internal_convert_temperature(temp_0_1k, temperature_c);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Temperature conversion validation failed");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Read battery capacity information
 *
 * See rx_bq4050.h for full documentation.
 */
rx_err_t rx_bq4050_read_capacity(rx_bus_manager_t* manager,
                                 const char*       bus_name,
                                 uint16_t*         remaining_mah,
                                 uint16_t*         full_mah)
{
  rx_err_t err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(remaining_mah, s_tag, "remaining_mah pointer is NULL");
  RX_CHECK_NULL_PTR(full_mah, s_tag, "full_mah pointer is NULL");

  /* Read remaining capacity */
  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_remaining_capacity, remaining_mah);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read remaining capacity");
    return err;
  }

  /* Read full charge capacity */
  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_full_charge_capacity, full_mah);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read full charge capacity");
    return err;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Read electrical status fields (voltage, current, cell voltages)
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus name
 * @param[out] status Status structure to populate
 * @param[in] num_cells Number of cells (s_bq4050_min_cells..k_bq4050_max_cells)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager, bus_name, or status is NULL
 * @return k_rx_err_invalid_arg if num_cells is out of range
 * @return Other rx_err_t values propagated from SMBus reads
 */
static rx_err_t internal_read_electrical_status(rx_bus_manager_t*   manager,
                                                const char*         bus_name,
                                                rx_bq4050_status_t* status,
                                                const uint8_t       num_cells)
{
  rx_err_t err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(status, s_tag, "status pointer is NULL");
  if ((num_cells < s_bq4050_min_cells) || (num_cells > k_bq4050_max_cells)) {
    rx_log_error(s_tag, "num_cells parameter out of range");
    return k_rx_err_invalid_arg;
  }

  err = rx_bq4050_read_voltage(manager, bus_name, &status->voltage_mv);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read voltage");
    return err;
  }

  err = rx_bq4050_read_cell_voltages(manager, bus_name, status->cell_voltages_mv, num_cells);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read cell voltages");
    return err;
  }

  err = rx_bq4050_read_current(manager, bus_name, &status->current_ma);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read current");
    return err;
  }

  err = rx_bq4050_read_average_current(manager, bus_name, &status->average_current_ma);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read average current");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Read SOC and temperature status fields
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus name
 * @param[out] status Status structure to populate
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager, bus_name, or status is NULL
 * @return Other rx_err_t values propagated from SMBus reads
 */
static rx_err_t internal_read_soc_status(rx_bus_manager_t*   manager,
                                         const char*         bus_name,
                                         rx_bq4050_status_t* status)
{
  rx_err_t err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(status, s_tag, "status pointer is NULL");

  err = rx_bq4050_read_relative_soc(manager, bus_name, &status->relative_soc);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read relative SOC");
    return err;
  }

  err = rx_bq4050_read_absolute_soc(manager, bus_name, &status->absolute_soc);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read absolute SOC");
    return err;
  }

  err = rx_bq4050_read_temperature(manager, bus_name, &status->temperature_c);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read temperature");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Read capacity-related status fields
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus name
 * @param[out] status Status structure to populate
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager, bus_name, or status is NULL
 * @return Other rx_err_t values propagated from SMBus reads
 */
static rx_err_t internal_read_capacity_status(rx_bus_manager_t*   manager,
                                              const char*         bus_name,
                                              rx_bq4050_status_t* status)
{
  rx_err_t err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(status, s_tag, "status pointer is NULL");

  err = rx_bq4050_read_capacity(manager,
                                bus_name,
                                &status->remaining_capacity_mah,
                                &status->full_capacity_mah);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read capacity");
    return err;
  }

  err = rx_bus_smbus_read_word_data(manager,
                                    bus_name,
                                    k_sbs_design_capacity,
                                    &status->design_capacity_mah);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read design capacity");
    return err;
  }

  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_cycle_count, &status->cycle_count);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read cycle count");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Read timing-related status fields
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus name
 * @param[out] status Status structure to populate
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager, bus_name, or status is NULL
 * @return Other rx_err_t values propagated from SMBus reads
 */
static rx_err_t internal_read_timing_status(rx_bus_manager_t*   manager,
                                            const char*         bus_name,
                                            rx_bq4050_status_t* status)
{
  rx_err_t err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(status, s_tag, "status pointer is NULL");

  err = rx_bus_smbus_read_word_data(manager,
                                    bus_name,
                                    k_sbs_run_time_to_empty,
                                    &status->time_to_empty_min);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read time to empty");
    return err;
  }

  err = rx_bus_smbus_read_word_data(manager,
                                    bus_name,
                                    k_sbs_average_time_to_full,
                                    &status->time_to_full_min);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read time to full");
    return err;
  }

  return k_rx_ok;
}

/**
 * @brief Read battery status flag fields
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus name
 * @param[out] status Status structure to populate
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_ptr if manager, bus_name, or status is NULL
 * @return Other rx_err_t values propagated from SMBus reads
 */
static rx_err_t internal_read_status_flags(rx_bus_manager_t*   manager,
                                           const char*         bus_name,
                                           rx_bq4050_status_t* status)
{
  uint16_t status_flags;
  rx_err_t err;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(status, s_tag, "status pointer is NULL");

  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_battery_status, &status_flags);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read battery status");
    return err;
  }

  status->is_charging = !(status_flags & k_bq4050_status_discharging);
  status->is_fully_charged =
    (status_flags & k_bq4050_status_fully_charged) != k_bq4050_status_flag_clear;
  status->is_fully_discharged =
    (status_flags & k_bq4050_status_fully_discharged) != k_bq4050_status_flag_clear;
  status->is_low_capacity =
    (status_flags & k_bq4050_status_remaining_capacity_alarm) != k_bq4050_status_flag_clear;

  return k_rx_ok;
}

/**
 * @brief Read complete battery status
 *
 * See rx_bq4050.h for full documentation.
 */
rx_err_t rx_bq4050_read_status(rx_bus_manager_t*   manager,
                               const char*         bus_name,
                               rx_bq4050_status_t* status,
                               const uint8_t       num_cells)
{
  char     log_msg[k_bq4050_log_msg_size];
  int      snprintf_result = k_bq4050_snprintf_init;
  rx_err_t err             = k_rx_ok;

  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(status, s_tag, "status pointer is NULL");

  if ((num_cells < s_bq4050_min_cells) || (num_cells > k_bq4050_max_cells)) {
    snprintf_result = snprintf(log_msg,
                               sizeof(log_msg),
                               "num_cells out of range: %u (allowed %u..%u)",
                               (unsigned)num_cells,
                               (unsigned)s_bq4050_min_cells,
                               (unsigned)k_bq4050_max_cells);
    if (snprintf_result > k_bq4050_snprintf_success_threshold &&
        (uint32_t)snprintf_result < sizeof(log_msg)) {
      rx_log_error(s_tag, log_msg);
    }
    return k_rx_err_invalid_arg;
  }

  err = internal_read_electrical_status(manager, bus_name, status, num_cells);
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_read_soc_status(manager, bus_name, status);
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_read_capacity_status(manager, bus_name, status);
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_read_timing_status(manager, bus_name, status);
  if (err != k_rx_ok) {
    return err;
  }

  err = internal_read_status_flags(manager, bus_name, status);
  if (err != k_rx_ok) {
    return err;
  }

  return k_rx_ok;
}
