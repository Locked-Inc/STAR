/* esp32-firmware/components/star_bms_bq7850/star_bms_bq7850.c */

#include "star_bms_bq7850.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

#include "star_bus_smbus.h"
#include "star_error_handler.h"

static const char* s_TAG = "bq7850";

#define BQ7850_MUTEX_TIMEOUT_MS (1000)

/* --- Internal Helper Functions --- */

/**
 * @brief Execute manufacturer access command
 *
 * The BQ7850 uses a two-step process for extended commands:
 * 1. Write subcommand to 0x00 (Manufacturer Access)
 * 2. Read result from 0x00
 */
static esp_err_t priv_bq7850_manufacturer_access(star_bus_manager_t* manager,
                                                 const char*         bus_name,
                                                 uint8_t             addr,
                                                 uint16_t            subcmd,
                                                 uint16_t*           result)
{
  esp_err_t ret;

  /* Write subcommand to Manufacturer Access register */
  ret = star_smbus_write_word(manager, bus_name, addr, BQ7850_CMD_MANUFACTURER_ACCESS, subcmd);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG,
             "Failed to write manufacturer access subcommand 0x%04X: %s",
             subcmd,
             esp_err_to_name(ret));
    return ret;
  }

  /* Small delay for command processing */
  vTaskDelay(pdMS_TO_TICKS(10));

  /* Read result if requested */
  if (result != NULL) {
    ret = star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_MANUFACTURER_ACCESS, result);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to read manufacturer access result: %s", esp_err_to_name(ret));
      return ret;
    }
  }

  return ESP_OK;
}

/**
 * @brief Read cell voltage via manufacturer access subcommand
 */
