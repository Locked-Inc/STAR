/* lib/star_bus/include/star_bus_gpio.h */

#ifndef STAR_COMPONENT_BUS_GPIO_H
#define STAR_COMPONENT_BUS_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"

#include <stdint.h>

#include "esp_err.h"
#include "star_bus_types.h"

/**
 * @file star_bus_gpio.h
 * @brief GPIO operations through the unified bus manager
 *
 * This module provides GPIO read/write and configuration operations through
 * the bus manager abstraction. It supports pin configuration, digital I/O,
 * and interrupt handling.
 *
 * Key Features:
 * - Pin mode configuration (input, output, open-drain)
 * - Pull-up/pull-down resistor configuration
 * - Digital read/write operations
 * - Interrupt configuration with ISR handlers
 * - Thread-safe through bus manager mutex
 *
 * Example Usage:
 * @code
 * // === Setup (see star_bus_manager.h for full setup) ===
 *
 * #include "star_bus_gpio.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // Create GPIO bus configuration
 * star_bus_config_t* gpio_bus = star_bus_config_create_gpio("gpio_bus");
 * star_bus_manager_add_bus(&bus_manager, gpio_bus);
 *
 *
 * // === Configuring Pins ===
 *
 * // Configure LED pin as output
 * esp_err_t ret = star_bus_gpio_configure(
 *     &bus_manager,
 *     "gpio_bus",
 *     GPIO_NUM_2,              // Built-in LED on many ESP32 boards
 *     GPIO_MODE_OUTPUT,        // Output mode
 *     GPIO_FLOATING            // No pull resistor needed for output
 * );
 *
 * // Configure button pin as input with pull-up
 * star_bus_gpio_configure(
 *     &bus_manager,
 *     "gpio_bus",
 *     GPIO_NUM_0,              // BOOT button on many ESP32 boards
 *     GPIO_MODE_INPUT,         // Input mode
 *     GPIO_PULLUP_ONLY         // Enable internal pull-up
 * );
 *
 * // Configure open-drain output (for I2C bit-banging, etc.)
 * star_bus_gpio_configure(
 *     &bus_manager,
 *     "gpio_bus",
 *     GPIO_NUM_4,
 *     GPIO_MODE_OUTPUT_OD,     // Open-drain output
 *     GPIO_PULLUP_ONLY         // External or internal pull-up required
 * );
 *
 *
 * // === Digital Write ===
 *
 * // Turn LED on
 * star_bus_gpio_write(&bus_manager, "gpio_bus", GPIO_NUM_2, 1);
 *
 * // Turn LED off
 * star_bus_gpio_write(&bus_manager, "gpio_bus", GPIO_NUM_2, 0);
 *
 * // Blink LED
 * for (int i = 0; i < 10; i++) {
 *     star_bus_gpio_write(&bus_manager, "gpio_bus", GPIO_NUM_2, 1);
 *     vTaskDelay(pdMS_TO_TICKS(500));
 *     star_bus_gpio_write(&bus_manager, "gpio_bus", GPIO_NUM_2, 0);
 *     vTaskDelay(pdMS_TO_TICKS(500));
 * }
 *
 *
 * // === Digital Read ===
 *
 * // Read button state
 * uint32_t button_level;
 * ret = star_bus_gpio_read(&bus_manager, "gpio_bus", GPIO_NUM_0, &button_level);
 * if (ret == ESP_OK) {
 *     if (button_level == 0) {
 *         ESP_LOGI(TAG, "Button pressed (active low)");
 *     } else {
 *         ESP_LOGI(TAG, "Button released");
 *     }
 * }
 *
 * // Poll for button press
 * while (1) {
 *     star_bus_gpio_read(&bus_manager, "gpio_bus", GPIO_NUM_0, &button_level);
 *     if (button_level == 0) {
 *         // Button pressed - toggle LED
 *         uint32_t led_state;
 *         star_bus_gpio_read(&bus_manager, "gpio_bus", GPIO_NUM_2, &led_state);
 *         star_bus_gpio_write(&bus_manager, "gpio_bus", GPIO_NUM_2, !led_state);
 *         vTaskDelay(pdMS_TO_TICKS(200));  // Debounce
 *     }
 *     vTaskDelay(pdMS_TO_TICKS(10));
 * }
 *
 *
 * // === Interrupt Handling ===
 *
 * // ISR handler for button press
 * static void IRAM_ATTR button_isr_handler(void* arg) {
 *     uint32_t* press_count = (uint32_t*)arg;
 *     (*press_count)++;
 *     // Note: Keep ISR minimal! Use a queue for complex processing.
 * }
 *
 * // Configure interrupt on falling edge (button press)
 * static uint32_t press_count = 0;
 * star_bus_gpio_set_interrupt(
 *     &bus_manager,
 *     "gpio_bus",
 *     GPIO_NUM_0,
 *     GPIO_INTR_NEGEDGE,       // Trigger on falling edge
 *     button_isr_handler,
 *     &press_count
 * );
 *
 * // Later: disable interrupt
 * star_bus_gpio_set_interrupt(
 *     &bus_manager,
 *     "gpio_bus",
 *     GPIO_NUM_0,
 *     GPIO_INTR_DISABLE,       // Disable interrupt
 *     NULL,                    // No handler
 *     NULL
 * );
 *
 *
 * // === Using with Sensors (e.g., HC-SR04 Ultrasonic) ===
 *
 * // Configure trigger pin (output)
 * star_bus_gpio_configure(&bus_manager, "gpio_bus", GPIO_NUM_5,
 *                         GPIO_MODE_OUTPUT, GPIO_FLOATING);
 *
 * // Configure echo pin (input)
 * star_bus_gpio_configure(&bus_manager, "gpio_bus", GPIO_NUM_18,
 *                         GPIO_MODE_INPUT, GPIO_FLOATING);
 *
 * // Send trigger pulse (10us)
 * star_bus_gpio_write(&bus_manager, "gpio_bus", GPIO_NUM_5, 0);
 * esp_rom_delay_us(2);
 * star_bus_gpio_write(&bus_manager, "gpio_bus", GPIO_NUM_5, 1);
 * esp_rom_delay_us(10);
 * star_bus_gpio_write(&bus_manager, "gpio_bus", GPIO_NUM_5, 0);
 *
 * // Measure echo pulse width (simplified - use ISR for accuracy)
 * uint32_t echo_level;
 * uint32_t start_time = esp_timer_get_time();
 * while (1) {
 *     star_bus_gpio_read(&bus_manager, "gpio_bus", GPIO_NUM_18, &echo_level);
 *     if (echo_level == 1) break;
 *     if (esp_timer_get_time() - start_time > 30000) break;  // Timeout
 * }
 *
 *
 * // === Multiple GPIO Buses (Different Configurations) ===
 *
 * // You can create multiple GPIO bus configs if needed for organization
 * star_bus_config_t* led_bus = star_bus_config_create_gpio("led_gpio");
 * star_bus_config_t* sensor_bus = star_bus_config_create_gpio("sensor_gpio");
 *
 * star_bus_manager_add_bus(&bus_manager, led_bus);
 * star_bus_manager_add_bus(&bus_manager, sensor_bus);
 *
 * // Use by name
 * star_bus_gpio_write(&bus_manager, "led_gpio", GPIO_NUM_2, 1);
 * star_bus_gpio_read(&bus_manager, "sensor_gpio", GPIO_NUM_4, &level);
 *
 *
 * // === Cleanup ===
 *
 * star_bus_manager_remove_bus(&bus_manager, "gpio_bus");
 * @endcode
 */

