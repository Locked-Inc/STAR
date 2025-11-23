/* lib/star_sensor_mq135/src/star_sensor_mq135.c */

#include "star_sensor_mq135.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

static const char* s_TAG = "mq135";

#define MQ135_WARMUP_PERIOD_MS (86400000UL)  // 24 hours in milliseconds

esp_err_t star_sensor_mq135_init(mq135_handle_t* handle, const mq135_config_t* config)
{
  if (handle == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  // Validate ADC channel
  if (config->adc_channel < ADC1_CHANNEL_0 || config->adc_channel >= ADC1_CHANNEL_MAX) {
    ESP_LOGE(s_TAG, "Invalid ADC channel: %d", config->adc_channel);
    return ESP_ERR_INVALID_ARG;
  }

  // Validate R0 if provided
  if (config->r0 <= 0.0f) {
    ESP_LOGW(s_TAG, "R0 not calibrated, must calibrate before use");
  }

  memset(handle, 0, sizeof(mq135_handle_t));
  handle->adc_channel = config->adc_channel;
  handle->attenuation = config->attenuation;
  handle->r0 = config->r0;
  handle->rl = (config->rl > 0.0f) ? config->rl : MQ135_RL_VALUE;

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG, "Initializing MQ135 (channel=%d, R0=%.2f)", config->adc_channel, config->r0);

  // Configure ADC
  ret = adc1_config_width(ADC_WIDTH_BIT_12);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure ADC width: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  ret = adc1_config_channel_atten(config->adc_channel, config->attenuation);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  handle->warmup_start_time = (uint32_t)(esp_timer_get_time() / 1000);
  handle->initialized = true;

  ESP_LOGI(s_TAG, "MQ135 initialization successful");
  ESP_LOGW(s_TAG, "Sensor requires 24-48h preheat for accurate readings");

  return ESP_OK;
}

