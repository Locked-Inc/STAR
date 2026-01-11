/* include/pmod_config.h */

/**
 * @file pmod_config.h
 * @brief PMOD Expansion Module Configuration
 *
 * This file provides compile-time configuration for PMOD (Peripheral Module
 * Interface) expansion modules. The RX72N's 182 GPIO pins support multiple
 * PMOD connectors for extensibility.
 *
 * PMOD Standard (Digilent):
 * - 6-pin GPIO: 4 GPIO + VCC + GND
 * - 8-pin I2C:  SDA/SCL + INT/RST + VCC + GND
 * - 12-pin SPI: COPI/CIPO/SCK/CS + INT + VCC + GND
 *
 * Task Priority Allocation:
 * - Priorities 0-9: Core system threads (motor, comm, env, health)
 * - Priorities 10-31: PMOD expansion tasks (22 available)
 *
 * Usage:
 * 1. Define PMOD module types in pmod_type_t enum
 * 2. Configure each connector in pmod_connectors[] array
 * 3. Enable desired modules by setting type (default: k_pmod_none)
 * 4. Implement task creation functions in src/tasks/pmod_*.c
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_PMOD_CONFIG_H
#define STAR_RX72N_PMOD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"

/* =============================================================================
 * PMOD Module Types
 * =============================================================================
 */

/**
 * @brief PMOD module type enumeration
 *
 * Defines all supported PMOD module types. Add new modules here as needed.
 */
typedef enum {
  /* No module connected */
  k_pmod_none = 0, /**< No module (connector unused) */

  /* Display modules (SPI) */
  k_pmod_oled_rgb = 1, /**< PmodOLEDrgb: 96x64 OLED RGB display (SPI) */
  k_pmod_cls      = 2, /**< PmodCLS: 16x2 character LCD (SPI) */

  /* Sensor modules (I2C) */
  k_pmod_acl  = 10, /**< PmodACL: 3-axis accelerometer (I2C) */
  k_pmod_gyro = 11, /**< PmodGYRO: 3-axis gyroscope (SPI) */
  k_pmod_als  = 12, /**< PmodALS: Ambient light sensor (I2C) */
  k_pmod_tmp2 = 13, /**< PmodTMP2: Temperature sensor (I2C) */

  /* Communication modules */
  k_pmod_wifi     = 20, /**< PmodWiFi: 802.11 WiFi module (SPI) */
  k_pmod_bt2      = 21, /**< PmodBT2: Bluetooth 2.1 module (UART) */
  k_pmod_gps      = 22, /**< PmodGPS: GPS receiver (UART) */
  k_pmod_rf2      = 23, /**< PmodRF2: 2.4 GHz transceiver (SPI) */
  k_pmod_maxsonar = 24, /**< PmodMAXSONAR: Ultrasonic rangefinder (UART) */

  /* Input/Output modules (GPIO) */
  k_pmod_led     = 30, /**< PmodLED: 4 LEDs (GPIO output) */
  k_pmod_btn     = 31, /**< PmodBTN: 4 push buttons (GPIO input) */
  k_pmod_swt     = 32, /**< PmodSWT: 4 slide switches (GPIO input) */
  k_pmod_8ld     = 33, /**< Pmod8LD: 8 LEDs (GPIO output) */
  k_pmod_enc     = 34, /**< PmodENC: Rotary encoder with button (GPIO) */
  k_pmod_jstk    = 35, /**< PmodJSTK: 2-axis joystick (SPI) */
  k_pmod_kypd    = 36, /**< PmodKYPD: 16-button keypad (GPIO) */
  k_pmod_ssd     = 37, /**< PmodSSD: 7-segment display (SPI) */
  k_pmod_r2r_dac = 38, /**< Pmod R2R DAC: 8-bit resistor ladder DAC (GPIO) */

  /* Motor/Actuator modules */
  k_pmod_dhb1  = 40, /**< PmodDHB1: Dual H-bridge (PWM) */
  k_pmod_step  = 41, /**< PmodSTEP: Stepper motor driver (SPI) */
  k_pmod_servo = 42, /**< Custom servo controller (PWM) */

  /* Audio modules */
  k_pmod_i2s2    = 50, /**< PmodI2S2: Stereo audio codec (I2S) */
  k_pmod_mic3    = 51, /**< PmodMIC3: MEMS microphone (I2S) */
  k_pmod_amp2    = 52, /**< PmodAMP2: Audio amplifier (I2S) */
  k_pmod_speaker = 53, /**< Custom speaker driver (PWM) */

  /* Memory/Storage modules */
  k_pmod_sd     = 60, /**< PmodSD: MicroSD card (SPI) */
  k_pmod_eeprom = 61, /**< Custom EEPROM module (I2C) */

  /* Custom/Application-Specific modules */
  k_pmod_custom_1 = 100, /**< Custom module 1 */
  k_pmod_custom_2 = 101, /**< Custom module 2 */
  k_pmod_custom_3 = 102, /**< Custom module 3 */
  k_pmod_custom_4 = 103, /**< Custom module 4 */
} pmod_type_t;

