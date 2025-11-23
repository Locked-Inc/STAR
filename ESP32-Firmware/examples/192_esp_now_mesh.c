/**
 * @file 192_esp_now_mesh.c
 * @brief ESP-NOW peer-to-peer sensor network
 */

#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "ESP_NOW";

/* Broadcast MAC for all peers */
static uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct {
  uint8_t node_id;
  float temperature;
  float humidity;
  uint32_t timestamp;
} sensor_packet_t;

static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
  if (len == sizeof(sensor_packet_t)) {
    sensor_packet_t* pkt = (sensor_packet_t*)data;
    ESP_LOGI(s_tag, "Node %d: Temp=%.1f, Hum=%.1f",
             pkt->node_id, pkt->temperature, pkt->humidity);
  }
}

static void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
  ESP_LOGD(s_tag, "Send status: %s", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void esp_now_mesh_example(void)
{
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();

  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();

  /* Initialize ESP-NOW */
  esp_now_init();
  esp_now_register_recv_cb(on_data_recv);
  esp_now_register_send_cb(on_data_sent);

  esp_now_peer_info_t peer = {
    .channel = 0,
    .ifidx = WIFI_IF_STA,
    .encrypt = false,
  };
  memcpy(peer.peer_addr, broadcast_mac, 6);
  esp_now_add_peer(&peer);

  ESP_LOGI(s_tag, "ESP-NOW initialized");

  /* Initialize sensor */
  dht22_handle_t dht;
  dht22_config_t cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };
  star_sensor_dht22_init(&dht, &cfg);

  uint8_t node_id = 1;  /* Configure per device */

  for (int i = 0; i < 200; i++) {
    dht22_data_t data;
    if (star_sensor_dht22_read(&dht, &data) == ESP_OK) {
      sensor_packet_t pkt = {
        .node_id = node_id,
        .temperature = data.temperature_c,
        .humidity = data.humidity_rh,
        .timestamp = xTaskGetTickCount()
      };

      esp_now_send(broadcast_mac, (uint8_t*)&pkt, sizeof(pkt));
      ESP_LOGI(s_tag, "Broadcast: Temp=%.1f, Hum=%.1f", data.temperature_c, data.humidity_rh);
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }

  esp_now_deinit();
  star_sensor_dht22_deinit(&dht);
}

void app_main(void) { esp_now_mesh_example(); }
