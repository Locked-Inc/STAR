/* lib/star_sensor_pca9685/include/star_sensor_pca9685.h */

#ifndef STAR_SENSOR_PCA9685_H
#define STAR_SENSOR_PCA9685_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "soc/gpio_num.h"
#include "star_bus_manager.h"
#include "star_error_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_pca9685.h
 * @brief PCA9685 16-channel 12-bit PWM driver
 *
 * This driver provides a high-level interface to the PCA9685 PWM controller.
 * It supports:
 * - 16 independent PWM channels
 * - 12-bit resolution (4096 steps)
 * - Configurable frequency (24-1526 Hz)
 * - Phase-shifted outputs
 * - All-call and sub-address support
 * - Open-drain and totem-pole outputs
 */

/* --- Constants --- */

/** PCA9685 default I2C address */
#define PCA9685_DEFAULT_ADDR (0x40)

/** PCA9685 all-call address */
#define PCA9685_ALLCALL_ADDR (0x70)

/** Number of PWM channels */
#define PCA9685_NUM_CHANNELS (16)

/** PWM resolution (12-bit) */
#define PCA9685_PWM_RESOLUTION (4096)

/** Minimum PWM frequency (Hz) */
#define PCA9685_MIN_FREQ (24)

/** Maximum PWM frequency (Hz) */
#define PCA9685_MAX_FREQ (1526)

/** Internal oscillator frequency (Hz) */
#define PCA9685_OSCILLATOR_FREQ (25000000)

/* --- Register Map --- */

#define PCA9685_REG_MODE1 (0x00)         /**< Mode register 1 */
#define PCA9685_REG_MODE2 (0x01)         /**< Mode register 2 */
#define PCA9685_REG_SUBADR1 (0x02)       /**< I2C sub-address 1 */
#define PCA9685_REG_SUBADR2 (0x03)       /**< I2C sub-address 2 */
#define PCA9685_REG_SUBADR3 (0x04)       /**< I2C sub-address 3 */
#define PCA9685_REG_ALLCALLADR (0x05)    /**< All-call I2C address */
#define PCA9685_REG_LED0_ON_L (0x06)     /**< LED0 output ON time, low byte */
#define PCA9685_REG_LED0_ON_H (0x07)     /**< LED0 output ON time, high byte */
#define PCA9685_REG_LED0_OFF_L (0x08)    /**< LED0 output OFF time, low byte */
#define PCA9685_REG_LED0_OFF_H (0x09)    /**< LED0 output OFF time, high byte */
#define PCA9685_REG_ALL_LED_ON_L (0xFA)  /**< All LED ON time, low byte */
#define PCA9685_REG_ALL_LED_ON_H (0xFB)  /**< All LED ON time, high byte */
#define PCA9685_REG_ALL_LED_OFF_L (0xFC) /**< All LED OFF time, low byte */
#define PCA9685_REG_ALL_LED_OFF_H (0xFD) /**< All LED OFF time, high byte */
#define PCA9685_REG_PRESCALE (0xFE)      /**< Prescaler for PWM output frequency */

/* --- MODE1 Register Bits --- */

#define PCA9685_MODE1_RESTART (1 << 7) /**< Restart enabled */
#define PCA9685_MODE1_EXTCLK (1 << 6)  /**< Use external clock */
#define PCA9685_MODE1_AI (1 << 5)      /**< Auto-increment enabled */
#define PCA9685_MODE1_SLEEP (1 << 4)   /**< Low power mode */
#define PCA9685_MODE1_SUB1 (1 << 3)    /**< Respond to sub-address 1 */
#define PCA9685_MODE1_SUB2 (1 << 2)    /**< Respond to sub-address 2 */
#define PCA9685_MODE1_SUB3 (1 << 1)    /**< Respond to sub-address 3 */
#define PCA9685_MODE1_ALLCALL (1 << 0) /**< Respond to all-call address */

/* --- MODE2 Register Bits --- */

#define PCA9685_MODE2_INVRT (1 << 4)  /**< Output logic state inverted */
#define PCA9685_MODE2_OCH (1 << 3)    /**< Outputs change on STOP command */
#define PCA9685_MODE2_OUTDRV (1 << 2) /**< Totem pole (1) or open-drain (0) */
#define PCA9685_MODE2_OUTNE1 (1 << 1) /**< Output enable bit 1 */
#define PCA9685_MODE2_OUTNE0 (1 << 0) /**< Output enable bit 0 */

/* --- Special Values --- */

#define PCA9685_LED_FULL_ON (1 << 4)  /**< LED full ON (in ON_H register) */
#define PCA9685_LED_FULL_OFF (1 << 4) /**< LED full OFF (in OFF_H register) */

/* --- Types --- */

/**
 * @brief Output driver mode
 */
typedef enum {
  PCA9685_OUTPUT_OPEN_DRAIN = 0, /**< Open-drain (requires pull-up) */
  PCA9685_OUTPUT_TOTEM_POLE = 1  /**< Totem pole (push-pull) */
} pca9685_output_mode_t;