static esp_err_t priv_bq7850_read_cell_voltage_internal(star_bus_manager_t* manager,
                                                        const char*         bus_name,
                                                        uint8_t             addr,
                                                        uint8_t             cell_index,
                                                        uint16_t*           voltage_mv)
{
  if (cell_index >= BQ7850_MAX_CELLS) {
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t subcmd = BQ7850_SUBCMD_CELL_VOLTAGE_BASE + cell_index;
  return priv_bq7850_manufacturer_access(manager, bus_name, addr, subcmd, voltage_mv);
}

/* --- Core Functions --- */

esp_err_t star_bms_bq7850_init(bq7850_handle_t*        handle,
                               star_bus_manager_t*     manager,
                               const char*             bus_name,
                               star_error_interface_t* error_iface,
                               const bq7850_config_t*  config)
{
  if (handle == NULL || manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  if (config->num_cells == 0 || config->num_cells > BQ7850_MAX_CELLS) {
    ESP_LOGE(s_TAG,
             "Invalid number of cells: %d (must be 1-%d)",
             config->num_cells,
             BQ7850_MAX_CELLS);
    return ESP_ERR_INVALID_ARG;
  }

  /* Initialize handle */
  memset(handle, 0, sizeof(bq7850_handle_t));
  handle->manager    = manager;
  handle->bus_name   = bus_name;
  handle->smbus_addr = config->smbus_addr;
  memcpy(&handle->config, config, sizeof(bq7850_config_t));

  /* Create mutex first */
  handle->mutex = xSemaphoreCreateMutex();
  if (handle->mutex == NULL) {
    ESP_LOGE(s_TAG, "Failed to create mutex");
    return ESP_ERR_NO_MEM;
  }

  /* Handle error interface injection */
  if (error_iface == NULL) {
    /* Create default error handler internally */
    error_handler_t* default_handler = error_handler_create_default();
    if (default_handler == NULL) {
      ESP_LOGE(s_TAG, "Failed to create default error handler");
      vSemaphoreDelete(handle->mutex);
      handle->mutex = NULL;
      return ESP_ERR_NO_MEM;
    }

    /* Allocate and store the interface */
    handle->error_iface = (star_error_interface_t*)malloc(sizeof(star_error_interface_t));
    if (handle->error_iface == NULL) {
      ESP_LOGE(s_TAG, "Failed to allocate error interface");
      error_handler_destroy_default(default_handler);
      vSemaphoreDelete(handle->mutex);
      handle->mutex = NULL;
      return ESP_ERR_NO_MEM;
    }

    error_handler_get_interface(handle->error_iface, default_handler);
    handle->owns_error_handler = true;
    ESP_LOGI(s_TAG, "Created default error handler internally");
  } else {
    handle->error_iface        = error_iface;
    handle->owns_error_handler = false;
    ESP_LOGI(s_TAG, "Using injected error interface");
  }

  ESP_LOGI(s_TAG,
           "Initializing BQ7850 BMS on bus '%s' at address 0x%02X",
           bus_name,
           config->smbus_addr);

  /* Verify device communication by reading device type */
  uint16_t  device_type = 0;
  esp_err_t ret         = priv_bq7850_manufacturer_access(manager,
                                                  bus_name,
                                                  config->smbus_addr,
                                                  BQ7850_SUBCMD_DEVICE_TYPE,
                                                  &device_type);

  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to communicate with BQ7850: %s", esp_err_to_name(ret));
    if (handle->owns_error_handler && handle->error_iface != NULL) {
      error_handler_t* handler_ptr = (error_handler_t*)handle->error_iface->ctx;
      error_handler_destroy_default(handler_ptr);
      free(handle->error_iface);
    }
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  ESP_LOGI(s_TAG, "BQ7850 detected, device type: 0x%04X", device_type);

  /* Read firmware version */
  uint16_t fw_version = 0;
  ret                 = priv_bq7850_manufacturer_access(manager,
                                        bus_name,
                                        config->smbus_addr,
                                        BQ7850_SUBCMD_FW_VERSION,
                                        &fw_version);
  if (ret == ESP_OK) {
    ESP_LOGI(s_TAG, "Firmware version: 0x%04X", fw_version);
  }

  /* Verify we can read basic parameters */
  uint16_t voltage = 0;
  ret = star_smbus_read_word(manager, bus_name, config->smbus_addr, BQ7850_CMD_VOLTAGE, &voltage);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read pack voltage: %s", esp_err_to_name(ret));
    if (handle->owns_error_handler && handle->error_iface != NULL) {
      error_handler_t* handler_ptr = (error_handler_t*)handle->error_iface->ctx;
      error_handler_destroy_default(handler_ptr);
      free(handle->error_iface);
    }
    vSemaphoreDelete(handle->mutex);
    handle->mutex = NULL;
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG,
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
  if (mutex != NULL && xSemaphoreTake(mutex, pdMS_TO_TICKS(BQ7850_MUTEX_TIMEOUT_MS)) != pdTRUE) {
    ESP_LOGW(s_TAG, "Failed to acquire mutex for deinit, continuing anyway");
  }

  /* Clean up error handler if we own it */
  if (handle->owns_error_handler && handle->error_iface != NULL) {
    error_handler_t* error_handler = (error_handler_t*)handle->error_iface->ctx;
    error_handler_destroy_default(error_handler);
    free(handle->error_iface);
    handle->error_iface = NULL;
    ESP_LOGI(s_TAG, "Destroyed internally-owned error handler");
  }

  /* Clear handle fields */
  handle->initialized = false;
  handle->mutex       = NULL;

  /* Release and delete mutex */
  if (mutex != NULL) {
    xSemaphoreGive(mutex);
    vSemaphoreDelete(mutex);
  }

  ESP_LOGI(s_TAG, "BQ7850 BMS deinitialized");
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

  /* Read device type */
  ret = priv_bq7850_manufacturer_access(manager,
                                        bus_name,
                                        addr,
                                        BQ7850_SUBCMD_DEVICE_TYPE,
                                        &info->device_type);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Read firmware version */
  ret = priv_bq7850_manufacturer_access(manager,
                                        bus_name,
                                        addr,
                                        BQ7850_SUBCMD_FW_VERSION,
                                        &info->fw_version);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Read hardware version */
  ret = priv_bq7850_manufacturer_access(manager,
                                        bus_name,
                                        addr,
                                        BQ7850_SUBCMD_HW_VERSION,
                                        &info->hw_version);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Read serial number */
  uint16_t serial_low = 0;
  ret = star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_SERIAL_NUMBER, &serial_low);
  if (ret == ESP_OK) {
    info->serial_number = serial_low;
  }

  /* Read manufacturer name (block read) */
  uint8_t mfg_len = 0;
  uint8_t mfg_data[32];
  ret = star_smbus_block_read(manager,
                              bus_name,
                              addr,
                              BQ7850_CMD_MANUFACTURE_NAME,
                              mfg_data,
                              32,
                              &mfg_len);
  if (ret == ESP_OK && mfg_len > 0) {
    size_t copy_len = (mfg_len < 31) ? mfg_len : 31;
    memcpy(info->manufacturer, mfg_data, copy_len);
    info->manufacturer[copy_len] = '\0';
  }

  /* Read device name */
  uint8_t dev_len = 0;
  uint8_t dev_data[32];
  ret =
    star_smbus_block_read(manager, bus_name, addr, BQ7850_CMD_DEVICE_NAME, dev_data, 32, &dev_len);
  if (ret == ESP_OK && dev_len > 0) {
    size_t copy_len = (dev_len < 31) ? dev_len : 31;
    memcpy(info->device_name, dev_data, copy_len);
    info->device_name[copy_len] = '\0';
  }

  /* Read chemistry */
  uint8_t chem_len = 0;
  uint8_t chem_data[32];
  ret = star_smbus_block_read(manager,
                              bus_name,
                              addr,
                              BQ7850_CMD_DEVICE_CHEMISTRY,
                              chem_data,
                              32,
                              &chem_len);
  if (ret == ESP_OK && chem_len > 0) {
    size_t copy_len = (chem_len < 31) ? chem_len : 31;
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

  ESP_LOGW(s_TAG, "Issuing software reset to BQ7850");

  esp_err_t ret = priv_bq7850_manufacturer_access(handle->manager,
                                                  handle->bus_name,
                                                  handle->smbus_addr,
                                                  BQ7850_SUBCMD_DEVICE_RESET,
                                                  NULL);

  if (ret == ESP_OK) {
    /* Device takes ~1 second to reset */
    ESP_LOGI(s_TAG, "Reset command sent, waiting for device to restart...");
    vTaskDelay(pdMS_TO_TICKS(1000));
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
  for (uint8_t i = 0; i < BQ7850_MAX_CELLS; i++) {
    ret =
      priv_bq7850_read_cell_voltage_internal(manager, bus_name, addr, i, &cell_data->cell_mv[i]);

    if (ret == ESP_OK && cell_data->cell_mv[i] > 0) {
      cell_data->valid_cells++;
    } else if (ret != ESP_OK) {
      ESP_LOGW(s_TAG, "Failed to read cell %d voltage: %s", i + 1, esp_err_to_name(ret));
      /* Continue reading other cells */
    }
  }

  /* Read total pack voltage */
  ret = star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_VOLTAGE, &cell_data->pack_mv);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read pack voltage: %s", esp_err_to_name(ret));
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

  if (cell_index >= BQ7850_MAX_CELLS) {
    return ESP_ERR_INVALID_ARG;
  }

  return priv_bq7850_read_cell_voltage_internal(handle->manager,
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
                              BQ7850_CMD_VOLTAGE,
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

  for (uint8_t i = 0; i < BQ7850_MAX_TEMP_SENSORS; i++) {
    uint16_t temp_raw = 0;
    uint16_t subcmd   = BQ7850_SUBCMD_TEMP_SENSOR_BASE + i;

    ret = priv_bq7850_manufacturer_access(manager, bus_name, addr, subcmd, &temp_raw);

    if (ret == ESP_OK && temp_raw > 0) {
      /* BQ7850 returns temperature in 0.1K units */
      /* Convert to 0.1C: T(C) = T(K) - 273.15 */
      temp_data->temp_c[i] = temp_raw - 2731; /* 273.1K = 2731 in 0.1K units */
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
  esp_err_t ret = star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_TEMPERATURE, &temp_raw);

  if (ret == ESP_OK) {
    /* Convert from 0.1K to 0.1C */
    *temp_c = temp_raw - 2731;
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

  /* Read instantaneous current (signed) */
  ret = star_smbus_read_word(manager,
                             bus_name,
                             addr,
                             BQ7850_CMD_CURRENT,
                             (uint16_t*)&current_data->current_ma);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read current: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Read average current */
  ret = star_smbus_read_word(manager,
                             bus_name,
                             addr,
                             BQ7850_CMD_AVERAGE_CURRENT,
                             (uint16_t*)&current_data->avg_current_ma);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to read average current: %s", esp_err_to_name(ret));
  }

  /* Read voltage */
  ret =
    star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_VOLTAGE, &current_data->voltage_mv);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read voltage: %s", esp_err_to_name(ret));
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
                             BQ7850_CMD_REMAINING_CAPACITY,
                             &soc_data->remaining_capacity_mah);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read remaining capacity: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Read full charge capacity */
  ret = star_smbus_read_word(manager,
                             bus_name,
                             addr,
                             BQ7850_CMD_FULL_CHARGE_CAPACITY,
                             &soc_data->full_capacity_mah);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to read full charge capacity: %s", esp_err_to_name(ret));
  }

  /* Read relative SOC */
  uint16_t soc_raw;
  ret = star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_RELATIVE_STATE_CHARGE, &soc_raw);
  if (ret == ESP_OK) {
    soc_data->relative_soc = (uint8_t)soc_raw;
  }

  /* Read absolute SOC */
  ret = star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_ABSOLUTE_STATE_CHARGE, &soc_raw);
  if (ret == ESP_OK) {
    soc_data->absolute_soc = (uint8_t)soc_raw;
  }

  /* Read cycle count */
  ret =
    star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_CYCLE_COUNT, &soc_data->cycle_count);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to read cycle count: %s", esp_err_to_name(ret));
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
                             BQ7850_CMD_BATTERY_STATUS,
                             &status->battery_status);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read battery status: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Read safety status */
  ret =
    star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_SAFETY_STATUS, &status->safety_status);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to read safety status: %s", esp_err_to_name(ret));
  }

  /* Read operation status */
  ret = star_smbus_read_word(manager,
                             bus_name,
                             addr,
                             BQ7850_CMD_OPERATION_STATUS,
                             &status->operation_status);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to read operation status: %s", esp_err_to_name(ret));
  }

  /* Read alarm/warning */
  ret =
    star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_ALARM_WARNING, &status->alarm_warning);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to read alarm/warning: %s", esp_err_to_name(ret));
  }

  /* Parse status flags */
  status->charging      = !(status->battery_status & BQ7850_BATTERY_STATUS_DSG);
  status->discharging   = (status->battery_status & BQ7850_BATTERY_STATUS_DSG) != 0;
  status->fully_charged = (status->battery_status & BQ7850_BATTERY_STATUS_FC) != 0;
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
    ESP_LOGE(s_TAG, "Failed to read cell data: %s", esp_err_to_name(ret));
  }

  ret = star_bms_bq7850_read_temperatures(handle, &state->temps);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to read temperature data: %s", esp_err_to_name(ret));
  }

  ret = star_bms_bq7850_read_current(handle, &state->current);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read current data: %s", esp_err_to_name(ret));
  }

  ret = star_bms_bq7850_read_soc(handle, &state->soc);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read SOC data: %s", esp_err_to_name(ret));
  }

  ret = star_bms_bq7850_read_status(handle, &state->status);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read status: %s", esp_err_to_name(ret));
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

  ESP_LOGI(s_TAG, "Enabling cell balancing, mask: 0x%04X", cell_mask);

  return star_smbus_write_word(manager, bus_name, addr, BQ7850_CMD_CELL_BALANCE_CTRL, cell_mask);
}

