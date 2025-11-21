/**
 * PCA9685 16-Channel PWM LED Controller Driver for ESP32-IDF
 *
 * Comprehensive C driver with full security and memory safety features
 * Implements complete register map and PWM control
 *
 * Author: Technical Research - Anthropic Claude Code
 * Version: 1.0
 * Date: November 2025
 *
 * License: MIT
 */

#ifndef PCA9685_H
#define PCA9685_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS AND DEFINITIONS
 * ============================================================================ */

// Device Configuration Constants
#define PCA9685_I2C_ADDR_BASE       0x40
#define PCA9685_NUM_CHANNELS        16
#define PCA9685_MAX_PWM_VALUE       0x0FFF  // 12-bit maximum
#define PCA9685_MIN_FREQUENCY_HZ    24
#define PCA9685_MAX_FREQUENCY_HZ    1526
#define PCA9685_OSC_FREQUENCY       25000000  // 25 MHz internal oscillator
#define PCA9685_DEFAULT_FREQUENCY   200  // Hz
#define PCA9685_PRESCALE_MIN        3
#define PCA9685_PRESCALE_MAX        255

// I2C Bus Timing
#define PCA9685_I2C_INIT_DELAY_MS   1  // Delay after register write
#define PCA9685_I2C_TIMEOUT_MS      1000

// Register Addresses
#define PCA9685_REG_MODE1           0x00
#define PCA9685_REG_MODE2           0x01
#define PCA9685_REG_SUBADR1         0x02
#define PCA9685_REG_SUBADR2         0x03
#define PCA9685_REG_SUBADR3         0x04
#define PCA9685_REG_ALLCALLADR      0x05
#define PCA9685_REG_LED0_ON_L       0x06
#define PCA9685_REG_LED0_ON_H       0x07
#define PCA9685_REG_LED0_OFF_L      0x08
#define PCA9685_REG_LED0_OFF_H      0x09
#define PCA9685_REG_PRE_SCALE       0xFE
#define PCA9685_REG_TESTMODE        0xFF

// All-Call LED Registers
#define PCA9685_REG_ALLLED_ON_L     0xFA
#define PCA9685_REG_ALLLED_ON_H     0xFB
#define PCA9685_REG_ALLLED_OFF_L    0xFC
#define PCA9685_REG_ALLLED_OFF_H    0xFD

// MODE1 Register Bit Definitions
#define PCA9685_MODE1_ALLCALL       (1 << 0)  // All Call address acknowledgment
#define PCA9685_MODE1_SUB1          (1 << 1)  // Subaddress 1 acknowledgment
#define PCA9685_MODE1_SUB2          (1 << 2)  // Subaddress 2 acknowledgment
#define PCA9685_MODE1_SUB3          (1 << 3)  // Subaddress 3 acknowledgment
#define PCA9685_MODE1_SLEEP         (1 << 4)  // Low power mode (0=normal, 1=sleep)
#define PCA9685_MODE1_AI            (1 << 5)  // Auto-increment (0=disabled, 1=enabled)
#define PCA9685_MODE1_EXTCLK        (1 << 6)  // External clock (0=internal, 1=external)
#define PCA9685_MODE1_RESTART       (1 << 7)  // Restart enable

// MODE2 Register Bit Definitions
#define PCA9685_MODE2_OUTNE_MASK    0x03  // Output enable mode (bits 1:0)
#define PCA9685_MODE2_OUTNE_LOW     0x00  // LED outputs off when OE=1
#define PCA9685_MODE2_OUTNE_PWM     0x01  // LED outputs follow PWM when OE=1
#define PCA9685_MODE2_OUTNE_HIGH    0x02  // LED outputs on when OE=1
#define PCA9685_MODE2_OUTDRV        (1 << 2)  // Output driver (0=open-drain, 1=totem-pole)
#define PCA9685_MODE2_INVRT         (1 << 4)  // Output inversion (0=normal, 1=inverted)

// Special PWM Values
#define PCA9685_LED_FULL_ON         0x1000  // Special value for full ON (bit 4 of ON_H)
#define PCA9685_LED_FULL_OFF        0x1000  // Special value for full OFF (bit 4 of OFF_H)

