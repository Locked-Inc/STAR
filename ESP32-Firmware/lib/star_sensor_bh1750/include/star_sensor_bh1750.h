/* lib/star_sensor_bh1750/include/star_sensor_bh1750.h */

#ifndef STAR_SENSOR_BH1750_H
#define STAR_SENSOR_BH1750_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"
#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_bh1750.h
 * @brief BH1750 ambient light sensor driver
 *
 * This driver provides interface to the BH1750 digital ambient light sensor.
 * It supports:
 * - High resolution mode (1 lx resolution)
 * - High resolution mode 2 (0.5 lx resolution)
 * - Low resolution mode (4 lx resolution)
 * - One-time and continuous measurement modes
 * - Measurement time adjustment (31-254)
 */

/* --- Constants --- */

/** BH1750 I2C addresses */
#define BH1750_ADDR_LOW  (0x23) /**< ADDR pin = LOW */
#define BH1750_ADDR_HIGH (0x5C) /**< ADDR pin = HIGH */

/** Default measurement time (69 for typical sensitivity) */
#define BH1750_DEFAULT_MTREG (69)

/** Measurement time register min/max values */
#define BH1750_MTREG_MIN (31)
#define BH1750_MTREG_MAX (254)

/** Typical measurement times (ms) */
#define BH1750_MEAS_TIME_H_RES  (120) /**< High resolution mode */
#define BH1750_MEAS_TIME_H_RES2 (120) /**< High resolution mode 2 */
#define BH1750_MEAS_TIME_L_RES  (16)  /**< Low resolution mode */

/* --- Command Codes --- */

#define BH1750_CMD_POWER_DOWN        (0x00) /**< No active state */
#define BH1750_CMD_POWER_ON          (0x01) /**< Waiting for measurement command */
#define BH1750_CMD_RESET             (0x07) /**< Reset data register */
#define BH1750_CMD_CONT_H_RES_MODE   (0x10) /**< Continuous H-Res mode */
#define BH1750_CMD_CONT_H_RES_MODE2  (0x11) /**< Continuous H-Res mode 2 */
#define BH1750_CMD_CONT_L_RES_MODE   (0x13) /**< Continuous L-Res mode */
#define BH1750_CMD_ONE_TIME_H_RES    (0x20) /**< One-time H-Res mode */
#define BH1750_CMD_ONE_TIME_H_RES2   (0x21) /**< One-time H-Res mode 2 */
#define BH1750_CMD_ONE_TIME_L_RES    (0x23) /**< One-time L-Res mode */
#define BH1750_CMD_CHANGE_MEAS_TIME_H (0x40) /**< Change measurement time high bits */
#define BH1750_CMD_CHANGE_MEAS_TIME_L (0x60) /**< Change measurement time low bits */

/* --- Types --- */

/**
 * @brief Measurement mode
 */
typedef enum {
  BH1750_MODE_ONE_TIME = 0,  /**< One-time measurement, auto power-down */
  BH1750_MODE_CONTINUOUS = 1 /**< Continuous measurement */
} bh1750_mode_t;

/**
 * @brief Resolution mode
 */
typedef enum {
  BH1750_RES_LOW = 0,   /**< 4 lx resolution, 16ms */
  BH1750_RES_HIGH = 1,  /**< 1 lx resolution, 120ms */
  BH1750_RES_HIGH2 = 2  /**< 0.5 lx resolution, 120ms */
} bh1750_resolution_t;

/**
 * @brief BH1750 device configuration
 */
typedef struct {
  uint8_t             i2c_addr;   /**< I2C device address */
  bh1750_mode_t       mode;       /**< Measurement mode */
  bh1750_resolution_t resolution; /**< Resolution mode */
  uint8_t             mtreg;      /**< Measurement time register (31-254) */
} bh1750_config_t;

/**
 * @brief BH1750 device handle
 */
typedef struct bh1750_handle {
  star_bus_manager_t* manager;       /**< Pointer to bus manager */
  const char*         bus_name;      /**< Name of I2C bus */
  uint8_t             i2c_addr;      /**< I2C device address */
  bh1750_config_t     config;        /**< Device configuration */
  error_handler_t     error_handler; /**< Error handler */
  bool                initialized;   /**< Initialization state */
  uint8_t             last_cmd;      /**< Last command sent */
} bh1750_handle_t;

/* --- Core Functions --- */

/**
 * @brief Initialize BH1750 light sensor
 *
 * @param[out] handle   Pointer to handle structure
 * @param[in]  manager  Pointer to bus manager
 * @param[in]  bus_name Name of I2C bus
 * @param[in]  config   Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_bh1750_init(bh1750_handle_t*       handle,
                                  star_bus_manager_t*    manager,
                                  const char*            bus_name,
                                  const bh1750_config_t* config);

/**
 * @brief Deinitialize BH1750 sensor
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_bh1750_deinit(bh1750_handle_t* handle);

/**
 * @brief Power on/off sensor
 *
 * @param[in] handle Pointer to initialized handle
 * @param[in] power  true to power on, false to power down
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_bh1750_power(const bh1750_handle_t* handle, bool power);

/**
 * @brief Reset sensor data register
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_bh1750_reset(const bh1750_handle_t* handle);

/* --- Measurement Functions --- */

/**
 * @brief Trigger measurement and read light level
 *
 * For one-time mode: triggers measurement, waits, and reads result
 * For continuous mode: reads current measurement value
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] lux    Light level in lux
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_bh1750_read_light(const bh1750_handle_t* handle, float* lux);

/**
 * @brief Trigger one-time measurement (non-blocking)
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Call bh1750_get_measurement_time() to determine wait time,
 *       then call star_sensor_bh1750_get_result() to read result
 */
esp_err_t star_sensor_bh1750_trigger_measurement(const bh1750_handle_t* handle);

/**
 * @brief Get measurement result without triggering
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] lux    Light level in lux
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_bh1750_get_result(const bh1750_handle_t* handle, float* lux);

/* --- Configuration Functions --- */

/**
 * @brief Set measurement mode
 *
 * @param[in] handle     Pointer to initialized handle
 * @param[in] mode       Measurement mode
 * @param[in] resolution Resolution mode
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_bh1750_set_mode(bh1750_handle_t*    handle,
                                      bh1750_mode_t       mode,
                                      bh1750_resolution_t resolution);

/**
 * @brief Set measurement time register
 *
 * Adjusts sensitivity and measurement time.
 * Higher values = higher sensitivity, longer measurement time
 *
 * @param[in] handle Pointer to initialized handle
 * @param[in] mtreg  Measurement time register value (31-254)
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if out of range
 */
esp_err_t star_sensor_bh1750_set_mtreg(bh1750_handle_t* handle, uint8_t mtreg);

/* --- Helper Functions --- */

/**
 * @brief Get measurement time for current configuration
 *
 * @param[in]  handle      Pointer to initialized handle
 * @param[out] time_ms     Measurement time in milliseconds
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_bh1750_get_measurement_time(const bh1750_handle_t* handle, uint32_t* time_ms);

/**
 * @brief Convert raw sensor value to lux
 *
 * @param[in]  raw_value   Raw 16-bit sensor value
 * @param[in]  resolution  Resolution mode used
 * @param[in]  mtreg       Measurement time register value
 * @param[out] lux         Light level in lux
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_bh1750_raw_to_lux(uint16_t            raw_value,
                                        bh1750_resolution_t resolution,
                                        uint8_t             mtreg,
                                        float*              lux);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_BH1750_H */
