/**
 * @file 300_ota_update.c
 * @brief Over-the-air firmware update
 */

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "OTA";

#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASS "YOUR_PASSWORD"
#define OTA_URL   "https://example.com/firmware.bin"

static void priv_wifi_init(void) {
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // Fixed();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = WIFI_SSID,
      .password = WIFI_PASS,
    },
  };
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  esp_wifi_start();
  esp_wifi_connect();
}

static esp_err_t priv_http_event_handler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
      ESP_LOGI(s_tag, "Received %d bytes", evt->data_len);
      break;
    default:
      break;
  }
  return ESP_OK;
}

void ota_update_example(void)
{
  ESP_LOGI(s_tag, "OTA update example");

  /* Print current partition info */
  const esp_partition_t *running = esp_ota_get_running_partition();
  ESP_LOGI(s_tag, "Running partition: %s", running->label);

  esp_app_desc_t app_desc;
  esp_ota_get_partition_description(running, &app_desc);
  ESP_LOGI(s_tag, "Firmware version: %s", app_desc.version);

  priv_wifi_init();
  ESP_LOGI(s_tag, "Waiting for WiFi...");
  vTaskDelay(pdMS_TO_TICKS(10000));

  ESP_LOGI(s_tag, "Starting OTA from: %s", OTA_URL);

  esp_http_client_config_t config = {
    .url = OTA_URL,
    .event_handler = priv_http_event_handler,
    .timeout_ms = 30000,
  };

  esp_https_ota_config_t ota_config = {
    .http_config = &config,
  };

  esp_err_t ret = esp_https_ota(&ota_config);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "OTA successful! Rebooting...");
    esp_restart();
  } else {
    ESP_LOGI(s_tag, "OTA failed: %s", esp_err_to_name(ret));
  }
}

void app_main(void) { ota_update_example(); }
