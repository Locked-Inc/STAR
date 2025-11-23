/**
 * @file 183_uart_multiplexing.c
 * @brief Multiple UART devices management
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_uart.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "UART_MUX";

void uart_mux_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "UART_Manager", NULL, NULL);

  /* GPS on UART1 */
  star_uart_config_t gps_config = STAR_UART_CONFIG_DEFAULT();
  gps_config.port = UART_NUM_1;
  gps_config.tx_pin = GPIO_NUM_17;
  gps_config.rx_pin = GPIO_NUM_16;
  gps_config.baud_rate = STAR_UART_BAUD_9600;

  /* LiDAR on UART2 */
  star_uart_config_t lidar_config = STAR_UART_CONFIG_DEFAULT();
  lidar_config.port = UART_NUM_2;
  lidar_config.tx_pin = GPIO_NUM_5;
  lidar_config.rx_pin = GPIO_NUM_4;
  lidar_config.baud_rate = STAR_UART_BAUD_115200;

  star_bus_uart_init(&manager, "gps", &gps_config);
  star_bus_uart_init(&manager, "lidar", &lidar_config);

  ESP_LOGI(s_tag, "Dual UART initialized");

  uint8_t gps_buf[256];
  uint8_t lidar_buf[256];
  size_t bytes_read;

  for (int i = 0; i < 100; i++) {
    esp_err_t ret = star_bus_uart_read(&manager, "gps", gps_buf, sizeof(gps_buf) - 1, &bytes_read, 100);
    if (ret == ESP_OK && bytes_read > 0) {
      gps_buf[bytes_read] = '\0';
      ESP_LOGI(s_tag, "GPS: %s", gps_buf);
    }

    ret = star_bus_uart_read(&manager, "lidar", lidar_buf, sizeof(lidar_buf), &bytes_read, 100);
    if (ret == ESP_OK && bytes_read > 0) {
      ESP_LOGI(s_tag, "LiDAR: %d bytes received", (int)bytes_read);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  star_bus_uart_deinit(&manager, "gps");
  star_bus_uart_deinit(&manager, "lidar");
  star_bus_manager_deinit(&manager);
}

void app_main(void) { uart_mux_example(); }
