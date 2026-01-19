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

#include "rx_bq4050_constants.h"
#include "rx_bus_smbus.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BQ4050";

/**
 * @brief Temperature conversion constants
 */
typedef enum : uint16_t {
  k_temp_kelvin_offset = 2731, /**< Offset: 0.1K to 0.1°C (273.15K × 10) */
  k_temp_decimal_scale = 10,   /**< Scale: 0.1 units to whole units */
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
 * @brief Convert temperature from 0.1K units to degrees Celsius
 *
 * SBS temperature format: 16-bit unsigned integer in 0.1 Kelvin units
 * Conversion: temp_c = (temp_0.1k * 0.1) - 273.15
 *
 * @param[in] temp_0_1k Temperature in 0.1K units
 * @return Temperature in degrees Celsius
 */
static int16_t internal_convert_temperature(uint16_t temp_0_1k)
{
  /* Convert from 0.1K to 0.1°C by subtracting 2731 (273.1K) */
  int32_t temp_0_1c = (int32_t)temp_0_1k - k_temp_kelvin_offset;

  /* Convert to whole degrees Celsius */
  return (int16_t)(temp_0_1c / k_temp_decimal_scale);
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t
rx_bq4050_init(rx_bus_manager_t* manager, const char* bus_name, const rx_bq4050_config_t* config)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  (void)config; /* Configuration stored in BQ4050 data flash, minimal init needed */

  rx_log_info(s_tag, "Initializing BQ4050 fuel gauge");

  /* Initialize SMBus */
  rx_err_t err = rx_bus_smbus_init(manager, bus_name);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "SMBus initialization failed");
    return err;
  }

  /* Verify communication by reading voltage */
  uint16_t voltage_mv;
  err = rx_bq4050_read_voltage(manager, bus_name, &voltage_mv);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to communicate with BQ4050 at address 0x0B");
    return err;
  }

  rx_log_info(s_tag, "BQ4050 initialized successfully at address 0x0B");

  return k_rx_ok;
}

rx_err_t
rx_bq4050_read_voltage(rx_bus_manager_t* manager, const char* bus_name, uint16_t* voltage_mv)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(voltage_mv, s_tag, "voltage_mv pointer is NULL");

  return rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_voltage, voltage_mv);
}

rx_err_t rx_bq4050_read_cell_voltages(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint16_t*         cell_voltages,
                                      uint8_t           num_cells)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(cell_voltages, s_tag, "cell_voltages pointer is NULL");

  if (num_cells > k_bq4050_max_cells) {
    rx_log_error(s_tag, "num_cells exceeds maximum (4 cells)");
    return k_rx_err_invalid_arg;
  }

  /* BQ4050 cell voltage registers are 0x3F (cell 1) to 0x3C (cell 4) */
  static const uint8_t s_cell_reg_map[k_bq4050_max_cells] = {
    [k_cell_idx_1] = k_sbs_cell_voltage_1, /* Cell 1 at 0x3F */
    [k_cell_idx_2] = k_sbs_cell_voltage_2, /* Cell 2 at 0x3E */
    [k_cell_idx_3] = k_sbs_cell_voltage_3, /* Cell 3 at 0x3D */
    [k_cell_idx_4] = k_sbs_cell_voltage_4, /* Cell 4 at 0x3C */
  };

  for (uint8_t i = 0; i < num_cells; i++) {
    rx_err_t err =
      rx_bus_smbus_read_word_data(manager, bus_name, s_cell_reg_map[i], &cell_voltages[i]);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to read cell voltage");
      return err;
    }
  }

  return k_rx_ok;
}

rx_err_t
rx_bq4050_read_current(rx_bus_manager_t* manager, const char* bus_name, int16_t* current_ma)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(current_ma, s_tag, "current_ma pointer is NULL");

  /* Read as unsigned, interpret as signed (SBS current is signed 16-bit) */
  uint16_t raw_current;
  rx_err_t err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_current, &raw_current);

  if (err == k_rx_ok) {
    *current_ma = (int16_t)raw_current;
  }

  return err;
}

rx_err_t rx_bq4050_read_average_current(rx_bus_manager_t* manager,
                                        const char*       bus_name,
                                        int16_t*          avg_current_ma)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(avg_current_ma, s_tag, "avg_current_ma pointer is NULL");

  /* Read as unsigned, interpret as signed */
  uint16_t raw_current;
  rx_err_t err =
    rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_average_current, &raw_current);

  if (err == k_rx_ok) {
    *avg_current_ma = (int16_t)raw_current;
  }

  return err;
}

rx_err_t
rx_bq4050_read_relative_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc_percent)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(soc_percent, s_tag, "soc_percent pointer is NULL");

  uint16_t soc_word;
  rx_err_t err =
    rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_relative_state_of_charge, &soc_word);

  if (err == k_rx_ok) {
    /* Clamp to 0-100 range (SBS allows > 100% in some conditions) */
    *soc_percent = (soc_word > k_soc_max_percent) ? k_soc_max_percent : (uint8_t)soc_word;
  }

  return err;
}

