/* src/rx_bq25798.c */

/**
 * @file rx_bq25798.c
 * @brief BQ25798RQMR Battery Charger Driver Implementation
 * @details
 * Texas Instruments BQ25798RQMR I2C battery charger driver.
 *
 * Register Map (selected registers):
 * - 0x00-0x01: Minimal System Voltage
 * - 0x02: Charge Voltage Limit
 * - 0x03: Charge Current Limit
 * - 0x04-0x05: Input Voltage Limit
 * - 0x06-0x07: Input Current Limit
 * - 0x0E-0x0F: Charger Control registers
 * - 0x1B: Charger Status 0
 * - 0x1C: Charger Status 1
 * - 0x1D: Charger Status 2
 * - 0x1E: Charger Status 3
 * - 0x1F: Charger Status 4
 * - 0x20-0x26: Fault Status registers
 * - 0x27: ADC Control
 * - 0x28: ADC Function Disable 0
 * - 0x29: ADC Function Disable 1
 * - 0x2F-0x36: ADC results
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx_bq25798.h"

#include "rx_bus_i2c.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "BQ25798";

/* =============================================================================
 * Register Map
 * =============================================================================
 */

typedef enum {
  k_reg_minimal_sys_volt_low     = 0x00, /* Minimal System Voltage Low Byte */
  k_reg_minimal_sys_volt_high    = 0x01, /* Minimal System Voltage High Byte */
  k_reg_charge_volt_limit        = 0x02, /* Charge Voltage Limit */
  k_reg_charge_current_limit     = 0x03, /* Charge Current Limit */
  k_reg_input_volt_limit_low     = 0x04, /* Input Voltage Limit Low Byte */
  k_reg_input_volt_limit_high    = 0x05, /* Input Voltage Limit High Byte */
  k_reg_input_current_limit_low  = 0x06, /* Input Current Limit Low Byte */
  k_reg_input_current_limit_high = 0x07, /* Input Current Limit High Byte */
  k_reg_precharge_control        = 0x08, /* Precharge Control */
  k_reg_termination_control      = 0x09, /* Termination Control */
  k_reg_recharge_control         = 0x0A, /* Recharge Control */
  k_reg_votg_regulation          = 0x0B, /* VOTG Regulation */
  k_reg_iotg_regulation_low      = 0x0C, /* IOTG Regulation Low Byte */
  k_reg_iotg_regulation_high     = 0x0D, /* IOTG Regulation High Byte */
  k_reg_charger_control_0        = 0x0E, /* Charger Control 0 */
  k_reg_charger_control_1        = 0x0F, /* Charger Control 1 */
  k_reg_charger_control_2        = 0x10, /* Charger Control 2 */
  k_reg_charger_control_3        = 0x11, /* Charger Control 3 */
  k_reg_charger_control_4        = 0x12, /* Charger Control 4 */
  k_reg_charger_control_5        = 0x13, /* Charger Control 5 */
  k_reg_charger_status_0         = 0x1B, /* Charger Status 0 */
  k_reg_charger_status_1         = 0x1C, /* Charger Status 1 */
  k_reg_charger_status_2         = 0x1D, /* Charger Status 2 */
  k_reg_charger_status_3         = 0x1E, /* Charger Status 3 */
  k_reg_charger_status_4         = 0x1F, /* Charger Status 4 */
  k_reg_fault_status_0           = 0x20, /* Fault Status 0 */
  k_reg_fault_status_1           = 0x21, /* Fault Status 1 */
  k_reg_adc_control              = 0x27, /* ADC Control */
  k_reg_adc_function_disable_0   = 0x28, /* ADC Function Disable 0 */
  k_reg_adc_function_disable_1   = 0x29, /* ADC Function Disable 1 */
  k_reg_adc_ibus_low             = 0x2F, /* ADC IBUS Low Byte */
  k_reg_adc_ibus_high            = 0x30, /* ADC IBUS High Byte */
  k_reg_adc_ichg_low             = 0x31, /* ADC ICHG Low Byte */
  k_reg_adc_ichg_high            = 0x32, /* ADC ICHG High Byte */
  k_reg_adc_vbus_low             = 0x33, /* ADC VBUS Low Byte */
  k_reg_adc_vbus_high            = 0x34, /* ADC VBUS High Byte */
  k_reg_adc_vbat_low             = 0x35, /* ADC VBAT Low Byte */
  k_reg_adc_vbat_high            = 0x36, /* ADC VBAT High Byte */
  k_reg_adc_vsys_low             = 0x37, /* ADC VSYS Low Byte */
  k_reg_adc_vsys_high            = 0x38, /* ADC VSYS High Byte */
  k_reg_adc_ts_low               = 0x39, /* ADC TS Low Byte */
  k_reg_adc_ts_high              = 0x3A, /* ADC TS High Byte */
  k_reg_adc_tdie_low             = 0x3B, /* ADC TDIE Low Byte */
  k_reg_adc_tdie_high            = 0x3C, /* ADC TDIE High Byte */
  k_reg_part_information         = 0x48, /* Part Information */
} bq25798_register_t;