/**
 * @brief PMOD interface type
 *
 * Specifies the communication interface used by the module.
 */
typedef enum {
  k_pmod_interface_gpio = 0, /**< GPIO-only (6-pin) */
  k_pmod_interface_i2c  = 1, /**< I2C (8-pin) */
  k_pmod_interface_spi  = 2, /**< SPI (12-pin) */
  k_pmod_interface_uart = 3, /**< UART (custom) */
  k_pmod_interface_pwm  = 4, /**< PWM (custom) */
  k_pmod_interface_i2s  = 5, /**< I2S audio (custom) */
} pmod_interface_t;

/* =============================================================================
 * PMOD Connector Configuration
 * =============================================================================
 */

/**
 * @brief PMOD connector configuration structure
 *
 * Each PMOD connector (JA-JF) has a configuration entry defining:
 * - Module type (what module is connected)
 * - Interface type (GPIO, I2C, SPI, etc.)
 * - Task priority (10-31 for PMOD tasks)
 * - Stack size (bytes, typically 1024-4096)
 * - Update rate (Hz, 0 = event-driven)
 */
typedef struct {
  pmod_type_t      type;           /**< Module type (k_pmod_none = unused) */
  pmod_interface_t interface;      /**< Communication interface */
  uint8_t          priority;       /**< ThreadX priority (10-31 for PMODs) */
  uint32_t         stack_size;     /**< Stack size in bytes */
  uint32_t         update_rate_hz; /**< Update rate (0 = event-driven) */
  const char*      task_name;      /**< Task name (NULL = auto-generated) */
} pmod_config_t;

/**
 * @brief PMOD connector identifiers
 *
 * Standard PMOD connector naming (JA-JF).
 */
typedef enum {
  k_pmod_connector_ja    = 0, /**< Connector JA */
  k_pmod_connector_jb    = 1, /**< Connector JB */
  k_pmod_connector_jc    = 2, /**< Connector JC */
  k_pmod_connector_jd    = 3, /**< Connector JD */
  k_pmod_connector_je    = 4, /**< Connector JE */
  k_pmod_connector_jf    = 5, /**< Connector JF */
  k_pmod_connector_count = 6, /**< Total connectors */
} pmod_connector_id_t;

/* =============================================================================
 * PMOD Priority Allocation
 * =============================================================================
 */

/**
 * @brief PMOD task priority ranges
 *
 * ThreadX priorities 0-31 (lower number = higher priority):
 * - 0-1:   Reserved (idle, timer)
 * - 2:     Motor_Controller (250 Hz, highest application priority)
 * - 3:     Comm_Manager (100 Hz, high priority)
 * - 4:     Reserved
 * - 5:     Env_Monitor (50 Hz, medium priority)
 * - 6-8:   Reserved
 * - 9:     System_Health (1 Hz, low priority)
 * - 10-31: PMOD tasks (22 available)
 *
 * PMOD Priority Guidelines:
 * - 10-15: High-rate sensors (>50 Hz) and real-time communication
 * - 16-23: Medium-rate sensors and I/O (1-50 Hz)
 * - 24-31: Low-rate sensors and background tasks (<1 Hz)
 */
