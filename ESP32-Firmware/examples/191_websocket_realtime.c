/**
 * @file 191_websocket_realtime.c
 * @brief WebSocket real-time sensor streaming
 *
 * NOTE: This example requires the esp_websocket_client component to be available.
 *       To enable WebSocket support in platformio.ini, add the following to the
 *       esp_idf component dependencies (usually in esp-idf folder configuration):
 *
 *       [env:example_191_websocket_realtime]
 *       extends = env:esp32_wroom
 *       platform_packages =
 *           framework-espidf @ 5.1.2  ; or your target version
 *
 *       This example demonstrates:
 *       - WiFi initialization and connection
 *       - WebSocket client setup and event handling
 *       - Real-time sensor data streaming via WebSocket
 *       - JSON formatting of sensor data
 *       - FreeRTOS task scheduling with 50 Hz streaming rate
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
/* WEBSOCKET COMPONENT REQUIRED - Uncomment when esp_websocket_client is available
#include <esp_websocket_client.h>
*/
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "WEBSOCKET";

#define WIFI_SSID       "YourSSID"
#define WIFI_PASSWORD   "YourPassword"
#define WS_URI          "ws://192.168.1.100:8080/sensor"

/* WEBSOCKET COMPONENT REQUIRED - Uncomment when esp_websocket_client is available
static esp_websocket_client_handle_t s_ws_client = NULL;
static bool s_ws_connected = false;

static void ws_event_handler(void *handler_args, esp_event_base_t base,
                             int32_t event_id, void *event_data)
{
  if (event_id == WEBSOCKET_EVENT_CONNECTED) {
    ESP_LOGI(s_tag, "WebSocket connected");
    s_ws_connected = true;
  } else if (event_id == WEBSOCKET_EVENT_DISCONNECTED) {
    ESP_LOGI(s_tag, "WebSocket disconnected");
    s_ws_connected = false;
  }
}
*/

void websocket_example(void)
{
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_cfg);

  wifi_config_t sta_config = {
    .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD },
  };
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &sta_config);
  esp_wifi_start();
  esp_wifi_connect();

  vTaskDelay(pdMS_TO_TICKS(5000));

  /* WEBSOCKET COMPONENT REQUIRED - Uncomment when esp_websocket_client is available
  Initialize WebSocket
  esp_websocket_client_config_t ws_cfg = { .uri = WS_URI };
  s_ws_client = esp_websocket_client_init(&ws_cfg);
  esp_websocket_register_events(s_ws_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
  esp_websocket_client_start(s_ws_client);
  */

  /* Placeholder for WebSocket initialization */
  bool s_ws_connected = false;
  void *s_ws_client = NULL;
  ESP_LOGI(s_tag, "WebSocket component not available. Install esp_websocket_client to enable this feature.");

  /* Initialize sensor */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "IMU_Manager", NULL, NULL);

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

  /* Sensor streaming loop - demonstrates 50 Hz data rate
   * When WebSocket is available, data would be sent via ws_client
   * Currently just logs sensor data for demonstration
   */
  for (int i = 0; i < 50; i++) {
    mpu6050_accel_t accel;
    mpu6050_gyro_t gyro;
    star_sensor_mpu6050_read_accel(&imu, &accel);
    star_sensor_mpu6050_read_gyro(&imu, &gyro);

    char json[256];
    int len = snprintf(json, sizeof(json),
                      "{\"ts\":%lu,\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f,"
                      "\"gx\":%.1f,\"gy\":%.1f,\"gz\":%.1f}",
                      xTaskGetTickCount(), accel.x_g, accel.y_g, accel.z_g,
                      gyro.x_dps, gyro.y_dps, gyro.z_dps);

    /* WEBSOCKET COMPONENT REQUIRED - Uncomment when esp_websocket_client is available
    if (s_ws_connected) {
      esp_websocket_client_send_text(s_ws_client, json, len, portMAX_DELAY);
    }
    */

    /* Log sensor data for now */
    ESP_LOGI(s_tag, "Sensor: %s", json);

    vTaskDelay(pdMS_TO_TICKS(20));  /* 50 Hz */
  }

  /* WEBSOCKET COMPONENT REQUIRED - Uncomment when esp_websocket_client is available
  esp_websocket_client_stop(s_ws_client);
  esp_websocket_client_destroy(s_ws_client);
  */
  star_sensor_mpu6050_deinit(&imu);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { websocket_example(); }
