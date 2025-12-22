/* include/rx_bq25798.h */

/**
 * @file rx_bq25798.h
 * @brief BQ25798RQMR Battery Charger Driver for RX72N
 * @details
 * Driver for Texas Instruments BQ25798RQMR battery charger IC.
 *
 * Features:
 * - 3.6V to 24V input voltage range
 * - Up to 5A charge current
 * - 1-4 cell Li-Ion/Li-Polymer battery support
 * - USB PD3.1 and USB Type-C support
 * - Integrated ADC for monitoring
 * - I2C interface (7-bit address 0x6B)
 *
 * Uses bus manager for thread-safe I2C communication.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef STAR_RX72N_BQ25798_H
#define STAR_RX72N_BQ25798_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief Charging status
 */
typedef enum {
  k_bq25798_charge_status_not_charging = 0,
  k_bq25798_charge_status_trickle      = 1,
  k_bq25798_charge_status_pre_charge   = 2,
  k_bq25798_charge_status_fast_charge  = 3,
  k_bq25798_charge_status_taper        = 4,
  k_bq25798_charge_status_top_off      = 5,
  k_bq25798_charge_status_done         = 6,
} bq25798_charge_status_t;

/**
 * @brief VBUS status
 */
typedef enum {
  k_bq25798_vbus_status_no_input = 0,
  k_bq25798_vbus_status_usb_host = 1,
  k_bq25798_vbus_status_adapter  = 2,
  k_bq25798_vbus_status_otg      = 3,
} bq25798_vbus_status_t;

/**
 * @brief Charger configuration
 */
typedef struct {
  uint16_t charge_voltage_mv;         /**< Charge voltage limit in mV (3000-18800) */
  uint16_t charge_current_ma;         /**< Charge current limit in mA (0-5000) */
  uint16_t input_voltage_limit_mv;    /**< Input voltage limit in mV (3600-24000) */
  uint16_t input_current_limit_ma;    /**< Input current limit in mA (100-3300) */
  uint16_t minimum_system_voltage_mv; /**< Minimum system voltage in mV */
  bool     enable_charging;           /**< Enable charging on init */
  bool     enable_adc;                /**< Enable ADC measurements */
} bq25798_config_t;

/**
 * @brief Charger status information
 */
typedef struct {
  bq25798_charge_status_t charge_status;      /**< Charging status */
  bq25798_vbus_status_t   vbus_status;        /**< VBUS status */
  bool                    power_good;         /**< Power good indicator */
  bool                    vindpm_active;      /**< Input voltage DPM active */
  bool                    iindpm_active;      /**< Input current DPM active */
  bool                    thermal_regulation; /**< Thermal regulation active */
  bool                    watchdog_fault;     /**< Watchdog timer fault */
  bool                    charging_enabled;   /**< Charging enabled */
} bq25798_status_t;

/**
 * @brief ADC measurement results
 */
typedef struct {
  uint16_t vbus_mv;     /**< VBUS voltage in mV */
  uint16_t vbat_mv;     /**< Battery voltage in mV */
  uint16_t vsys_mv;     /**< System voltage in mV */
  int16_t  ichg_ma;     /**< Charge current in mA */
  int16_t  ibus_ma;     /**< Input current in mA */
  int16_t  ts_temp_c;   /**< TS pin temperature in deg C */
  int16_t  tdie_temp_c; /**< Die temperature in deg C */
} bq25798_adc_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize BQ25798 charger
 *
 * Configures charger with specified parameters and enables charging if requested.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name for charger
 * @param[in] config Charger configuration
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager, bus_name, or config is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if I2C timeout
 * @return RX_ERR_NACK if device not responding
 * @return RX_ERR_INVALID_ARG if configuration values are out of range
 */
rx_err_t
rx_bq25798_init(rx_bus_manager_t* manager, const char* bus_name, const bq25798_config_t* config);

/**
 * @brief Enable or disable charging
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[in] enable True to enable charging, false to disable
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if I2C timeout
 */
rx_err_t rx_bq25798_enable_charging(rx_bus_manager_t* manager, const char* bus_name, bool enable);

/**
 * @brief Set charge voltage limit
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[in] voltage_mv Charge voltage in mV (3000-18800, step 10mV)
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_INVALID_ARG if voltage is out of range
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if I2C timeout
 */
rx_err_t
rx_bq25798_set_charge_voltage(rx_bus_manager_t* manager, const char* bus_name, uint16_t voltage_mv);

/**
 * @brief Set charge current limit
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[in] current_ma Charge current in mA (0-5000, step 10mA)
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_INVALID_ARG if current is out of range
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if I2C timeout
 */
rx_err_t
rx_bq25798_set_charge_current(rx_bus_manager_t* manager, const char* bus_name, uint16_t current_ma);

/**
 * @brief Set input voltage limit (VINDPM)
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[in] voltage_mv Input voltage limit in mV (3600-24000, step 100mV)
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_INVALID_ARG if voltage is out of range
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if I2C timeout
 */
rx_err_t rx_bq25798_set_input_voltage_limit(rx_bus_manager_t* manager,
                                            const char*       bus_name,
                                            uint16_t          voltage_mv);

/**
 * @brief Set input current limit (IINDPM)
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[in] current_ma Input current limit in mA (100-3300, step 10mA)
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_INVALID_ARG if current is out of range
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if I2C timeout
 */
rx_err_t rx_bq25798_set_input_current_limit(rx_bus_manager_t* manager,
                                            const char*       bus_name,
                                            uint16_t          current_ma);

/**
 * @brief Read charger status
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[out] status Pointer to status structure
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager, bus_name, or status is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if I2C timeout
 */
rx_err_t
rx_bq25798_read_status(rx_bus_manager_t* manager, const char* bus_name, bq25798_status_t* status);

/**
 * @brief Read ADC measurements
 *
 * Reads all ADC channels and returns voltage, current, and temperature measurements.
 * ADC must be enabled during initialization.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 * @param[out] adc Pointer to ADC measurement structure
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager, bus_name, or adc is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized or ADC not enabled
 * @return RX_ERR_TIMEOUT if I2C timeout
 */
rx_err_t rx_bq25798_read_adc(rx_bus_manager_t* manager, const char* bus_name, bq25798_adc_t* adc);

/**
 * @brief Reset watchdog timer
 *
 * Resets the watchdog timer to prevent charger timeout.
 * Should be called periodically if watchdog is enabled.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if I2C timeout
 */
rx_err_t rx_bq25798_reset_watchdog(rx_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Perform device reset
 *
 * Resets all registers to default values.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name I2C bus name
 *
 * @return RX_OK on success
 * @return RX_ERR_NULL_POINTER if manager or bus_name is NULL
 * @return RX_ERR_NOT_FOUND if bus not found
 * @return RX_ERR_INVALID_STATE if bus not initialized
 * @return RX_ERR_TIMEOUT if I2C timeout
 */
rx_err_t rx_bq25798_reset(rx_bus_manager_t* manager, const char* bus_name);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BQ25798_H */
