/**
 * @file 185_a7670g_wifi_hotspot.c
 * @brief A7670G WiFi hotspot mode - ESP32 connects to cellular modem AP
 *
 * TODO: WiFi hotspot functionality is NOT SUPPORTED by A7670G hardware
 *
 * ISSUE: The A7670G is a 4G LTE CAT-1 modem that communicates via UART AT commands.
 *        It does NOT have WiFi capabilities or hotspot functionality.
 *        This example needs to be rearchitected to use actual A7670G features:
 *        - LTE data connection via AT commands
 *        - TCP/IP stack through AT commands
 *        - PPP mode for internet sharing (requires different approach)
 *
 * Current state: Non-functional example with commented-out code
 * Required refactor: Implement PPP or AT command TCP/IP bridge instead
 */

#include "star_module_a7670g.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <lwip/sockets.h>

static const char *s_tag = "A7670G_HOTSPOT";

#define A7670G_TX_PIN       (GPIO_NUM_17)
#define A7670G_RX_PIN       (GPIO_NUM_16)
#define A7670G_POWER_PIN    (GPIO_NUM_4)
#define A7670G_UART_PORT    (UART_NUM_2)

/* Hotspot credentials created by A7670G */
#define HOTSPOT_SSID        "A7670G_AP"
#define HOTSPOT_PASSWORD    "12345678"

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  (BIT0)

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGI(s_tag, "Disconnected, reconnecting...");
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    ESP_LOGI(s_tag, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void a7670g_hotspot_example(void)
{
  /* Initialize A7670G and enable hotspot mode */
  a7670g_handle_t module;
  a7670g_config_t cfg = {
    .uart_port = A7670G_UART_PORT,
    .tx_pin = A7670G_TX_PIN,
    .rx_pin = A7670G_RX_PIN,
    .power_pin = A7670G_POWER_PIN,
    .reset_pin = GPIO_NUM_NC,
    .apn = "internet"
  };

  if (star_module_a7670g_init(&module, &cfg) != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init A7670G");
    return;
  }

  star_module_a7670g_power_on(&module);

  bool ready = false;
  for (int i = 0; i < 30 && !ready; i++) {
    star_module_a7670g_is_ready(&module, &ready);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  /* Connect to cellular network first */
  if (star_module_a7670g_connect_network(&module) != ESP_OK) {
    ESP_LOGE(s_tag, "Cellular connection failed");
    return;
  }

  ESP_LOGI(s_tag, "Cellular connected");

  /* TODO: WiFi hotspot NOT SUPPORTED by A7670G LTE modem
   * The A7670G does not have WiFi hardware - it is a 4G LTE modem only.
   * These functions do not exist in the A7670G library:
   * - a7670g_hotspot_config_t (struct doesn't exist)
   * - star_module_a7670g_enable_hotspot() (function doesn't exist)
   * - star_module_a7670g_disable_hotspot() (function doesn't exist)
   *
   * To share cellular internet with ESP32, use one of these approaches:
   * 1. PPP mode: Configure A7670G as PPP modem, ESP32 as PPP client
   * 2. AT command TCP/IP: Use A7670G's built-in TCP/IP stack via AT commands
   * 3. USB tethering: If hardware supports it (requires USB host on ESP32)
   */

  /*
  // DOES NOT WORK - commented out non-existent code
  a7670g_hotspot_config_t hotspot_cfg = {
    .ssid = HOTSPOT_SSID,
    .password = HOTSPOT_PASSWORD,
    .channel = 6,
    .max_clients = 4
  };
  star_module_a7670g_enable_hotspot(&module, &hotspot_cfg);
  ESP_LOGI(s_tag, "Hotspot enabled: SSID=%s", HOTSPOT_SSID);
  */

  ESP_LOGW(s_tag, "WiFi hotspot not supported - A7670G is LTE modem, not WiFi module");

  /* TODO: WiFi connection code commented out - A7670G doesn't provide WiFi hotspot
   * This entire section is invalid because the A7670G cannot create a WiFi AP.
   * Instead, implement cellular internet sharing using:
   * 1. PPP over UART (A7670G supports PPP mode)
   * 2. AT command TCP/IP bridge
   * 3. Separate WiFi module on ESP32 for actual hotspot creation
   */

  /*
  // DOES NOT WORK - A7670G cannot create WiFi hotspot
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
    .sta = {
      .ssid = HOTSPOT_SSID,
      .password = HOTSPOT_PASSWORD,
    },
  };

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &sta_config);
  esp_wifi_start();

  ESP_LOGI(s_tag, "Connecting to A7670G hotspot...");

  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                          pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(s_tag, "Connected to A7670G hotspot!");
    ESP_LOGI(s_tag, "ESP32 now has internet via A7670G cellular bridge");
    vTaskDelay(pdMS_TO_TICKS(60000));
  } else {
    ESP_LOGE(s_tag, "Failed to connect to hotspot");
  }

  esp_wifi_stop();
  */

  /* Demonstrate actual A7670G capabilities instead */
  ESP_LOGI(s_tag, "A7670G successfully connected to cellular network");
  ESP_LOGI(s_tag, "Available A7670G features:");
  ESP_LOGI(s_tag, "  - HTTP/HTTPS client (star_module_a7670g_http_get)");
  ESP_LOGI(s_tag, "  - TCP/IP via AT commands");
  ESP_LOGI(s_tag, "  - GPS positioning (star_module_a7670g_gps_*)");
  ESP_LOGI(s_tag, "  - SMS send/receive (star_module_a7670g_send_sms)");
  ESP_LOGI(s_tag, "  - Network info (star_module_a7670g_get_network_info)");

  a7670g_network_info_t net_info;
  if (star_module_a7670g_get_network_info(&module, &net_info) == ESP_OK) {
    ESP_LOGI(s_tag, "Network: %s, Signal: %d/31, Type: %d",
             net_info.operator_name, net_info.signal_quality, net_info.network_type);
  }

  vTaskDelay(pdMS_TO_TICKS(5000));

  // star_module_a7670g_disable_hotspot(&module);  // Function doesn't exist
  star_module_a7670g_power_off(&module);
  star_module_a7670g_deinit(&module);
  ESP_LOGI(s_tag, "Example complete - NOTE: WiFi hotspot functionality needs implementation");
}

void app_main(void) { a7670g_hotspot_example(); }
