/* src/rx_fuel_gauge.c */

/**
 * @file rx_fuel_gauge.c
 * @brief Fuel Gauge Driver Implementation for RX72N
 * @details
 * Generic SMBUS fuel gauge driver implementation.
 * Compatible with BQ27441, MAX17048, and similar devices.
 *
 * Standard SMBUS battery fuel gauge registers (SBS 1.1 compliant):
 * - 0x08: Temperature (word, 0.1K units)
 * - 0x09: Voltage (word, mV)
 * - 0x0A: Flags (word, status bits)
 * - 0x0C: Nominal Available Capacity (word, mAh)
 * - 0x0D: Full Available Capacity (word, mAh)
 * - 0x0E: Remaining Capacity (word, mAh)
 * - 0x10: Average Current (word, mA, signed)
 * - 0x12: Time To Empty (word, minutes)
 * - 0x13: Time To Full (word, minutes)
 * - 0x16: State of Charge (word, percent)
 * - 0x17: Cycle Count (word, cycles)
 * - 0x3E: Design Capacity (word, mAh)
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx_fuel_gauge.h"

#include "rx_bus_smbus.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "FUEL_GAUGE";

/* =============================================================================
 * SMBUS Register Map (SBS 1.1)
 * =============================================================================
 */

typedef enum {
  k_reg_temperature           = 0x08, /* Temperature in 0.1K */
  k_reg_voltage               = 0x09, /* Voltage in mV */
  k_reg_flags                 = 0x0A, /* Status flags */
  k_reg_nominal_capacity      = 0x0C, /* Nominal available capacity mAh */
  k_reg_full_capacity         = 0x0D, /* Full available capacity mAh */
  k_reg_remaining_capacity    = 0x0E, /* Remaining capacity mAh */
  k_reg_average_current       = 0x10, /* Average current mA (signed) */
  k_reg_time_to_empty         = 0x12, /* Time to empty in minutes */
  k_reg_time_to_full          = 0x13, /* Time to full in minutes */
  k_reg_state_of_charge       = 0x16, /* State of charge percent */
  k_reg_cycle_count           = 0x17, /* Cycle count */
  k_reg_design_capacity       = 0x18, /* Design capacity mAh */
  k_reg_design_voltage        = 0x19, /* Design voltage mV */
  k_reg_specification_info    = 0x1A, /* Specification info */
  k_reg_manufacture_date      = 0x1B, /* Manufacture date */
  k_reg_serial_number         = 0x1C, /* Serial number */
  k_reg_manufacturer_name     = 0x20, /* Manufacturer name (block) */
  k_reg_device_name           = 0x21, /* Device name (block) */
  k_reg_device_chemistry      = 0x22, /* Device chemistry (block) */
  k_reg_manufacturer_data     = 0x23, /* Manufacturer data (block) */
  k_reg_optional_mfg_function = 0x3F, /* Optional manufacturer function */
} fuel_gauge_register_t;

/**
 * @brief Battery status flags (register 0x0A)
 */
typedef enum {
  k_flag_over_charged_alarm        = (1 << 15), /* Over charged alarm */
  k_flag_terminate_charge_alarm    = (1 << 14), /* Terminate charge alarm */
  k_flag_over_temp_alarm           = (1 << 12), /* Over temperature alarm */
  k_flag_terminate_discharge_alarm = (1 << 11), /* Terminate discharge alarm */
  k_flag_remaining_capacity_alarm  = (1 << 9),  /* Remaining capacity alarm */
  k_flag_remaining_time_alarm      = (1 << 8),  /* Remaining time alarm */
  k_flag_initialized               = (1 << 7),  /* Battery initialized */
  k_flag_discharging               = (1 << 6),  /* Battery discharging */
  k_flag_fully_charged             = (1 << 5),  /* Battery fully charged */
  k_flag_fully_discharged          = (1 << 4),  /* Battery fully discharged */
} battery_flags_t;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Convert temperature from 0.1K units to degrees Celsius
 *
 * @param[in] temp_0_1k Temperature in 0.1K units
 *
 * @return Temperature in degrees Celsius
 */
static int16_t internal_convert_temperature(uint16_t temp_0_1k)
{
  /* Convert from 0.1K to 0.1 deg C: subtract 2731.5 (273.15K) */
  int32_t temp_0_1c = (int32_t)temp_0_1k - 2731;

  /* Convert to whole degrees Celsius */
  return (int16_t)(temp_0_1c / 10);
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_fuel_gauge_init(rx_bus_manager_t*             manager,
                            const char*                   bus_name,
                            const rx_fuel_gauge_config_t* config)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  star_log_info(s_tag, "Initializing fuel gauge");

  /* Initialize SMBUS */
  rx_err_t err = rx_bus_smbus_init(manager, bus_name);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "SMBUS initialization failed");
    return err;
  }

  /* Write design capacity if provided */
  if (config->design_capacity_mah > 0) {
    err = rx_bus_smbus_write_word_data(manager,
                                       bus_name,
                                       k_reg_design_capacity,
                                       config->design_capacity_mah);
    if (err != k_rx_ok) {
      star_log_warn(s_tag, "Failed to write design capacity");
      /* Non-fatal - continue */
    }
  }

  /* Verify communication by reading voltage */
  uint16_t voltage_mv;
  err = rx_fuel_gauge_read_voltage(manager, bus_name, &voltage_mv);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to communicate with fuel gauge");
    return err;
  }

  star_log_info(s_tag, "Info");

  return k_rx_ok;
}

