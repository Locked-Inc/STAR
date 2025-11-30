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

/* RGB LED Channel Mappings */
#define LED_LEFT_RED (0)
#define LED_LEFT_GREEN (1)
#define LED_LEFT_BLUE (2)
#define LED_RIGHT_RED (3)
#define LED_RIGHT_GREEN (4)
#define LED_RIGHT_BLUE (5)

/* Distance Ranges (in cm) */
#define DISTANCE_VERY_CLOSE (10.0f) /* < 10cm: Bright Red */
#define DISTANCE_CLOSE (25.0f)      /* 10-25cm: Red to Yellow */
#define DISTANCE_MEDIUM (50.0f)     /* 25-50cm: Yellow to Green */
#define DISTANCE_FAR (100.0f)       /* 50-100cm: Green to Blue */
#define DISTANCE_VERY_FAR (200.0f)  /* 100-200cm: Blue */
                                    /* > 200cm: LEDs off */

static const char* s_TAG         = "POC Demo";
static const char* s_DHT22_TAG   = "DHT-22";
static const char* s_HCSR04_TAG  = "HC-SR04";
static const char* s_PCA9685_TAG = "PCA9685";

/* PWM scaling - adjust these values as needed (lower = brighter, higher = dimmer) */
#define MIN_PWM_RED (45.0f)   /* Red LED minimum PWM */
#define MIN_PWM_GREEN (45.0f) /* Green LED minimum PWM */
#define MIN_PWM_BLUE (45.0f)  /* Blue LED minimum PWM */
#define MAX_PWM_ALL (100.0f)  /* Maximum PWM for all colors */

typedef struct {
  float red;
  float green;
  float blue;
} rgb_color_t;

static float internal_scale_pwm_for_color(float original_percent, float min_pwm)
{
  if (original_percent <= 0.1f) {
    return 0.0f;
  }
  return min_pwm + (original_percent / 100.0f) * (MAX_PWM_ALL - min_pwm);
}

static void
internal_set_rgb_led(const pca9685_handle_t* handle, uint8_t led_index, const rgb_color_t* color)
{
  uint8_t red_channel   = led_index * 3 + 0;
  uint8_t green_channel = led_index * 3 + 1;
  uint8_t blue_channel  = led_index * 3 + 2;

  float scaled_red   = internal_scale_pwm_for_color(color->red, MIN_PWM_RED);
  float scaled_green = internal_scale_pwm_for_color(color->green, MIN_PWM_GREEN);
  float scaled_blue  = internal_scale_pwm_for_color(color->blue, MIN_PWM_BLUE);

  star_sensor_pca9685_set_duty_cycle(handle, red_channel, scaled_red);
  star_sensor_pca9685_set_duty_cycle(handle, green_channel, scaled_green);
  star_sensor_pca9685_set_duty_cycle(handle, blue_channel, scaled_blue);
}

static rgb_color_t internal_distance_to_rgb(float distance_cm)
{
  rgb_color_t color = {0.0f, 0.0f, 0.0f};

  if (distance_cm > DISTANCE_VERY_FAR) {
    return color;
  }

  float brightness_factor = 1.0f;

  if (distance_cm < DISTANCE_VERY_CLOSE) {
    color.red         = 100.0f;
    brightness_factor = 1.0f;
  } else if (distance_cm < DISTANCE_CLOSE) {
    float ratio = (distance_cm - DISTANCE_VERY_CLOSE) / (DISTANCE_CLOSE - DISTANCE_VERY_CLOSE);
    color.red   = 100.0f;
    color.green = ratio * 100.0f;
    brightness_factor = 1.0f - (ratio * 0.3f);
  } else if (distance_cm < DISTANCE_MEDIUM) {
    float ratio       = (distance_cm - DISTANCE_CLOSE) / (DISTANCE_MEDIUM - DISTANCE_CLOSE);
    color.red         = (1.0f - ratio) * 100.0f;
    color.green       = 100.0f;
    brightness_factor = 0.7f - (ratio * 0.2f);
  } else if (distance_cm < DISTANCE_FAR) {
    float ratio       = (distance_cm - DISTANCE_MEDIUM) / (DISTANCE_FAR - DISTANCE_MEDIUM);
    color.green       = (1.0f - ratio) * 100.0f;
    color.blue        = ratio * 100.0f;
    brightness_factor = 0.5f - (ratio * 0.2f);
  } else if (distance_cm <= DISTANCE_VERY_FAR) {
    color.blue        = 100.0f;
    float ratio       = (distance_cm - DISTANCE_FAR) / (DISTANCE_VERY_FAR - DISTANCE_FAR);
    brightness_factor = 0.3f - (ratio * 0.2f);
  }

  color.red *= brightness_factor;
  color.green *= brightness_factor;
  color.blue *= brightness_factor;

  return color;
}

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
        const char* sensor_name = (i == HCSR04_LEFT) ? "Left" : "Right";
        ESP_LOGI(s_TAG, "%s sensor distance: %.1f cm", sensor_name, hcsr04_data[i]);

        rgb_color_t color = internal_distance_to_rgb(hcsr04_data[i]);
        internal_set_rgb_led(&pca9685_handle, i, &color);

        if (color.red > 0.1f || color.green > 0.1f || color.blue > 0.1f) {
          ESP_LOGI(s_TAG,
                   "%s LED: R=%.1f%%, G=%.1f%%, B=%.1f%%",
                   sensor_name,
                   color.red,
                   color.green,
                   color.blue);
        } else {
          ESP_LOGI(s_TAG, "%s LED: OFF (distance > %.0fcm)", sensor_name, DISTANCE_VERY_FAR);
        }
      } else {
        ESP_LOGW(s_TAG, "Failed to read sensor %d: %s", i, esp_err_to_name(ret));
        rgb_color_t off_color = {0.0f, 0.0f, 0.0f};
        internal_set_rgb_led(&pca9685_handle, i, &off_color);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
