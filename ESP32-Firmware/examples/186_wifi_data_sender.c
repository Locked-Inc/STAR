/**
 * @file 186_wifi_data_sender.c
 * @brief ESP32 sends sensor data to host over WiFi TCP
 *
 * Demonstrates:
 * - WiFi station connection
 * - TCP client socket
 * - JSON sensor data transmission
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"
#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <lwip/sockets.h>
#include <string.h>

static const char *s_tag = "WIFI_SENDER";

#define WIFI_SSID       "YourSSID"
#define WIFI_PASSWORD   "YourPassword"
#define HOST_IP         "192.168.1.100"
#define HOST_PORT (8080)

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT (BIT0)

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static int connect_to_host(void)
{
  struct sockaddr_in dest_addr;
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(HOST_PORT);
  inet_pton(AF_INET, HOST_IP, &dest_addr.sin_addr);

  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (sock < 0) return -1;

  if (connect(sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) != 0) {
    close(sock);
    return -1;
  }

  return sock;
}

void wifi_sender_example(void)
{
  /* Initialize NVS and WiFi */
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  s_wifi_event_group = xEventGroupCreate();
  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_cfg);

  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

  wifi_config_t sta_config = {
    .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD },
  };
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &sta_config);
  esp_wifi_start();

  xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
  ESP_LOGI(s_tag, "WiFi connected");

  /* Initialize sensors */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Sensor_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);

  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_handle_t imu;
  dht22_handle_t dht;
  mpu6050_config_t imu_cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  dht22_config_t dht_cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };
  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &imu_cfg);
  star_sensor_dht22_init(&dht, &dht_cfg);

  /* Connect and send data */
  int sock = connect_to_host();
  if (sock < 0) {
    ESP_LOGE(s_tag, "Failed to connect to host");
    return;
  }

  ESP_LOGI(s_tag, "Connected to %s:%d", HOST_IP, HOST_PORT);

  for (int i = 0; i < 100; i++) {
    mpu6050_accel_t accel;
    dht22_data_t dht_data;
    star_sensor_mpu6050_read_accel(&imu, &accel);
    star_sensor_dht22_read(&dht, &dht_data);

    char json[256];
    snprintf(json, sizeof(json),
             "{\"seq\":%d,\"accel\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"
             "\"temp\":%.1f,\"humidity\":%.1f}\n",
             i, accel.x_g, accel.y_g, accel.z_g,
             dht_data.temperature_c, dht_data.humidity_rh);

    int sent = send(sock, json, strlen(json), 0);
    if (sent < 0) {
      ESP_LOGE(s_tag, "Send failed, reconnecting...");
      close(sock);
      sock = connect_to_host();
      continue;
    }

    ESP_LOGI(s_tag, "Sent: %s", json);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  close(sock);
  star_sensor_mpu6050_deinit(&imu);
  star_sensor_dht22_deinit(&dht);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { wifi_sender_example(); }
