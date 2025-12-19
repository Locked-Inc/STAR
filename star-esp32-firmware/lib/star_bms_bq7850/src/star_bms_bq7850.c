/* lib/star_bms_bq7850/src/star_bms_bq7850.c */

/**
 * @file star_bms_bq7850.c
 * @brief BQ7850 battery management system driver implementation
 */

#include "star_bms_bq7850.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

#include "star_bus_smbus.h"

static const char* const s_tag = "STAR_BMS_BQ7850";

/* Use constant instead of macro for type safety */
static const uint32_t s_bq7850_mutex_timeout_ms = 1000;

/* --- Internal Helper Functions --- */

/**
 * @brief Execute manufacturer access command
 *
 * The BQ7850 uses a two-step process for extended commands:
 * 1. Write subcommand to 0x00 (Manufacturer Access)
 * 2. Read result from 0x00
 */
static esp_err_t internal_bq7850_manufacturer_access(star_bus_manager_t* manager,
                                                     const char*         bus_name,
                                                     uint8_t             addr,
                                                     uint16_t            subcmd,
                                                     uint16_t*           result)
{
  esp_err_t ret;

  /* Write subcommand to Manufacturer Access register */
  ret = star_smbus_write_word(manager, bus_name, addr, k_bq7850_cmd_manufacturer_access, subcmd);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag,
             "Failed to write manufacturer access subcommand 0x%04X: %s",
             subcmd,
             esp_err_to_name(ret));
    return ret;
  }

  /* Small delay for command processing (required by BQ7850 datasheet) */
  vTaskDelay(pdMS_TO_TICKS(k_bq7850_mfr_access_delay_ms));

  /* Read result if requested */
  if (result != NULL) {
    ret = star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_manufacturer_access, result);
    if (ret != ESP_OK) {
      ESP_LOGE(s_tag, "Failed to read manufacturer access result: %s", esp_err_to_name(ret));
      return ret;
    }
  }

  return ESP_OK;
}

/**
 * @brief Read cell voltage via manufacturer access subcommand
 */
static esp_err_t internal_bq7850_read_cell_voltage(star_bus_manager_t* manager,
                                                   const char*         bus_name,
                                                   uint8_t             addr,
                                                   uint8_t             cell_index,
                                                   uint16_t*           voltage_mv)
{
  if (cell_index >= k_bq7850_max_cells) {
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t subcmd = k_bq7850_subcmd_cell_voltage_base + cell_index;
  return internal_bq7850_manufacturer_access(manager, bus_name, addr, subcmd, voltage_mv);
}

/* --- Core Functions --- */

esp_err_t star_bms_bq7850_init(bq7850_handle_t*       handle,
                               star_bus_manager_t*    manager,
                               const char*            bus_name,
                               const bq7850_config_t* config)
{
  if (handle == NULL || manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_tag, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  if (config->num_cells == 0 || config->num_cells > k_bq7850_max_cells) {
    ESP_LOGE(s_tag,
             "Invalid number of cells: %d (must be 1-%d)",
             config->num_cells,
             k_bq7850_max_cells);
    return ESP_ERR_INVALID_ARG;
  }

  /* Initialize handle */
  memset(handle, 0, sizeof(bq7850_handle_t));
  handle->manager    = manager;
  handle->bus_name   = bus_name;
  handle->smbus_addr = config->smbus_addr;
  memcpy(&handle->config, config, sizeof(bq7850_config_t));

  /* Create mutex */
  handle->mutex = xSemaphoreCreateMutex();
  if (handle->mutex == NULL) {
    ESP_LOGE(s_tag, "Failed to create mutex");
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(s_tag,
           "Initializing BQ7850 BMS on bus '%s' at address 0x%02X",
           bus_name,
           config->smbus_addr);

  /* Verify device communication by reading device type */
  uint16_t  device_type = 0;
  esp_err_t ret         = internal_bq7850_manufacturer_access(manager,
                                                      bus_name,
                                                      config->smbus_addr,
                                                      k_bq7850_subcmd_device_type,
                                                      &device_type);

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to communicate with BQ7850: %s", esp_err_to_name(ret));
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  ESP_LOGI(s_tag, "BQ7850 detected, device type: 0x%04X", device_type);

  /* Read firmware version */
  uint16_t fw_version = 0;
  ret                 = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            config->smbus_addr,
                                            k_bq7850_subcmd_fw_version,
                                            &fw_version);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Firmware version: 0x%04X", fw_version);
  }

  /* Verify we can read basic parameters */
  uint16_t voltage = 0;
  ret = star_smbus_read_word(manager, bus_name, config->smbus_addr, k_bq7850_cmd_voltage, &voltage);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read pack voltage: %s", esp_err_to_name(ret));
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_tag,
           "BQ7850 initialization successful (thread-safe with mutex), pack voltage: %d mV",
           voltage);

  return ESP_OK;
}

