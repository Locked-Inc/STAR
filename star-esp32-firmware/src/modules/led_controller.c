#include "include/led_controller.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <string.h>

static const char* s_TAG = "LED_CTRL"; /* Use STAR_SYSTEM_CONFIG.log_tags.led_ctrl */

/* ========== Private Functions ========== */

static float internal_scale_pwm_for_color(const float original_percent, const float min_pwm)
{
  /* Ultra-smooth PWM scaling with no discrete jumps */

  /* Use an extremely low threshold for near-seamless transitions */
  if (original_percent <= STAR_SYSTEM_CONFIG.pwm.off_threshold) {
    return 0.0f; /* True off state */
  }

  /* Smooth exponential scaling to avoid linear "steps" */
  /* This creates a more natural brightness perception curve */
  const float normalized = original_percent / s_led_normalize_percent;

  /* Apply gamma correction for perceptually linear brightness */
  /* Human eye perceives brightness non-linearly */
  const float gamma_corrected = powf(normalized, s_led_gamma_correction_factor);

  /* Map to safe PWM range with smooth interpolation */
  const float pwm_range = STAR_SYSTEM_CONFIG.pwm.max_pwm_all - min_pwm;
  float       scaled    = min_pwm + (gamma_corrected * pwm_range);

  /* Ensure we stay within safe bounds */
  scaled = fmaxf(min_pwm, fminf(STAR_SYSTEM_CONFIG.pwm.max_pwm_all, scaled));

  /* Enhanced debug logging for PWM analysis */
  ESP_LOGI(s_TAG,
           "PWM: %.3f%% -> γ:%.3f -> %.2f%% (range: %.1f-%.1f)",
           original_percent,
           gamma_corrected,
           scaled,
           min_pwm,
           STAR_SYSTEM_CONFIG.pwm.max_pwm_all);

  return scaled;
}

static esp_err_t internal_set_rgb_channels(const pca9685_handle_t* pca_handle,
                                           led_index_t             led_index,
                                           const rgb_color_t*      color)
{
  if (!star_validate_pwm_percent(color->red) || !star_validate_pwm_percent(color->green) ||
      !star_validate_pwm_percent(color->blue)) {
    ESP_LOGE(s_TAG,
             "Invalid PWM values: R=%.1f G=%.1f B=%.1f",
             color->red,
             color->green,
             color->blue);
    return ESP_ERR_INVALID_ARG;
  }

  const uint8_t base_channel  = led_index * s_led_channels_per_rgb;
  const uint8_t red_channel   = base_channel + s_led_channel_offset_red;
  const uint8_t green_channel = base_channel + s_led_channel_offset_green;
  const uint8_t blue_channel  = base_channel + s_led_channel_offset_blue;

  const float scaled_red =
    internal_scale_pwm_for_color(color->red, STAR_SYSTEM_CONFIG.pwm.min_pwm_red);
  const float scaled_green =
    internal_scale_pwm_for_color(color->green, STAR_SYSTEM_CONFIG.pwm.min_pwm_green);
  const float scaled_blue =
    internal_scale_pwm_for_color(color->blue, STAR_SYSTEM_CONFIG.pwm.min_pwm_blue);

  esp_err_t ret = ESP_OK;

  ret |= star_sensor_pca9685_set_duty_cycle(pca_handle, red_channel, scaled_red);
  ret |= star_sensor_pca9685_set_duty_cycle(pca_handle, green_channel, scaled_green);
  ret |= star_sensor_pca9685_set_duty_cycle(pca_handle, blue_channel, scaled_blue);

  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set PWM channels for LED %d", led_index);
  }

  return ret;
}

/* ========== Color Conversion Functions ========== */

