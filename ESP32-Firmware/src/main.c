// TODO: Add PCA9685 To this to move servo motors

#include <esp_log.h>

#include "star_bus_common_types.h"
#include "star_bus_config.h"
#include "star_bus_dht22_proprietary.h"
#include "star_bus_gpio.h"
#include "star_bus_manager.h"
#include "star_bus_manager_types.h"
#include "star_error_handler.h"
#include "star_error_interface.h"
#include "star_pin_interface.h"
#include "star_pin_validator.h"
#include "star_sensor_hcsr04.h"

/* Hardware Config */
#define NUM_HCSR04 (2)
#define HCSR04_MAX_DISTANCE_CM (400)

// Left Sensor
#define HCSR04_LEFT    (0)
#define GPIO_LEFT_TRIG (GPIO_NUM_18)
#define GPIO_LEFT_ECHO (GPIO_NUM_19)

// Right Sensor
#define HCSR04_RIGHT    (1)
#define GPIO_RIGHT_TRIG (GPIO_NUM_21)
#define GPIO_RIGHT_ECHO (GPIO_NUM_22)

// DHT22 Pin (out)
#define GPIO_DHT22_PIN (GPIO_NUM_5)

static const char* s_TAG      = "POC Demo";
static const char* s_DHT22_TAG = "DHT-22";
static const char* s_HCSR04_TAG = "HC-SR04";

void app_main(void)
{
  /* I am using 2 HC-SR04 Sensors, one for "Left" and one for "Right", in the actual project, we will have 7.
     * This is a POC for a software demo. For accurate data collection, speed of sound needs correction with temperature.
     * In this example a DHT-22 will be used. However, for the actual project we will use DS18B20 with OneWire
     */

  esp_err_t ret;

  ESP_LOGI(s_TAG, "Starting POC Demo");

  /* First we need to set up the pin validator */
  star_pin_interface_t pin_iface;
  pin_validator_get_interface(&pin_iface);

  /* Now set up the error interface */
  star_error_interface_t* error_iface = star_error_interface_create_default();
  if (error_iface == NULL) {
    ESP_LOGE(s_TAG, "Failed to create error interface");
    return;
  }

  /* Setup bus manager */
  star_bus_manager_t bus_manager;
  ret = star_bus_manager_init(&bus_manager, "main_bus_mgr", error_iface, &pin_iface);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to init bus manager: %s", esp_err_to_name(ret));
    star_error_interface_destroy(error_iface);
    return;
  }

  /* Setup DHT-22 using proprietary single-wire protocol */
  ESP_LOGI(s_TAG, "Setting up DHT22 on GPIO %d", GPIO_DHT22_PIN);

  star_dht22_config_t dht22_config = STAR_DHT22_CONFIG_DEFAULT();
  dht22_config.gpio_pin            = GPIO_DHT22_PIN;

  ret = star_bus_dht22_init(&bus_manager, s_DHT22_TAG, &dht22_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to init DHT22: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&bus_manager);
    star_error_interface_destroy(error_iface);
    return;
  }

  /* Read temperature from DHT22 for sound speed correction */
  star_dht22_data_t dht22_data;
  ret          = star_bus_dht22_read(&bus_manager, s_DHT22_TAG, &dht22_data); // TODO: Move this to a Task
  float temp_c = 25.0f; /* Default temperature if read fails */
  if (ret == ESP_OK && dht22_data.checksum_valid) {
    temp_c = dht22_data.temperature_c;
    ESP_LOGI(s_TAG,
             "DHT22 Temperature: %.1f C, Humidity: %.1f %%",
             dht22_data.temperature_c,
             dht22_data.humidity_percent);
  } else {
    ESP_LOGW(s_TAG, "DHT22 read failed, using default temperature %.1f C", temp_c);
  }

  hcsr04_config_t hcsr04_config[NUM_HCSR04] = {{GPIO_LEFT_TRIG, GPIO_LEFT_ECHO, temp_c},
                                               {GPIO_RIGHT_TRIG, GPIO_RIGHT_ECHO, temp_c}};
  hcsr04_handle_t hcsr04_handle;
  float hcsr04_data[NUM_HCSR04];
  for (uint8_t i = 0; i < NUM_HCSR04; i++) {
    star_sensor_hcsr04_init(&hcsr04_handle, &bus_manager, s_HCSR04_TAG, error_iface, &hcsr04_config[i]);
  }

  while (1) {
    for (uint8_t i = 0; i < NUM_HCSR04; i++) {
      star_sensor_hcsr04_read_distance(&hcsr04_handle, &hcsr04_data[i]);

      if (hcsr04_data[i] >= HCSR04_MAX_DISTANCE_CM) {
        // TODO: Move Servo Motor
      }
    }
  }
}
