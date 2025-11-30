#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <esp_err.h>
#include <stdbool.h>

#include "../../include/system_config.h"
#include "star_sensor_pca9685.h"

/*
 * LED Controller Module
 * Provides high-level abstraction for distance-based LED color control
 * Follows STAR framework patterns with proper error handling
 */

/* ========== Data Types ========== */

typedef struct {
  float red;
  float green;
  float blue;
} rgb_color_t;

typedef struct {
  float hue;        // 0.0 - 360.0 degrees
  float saturation; // 0.0 - 1.0
  float value;      // 0.0 - 1.0 (brightness)
} hsv_color_t;

typedef enum { LED_INDEX_LEFT = 0, LED_INDEX_RIGHT = 1, LED_INDEX_MAX = STAR_SYSTEM_NUM_HCSR04 } led_index_t;

typedef struct {
  const pca9685_handle_t* pca_handle;
  bool                    initialized;
  SemaphoreHandle_t       mutex;
} led_controller_t;

/* ========== Public API ========== */

/**
 * @brief Initialize LED controller
 * 
 * @param controller LED controller handle
 * @param pca_handle Initialized PCA9685 handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_controller_init(led_controller_t* controller, const pca9685_handle_t* pca_handle);

/**
 * @brief Deinitialize LED controller
 * 
 * @param controller LED controller handle
 */
void led_controller_deinit(led_controller_t* controller);

/**
 * @brief Convert HSV color to RGB color
 * 
 * @param hsv HSV color values
 * @return rgb_color_t Corresponding RGB color values (0-100%)
 */
rgb_color_t led_controller_hsv_to_rgb(const hsv_color_t* hsv);

/**
 * @brief Convert distance measurement to RGB color using smooth HSV gradient
 * 
 * @param distance_cm Distance in centimeters
 * @return rgb_color_t Corresponding RGB color values (0-100%)
 */
rgb_color_t led_controller_distance_to_color(float distance_cm);

/**
 * @brief Set RGB LED color
 * 
 * @param controller LED controller handle
 * @param led_index LED index (left/right)
 * @param color RGB color to set
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_controller_set_rgb(led_controller_t*  controller,
                                 led_index_t        led_index,
                                 const rgb_color_t* color);

/**
 * @brief Update LED based on distance measurement
 * 
 * @param controller LED controller handle
 * @param led_index LED index (left/right)
 * @param distance_cm Distance measurement in cm
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_controller_update_distance(led_controller_t* controller,
                                         led_index_t       led_index,
                                         float             distance_cm);

/**
 * @brief Turn off specific LED
 * 
 * @param controller LED controller handle
 * @param led_index LED index (left/right)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_controller_turn_off(led_controller_t* controller, led_index_t led_index);

/**
 * @brief Turn off all LEDs
 * 
 * @param controller LED controller handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_controller_turn_off_all(led_controller_t* controller);

/**
 * @brief Check if LED controller is properly initialized
 * 
 * @param controller LED controller handle
 * @return true if initialized and ready for use
 */
bool led_controller_is_ready(const led_controller_t* controller);

#endif /* LED_CONTROLLER_H */