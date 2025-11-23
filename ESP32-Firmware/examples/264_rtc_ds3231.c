/**
 * @file 264_rtc_ds3231.c
 * @brief DS3231 precision RTC with temperature compensation
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "RTC";

#define DS3231_ADDR (0x68)

static uint8_t priv_bcd_to_dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }

void rtc_ds3231_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "RTC_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "ds3231", I2C_NUM_0, DS3231_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  ESP_LOGI(s_tag, "DS3231 RTC initialized");

  for (int i = 0; i < 60; i++) {
    uint8_t reg = 0x00;
    uint8_t data[7];
    size_t bytes_read;
    star_bus_i2c_read(&manager, "ds3231", data, 7, reg, &bytes_read);

    int sec = priv_bcd_to_dec(data[0]);
    int min = priv_bcd_to_dec(data[1]);
    int hr = priv_bcd_to_dec(data[2] & 0x3F);
    int day = priv_bcd_to_dec(data[4]);
    int mon = priv_bcd_to_dec(data[5] & 0x1F);
    int yr = priv_bcd_to_dec(data[6]) + 2000;

    /* Read temperature */
    reg = 0x11;
    uint8_t temp[2];
    star_bus_i2c_read(&manager, "ds3231", temp, 2, reg, &bytes_read);
    float temperature = temp[0] + (temp[1] >> 6) * 0.25f;

    ESP_LOGI(s_tag, "%04d-%02d-%02d %02d:%02d:%02d  Temp: %.2fC",
             yr, mon, day, hr, min, sec, temperature);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { rtc_ds3231_example(); }
