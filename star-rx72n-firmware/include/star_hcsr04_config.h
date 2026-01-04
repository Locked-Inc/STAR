/* include/star_hcsr04_config.h */

/**
 * @file star_hcsr04_config.h
 * @brief STAR Motor Controller HC-SR04 Sensor Configuration
 *
 * @details
 * Board-specific configuration for HC-SR04 ultrasonic distance sensors.
 * Defines the 4 sensor connectors (J24-J27) on the STAR Motor Controller PCB.
 *
 * Sensor Connections:
 * - J24 (Sensor 1): PMOD JB GPIO0/GPIO1 - Front Left
 * - J25 (Sensor 2): PMOD JB GPIO2/GPIO3 - Front Right
 * - J26 (Sensor 3): PMOD JE GPIO0/GPIO1 - Rear Left
 * - J27 (Sensor 4): PMOD JE GPIO2/GPIO3 - Rear Right
 *
 * @see docs/sections/03_hardware_pinout.tex for complete pin assignments
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_HCSR04_CONFIG_H
#define STAR_RX72N_HCSR04_CONFIG_H

#include "hardware_pinout.h"
#include "rx_hcsr04.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Sensor Identifiers
 * =============================================================================
 */

/**
 * @brief HC-SR04 sensor identifiers for STAR Motor Controller
 *
 * Maps to the J24-J27 connectors on the PCB.
 */
typedef enum {
  k_star_hcsr04_sensor_1     = 0, /**< J24: PMOD JB GPIO0/GPIO1 */
  k_star_hcsr04_sensor_2     = 1, /**< J25: PMOD JB GPIO2/GPIO3 */
  k_star_hcsr04_sensor_3     = 2, /**< J26: PMOD JE GPIO0/GPIO1 */
  k_star_hcsr04_sensor_4     = 3, /**< J27: PMOD JE GPIO2/GPIO3 */
  k_star_hcsr04_sensor_count = 4,
} star_hcsr04_sensor_id_t;

/* =============================================================================
 * Default Configurations
 * =============================================================================
 */

/**
 * @brief Default sensor configurations for all 4 HC-SR04 sensors
 *
 * Sensor 1 (J24 - PMOD JB):
 *   TRIG: PC6 (PMOD JB GPIO0)
 *   ECHO: P55 (PMOD JB GPIO1)
 *
 * Sensor 2 (J25 - PMOD JB):
 *   TRIG: P54 (PMOD JB GPIO2)
 *   ECHO: PA2 (PMOD JB GPIO3)
 *
 * Sensor 3 (J26 - PMOD JE):
 *   TRIG: PJ3 (PMOD JE GPIO0)
 *   ECHO: PB1 (PMOD JE GPIO1)
 *
 * Sensor 4 (J27 - PMOD JE):
 *   TRIG: PB0 (PMOD JE GPIO2)
 *   ECHO: PA1 (PMOD JE GPIO3)
 */
static const rx_hcsr04_config_t k_star_hcsr04_default_configs[k_star_hcsr04_sensor_count] = {
  [k_star_hcsr04_sensor_1] =
    {
      .trigger_port = k_pmod_jb_gpio0_port,
      .trigger_pin  = k_pmod_jb_gpio0_pin,
      .echo_port    = k_pmod_jb_gpio1_port,
      .echo_pin     = k_pmod_jb_gpio1_pin,
      .timeout_us   = k_hcsr04_echo_timeout_us,
    },
  [k_star_hcsr04_sensor_2] =
    {
      .trigger_port = k_pmod_jb_gpio2_port,
      .trigger_pin  = k_pmod_jb_gpio2_pin,
      .echo_port    = k_pmod_jb_gpio3_port,
      .echo_pin     = k_pmod_jb_gpio3_pin,
      .timeout_us   = k_hcsr04_echo_timeout_us,
    },
  [k_star_hcsr04_sensor_3] =
    {
      .trigger_port = k_pmod_je_gpio0_port,
      .trigger_pin  = k_pmod_je_gpio0_pin,
      .echo_port    = k_pmod_je_gpio1_port,
      .echo_pin     = k_pmod_je_gpio1_pin,
      .timeout_us   = k_hcsr04_echo_timeout_us,
    },
  [k_star_hcsr04_sensor_4] =
    {
      .trigger_port = k_pmod_je_gpio2_port,
      .trigger_pin  = k_pmod_je_gpio2_pin,
      .echo_port    = k_pmod_je_gpio3_port,
      .echo_pin     = k_pmod_je_gpio3_pin,
      .timeout_us   = k_hcsr04_echo_timeout_us,
    },
};

/* =============================================================================
 * Convenience Functions
 * =============================================================================
 */

/**
 * @brief Get default configuration for a STAR board sensor
 *
 * Populates config structure with pin assignments for the specified sensor.
 *
 * @param[in]  sensor_id Sensor identifier (k_star_hcsr04_sensor_1 to _4)
 * @param[out] config    Configuration structure to populate
 *
 * @return k_rx_ok on success
 * @return k_rx_err_null_pointer if config is NULL
 * @return k_rx_err_invalid_arg if sensor_id is out of range
 */
static inline rx_err_t star_hcsr04_get_config(star_hcsr04_sensor_id_t sensor_id,
                                              rx_hcsr04_config_t*     config)
{
  if (config == NULL) {
    return k_rx_err_null_pointer;
  }

  if (sensor_id >= k_star_hcsr04_sensor_count) {
    return k_rx_err_invalid_arg;
  }

  *config = k_star_hcsr04_default_configs[sensor_id];
  return k_rx_ok;
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_HCSR04_CONFIG_H */