rgb_color_t led_controller_hsv_to_rgb(const hsv_color_t* hsv)
{
  rgb_color_t rgb = {0.0f, 0.0f, 0.0f};

  if (hsv == NULL || hsv->saturation < 0.0f || hsv->saturation > s_led_hsv_saturation_max ||
      hsv->value < 0.0f || hsv->value > s_led_hsv_value_max) {
    return rgb;
  }

  float hue = fmodf(hsv->hue, s_led_hue_max_degrees);
  if (hue < 0.0f)
    hue += s_led_hue_max_degrees;

  const float c = hsv->value * hsv->saturation;
  const float x =
    c *
    (s_led_hsv_saturation_max -
     fabsf(fmodf(hue / s_led_hue_sector_size, s_led_hue_sector_double) - s_led_hsv_saturation_max));
  const float m = hsv->value - c;

  float r_prime, g_prime, b_prime;

  if (hue < s_led_hue_sector_1) {
    r_prime = c;
    g_prime = x;
    b_prime = 0.0f;
  } else if (hue < s_led_hue_sector_2) {
    r_prime = x;
    g_prime = c;
    b_prime = 0.0f;
  } else if (hue < s_led_hue_sector_3) {
    r_prime = 0.0f;
    g_prime = c;
    b_prime = x;
  } else if (hue < s_led_hue_sector_4) {
    r_prime = 0.0f;
    g_prime = x;
    b_prime = c;
  } else if (hue < s_led_hue_sector_5) {
    r_prime = x;
    g_prime = 0.0f;
    b_prime = c;
  } else {
    r_prime = c;
    g_prime = 0.0f;
    b_prime = x;
  }

  rgb.red   = (r_prime + m) * s_led_rgb_max_percent;
  rgb.green = (g_prime + m) * s_led_rgb_max_percent;
  rgb.blue  = (b_prime + m) * s_led_rgb_max_percent;

  return rgb;
}

/* ========== Public API Implementation ========== */

