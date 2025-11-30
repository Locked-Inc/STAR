#include "include/led_controller.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <string.h>

static const char* s_TAG = TAG_LED_CTRL;

/* ========== Private Functions ========== */

static float internal_scale_pwm_for_color(float original_percent, float min_pwm)
{
  // Ultra-smooth PWM scaling with no discrete jumps
  
  // Use an extremely low threshold for near-seamless transitions
  if (original_percent <= LED_OFF_THRESHOLD) {
    return 0.0f;  // True off state
  }
  
  // Smooth exponential scaling to avoid linear "steps"
  // This creates a more natural brightness perception curve
  float normalized = original_percent / 100.0f;
  
  // Apply gamma correction for perceptually linear brightness
  // Human eye perceives brightness non-linearly
  float gamma_corrected = powf(normalized, 2.2f);
  
  // Map to safe PWM range with smooth interpolation
  float pwm_range = MAX_PWM_ALL - min_pwm;
  float scaled = min_pwm + (gamma_corrected * pwm_range);
  
  // Ensure we stay within safe bounds
  scaled = fmaxf(min_pwm, fminf(MAX_PWM_ALL, scaled));
  
  // Enhanced debug logging for PWM analysis
  ESP_LOGI(s_TAG, "PWM: %.3f%% -> γ:%.3f -> %.2f%% (range: %.1f-%.1f)",
           original_percent, gamma_corrected, scaled, min_pwm, MAX_PWM_ALL);
  
  return scaled;
}

static esp_err_t internal_set_rgb_channels(const pca9685_handle_t* pca_handle,
                                           led_index_t             led_index,
                                           const rgb_color_t*      color)
{
  if (!VALIDATE_PWM_PERCENT(color->red) || !VALIDATE_PWM_PERCENT(color->green) ||
      !VALIDATE_PWM_PERCENT(color->blue)) {
    ESP_LOGE(s_TAG,
             "Invalid PWM values: R=%.1f G=%.1f B=%.1f",
             color->red,
             color->green,
             color->blue);
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t base_channel  = led_index * 3;
  uint8_t red_channel   = base_channel + 0;
  uint8_t green_channel = base_channel + 1;
  uint8_t blue_channel  = base_channel + 2;

  float scaled_red   = internal_scale_pwm_for_color(color->red, MIN_PWM_RED);
  float scaled_green = internal_scale_pwm_for_color(color->green, MIN_PWM_GREEN);
  float scaled_blue  = internal_scale_pwm_for_color(color->blue, MIN_PWM_BLUE);

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
  
  if (hsv == NULL || hsv->saturation < 0.0f || hsv->saturation > 1.0f || 
      hsv->value < 0.0f || hsv->value > 1.0f) {
    return rgb;
  }
  
  float hue = fmodf(hsv->hue, 360.0f);
  if (hue < 0.0f) hue += 360.0f;
  
  float c = hsv->value * hsv->saturation;
  float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
  float m = hsv->value - c;
  
  float r_prime, g_prime, b_prime;
  
  if (hue < 60.0f) {
    r_prime = c; g_prime = x; b_prime = 0.0f;
  } else if (hue < 120.0f) {
    r_prime = x; g_prime = c; b_prime = 0.0f;
  } else if (hue < 180.0f) {
    r_prime = 0.0f; g_prime = c; b_prime = x;
  } else if (hue < 240.0f) {
    r_prime = 0.0f; g_prime = x; b_prime = c;
  } else if (hue < 300.0f) {
    r_prime = x; g_prime = 0.0f; b_prime = c;
  } else {
    r_prime = c; g_prime = 0.0f; b_prime = x;
  }
  
  rgb.red = (r_prime + m) * 100.0f;
  rgb.green = (g_prime + m) * 100.0f;
  rgb.blue = (b_prime + m) * 100.0f;
  
  return rgb;
}

/* ========== Public API Implementation ========== */

esp_err_t led_controller_init(led_controller_t* controller, const pca9685_handle_t* pca_handle)
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

  esp_err_t ret = led_controller_turn_off_all(controller);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to turn off all LEDs during init");
  }

  return ESP_OK;
}