// Special I2C Addresses
#define PCA9685_ALLCALL_ADDR        0xE0    // All Call address (broadcast)
#define PCA9685_SUBADR1_ADDR        0xE2    // Subaddress 1
#define PCA9685_SUBADR2_ADDR        0xE4    // Subaddress 2
#define PCA9685_SUBADR3_ADDR        0xE8    // Subaddress 3

/* ============================================================================
 * TYPE DEFINITIONS
 * ============================================================================ */

/**
 * PCA9685 Device Handle
 *
 * Contains device state and configuration
 */
typedef struct {
    i2c_port_t i2c_port;           ///< I2C port number (I2C_NUM_0, I2C_NUM_1)
    uint8_t address;               ///< 7-bit I2C slave address
    uint16_t current_frequency;    ///< Current PWM frequency in Hz
    bool initialized;              ///< Initialization state flag
    uint8_t mode1_shadow;          ///< Cached MODE1 register value
    uint8_t mode2_shadow;          ///< Cached MODE2 register value
    uint32_t error_count;          ///< I2C error counter
    uint32_t last_error_time;      ///< Timestamp of last error
} pca9685_t;

/**
 * PWM Output Configuration Structure
 *
 * Represents a single PWM output setting
 */
typedef struct {
    uint8_t channel;    ///< Channel number (0-15)
    uint16_t on_time;   ///< ON counter value (0x000-0x0FFF)
    uint16_t off_time;  ///< OFF counter value (0x000-0x0FFF)
    uint16_t frequency; ///< Operating frequency (Hz)
} pca9685_pwm_t;

/**
 * Enumeration for PWM Channels
 */
typedef enum {
    PCA9685_CHANNEL_0 = 0,
    PCA9685_CHANNEL_1,
    PCA9685_CHANNEL_2,
    PCA9685_CHANNEL_3,
    PCA9685_CHANNEL_4,
    PCA9685_CHANNEL_5,
    PCA9685_CHANNEL_6,
    PCA9685_CHANNEL_7,
    PCA9685_CHANNEL_8,
    PCA9685_CHANNEL_9,
    PCA9685_CHANNEL_10,
    PCA9685_CHANNEL_11,
    PCA9685_CHANNEL_12,
    PCA9685_CHANNEL_13,
    PCA9685_CHANNEL_14,
    PCA9685_CHANNEL_15,
    PCA9685_CHANNEL_ALL = 0xFF  // Special value for all channels
} pca9685_channel_t;

/**
 * Output Driver Mode Selection
 */
typedef enum {
    PCA9685_OUTDRV_OPEN_DRAIN = 0,  // Open-drain output (default)
    PCA9685_OUTDRV_TOTEM_POLE = 1   // Totem-pole output (push-pull)
} pca9685_outdrv_mode_t;

/* ============================================================================
 * PUBLIC API FUNCTIONS
 * ============================================================================ */

/**
 * Initialize PCA9685 Device
 *
 * Configures I2C communication and sets default operation modes
 *
 * @param dev          Pointer to device handle (must not be NULL)
 * @param i2c_port     I2C port number (I2C_NUM_0, I2C_NUM_1)
 * @param address      7-bit I2C slave address (0x40-0x7E)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if parameters are invalid
 * @return ESP_ERR_NOT_FOUND if device not responding at address
 *
 * @note Blocks for approximately 3 milliseconds during initialization
 * @note Enables auto-increment for efficient multi-register writes
 * @note Sets default 200 Hz PWM frequency
 */
esp_err_t pca9685_init(pca9685_t *dev, i2c_port_t i2c_port, uint8_t address);

/**
 * Deinitialize PCA9685 Device
 *
 * Safely shuts down device and releases resources
 *
 * @param dev  Pointer to device handle
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if device pointer is NULL
 *
 * @note Sets all LED outputs to OFF state
 */
esp_err_t pca9685_deinit(pca9685_t *dev);