esp_err_t star_bms_bq7850_deinit(bq7850_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Acquire mutex before cleanup */
  SemaphoreHandle_t mutex = handle->mutex;
  if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(s_bq7850_mutex_timeout_ms)) != pdTRUE) {
    ESP_LOGW(s_tag, "Failed to acquire mutex for deinit, continuing anyway");
  }

  /* Clear handle fields */
  handle->initialized = false;
  handle->mutex       = NULL;

  /* Release and delete mutex */
  if (mutex != NULL) {
    xSemaphoreGive(mutex);
    vSemaphoreDelete(mutex);
  }

  ESP_LOGI(s_tag, "BQ7850 BMS deinitialized");
  return ESP_OK;
}

esp_err_t star_bms_bq7850_get_device_info(const bq7850_handle_t* handle, bq7850_device_info_t* info)
{
  if (handle == NULL || info == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;
  esp_err_t           ret;

  memset(info, 0, sizeof(bq7850_device_info_t));

  /* String buffer size (reserve 1 byte for null terminator) */
  static const size_t s_max_string_copy_len = k_bq7850_device_info_string_len - 1;

  /* Read device type */
  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_device_type,
                                            &info->device_type);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Read firmware version */
  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_fw_version,
                                            &info->fw_version);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Read hardware version */
  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_hw_version,
                                            &info->hw_version);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Read serial number */
  uint16_t serial_low = 0;
  ret = star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_serial_number, &serial_low);
  if (ret == ESP_OK) {
    info->serial_number = serial_low;
  }

  /* Read manufacturer name (block read) */
  uint8_t mfg_len = 0;
  uint8_t mfg_data[k_bq7850_device_info_string_len];
  ret = star_smbus_block_read(manager,
                              bus_name,
                              addr,
                              k_bq7850_cmd_manufacture_name,
                              mfg_data,
                              k_bq7850_device_info_string_len,
                              &mfg_len);
  if (ret == ESP_OK && mfg_len > 0) {
    size_t copy_len = (mfg_len < s_max_string_copy_len) ? mfg_len : s_max_string_copy_len;
    memcpy(info->manufacturer, mfg_data, copy_len);
    info->manufacturer[copy_len] = '\0';
  }

  /* Read device name */
  uint8_t dev_len = 0;
  uint8_t dev_data[k_bq7850_device_info_string_len];
  ret = star_smbus_block_read(manager,
                              bus_name,
                              addr,
                              k_bq7850_cmd_device_name,
                              dev_data,
                              k_bq7850_device_info_string_len,
                              &dev_len);
  if (ret == ESP_OK && dev_len > 0) {
    size_t copy_len = (dev_len < s_max_string_copy_len) ? dev_len : s_max_string_copy_len;
    memcpy(info->device_name, dev_data, copy_len);
    info->device_name[copy_len] = '\0';
  }

  /* Read chemistry */
  uint8_t chem_len = 0;
  uint8_t chem_data[k_bq7850_device_info_string_len];
  ret = star_smbus_block_read(manager,
                              bus_name,
                              addr,
                              k_bq7850_cmd_device_chemistry,
                              chem_data,
                              k_bq7850_device_info_string_len,
                              &chem_len);
  if (ret == ESP_OK && chem_len > 0) {
    size_t copy_len = (chem_len < s_max_string_copy_len) ? chem_len : s_max_string_copy_len;
    memcpy(info->chemistry, chem_data, copy_len);
    info->chemistry[copy_len] = '\0';
  }

  return ESP_OK;
}