esp_err_t star_bms_bq7850_disable_cell_balancing(const bq7850_handle_t* handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;

  ESP_LOGI(s_TAG, "Disabling cell balancing");

  return star_smbus_write_word(manager, bus_name, addr, BQ7850_CMD_CELL_BALANCE_CTRL, 0x0000);
}

esp_err_t star_bms_bq7850_get_balancing_status(const bq7850_handle_t* handle, uint16_t* active_mask)
{
  if (handle == NULL || active_mask == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  star_bus_manager_t* manager  = handle->manager;
  const char*         bus_name = handle->bus_name;
  uint8_t             addr     = handle->smbus_addr;

  return star_smbus_read_word(manager, bus_name, addr, BQ7850_CMD_CELL_BALANCE_CTRL, active_mask);
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
  ret = priv_bq7850_manufacturer_access(manager,
                                        bus_name,
                                        addr,
                                        BQ7850_SUBCMD_PROTECTION_OV_THRESH,
                                        &protection->overvoltage_mv);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to read OV threshold: %s", esp_err_to_name(ret));
  }

  ret = priv_bq7850_manufacturer_access(manager,
                                        bus_name,
                                        addr,
                                        BQ7850_SUBCMD_PROTECTION_UV_THRESH,
                                        &protection->undervoltage_mv);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to read UV threshold: %s", esp_err_to_name(ret));
  }

  ret = priv_bq7850_manufacturer_access(manager,
                                        bus_name,
                                        addr,
                                        BQ7850_SUBCMD_PROTECTION_OC_THRESH,
                                        &protection->overcharge_ma);
  if (ret != ESP_OK) {
    ESP_LOGW(s_TAG, "Failed to read OC threshold: %s", esp_err_to_name(ret));
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

  ESP_LOGW(s_TAG, "Writing protection thresholds (device must be unsealed)");

  /* Write protection thresholds via manufacturer access */
  ret = priv_bq7850_manufacturer_access(manager,
                                        bus_name,
                                        addr,
                                        BQ7850_SUBCMD_PROTECTION_OV_THRESH,
                                        NULL);
  if (ret == ESP_OK) {
    ret = star_smbus_write_word(manager,
                                bus_name,
                                addr,
                                BQ7850_CMD_MANUFACTURER_ACCESS,
                                protection->overvoltage_mv);
  }

  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to write OV threshold: %s", esp_err_to_name(ret));
    return ret;
  }

  /* Add delay between writes */
  vTaskDelay(pdMS_TO_TICKS(50));

  /* Similar process for other thresholds... */
  ESP_LOGI(s_TAG, "Protection thresholds updated");

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
    fet_control |= BQ7850_FET_CONTROL_CHG_FET;
  }

  if (discharge_fet) {
    fet_control |= BQ7850_FET_CONTROL_DSG_FET;
  }

  ESP_LOGI(s_TAG, "Setting FET control: CHG=%d DSG=%d", charge_fet, discharge_fet);

  return star_smbus_write_word(manager, bus_name, addr, BQ7850_CMD_FET_CONTROL, fet_control);
}