/* Charger Control 0 bits */
typedef enum {
  k_ctrl0_en_chg  = (1 << 5), /* Charge enable */
  k_ctrl0_en_hiz  = (1 << 7), /* High impedance mode */
  k_ctrl0_wdt_rst = (1 << 6), /* Watchdog timer reset */
} charger_control_0_bits_t;

/* Charger Status 1 bits */
typedef enum {
  k_status1_vbus_stat_mask  = 0x0F, /* VBUS status mask (bits 0-3) */
  k_status1_chrg_stat_mask  = 0x70, /* Charge status mask (bits 4-6) */
  k_status1_chrg_stat_shift = 4,    /* Charge status shift */
} charger_status_1_bits_t;

/* ADC Control bits */
typedef enum {
  k_adc_en            = (1 << 7), /* ADC enable */
  k_adc_rate_mask     = (1 << 6), /* ADC conversion rate */
  k_adc_sample_mask   = (3 << 4), /* ADC sample speed */
  k_adc_avg_mask      = (1 << 3), /* ADC average enable */
  k_adc_avg_init_mask = (1 << 2), /* ADC average init */
} adc_control_bits_t;

/* =============================================================================
 * Constants
 * =============================================================================
 */

typedef enum {
  k_bq25798_i2c_address = 0x6B, /* Default I2C address */

  /* Voltage/Current conversion factors */
  k_charge_volt_offset_mv   = 3000, /* Charge voltage offset: 3000mV */
  k_charge_volt_step_mv     = 10,   /* Charge voltage step: 10mV */
  k_charge_current_step_ma  = 10,   /* Charge current step: 10mA */
  k_input_volt_offset_mv    = 3600, /* Input voltage offset: 3600mV */
  k_input_volt_step_mv      = 100,  /* Input voltage step: 100mV */
  k_input_current_offset_ma = 100,  /* Input current offset: 100mA */
  k_input_current_step_ma   = 10,   /* Input current step: 10mA */

  /* ADC conversion factors */
  k_adc_vbus_step_mv = 1, /* VBUS ADC step: 1mV */
  k_adc_vbat_step_mv = 1, /* VBAT ADC step: 1mV */
  k_adc_vsys_step_mv = 1, /* VSYS ADC step: 1mV */
  k_adc_ichg_step_ma = 2, /* ICHG ADC step: 2mA */
  k_adc_ibus_step_ma = 1, /* IBUS ADC step: 1mA */
} bq25798_constants_t;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Read 16-bit value from two consecutive registers
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[in] reg_low Low byte register address
 * @param[out] value Pointer to store 16-bit value
 *
 * @return RX_OK on success, error code on failure
 */
