/* esp32-firmware/components/pynq_wifi_bridge/pynq_wifi_manager.c */

#include "pynq_wifi_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "star_error_handler.h"

static const char* TAG = "wifi_manager";

/* WiFi event bits */
#define WIFI_CONNECTED_BIT (BIT0)
#define WIFI_FAIL_BIT (BIT1)
#define WIFI_GOT_IP_BIT (BIT2)
#define WIFI_SCAN_DONE_BIT (BIT3)
#define MAX_RETRY_ATTEMPTS (5)
#define MAX_SCAN_RESULTS (20)

/* Module state */
static bool               g_initialized = false;
static EventGroupHandle_t g_wifi_event_group;
static wifi_status_t      g_wifi_status     = k_wifi_disconnected;
static uint8_t            g_ip_addr[4]      = {0, 0, 0, 0};
static int8_t             g_rssi            = 0;
static uint8_t            g_retry_count     = 0;
static uint16_t           g_scan_count      = 0;
static error_handler_t    g_connect_handler = {0};

/**
 * @brief WiFi connection reset function for error handler
 * @param context Context (unused)
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t wifi_connection_reset(void* context)
{
  (void)context; /* Unused */

  ESP_LOGI(TAG, "Attempting WiFi connection reset");

  /* Try to reinitialize connection */
  esp_err_t ret = esp_wifi_disconnect();
  if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED) {
    ESP_LOGE(TAG, "Failed to disconnect during reset: %d", ret);
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(1000)); /* Wait a bit before reconnecting */

  ret = esp_wifi_connect();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to reconnect during reset: %d", ret);
    return ret;
  }

  ESP_LOGI(TAG, "WiFi connection reset successful");
  return ESP_OK;
}

/**
 * @brief WiFi event handler
 * @param arg Event handler argument
 * @param event_base Event base
 * @param event_id Event ID
 * @param event_data Event data
 */
static void
wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "WiFi station started");
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (g_wifi_status == k_wifi_connecting && error_handler_can_retry(&g_connect_handler)) {
      /* Use error handler for connection retry with exponential backoff */
      (void)event_data; /* Unused in this context */

      /* Record the error */
      RECORD_ERROR(&g_connect_handler, ESP_ERR_WIFI_NOT_CONNECT, "WiFi connection failed");

      /* Apply backoff delay */
      vTaskDelay(pdMS_TO_TICKS(g_connect_handler.current_retry_delay));

      /* Try to reconnect */
      esp_err_t ret = esp_wifi_connect();
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to retry connection: %d", ret);
        g_wifi_status = k_wifi_failed;
        xEventGroupSetBits(g_wifi_event_group, WIFI_FAIL_BIT);
      }

      g_retry_count++;
    } else {
      /* Max retries exhausted or not in connecting state */
      xEventGroupSetBits(g_wifi_event_group, WIFI_FAIL_BIT);
      g_wifi_status = k_wifi_disconnected;
      ESP_LOGI(TAG, "WiFi disconnected");
    }
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    ESP_LOGI(TAG, "WiFi connected to AP");
    xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    g_ip_addr[0]             = esp_ip4_addr1(&event->ip_info.ip);
    g_ip_addr[1]             = esp_ip4_addr2(&event->ip_info.ip);
    g_ip_addr[2]             = esp_ip4_addr3(&event->ip_info.ip);
    g_ip_addr[3]             = esp_ip4_addr4(&event->ip_info.ip);

    ESP_LOGI(TAG, "Got IP: %d.%d.%d.%d", g_ip_addr[0], g_ip_addr[1], g_ip_addr[2], g_ip_addr[3]);
    g_wifi_status = k_wifi_connected;
    g_retry_count = 0;

    /* Reset error handler state on successful connection */
    error_handler_reset_state(&g_connect_handler);

    xEventGroupSetBits(g_wifi_event_group, WIFI_GOT_IP_BIT);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
    ESP_LOGI(TAG, "WiFi scan completed");
    xEventGroupSetBits(g_wifi_event_group, WIFI_SCAN_DONE_BIT);
  }
}

