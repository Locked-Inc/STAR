#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
#include "star_sensor_pca9685.h"

/* Hardware Config */
#define NUM_HCSR04 (2)
#define HCSR04_MAX_DISTANCE_CM (400)

/* Left Sensor */
#define HCSR04_LEFT (0)
#define GPIO_LEFT_TRIG (GPIO_NUM_18)
#define GPIO_LEFT_ECHO (GPIO_NUM_19)

/* Right Sensor */
#define HCSR04_RIGHT (1)
#define GPIO_RIGHT_TRIG (GPIO_NUM_4)
#define GPIO_RIGHT_ECHO (GPIO_NUM_2)

/* DHT22 Pin (out) */
#define GPIO_DHT22_PIN (GPIO_NUM_5)

/* PCA9685 */
#define GPIO_PCA9685_I2C_SDA (GPIO_NUM_21)
#define GPIO_PCA9685_I2C_SCL (GPIO_NUM_22)
#define PCA9685_I2C_FREQUENCY (400000)
#define PCA9685_I2C_NUM (I2C_NUM_0)
#define PCA9685_PWM_FREQUENCY_kHZ (50)
#define GPIO_PCA9685_OE (GPIO_NUM_15)

static const char* s_TAG         = "POC Demo";
static const char* s_DHT22_TAG   = "DHT-22";
static const char* s_HCSR04_TAG  = "HC-SR04";
static const char* s_PCA9685_TAG = "PCA9685";

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
  ret = star_bus_dht22_read(&bus_manager, s_DHT22_TAG, &dht22_data); // TODO: Move this to a Task
  float temp_c = 25.0f; /* Default temperature if read fails */
  if (ret == ESP_OK && dht22_data.checksum_valid) {
    temp_c = dht22_data.temperature_c;
    ESP_LOGI(s_TAG,
             "DHT-22 Temperature: %.1f C, Humidity: %.1f %%",
             dht22_data.temperature_c,
             dht22_data.humidity_percent);
  } else {
    ESP_LOGW(s_TAG, "DHT-22 read failed, using default temperature %.1f C", temp_c);
  }

  /* Set up HCSR-04 */
  /* First create a GPIO bus for the HC-SR04 sensors */
  gpio_num_t hcsr04_pins[] = {GPIO_LEFT_TRIG, GPIO_LEFT_ECHO, GPIO_RIGHT_TRIG, GPIO_RIGHT_ECHO};
  star_bus_config_t* hcsr04_gpio_bus =
    star_bus_config_create_gpio(s_HCSR04_TAG,
                                hcsr04_pins,
                                sizeof(hcsr04_pins) / sizeof(hcsr04_pins[0]));
  if (hcsr04_gpio_bus == NULL) {
    ESP_LOGE(s_TAG, "Failed to create GPIO bus for HC-SR04");
    star_bus_manager_deinit(&bus_manager);
    star_error_interface_destroy(error_iface);
    return;
  }
  ret = star_bus_manager_add_bus(&bus_manager, hcsr04_gpio_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to add GPIO bus for HC-SR04: %s", esp_err_to_name(ret));
    star_bus_config_destroy(hcsr04_gpio_bus);
    star_bus_manager_deinit(&bus_manager);
    star_error_interface_destroy(error_iface);
    return;
  }

  const hcsr04_config_t hcsr04_config[NUM_HCSR04] = {{GPIO_LEFT_TRIG, GPIO_LEFT_ECHO, temp_c},
                                                     {GPIO_RIGHT_TRIG, GPIO_RIGHT_ECHO, temp_c}};
  hcsr04_handle_t       hcsr04_handles[NUM_HCSR04];
  float                 hcsr04_data[NUM_HCSR04];

  for (uint8_t i = 0; i < NUM_HCSR04; i++) {
    ret = star_sensor_hcsr04_init(&hcsr04_handles[i],
                                  &bus_manager,
                                  s_HCSR04_TAG,
                                  error_iface,
                                  &hcsr04_config[i]);
    if (ret != ESP_OK) {
      ESP_LOGE(s_TAG, "Failed to init HC-SR04 sensor %d: %s", i, esp_err_to_name(ret));
    }
  }

  /* Set up PCA9685 */
  star_bus_config_t* pwm_bus = star_bus_config_create_i2c(s_PCA9685_TAG,
                                                          PCA9685_I2C_NUM,
                                                          PCA9685_DEFAULT_ADDR,
                                                          GPIO_PCA9685_I2C_SDA,
                                                          GPIO_PCA9685_I2C_SCL,
                                                          PCA9685_I2C_FREQUENCY);
  star_bus_manager_add_bus(&bus_manager, pwm_bus);

  pca9685_handle_t pca9685_handle;
  pca9685_config_t pca9685_config = {.i2c_addr      = PCA9685_DEFAULT_ADDR,
                                     .pwm_freq      = PCA9685_PWM_FREQUENCY_kHZ,
                                     .output_mode   = PCA9685_OUTPUT_TOTEM_POLE,
                                     .ext_clock     = false,
                                     .invert_output = false};
  ret                             = star_sensor_pca9685_init(&pca9685_handle,
                                 &bus_manager,
                                 s_PCA9685_TAG,
                                 error_iface,
                                 GPIO_PCA9685_OE,
                                 false,
                                 &pca9685_config);

  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to init PCA9685: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&bus_manager);
    star_error_interface_destroy(error_iface);
    return;
  }

  ESP_LOGI(s_TAG, "PCA9685 initialized successfully");

  while (1) {
    for (uint8_t i = 0; i < NUM_HCSR04; i++) {
      ret = star_sensor_hcsr04_read_distance(&hcsr04_handles[i], &hcsr04_data[i]);

      if (ret == ESP_OK) {
        ESP_LOGI(s_TAG, "Sensor %d distance: %.1f cm", i, hcsr04_data[i]);

        if (hcsr04_data[i] >= HCSR04_MAX_DISTANCE_CM) {
          star_sensor_pca9685_set_channel_off(&pca9685_handle, i);
        } else {
          /* Set LED brightness based on distance (closer = brighter) */
          float duty_percent = 100.0f * (1.0f - (hcsr04_data[i] / HCSR04_MAX_DISTANCE_CM));
          star_sensor_pca9685_set_duty_cycle(&pca9685_handle, i, duty_percent);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
