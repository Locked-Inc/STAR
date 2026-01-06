/* lib/rx_bq4050/inc/rx_bq4050.h */

/**
 * @file rx_bq4050.h
 * @brief BQ4050 Battery Fuel Gauge Driver for RX72N
 *
 * Texas Instruments BQ4050RSMR battery fuel gauge driver using SMBus protocol.
 * The BQ4050 is a fully integrated battery management IC for 1-4 series Li-Ion/
 * Li-Polymer battery packs with:
 *
 * - CEDV (Compensated End-of-Discharge Voltage) gas gauging algorithm
 * - Accurate state of charge estimation (±1% typical)
 * - Individual cell voltage monitoring and balancing
 * - Battery protection (overvoltage, undervoltage, overcurrent, temperature)
 * - SHA-1 authentication
 * - Non-volatile configuration storage
 *
 * Communication:
 * - Protocol: SMBus v1.1/v2.0 (System Management Bus)
 * - I2C Address: 0x0B (7-bit)
 * - Speed: Up to 400 kHz (Fast Mode)
 * - Implements Smart Battery System (SBS) 1.1 specification
 *
 * References:
 * - BQ4050 Data Sheet (Texas Instruments)
 * - BQ4050 Technical Reference Manual (SLUUAQ3A)
 * - Smart Battery Data Specification v1.1
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_BQ4050_H
#define STAR_RX72N_BQ4050_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_bus_manager.h"
#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief BQ4050 I2C address (7-bit)
 */
typedef enum {
  k_bq4050_i2c_addr = 0x0B, /**< BQ4050 default I2C address */
} bq4050_i2c_constants_t;

/**
 * @brief BQ4050 cell configuration
 */
typedef enum {
  k_bq4050_max_cells = 4, /**< Maximum number of battery cells (1-4 series) */
} bq4050_cell_constants_t;

/* =============================================================================
 * Type Definitions
 * =============================================================================
 */

/**
 * @brief BQ4050 battery status structure
 *
 * Contains complete battery state information from the BQ4050 fuel gauge.
 */
typedef struct {
  uint16_t voltage_mv;                           /**< Battery pack voltage in millivolts */
  uint16_t cell_voltages_mv[k_bq4050_max_cells]; /**< Individual cell voltages (mV) */
  int16_t  current_ma;         /**< Battery current in milliamps (+ = charging, - = discharging) */
  int16_t  average_current_ma; /**< 1-minute rolling average current in milliamps */
  uint8_t  relative_soc;       /**< Relative state of charge (% of full charge capacity, 0-100) */
  uint8_t  absolute_soc;       /**< Absolute state of charge (% of design capacity, 0-100) */
  int16_t  temperature_c;      /**< Battery pack temperature in degrees Celsius */
  uint16_t remaining_capacity_mah; /**< Remaining capacity in milliamp-hours */
  uint16_t full_capacity_mah;      /**< Full charge capacity in milliamp-hours (aged) */
  uint16_t design_capacity_mah;    /**< Design capacity in milliamp-hours (new battery) */
  uint16_t cycle_count;            /**< Number of charge/discharge cycles */
  uint16_t time_to_empty_min;      /**< Estimated time to empty in minutes (0xFFFF if charging) */
  uint16_t time_to_full_min;       /**< Estimated time to full in minutes (0xFFFF if discharging) */
  bool     is_charging;            /**< True if battery is charging */
  bool     is_fully_charged;       /**< True if battery is fully charged */
  bool     is_fully_discharged;    /**< True if battery is fully discharged */
  bool     is_low_capacity;        /**< True if remaining capacity alarm triggered */
} rx_bq4050_status_t;

/**
 * @brief BQ4050 configuration structure
 *
 * Configuration parameters for BQ4050 initialization.
 * Most parameters are stored in BQ4050's non-volatile data flash
 * and configured during battery pack manufacturing.
 */
typedef struct {
  uint8_t num_cells; /**< Number of cells in series (1-4) */
} rx_bq4050_config_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize BQ4050 fuel gauge
 *
 * Verifies communication with the BQ4050 and reads initial battery state.
 * The BQ4050 is pre-configured via data flash during battery pack
 * manufacturing, so minimal initialization is required.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name for BQ4050
 * @param[in] config BQ4050 configuration (optional, can be NULL for defaults)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager or bus_name is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBus timeout
 * @return k_rx_err_nack if BQ4050 not responding at address 0x0B
 */
