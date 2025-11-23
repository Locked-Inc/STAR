/**
 * @file 171_a7670g_http_client.c
 * @brief A7670G HTTP client operations
 *
 * Demonstrates:
 * - HTTP GET requests
 * - Response parsing
 *
 * NOTE: HTTP POST and network disconnect functions are not yet implemented
 */

#include "star_module_a7670g.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "A7670G_HTTP";

#define A7670G_TX_PIN       (GPIO_NUM_17)
#define A7670G_RX_PIN       (GPIO_NUM_16)
#define A7670G_POWER_PIN    (GPIO_NUM_4)
#define A7670G_UART_PORT    (UART_NUM_2)

void a7670g_http_example(void)
{
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

  if (!ready) {
    ESP_LOGE(s_tag, "Module not ready");
    return;
  }

  /* Connect to network */
  if (star_module_a7670g_connect_network(&module) != ESP_OK) {
    ESP_LOGE(s_tag, "Network connection failed");
    return;
  }

  ESP_LOGI(s_tag, "Network connected, performing HTTP requests...");

  /* HTTP GET request */
  char response[1024];
  esp_err_t ret = star_module_a7670g_http_get(&module, "http://httpbin.org/get", response, sizeof(response));
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "GET Response: %s", response);
  }

  /* HTTP POST with JSON */
  /* NOTE: star_module_a7670g_http_post() is not yet implemented */
  /*
  const char *json_data = "{\"sensor\":\"esp32\",\"value\":25.5}";
  ret = star_module_a7670g_http_post(&module, "http://httpbin.org/post", json_data, response, sizeof(response));
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "POST Response: %s", response);
  }
  */

  /* NOTE: star_module_a7670g_disconnect_network() is not yet implemented */
  /* star_module_a7670g_disconnect_network(&module); */
  star_module_a7670g_power_off(&module);
  star_module_a7670g_deinit(&module);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { a7670g_http_example(); }
