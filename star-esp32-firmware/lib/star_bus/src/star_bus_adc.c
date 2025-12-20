/**
 * @file star_bus_adc.c
 * @brief ADC (Analog-to-Digital Converter) operations implementation
 * @details
 * Implements analog-to-digital conversion operations for ESP32 ADC units.
 * Supports configurable bit width, attenuation levels, and channel selection.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "star_bus_adc.h"

#include <inttypes.h>
#include <string.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"
#include "star_bus_manager.h" /* Needed for star_bus_manager_find_bus */
#include "star_bus_types.h"   /* Needed for star_bus_config_t */

/* --- Constants --- */

static const char* s_tag = "STAR_BUS_ADC";

/* --- Private Function Prototypes (Default Ops) --- */

static esp_err_t internal_star_bus_adc_read_raw(const star_bus_config_t* config, int32_t* out_raw);

static esp_err_t internal_star_bus_adc_read_voltage(const star_bus_config_t* config,
                                                    int32_t*                 out_voltage_mv);

/* --- Public Functions --- */

star_adc_ops_t star_bus_adc_get_default_ops(void)
{
  star_adc_ops_t ops = {
    .read_raw     = internal_star_bus_adc_read_raw,
    .read_voltage = internal_star_bus_adc_read_voltage,
  };
  ESP_LOGD(s_tag, "Default ADC operations initialized");
  return ops;
}

esp_err_t star_bus_adc_read_raw(star_bus_manager_t* manager, const char* bus_name, int32_t* out_raw)
{
  ESP_RETURN_ON_FALSE(manager && bus_name && out_raw,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Manager, bus_name, or out_raw is NULL");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, bus_name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_tag, "Bus '%s' not found", bus_name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_adc,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Bus '%s' is not an ADC bus",
                      bus_name);
  ESP_RETURN_ON_FALSE(bus_config->proto.adc.ops.read_raw,
                      ESP_ERR_NOT_SUPPORTED,
                      s_tag,
                      "No read_raw op for '%s'",
                      bus_name);

  return bus_config->proto.adc.ops.read_raw(bus_config, out_raw);
}

esp_err_t star_bus_adc_read_voltage(star_bus_manager_t* manager,
                                    const char*         bus_name,
                                    int32_t*            out_voltage_mv)
{
  ESP_RETURN_ON_FALSE(manager && bus_name && out_voltage_mv,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Manager, bus_name, or out_voltage_mv is NULL");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, bus_name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_tag, "Bus '%s' not found", bus_name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_adc,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Bus '%s' is not an ADC bus",
                      bus_name);
  ESP_RETURN_ON_FALSE(bus_config->proto.adc.ops.read_voltage,
                      ESP_ERR_NOT_SUPPORTED,
                      s_tag,
                      "No read_voltage op for '%s'",
                      bus_name);

  return bus_config->proto.adc.ops.read_voltage(bus_config, out_voltage_mv);
}

/* --- Private Functions (Default Ops Implementation) --- */

static esp_err_t internal_star_bus_adc_read_raw(const star_bus_config_t* config, int32_t* out_raw)
{
  ESP_RETURN_ON_FALSE(config && out_raw, ESP_ERR_INVALID_ARG, s_tag, "Config or out_raw is NULL");
  ESP_RETURN_ON_FALSE(config->proto.adc.unit_handle,
                      ESP_ERR_INVALID_STATE,
                      s_tag,
                      "ADC unit handle is NULL");

  /* Use int for ESP-IDF function call */
  int       esp_raw_value;
  esp_err_t ret =
    adc_oneshot_read(config->proto.adc.unit_handle, config->proto.adc.channel, &esp_raw_value);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to read ADC channel %d", config->proto.adc.channel);

  /* Explicit conversion from int to int32_t */
  *out_raw = (int32_t)esp_raw_value;

  ESP_LOGD(s_tag, "ADC channel %d raw value: %" PRId32, config->proto.adc.channel, *out_raw);
  return ESP_OK;
}

static esp_err_t internal_star_bus_adc_read_voltage(const star_bus_config_t* config,
                                                    int32_t*                 out_voltage_mv)
{
  ESP_RETURN_ON_FALSE(config && out_voltage_mv,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Config or out_voltage_mv is NULL");
  ESP_RETURN_ON_FALSE(config->proto.adc.unit_handle,
                      ESP_ERR_INVALID_STATE,
                      s_tag,
                      "ADC unit handle is NULL");

  /* Use int for ESP-IDF function calls */
  int       esp_raw_value;
  esp_err_t ret =
    adc_oneshot_read(config->proto.adc.unit_handle, config->proto.adc.channel, &esp_raw_value);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to read ADC channel %d", config->proto.adc.channel);

  /* Convert to voltage if calibration is available */
  if (config->proto.adc.cali_handle) {
    int esp_voltage_mv;
    ret = adc_cali_raw_to_voltage(config->proto.adc.cali_handle, esp_raw_value, &esp_voltage_mv);
    ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to convert ADC raw to voltage");

    /* Explicit conversion from int to int32_t */
    *out_voltage_mv = (int32_t)esp_voltage_mv;

    ESP_LOGD(s_tag,
             "ADC channel %d: raw=%d, voltage=%" PRId32 " mV",
             config->proto.adc.channel,
             esp_raw_value,
             *out_voltage_mv);
  } else {
    /* No calibration available, estimate voltage from raw value */
    /* For 12-bit ADC with 3.3V reference: voltage = (raw / 4095) * 3300 */
    *out_voltage_mv = (int32_t)((esp_raw_value * 3300) / 4095);
    ESP_LOGW(s_tag,
             "ADC channel %d: No calibration, estimated voltage=%" PRId32 " mV (raw=%d)",
             config->proto.adc.channel,
             *out_voltage_mv,
             esp_raw_value);
  }

  return ESP_OK;
}