esp_err_t star_bms_bq7850_reset(const bq7850_handle_t* handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGW(s_tag, "Issuing software reset to BQ7850");

  esp_err_t ret = internal_bq7850_manufacturer_access(handle->manager,
                                                      handle->bus_name,
                                                      handle->smbus_addr,
                                                      k_bq7850_subcmd_device_reset,
                                                      NULL);

  if (ret == ESP_OK) {
    /* Device takes ~1 second to reset (per BQ7850 datasheet) */
    ESP_LOGI(s_tag, "Reset command sent, waiting for device to restart...");
    vTaskDelay(pdMS_TO_TICKS(k_bq7850_reset_delay_ms));
  }

  return ret;
}

/* --- Cell Voltage Functions --- */

esp_err_t star_bms_bq7850_read_cells(const bq7850_handle_t* handle, bq7850_cell_data_t* cell_data)
{
  if (handle == NULL || cell_data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;
  esp_err_t           ret      = ESP_OK;

  memset(cell_data, 0, sizeof(bq7850_cell_data_t));

  /* Read individual cell voltages */
  for (uint8_t i = 0; i < k_bq7850_max_cells; i++) {
    ret = internal_bq7850_read_cell_voltage(manager, bus_name, addr, i, &cell_data->cell_mv[i]);

    if (ret == ESP_OK && cell_data->cell_mv[i] > 0) {
      cell_data->valid_cells++;
    } else if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Failed to read cell %d voltage: %s", i + 1, esp_err_to_name(ret));
      /* Continue reading other cells */
    }
  }

  /* Read total pack voltage */
  ret = star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_voltage, &cell_data->pack_mv);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read pack voltage: %s", esp_err_to_name(ret));
  }

  return (cell_data->valid_cells > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t star_bms_bq7850_read_cell_voltage(const bq7850_handle_t* handle,
                                            uint8_t                cell_index,
                                            uint16_t*              voltage_mv)
{
  if (handle == NULL || voltage_mv == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (cell_index >= k_bq7850_max_cells) {
    return ESP_ERR_INVALID_ARG;
  }

  return internal_bq7850_read_cell_voltage(handle->manager,
                                           handle->bus_name,
                                           handle->smbus_addr,
                                           cell_index,
                                           voltage_mv);
}

esp_err_t star_bms_bq7850_read_pack_voltage(const bq7850_handle_t* handle, uint16_t* voltage_mv)
{
  if (handle == NULL || voltage_mv == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  return star_smbus_read_word(handle->manager,
                              handle->bus_name,
                              handle->smbus_addr,
                              k_bq7850_cmd_voltage,
                              voltage_mv);
}

/* --- Temperature Functions --- */

esp_err_t star_bms_bq7850_read_temperatures(const bq7850_handle_t* handle,
                                            bq7850_temp_data_t*    temp_data)
{
  if (handle == NULL || temp_data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;
  esp_err_t           ret;

  memset(temp_data, 0, sizeof(bq7850_temp_data_t));

  /* Read temperature sensors via manufacturer access */
  int32_t temp_sum = 0;

  for (uint8_t i = 0; i < k_bq7850_max_temp_sensors; i++) {
    uint16_t temp_raw = 0;
    uint16_t subcmd   = k_bq7850_subcmd_temp_sensor_base + i;

    ret = internal_bq7850_manufacturer_access(manager, bus_name, addr, subcmd, &temp_raw);

    if (ret == ESP_OK && temp_raw > 0) {
      /* BQ7850 returns temperature in 0.1K units */
      /* Convert to 0.1C: T(C) = T(K) - 273.15 */
      temp_data->temp_c[i] = temp_raw - k_bq7850_kelvin_offset_deci_c;
      temp_data->valid_sensors++;
      temp_sum += temp_data->temp_c[i];
    }
  }

  /* Calculate average temperature */
  if (temp_data->valid_sensors > 0) {
    temp_data->avg_temp_c = temp_sum / temp_data->valid_sensors;
  }

  return (temp_data->valid_sensors > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t star_bms_bq7850_read_temperature(const bq7850_handle_t* handle, int16_t* temp_c)
{
  if (handle == NULL || temp_c == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;
  uint16_t            temp_raw;
  esp_err_t           ret =
    star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_temperature, &temp_raw);

  if (ret == ESP_OK) {
    /* Convert from 0.1K to 0.1C */
    *temp_c = temp_raw - k_bq7850_kelvin_offset_deci_c;
  }

  return ret;
}

/* --- Current and Power Functions --- */

esp_err_t star_bms_bq7850_read_current(const bq7850_handle_t* handle,
                                       bq7850_current_data_t* current_data)
{
  if (handle == NULL || current_data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;
  esp_err_t           ret;

  memset(current_data, 0, sizeof(bq7850_current_data_t));

  /* Read instantaneous current (signed) - use intermediate variable to avoid aliasing */
  uint16_t current_raw = 0;
  ret = star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_current, &current_raw);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read current: %s", esp_err_to_name(ret));
    return ret;
  }
  current_data->current_ma = (int16_t)current_raw;

  /* Read average current - use intermediate variable to avoid aliasing */
  uint16_t avg_current_raw = 0;
  ret =
    star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_average_current, &avg_current_raw);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to read average current: %s", esp_err_to_name(ret));
  } else {
    current_data->avg_current_ma = (int16_t)avg_current_raw;
  }

  /* Read voltage */
  ret =
    star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_voltage, &current_data->voltage_mv);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read voltage: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Calculate power (mW) = voltage (mV) * current (mA) / 1000 */
  current_data->power_mw = ((int32_t)current_data->voltage_mv * current_data->current_ma) / 1000;

  return ESP_OK;
}

/* --- State of Charge Functions --- */

esp_err_t star_bms_bq7850_read_soc(const bq7850_handle_t* handle, bq7850_soc_data_t* soc_data)
{
  if (handle == NULL || soc_data == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;
  esp_err_t           ret;

  memset(soc_data, 0, sizeof(bq7850_soc_data_t));

  /* Read remaining capacity */
  ret = star_smbus_read_word(manager,
                             bus_name,
                             addr,
                             k_bq7850_cmd_remaining_capacity,
                             &soc_data->remaining_capacity_mah);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read remaining capacity: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Read full charge capacity */
  ret = star_smbus_read_word(manager,
                             bus_name,
                             addr,
                             k_bq7850_cmd_full_charge_capacity,
                             &soc_data->full_capacity_mah);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to read full charge capacity: %s", esp_err_to_name(ret));
  }

  /* Read relative SOC */
  uint16_t soc_raw;
  ret = star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_relative_state_charge, &soc_raw);
  if (ret == ESP_OK) {
    soc_data->relative_soc = (uint8_t)soc_raw;
  }

  /* Read absolute SOC */
  ret = star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_absolute_state_charge, &soc_raw);
  if (ret == ESP_OK) {
    soc_data->absolute_soc = (uint8_t)soc_raw;
  }

  /* Read cycle count */
  ret =
    star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_cycle_count, &soc_data->cycle_count);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to read cycle count: %s", esp_err_to_name(ret));
  }

  return ESP_OK;
}

