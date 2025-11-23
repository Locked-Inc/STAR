/**
 * @file 200_http_server.c
 * @brief HTTP server exposing sensor data REST API
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_dht22.h"
#include "star_sensor_mpu6050.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "HTTP_SERVER";

#define WIFI_SSID     "YourSSID"
#define WIFI_PASSWORD "YourPassword"

static dht22_handle_t s_dht;
static mpu6050_handle_t s_imu;

static esp_err_t sensor_get_handler(httpd_req_t* req)
{
  dht22_data_t dht_data;
  mpu6050_accel_t accel;
  mpu6050_gyro_t gyro;
  star_sensor_dht22_read(&s_dht, &dht_data);
  star_sensor_mpu6050_read_accel(&s_imu, &accel);
  star_sensor_mpu6050_read_gyro(&s_imu, &gyro);

  char json[512];
  snprintf(json, sizeof(json),
           "{\"temperature\":%.1f,\"humidity\":%.1f,"
           "\"accel\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"
           "\"gyro\":{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f}}",
           dht_data.temperature_c, dht_data.humidity_rh,
           accel.x_g, accel.y_g, accel.z_g,
           gyro.x_dps, gyro.y_dps, gyro.z_dps);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json, strlen(json));
  return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t* req)
{
  const char *html = "<!DOCTYPE html><html><head><title>ESP32 Sensors</title></head>"
                     "<body><h1>ESP32 Sensor Dashboard</h1>"
                     "<div id='data'></div>"
                     "<script>"
                     "setInterval(()=>fetch('/api/sensors').then(r=>r.json())"
                     ".then(d=>document.getElementById('data').innerHTML="
                     "'<p>Temp: '+d.temperature+'°C</p><p>Humidity: '+d.humidity+'%</p>'),1000);"
                     "</script></body></html>";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, html, strlen(html));
  return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = NULL;

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
    httpd_uri_t api = { .uri = "/api/sensors", .method = HTTP_GET, .handler = sensor_get_handler };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &api);
  }

  return server;
}

void http_server_example(void)
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

  /* Initialize sensors */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "HTTP_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);

  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

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
  star_sensor_mpu6050_init(&s_imu, &manager, "mpu6050", &imu_cfg);
  star_sensor_dht22_init(&s_dht, &dht_cfg);

  httpd_handle_t server = start_webserver();
  ESP_LOGI(s_tag, "HTTP server started on port 80");

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void) { http_server_example(); }
