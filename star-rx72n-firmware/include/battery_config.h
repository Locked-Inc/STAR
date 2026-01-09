/* include/battery_config.h */

/**
 * @file battery_config.h
 * @brief Battery monitoring configuration constants
 * @details
 * Defines battery health thresholds for BQ4050 fuel gauge.
 * Used by System_Health task.
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef BATTERY_CONFIG_H
#define BATTERY_CONFIG_H

/* =============================================================================
 * Battery Monitoring Configuration
 * =============================================================================
 */

/**
 * @brief Battery monitoring configuration (BQ4050)
 * @details Battery health thresholds
 */
typedef enum {
  k_battery_low_soc_percent  = 20, /**< Low battery threshold (%) */
  k_battery_critical_percent = 10, /**< Critical battery threshold (%) */
  k_battery_sample_hz        = 1,  /**< Sample rate (Hz) */
} battery_config_t;

#endif /* BATTERY_CONFIG_H */
