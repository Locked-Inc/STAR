/* esp32-firmware/components/star_pin_validator/include/star_pin_validator.h */

#ifndef STAR_PIN_VALIDATOR_H
#define STAR_PIN_VALIDATOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "soc/gpio_num.h"

#define PIN_VALIDATOR_DESC_MAX_LEN (64)

/**
 * @brief Info for each GPIO pin
 */
typedef struct {
  bool    can_be_shared; /* Whether this pin can be shared */
  uint8_t usage_count;   /* Number of components using this pin */
  char**  users;         /* Dynamically allocated array of descriptions */
} star_pin_info_t;

/**
 * @brief The main validator containing all pin usage information
 */
typedef struct {
  star_pin_info_t   pins[GPIO_NUM_MAX]; /* Every pin on the MCU */
  bool              initialized;        /* Whether the validator has been initialized */
  SemaphoreHandle_t mutex;              /* Mutex for thread safety */
} star_pin_validator_t;

/**
 * @brief Register a pin on the validator
 * @param gpio_num GPIO pin number
 * @param desc Description of what's using this pin
 * @param can_be_shared Whether this pin can be shared with other users
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t star_register_pin(gpio_num_t gpio_num, const char* desc, bool can_be_shared);

/**
 * @brief Unregister a pin from the validator
 * @param gpio_num GPIO pin number
 * @param desc Description that was used when registering (must match)
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t star_unregister_pin(gpio_num_t gpio_num, const char* desc);

/**
 * @brief Validate all registered pins for conflicts
 * @return ESP_OK if no conflicts, ESP_ERR_INVALID_STATE if conflicts found
 */
esp_err_t star_validate_pins(void);

/**
 * @brief Free the pin validator resources
 * @return ESP_OK if successful, otherwise an error code
 */
esp_err_t star_free_pin_validator(void);

#endif /* STAR_PIN_VALIDATOR_H */
