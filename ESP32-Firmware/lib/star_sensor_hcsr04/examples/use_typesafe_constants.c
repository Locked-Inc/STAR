/* lib/star_sensor_hcsr04/examples/use_typesafe_constants.c */

/**
 * @file use_typesafe_constants.c
 * @brief Example demonstrating the use of type-safe constants for HC-SR04
 * 
 * This example shows how to use the new type-safe constants instead of
 * the deprecated macros when working with the HC-SR04 sensor.
 */

#include <esp_log.h>

#include "star_sensor_hcsr04.h"
#include "star_sensor_hcsr04_constants.h"

static const char* TAG = "hcsr04_example";

void validate_measurement(float distance_cm)
{
  /* Use type-safe constants for range checking */
  if (distance_cm < (float)STAR_HCSR04_MIN_DISTANCE_CM) {
    ESP_LOGW(TAG,
             "Distance %.2f cm is below minimum range of %u cm",
             distance_cm,
             STAR_HCSR04_MIN_DISTANCE_CM);
  } else if (distance_cm > (float)STAR_HCSR04_MAX_DISTANCE_CM) {
    ESP_LOGW(TAG,
             "Distance %.2f cm is above maximum range of %u cm",
             distance_cm,
             STAR_HCSR04_MAX_DISTANCE_CM);
  } else {
    ESP_LOGI(TAG, "Valid distance measurement: %.2f cm", distance_cm);
  }
}

float calculate_custom_timeout_distance(void)
{
  /* Calculate the maximum theoretical distance based on timeout */
  /* Distance = (timeout * speed_of_sound) / 2 */
  float max_distance = (STAR_HCSR04_TIMEOUT_US * STAR_HCSR04_SPEED_OF_SOUND_CM_US) / 2.0f;

  ESP_LOGI(TAG,
           "Maximum distance based on %u us timeout: %.2f cm",
           STAR_HCSR04_TIMEOUT_US,
           max_distance);

  return max_distance;
}

void display_sensor_specifications(void)
{
  ESP_LOGI(TAG, "HC-SR04 Sensor Specifications:");
  ESP_LOGI(TAG, "  Minimum Distance: %u cm", STAR_HCSR04_MIN_DISTANCE_CM);
  ESP_LOGI(TAG, "  Maximum Distance: %u cm", STAR_HCSR04_MAX_DISTANCE_CM);
  ESP_LOGI(TAG, "  Speed of Sound: %.4f cm/us", STAR_HCSR04_SPEED_OF_SOUND_CM_US);
  ESP_LOGI(TAG, "  Measurement Timeout: %u us", STAR_HCSR04_TIMEOUT_US);

  /* Calculate round-trip time for maximum distance */
  uint32_t round_trip_time =
    (uint32_t)((2.0f * STAR_HCSR04_MAX_DISTANCE_CM) / STAR_HCSR04_SPEED_OF_SOUND_CM_US);
  ESP_LOGI(TAG, "  Round-trip time for max distance: %u us", round_trip_time);
}

/* Example usage in application code */
void app_main(void)
{
  /* Display sensor specifications using type-safe constants */
  display_sensor_specifications();

  /* Calculate theoretical maximum */
  float theoretical_max = calculate_custom_timeout_distance();
  ESP_LOGI(TAG, "Theoretical maximum distance: %.2f cm", theoretical_max);

  /* Example distance validation */
  validate_measurement(1.5f);   /* Below minimum */
  validate_measurement(50.0f);  /* Valid range */
  validate_measurement(450.0f); /* Above maximum */
}