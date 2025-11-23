/**
 * @file 182_spi_sensor_chain.c
 * @brief Multiple SPI sensors with different CS pins
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_spi.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "SPI_CHAIN";

void spi_chain_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "SPI_Manager", NULL, NULL);

  /* Multiple SPI devices, same bus, different CS */
  spi_device_interface_config_t dev_cfg1 = {
    .clock_speed_hz = 1000000, .mode = 0, .spics_io_num = GPIO_NUM_5, .queue_size = 1,
  };
  spi_device_interface_config_t dev_cfg2 = {
    .clock_speed_hz = 1000000, .mode = 0, .spics_io_num = GPIO_NUM_15, .queue_size = 1,
  };
  spi_device_interface_config_t dev_cfg3 = {
    .clock_speed_hz = 1000000, .mode = 0, .spics_io_num = GPIO_NUM_4, .queue_size = 1,
  };
  star_bus_config_t* spi1 = star_bus_config_create_spi_device(
    "sensor1", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18, SPI_DMA_CH_AUTO, &dev_cfg1);
  star_bus_config_t* spi2 = star_bus_config_create_spi_device(
    "sensor2", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18, SPI_DMA_CH_AUTO, &dev_cfg2);
  star_bus_config_t* spi3 = star_bus_config_create_spi_device(
    "sensor3", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18, SPI_DMA_CH_AUTO, &dev_cfg3);

  star_bus_manager_add_bus(&manager, spi1);
  star_bus_manager_add_bus(&manager, spi2);
  star_bus_manager_add_bus(&manager, spi3);

  star_bus_config_init(spi1, &manager);
  star_bus_config_init(spi2, &manager);
  star_bus_config_init(spi3, &manager);

  ESP_LOGI(s_tag, "SPI chain initialized with 3 devices");

  /* Read from each device */
  uint8_t tx_buf[4] = {0x00, 0x00, 0x00, 0x00};
  uint8_t rx_buf[4];

  for (int i = 0; i < 50; i++) {
    star_bus_spi_transmit(&manager, "sensor1", tx_buf, 4, 0);
    star_bus_spi_receive(&manager, "sensor1", rx_buf, 4, 0);
    ESP_LOGI(s_tag, "Sensor1: %02X %02X %02X %02X", rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);

    star_bus_spi_transmit(&manager, "sensor2", tx_buf, 4, 0);
    star_bus_spi_receive(&manager, "sensor2", rx_buf, 4, 0);
    ESP_LOGI(s_tag, "Sensor2: %02X %02X %02X %02X", rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);

    star_bus_spi_transmit(&manager, "sensor3", tx_buf, 4, 0);
    star_bus_spi_receive(&manager, "sensor3", rx_buf, 4, 0);
    ESP_LOGI(s_tag, "Sensor3: %02X %02X %02X %02X", rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);

    vTaskDelay(pdMS_TO_TICKS(200));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { spi_chain_example(); }
