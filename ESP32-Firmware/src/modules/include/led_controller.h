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

/* ========== Constants ========== */

/* Color and PWM Constants */
static const float s_led_gamma_correction_factor =
  2.2f; /**< Gamma correction for perceptual brightness */
static const float s_led_normalize_percent = 100.0f; /**< Convert percentage to 0.0-1.0 range */
static const float s_led_rgb_max_percent   = 100.0f; /**< Maximum RGB color percentage */

/* HSV to RGB Conversion Constants */
static const float s_led_hue_max_degrees    = 360.0f; /**< Maximum hue value in degrees */
static const float s_led_hue_sector_size    = 60.0f;  /**< Size of each hue sector in degrees */
static const float s_led_hue_sector_double  = 2.0f;   /**< Double sector size for calculations */
static const float s_led_hsv_saturation_max = 1.0f;   /**< Maximum saturation value */
static const float s_led_hsv_value_max      = 1.0f;   /**< Maximum value (brightness) */

/* HSV Hue Sector Boundaries (degrees) */
static const float s_led_hue_sector_1 = 60.0f;  /**< Red to Yellow boundary */
static const float s_led_hue_sector_2 = 120.0f; /**< Yellow to Green boundary */
static const float s_led_hue_sector_3 = 180.0f; /**< Green to Cyan boundary */
static const float s_led_hue_sector_4 = 240.0f; /**< Cyan to Blue boundary */
static const float s_led_hue_sector_5 = 300.0f; /**< Blue to Magenta boundary */

/* Distance to Color Gradient Constants */
static const float s_led_distance_range_min_offset =
  1.0f; /**< Minimum range offset to prevent division by zero */
static const float s_led_distance_smooth_factor  = 3.0f;   /**< Exponential smoothing factor */
static const float s_led_gradient_max_hue        = 240.0f; /**< Blue hue (end of rainbow) */
static const float s_led_brightness_curve_factor = 3.0f;   /**< Brightness curve smoothing factor */
static const float s_led_brightness_power_factor = 2.0f;   /**< Power for brightness curve */
static const float s_led_close_boost_factor      = 0.3f;   /**< Boost factor for close objects */
static const float s_led_brightness_min          = 0.4f;   /**< Minimum brightness value */
static const float s_led_brightness_max          = 1.0f;   /**< Maximum brightness value */

/* LED Hardware Constants */
static const uint8_t s_led_channels_per_rgb     = 3; /**< Number of channels per RGB LED */
static const uint8_t s_led_channel_offset_red   = 0; /**< Red channel offset */
static const uint8_t s_led_channel_offset_green = 1; /**< Green channel offset */
static const uint8_t s_led_channel_offset_blue  = 2; /**< Blue channel offset */

/* ========== Data Types ========== */

typedef struct {
  float red;
  float green;
  float blue;
} rgb_color_t;

typedef struct {
  float hue;        /* 0.0 - 360.0 degrees */
  float saturation; /* 0.0 - 1.0 */
  float value;      /* 0.0 - 1.0 (brightness) */
} hsv_color_t;

typedef enum { k_led_index_left = 0, k_led_index_right = 1, k_led_index_max = 2 } led_index_t;

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
rgb_color_t led_controller_distance_to_color(const float distance_cm);

/**
 * @brief Set RGB LED color
 * 
 * @param controller LED controller handle
 * @param led_index LED index (left/right)
 * @param color RGB color to set
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_controller_set_rgb(led_controller_t*  controller,
                                 const led_index_t  led_index,
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
                                         const led_index_t led_index,
                                         const float       distance_cm);

/**
 * @brief Turn off specific LED
 * 
 * @param controller LED controller handle
 * @param led_index LED index (left/right)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_controller_turn_off(led_controller_t* controller, const led_index_t led_index);

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