rx_err_t
rx_bq4050_init(rx_bus_manager_t* manager, const char* bus_name, const rx_bq4050_config_t* config);

/**
 * @brief Read battery pack voltage
 *
 * Reads total battery pack voltage from SBS Voltage register (0x09).
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[out] voltage_mv Pointer to store voltage in millivolts
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or voltage_mv is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBus timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t
rx_bq4050_read_voltage(rx_bus_manager_t* manager, const char* bus_name, uint16_t* voltage_mv);

/**
 * @brief Read individual cell voltages
 *
 * Reads voltages for each cell in the battery pack from BQ4050-specific
 * cell voltage registers (0x3C-0x3F).
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[out] cell_voltages Array to store cell voltages (mV), size k_bq4050_max_cells
 * @param[in] num_cells Number of cells to read (1-4)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or cell_voltages is NULL
 * @return k_rx_err_invalid_arg if num_cells > k_bq4050_max_cells
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBus timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_bq4050_read_cell_voltages(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint16_t*         cell_voltages,
                                      uint8_t           num_cells);

/**
 * @brief Read battery current
 *
 * Reads instantaneous battery current from SBS Current register (0x0A).
 * Positive current indicates charging, negative indicates discharging.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[out] current_ma Pointer to store current in milliamps
 *                        (+ = charging, - = discharging)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or current_ma is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBus timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t
rx_bq4050_read_current(rx_bus_manager_t* manager, const char* bus_name, int16_t* current_ma);

/**
 * @brief Read average battery current
 *
 * Reads 1-minute rolling average current from SBS AverageCurrent register (0x0B).
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[out] avg_current_ma Pointer to store average current in milliamps
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or avg_current_ma is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBus timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_bq4050_read_average_current(rx_bus_manager_t* manager,
                                        const char*       bus_name,
                                        int16_t*          avg_current_ma);

/**
 * @brief Read relative state of charge
 *
 * Reads state of charge as percentage of current full charge capacity
 * from SBS RelativeStateOfCharge register (0x0D). Accounts for battery aging.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[out] soc_percent Pointer to store state of charge (0-100%)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or soc_percent is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBus timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t
rx_bq4050_read_relative_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc_percent);

/**
 * @brief Read absolute state of charge
 *
 * Reads state of charge as percentage of original design capacity
 * from SBS AbsoluteStateOfCharge register (0x0E).
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[out] soc_percent Pointer to store absolute SOC (0-100%)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or soc_percent is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBus timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t
rx_bq4050_read_absolute_soc(rx_bus_manager_t* manager, const char* bus_name, uint8_t* soc_percent);

/**
 * @brief Read battery temperature
 *
 * Reads battery pack temperature from SBS Temperature register (0x08).
 * Temperature is measured via thermistor in the battery pack.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[out] temperature_c Pointer to store temperature in degrees Celsius
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or temperature_c is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBus timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t
rx_bq4050_read_temperature(rx_bus_manager_t* manager, const char* bus_name, int16_t* temperature_c);

/**
 * @brief Read battery capacity information
 *
 * Reads remaining capacity and full charge capacity from SBS registers
 * (0x0F and 0x10).
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[out] remaining_mah Pointer to store remaining capacity (mAh)
 * @param[out] full_mah Pointer to store full charge capacity (mAh)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if any pointer is NULL
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBus timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_bq4050_read_capacity(rx_bus_manager_t* manager,
                                 const char*       bus_name,
                                 uint16_t*         remaining_mah,
                                 uint16_t*         full_mah);

/**
 * @brief Read complete battery status
 *
 * Reads all available battery parameters in a single call.
 * More efficient than calling individual read functions.
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name SMBus bus name
 * @param[out] status Pointer to status structure
 * @param[in] num_cells Number of cells to read (1-4)
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if manager, bus_name, or status is NULL
 * @return k_rx_err_invalid_arg if num_cells > k_bq4050_max_cells
 * @return k_rx_err_not_found if bus not found
 * @return k_rx_err_invalid_state if bus not initialized
 * @return k_rx_err_timeout if SMBus timeout
 * @return k_rx_err_crc_mismatch if PEC check fails
 */
rx_err_t rx_bq4050_read_status(rx_bus_manager_t*   manager,
                               const char*         bus_name,
                               rx_bq4050_status_t* status,
                               uint8_t             num_cells);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BQ4050_H */