/**
 * Set PWM Frequency
 *
 * Changes the PWM output frequency by adjusting the prescaler
 *
 * @param dev           Pointer to initialized device handle
 * @param frequency_hz  Desired frequency in Hz (24-1526)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if device not initialized
 * @return ESP_ERR_INVALID_ARG if frequency out of range
 *
 * @note Frequency will be clamped to valid range [24, 1526] Hz
 * @note Blocks for approximately 3 milliseconds during write
 * @note Actual frequency may differ slightly due to prescaler resolution
 * @note Use pca9685_get_actual_frequency() to verify result
 *
 * @warning Modifies SLEEP bit in MODE1 register; brief output pause expected
 */
esp_err_t pca9685_set_frequency(pca9685_t *dev, uint16_t frequency_hz);

/**
 * Get Current PWM Frequency
 *
 * Returns the previously configured frequency (not read from device)
 *
 * @param dev  Pointer to initialized device handle
 *
 * @return Frequency in Hz (24-1526), 0 if device not initialized
 */
uint16_t pca9685_get_frequency(const pca9685_t *dev);

/**
 * Get Actual PWM Frequency
 *
 * Calculates the actual frequency based on current prescaler value
 *
 * @param dev        Pointer to initialized device handle
 * @param freq_hz    Pointer to store calculated frequency (must not be NULL)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if device not initialized
 * @return ESP_ERR_INVALID_ARG if freq_hz is NULL
 */
esp_err_t pca9685_get_actual_frequency(pca9685_t *dev, float *freq_hz);

/**
 * Set PWM Output on Single Channel
 *
 * Configures ON and OFF counter values for specified channel
 *
 * @param dev     Pointer to initialized device handle
 * @param channel Channel number (0-15)
 * @param on      ON counter value (0x000-0x0FFF)
 * @param off     OFF counter value (0x000-0x0FFF)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if device not initialized
 * @return ESP_ERR_INVALID_ARG if parameters invalid
 *
 * @note Duty cycle = (off - on) / 4096 * 100%
 * @note CRITICAL: on and off MUST be different (hardware limitation)
 * @note Values automatically split across 2 x 8-bit registers
 *
 * Example - 50% duty cycle:
 *   pca9685_set_pwm(dev, 0, 0, 2048);
 *
 * Example - 25% duty cycle with phase shift:
 *   pca9685_set_pwm(dev, 0, 512, 1536);
 */
esp_err_t pca9685_set_pwm(pca9685_t *dev, uint8_t channel, uint16_t on, uint16_t off);

/**
 * Set PWM Output Using Pulse Width in Microseconds
 *
 * Converts pulse width specification to ON/OFF register values
 * Useful for servo control applications
 *
 * @param dev      Pointer to initialized device handle
 * @param channel  Channel number (0-15)
 * @param pulse_us Pulse width in microseconds
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if device not initialized
 * @return ESP_ERR_INVALID_ARG if parameters invalid
 *
 * @note Pulse width must be less than PWM period (1000000 / frequency)
 * @note Automatically calculates ON=0, OFF=(pulse_us * 4096 / period_us)
 *
 * Example - 50 Hz servo, 1.5ms pulse (center):
 *   pca9685_set_frequency(dev, 50);
 *   pca9685_set_pwm_pulse(dev, 0, 1500);  // 1.5 milliseconds
 */
esp_err_t pca9685_set_pwm_pulse(pca9685_t *dev, uint8_t channel, uint16_t pulse_us);

/**
 * Read PWM Output Configuration
 *
 * Retrieves ON and OFF counter values from specified channel
 *
 * @param dev     Pointer to initialized device handle
 * @param channel Channel number (0-15)
 * @param on      Pointer to store ON value (must not be NULL)
 * @param off     Pointer to store OFF value (must not be NULL)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if device not initialized
 * @return ESP_ERR_INVALID_ARG if parameters invalid
 */
esp_err_t pca9685_get_pwm(pca9685_t *dev, uint8_t channel, uint16_t *on, uint16_t *off);

/**
 * Set Output Driver Mode
 *
 * Configures all LED outputs as open-drain or totem-pole
 *
 * @param dev   Pointer to initialized device handle
 * @param mode  Output mode (OPEN_DRAIN or TOTEM_POLE)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if device not initialized
 *
 * @note OPEN_DRAIN: 25 mA sink only (for active-high LED control)
 * @note TOTEM_POLE: 10 mA source, 25 mA sink (for direct MOSFET drive)
 */