bool wifi_manager_init(void)
{
  if (g_initialized) {
    ESP_LOGW(TAG, "Already initialized");
    return true;
  }

  ESP_LOGI(TAG, "Initializing WiFi manager");

  /* Initialize error handler for connection retries */
  /* Base delay: 1000ms, Max delay: 10000ms, Max retries: 5 */
  esp_err_t ret = error_handler_init(&g_connect_handler,
                                     MAX_RETRY_ATTEMPTS,
                                     1000,
                                     10000,
                                     wifi_connection_reset,
                                     NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize error handler: %d", ret);
    return false;
  }

  /* Create event group */
  g_wifi_event_group = xEventGroupCreate();
  if (!g_wifi_event_group) {
    ESP_LOGE(TAG, "Failed to create event group");
    error_handler_deinit(&g_connect_handler);
    return false;
  }

  /* Initialize network interface */
  ret = esp_netif_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize network interface: %d", ret);
    error_handler_deinit(&g_connect_handler);
    vEventGroupDelete(g_wifi_event_group);
    return false;
  }

  /* Create default event loop */
  ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to create event loop: %d", ret);
    error_handler_deinit(&g_connect_handler);
    vEventGroupDelete(g_wifi_event_group);
    return false;
  }

  /* Create default WiFi station */
  esp_netif_create_default_wifi_sta();

  /* Initialize WiFi with default config */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret                    = esp_wifi_init(&cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize WiFi: %d", ret);
    error_handler_deinit(&g_connect_handler);
    vEventGroupDelete(g_wifi_event_group);
    return false;
  }

  /* Register event handlers */
  ret = esp_event_handler_instance_register(WIFI_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            &wifi_event_handler,
                                            NULL,
                                            NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register WiFi event handler: %d", ret);
    esp_wifi_deinit();
    error_handler_deinit(&g_connect_handler);
    vEventGroupDelete(g_wifi_event_group);
    return false;
  }

  ret = esp_event_handler_instance_register(IP_EVENT,
                                            IP_EVENT_STA_GOT_IP,
                                            &wifi_event_handler,
                                            NULL,
                                            NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register IP event handler: %d", ret);
    esp_wifi_deinit();
    error_handler_deinit(&g_connect_handler);
    vEventGroupDelete(g_wifi_event_group);
    return false;
  }

  /* Set WiFi mode to station */
  ret = esp_wifi_set_mode(WIFI_MODE_STA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi mode: %d", ret);
    esp_wifi_deinit();
    error_handler_deinit(&g_connect_handler);
    vEventGroupDelete(g_wifi_event_group);
    return false;
  }

  /* Start WiFi */
  ret = esp_wifi_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WiFi: %d", ret);
    esp_wifi_deinit();
    error_handler_deinit(&g_connect_handler);
    vEventGroupDelete(g_wifi_event_group);
    return false;
  }

  g_initialized = true;
  ESP_LOGI(TAG, "WiFi manager initialized");

  return true;
}

bool wifi_manager_connect(const char* ssid, const char* password)
{
  if (!g_initialized) {
    ESP_LOGE(TAG, "WiFi manager not initialized");
    return false;
  }

  if (!ssid) {
    ESP_LOGE(TAG, "SSID is NULL");
    return false;
  }

  ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);

  /* Configure WiFi connection */
  wifi_config_t wifi_config = {0};
  strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
  if (password) {
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
  }

  /* Set WiFi configuration */
  esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi config: %d", ret);
    return false;
  }

  /* Clear event bits */
  xEventGroupClearBits(g_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_GOT_IP_BIT);

  /* Reset retry count and error handler state */
  g_retry_count = 0;
  g_wifi_status = k_wifi_connecting;
  error_handler_reset_state(&g_connect_handler);

  /* Start connection */
  ret = esp_wifi_connect();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WiFi connection: %d", ret);
    g_wifi_status = k_wifi_failed;
    return false;
  }

  return true;
}

bool wifi_manager_disconnect(void)
{
  if (!g_initialized) {
    ESP_LOGE(TAG, "WiFi manager not initialized");
    return false;
  }

  ESP_LOGI(TAG, "Disconnecting from WiFi");

  esp_err_t ret = esp_wifi_disconnect();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to disconnect: %d", ret);
    return false;
  }

  g_wifi_status = k_wifi_disconnected;
  memset(g_ip_addr, 0, sizeof(g_ip_addr));
  g_rssi = 0;

  return true;
}