/* --- Status Functions --- */

esp_err_t star_bms_bq7850_read_status(const bq7850_handle_t* handle, bq7850_status_t* status)
{
  if (handle == NULL || status == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;
  esp_err_t           ret;

  memset(status, 0, sizeof(bq7850_status_t));

  /* Read battery status */
  ret = star_smbus_read_word(manager,
                             bus_name,
                             addr,
                             k_bq7850_cmd_battery_status,
                             &status->battery_status);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read battery status: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Read safety status */
  ret = star_smbus_read_word(manager,
                             bus_name,
                             addr,
                             k_bq7850_cmd_safety_status,
                             &status->safety_status);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to read safety status: %s", esp_err_to_name(ret));
  }

  /* Read operation status */
  ret = star_smbus_read_word(manager,
                             bus_name,
                             addr,
                             k_bq7850_cmd_operation_status,
                             &status->operation_status);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to read operation status: %s", esp_err_to_name(ret));
  }

  /* Read alarm/warning */
  ret = star_smbus_read_word(manager,
                             bus_name,
                             addr,
                             k_bq7850_cmd_alarm_warning,
                             &status->alarm_warning);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to read alarm/warning: %s", esp_err_to_name(ret));
  }

  /* Parse status flags */
  status->charging      = !(status->battery_status & k_bq7850_battery_status_dsg);
  status->discharging   = (status->battery_status & k_bq7850_battery_status_dsg) != 0;
  status->fully_charged = (status->battery_status & k_bq7850_battery_status_fc) != 0;
  status->fault_active  = (status->safety_status != 0);

  return ESP_OK;
}

