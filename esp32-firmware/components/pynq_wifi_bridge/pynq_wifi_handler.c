/* esp32-firmware/components/pynq_wifi_bridge/pynq_wifi_handler.c */

#include "pynq_wifi_handler.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "pynq_wifi_manager.h"

static const char* TAG = "command_handler";

/* Firmware version */
#define FIRMWARE_VERSION_MAJOR (0)
#define FIRMWARE_VERSION_MINOR (1)
#define FIRMWARE_VERSION_PATCH (0)

static bool g_initialized = false;

/**
 * @brief Handle CMD_PING command
 * @param packet Received packet
 */
static void handle_cmd_ping(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received PING command");

  /* Echo back any payload data */
  uint16_t payload_len = packet->payload_len;
  protocol_send_response(k_status_ok, packet->payload, payload_len);
}

/**
 * @brief Handle CMD_RESET command
 * @param packet Received packet
 */
static void handle_cmd_reset(protocol_packet_t* packet)
{
  ESP_LOGW(TAG, "Received RESET command - restarting in 1 second");

  protocol_send_response(k_status_ok, NULL, 0);

  /* Delay to allow response to be sent */
  vTaskDelay(pdMS_TO_TICKS(1000));

  esp_restart();
}

/**
 * @brief Handle CMD_GET_VERSION command
 * @param packet Received packet
 */
static void handle_cmd_get_version(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received GET_VERSION command");

  /* Create version response: major.minor.patch */
  uint8_t version[3] = {
    FIRMWARE_VERSION_MAJOR,
    FIRMWARE_VERSION_MINOR,
    FIRMWARE_VERSION_PATCH,
  };

  protocol_send_response(k_status_ok, version, sizeof(version));
}

/**
 * @brief Handle CMD_WIFI_CONNECT command
 * @param packet Received packet
 */
static void handle_cmd_wifi_connect(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received WIFI_CONNECT command");

  if (packet->payload_len < sizeof(wifi_connect_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  wifi_connect_payload_t* payload = (wifi_connect_payload_t*)packet->payload;

  ESP_LOGI(TAG, "WiFi connect request: SSID='%s'", payload->ssid);

  /* Attempt to connect to WiFi */
  if (wifi_manager_connect(payload->ssid, payload->password)) {
    protocol_send_response(k_status_ok, NULL, 0);
  } else {
    protocol_send_error(k_status_error);
  }
}

/**
 * @brief Handle CMD_WIFI_DISCONNECT command
 * @param packet Received packet
 */
static void handle_cmd_wifi_disconnect(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received WIFI_DISCONNECT command");

  /* Disconnect from WiFi */
  if (wifi_manager_disconnect()) {
    protocol_send_response(k_status_ok, NULL, 0);
  } else {
    protocol_send_error(k_status_error);
  }
}

/**
 * @brief Handle CMD_WIFI_STATUS command
 * @param packet Received packet
 */
static void handle_cmd_wifi_status(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received WIFI_STATUS command");

  /* Get current WiFi status */
  wifi_status_payload_t status = {
    .status  = wifi_manager_get_status(),
    .ip_addr = {0, 0, 0, 0},
    .rssi    = 0,
  };

  /* Get IP address if connected */
  wifi_manager_get_ip(status.ip_addr);

  /* Get RSSI if connected */
  status.rssi = wifi_manager_get_rssi();

  protocol_send_response(k_status_ok, (uint8_t*)&status, sizeof(status));
}

/**
 * @brief Handle CMD_WIFI_SCAN command
 * @param packet Received packet
 */
static void handle_cmd_wifi_scan(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received WIFI_SCAN command");

  /* Start WiFi scan */
  if (!wifi_manager_start_scan()) {
    ESP_LOGE(TAG, "Failed to start WiFi scan");
    protocol_send_error(k_status_error);
    return;
  }

  /* Get scan results */
  wifi_scan_result_t scan_results[20];
  uint16_t           num_results = 0;

  if (!wifi_manager_get_scan_results(scan_results, 20, &num_results)) {
    ESP_LOGE(TAG, "Failed to get scan results");
    protocol_send_error(k_status_error);
    return;
  }

  ESP_LOGI(TAG, "Scan found %d networks", num_results);

  /* Send response with scan results */
  protocol_send_response(k_status_ok,
                         (uint8_t*)scan_results,
                         num_results * sizeof(wifi_scan_result_t));
}

/**
 * @brief Handle CMD_HTTP_GET command
 * @param packet Received packet
 */
static void handle_cmd_http_get(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received HTTP_GET command");

  if (packet->payload_len < sizeof(http_get_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  http_get_payload_t* payload = (http_get_payload_t*)packet->payload;

  ESP_LOGI(TAG, "HTTP GET request: URL='%s'", payload->url);

  /* TODO: Implement HTTP GET logic */
  ESP_LOGW(TAG, "HTTP functionality not yet implemented");
  protocol_send_error(k_status_error);
}

/**
 * @brief Handle CMD_HTTP_POST command
 * @param packet Received packet
 */
static void handle_cmd_http_post(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received HTTP_POST command");

  if (packet->payload_len < sizeof(http_post_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  /* TODO: Implement HTTP POST logic */
  ESP_LOGW(TAG, "HTTP functionality not yet implemented");
  protocol_send_error(k_status_error);
}

/**
 * @brief Handle unknown command
 * @param packet Received packet
 */
static void handle_cmd_unknown(protocol_packet_t* packet)
{
  ESP_LOGW(TAG, "Unknown command: 0x%02X", packet->cmd);
  protocol_send_error(k_status_invalid_cmd);
}

bool command_handler_init(void)
{
  if (g_initialized) {
    ESP_LOGW(TAG, "Already initialized");
    return true;
  }

  ESP_LOGI(TAG, "Initializing command handler");

  /* Initialize WiFi manager */
  if (!wifi_manager_init()) {
    ESP_LOGE(TAG, "Failed to initialize WiFi manager");
    return false;
  }

  g_initialized = true;

  ESP_LOGI(TAG, "Command handler initialized");

  return true;
}

void command_handler_process(protocol_packet_t* packet)
{
  if (!g_initialized) {
    ESP_LOGE(TAG, "Command handler not initialized");
    return;
  }

  if (!packet) {
    ESP_LOGE(TAG, "Null packet");
    return;
  }

  ESP_LOGD(TAG, "Processing command: 0x%02X", packet->cmd);

  /* Dispatch command to handler */
  switch (packet->cmd) {
    case k_cmd_ping:
      handle_cmd_ping(packet);
      break;

    case k_cmd_reset:
      handle_cmd_reset(packet);
      break;

    case k_cmd_get_version:
      handle_cmd_get_version(packet);
      break;

    case k_cmd_wifi_connect:
      handle_cmd_wifi_connect(packet);
      break;

    case k_cmd_wifi_disconnect:
      handle_cmd_wifi_disconnect(packet);
      break;

    case k_cmd_wifi_status:
      handle_cmd_wifi_status(packet);
      break;

    case k_cmd_wifi_scan:
      handle_cmd_wifi_scan(packet);
      break;

    case k_cmd_http_get:
      handle_cmd_http_get(packet);
      break;

    case k_cmd_http_post:
      handle_cmd_http_post(packet);
      break;

    default:
      handle_cmd_unknown(packet);
      break;
  }
}

void command_handler_deinit(void)
{
  if (!g_initialized) {
    return;
  }

  ESP_LOGI(TAG, "Deinitializing command handler");

  /* Deinitialize WiFi manager */
  wifi_manager_deinit();

  g_initialized = false;

  ESP_LOGI(TAG, "Command handler deinitialized");
}
