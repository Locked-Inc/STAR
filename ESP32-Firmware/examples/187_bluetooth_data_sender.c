/**
 * @file 187_bluetooth_data_sender.c
 * @brief ESP32 sends sensor data over Bluetooth Classic SPP
 *
 * IMPORTANT: Bluetooth support is NOT enabled by default
 * To use this example, add the following to platformio.ini:
 *
 * [env]
 * build_flags =
 *     -DCONFIG_BT_ENABLED=1
 *     -DCONFIG_CLASSIC_BT_ENABLED=1
 * lib_deps =
 *     ...existing deps...
 *     bt
 *
 * Demonstrates:
 * - Bluetooth Classic SPP server
 * - Sensor data streaming over BT
 * - Mobile app connection
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
/* #include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_bt_device.h>
#include <esp_spp_api.h> */
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "BT_SENDER";

#define SPP_SERVER_NAME "ESP32_Sensor"
#define DEVICE_NAME     "ESP32-STAR"

/* Bluetooth support disabled - uncomment when bt component is enabled
static uint32_t s_spp_handle = 0;
static bool s_client_connected = false;

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t* param)
{
  switch (event) {
    case ESP_SPP_INIT_EVT:
      esp_bt_dev_set_device_name(DEVICE_NAME);
      esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
      esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);
      break;

    case ESP_SPP_SRV_OPEN_EVT:
      ESP_LOGI(s_tag, "Client connected!");
      s_spp_handle = param->srv_open.handle;
      s_client_connected = true;
      break;

    case ESP_SPP_CLOSE_EVT:
      ESP_LOGI(s_tag, "Client disconnected");
      s_client_connected = false;
      break;

    case ESP_SPP_DATA_IND_EVT:
      ESP_LOGI(s_tag, "Received %d bytes", param->data_ind.len);
      break;

    default:
      break;
  }
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param)
{
  if (event == ESP_BT_GAP_AUTH_CMPL_EVT) {
    if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
      ESP_LOGI(s_tag, "Authentication success: %s", param->auth_cmpl.device_name);
    }
  }
}
*/
static bool s_client_connected = false;

void bluetooth_sender_example(void)
{
  /* Initialize NVS */
  nvs_flash_init();

  /* Bluetooth initialization disabled - enable bt component first
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bt_controller_init(&bt_cfg);
  esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);

  esp_bluedroid_init();
  esp_bluedroid_enable();

  esp_bt_gap_register_callback(esp_bt_gap_cb);
  esp_spp_register_callback(esp_spp_cb);
  esp_spp_init(ESP_SPP_MODE_CB);

  ESP_LOGI(s_tag, "Bluetooth SPP initialized, device: %s", DEVICE_NAME);
  */
  ESP_LOGI(s_tag, "Bluetooth disabled - see file header for enabling instructions");

  /* Initialize sensor */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Sensor_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_handle_t imu;
  mpu6050_config_t cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &cfg);

  /* Bluetooth connection disabled
  ESP_LOGI(s_tag, "Waiting for Bluetooth connection...");

  // Stream sensor data when connected
  for (int i = 0; i < 1000; i++) {
    if (s_client_connected) {
      mpu6050_accel_t accel;
      mpu6050_gyro_t gyro;
      star_sensor_mpu6050_read_accel(&imu, &accel);
      star_sensor_mpu6050_read_gyro(&imu, &gyro);

      char msg[128];
      int len = snprintf(msg, sizeof(msg),
                        "A:%.2f,%.2f,%.2f G:%.1f,%.1f,%.1f\n",
                        accel.x_g, accel.y_g, accel.z_g,
                        gyro.x_dps, gyro.y_dps, gyro.z_dps);

      esp_spp_write(s_spp_handle, len, (uint8_t*)msg);
      ESP_LOGI(s_tag, "Sent: %s", msg);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  esp_spp_deinit();
  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  */

  /* Sensor demonstration mode (without Bluetooth) */
  for (int i = 0; i < 20; i++) {
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    star_sensor_mpu6050_read_accel(&imu, &accel);
    star_sensor_mpu6050_read_gyro(&imu, &gyro);

    ESP_LOGI(s_tag, "Sensor data - A:%.2f,%.2f,%.2f G:%.1f,%.1f,%.1f",
             accel.x_g, accel.y_g, accel.z_g,
             gyro.x_dps, gyro.y_dps, gyro.z_dps);

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { bluetooth_sender_example(); }
