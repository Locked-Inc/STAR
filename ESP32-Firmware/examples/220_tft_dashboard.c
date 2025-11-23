/**
 * @file 220_tft_dashboard.c
 * @brief TFT LCD sensor dashboard
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_mpu6050.h"
#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "TFT_DASH";

#define TFT_MOSI  (GPIO_NUM_23)
#define TFT_CLK   (GPIO_NUM_18)
#define TFT_CS    (GPIO_NUM_5)
#define TFT_DC    (GPIO_NUM_16)
#define TFT_RST   (GPIO_NUM_17)

static spi_device_handle_t spi;

static void priv_tft_cmd(uint8_t cmd)
{
  gpio_set_level(TFT_DC, 0);
  spi_transaction_t t = { .length = 8, .tx_buffer = &cmd };
  spi_device_transmit(spi, &t);
}

static void priv_tft_data(uint8_t data)
{
  gpio_set_level(TFT_DC, 1);
  spi_transaction_t t = { .length = 8, .tx_buffer = &data };
  spi_device_transmit(spi, &t);
}

static void priv_tft_init(void)
{
  gpio_set_direction(TFT_DC, GPIO_MODE_OUTPUT);
  gpio_set_direction(TFT_RST, GPIO_MODE_OUTPUT);

  gpio_set_level(TFT_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level(TFT_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(100));

  priv_tft_cmd(0x01);  /* Software reset */
  vTaskDelay(pdMS_TO_TICKS(150));
  priv_tft_cmd(0x11);  /* Sleep out */
  vTaskDelay(pdMS_TO_TICKS(500));
  priv_tft_cmd(0x3A); priv_tft_data(0x55);  /* 16-bit color */
  priv_tft_cmd(0x29);  /* Display on */
}

static void priv_tft_fill_rect(int x, int y, int w, int h, uint16_t color)
{
  priv_tft_cmd(0x2A);
  priv_tft_data(x >> 8); priv_tft_data(x & 0xFF);
  priv_tft_data((x + w - 1) >> 8); priv_tft_data((x + w - 1) & 0xFF);
  priv_tft_cmd(0x2B);
  priv_tft_data(y >> 8); priv_tft_data(y & 0xFF);
  priv_tft_data((y + h - 1) >> 8); priv_tft_data((y + h - 1) & 0xFF);
  priv_tft_cmd(0x2C);

  gpio_set_level(TFT_DC, 1);
  for (int i = 0; i < w * h; i++) {
    uint8_t buf[2] = {color >> 8, color & 0xFF};
    spi_transaction_t t = { .length = 16, .tx_buffer = buf };
    spi_device_transmit(spi, &t);
  }
}

static uint16_t priv_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void tft_dashboard_example(void)
{
  spi_bus_config_t buscfg = {
    .mosi_io_num = TFT_MOSI,
    .sclk_io_num = TFT_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4096
  };

  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 20 * 1000 * 1000,
    .mode = 0,
    .spics_io_num = TFT_CS,
    .queue_size = 7
  };

  spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  spi_bus_add_device(SPI2_HOST, &devcfg, &spi);

  priv_tft_init();
  priv_tft_fill_rect(0, 0, 240, 320, priv_rgb565(0, 0, 0));

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "TFT_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "mpu6050", I2C_NUM_0, 0x68, GPIO_NUM_21, GPIO_NUM_22, 400000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  mpu6050_handle_t imu;
  dht22_handle_t dht;
  mpu6050_config_t imu_cfg = {
    .i2c_addr = MPU6050_I2C_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_RANGE_2G,
    .gyro_range = MPU6050_GYRO_RANGE_250,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 0,
    .enable_fifo = false
  };
  dht22_config_t dht_cfg = { .data_pin = GPIO_NUM_4, .enable_pullup = false };

  star_sensor_mpu6050_init(&imu, &manager, "mpu6050", &imu_cfg);
  star_sensor_dht22_init(&dht, &dht_cfg);

  ESP_LOGI(s_tag, "TFT Dashboard running");

  for (int i = 0; i < 200; i++) {
    mpu6050_accel_t accel;
    dht22_data_t dht_data;
    star_sensor_mpu6050_read_accel(&imu, &accel);
    star_sensor_dht22_read(&dht, &dht_data);

    /* Draw temperature bar */
    int temp_height = (int)(dht_data.temperature_c * 3);
    priv_tft_fill_rect(20, 50, 40, 200, priv_rgb565(50, 50, 50));
    priv_tft_fill_rect(20, 250 - temp_height, 40, temp_height, priv_rgb565(255, 0, 0));

    /* Draw humidity bar */
    int hum_height = (int)(dht_data.humidity_rh * 2);
    priv_tft_fill_rect(80, 50, 40, 200, priv_rgb565(50, 50, 50));
    priv_tft_fill_rect(80, 250 - hum_height, 40, hum_height, priv_rgb565(0, 0, 255));

    ESP_LOGI(s_tag, "Temp: %.1f, Hum: %.1f", dht_data.temperature_c, dht_data.humidity_rh);
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  star_sensor_mpu6050_deinit(&imu);
  star_sensor_dht22_deinit(&dht);
  star_bus_manager_deinit(&manager);
  spi_bus_remove_device(spi);
}

void app_main(void) { tft_dashboard_example(); }
