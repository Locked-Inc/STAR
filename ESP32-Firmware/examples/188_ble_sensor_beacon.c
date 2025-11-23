/**
 * @file 188_ble_sensor_beacon.c
 * @brief BLE advertising sensor data as beacon
 *
 * IMPORTANT: Bluetooth support is NOT enabled by default
 * To use this example, add the following to platformio.ini:
 *
 * [env]
 * build_flags =
 *     -DCONFIG_BT_ENABLED=1
 *     -DCONFIG_BLUEDROID_ENABLED=1
 * lib_deps =
 *     ...existing deps...
 *     bt
 *
 * Demonstrates:
 * - BLE GATT server
 * - Sensor characteristics
 * - Low power advertising
 */

#include "star_sensor_dht22.h"

#include <esp_log.h>
/* #include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_main.h> */
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "BLE_BEACON";

#define DEVICE_NAME "ESP32-Sensor"
#define SERVICE_UUID (0x181A) /* Environmental Sensing */
#define TEMP_CHAR_UUID (0x2A6E)
#define HUMIDITY_CHAR_UUID (0x2A6F)

/* BLE structures and callbacks disabled - uncomment when bt component is enabled
static esp_ble_adv_params_t adv_params = {
  .adv_int_min = 0x20,
  .adv_int_max = 0x40,
  .adv_type = ADV_TYPE_IND,
  .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
  .channel_map = ADV_CHNL_ALL,
  .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static uint16_t s_gatts_handle_table[4];

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param)
{
  if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT) {
    esp_ble_gap_start_advertising(&adv_params);
  }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t* param)
{
  switch (event) {
    case ESP_GATTS_REG_EVT:
      esp_ble_gap_set_device_name(DEVICE_NAME);
      esp_ble_gap_config_adv_data_raw((uint8_t*)"\x02\x01\x06\x03\x03\x1A\x18", 7);
      break;

    case ESP_GATTS_READ_EVT:
      if (param->read.handle == s_gatts_handle_table[1]) {
        int16_t temp_val = (int16_t)(s_current_temp * 100);
        esp_gatt_rsp_t rsp = {0};
        rsp.attr_value.len = 2;
        rsp.attr_value.value[0] = temp_val & 0xFF;
        rsp.attr_value.value[1] = (temp_val >> 8) & 0xFF;
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
      } else if (param->read.handle == s_gatts_handle_table[2]) {
        uint16_t hum_val = (uint16_t)(s_current_humidity * 100);
        esp_gatt_rsp_t rsp = {0};
        rsp.attr_value.len = 2;
        rsp.attr_value.value[0] = hum_val & 0xFF;
        rsp.attr_value.value[1] = (hum_val >> 8) & 0xFF;
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
      }
      break;

    default:
      break;
  }
}
*/
static float s_current_temp = 0;
static float s_current_humidity = 0;

void ble_sensor_beacon_example(void)
{
  nvs_flash_init();

  /* BLE initialization disabled - enable bt component first
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bt_controller_init(&bt_cfg);
  esp_bt_controller_enable(ESP_BT_MODE_BLE);

  esp_bluedroid_init();
  esp_bluedroid_enable();

  esp_ble_gap_register_callback(gap_event_handler);
  esp_ble_gatts_register_callback(gatts_event_handler);
  esp_ble_gatts_app_register(0);

  ESP_LOGI(s_tag, "BLE beacon started: %s", DEVICE_NAME);
  */
  ESP_LOGI(s_tag, "Bluetooth disabled - see file header for enabling instructions");

  /* Initialize sensor */
  dht22_handle_t dht;
  dht22_config_t cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };
  star_sensor_dht22_init(&dht, &cfg);

  /* Read sensor values periodically (without BLE advertising) */
  for (int i = 0; i < 20; i++) {
    dht22_data_t data;
    if (star_sensor_dht22_read(&dht, &data) == ESP_OK) {
      s_current_temp = data.temperature_c;
      s_current_humidity = data.humidity_rh;
      ESP_LOGI(s_tag, "Sensor data: Temp=%.1fC, Humidity=%.1f%%", s_current_temp, s_current_humidity);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  star_sensor_dht22_deinit(&dht);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { ble_sensor_beacon_example(); }