/* --- Public Functions --- */

/**
 * @brief Initialize default GPIO operations function pointers within a config structure.
 *        Typically called internally by star_bus_config_create_gpio.
 *        Assigns default implementations for configure, write, and read operations.
 *
 * @param[out] ops Pointer to GPIO operations structure to initialize. Must not be NULL.
 */
void star_bus_gpio_init_default_ops(star_gpio_ops_t* ops);

/**
 * @brief Configure a GPIO pin (set mode, pull-up/down) via bus manager.
 *
 * Finds the bus configuration by name and calls its configure operation.
 *
 * @param[in] manager Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name    Name of the GPIO bus configuration. Must not be NULL.
 * @param[in] pin     GPIO pin number to configure.
 * @param[in] mode    GPIO mode (input, output, etc.).
 * @param[in] pull    GPIO pull mode (pull-up, pull-down, floating).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid,
 *                   ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not
 * initialized, or an error from the driver.
 */
esp_err_t star_bus_gpio_configure(const star_bus_manager_t* manager,
                                  const char*               name,
                                  gpio_num_t                pin,
                                  gpio_mode_t               mode,
                                  gpio_pull_mode_t          pull);

/**
 * @brief Set the output level of a GPIO pin via bus manager.
 *
 * Finds the bus configuration by name and calls its write operation.
 *
 * @param[in] manager Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name    Name of the GPIO bus configuration. Must not be NULL.
 * @param[in] pin     GPIO pin number to write.
 * @param[in] level   Level to set (0 = low, 1 = high).
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid,
 *                   ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not
 * initialized, or an error from the driver.
 */
esp_err_t star_bus_gpio_write(const star_bus_manager_t* manager,
                              const char*               name,
                              gpio_num_t                pin,
                              uint32_t                  level);

/**
 * @brief Read the input level of a GPIO pin via bus manager.
 *
 * Finds the bus configuration by name and calls its read operation.
 *
 * @param[in]  manager Pointer to the initialized bus manager. Must not be NULL.
 * @param[in]  name    Name of the GPIO bus configuration. Must not be NULL.
 * @param[in]  pin     GPIO pin number to read.
 * @param[out] level   Pointer to store the read level (0 = low, 1 = high). Must not be NULL.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid,
 *                   ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not
 * initialized, or an error from the driver.
 */
esp_err_t star_bus_gpio_read(const star_bus_manager_t* manager,
                             const char*               name,
                             gpio_num_t                pin,
                             uint32_t*                 level);

/**
 * @brief Configure GPIO interrupt for a pin via bus manager.
 *
 * Finds the bus configuration by name and calls its set_interrupt operation.
 *
 * @param[in] manager     Pointer to the initialized bus manager. Must not be NULL.
 * @param[in] name        Name of the GPIO bus configuration. Must not be NULL.
 * @param[in] pin         GPIO pin number to configure interrupt for.
 * @param[in] intr_type   Interrupt type (rising edge, falling edge, etc.).
 * @param[in] isr_handler ISR handler function. Can be NULL to disable interrupt.
 * @param[in] args        Arguments to pass to ISR handler. Can be NULL.
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG if params invalid,
 *                   ESP_ERR_NOT_FOUND if bus not found, ESP_ERR_INVALID_STATE if bus not
 * initialized, or an error from the driver.
 */
esp_err_t star_bus_gpio_set_interrupt(const star_bus_manager_t* manager,
                                      const char*               name,
                                      gpio_num_t                pin,
                                      gpio_int_type_t           intr_type,
                                      gpio_isr_t                isr_handler,
                                      void*                     args);

#ifdef __cplusplus
}
#endif

#endif /* STAR_COMPONENT_BUS_GPIO_H */