esp_err_t star_bms_bq7850_read_battery_state(const bq7850_handle_t*  handle,
                                             bq7850_battery_state_t* state)
{
  if (handle == NULL || state == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret;

  memset(state, 0, sizeof(bq7850_battery_state_t));

  /* Read all battery parameters */
  ret = star_bms_bq7850_read_cells(handle, &state->cells);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read cell data: %s", esp_err_to_name(ret));
  }

  ret = star_bms_bq7850_read_temperatures(handle, &state->temps);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to read temperature data: %s", esp_err_to_name(ret));
  }

  ret = star_bms_bq7850_read_current(handle, &state->current);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read current data: %s", esp_err_to_name(ret));
  }

  ret = star_bms_bq7850_read_soc(handle, &state->soc);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read SOC data: %s", esp_err_to_name(ret));
  }

  ret = star_bms_bq7850_read_status(handle, &state->status);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to read status: %s", esp_err_to_name(ret));
  }

  return ESP_OK;
}

/* --- Cell Balancing Functions --- */

esp_err_t star_bms_bq7850_enable_cell_balancing(const bq7850_handle_t* handle, uint16_t cell_mask)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;

  ESP_LOGI(s_tag, "Enabling cell balancing, mask: 0x%04X", cell_mask);

  return star_smbus_write_word(manager, bus_name, addr, k_bq7850_cmd_cell_balance_ctrl, cell_mask);
}

esp_err_t star_bms_bq7850_disable_cell_balancing(const bq7850_handle_t* handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t*   manager              = handle->manager;
  const char*           bus_name             = handle->bus_name;
  uint8_t               addr                 = handle->smbus_addr;
  static const uint16_t s_balancing_disabled = 0x0000;

  ESP_LOGI(s_tag, "Disabling cell balancing");

  return star_smbus_write_word(manager,
                               bus_name,
                               addr,
                               k_bq7850_cmd_cell_balance_ctrl,
                               s_balancing_disabled);
}

esp_err_t star_bms_bq7850_get_balancing_status(const bq7850_handle_t* handle, uint16_t* active_mask)
{
  if (handle == NULL || active_mask == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;

  return star_smbus_read_word(manager, bus_name, addr, k_bq7850_cmd_cell_balance_ctrl, active_mask);
}

/* --- Protection Functions --- */

esp_err_t star_bms_bq7850_read_protection(const bq7850_handle_t* handle,
                                          bq7850_protection_t*   protection)
{
  if (handle == NULL || protection == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;
  esp_err_t           ret;

  memset(protection, 0, sizeof(bq7850_protection_t));

  /* Read protection thresholds via manufacturer access */
  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_protection_ov_thresh,
                                            &protection->overvoltage_mv);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to read OV threshold: %s", esp_err_to_name(ret));
  }

  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_protection_uv_thresh,
                                            &protection->undervoltage_mv);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to read UV threshold: %s", esp_err_to_name(ret));
  }

  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_protection_oc_thresh,
                                            &protection->overcharge_ma);
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Failed to read OC threshold: %s", esp_err_to_name(ret));
  }

  return ESP_OK;
}

esp_err_t star_bms_bq7850_write_protection(const bq7850_handle_t*     handle,
                                           const bq7850_protection_t* protection)
{
  if (handle == NULL || protection == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;
  esp_err_t           ret;

  ESP_LOGW(s_tag, "Writing protection thresholds (device must be unsealed)");

  /* Write protection thresholds via manufacturer access */
  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_protection_ov_thresh,
                                            NULL);
  if (ret == ESP_OK) {
    ret = star_smbus_write_word(manager,
                                bus_name,
                                addr,
                                k_bq7850_cmd_manufacturer_access,
                                protection->overvoltage_mv);
  }

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to write OV threshold: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Add delay between writes (required for BQ7850 EEPROM write settling) */
  vTaskDelay(pdMS_TO_TICKS(k_bq7850_cmd_write_delay_ms));

  /* Write undervoltage threshold */
  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_protection_uv_thresh,
                                            NULL);
  if (ret == ESP_OK) {
    ret = star_smbus_write_word(manager,
                                bus_name,
                                addr,
                                k_bq7850_cmd_manufacturer_access,
                                protection->undervoltage_mv);
  }

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to write UV threshold: %s", esp_err_to_name(ret));
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(k_bq7850_cmd_write_delay_ms));

  /* Write overcurrent threshold */
  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_protection_oc_thresh,
                                            NULL);
  if (ret == ESP_OK) {
    ret = star_smbus_write_word(manager,
                                bus_name,
                                addr,
                                k_bq7850_cmd_manufacturer_access,
                                protection->overcharge_ma);
  }

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to write OC threshold: %s", esp_err_to_name(ret));
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(k_bq7850_cmd_write_delay_ms));

  /* Write overtemperature threshold */
  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_protection_ot_thresh,
                                            NULL);
  if (ret == ESP_OK) {
    ret = star_smbus_write_word(manager,
                                bus_name,
                                addr,
                                k_bq7850_cmd_manufacturer_access,
                                (uint16_t)protection->overtemp_c);
  }

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to write OT threshold: %s", esp_err_to_name(ret));
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(k_bq7850_cmd_write_delay_ms));

  /* Write undertemperature threshold */
  ret = internal_bq7850_manufacturer_access(manager,
                                            bus_name,
                                            addr,
                                            k_bq7850_subcmd_protection_ut_thresh,
                                            NULL);
  if (ret == ESP_OK) {
    ret = star_smbus_write_word(manager,
                                bus_name,
                                addr,
                                k_bq7850_cmd_manufacturer_access,
                                (uint16_t)protection->undertemp_c);
  }

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to write UT threshold: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_tag, "All protection thresholds updated (OV, UV, OC, OT, UT)");

  return ESP_OK;
}

