/**
 * @file 245_color_sensor.c
 * @brief TCS34725 RGB color sensor
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "COLOR";

#define TCS34725_ADDR (0x29)
#define TCS34725_ENABLE (0x00)
#define TCS34725_ATIME (0x01)
#define TCS34725_CDATA (0x14)

static star_bus_manager_t *g_manager;

static void priv_tcs34725_write(uint8_t reg, uint8_t val)
{
  size_t bytes_written;
  star_bus_i2c_write(g_manager, "tcs34725", &val, 1, 0x80 | reg, &bytes_written);
}

static void priv_tcs34725_read_colors(uint16_t *c, uint16_t *r, uint16_t *g, uint16_t *b)
{
  uint8_t data[8];
  size_t bytes_read;
  star_bus_i2c_read(g_manager, "tcs34725", data, 8, 0x80 | 0x20 | TCS34725_CDATA, &bytes_read);
  *c = data[0] | (data[1] << 8);
  *r = data[2] | (data[3] << 8);
  *g = data[4] | (data[5] << 8);
  *b = data[6] | (data[7] << 8);
}

static const char *priv_identify_color(uint16_t r, uint16_t g, uint16_t b)
{
  uint16_t max = r > g ? (r > b ? r : b) : (g > b ? g : b);
  if (max < 50) return "BLACK";
  if (r > 200 && g > 200 && b > 200) return "WHITE";
  if (r > g && r > b) return "RED";
  if (g > r && g > b) return "GREEN";
  if (b > r && b > g) return "BLUE";
  if (r > 150 && g > 100 && b < 100) return "YELLOW";
  return "UNKNOWN";
}

void color_sensor_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Color_Manager", NULL, NULL);
  g_manager = &manager;

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "tcs34725", I2C_NUM_0, TCS34725_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Initialize sensor */
  priv_tcs34725_write(TCS34725_ATIME, 0xF6);  /* 24ms integration */
  priv_tcs34725_write(TCS34725_ENABLE, 0x03); /* Power on + enable */

  ESP_LOGI(s_tag, "TCS34725 color sensor started");

  for (int i = 0; i < 100; i++) {
    uint16_t c, r, g, b;
    priv_tcs34725_read_colors(&c, &r, &g, &b);

    const char *color = priv_identify_color(r, g, b);
    ESP_LOGI(s_tag, "C:%d R:%d G:%d B:%d -> %s", c, r, g, b, color);

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { color_sensor_example(); }
