/**
 * @file 190_mqtt_sensor_publish.c
 * @brief MQTT sensor data publishing
 */

#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <mqtt_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "MQTT_PUBLISH";

#define WIFI_SSID       "YourSSID"
#define WIFI_PASSWORD   "YourPassword"
#define MQTT_BROKER     "mqtt://broker.hivemq.com"
#define MQTT_TOPIC      "esp32/sensors"

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
  if (event_id == MQTT_EVENT_CONNECTED) {
    ESP_LOGI(s_tag, "MQTT connected");
    s_mqtt_connected = true;
  } else if (event_id == MQTT_EVENT_DISCONNECTED) {
    ESP_LOGI(s_tag, "MQTT disconnected");
    s_mqtt_connected = false;
  }
}

void mqtt_publish_example(void)
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

  /* Initialize MQTT */
  esp_mqtt_client_config_t mqtt_cfg = { .broker.address.uri = MQTT_BROKER };
  s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  esp_mqtt_client_start(s_mqtt_client);

  /* Initialize sensor */
  dht22_handle_t dht;
  dht22_config_t cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };
  star_sensor_dht22_init(&dht, &cfg);

  for (int i = 0; i < 100; i++) {
    if (s_mqtt_connected) {
      dht22_data_t data;
      if (star_sensor_dht22_read(&dht, &data) == ESP_OK) {
        char payload[128];
        snprintf(payload, sizeof(payload),
                 "{\"temp\":%.1f,\"humidity\":%.1f}",
                 data.temperature_c, data.humidity_rh);

        esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC, payload, 0, 1, 0);
        ESP_LOGI(s_tag, "Published: %s", payload);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }

  esp_mqtt_client_stop(s_mqtt_client);
  star_sensor_dht22_deinit(&dht);
}

void app_main(void) { mqtt_publish_example(); }
