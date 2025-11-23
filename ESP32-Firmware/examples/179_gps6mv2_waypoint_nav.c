/**
 * @file 179_gps6mv2_waypoint_nav.c
 * @brief GPS waypoint navigation
 */

#include "star_sensor_gps6mv2.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "WAYPOINT_NAV";

typedef struct {
  float lat;
  float lon;
  const char *name;
} waypoint_t;

static const waypoint_t waypoints[] = {
  {37.7749, -122.4194, "Point A"},
  {37.7849, -122.4094, "Point B"},
  {37.7649, -122.4294, "Point C"},
};
#define NUM_WAYPOINTS (sizeof(waypoints) / sizeof(waypoints[0]))

static float calc_distance(float lat1, float lon1, float lat2, float lon2)
{
  float R = 6371000.0f;
  float dLat = (lat2 - lat1) * M_PI / 180.0f;
  float dLon = (lon2 - lon1) * M_PI / 180.0f;
  float a = sinf(dLat/2) * sinf(dLat/2) +
            cosf(lat1 * M_PI / 180.0f) * cosf(lat2 * M_PI / 180.0f) *
            sinf(dLon/2) * sinf(dLon/2);
  return R * 2 * atan2f(sqrtf(a), sqrtf(1-a));
}

static float calc_bearing(float lat1, float lon1, float lat2, float lon2)
{
  float dLon = (lon2 - lon1) * M_PI / 180.0f;
  lat1 *= M_PI / 180.0f;
  lat2 *= M_PI / 180.0f;
  float y = sinf(dLon) * cosf(lat2);
  float x = cosf(lat1) * sinf(lat2) - sinf(lat1) * cosf(lat2) * cosf(dLon);
  float bearing = atan2f(y, x) * 180.0f / M_PI;
  return fmodf(bearing + 360.0f, 360.0f);
}

void waypoint_nav_example(void)
{
  gps6mv2_handle_t gps;
  gps6mv2_config_t cfg = {
    .uart_port = UART_NUM_1,
    .tx_pin = GPIO_NUM_17,
    .rx_pin = GPIO_NUM_16,
    .enable_gga = true,
    .enable_rmc = true,
    .enable_gsa = false,
    .enable_gsv = false
  };
  star_sensor_gps6mv2_init(&gps, &cfg);

  int current_wp = 0;

  for (int i = 0; i < 200; i++) {
    star_sensor_gps6mv2_process(&gps);

    gps6mv2_data_t data;
    if (star_sensor_gps6mv2_get_data(&gps, &data) == ESP_OK && data.valid) {
      const waypoint_t* wp = &waypoints[current_wp];
      float dist = calc_distance(data.latitude, data.longitude, wp->lat, wp->lon);
      float bearing = calc_bearing(data.latitude, data.longitude, wp->lat, wp->lon);

      ESP_LOGI(s_tag, "To %s: %.0f m, bearing %.0f°", wp->name, dist, bearing);

      if (dist < 10.0f) {
        ESP_LOGW(s_tag, "Reached %s!", wp->name);
        current_wp = (current_wp + 1) % NUM_WAYPOINTS;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  star_sensor_gps6mv2_deinit(&gps);
}

void app_main(void) { waypoint_nav_example(); }
