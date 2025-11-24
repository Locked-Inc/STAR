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