/**
 * @brief PCA9685 device configuration
 */
typedef struct {
  uint8_t               i2c_addr;      /**< I2C device address (default 0x40) */
  uint16_t              pwm_freq;      /**< PWM frequency in Hz (24-1526) */
  pca9685_output_mode_t output_mode;   /**< Output driver mode */
  bool                  ext_clock;     /**< Use external clock source */
  bool                  invert_output; /**< Invert output logic state */
} pca9685_config_t;

/**
 * @brief PWM channel configuration
 */
typedef struct {
  uint16_t on_time;  /**< ON time (0-4095) */
  uint16_t off_time; /**< OFF time (0-4095) */
} pca9685_pwm_t;

/**
 * @brief PCA9685 device handle
 *
 * Maintains state and error handling for PWM operations.
 * This structure should be initialized once and reused for all operations.
 * Thread-safe: All operations are protected by an internal mutex.
 */
typedef struct pca9685_handle {
  star_bus_manager_t* manager;  /**< Pointer to bus manager */
  const char*         bus_name; /**< Name of I2C bus for this device */
  uint8_t             i2c_addr; /**< I2C device address */
  pca9685_config_t    config;   /**< Device configuration */
  star_error_interface_t*
    error_iface;            /**< Injected error interface (NULL = default created internally) */
  SemaphoreHandle_t mutex;  /**< Mutex for thread-safe operations */
  gpio_num_t        oe_pin; /**< Output Enable pin (GPIO_NUM_NC if not used) */
  bool              oe_active_low;      /**< True if OE pin is active-low */
  bool              initialized;        /**< Initialization state */
  bool              owns_error_handler; /**< True if we created default error handler internally */
  uint8_t           prescale;           /**< Current prescale value */
} pca9685_handle_t;

/* --- Core Functions --- */

/**
 * @brief Create and initialize PCA9685 PWM controller handle
 *
 * Performs device detection, configuration, and initialization.
 * Creates a handle that maintains state and error tracking across operations.
 * Thread-safe: Creates an internal mutex for protecting handle state.
 *
 * @param[out] handle         Pointer to handle structure (must be allocated by caller)
 * @param[in]  manager        Pointer to initialized bus manager
 * @param[in]  bus_name       Name of I2C bus configured for this device
 * @param[in]  error_iface    Error interface for error handling (NULL = create default internally)
 * @param[in]  oe_pin         Output Enable GPIO pin (GPIO_NUM_NC if not used)
 * @param[in]  oe_active_low  True if OE pin is active-low (outputs enabled when pin is LOW)
 * @param[in]  config         Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note The handle must be deinitialized with star_sensor_pca9685_deinit() when done
 * @note If error_iface is NULL, a default error handler will be created internally
 * @note All operations after init are protected by mutex for thread safety
 */
esp_err_t star_sensor_pca9685_init(pca9685_handle_t*       handle,
                                   star_bus_manager_t*     manager,
                                   const char*             bus_name,
                                   star_error_interface_t* error_iface,
                                   gpio_num_t              oe_pin,
                                   bool                    oe_active_low,
                                   const pca9685_config_t* config);

/**
 * @brief Deinitialize PCA9685 PWM controller handle
 *
 * Puts device to sleep and cleans up resources.
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_pca9685_deinit(pca9685_handle_t* handle);

/**
 * @brief Software reset of PCA9685
 *
 * Resets device to power-on state using I2C software reset command.
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_pca9685_reset(const pca9685_handle_t* handle);

/* --- Frequency Configuration --- */

/**
 * @brief Set PWM frequency
 *
 * Frequency must be in range 24-1526 Hz. Device will be put to sleep
 * momentarily to change the frequency.
 *
 * @param[in] handle    Pointer to initialized handle
 * @param[in] freq_hz   Desired frequency in Hz (24-1526)
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if frequency out of range
 */
esp_err_t star_sensor_pca9685_set_frequency(pca9685_handle_t* handle, uint16_t freq_hz);

/**
 * @brief Get current PWM frequency
 *
 * @param[in]  handle   Pointer to initialized handle
 * @param[out] freq_hz  Current frequency in Hz
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_pca9685_get_frequency(const pca9685_handle_t* handle, uint16_t* freq_hz);

/* --- PWM Control Functions --- */

/**
 * @brief Set PWM duty cycle for a channel (0-100%)
 *
 * @param[in] handle      Pointer to initialized handle
 * @param[in] channel     Channel number (0-15)
 * @param[in] duty_percent Duty cycle in percent (0.0-100.0)
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if channel >= 16
 */
esp_err_t star_sensor_pca9685_set_duty_cycle(const pca9685_handle_t* handle,
                                             uint8_t                 channel,
                                             float                   duty_percent);

/**
 * @brief Set PWM values directly for a channel
 *
 * @param[in] handle   Pointer to initialized handle
 * @param[in] channel  Channel number (0-15)
 * @param[in] on_time  ON time (0-4095)
 * @param[in] off_time OFF time (0-4095)
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if parameters invalid
 *
 * @note on_time and off_time define phase and duty cycle:
 *       - duty = (off_time - on_time) / 4096
 *       - phase = on_time / 4096
 */
