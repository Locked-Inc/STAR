/**
 * @file 296_tft_ili9341.c
 * @brief ILI9341 320x240 TFT display
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_spi.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "TFT";

#define TFT_CS   (GPIO_NUM_5)
#define TFT_DC   (GPIO_NUM_2)
#define TFT_RST  (GPIO_NUM_4)

static star_bus_manager_t *g_manager;

static void priv_tft_cmd(uint8_t cmd) {
  gpio_set_level(TFT_DC, 0);
  star_bus_spi_transmit(g_manager, "ili9341", &cmd, 1, 0);
}

static void priv_tft_data(uint8_t data) {
  gpio_set_level(TFT_DC, 1);
  star_bus_spi_transmit(g_manager, "ili9341", &data, 1, 0);
}

static void priv_tft_init(void) {
  gpio_set_direction(TFT_DC, GPIO_MODE_OUTPUT);
  gpio_set_direction(TFT_RST, GPIO_MODE_OUTPUT);

  gpio_set_level(TFT_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(10));
  gpio_set_level(TFT_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(120));

  priv_tft_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(5));  /* Reset */
  priv_tft_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120)); /* Sleep out */
  priv_tft_cmd(0x3A); priv_tft_data(0x55);  /* 16-bit color */
  priv_tft_cmd(0x36); priv_tft_data(0x48);  /* Memory access */
  priv_tft_cmd(0x29);  /* Display on */
}

static void priv_tft_fill(uint16_t color) {
  priv_tft_cmd(0x2A);
  priv_tft_data(0); priv_tft_data(0); priv_tft_data(0); priv_tft_data(239);
  priv_tft_cmd(0x2B);
  priv_tft_data(0); priv_tft_data(0); priv_tft_data(1); priv_tft_data(63);
  priv_tft_cmd(0x2C);

  gpio_set_level(TFT_DC, 1);
  uint8_t hi = color >> 8, lo = color & 0xFF;
  for (int i = 0; i < 240 * 320 / 8; i++) {
    uint8_t buf[16] = {hi, lo, hi, lo, hi, lo, hi, lo,
                        hi, lo, hi, lo, hi, lo, hi, lo};
    star_bus_spi_transmit(g_manager, "ili9341", buf, 16, 0);
  }
}

void tft_ili9341_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TFT_Manager", NULL, NULL);
  g_manager = &manager;

  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 40000000,
    .mode = 0,
    .spics_io_num = TFT_CS,
    .queue_size = 1,
  };
  star_bus_config_t *tft_bus = star_bus_config_create_spi_device(
    "ili9341", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18, SPI_DMA_CH_AUTO, &dev_cfg);
  star_bus_manager_add_bus(&manager, tft_bus);
  star_bus_config_init(tft_bus, &manager);

  priv_tft_init();
  ESP_LOGI(s_tag, "ILI9341 TFT initialized");

  uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFE0, 0xF81F, 0x07FF, 0xFFFF, 0x0000};
  const char *names[] = {"Red", "Green", "Blue", "Yellow", "Magenta", "Cyan", "White", "Black"};

  for (int i = 0; i < 8; i++) {
    ESP_LOGI(s_tag, "Filling: %s", names[i]);
    priv_tft_fill(colors[i]);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { tft_ili9341_example(); }
