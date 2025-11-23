/**
 * @file 197_spiffs_config.c
 * @brief SPIFFS configuration storage
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"

#include <esp_log.h>
#include <esp_spiffs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>
#include <string.h>

static const char *s_tag = "SPIFFS_CONFIG";

#define CONFIG_FILE "/spiffs/config.json"

typedef struct {
  char wifi_ssid[32];
  char wifi_password[64];
  int sample_rate_ms;
  float temp_offset;
  bool logging_enabled;
} device_config_t;

static esp_err_t save_config(const device_config_t* cfg)
{
  cJSON* root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "wifi_ssid", cfg->wifi_ssid);
  cJSON_AddStringToObject(root, "wifi_password", cfg->wifi_password);
  cJSON_AddNumberToObject(root, "sample_rate_ms", cfg->sample_rate_ms);
  cJSON_AddNumberToObject(root, "temp_offset", cfg->temp_offset);
  cJSON_AddBoolToObject(root, "logging_enabled", cfg->logging_enabled);

  char *json_str = cJSON_Print(root);
  FILE* f = fopen(CONFIG_FILE, "w");
  if (f) {
    fprintf(f, "%s", json_str);
    fclose(f);
  }

  cJSON_free(json_str);
  cJSON_Delete(root);
  return ESP_OK;
}

static esp_err_t load_config(device_config_t* cfg)
{
  FILE* f = fopen(CONFIG_FILE, "r");
  if (!f) return ESP_ERR_NOT_FOUND;

  char buf[512];
  size_t len = fread(buf, 1, sizeof(buf) - 1, f);
  buf[len] = '\0';
  fclose(f);

  cJSON* root = cJSON_Parse(buf);
  if (!root) return ESP_ERR_INVALID_ARG;

  cJSON* item;
  if ((item = cJSON_GetObjectItem(root, "wifi_ssid")))
    strncpy(cfg->wifi_ssid, item->valuestring, sizeof(cfg->wifi_ssid) - 1);
  if ((item = cJSON_GetObjectItem(root, "wifi_password")))
    strncpy(cfg->wifi_password, item->valuestring, sizeof(cfg->wifi_password) - 1);
  if ((item = cJSON_GetObjectItem(root, "sample_rate_ms")))
    cfg->sample_rate_ms = item->valueint;
  if ((item = cJSON_GetObjectItem(root, "temp_offset")))
    cfg->temp_offset = (float)item->valuedouble;
  if ((item = cJSON_GetObjectItem(root, "logging_enabled")))
    cfg->logging_enabled = cJSON_IsTrue(item);

  cJSON_Delete(root);
  return ESP_OK;
}

void spiffs_config_example(void)
{
  /* Initialize SPIFFS */
  esp_vfs_spiffs_conf_t spiffs_conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = true
  };
  esp_vfs_spiffs_register(&spiffs_conf);

  size_t total, used;
  esp_spiffs_info(NULL, &total, &used);
  ESP_LOGI(s_tag, "SPIFFS: %d/%d bytes used", used, total);

  device_config_t cfg;

  /* Try to load existing config */
  if (load_config(&cfg) != ESP_OK) {
    ESP_LOGI(s_tag, "No config found, creating defaults");
    strcpy(cfg.wifi_ssid, "MyNetwork");
    strcpy(cfg.wifi_password, "MyPassword");
    cfg.sample_rate_ms = 1000;
    cfg.temp_offset = 0.0f;
    cfg.logging_enabled = true;
    save_config(&cfg);
  }

  ESP_LOGI(s_tag, "Config loaded:");
  ESP_LOGI(s_tag, "  WiFi SSID: %s", cfg.wifi_ssid);
  ESP_LOGI(s_tag, "  Sample Rate: %d ms", cfg.sample_rate_ms);
  ESP_LOGI(s_tag, "  Temp Offset: %.1f", cfg.temp_offset);
  ESP_LOGI(s_tag, "  Logging: %s", cfg.logging_enabled ? "ON" : "OFF");

  /* Modify and save */
  cfg.sample_rate_ms = 500;
  save_config(&cfg);
  ESP_LOGI(s_tag, "Config updated");

  esp_vfs_spiffs_unregister(NULL);
}

void app_main(void) { spiffs_config_example(); }
