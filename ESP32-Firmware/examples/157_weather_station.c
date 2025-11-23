/**
 * @file 157_weather_station.c
 * @brief Complete weather station example
 *
 * Demonstrates:
 * - DHT22 for temperature/humidity
 * - BH1750 for light level
 * - Multi-sensor coordination
 * - Data logging format
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_dht22.h"
#include "star_sensor_bh1750.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>

static const char *s_tag = "WEATHER_STATION";

#define I2C_SDA_PIN     (GPIO_NUM_21)
#define I2C_SCL_PIN     (GPIO_NUM_22)
#define DHT22_PIN       (GPIO_NUM_4)

typedef struct {
  float temperature;
  float humidity;
  float light_lux;
  float heat_index;
  float dew_point;
  bool valid;
} weather_data_t;

static float calculate_heat_index(float temp, float humidity)
{
  if (temp < 27.0f) return temp;
  float hi = -8.78469475556f + 1.61139411f * temp + 2.33854883889f * humidity
             - 0.14611605f * temp * humidity - 0.012308094f * temp * temp
             - 0.0164248277778f * humidity * humidity
             + 0.002211732f * temp * temp * humidity
             + 0.00072546f * temp * humidity * humidity
             - 0.000003582f * temp * temp * humidity * humidity;
  return hi;
}

static float calculate_dew_point(float temp, float humidity)
{
  float a = 17.27f;
  float b = 237.7f;
  float alpha = ((a * temp) / (b + temp)) + logf(humidity / 100.0f);
  return (b * alpha) / (a - alpha);
}

void weather_station_example(void)
{
  esp_err_t ret;

  /* Initialize bus manager for I2C sensors */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Weather_Manager", NULL, NULL);

  star_bus_config_t* i2c = star_bus_config_create_i2c(
    "bh1750", I2C_NUM_0, 0x23, I2C_SDA_PIN, I2C_SCL_PIN, 100000);
  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  /* Initialize DHT22 */
  dht22_handle_t dht;
  dht22_config_t dht_cfg = {
    .data_pin = DHT22_PIN,
    .enable_pullup = false
  };
  ret = star_sensor_dht22_init(&dht, &dht_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "DHT22 init failed");
    return;
  }

  /* Initialize BH1750 */
  bh1750_handle_t light;
  bh1750_config_t light_cfg = {
    .i2c_addr = 0x23,
    .mode = BH1750_MODE_CONTINUOUS,
    .resolution = BH1750_RES_HIGH,
    .mtreg = BH1750_DEFAULT_MTREG
  };
  ret = star_sensor_bh1750_init(&light, &manager, "bh1750", &light_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "BH1750 init failed");
    return;
  }

  ESP_LOGI(s_tag, "Weather station initialized");
  ESP_LOGI(s_tag, "CSV: timestamp,temp_c,humidity_pct,light_lux,heat_index_c,dew_point_c");

  uint32_t sample_count = 0;

  /* Main data collection loop */
  for (int i = 0; i < 60; i++) {  /* 60 samples, every 3 seconds = 3 minutes */
    weather_data_t weather = {0};

    /* Read DHT22 if ready */
    bool can_read = false;
    star_sensor_dht22_can_read(&dht, &can_read);
    if (can_read) {
      dht22_data_t dht_data;
      if (star_sensor_dht22_read(&dht, &dht_data) == ESP_OK) {
        weather.temperature = dht_data.temperature_c;
        weather.humidity = dht_data.humidity_rh;
        weather.heat_index = calculate_heat_index(weather.temperature, weather.humidity);
        weather.dew_point = calculate_dew_point(weather.temperature, weather.humidity);
        weather.valid = true;
      }
    }

    /* Read light level */
    star_sensor_bh1750_read_light(&light, &weather.light_lux);

    if (weather.valid) {
      sample_count++;
      ESP_LOGI(s_tag, "CSV: %lu,%.1f,%.1f,%.1f,%.1f,%.1f",
               sample_count,
               weather.temperature,
               weather.humidity,
               weather.light_lux,
               weather.heat_index,
               weather.dew_point);
    }

    vTaskDelay(pdMS_TO_TICKS(3000));
  }

  star_sensor_dht22_deinit(&dht);
  star_sensor_bh1750_deinit(&light);
  star_bus_manager_deinit(&manager);
  ESP_LOGI(s_tag, "Weather station stopped. Samples collected: %lu", sample_count);
}

void app_main(void) { weather_station_example(); }