/* --- FET Control Functions --- */

esp_err_t
star_bms_bq7850_control_fets(const bq7850_handle_t* handle, bool charge_fet, bool discharge_fet)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager     = handle->manager;
  const char*         bus_name    = handle->bus_name;
  uint8_t             addr        = handle->smbus_addr;
  uint16_t            fet_control = 0;

  if (charge_fet) {
    fet_control |= k_bq7850_fet_control_chg_fet;
  }

  if (discharge_fet) {
    fet_control |= k_bq7850_fet_control_dsg_fet;
  }

  ESP_LOGI(s_tag, "Setting FET control: CHG=%d DSG=%d", charge_fet, discharge_fet);

  return star_smbus_write_word(manager, bus_name, addr, k_bq7850_cmd_fet_control, fet_control);
}

/* --- Helper Functions --- */

/** Temperature conversion divisor (convert 0.1C to C) */
static const float s_temp_divisor = 10.0f;

float star_bms_bq7850_convert_temp_to_celsius(int16_t temp_01k)
{
  return (temp_01k - k_bq7850_kelvin_offset_deci_c) / s_temp_divisor;
}

bool star_bms_bq7850_is_fault_active(const bq7850_status_t* status)
{
  if (status == NULL) {
    return false;
  }

  return (status->safety_status != 0) || (status->alarm_warning != 0);
}

size_t star_bms_bq7850_status_to_string(const bq7850_status_t* status, char* buffer, size_t size)
{
  if (status == NULL || buffer == NULL || size == 0) {
    return 0;
  }

  size_t offset = 0;

  /* Helper macro to safely append to buffer with overflow protection */
#define SAFE_APPEND(fmt, ...)                                                                      \
  do {                                                                                             \
    if (offset < size) {                                                                           \
      int written = snprintf(buffer + offset, size - offset, fmt, ##__VA_ARGS__);                  \
      if (written > 0) {                                                                           \
        offset += (size_t)written;                                                                 \
        if (offset > size) {                                                                       \
          offset = size;                                                                           \
        }                                                                                          \
      }                                                                                            \
    }                                                                                              \
  } while (0)

  SAFE_APPEND("Battery Status: ");

  if (status->charging) {
    SAFE_APPEND("CHARGING ");
  }
  if (status->discharging) {
    SAFE_APPEND("DISCHARGING ");
  }
  if (status->fully_charged) {
    SAFE_APPEND("FULL ");
  }

  if (status->safety_status) {
    SAFE_APPEND("\nSafety: ");
    if (status->safety_status & k_bq7850_safety_status_cuv) {
      SAFE_APPEND("CUV ");
    }
    if (status->safety_status & k_bq7850_safety_status_cov) {
      SAFE_APPEND("COV ");
    }
    if (status->safety_status & k_bq7850_safety_status_occ) {
      SAFE_APPEND("OCC ");
    }
    if (status->safety_status & k_bq7850_safety_status_ocd) {
      SAFE_APPEND("OCD ");
    }
  }

#undef SAFE_APPEND

  return offset;
}