void led_controller_deinit(led_controller_t* controller)
{
  if (controller == NULL || !controller->initialized) {
    return;
  }

  if (xSemaphoreTake(controller->mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) == pdTRUE) {
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

rgb_color_t led_controller_distance_to_color(float distance_cm)
{
  rgb_color_t color = {0.0f, 0.0f, 0.0f};

  // Handle invalid distances gracefully
  if (!VALIDATE_DISTANCE(distance_cm)) {
    return color;  // Return off for invalid readings
  }
  
  // Turn off LEDs for distances beyond maximum range
  if (distance_cm > LED_COLOR_DISTANCE_MAX) {
    return color;  // Return off color
  }

  // CONFIGURABLE DISTANCE RANGE MAPPING
  // Ultra-smooth mapping with configurable min/max distances
  float min_distance = LED_COLOR_DISTANCE_MIN * LED_COLOR_EXTENSION_FACTOR;
  float max_distance = LED_COLOR_DISTANCE_MAX;
  
  // Handle edge case where min might equal max
  if (max_distance <= min_distance) {
    max_distance = min_distance + 1.0f;  // Ensure at least 1cm range
  }
  
  // Smooth clamping with exponential approach to boundaries
  float effective_distance = distance_cm;
  if (effective_distance < min_distance) {
    // Exponential approach to minimum for ultra-close distances
    float ratio = effective_distance / min_distance;
    effective_distance = min_distance * (1.0f - expf(-3.0f * ratio));
  }
  
  // Map to normalized value with configurable range
  float range = max_distance - min_distance;
  float t = (effective_distance - min_distance) / range;
  
  // Ensure t is in valid range with smooth clamping
  t = fmaxf(0.0f, fminf(1.0f, t));

  // Ultra-smooth HSV gradient: Red (0°) -> Blue (240°) via rainbow spectrum
  hsv_color_t hsv;
  
  // Smooth hue transition across extended color spectrum
  // 0° = Red, 60° = Yellow, 120° = Green, 180° = Cyan, 240° = Blue
  hsv.hue = t * 240.0f;
  
  // Full saturation for vibrant, rich colors
  hsv.saturation = 1.0f;
  
  // Smooth brightness curve with emphasis on close-range visibility
  // Use smooth polynomial instead of linear for more natural transitions
  float t_squared = t * t;
  float t_cubed = t_squared * t;
  
  // Hermite interpolation for ultra-smooth brightness transitions
  float brightness_base = 1.0f - (0.5f * (3.0f * t_squared - 2.0f * t_cubed));
  
  // Configurable brightness boost for safety warning at close range
  float close_boost = 0.0f;
  if (t < LED_COLOR_CLOSE_BOOST_THRESHOLD) {
    // Smooth exponential boost for close distances
    float close_factor = 1.0f - (t / LED_COLOR_CLOSE_BOOST_THRESHOLD);
    close_boost = 0.3f * close_factor * close_factor;  // Quadratic falloff
  }
  
  hsv.value = fmaxf(0.4f, fminf(1.0f, brightness_base + close_boost));
  
  // Convert HSV to RGB
  color = led_controller_hsv_to_rgb(&hsv);
  
  // Enhanced debug logging showing configurable range
  ESP_LOGI(s_TAG, "Range[%.1f-%.1f]cm: %.2fcm -> Eff: %.2fcm, t=%.4f, HSV(%.2f,%.2f,%.2f) -> RGB(%.2f,%.2f,%.2f)",
           LED_COLOR_DISTANCE_MIN, LED_COLOR_DISTANCE_MAX, distance_cm, effective_distance, t, 
           hsv.hue, hsv.saturation, hsv.value, color.red, color.green, color.blue);
  
  return color;
}

esp_err_t led_controller_set_rgb(led_controller_t*  controller,
                                 led_index_t        led_index,
                                 const rgb_color_t* color)
{
  if (controller == NULL || !controller->initialized || color == NULL) {
    ESP_LOGE(s_TAG, "Invalid arguments or controller not initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (led_index >= LED_INDEX_MAX) {
    ESP_LOGE(s_TAG, "Invalid LED index: %d", led_index);
    return ESP_ERR_INVALID_ARG;
  }

  if (xSemaphoreTake(controller->mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
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

esp_err_t led_controller_update_distance(led_controller_t* controller,
                                         led_index_t       led_index,
                                         float             distance_cm)
{
  if (!VALIDATE_DISTANCE(distance_cm) && distance_cm <= LED_COLOR_DISTANCE_MAX) {
    ESP_LOGW(s_TAG, "Distance out of valid range: %.1f cm", distance_cm);
  }

  rgb_color_t color = led_controller_distance_to_color(distance_cm);
  return led_controller_set_rgb(controller, led_index, &color);
}

esp_err_t led_controller_turn_off(led_controller_t* controller, led_index_t led_index)
{
  rgb_color_t off_color = {0.0f, 0.0f, 0.0f};
  return led_controller_set_rgb(controller, led_index, &off_color);
}

esp_err_t led_controller_turn_off_all(led_controller_t* controller)
{
  esp_err_t ret = ESP_OK;

  for (led_index_t i = 0; i < LED_INDEX_MAX; i++) {
    esp_err_t single_ret = led_controller_turn_off(controller, i);
    if (single_ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to turn off LED %d: %s", i, esp_err_to_name(single_ret));
      ret = single_ret;
    }
  }

  return ret;
}

bool led_controller_is_ready(const led_controller_t* controller)
{
  return (controller != NULL && controller->initialized && controller->pca_handle != NULL &&
          controller->mutex != NULL);
}