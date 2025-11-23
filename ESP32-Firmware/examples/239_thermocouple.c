/**
 * @file 239_thermocouple.c
 * @brief MAX6675 thermocouple reader
 */

#include <esp_log.h>
#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

static const char *s_tag = "THERMO";

#define TC_CS_PIN   (GPIO_NUM_5)
#define TC_CLK_PIN  (GPIO_NUM_18)
#define TC_MISO_PIN (GPIO_NUM_19)

static spi_device_handle_t s_spi;

static float priv_max6675_read_temp(void)
{
  uint8_t rx[2];
  spi_transaction_t t = { .length = 16, .rx_buffer = rx };
  spi_device_transmit(s_spi, &t);

  uint16_t data = (rx[0] << 8) | rx[1];

  if (data & 0x04) return -1;  /* Thermocouple not connected */

  return ((data >> 3) & 0x0FFF) * 0.25f;
}

void thermocouple_example(void)
{
  spi_bus_config_t buscfg = {
    .miso_io_num = TC_MISO_PIN,
    .mosi_io_num = -1,
    .sclk_io_num = TC_CLK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
  };

  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 1 * 1000 * 1000,
    .mode = 0,
    .spics_io_num = TC_CS_PIN,
    .queue_size = 1,
  };

  spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);
  spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);

  ESP_LOGI(s_tag, "MAX6675 thermocouple reader started");

  float min_temp = 1000, max_temp = -100;

  for (int i = 0; i < 200; i++) {
    float temp = priv_max6675_read_temp();

    if (temp >= 0) {
      if (temp < min_temp) min_temp = temp;
      if (temp > max_temp) max_temp = temp;

      ESP_LOGI(s_tag, "Temperature: %.2f C (min: %.2f, max: %.2f)", temp, min_temp, max_temp);

      if (temp > 100) ESP_LOGI(s_tag, "HIGH TEMPERATURE!");
    } else {
      ESP_LOGI(s_tag, "Thermocouple not connected!");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  spi_bus_remove_device(s_spi);
}

void app_main(void) { thermocouple_example(); }