esp_err_t pca9685_set_output_driver(pca9685_t *dev, pca9685_outdrv_mode_t mode);

/**
 * Invert PWM Output
 *
 * Inverts logic level of all PWM outputs
 * (PWM high → output low, and vice versa)
 *
 * @param dev      Pointer to initialized device handle
 * @param inverted Enable (true) or disable (false) inversion
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if device not initialized
 */
esp_err_t pca9685_set_output_inverted(pca9685_t *dev, bool inverted);

/**
 * Enable Sleep Mode
 *
 * Places device in low-power sleep mode (disables oscillator)
 *
 * @param dev     Pointer to initialized device handle
 * @param enabled Enable (true) or disable (false) sleep mode
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if device not initialized
 *
 * @warning Device must be awakened before PWM output resumes
 */
esp_err_t pca9685_set_sleep_mode(pca9685_t *dev, bool enabled);

/**
 * Enable/Disable All Call Address Acknowledgment
 *
 * When enabled, device responds to broadcast ALL_CALL address (0xE0)
 *
 * @param dev     Pointer to initialized device handle
 * @param enabled Enable (true) or disable (false) all call
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if device not initialized
 *
 * @warning If enabled, device will respond to all PCA9685 broadcast commands
 */
esp_err_t pca9685_set_allcall_address(pca9685_t *dev, bool enabled);

/**
 * Set All Channels to Same PWM Configuration
 *
 * Simultaneously configures all 16 channels to same ON/OFF values
 *
 * @param dev Pointer to initialized device handle
 * @param on  ON counter value (0x000-0x0FFF)
 * @param off OFF counter value (0x000-0x0FFF)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if device not initialized
 * @return ESP_ERR_INVALID_ARG if parameters invalid
 *
 * @note More efficient than writing 16 individual channels
 * @note Uses ALL_LED registers (0xFA-0xFD)
 */
esp_err_t pca9685_set_all_pwm(pca9685_t *dev, uint16_t on, uint16_t off);

/**
 * Verify Device Presence and Integrity
 *
 * Tests I2C communication and validates device response
 *
 * @param dev Pointer to initialized device handle
 *
 * @return ESP_OK if device responds correctly
 * @return ESP_ERR_NOT_FOUND if device not responding
 * @return ESP_ERR_INVALID_RESPONSE if device response invalid
 *
 * @note Does not modify device state
 */
esp_err_t pca9685_verify_device(pca9685_t *dev);

/**
 * Get Device Error Statistics
 *
 * Returns count of I2C errors since device initialization
 *
 * @param dev   Pointer to initialized device handle
 * @param count Pointer to store error count (must not be NULL)
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if count is NULL
 */
esp_err_t pca9685_get_error_count(const pca9685_t *dev, uint32_t *count);

/* ============================================================================
 * INTERNAL FUNCTIONS (Advanced Users Only)
 * ============================================================================ */

/**
 * Write Single Register
 *
 * Low-level register write via I2C
 *
 * @param dev   Device handle
 * @param reg   Register address
 * @param value Value to write
 *
 * @return ESP_OK on success, ESP_FAIL on I2C error
 *
 * @warning Does not validate register address
 */
esp_err_t pca9685_write_register(pca9685_t *dev, uint8_t reg, uint8_t value);

/**
 * Read Single Register
 *
 * Low-level register read via I2C
 *
 * @param dev   Device handle
 * @param reg   Register address
 * @param value Pointer to store read value
 *
 * @return ESP_OK on success, ESP_FAIL on I2C error
 *
 * @warning Does not validate register address
 */
esp_err_t pca9685_read_register(pca9685_t *dev, uint8_t reg, uint8_t *value);

/**
 * Write Burst of Registers
 *
 * Low-level multi-register write via I2C (requires AI bit set)
 *
 * @param dev        Device handle
 * @param start_reg  Starting register address
 * @param data       Pointer to data buffer
 * @param len        Number of bytes to write
 *
 * @return ESP_OK on success, ESP_FAIL on I2C error
 *
 * @note Requires MODE1 AI bit (auto-increment) to be enabled
 */
esp_err_t pca9685_write_registers_burst(pca9685_t *dev, uint8_t start_reg,
                                        const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // PCA9685_H