esp_err_t led_controller_init(led_controller_t* const       controller,
                              const pca9685_handle_t* const pca_handle)
{
  if (controller == NULL || pca_handle == NULL) {
    ESP_LOGE(s_TAG, "Invalid arguments for LED controller init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(controller, 0, sizeof(led_controller_t));

  controller->mutex = xSemaphoreCreateMutex();
  if (controller->mutex == NULL) {
    ESP_LOGE(s_TAG, "Failed to create mutex for LED controller");
    return ESP_ERR_NO_MEM;
  }

  controller->pca_handle  = pca_handle;
  controller->initialized = true;

  ESP_LOGI(s_TAG, "LED controller initialized successfully");

  const esp_err_t ret = led_controller_turn_off_all(controller);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to turn off all LEDs during init");
  }

  return ESP_OK;
}

void led_controller_deinit(led_controller_t* const controller)
{
  if (controller == NULL || !controller->initialized) {
    return;
  }

  if (xSemaphoreTake(controller->mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) == pdTRUE) {
    led_controller_turn_off_all(controller);
    controller->initialized = false;
    controller->pca_handle  = NULL;
    xSemaphoreGive(controller->mutex);
  }

  if (controller->mutex != NULL) {
    vSemaphoreDelete(controller->mutex);
    controller->mutex = NULL;
  }

  ESP_LOGI(s_TAG, "LED controller deinitialized");
}

rgb_color_t led_controller_distance_to_color(const float distance_cm)
{
  rgb_color_t color = {0.0f, 0.0f, 0.0f};

  /* Handle invalid distances gracefully */
  if (!star_validate_distance(distance_cm)) {
    return color; /* Return off for invalid readings */
  }

  /* Turn off LEDs for distances beyond maximum range */
  if (distance_cm > STAR_SYSTEM_CONFIG.distance.max_distance_cm) {
    return color; /* Return off color */
  }

  /* CONFIGURABLE DISTANCE RANGE MAPPING */
  /* Ultra-smooth mapping with configurable min/max distances */
  const float min_distance =
    STAR_SYSTEM_CONFIG.distance.min_distance_cm * STAR_SYSTEM_CONFIG.distance.extension_factor;
  float max_distance = STAR_SYSTEM_CONFIG.distance.max_distance_cm;

  /* Handle edge case where min might equal max */
  if (max_distance <= min_distance) {
    max_distance = min_distance + s_led_distance_range_min_offset; /* Ensure at least 1cm range */
  }

  /* Smooth clamping with exponential approach to boundaries */
  float effective_distance = distance_cm;
  if (effective_distance < min_distance) {
    /* Exponential approach to minimum for ultra-close distances */
    float ratio = effective_distance / min_distance;
    effective_distance =
      min_distance * (s_led_hsv_saturation_max - expf(-s_led_distance_smooth_factor * ratio));
  }

  /* Map to normalized value with configurable range */
  const float range = max_distance - min_distance;
  float       t     = (effective_distance - min_distance) / range;

  /* Ensure t is in valid range with smooth clamping */
  t = fmaxf(0.0f, fminf(s_led_hsv_saturation_max, t));

  /* Ultra-smooth HSV gradient: Red (0°) -> Blue (240°) via rainbow spectrum */
  hsv_color_t hsv;

  /* Smooth hue transition across extended color spectrum */
  /* 0° = Red, 60° = Yellow, 120° = Green, 180° = Cyan, 240° = Blue */
  hsv.hue = t * s_led_gradient_max_hue;

  /* Full saturation for vibrant, rich colors */
  hsv.saturation = s_led_hsv_saturation_max;

  /* Smooth brightness curve with emphasis on close-range visibility */
  /* Use smooth polynomial instead of linear for more natural transitions */
  const float t_squared = t * t;
  const float t_cubed   = t_squared * t;

  /* Hermite interpolation for ultra-smooth brightness transitions */
  const float brightness_base =
    s_led_hsv_saturation_max -
    (0.5f * (s_led_brightness_curve_factor * t_squared - s_led_brightness_power_factor * t_cubed));

  /* Configurable brightness boost for safety warning at close range */
  float close_boost = 0.0f;
  if (t < STAR_SYSTEM_CONFIG.distance.close_boost_threshold) {
    /* Smooth exponential boost for close distances */
    float close_factor =
      s_led_hsv_saturation_max - (t / STAR_SYSTEM_CONFIG.distance.close_boost_threshold);
    close_boost = s_led_close_boost_factor * close_factor * close_factor; /* Quadratic falloff */
  }

  hsv.value =
    fmaxf(s_led_brightness_min, fminf(s_led_brightness_max, brightness_base + close_boost));

  /* Convert HSV to RGB */
  color = led_controller_hsv_to_rgb(&hsv);

  /* Enhanced debug logging showing configurable range */
  ESP_LOGI(
    s_TAG,
    "Range[%.1f-%.1f]cm: %.2fcm -> Eff: %.2fcm, t=%.4f, HSV(%.2f,%.2f,%.2f) -> RGB(%.2f,%.2f,%.2f)",
    STAR_SYSTEM_CONFIG.distance.min_distance_cm,
    STAR_SYSTEM_CONFIG.distance.max_distance_cm,
    distance_cm,
    effective_distance,
    t,
    hsv.hue,
    hsv.saturation,
    hsv.value,
    color.red,
    color.green,
    color.blue);

  return color;
}

esp_err_t led_controller_set_rgb(led_controller_t* const  controller,
                                 const led_index_t        led_index,
                                 const rgb_color_t* const color)
{
  if (controller == NULL || !controller->initialized || color == NULL) {
    ESP_LOGE(s_TAG, "Invalid arguments or controller not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (led_index >= k_led_index_max) {
    ESP_LOGE(s_TAG, "Invalid LED index: %d", led_index);
    return ESP_ERR_INVALID_ARG;
  }

  if (xSemaphoreTake(controller->mutex,
                     pdMS_TO_TICKS(STAR_SYSTEM_CONFIG.freertos.mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGE(s_TAG, "Failed to acquire mutex for LED %d", led_index);
    return ESP_ERR_TIMEOUT;
  }

  esp_err_t ret = ESP_OK;
  if (controller->initialized && controller->pca_handle != NULL) {
    ret = internal_set_rgb_channels(controller->pca_handle, led_index, color);
  } else {
    ret = ESP_ERR_INVALID_STATE;
  }

  xSemaphoreGive(controller->mutex);
  return ret;
}

esp_err_t led_controller_update_distance(led_controller_t* const controller,
                                         const led_index_t       led_index,
                                         const float             distance_cm)
{
  if (!star_validate_distance(distance_cm) &&
      distance_cm <= STAR_SYSTEM_CONFIG.distance.max_distance_cm) {
    ESP_LOGW(s_TAG, "Distance out of valid range: %.1f cm", distance_cm);
  }

  const rgb_color_t color = led_controller_distance_to_color(distance_cm);
  return led_controller_set_rgb(controller, led_index, &color);
}

esp_err_t led_controller_turn_off(led_controller_t* const controller, const led_index_t led_index)
{
  const rgb_color_t off_color = {0.0f, 0.0f, 0.0f};
  return led_controller_set_rgb(controller, led_index, &off_color);
}

esp_err_t led_controller_turn_off_all(led_controller_t* const controller)
{
  esp_err_t ret = ESP_OK;

  for (led_index_t i = 0; i < k_led_index_max; i++) {
    const esp_err_t single_ret = led_controller_turn_off(controller, i);
    if (single_ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to turn off LED %d: %s", i, esp_err_to_name(single_ret));
      ret = single_ret;
    }
  }

  return ret;
}

bool led_controller_is_ready(const led_controller_t* const controller)
{
  return (controller != NULL && controller->initialized && controller->pca_handle != NULL &&
          controller->mutex != NULL);
}