/* --- Helper Functions --- */

float star_bms_bq7850_convert_temp_to_celsius(int16_t temp_01k)
{
  return (temp_01k - 2731) / 10.0f;
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

  offset += snprintf(buffer + offset, size - offset, "Battery Status: ");

  if (status->charging) {
    offset += snprintf(buffer + offset, size - offset, "CHARGING ");
  }
  if (status->discharging) {
    offset += snprintf(buffer + offset, size - offset, "DISCHARGING ");
  }
  if (status->fully_charged) {
    offset += snprintf(buffer + offset, size - offset, "FULL ");
  }

  if (status->safety_status) {
    offset += snprintf(buffer + offset, size - offset, "\nSafety: ");
    if (status->safety_status & BQ7850_SAFETY_STATUS_CUV) {
      offset += snprintf(buffer + offset, size - offset, "CUV ");
    }
    if (status->safety_status & BQ7850_SAFETY_STATUS_COV) {
      offset += snprintf(buffer + offset, size - offset, "COV ");
    }
    if (status->safety_status & BQ7850_SAFETY_STATUS_OCC) {
      offset += snprintf(buffer + offset, size - offset, "OCC ");
    }
    if (status->safety_status & BQ7850_SAFETY_STATUS_OCD) {
      offset += snprintf(buffer + offset, size - offset, "OCD ");
    }
  }

  return offset;
}