rx_err_t
rx_fuel_gauge_read_voltage(rx_bus_manager_t* manager, const char* bus_name, uint16_t* voltage_mv)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(voltage_mv, s_tag, "voltage_mv pointer is NULL");

  return rx_bus_smbus_read_word_data(manager, bus_name, k_reg_voltage, voltage_mv);
}

rx_err_t
rx_fuel_gauge_read_current(rx_bus_manager_t* manager, const char* bus_name, int16_t* current_ma)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(current_ma, s_tag, "current_ma pointer is NULL");

  /* Read as unsigned, interpret as signed */
  uint16_t raw_current;
  rx_err_t err =
    rx_bus_smbus_read_word_data(manager, bus_name, k_reg_average_current, &raw_current);

  if (err == k_rx_ok) {
    *current_ma = (int16_t)raw_current;
  }

  return err;
}

rx_err_t
rx_fuel_gauge_read_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc_percent)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(soc_percent, s_tag, "soc_percent pointer is NULL");

  uint16_t soc_word;
  rx_err_t err = rx_bus_smbus_read_word_data(manager, bus_name, k_reg_state_of_charge, &soc_word);

  if (err == k_rx_ok) {
    /* Clamp to 0-100 range */
    *soc_percent = (soc_word > 100) ? 100 : (uint8_t)soc_word;
  }

  return err;
}

rx_err_t rx_fuel_gauge_read_temperature(rx_bus_manager_t* manager,
                                        const char*       bus_name,
                                        int16_t*          temperature_c)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(temperature_c, s_tag, "temperature_c pointer is NULL");

  uint16_t temp_0_1k;
  rx_err_t err = rx_bus_smbus_read_word_data(manager, bus_name, k_reg_temperature, &temp_0_1k);

  if (err == k_rx_ok) {
    *temperature_c = internal_convert_temperature(temp_0_1k);
  }

  return err;
}

rx_err_t rx_fuel_gauge_read_status(rx_bus_manager_t*       manager,
                                   const char*             bus_name,
                                   rx_fuel_gauge_status_t* status)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(status, s_tag, "status pointer is NULL");

  rx_err_t err;

  /* Read voltage */
  err = rx_fuel_gauge_read_voltage(manager, bus_name, &status->voltage_mv);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to read voltage");
    return err;
  }

  /* Read current */
  err = rx_fuel_gauge_read_current(manager, bus_name, &status->current_ma);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to read current");
    return err;
  }

  /* Read state of charge */
  err = rx_fuel_gauge_read_soc(manager, bus_name, &status->state_of_charge);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to read state of charge");
    return err;
  }

  /* Read temperature */
  err = rx_fuel_gauge_read_temperature(manager, bus_name, &status->temperature_c);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to read temperature");
    return err;
  }

  /* Read remaining capacity */
  err =
    rx_bus_smbus_read_word_data(manager, bus_name, k_reg_remaining_capacity, &status->capacity_mah);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to read remaining capacity");
    return err;
  }

  /* Read full capacity */
  err =
    rx_bus_smbus_read_word_data(manager, bus_name, k_reg_full_capacity, &status->full_capacity_mah);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to read full capacity");
    return err;
  }

  /* Read time to empty */
  err =
    rx_bus_smbus_read_word_data(manager, bus_name, k_reg_time_to_empty, &status->time_to_empty_min);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to read time to empty");
    return err;
  }

  /* Read time to full */
  err =
    rx_bus_smbus_read_word_data(manager, bus_name, k_reg_time_to_full, &status->time_to_full_min);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to read time to full");
    return err;
  }

  /* Read cycle count */
  err = rx_bus_smbus_read_word_data(manager, bus_name, k_reg_cycle_count, &status->cycle_count);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to read cycle count");
    return err;
  }

  /* Read status flags */
  uint16_t flags;
  err = rx_bus_smbus_read_word_data(manager, bus_name, k_reg_flags, &flags);
  if (err != k_rx_ok) {
    star_log_error(s_tag, "Failed to read flags");
    return err;
  }

  /* Parse flags */
  status->is_charging = !(flags & k_flag_discharging);
  status->is_full     = (flags & k_flag_fully_charged) != 0;
  status->is_low      = (flags & k_flag_remaining_capacity_alarm) != 0;

  return k_rx_ok;
}

rx_err_t rx_fuel_gauge_set_low_threshold(rx_bus_manager_t* manager,
                                         const char*       bus_name,
                                         uint8_t           threshold_percent)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  if (threshold_percent > 100) {
    star_log_error(s_tag, "Error occurred");
    return k_rx_err_invalid_arg;
  }

  /* Note: Setting alarm thresholds is device-specific.
   * This is a placeholder that would need to be customized
   * for specific fuel gauge ICs.
   */
  star_log_warn(s_tag, "Low threshold setting not implemented for this device");

  return k_rx_ok;
}

rx_err_t rx_fuel_gauge_start_learning(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  /* Note: Learning cycle initiation is device-specific.
   * This is a placeholder that would need to be customized
   * for specific fuel gauge ICs (e.g., BQ27441 uses AtRate() commands).
   */
  star_log_warn(s_tag, "Learning cycle not implemented for this device");

  return k_rx_ok;
}
