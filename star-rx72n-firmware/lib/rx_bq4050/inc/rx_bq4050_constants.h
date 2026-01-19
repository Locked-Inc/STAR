/* lib/rx_bq4050/inc/rx_bq4050_constants.h */

/**
 * @file rx_bq4050_constants.h
 * @brief Shared BQ4050 SBS command and status constants
 *
 * Centralizes Smart Battery System (SBS) command codes and battery status
 * flag definitions for reuse across the driver and tests.
 *
 * @date 2026-01-16
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_BQ4050_CONSTANTS_H
#define STAR_RX72N_BQ4050_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * SBS Command Codes
 * =============================================================================
 */

/**
 * @brief Smart Battery System (SBS) 1.1 command codes
 */
typedef enum : uint8_t {
  k_sbs_manufacturer_access      = 0x00, /**< Manufacturer access commands */
  k_sbs_temperature              = 0x08, /**< Temperature (0.1K) */
  k_sbs_voltage                  = 0x09, /**< Pack voltage (mV) */
  k_sbs_current                  = 0x0A, /**< Current (mA, signed) */
  k_sbs_average_current          = 0x0B, /**< Average current (mA, signed) */
  k_sbs_relative_state_of_charge = 0x0D, /**< Relative SOC (%) */
  k_sbs_absolute_state_of_charge = 0x0E, /**< Absolute SOC (%) */
  k_sbs_remaining_capacity       = 0x0F, /**< Remaining capacity (mAh) */
  k_sbs_full_charge_capacity     = 0x10, /**< Full charge capacity (mAh) */
  k_sbs_run_time_to_empty        = 0x11, /**< Time to empty (min) */
  k_sbs_average_time_to_full     = 0x13, /**< Time to full (min) */
  k_sbs_battery_status           = 0x16, /**< Battery status flags */
  k_sbs_cycle_count              = 0x17, /**< Cycle count */
  k_sbs_design_capacity          = 0x18, /**< Design capacity (mAh) */
  k_sbs_cell_voltage_4           = 0x3C, /**< Cell 4 voltage (mV) */
  k_sbs_cell_voltage_3           = 0x3D, /**< Cell 3 voltage (mV) */
  k_sbs_cell_voltage_2           = 0x3E, /**< Cell 2 voltage (mV) */
  k_sbs_cell_voltage_1           = 0x3F, /**< Cell 1 voltage (mV) */
} bq4050_sbs_command_t;

/* =============================================================================
 * Battery Status Flags
 * =============================================================================
 */

/**
 * @brief Battery status flags (SBS register 0x16)
 */
typedef enum : uint8_t {
  k_bq4050_status_bit_pos_over_charged_alarm        = 15, /**< Over charged alarm bit */
  k_bq4050_status_bit_pos_terminate_charge_alarm    = 14, /**< Terminate charge alarm bit */
  k_bq4050_status_bit_pos_over_temp_alarm           = 12, /**< Over temperature alarm bit */
  k_bq4050_status_bit_pos_terminate_discharge_alarm = 11, /**< Terminate discharge alarm bit */
  k_bq4050_status_bit_pos_remaining_capacity_alarm  = 9,  /**< Remaining capacity alarm bit */
  k_bq4050_status_bit_pos_remaining_time_alarm      = 8,  /**< Remaining time alarm bit */
  k_bq4050_status_bit_pos_initialized               = 7,  /**< Battery initialized bit */
  k_bq4050_status_bit_pos_discharging               = 6,  /**< Battery discharging bit */
  k_bq4050_status_bit_pos_fully_charged             = 5,  /**< Fully charged bit */
  k_bq4050_status_bit_pos_fully_discharged          = 4,  /**< Fully discharged bit */
} bq4050_status_bit_positions_t;

/**
 * @brief Battery status flags (SBS register 0x16)
 */
typedef enum : uint16_t {
  k_bq4050_status_over_charged_alarm =
    (1U << k_bq4050_status_bit_pos_over_charged_alarm), /**< Over charged alarm */
  k_bq4050_status_terminate_charge_alarm =
    (1U << k_bq4050_status_bit_pos_terminate_charge_alarm), /**< Terminate charge alarm */
  k_bq4050_status_over_temp_alarm =
    (1U << k_bq4050_status_bit_pos_over_temp_alarm), /**< Over temperature alarm */
  k_bq4050_status_terminate_discharge_alarm =
    (1U << k_bq4050_status_bit_pos_terminate_discharge_alarm), /**< Terminate discharge alarm */
  k_bq4050_status_remaining_capacity_alarm =
    (1U << k_bq4050_status_bit_pos_remaining_capacity_alarm), /**< Remaining capacity alarm */
  k_bq4050_status_remaining_time_alarm =
    (1U << k_bq4050_status_bit_pos_remaining_time_alarm), /**< Remaining time alarm */
  k_bq4050_status_initialized =
    (1U << k_bq4050_status_bit_pos_initialized), /**< Battery initialized */
  k_bq4050_status_discharging =
    (1U << k_bq4050_status_bit_pos_discharging), /**< Battery discharging */
  k_bq4050_status_fully_charged =
    (1U << k_bq4050_status_bit_pos_fully_charged), /**< Fully charged */
  k_bq4050_status_fully_discharged =
    (1U << k_bq4050_status_bit_pos_fully_discharged), /**< Fully discharged */
} bq4050_status_flags_t;

/* =============================================================================
 * Time Constants
 * =============================================================================
 */

/**
 * @brief Invalid/N/A time-to-full value
 */
static const uint16_t s_bq4050_time_to_full_invalid = 0xFFFF;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BQ4050_CONSTANTS_H */
