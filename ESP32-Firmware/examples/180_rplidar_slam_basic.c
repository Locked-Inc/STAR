/**
 * @file 180_rplidar_slam_basic.c
 * @brief RPLiDAR basic SLAM data collection
 */

#include "star_sensor_rplidar_c1.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <math.h>

static const char *s_tag = "RPLIDAR_SLAM";

#define MAP_SIZE (100)
#define MAP_RESOLUTION (0.1f) /* 10cm per cell */

static uint8_t s_occupancy_map[MAP_SIZE][MAP_SIZE];

void rplidar_slam_example(void)
{
  memset(s_occupancy_map, 128, sizeof(s_occupancy_map));  /* Unknown */

  rplidar_handle_t lidar;
  rplidar_config_t cfg = {
    .uart_port = UART_NUM_1,
    .tx_pin = GPIO_NUM_17,
    .rx_pin = GPIO_NUM_16,
    .motor_pwm_pin = GPIO_NUM_25,
    .motor_pwm_duty = 650
  };
  star_sensor_rplidar_init(&lidar, &cfg);
  star_sensor_rplidar_start_motor(&lidar);
  star_sensor_rplidar_start_scan(&lidar, false);

  int robot_x = MAP_SIZE / 2;
  int robot_y = MAP_SIZE / 2;

  for (int scan_num = 0; scan_num < 100; scan_num++) {
    star_sensor_rplidar_process(&lidar);

    rplidar_scan_point_t points[360];
    uint16_t count;
    if (star_sensor_rplidar_get_scan_data(&lidar, points, 360, &count) == ESP_OK) {
      for (int p = 0; p < count; p++) {
        float angle_rad = points[p].angle_deg * M_PI / 180.0f;
        float dist_m = points[p].distance_m;

        if (dist_m < 0.1f || dist_m > 5.0f) continue;

        int cell_x = robot_x + (int)(cosf(angle_rad) * dist_m / MAP_RESOLUTION);
        int cell_y = robot_y + (int)(sinf(angle_rad) * dist_m / MAP_RESOLUTION);

        if (cell_x >= 0 && cell_x < MAP_SIZE && cell_y >= 0 && cell_y < MAP_SIZE) {
          s_occupancy_map[cell_y][cell_x] = 255;  /* Occupied */
        }
      }

      ESP_LOGI(s_tag, "Scan %d: %d points mapped", scan_num, count);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_sensor_rplidar_stop_scan(&lidar);
  star_sensor_rplidar_stop_motor(&lidar);
  star_sensor_rplidar_deinit(&lidar);
}

void app_main(void) { rplidar_slam_example(); }