esp_err_t star_sensor_mq135_deinit(mq135_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_sensor_mq135_read_raw(const mq135_handle_t* handle, uint16_t* adc_value)
{
  if (handle == NULL || adc_value == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  int raw = adc1_get_raw(handle->adc_channel);
  if (raw < 0) {
    ESP_LOGE(s_TAG, "ADC read failed");
    return ESP_FAIL;
  }

  // Bounds check
  if (raw > 4095) {
    ESP_LOGW(s_TAG, "ADC value out of range: %d", raw);
    raw = 4095;
  }

  *adc_value = (uint16_t)raw;
  return ESP_OK;
}

esp_err_t star_sensor_mq135_calculate_rs(const mq135_handle_t* handle,
                                         uint16_t              adc_value,
                                         float*                rs)
{
  if (handle == NULL || rs == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Avoid division by zero
  if (adc_value == 0) {
    ESP_LOGE(s_TAG, "ADC value is zero, cannot calculate Rs");
    return ESP_ERR_INVALID_ARG;
  }

  // Convert ADC to voltage
  float voltage = ((float)adc_value / MQ135_ADC_RESOLUTION) * MQ135_VCC;

  // Calculate sensor resistance: Rs = [(Vc * RL) / Vout] - RL
  // Vout = voltage, Vc = MQ135_VCC
  float rs_calculated = ((MQ135_VCC * handle->rl) / voltage) - handle->rl;

  // Sanity check
  if (rs_calculated < 0.0f) {
    ESP_LOGW(s_TAG, "Calculated Rs is negative: %.2f", rs_calculated);
    return ESP_ERR_INVALID_RESPONSE;
  }

  if (rs_calculated > 1000000.0f) {  // 1MΩ sanity limit
    ESP_LOGW(s_TAG, "Calculated Rs too high: %.2f", rs_calculated);
    return ESP_ERR_INVALID_RESPONSE;
  }

  *rs = rs_calculated;
  return ESP_OK;
}

esp_err_t star_sensor_mq135_calculate_ppm(float             ratio,
                                          mq135_gas_type_t  gas_type,
                                          float*            ppm)
{
  if (ppm == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Avoid invalid ratios
  if (ratio <= 0.0f || ratio > 100.0f) {
    ESP_LOGW(s_TAG, "Invalid Rs/R0 ratio: %.3f", ratio);
    return ESP_ERR_INVALID_ARG;
  }

  float a, b;

  switch (gas_type) {
    case MQ135_GAS_CO2:
      a = MQ135_CO2_A;
      b = MQ135_CO2_B;
      break;
    case MQ135_GAS_NH3:
      a = MQ135_NH3_A;
      b = MQ135_NH3_B;
      break;
    case MQ135_GAS_ALCOHOL:
      a = MQ135_ALCOHOL_A;
      b = MQ135_ALCOHOL_B;
      break;
    default:
      ESP_LOGE(s_TAG, "Invalid gas type: %d", gas_type);
      return ESP_ERR_INVALID_ARG;
  }

  // PPM = a * (Rs/R0)^b
  // Use powf for floating point power
  *ppm = a * powf(ratio, b);

  // Sanity bounds
  if (*ppm < 0.0f) *ppm = 0.0f;
  if (*ppm > 10000.0f) {
    ESP_LOGW(s_TAG, "Calculated PPM very high: %.2f", *ppm);
  }

  return ESP_OK;
}

esp_err_t star_sensor_mq135_calibrate(mq135_handle_t* handle, uint16_t samples, float* r0)
{
  if (handle == NULL || r0 == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (samples == 0 || samples > 1000) {
    ESP_LOGE(s_TAG, "Invalid sample count: %d (must be 1-1000)", samples);
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(s_TAG, "Starting calibration with %d samples", samples);

  float rs_sum = 0.0f;
  uint16_t valid_samples = 0;

  for (uint16_t i = 0; i < samples; i++) {
    uint16_t adc_value;
    esp_err_t ret = star_sensor_mq135_read_raw(handle, &adc_value);
    if (ret != ESP_OK) {
      continue;
    }

    float rs;
    ret = star_sensor_mq135_calculate_rs(handle, adc_value, &rs);
    if (ret == ESP_OK) {
      rs_sum += rs;
      valid_samples++;
    }

    vTaskDelay(pdMS_TO_TICKS(100));  // 100ms between samples
  }

  if (valid_samples == 0) {
    ESP_LOGE(s_TAG, "No valid samples during calibration");
    return ESP_FAIL;
  }

  float rs_avg = rs_sum / valid_samples;

  // R0 = Rs / clean_air_ratio
  *r0 = rs_avg / MQ135_CLEAN_AIR_RATIO;

  if (*r0 <= 0.0f || *r0 > 100000.0f) {
    ESP_LOGW(s_TAG, "Calculated R0 out of expected range: %.2f", *r0);
  }

  ESP_LOGI(s_TAG, "Calibration complete: R0=%.2f (from %d samples)", *r0, valid_samples);

  return ESP_OK;
}

esp_err_t star_sensor_mq135_set_r0(mq135_handle_t* handle, float r0)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (r0 <= 0.0f) {
    ESP_LOGE(s_TAG, "Invalid R0 value: %.2f", r0);
    return ESP_ERR_INVALID_ARG;
  }

  handle->r0 = r0;
  ESP_LOGI(s_TAG, "R0 set to %.2f", r0);

  return ESP_OK;
}

esp_err_t star_sensor_mq135_read(const mq135_handle_t* handle, mq135_data_t* data)
{
  if (handle == NULL || data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (handle->r0 <= 0.0f) {
    ESP_LOGE(s_TAG, "Sensor not calibrated, R0 not set");
    return ESP_ERR_INVALID_STATE;
  }

  uint16_t adc_value;
  esp_err_t ret = star_sensor_mq135_read_raw(handle, &adc_value);
  if (ret != ESP_OK) {
    return ret;
  }

  ret = star_sensor_mq135_calculate_rs(handle, adc_value, &data->rs);
  if (ret != ESP_OK) {
    return ret;
  }

  // Calculate ratio
  data->ratio = data->rs / handle->r0;

  // Calculate gas concentrations
  ret = star_sensor_mq135_calculate_ppm(data->ratio, MQ135_GAS_CO2, &data->ppm_co2);
  if (ret != ESP_OK) {
    data->ppm_co2 = 0.0f;
  }

  ret = star_sensor_mq135_calculate_ppm(data->ratio, MQ135_GAS_NH3, &data->ppm_nh3);
  if (ret != ESP_OK) {
    data->ppm_nh3 = 0.0f;
  }

  ret = star_sensor_mq135_calculate_ppm(data->ratio, MQ135_GAS_ALCOHOL, &data->ppm_alcohol);
  if (ret != ESP_OK) {
    data->ppm_alcohol = 0.0f;
  }

  ESP_LOGD(s_TAG,
           "Rs=%.2f, Ratio=%.3f, CO2=%.1f ppm, NH3=%.1f ppm, Alcohol=%.1f ppm",
           data->rs,
           data->ratio,
           data->ppm_co2,
           data->ppm_nh3,
           data->ppm_alcohol);

  return ESP_OK;
}

esp_err_t star_sensor_mq135_compensate(const mq135_handle_t* handle,
                                       float                 temperature,
                                       float                 humidity,
                                       float                 rs,
                                       float*                rs_corrected)
{
  if (handle == NULL || rs_corrected == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Validate inputs
  if (temperature < -40.0f || temperature > 85.0f) {
    ESP_LOGW(s_TAG, "Temperature out of range: %.1f°C", temperature);
    return ESP_ERR_INVALID_ARG;
  }

  if (humidity < 0.0f || humidity > 100.0f) {
    ESP_LOGW(s_TAG, "Humidity out of range: %.1f%%", humidity);
    return ESP_ERR_INVALID_ARG;
  }

  // Temperature compensation factor
  // Simplified linear model: factor increases with temperature
  float temp_factor = 1.0f + ((temperature - MQ135_TEMP_REFERENCE) * 0.01f);

  // Humidity compensation factor
  // Simplified model: factor decreases with humidity
  float humidity_factor = 1.0f - ((humidity - MQ135_HUMIDITY_REFERENCE) * 0.002f);

  // Apply compensation
  *rs_corrected = rs * temp_factor * humidity_factor;

  ESP_LOGD(s_TAG,
           "Compensation: T=%.1f°C, H=%.1f%%, Rs=%.2f -> %.2f",
           temperature,
           humidity,
           rs,
           *rs_corrected);

  return ESP_OK;
}

esp_err_t star_sensor_mq135_is_warmed_up(const mq135_handle_t* handle, bool* warmed_up)
{
  if (handle == NULL || warmed_up == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000);
  uint32_t elapsed = current_time - handle->warmup_start_time;

  *warmed_up = (elapsed >= MQ135_WARMUP_PERIOD_MS);

  return ESP_OK;
}