static rx_err_t internal_read_word(rx_bus_manager_t* manager,
                                   const char*       bus_name,
                                   uint8_t           reg_low,
                                   uint16_t*         value)
{
  uint8_t  data[2];
  rx_err_t err;

  /* Read low byte */
  err = rx_bus_i2c_write_read(manager, bus_name, &reg_low, 1, &data[0], 1);
  if (err != RX_OK) {
    return err;
  }

  /* Read high byte */
  uint8_t reg_high = reg_low + 1;
  err              = rx_bus_i2c_write_read(manager, bus_name, &reg_high, 1, &data[1], 1);
  if (err != RX_OK) {
    return err;
  }

  *value = ((uint16_t)data[1] << 8) | data[0];
  return RX_OK;
}

/**
 * @brief Write 16-bit value to two consecutive registers
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[in] reg_low Low byte register address
 * @param[in] value 16-bit value to write
 *
 * @return RX_OK on success, error code on failure
 */
static rx_err_t internal_write_word(rx_bus_manager_t* manager,
                                    const char*       bus_name,
                                    uint8_t           reg_low,
                                    uint16_t          value)
{
  uint8_t  write_data[2];
  rx_err_t err;

  /* Write low byte */
  write_data[0] = reg_low;
  write_data[1] = (uint8_t)(value & 0xFF);
  err           = rx_bus_i2c_write(manager, bus_name, write_data, 2);
  if (err != RX_OK) {
    return err;
  }

  /* Write high byte */
  write_data[0] = reg_low + 1;
  write_data[1] = (uint8_t)(value >> 8);
  err           = rx_bus_i2c_write(manager, bus_name, write_data, 2);

  return err;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t
rx_bq25798_init(rx_bus_manager_t* manager, const char* bus_name, const bq25798_config_t* config)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  RX_LOG_INFO(s_tag, "Initializing BQ25798 charger");

  /* Initialize I2C bus */
  rx_err_t err = rx_bus_i2c_init(manager, bus_name);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "I2C initialization failed");
    return err;
  }

  /* Verify part information */
  uint8_t part_info;
  uint8_t reg_addr = k_reg_part_information;
  err              = rx_bus_i2c_write_read(manager, bus_name, &reg_addr, 1, &part_info, 1);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to read part information");
    return err;
  }

  RX_LOG_INFO(s_tag, "Info");

  /* Configure charge voltage */
  if (config->charge_voltage_mv > 0) {
    err = rx_bq25798_set_charge_voltage(manager, bus_name, config->charge_voltage_mv);
    if (err != RX_OK) {
      RX_LOG_ERROR(s_tag, "Failed to set charge voltage");
      return err;
    }
  }

  /* Configure charge current */
  err = rx_bq25798_set_charge_current(manager, bus_name, config->charge_current_ma);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to set charge current");
    return err;
  }

  /* Configure input voltage limit */
  if (config->input_voltage_limit_mv > 0) {
    err = rx_bq25798_set_input_voltage_limit(manager, bus_name, config->input_voltage_limit_mv);
    if (err != RX_OK) {
      RX_LOG_ERROR(s_tag, "Failed to set input voltage limit");
      return err;
    }
  }

  /* Configure input current limit */
  if (config->input_current_limit_ma > 0) {
    err = rx_bq25798_set_input_current_limit(manager, bus_name, config->input_current_limit_ma);
    if (err != RX_OK) {
      RX_LOG_ERROR(s_tag, "Failed to set input current limit");
      return err;
    }
  }

  /* Enable ADC if requested */
  if (config->enable_adc) {
    uint8_t write_data[2] = {k_reg_adc_control, k_adc_en | (1 << 6)}; /* Continuous mode */
    err                   = rx_bus_i2c_write(manager, bus_name, write_data, 2);
    if (err != RX_OK) {
      RX_LOG_ERROR(s_tag, "Failed to enable ADC");
      return err;
    }
  }

  /* Enable/disable charging */
  err = rx_bq25798_enable_charging(manager, bus_name, config->enable_charging);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to configure charging enable");
    return err;
  }

  RX_LOG_INFO(s_tag, "BQ25798 initialized successfully");

  return RX_OK;
}

