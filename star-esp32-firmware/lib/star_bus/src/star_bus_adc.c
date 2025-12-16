/* lib/star_bus/src/star_bus_adc.c */

#include "star_bus_adc.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"

#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "star_bus_manager.h" /* Needed for star_bus_manager_find_bus */
#include "star_bus_types.h"   /* Needed for star_bus_config_t */

/* --- Constants --- */

static const char* s_TAG = "STAR_BUS_ADC";

/* --- Private Function Prototypes (Default Ops) --- */

static esp_err_t internal_star_bus_adc_read_raw(const star_bus_config_t* config,
                                                 int*                     out_raw);

static esp_err_t internal_star_bus_adc_read_voltage(const star_bus_config_t* config,
                                                     int*                     out_voltage_mv);

/* --- Public Functions --- */

star_adc_ops_t star_bus_adc_get_default_ops(void)
{
  star_adc_ops_t ops = {
    .read_raw     = internal_star_bus_adc_read_raw,
    .read_voltage = internal_star_bus_adc_read_voltage,
  };
  ESP_LOGD(s_TAG, "Default ADC operations initialized");
  return ops;
}

esp_err_t star_bus_adc_read_raw(star_bus_manager_t* manager,
                                 const char*         bus_name,
                                 int*                out_raw)
{
  ESP_RETURN_ON_FALSE(manager && bus_name && out_raw,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Manager, bus_name, or out_raw is NULL");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, bus_name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_TAG, "Bus '%s' not found", bus_name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_adc,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Bus '%s' is not an ADC bus",
                      bus_name);
  ESP_RETURN_ON_FALSE(bus_config->proto.adc.ops.read_raw,
                      ESP_ERR_NOT_SUPPORTED,
                      s_TAG,
                      "No read_raw op for '%s'",
                      bus_name);

  return bus_config->proto.adc.ops.read_raw(bus_config, out_raw);
}

esp_err_t star_bus_adc_read_voltage(star_bus_manager_t* manager,
                                     const char*         bus_name,
                                     int*                out_voltage_mv)
{
  ESP_RETURN_ON_FALSE(manager && bus_name && out_voltage_mv,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Manager, bus_name, or out_voltage_mv is NULL");

  star_bus_config_t* bus_config = star_bus_manager_find_bus(manager, bus_name);
  ESP_RETURN_ON_FALSE(bus_config, ESP_ERR_NOT_FOUND, s_TAG, "Bus '%s' not found", bus_name);
  ESP_RETURN_ON_FALSE(bus_config->type == k_star_bus_type_adc,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Bus '%s' is not an ADC bus",
                      bus_name);
  ESP_RETURN_ON_FALSE(bus_config->proto.adc.ops.read_voltage,
                      ESP_ERR_NOT_SUPPORTED,
                      s_TAG,
                      "No read_voltage op for '%s'",
                      bus_name);

  return bus_config->proto.adc.ops.read_voltage(bus_config, out_voltage_mv);
}

/* --- Private Functions (Default Ops Implementation) --- */

static esp_err_t internal_star_bus_adc_read_raw(const star_bus_config_t* config, int* out_raw)
{
  ESP_RETURN_ON_FALSE(config && out_raw, ESP_ERR_INVALID_ARG, s_TAG, "Config or out_raw is NULL");
  ESP_RETURN_ON_FALSE(config->proto.adc.unit_handle,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "ADC unit handle is NULL");

  esp_err_t ret = adc_oneshot_read(config->proto.adc.unit_handle,
                                    config->proto.adc.channel,
                                    out_raw);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to read ADC channel %d", config->proto.adc.channel);

  ESP_LOGD(s_TAG, "ADC channel %d raw value: %d", config->proto.adc.channel, *out_raw);
  return ESP_OK;
}

static esp_err_t internal_star_bus_adc_read_voltage(const star_bus_config_t* config,
                                                     int*                     out_voltage_mv)
{
  ESP_RETURN_ON_FALSE(config && out_voltage_mv,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Config or out_voltage_mv is NULL");
  ESP_RETURN_ON_FALSE(config->proto.adc.unit_handle,
                      ESP_ERR_INVALID_STATE,
                      s_TAG,
                      "ADC unit handle is NULL");

  /* First read raw value */
  int raw_value;
  esp_err_t ret = adc_oneshot_read(config->proto.adc.unit_handle,
                                    config->proto.adc.channel,
                                    &raw_value);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to read ADC channel %d", config->proto.adc.channel);

  /* Convert to voltage if calibration is available */
  if (config->proto.adc.cali_handle) {
    ret = adc_cali_raw_to_voltage(config->proto.adc.cali_handle, raw_value, out_voltage_mv);
    ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to convert ADC raw to voltage");

    ESP_LOGD(s_TAG,
             "ADC channel %d: raw=%d, voltage=%d mV",
             config->proto.adc.channel,
             raw_value,
             *out_voltage_mv);
  } else {
    /* No calibration available, estimate voltage from raw value */
    /* For 12-bit ADC with 3.3V reference: voltage = (raw / 4095) * 3300 */
    *out_voltage_mv = (raw_value * 3300) / 4095;
    ESP_LOGW(s_TAG,
             "ADC channel %d: No calibration, estimated voltage=%d mV (raw=%d)",
             config->proto.adc.channel,
             *out_voltage_mv,
             raw_value);
  }

  return ESP_OK;
}
