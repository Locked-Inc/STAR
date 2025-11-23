/**
 * @file 205_smart_greenhouse.c
 * @brief Automated greenhouse with climate control
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_sensor_dht22.h"
#include "star_sensor_bh1750.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "GREENHOUSE";

#define RELAY_FAN     (GPIO_NUM_25)
#define RELAY_HEATER  (GPIO_NUM_26)
#define RELAY_PUMP    (GPIO_NUM_27)
#define RELAY_LIGHT   (GPIO_NUM_32)
#define SOIL_ADC      (ADC1_CHANNEL_4)

typedef struct {
  float temp_min, temp_max;
  float humidity_min;
  float light_min;
  int soil_moisture_min;
} greenhouse_config_t;

static void priv_set_relay(gpio_num_t pin, bool on)
{
  gpio_set_level(pin, on ? 1 : 0);
}

void greenhouse_example(void)
{
  /* Initialize relays */
  gpio_set_direction(RELAY_FAN, GPIO_MODE_OUTPUT);
  gpio_set_direction(RELAY_HEATER, GPIO_MODE_OUTPUT);
  gpio_set_direction(RELAY_PUMP, GPIO_MODE_OUTPUT);
  gpio_set_direction(RELAY_LIGHT, GPIO_MODE_OUTPUT);

  /* Initialize sensors */
  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "Greenhouse_Manager", NULL, NULL);

  star_bus_config_t *i2c = star_bus_config_create_i2c(
    "bh1750", I2C_NUM_0, 0x23, GPIO_NUM_21, GPIO_NUM_22, 400000);

  star_bus_manager_add_bus(&manager, i2c);
  star_bus_config_init(i2c, &manager);

  dht22_handle_t dht;
  bh1750_handle_t light;

  dht22_config_t dht_cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };
  bh1750_config_t light_cfg = {
    .i2c_addr = 0x23,
    .mode = BH1750_MODE_CONTINUOUS,
    .resolution = BH1750_RES_HIGH,
    .mtreg = BH1750_DEFAULT_MTREG
  };

  star_sensor_dht22_init(&dht, &dht_cfg);
  star_sensor_bh1750_init(&light, &manager, "bh1750", &light_cfg);

  greenhouse_config_t config = {
    .temp_min = 18.0f,
    .temp_max = 28.0f,
    .humidity_min = 50.0f,
    .light_min = 500.0f,
    .soil_moisture_min = 2000
  };

  ESP_LOGI(s_tag, "Greenhouse automation started");

  for (int i = 0; i < 500; i++) {
    dht22_data_t dht_data;
    float lux;

    star_sensor_dht22_read(&dht, &dht_data);
    star_sensor_bh1750_read_light(&light, &lux);

    ESP_LOGI(s_tag, "Temp: %.1fC, Hum: %.1f%%, Light: %.0f lux",
             dht_data.temperature_c, dht_data.humidity_rh, lux);

    /* Temperature control */
    if (dht_data.temperature_c > config.temp_max) {
      priv_set_relay(RELAY_FAN, true);
      priv_set_relay(RELAY_HEATER, false);
      ESP_LOGI(s_tag, "Cooling - fan ON");
    } else if (dht_data.temperature_c < config.temp_min) {
      priv_set_relay(RELAY_FAN, false);
      priv_set_relay(RELAY_HEATER, true);
      ESP_LOGI(s_tag, "Heating - heater ON");
    } else {
      priv_set_relay(RELAY_FAN, false);
      priv_set_relay(RELAY_HEATER, false);
    }

    /* Light control */
    if (lux < config.light_min) {
      priv_set_relay(RELAY_LIGHT, true);
      ESP_LOGI(s_tag, "Supplemental light ON");
    } else {
      priv_set_relay(RELAY_LIGHT, false);
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }

  /* Turn off all relays */
  priv_set_relay(RELAY_FAN, false);
  priv_set_relay(RELAY_HEATER, false);
  priv_set_relay(RELAY_PUMP, false);
  priv_set_relay(RELAY_LIGHT, false);

  star_sensor_dht22_deinit(&dht);
  star_sensor_bh1750_deinit(&light);
  star_bus_manager_deinit(&manager);
}

void app_main(void) { greenhouse_example(); }
