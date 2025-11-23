/**
 * @file 195_ota_update.c
 * @brief Over-the-air firmware update
 */

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "OTA_UPDATE";

#define WIFI_SSID       "YourSSID"
#define WIFI_PASSWORD   "YourPassword"
#define OTA_URL         "http://192.168.1.100:8080/firmware.bin"

static void print_partition_info(void)
{
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* boot = esp_ota_get_boot_partition();

  ESP_LOGI(s_tag, "Running partition: %s at 0x%lx", running->label, running->address);
  ESP_LOGI(s_tag, "Boot partition: %s at 0x%lx", boot->label, boot->address);

  esp_app_desc_t app_desc;
  esp_ota_get_partition_description(running, &app_desc);
  ESP_LOGI(s_tag, "Firmware version: %s", app_desc.version);
  ESP_LOGI(s_tag, "Build date: %s %s", app_desc.date, app_desc.time);
}

static esp_err_t perform_ota(void)
{
  ESP_LOGI(s_tag, "Starting OTA from: %s", OTA_URL);

  esp_http_client_config_t http_config = {
    .url = OTA_URL,
    .timeout_ms = 60000,
  };

  esp_https_ota_config_t ota_config = {
    .http_config = &http_config,
  };

  esp_err_t ret = esp_https_ota(&ota_config);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "OTA succeeded! Rebooting...");
    esp_restart();
  } else {
    ESP_LOGE(s_tag, "OTA failed: %s", esp_err_to_name(ret));
  }

  return ret;
}

void ota_update_example(void)
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

  print_partition_info();

  /* Check for OTA update */
  ESP_LOGI(s_tag, "Checking for firmware update...");
  perform_ota();

  /* Normal operation if no update */
  ESP_LOGI(s_tag, "Running normal operation");
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void) { ota_update_example(); }