rx_err_t rx_bq25798_enable_charging(rx_bus_manager_t* manager, const char* bus_name, bool enable)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  /* Read current control register */
  uint8_t  ctrl_reg;
  uint8_t  reg_addr = k_reg_charger_control_0;
  rx_err_t err      = rx_bus_i2c_write_read(manager, bus_name, &reg_addr, 1, &ctrl_reg, 1);
  if (err != RX_OK) {
    return err;
  }

  /* Modify EN_CHG bit */
  if (enable) {
    ctrl_reg |= k_ctrl0_en_chg;
  } else {
    ctrl_reg &= ~k_ctrl0_en_chg;
  }

  /* Write back */
  uint8_t write_data[2] = {k_reg_charger_control_0, ctrl_reg};
  return rx_bus_i2c_write(manager, bus_name, write_data, 2);
}

rx_err_t
rx_bq25798_set_charge_voltage(rx_bus_manager_t* manager, const char* bus_name, uint16_t voltage_mv)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  if (voltage_mv < 3000 || voltage_mv > 18800) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  /* Convert to register value */
  uint16_t reg_val = (voltage_mv - k_charge_volt_offset_mv) / k_charge_volt_step_mv;

  uint8_t write_data[2] = {k_reg_charge_volt_limit, (uint8_t)reg_val};
  return rx_bus_i2c_write(manager, bus_name, write_data, 2);
}

rx_err_t
rx_bq25798_set_charge_current(rx_bus_manager_t* manager, const char* bus_name, uint16_t current_ma)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  if (current_ma > 5000) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  /* Convert to register value */
  uint16_t reg_val = current_ma / k_charge_current_step_ma;

  uint8_t write_data[2] = {k_reg_charge_current_limit, (uint8_t)reg_val};
  return rx_bus_i2c_write(manager, bus_name, write_data, 2);
}

rx_err_t rx_bq25798_set_input_voltage_limit(rx_bus_manager_t* manager,
                                            const char*       bus_name,
                                            uint16_t          voltage_mv)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  if (voltage_mv < 3600 || voltage_mv > 24000) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  /* Convert to register value */
  uint16_t reg_val = (voltage_mv - k_input_volt_offset_mv) / k_input_volt_step_mv;

  return internal_write_word(manager, bus_name, k_reg_input_volt_limit_low, reg_val);
}

rx_err_t rx_bq25798_set_input_current_limit(rx_bus_manager_t* manager,
                                            const char*       bus_name,
                                            uint16_t          current_ma)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  if (current_ma < 100 || current_ma > 3300) {
    RX_LOG_ERROR(s_tag, "Error occurred");
    return RX_ERR_INVALID_ARG;
  }

  /* Convert to register value */
  uint16_t reg_val = (current_ma - k_input_current_offset_ma) / k_input_current_step_ma;

  return internal_write_word(manager, bus_name, k_reg_input_current_limit_low, reg_val);
}

rx_err_t
rx_bq25798_read_status(rx_bus_manager_t* manager, const char* bus_name, bq25798_status_t* status)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(status, s_tag, "status pointer is NULL");

  uint8_t  status_regs[5];
  rx_err_t err;

  /* Read status registers 0-4 */
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t reg_addr = k_reg_charger_status_0 + i;
    err              = rx_bus_i2c_write_read(manager, bus_name, &reg_addr, 1, &status_regs[i], 1);
    if (err != RX_OK) {
      RX_LOG_ERROR(s_tag, "Error occurred");
      return err;
    }
  }

  /* Parse status from register 1 */
  status->vbus_status   = (bq25798_vbus_status_t)(status_regs[1] & k_status1_vbus_stat_mask);
  status->charge_status = (bq25798_charge_status_t)((status_regs[1] & k_status1_chrg_stat_mask) >>
                                                    k_status1_chrg_stat_shift);

  /* Parse other status flags */
  status->power_good         = (status_regs[0] & (1 << 7)) != 0;
  status->vindpm_active      = (status_regs[2] & (1 << 0)) != 0;
  status->iindpm_active      = (status_regs[2] & (1 << 1)) != 0;
  status->thermal_regulation = (status_regs[2] & (1 << 2)) != 0;

  /* Read charger control to check if charging is enabled */
  uint8_t ctrl_reg;
  uint8_t reg_addr = k_reg_charger_control_0;
  err              = rx_bus_i2c_write_read(manager, bus_name, &reg_addr, 1, &ctrl_reg, 1);
  if (err != RX_OK) {
    return err;
  }
  status->charging_enabled = (ctrl_reg & k_ctrl0_en_chg) != 0;

  /* Read fault status */
  uint8_t fault_reg;
  reg_addr = k_reg_fault_status_0;
  err      = rx_bus_i2c_write_read(manager, bus_name, &reg_addr, 1, &fault_reg, 1);
  if (err != RX_OK) {
    return err;
  }
  status->watchdog_fault = (fault_reg & (1 << 7)) != 0;

  return RX_OK;
}

