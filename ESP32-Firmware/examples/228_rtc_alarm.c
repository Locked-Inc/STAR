/**
 * @file 228_rtc_alarm.c
 * @brief DS3231 RTC with alarms
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_i2c.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "RTC_ALARM";

#define DS3231_ADDR (0x68)

typedef struct {
  uint8_t second, minute, hour;
  uint8_t day, date, month, year;
} rtc_time_t;

static uint8_t priv_bcd_to_dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t priv_dec_to_bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

static star_bus_manager_t *g_manager;

static void priv_ds3231_get_time(rtc_time_t *time)
{
  uint8_t data[7];
  size_t bytes_read;
  star_bus_i2c_read(g_manager, "ds3231", data, 7, 0x00, &bytes_read);

  time->second = priv_bcd_to_dec(data[0] & 0x7F);
  time->minute = priv_bcd_to_dec(data[1]);
  time->hour = priv_bcd_to_dec(data[2] & 0x3F);
  time->day = priv_bcd_to_dec(data[3]);
  time->date = priv_bcd_to_dec(data[4]);
  time->month = priv_bcd_to_dec(data[5] & 0x1F);
  time->year = priv_bcd_to_dec(data[6]);
}

static void priv_ds3231_set_time(rtc_time_t *time)
{
  uint8_t data[7] = {
    priv_dec_to_bcd(time->second),
    priv_dec_to_bcd(time->minute),
    priv_dec_to_bcd(time->hour),
    priv_dec_to_bcd(time->day),
    priv_dec_to_bcd(time->date),
    priv_dec_to_bcd(time->month),
    priv_dec_to_bcd(time->year)
  };
  size_t bytes_written;
  star_bus_i2c_write(g_manager, "ds3231", data, 7, 0x00, &bytes_written);
}

static void priv_ds3231_set_alarm1(uint8_t hour, uint8_t minute)
{
  uint8_t data[4] = {
    0x00,  /* Seconds (match) */
    priv_dec_to_bcd(minute),
    priv_dec_to_bcd(hour),
    0x80   /* Alarm when hours/minutes match */
  };
  size_t bytes_written;
  star_bus_i2c_write(g_manager, "ds3231", data, 4, 0x07, &bytes_written);

  /* Enable alarm 1 interrupt */
  uint8_t ctrl = 0x05;  /* A1IE + INTCN */
  star_bus_i2c_write(g_manager, "ds3231", &ctrl, 1, 0x0E, &bytes_written);
}

static bool priv_ds3231_check_alarm(void)
{
  uint8_t status;
  size_t bytes_read;
  star_bus_i2c_read(g_manager, "ds3231", &status, 1, 0x0F, &bytes_read);

  if (status & 0x01) {  /* A1F flag */
    uint8_t clear = status & ~0x01;
    size_t bytes_written;
    star_bus_i2c_write(g_manager, "ds3231", &clear, 1, 0x0F, &bytes_written);
    return true;
  }
  return false;
}

void rtc_alarm_example(void)
{
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "RTC_Manager", NULL, NULL);
  g_manager = &manager;

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "ds3231", I2C_NUM_0, DS3231_ADDR, GPIO_NUM_21, GPIO_NUM_22, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Set current time */
  rtc_time_t now = {.second = 0, .minute = 30, .hour = 14, .day = 1, .date = 15, .month = 6, .year = 24};
  priv_ds3231_set_time(&now);

  /* Set alarm for 14:31 */
  priv_ds3231_set_alarm1(14, 31);
  ESP_LOGI(s_tag, "Alarm set for 14:31");

  for (int i = 0; i < 120; i++) {
    rtc_time_t time;
    priv_ds3231_get_time(&time);

    ESP_LOGI(s_tag, "Time: %02d:%02d:%02d  Date: 20%02d-%02d-%02d",
             time.hour, time.minute, time.second,
             time.year, time.month, time.date);

    if (priv_ds3231_check_alarm()) {
      ESP_LOGI(s_tag, "*** ALARM TRIGGERED! ***");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  star_bus_manager_deinit(&manager);
}

void app_main(void) { rtc_alarm_example(); }
