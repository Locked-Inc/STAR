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
 *
 * Example Usage:
 * @code
 * // === Basic Setup ===
 *
 * #include "star_sensor_pca9685.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // 1. Setup bus manager (see star_bus_manager.h for full setup)
 * star_bus_manager_t bus_manager;
 * star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 *
 * // 2. Create I2C bus for PCA9685
 * star_bus_config_t* pwm_bus = star_bus_config_create_i2c(
 *     "pwm_i2c",
 *     I2C_NUM_0,
 *     PCA9685_DEFAULT_ADDR,   // 0x40
 *     GPIO_NUM_21,            // SDA
 *     GPIO_NUM_22,            // SCL
 *     400000                  // 400kHz
 * );
 * star_bus_manager_add_bus(&bus_manager, pwm_bus);
 *
 * // 3. Configure and initialize PCA9685
 * pca9685_handle_t pwm;
 * pca9685_config_t config = {
 *     .i2c_addr = PCA9685_DEFAULT_ADDR,
 *     .pwm_freq = 50,                      // 50Hz for servos
 *     .output_mode = PCA9685_OUTPUT_TOTEM_POLE,
 *     .ext_clock = false,
 *     .invert_output = false
 * };
 *
 * esp_err_t ret = star_sensor_pca9685_init(
 *     &pwm,
 *     &bus_manager,
 *     "pwm_i2c",
 *     NULL,               // Use default error handler
 *     GPIO_NUM_NC,        // No OE pin (or use GPIO_NUM_X)
 *     false,              // OE active low
 *     &config
 * );
 *
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init PCA9685: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === LED Dimming (Duty Cycle) ===
 *
 * // Set LED brightness as duty cycle percentage
 * star_sensor_pca9685_set_duty_cycle(&pwm, 0, 50.0f);   // Channel 0 at 50%
 * star_sensor_pca9685_set_duty_cycle(&pwm, 1, 25.0f);   // Channel 1 at 25%
 * star_sensor_pca9685_set_duty_cycle(&pwm, 2, 100.0f);  // Channel 2 at 100%
 *
 * // Fade LED
 * for (float duty = 0; duty <= 100; duty += 1) {
 *     star_sensor_pca9685_set_duty_cycle(&pwm, 0, duty);
 *     vTaskDelay(pdMS_TO_TICKS(10));
 * }
 *
 * // Turn channel fully on/off
 * star_sensor_pca9685_set_channel_on(&pwm, 3);   // 100% on
 * star_sensor_pca9685_set_channel_off(&pwm, 4);  // 0% off
 *
 * // Set all channels at once
 * star_sensor_pca9685_set_all_duty_cycle(&pwm, 75.0f);  // All at 75%
 *
 *
 * // === Direct PWM Control ===
 *
 * // Set raw PWM on/off times (0-4095)
 * star_sensor_pca9685_set_pwm(&pwm, 5, 0, 2048);     // 50% duty, starts at 0
 * star_sensor_pca9685_set_pwm(&pwm, 6, 1024, 3072);  // 50% duty, phase shifted
 *
 * // Get current PWM values
 * uint16_t on_time, off_time;
 * star_sensor_pca9685_get_pwm(&pwm, 5, &on_time, &off_time);
 * ESP_LOGI(TAG, "Channel 5: ON=%d, OFF=%d", on_time, off_time);
 *
 *
 * // === Phase-Shifted PWM ===
 *
 * // Useful for reducing power supply spikes with multiple motors
 * star_sensor_pca9685_set_duty_with_phase(&pwm, 0, 50.0f, 0.0f);    // No phase shift
 * star_sensor_pca9685_set_duty_with_phase(&pwm, 1, 50.0f, 25.0f);   // 25% phase shift
 * star_sensor_pca9685_set_duty_with_phase(&pwm, 2, 50.0f, 50.0f);   // 50% phase shift
 * star_sensor_pca9685_set_duty_with_phase(&pwm, 3, 50.0f, 75.0f);   // 75% phase shift
 *
 *
 * // === Servo Control ===
 *
 * // Set frequency for servos (50Hz typical)
 * star_sensor_pca9685_set_frequency(&pwm, 50);
 *
 * // Control servo by angle (0-180 degrees)
 * star_sensor_pca9685_set_servo_angle(&pwm, 0, 90);   // Center position
 * star_sensor_pca9685_set_servo_angle(&pwm, 0, 0);    // Min position
 * star_sensor_pca9685_set_servo_angle(&pwm, 0, 180);  // Max position
 *
 * // Sweep servo
 * for (int angle = 0; angle <= 180; angle++) {
 *     star_sensor_pca9685_set_servo_angle(&pwm, 0, angle);
 *     vTaskDelay(pdMS_TO_TICKS(15));
 * }
 *
 *
 * // === Frequency Configuration ===
 *
 * // Set PWM frequency (24-1526 Hz)
 * star_sensor_pca9685_set_frequency(&pwm, 1000);  // 1kHz for LEDs
 *
 * // Get current frequency
 * uint16_t freq_hz;
 * star_sensor_pca9685_get_frequency(&pwm, &freq_hz);
 * ESP_LOGI(TAG, "Current frequency: %d Hz", freq_hz);
 *
 * // Calculate prescale for custom frequency
 * uint8_t prescale;
 * star_sensor_pca9685_calculate_prescale(200, &prescale);
 * ESP_LOGI(TAG, "Prescale for 200Hz: %d", prescale);
 *
 *
 * // === Output Enable Pin Control ===
 *
 * // If OE pin was configured during init:
 * star_sensor_pca9685_output_enable(&pwm);   // Enable all outputs
 * star_sensor_pca9685_output_disable(&pwm);  // Disable all outputs (safe state)
 *
 *
 * // === Power Management ===
 *
 * // Put device to sleep (low power)
 * star_sensor_pca9685_sleep(&pwm, true);
 *
 * // Wake up
 * star_sensor_pca9685_sleep(&pwm, false);
 *
 * // Software reset
 * star_sensor_pca9685_reset(&pwm);
 *
 *
 * // === Multiple PCA9685 Boards ===
 *
 * // Use different I2C addresses (set via A0-A5 pins on board)
 * star_bus_config_t* pwm_bus2 = star_bus_config_create_i2c(
 *     "pwm2_i2c", I2C_NUM_0, 0x41, GPIO_NUM_21, GPIO_NUM_22, 400000);
 * star_bus_manager_add_bus(&bus_manager, pwm_bus2);
 *
 * pca9685_handle_t pwm2;
 * pca9685_config_t config2 = config;
 * config2.i2c_addr = 0x41;  // Different address
 *
 * star_sensor_pca9685_init(&pwm2, &bus_manager, "pwm2_i2c", NULL, GPIO_NUM_NC, false, &config2);
 *
 * // Now you have 32 PWM channels total!
 * star_sensor_pca9685_set_duty_cycle(&pwm, 0, 50.0f);   // First board
 * star_sensor_pca9685_set_duty_cycle(&pwm2, 0, 50.0f);  // Second board
 *
 *
 * // === Cleanup ===
 *
 * star_sensor_pca9685_deinit(&pwm);
 * star_bus_manager_remove_bus(&bus_manager, "pwm_i2c");
 * @endcode
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
