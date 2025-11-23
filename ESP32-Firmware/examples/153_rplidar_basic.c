/**
 * @file 153_rplidar_basic.c
 * @brief RPLiDAR C1 360-degree laser scanner example
 *
 * Demonstrates:
 * - Motor control and scan initialization
 * - Point cloud data processing
 * - Obstacle detection
 */

#include "star_sensor_rplidar_c1.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "RPLIDAR_EXAMPLE";

#define RPLIDAR_TX_PIN      (GPIO_NUM_17)
#define RPLIDAR_RX_PIN      (GPIO_NUM_16)
#define RPLIDAR_MOTOR_PIN   (GPIO_NUM_4)
#define RPLIDAR_UART_PORT   (UART_NUM_2)

void rplidar_example(void)
{
  esp_err_t ret;

  rplidar_handle_t lidar;
  rplidar_config_t cfg = {
    .uart_port = RPLIDAR_UART_PORT,
    .tx_pin = RPLIDAR_TX_PIN,
    .rx_pin = RPLIDAR_RX_PIN,
    .motor_pwm_pin = RPLIDAR_MOTOR_PIN
  };

  ret = star_sensor_rplidar_init(&lidar, &cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init RPLiDAR");
    return;
  }

  /* Get device info */
  rplidar_device_info_t info;
  star_sensor_rplidar_get_info(&lidar, &info);
  ESP_LOGI(s_tag, "RPLiDAR Model: %d, FW: %d.%d", info.model, info.firmware_major, info.firmware_minor);

  /* Check health */
  rplidar_health_info_t health;
  star_sensor_rplidar_get_health(&lidar, &health);
  ESP_LOGI(s_tag, "Health: %d (0=Good, 1=Warning, 2=Error)", health.status);

  /* Start scanning */
  ret = star_sensor_rplidar_start_scan(&lidar, false);  /* Standard scan mode */
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to start scan");
    star_sensor_rplidar_deinit(&lidar);
    return;
  }

  ESP_LOGI(s_tag, "Scanning...");

  /* Process scan data */
  for (int i = 0; i < 100; i++) {
    star_sensor_rplidar_process(&lidar);

    rplidar_scan_point_t points[360];
    uint16_t count;
    star_sensor_rplidar_get_scan_data(&lidar, points, 360, &count);

    if (count > 0) {
      /* Find closest obstacle */
      float min_dist = 999.0f;
      float min_angle = 0.0f;

      for (uint16_t j = 0; j < count; j++) {
        if (points[j].distance_m > 0.01f && points[j].distance_m < min_dist) {
          min_dist = points[j].distance_m;
          min_angle = points[j].angle_deg;
        }
      }

      if (min_dist < 999.0f) {
        ESP_LOGI(s_tag, "Closest obstacle: %.2f m at %.1f deg (points: %d)",
                 min_dist, min_angle, count);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_sensor_rplidar_stop_scan(&lidar);
  star_sensor_rplidar_deinit(&lidar);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { rplidar_example(); }