wifi_status_t wifi_manager_get_status(void)
{
  return g_wifi_status;
}

bool wifi_manager_get_ip(uint8_t ip_addr[4])
{
  if (!ip_addr) {
    return false;
  }

  if (g_wifi_status != k_wifi_connected) {
    return false;
  }

  memcpy(ip_addr, g_ip_addr, 4);
  return true;
}

int8_t wifi_manager_get_rssi(void)
{
  if (g_wifi_status != k_wifi_connected) {
    return 0;
  }

  wifi_ap_record_t ap_info;
  esp_err_t        ret = esp_wifi_sta_get_ap_info(&ap_info);
  if (ret != ESP_OK) {
    return 0;
  }

  g_rssi = ap_info.rssi;
  return g_rssi;
}

bool wifi_manager_start_scan(void)
{
  if (!g_initialized) {
    ESP_LOGE(TAG, "WiFi manager not initialized");
    return false;
  }

  ESP_LOGI(TAG, "Starting WiFi scan");

  /* Clear event bit */
  xEventGroupClearBits(g_wifi_event_group, WIFI_SCAN_DONE_BIT);

  /* Configure scan */
  wifi_scan_config_t scan_config = {
    .ssid        = NULL,
    .bssid       = NULL,
    .channel     = 0,
    .show_hidden = false,
    .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
  };

  /* Start scan */
  esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start scan: %d", ret);
    return false;
  }

  return true;
}

bool wifi_manager_get_scan_results(wifi_scan_result_t* results,
                                   uint16_t            max_results,
                                   uint16_t*           num_results)
{
  if (!g_initialized || !results || !num_results) {
    return false;
  }

  /* Wait for scan to complete (with timeout) */
  EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
                                         WIFI_SCAN_DONE_BIT,
                                         pdTRUE,
                                         pdFALSE,
                                         pdMS_TO_TICKS(10000));

  if (!(bits & WIFI_SCAN_DONE_BIT)) {
    ESP_LOGE(TAG, "Scan timeout");
    return false;
  }

  /* Get scan results */
  uint16_t          ap_count = MAX_SCAN_RESULTS;
  wifi_ap_record_t* ap_info  = malloc(ap_count * sizeof(wifi_ap_record_t));
  if (!ap_info) {
    ESP_LOGE(TAG, "Failed to allocate memory for scan results");
    return false;
  }

  esp_err_t ret = esp_wifi_scan_get_ap_records(&ap_count, ap_info);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get scan results: %d", ret);
    free(ap_info);
    return false;
  }

  ESP_LOGI(TAG, "Found %d access points", ap_count);

  /* Copy results */
  uint16_t count = (ap_count < max_results) ? ap_count : max_results;
  for (uint16_t i = 0; i < count; i++) {
    strncpy(results[i].ssid, (char*)ap_info[i].ssid, sizeof(results[i].ssid) - 1);
    results[i].ssid[sizeof(results[i].ssid) - 1] = '\0';
    results[i].rssi                              = ap_info[i].rssi;
    results[i].channel                           = ap_info[i].primary;
    results[i].auth_mode                         = ap_info[i].authmode;
  }

  *num_results = count;
  g_scan_count = count;

  free(ap_info);
  return true;
}

void wifi_manager_deinit(void)
{
  if (!g_initialized) {
    return;
  }

  ESP_LOGI(TAG, "Deinitializing WiFi manager");

  esp_wifi_stop();
  esp_wifi_deinit();

  if (g_wifi_event_group) {
    vEventGroupDelete(g_wifi_event_group);
    g_wifi_event_group = NULL;
  }

  /* Deinitialize error handler */
  error_handler_deinit(&g_connect_handler);

  g_initialized = false;
  g_wifi_status = k_wifi_disconnected;
  memset(g_ip_addr, 0, sizeof(g_ip_addr));
  g_rssi = 0;

  ESP_LOGI(TAG, "WiFi manager deinitialized");
}
