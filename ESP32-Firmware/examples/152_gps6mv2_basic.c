/**
 * @file 152_gps6mv2_basic.c
 * @brief GPS6MV2 (NEO-6M) GPS module example
 *
 * Demonstrates:
 * - UART communication with GPS module
 * - NMEA sentence parsing
 * - Position, speed, and satellite tracking
 */

#include "star_sensor_gps6mv2.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "GPS_EXAMPLE";

#define GPS_TX_PIN      (GPIO_NUM_17)
#define GPS_RX_PIN      (GPIO_NUM_16)
#define GPS_UART_PORT   (UART_NUM_2)

void gps_example(void)
{
  esp_err_t ret;

  gps6mv2_handle_t gps;
  gps6mv2_config_t cfg = {
    .uart_port = GPS_UART_PORT,
    .tx_pin = GPS_TX_PIN,
    .rx_pin = GPS_RX_PIN,
    .enable_gga = true,
    .enable_rmc = true,
    .enable_gsa = false,
    .enable_gsv = false
  };

  ret = star_sensor_gps6mv2_init(&gps, &cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init GPS: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(s_tag, "GPS initialized, waiting for fix...");

  /* Main processing loop */
  for (int i = 0; i < 300; i++) {  /* 5 minutes */
    /* Process incoming NMEA data */
    star_sensor_gps6mv2_process(&gps);

    gps6mv2_data_t data;
    ret = star_sensor_gps6mv2_get_data(&gps, &data);

    if (ret == ESP_OK && data.valid) {
      ESP_LOGI(s_tag, "Position: %.6f, %.6f", data.latitude, data.longitude);
      ESP_LOGI(s_tag, "Altitude: %.1f m, Speed: %.1f km/h", data.altitude, data.speed_kmh);
      ESP_LOGI(s_tag, "Course: %.1f deg, Satellites: %d", data.course, data.satellites_used);
      ESP_LOGI(s_tag, "HDOP: %.1f, Fix Quality: %d", data.hdop, data.fix_quality);
    } else {
      ESP_LOGD(s_tag, "Waiting for GPS fix... (sats: %d)", data.satellites_used);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  star_sensor_gps6mv2_deinit(&gps);
  ESP_LOGI(s_tag, "Example complete");
}

void app_main(void) { gps_example(); }
