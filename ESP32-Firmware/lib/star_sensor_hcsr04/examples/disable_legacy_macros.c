/* lib/star_sensor_hcsr04/examples/disable_legacy_macros.c */

/**
 * @file disable_legacy_macros.c
 * @brief Example demonstrating how to disable legacy macros
 * 
 * This example shows how to completely disable the deprecated macros
 * and use only the type-safe constants.
 */

/* Define this before including the header to disable legacy macros */
#define STAR_HCSR04_DISABLE_LEGACY_MACROS

#include <esp_log.h>

#include "star_sensor_hcsr04.h"
#include "star_sensor_hcsr04_constants.h"

static const char* TAG = "hcsr04_no_macros";

void use_only_typesafe_constants(void)
{
  /* This code will only compile with type-safe constants */
  /* The old HCSR04_* macros are not available when disabled */

  ESP_LOGI(TAG, "Using only type-safe constants:");
  ESP_LOGI(TAG, "  Min: %u cm", STAR_HCSR04_MIN_DISTANCE_CM);
  ESP_LOGI(TAG, "  Max: %u cm", STAR_HCSR04_MAX_DISTANCE_CM);

  /* The following would cause compilation errors if uncommented: */
  /* ESP_LOGI(TAG, "Old macro: %d", HCSR04_MIN_DISTANCE_CM); // ERROR! */
  /* ESP_LOGI(TAG, "Old macro: %d", HCSR04_MAX_DISTANCE_CM); // ERROR! */

  ESP_LOGI(TAG, "Legacy macros successfully disabled!");
}

void app_main(void)
{
  use_only_typesafe_constants();
}