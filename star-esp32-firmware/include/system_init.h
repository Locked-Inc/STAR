/* include/system_init.h - System initialization API */

#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include "esp_err.h"

#include "system_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize entire system (buses, motors, encoders, sensors, PIDs)
 *
 * Performs complete system initialization in the following order:
 * 1. Error handler and pin validator
 * 2. Bus manager
 * 3. All buses (GPIO, ADC, I2C, 1-Wire, SPI peripheral)
 * 4. Pin validation
 * 5. Multiplexer GPIO control
 * 6. Motor drivers (4x star_motor)
 * 7. Shared encoder (multiplexed)
 * 8. PID controllers (4x)
 * 9. Sensors (DS18B20, BQ7850 BMS)
 * 10. System state and mutex
 *
 * @param[out] ctx System context to initialize. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t system_init(system_context_t* ctx);

/**
 * @brief Deinitialize system and release resources
 *
 * Performs cleanup in reverse order of initialization.
 * Stops all motors, releases hardware resources, and frees memory.
 *
 * @param[in] ctx System context to deinitialize. Must not be NULL.
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t system_deinit(system_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_INIT_H */