rx_err_t rx_bq25798_read_adc(rx_bus_manager_t* manager, const char* bus_name, bq25798_adc_t* adc)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(adc, s_tag, "adc pointer is NULL");

  rx_err_t err;

  /* Read VBUS */
  uint16_t raw_val;
  err = internal_read_word(manager, bus_name, k_reg_adc_vbus_low, &raw_val);
  if (err != RX_OK) {
    return err;
  }
  adc->vbus_mv = raw_val * k_adc_vbus_step_mv;

  /* Read VBAT */
  err = internal_read_word(manager, bus_name, k_reg_adc_vbat_low, &raw_val);
  if (err != RX_OK) {
    return err;
  }
  adc->vbat_mv = raw_val * k_adc_vbat_step_mv;

  /* Read VSYS */
  err = internal_read_word(manager, bus_name, k_reg_adc_vsys_low, &raw_val);
  if (err != RX_OK) {
    return err;
  }
  adc->vsys_mv = raw_val * k_adc_vsys_step_mv;

  /* Read ICHG (signed) */
  err = internal_read_word(manager, bus_name, k_reg_adc_ichg_low, &raw_val);
  if (err != RX_OK) {
    return err;
  }
  adc->ichg_ma = (int16_t)raw_val * k_adc_ichg_step_ma;

  /* Read IBUS */
  err = internal_read_word(manager, bus_name, k_reg_adc_ibus_low, &raw_val);
  if (err != RX_OK) {
    return err;
  }
  adc->ibus_ma = raw_val * k_adc_ibus_step_ma;

  /* Read TS temperature (placeholder - conversion is device-specific) */
  err = internal_read_word(manager, bus_name, k_reg_adc_ts_low, &raw_val);
  if (err != RX_OK) {
    return err;
  }
  adc->ts_temp_c = 25; /* Placeholder */

  /* Read die temperature (placeholder - conversion is device-specific) */
  err = internal_read_word(manager, bus_name, k_reg_adc_tdie_low, &raw_val);
  if (err != RX_OK) {
    return err;
  }
  adc->tdie_temp_c = 25; /* Placeholder */

  return RX_OK;
}

rx_err_t rx_bq25798_reset_watchdog(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  /* Read current control register */
  uint8_t  ctrl_reg;
  uint8_t  reg_addr = k_reg_charger_control_0;
  rx_err_t err      = rx_bus_i2c_write_read(manager, bus_name, &reg_addr, 1, &ctrl_reg, 1);
  if (err != RX_OK) {
    return err;
  }

  /* Set watchdog reset bit */
  ctrl_reg |= k_ctrl0_wdt_rst;

  /* Write back */
  uint8_t write_data[2] = {k_reg_charger_control_0, ctrl_reg};
  return rx_bus_i2c_write(manager, bus_name, write_data, 2);
}

rx_err_t rx_bq25798_reset(rx_bus_manager_t* manager, const char* bus_name)
{
  RX_CHECK_NULL_PTR(manager, s_tag, "manager pointer is NULL");
  RX_CHECK_NULL_PTR(bus_name, s_tag, "bus_name pointer is NULL");

  /* Set REG_RST bit in Charger Control 1 */
  uint8_t write_data[2] = {k_reg_charger_control_1, (1 << 7)}; /* REG_RST bit */
  return rx_bus_i2c_write(manager, bus_name, write_data, 2);
}