rx_err_t
rx_bq4050_read_absolute_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc_percent)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(soc_percent, s_tag, "soc_percent pointer is NULL");

  uint16_t soc_word;
  rx_err_t err =
    rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_absolute_state_of_charge, &soc_word);

  if (err == k_rx_ok) {
    /* Clamp to 0-100 range */
    *soc_percent = (soc_word > k_soc_max_percent) ? k_soc_max_percent : (uint8_t)soc_word;
  }

  return err;
}

rx_err_t
rx_bq4050_read_temperature(rx_bus_manager_t* manager, const char* bus_name, int16_t* temperature_c)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(temperature_c, s_tag, "temperature_c pointer is NULL");

  uint16_t temp_0_1k;
  rx_err_t err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_temperature, &temp_0_1k);

  if (err == k_rx_ok) {
    *temperature_c = internal_convert_temperature(temp_0_1k);
  }

  return err;
}

rx_err_t rx_bq4050_read_capacity(rx_bus_manager_t* manager,
                                 const char*       bus_name,
                                 uint16_t*         remaining_mah,
                                 uint16_t*         full_mah)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(remaining_mah, s_tag, "remaining_mah pointer is NULL");
  RX_CHECK_NULL_PTR(full_mah, s_tag, "full_mah pointer is NULL");

  /* Read remaining capacity */
  rx_err_t err =
    rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_remaining_capacity, remaining_mah);
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

rx_err_t rx_bq4050_read_status(rx_bus_manager_t*   manager,
                               const char*         bus_name,
                               rx_bq4050_status_t* status,
                               uint8_t             num_cells)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(status, s_tag, "status pointer is NULL");

  if (num_cells > k_bq4050_max_cells) {
    rx_log_error(s_tag, "num_cells parameter exceeds k_bq4050_max_cells");
    return k_rx_err_invalid_arg;
  }

  rx_err_t err;

  /* Read pack voltage */
  err = rx_bq4050_read_voltage(manager, bus_name, &status->voltage_mv);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read voltage");
    return err;
  }

  /* Read individual cell voltages */
  err = rx_bq4050_read_cell_voltages(manager, bus_name, status->cell_voltages_mv, num_cells);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read cell voltages");
    return err;
  }

  /* Read current */
  err = rx_bq4050_read_current(manager, bus_name, &status->current_ma);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read current");
    return err;
  }

  /* Read average current */
  err = rx_bq4050_read_average_current(manager, bus_name, &status->average_current_ma);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read average current");
    return err;
  }

  /* Read relative state of charge */
  err = rx_bq4050_read_relative_soc(manager, bus_name, &status->relative_soc);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read relative SOC");
    return err;
  }

  /* Read absolute state of charge */
  err = rx_bq4050_read_absolute_soc(manager, bus_name, &status->absolute_soc);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read absolute SOC");
    return err;
  }

  /* Read temperature */
  err = rx_bq4050_read_temperature(manager, bus_name, &status->temperature_c);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read temperature");
    return err;
  }

  /* Read capacity */
  err = rx_bq4050_read_capacity(manager,
                                bus_name,
                                &status->remaining_capacity_mah,
                                &status->full_capacity_mah);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read capacity");
    return err;
  }

  /* Read design capacity */
  err = rx_bus_smbus_read_word_data(manager,
                                    bus_name,
                                    k_sbs_design_capacity,
                                    &status->design_capacity_mah);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read design capacity");
    return err;
  }

  /* Read cycle count */
  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_cycle_count, &status->cycle_count);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read cycle count");
    return err;
  }

  /* Read time to empty */
  err = rx_bus_smbus_read_word_data(manager,
                                    bus_name,
                                    k_sbs_run_time_to_empty,
                                    &status->time_to_empty_min);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read time to empty");
    return err;
  }

  /* Read time to full */
  err = rx_bus_smbus_read_word_data(manager,
                                    bus_name,
                                    k_sbs_average_time_to_full,
                                    &status->time_to_full_min);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read time to full");
    return err;
  }

  /* Read battery status flags */
  uint16_t status_flags;
  err = rx_bus_smbus_read_word_data(manager, bus_name, k_sbs_battery_status, &status_flags);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read battery status");
    return err;
  }

  /* Parse status flags */
  status->is_charging         = !(status_flags & k_bq4050_status_discharging);
  status->is_fully_charged    = (status_flags & k_bq4050_status_fully_charged) != 0;
  status->is_fully_discharged = (status_flags & k_bq4050_status_fully_discharged) != 0;
  status->is_low_capacity     = (status_flags & k_bq4050_status_remaining_capacity_alarm) != 0;

  return k_rx_ok;
}
