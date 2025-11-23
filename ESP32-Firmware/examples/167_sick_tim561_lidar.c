/**
 * @file 167_sick_tim561_lidar.c
 * @brief SICK TiM561 industrial 2D LiDAR example
 *
 * Demonstrates:
 * - TCP/IP connection to LiDAR
 * - COLA protocol commands
 * - High-precision scan data processing
 */

#include "star_sensor_sick_tim561.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "SICK_TIM561";

#define LIDAR_IP        "192.168.1.100"
#define LIDAR_PORT (2111)

void sick_tim561_example(void)
{
  tim561_handle_t lidar;
  tim561_config_t cfg = {
    .host = LIDAR_IP,
    .port = LIDAR_PORT
  };

  if (star_sensor_tim561_init(&lidar, &cfg) != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SICK TiM561");
    return;
  }

  /* Connect to LiDAR */
  if (star_sensor_tim561_connect(&lidar) != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to connect to %s:%d", LIDAR_IP, LIDAR_PORT);
    return;
  }

  ESP_LOGI(s_tag, "Connected to SICK TiM561");

  /* Get device info */
  tim561_device_info_t info;
  star_sensor_tim561_get_device_info(&lidar, &info);
  ESP_LOGI(s_tag, "Device: %s, Serial: %u", info.device_name, info.serial_number);

  /* Start continuous scanning */
  star_sensor_tim561_start_scan(&lidar);
  ESP_LOGI(s_tag, "Scanning started (270 deg, 2880 points, 15 Hz)");

  /* Process scan data */
  tim561_scan_data_t scan;
  for (int i = 0; i < 100; i++) {
    if (star_sensor_tim561_read_scan(&lidar, &scan) == ESP_OK) {
      /* Find closest point */
      float min_dist = 10000.0f;
      float min_angle = 0.0f;

      for (int p = 0; p < scan.point_count; p++) {
        float dist_mm = (float)scan.points[p].distance_mm;
        if (dist_mm > 10.0f && dist_mm < min_dist) {
          min_dist = dist_mm;
          min_angle = scan.start_angle_deg + p * scan.angular_resolution_deg;
        }
      }

      ESP_LOGI(s_tag, "Scan %d: %d points, closest: %.2f mm at %.2f deg",
               i, scan.point_count, min_dist, min_angle);
    }

    vTaskDelay(pdMS_TO_TICKS(67));  /* ~15 Hz */
  }

  star_sensor_tim561_stop_scan(&lidar);
  star_sensor_tim561_disconnect(&lidar);
  star_sensor_tim561_deinit(&lidar);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { sick_tim561_example(); }
