/**
 * @file 256_led_matrix.c
 * @brief MAX7219 8x8 LED matrix display
 */

#include <esp_log.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "LED_MATRIX";

#define MAX7219_CS   (GPIO_NUM_5)
#define MAX7219_CLK  (GPIO_NUM_18)
#define MAX7219_DIN  (GPIO_NUM_23)

static spi_device_handle_t s_spi;

static void priv_max7219_write(uint8_t reg, uint8_t data) {
  uint8_t buf[2] = {reg, data};
  spi_transaction_t t = { .length = 16, .tx_buffer = buf };
  spi_device_transmit(s_spi, &t);
}

static void priv_max7219_init(void) {
  priv_max7219_write(0x09, 0x00);  /* No decode */
  priv_max7219_write(0x0A, 0x08);  /* Intensity */
  priv_max7219_write(0x0B, 0x07);  /* Scan all 8 */
  priv_max7219_write(0x0C, 0x01);  /* Normal mode */
  priv_max7219_write(0x0F, 0x00);  /* Display test off */
}

static void priv_display_pattern(uint8_t pattern[8]) {
  for (int i = 0; i < 8; i++) priv_max7219_write(i + 1, pattern[i]);
}

void led_matrix_example(void)
{
  spi_bus_config_t buscfg = { .mosi_io_num = MAX7219_DIN, .sclk_io_num = MAX7219_CLK };
  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10000000, .mode = 0, .spics_io_num = MAX7219_CS, .queue_size = 1
  };
  spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);
  spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);

  priv_max7219_init();
  ESP_LOGI(s_tag, "LED matrix initialized");

  uint8_t smiley[8] = {0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C};
  uint8_t heart[8] = {0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};

  for (int i = 0; i < 10; i++) {
    priv_display_pattern(smiley);
    vTaskDelay(pdMS_TO_TICKS(500));
    priv_display_pattern(heart);
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  spi_bus_remove_device(s_spi);
}

void app_main(void) { led_matrix_example(); }
