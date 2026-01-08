/**
 * @file pmod_config.h
 * @brief PMOD expansion module configuration
 * @details
 * Defines PMOD expansion framework for future module integration.
 * PMOD tasks use priorities 10-31 (lower priority than core tasks).
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef PMOD_CONFIG_H
#define PMOD_CONFIG_H

/* =============================================================================
 * PMOD Module Types
 * =============================================================================
 */

/**
 * @brief PMOD module types
 */
typedef enum {
    k_pmod_none = 0,         /**< No module installed */
    k_pmod_spi_display,      /**< SPI display (e.g., OLED) */
    k_pmod_i2c_accel,        /**< I2C accelerometer/IMU */
    k_pmod_gpio_leds,        /**< GPIO LED array */
    k_pmod_uart_gps,         /**< UART GPS module */
    k_pmod_custom,           /**< Custom user module */
} pmod_module_type_t;

/**
 * @brief PMOD connector configuration
 */
typedef struct {
    pmod_module_type_t type;     /**< Module type */
    uint8_t            priority;  /**< ThreadX priority (10-31) */
    uint32_t           stack_size; /**< Thread stack size */
    bool               enabled;    /**< Module enabled */
} pmod_config_t;

/* =============================================================================
 * PMOD Connectors
 * =============================================================================
 */

/**
 * @brief PMOD connector enumeration
 */
typedef enum {
    k_pmod_ja = 0, /**< PMOD connector JA */
    k_pmod_jb,     /**< PMOD connector JB */
    k_pmod_jc,     /**< PMOD connector JC */
    k_pmod_jd,     /**< PMOD connector JD */
    k_pmod_je,     /**< PMOD connector JE */
    k_pmod_jf,     /**< PMOD connector JF */
    k_pmod_count,  /**< Total PMOD connectors */
} pmod_connector_t;

/* =============================================================================
 * PMOD Configuration (Compile-Time)
 * =============================================================================
 */

/**
 * @brief PMOD module configuration
 * @details All modules disabled by default
 */
static const pmod_config_t s_pmod_configs[k_pmod_count] = {
    [k_pmod_ja] = {.type = k_pmod_none, .priority = 10, .stack_size = 2048, .enabled = false},
    [k_pmod_jb] = {.type = k_pmod_none, .priority = 11, .stack_size = 2048, .enabled = false},
    [k_pmod_jc] = {.type = k_pmod_none, .priority = 12, .stack_size = 2048, .enabled = false},
    [k_pmod_jd] = {.type = k_pmod_none, .priority = 13, .stack_size = 2048, .enabled = false},
    [k_pmod_je] = {.type = k_pmod_none, .priority = 14, .stack_size = 2048, .enabled = false},
    [k_pmod_jf] = {.type = k_pmod_none, .priority = 15, .stack_size = 2048, .enabled = false},
};

#endif /* PMOD_CONFIG_H */