typedef enum {
  k_pmod_priority_min = 10, /**< Minimum PMOD priority (highest) */
  k_pmod_priority_max = 31, /**< Maximum PMOD priority (lowest) */

  /* Suggested priority ranges */
  k_pmod_priority_high_start   = 10, /**< High-rate tasks (>50 Hz) */
  k_pmod_priority_high_end     = 15, /**< High-rate tasks end */
  k_pmod_priority_medium_start = 16, /**< Medium-rate tasks (1-50 Hz) */
  k_pmod_priority_medium_end   = 23, /**< Medium-rate tasks end */
  k_pmod_priority_low_start    = 24, /**< Low-rate tasks (<1 Hz) */
  k_pmod_priority_low_end      = 31, /**< Low-rate tasks end */
} pmod_priority_t;

/* =============================================================================
 * PMOD Configuration Array
 * =============================================================================
 */

/**
 * @brief PMOD connector configuration array
 *
 * Default configuration: All connectors set to k_pmod_none (unused).
 * To enable a module:
 * 1. Set type to desired module (e.g., k_pmod_oled_rgb)
 * 2. Set interface type (e.g., k_pmod_interface_spi)
 * 3. Set priority (10-31, see guidelines above)
 * 4. Set stack size (typically 1024-4096 bytes)
 * 5. Set update rate (Hz, 0 = event-driven)
 * 6. Set task name (NULL = auto-generated "PMOD_JA", etc.)
 *
 * Example (enable OLED RGB display on JA):
 * @code
 * [k_pmod_connector_ja] = {
 *     .type = k_pmod_oled_rgb,
 *     .interface = k_pmod_interface_spi,
 *     .priority = 20,
 *     .stack_size = 2048,
 *     .update_rate_hz = 30,
 *     .task_name = "PMOD_Display",
 * },
 * @endcode
 */
static const pmod_config_t g_pmod_connectors[k_pmod_connector_count] = {
  /* JA: Connector A (default: unused) */
  [k_pmod_connector_ja] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 20,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },

  /* JB: Connector B (default: unused) */
  [k_pmod_connector_jb] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 21,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },

  /* JC: Connector C (default: unused) */
  [k_pmod_connector_jc] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 22,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },

  /* JD: Connector D (default: unused) */
  [k_pmod_connector_jd] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 23,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },

  /* JE: Connector E (default: unused) */
  [k_pmod_connector_je] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 24,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },

  /* JF: Connector F (default: unused) */
  [k_pmod_connector_jf] =
    {
      .type           = k_pmod_none,
      .interface      = k_pmod_interface_gpio,
      .priority       = 25,
      .stack_size     = 2048,
      .update_rate_hz = 0,
      .task_name      = NULL,
    },
};

/* =============================================================================
 * PMOD Task Creation API
 * =============================================================================
 */

/**
 * @brief Create all enabled PMOD tasks
 *
 * Iterates through g_pmod_connectors[] and creates ThreadX threads for all
 * modules with type != k_pmod_none.
 *
 * Called from tx_application_define() after core system threads are created.
 *
 * @return k_rx_ok on success, error code otherwise
 */
rx_err_t pmod_create_all_tasks(void);

/**
 * @brief Get PMOD configuration for a connector
 *
 * @param[in] connector Connector ID (JA-JF)
 * @return Pointer to configuration, or NULL if invalid connector
 */
const pmod_config_t* pmod_get_config(pmod_connector_id_t connector);

/**
 * @brief Check if a PMOD connector is enabled
 *
 * @param[in] connector Connector ID (JA-JF)
 * @return true if module is enabled (type != k_pmod_none), false otherwise
 */
bool pmod_is_enabled(pmod_connector_id_t connector);

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_PMOD_CONFIG_H */
