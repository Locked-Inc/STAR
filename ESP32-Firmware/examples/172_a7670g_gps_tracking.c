/**
 * @file 172_a7670g_gps_tracking.c
 * @brief A7670G integrated GPS tracking
 *
 * Demonstrates:
 * - Built-in GPS module usage
 * - Location reporting via cellular
 * - Geofencing basics
 */

#include "star_module_a7670g.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "A7670G_GPS";

#define A7670G_TX_PIN       (GPIO_NUM_17)
#define A7670G_RX_PIN       (GPIO_NUM_16)
#define A7670G_POWER_PIN    (GPIO_NUM_4)
#define A7670G_UART_PORT    (UART_NUM_2)

/* Geofence center point */
#define FENCE_LAT (37.7749) 
#define FENCE_LON   -122.4194
#define FENCE_RADIUS_M (1000.0) 
static float haversine_distance(float lat1, float lon1, float lat2, float lon2)
{
  float R = 6371000.0f;  /* Earth radius in meters */
  float dLat = (lat2 - lat1) * M_PI / 180.0f;
  float dLon = (lon2 - lon1) * M_PI / 180.0f;
  float a = sinf(dLat/2) * sinf(dLat/2) +
            cosf(lat1 * M_PI / 180.0f) * cosf(lat2 * M_PI / 180.0f) *
            sinf(dLon/2) * sinf(dLon/2);
  float c = 2 * atan2f(sqrtf(a), sqrtf(1-a));
  return R * c;
}

void a7670g_gps_example(void)
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

  /* Enable GPS */
  star_module_a7670g_gps_start(&module);
  ESP_LOGI(s_tag, "GPS enabled, waiting for fix...");

  /* Track location */
  for (int i = 0; i < 100; i++) {
    a7670g_gps_data_t gps;
    if (star_module_a7670g_gps_get_position(&module, &gps) == ESP_OK && gps.fix_valid) {
      ESP_LOGI(s_tag, "Position: %.6f, %.6f", gps.latitude, gps.longitude);
      ESP_LOGI(s_tag, "Altitude: %.1f m, Speed: %.1f km/h", gps.altitude, gps.speed);
      ESP_LOGI(s_tag, "Satellites: %d", gps.satellites);

      /* Check geofence */
      float dist = haversine_distance(gps.latitude, gps.longitude, FENCE_LAT, FENCE_LON);
      if (dist > FENCE_RADIUS_M) {
        ESP_LOGW(s_tag, "GEOFENCE BREACH! Distance: %.0f m", dist);
      } else {
        ESP_LOGI(s_tag, "Inside geofence, distance: %.0f m", dist);
      }
    } else {
      ESP_LOGI(s_tag, "Waiting for GPS fix...");
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }

  star_module_a7670g_gps_stop(&module);
  star_module_a7670g_power_off(&module);
  star_module_a7670g_deinit(&module);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { a7670g_gps_example(); }