esp_err_t star_sensor_pca9685_set_pwm(const pca9685_handle_t* handle,
                                      uint8_t                 channel,
                                      uint16_t                on_time,
                                      uint16_t                off_time);

/**
 * @brief Set PWM duty cycle with phase shift
 *
 * @param[in] handle        Pointer to initialized handle
 * @param[in] channel       Channel number (0-15)
 * @param[in] duty_percent  Duty cycle in percent (0.0-100.0)
 * @param[in] phase_percent Phase shift in percent (0.0-100.0)
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if parameters invalid
 */
esp_err_t star_sensor_pca9685_set_duty_with_phase(const pca9685_handle_t* handle,
                                                  uint8_t                 channel,
                                                  float                   duty_percent,
                                                  float                   phase_percent);

/**
 * @brief Get PWM values for a channel
 *
 * @param[in]  handle   Pointer to initialized handle
 * @param[in]  channel  Channel number (0-15)
 * @param[out] on_time  ON time (0-4095)
 * @param[out] off_time OFF time (0-4095)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_pca9685_get_pwm(const pca9685_handle_t* handle,
                                      uint8_t                 channel,
                                      uint16_t*               on_time,
                                      uint16_t*               off_time);

/**
 * @brief Set channel to full ON (100% duty cycle, no PWM)
 *
 * @param[in] handle  Pointer to initialized handle
 * @param[in] channel Channel number (0-15)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_pca9685_set_channel_on(const pca9685_handle_t* handle, uint8_t channel);

/**
 * @brief Set channel to full OFF (0% duty cycle)
 *
 * @param[in] handle  Pointer to initialized handle
 * @param[in] channel Channel number (0-15)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_pca9685_set_channel_off(const pca9685_handle_t* handle, uint8_t channel);

/**
 * @brief Set all channels to same duty cycle
 *
 * @param[in] handle       Pointer to initialized handle
 * @param[in] duty_percent Duty cycle in percent (0.0-100.0)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_pca9685_set_all_duty_cycle(const pca9685_handle_t* handle,
                                                 float                   duty_percent);

/* --- Helper Functions --- */

/**
 * @brief Calculate prescale value for desired frequency
 *
 * @param[in]  freq_hz  Desired frequency in Hz (24-1526)
 * @param[out] prescale Calculated prescale value (3-255)
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if frequency out of range
 */
esp_err_t star_sensor_pca9685_calculate_prescale(uint16_t freq_hz, uint8_t* prescale);

/**
 * @brief Convert prescale value to frequency
 *
 * @param[in]  prescale Prescale value (3-255)
 * @param[out] freq_hz  Resulting frequency in Hz
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_pca9685_prescale_to_freq(uint8_t prescale, uint16_t* freq_hz);

/**
 * @brief Sleep/wake device
 *
 * @param[in] handle Pointer to initialized handle
 * @param[in] sleep  true to sleep, false to wake
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_pca9685_sleep(pca9685_handle_t* handle, bool sleep);

/* --- Output Enable Pin Control --- */

/**
 * @brief Enable PWM outputs via OE pin
 *
 * If OE pin was configured during init, this enables all PWM outputs.
 * For active-low OE pins, this sets the pin LOW.
 * For active-high OE pins, this sets the pin HIGH.
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if OE pin not configured
 *
 * @note This is a no-op if oe_pin was set to GPIO_NUM_NC during init
 */
esp_err_t star_sensor_pca9685_output_enable(const pca9685_handle_t* handle);

/**
 * @brief Disable PWM outputs via OE pin
 *
 * If OE pin was configured during init, this disables all PWM outputs.
 * For active-low OE pins, this sets the pin HIGH.
 * For active-high OE pins, this sets the pin LOW.
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if OE pin not configured
 *
 * @note This is a no-op if oe_pin was set to GPIO_NUM_NC during init
 */
esp_err_t star_sensor_pca9685_output_disable(const pca9685_handle_t* handle);

/* --- Servo Control Convenience Functions --- */

/**
 * @brief Set servo position by angle
 *
 * Convenience function that converts servo angle (0-180°) to PWM values
 * using the star_servo library and sets the channel output.
 *
 * Standard servo mapping:
 * - 0° = 1.0ms pulse width
 * - 90° = 1.5ms pulse width (center)
 * - 180° = 2.0ms pulse width
 *
 * @param[in] handle  Pointer to initialized handle
 * @param[in] channel Channel number (0-15)
 * @param[in] angle   Servo angle in degrees (0-180)
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if channel >= 16
 *
 * @note This function internally uses star_servo_angle_to_count() for conversion
 * @note Angles outside 0-180 range are automatically clamped
 */
esp_err_t
star_sensor_pca9685_set_servo_angle(const pca9685_handle_t* handle, uint8_t channel, uint8_t angle);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_PCA9685_H */
