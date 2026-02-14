#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "include/system_config.h"
#include "modules/include/shared_data.h"
#include "star_bus_config.h"
#include "star_bus_dht22_proprietary.h"
#include "star_bus_gpio.h"
#include "star_bus_manager.h"
#include "star_error_handler.h"
#include "star_error_interface.h"
#include "star_pin_interface.h"
#include "star_pin_validator.h"
#include "star_sensor_pca9685.h"
#include "tasks/include/dht22_task.h"
#include "tasks/include/led_task.h"
#include "tasks/include/sensor_task.h"
#include "tasks/include/watchdog_task.h"

/* ========== Main Application Constants ========== */

/* Error Handler Configuration for Fast LED Response */
static const uint8_t s_main_fast_error_retries =
    2; /**< Fast retry count for LED operations */
static const uint32_t s_main_fast_error_base_delay =
    10; /**< Fast base delay in milliseconds */
static const uint32_t s_main_fast_error_max_delay =
    50; /**< Fast maximum delay in milliseconds */

/* Main Loop Configuration */
static const uint32_t s_main_loop_sleep_ms =
    10000; /**< Main loop sleep duration in milliseconds */

static const char *s_TAG = "MAIN"; /* Use STAR_SYSTEM_CONFIG.log_tags.main */

static esp_err_t
initialize_hardware_buses(star_bus_manager_t *const bus_manager) {
  esp_err_t ret;

  ESP_LOGI(s_TAG, "Setting up DHT22 on GPIO %d",
           STAR_SYSTEM_CONFIG.gpio.dht22_data);
  star_dht22_config_t dht22_config = STAR_DHT22_CONFIG_DEFAULT();
  dht22_config.gpio_pin = STAR_SYSTEM_CONFIG.gpio.dht22_data;

  ret = star_bus_dht22_init(bus_manager, STAR_SYSTEM_CONFIG.bus_names.dht22,
                            &dht22_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to init DHT22: %s", esp_err_to_name(ret));
    return ret;
  }

  const gpio_num_t hcsr04_pins[] = {
      STAR_SYSTEM_CONFIG.gpio.left_trig, STAR_SYSTEM_CONFIG.gpio.left_echo,
      STAR_SYSTEM_CONFIG.gpio.right_trig, STAR_SYSTEM_CONFIG.gpio.right_echo};
  star_bus_config_t *hcsr04_gpio_bus = star_bus_config_create_gpio(
      STAR_SYSTEM_CONFIG.bus_names.hcsr04, hcsr04_pins,
      sizeof(hcsr04_pins) / sizeof(hcsr04_pins[0]));

  if (hcsr04_gpio_bus == NULL) {
    ESP_LOGE(s_TAG, "Failed to create GPIO bus for HC-SR04");
    return ESP_ERR_NO_MEM;
  }

  ret = star_bus_manager_add_bus(bus_manager, hcsr04_gpio_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to add GPIO bus for HC-SR04: %s",
             esp_err_to_name(ret));
    star_bus_config_destroy(hcsr04_gpio_bus);
    return ret;
  }

  star_bus_config_t *pwm_bus = star_bus_config_create_i2c(
      STAR_SYSTEM_CONFIG.bus_names.pca9685, STAR_SYSTEM_CONFIG.pca9685.i2c_port,
      STAR_SYSTEM_CONFIG.pca9685.i2c_addr, STAR_SYSTEM_CONFIG.gpio.pca9685_sda,
      STAR_SYSTEM_CONFIG.gpio.pca9685_scl,
      STAR_SYSTEM_CONFIG.pca9685.i2c_frequency);

  if (pwm_bus == NULL) {
    ESP_LOGE(s_TAG, "Failed to create I2C bus for PCA9685");
    return ESP_ERR_NO_MEM;
  }

  ret = star_bus_manager_add_bus(bus_manager, pwm_bus);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to add I2C bus for PCA9685: %s",
             esp_err_to_name(ret));
    star_bus_config_destroy(pwm_bus);
    return ret;
  }

  ESP_LOGI(s_TAG, "All hardware buses initialized successfully");
  return ESP_OK;
}

static esp_err_t initialize_pca9685(star_bus_manager_t *const bus_manager,
                                    star_error_interface_t *const error_iface,
                                    pca9685_handle_t *const pca_handle) {
  const pca9685_config_t pca9685_config = {
      .i2c_addr = STAR_SYSTEM_CONFIG.pca9685.i2c_addr,
      .pwm_freq = STAR_SYSTEM_CONFIG.pca9685.pwm_frequency_hz,
      .output_mode = k_pca9685_output_totem_pole,
      .ext_clock = false,
      .invert_output = false};

  esp_err_t ret = star_sensor_pca9685_init(
      pca_handle, bus_manager, STAR_SYSTEM_CONFIG.bus_names.pca9685,
      error_iface, STAR_SYSTEM_CONFIG.gpio.pca9685_oe, false, &pca9685_config);

  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to init PCA9685: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG, "PCA9685 initialized successfully");
  return ESP_OK;
}

static esp_err_t start_system_tasks(star_bus_manager_t *const bus_manager,
                                    star_error_interface_t *const error_iface,
                                    const pca9685_handle_t *pca_handle) {
  esp_err_t ret;

  ret = dht22_task_start(bus_manager, STAR_SYSTEM_CONFIG.bus_names.dht22);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to start DHT22 task: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = sensor_task_start(bus_manager, error_iface,
                          STAR_SYSTEM_CONFIG.bus_names.hcsr04);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to start sensor task: %s", esp_err_to_name(ret));
    dht22_task_stop();
    return ret;
  }

  ret = led_task_start(pca_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to start LED task: %s", esp_err_to_name(ret));
    sensor_task_stop();
    dht22_task_stop();
    return ret;
  }

  ret = watchdog_task_start();
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to start watchdog task: %s", esp_err_to_name(ret));
    led_task_stop();
    sensor_task_stop();
    dht22_task_stop();
    return ret;
  }

  ESP_LOGI(s_TAG, "All system tasks started successfully");
  return ESP_OK;
}

void app_main(void) {
  ESP_LOGI(s_TAG, "Starting STAR Firmware - Task-Based Architecture");
  ESP_LOGI(s_TAG,
           "System: %d HC-SR04 sensors, DHT22 temperature, RGB LED feedback",
           STAR_SYSTEM_CONFIG.num_hcsr04_sensors);

  esp_err_t ret = shared_data_init();
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize shared data: %s",
             esp_err_to_name(ret));
    return;
  }

  shared_data_set_system_state(k_system_state_initializing);

  star_pin_interface_t pin_iface;
  pin_validator_get_interface(&pin_iface);

  /* Create fast-retry error handler for instant LED response */
  /* Fast config: 2 retries, 10ms base delay, 50ms max delay (vs default 3
   * retries, 100ms base, 5000ms max) */
  error_handler_t *fast_error_handler = error_handler_create_custom(
      s_main_fast_error_retries, s_main_fast_error_base_delay,
      s_main_fast_error_max_delay, NULL, NULL);
  if (fast_error_handler == NULL) {
    ESP_LOGE(s_TAG, "Failed to create fast error handler");
    shared_data_deinit();
    return;
  }

  star_error_interface_t *error_iface =
      (star_error_interface_t *)malloc(sizeof(star_error_interface_t));
  if (error_iface == NULL) {
    ESP_LOGE(s_TAG, "Failed to allocate error interface");
    error_handler_destroy_default(fast_error_handler);
    shared_data_deinit();
    return;
  }

  error_handler_get_interface(error_iface, fast_error_handler);

  star_bus_manager_t bus_manager;
  ret = star_bus_manager_init(&bus_manager, "main_bus_mgr", error_iface,
                              &pin_iface);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to init bus manager: %s", esp_err_to_name(ret));
    free(error_iface);
    error_handler_destroy_default(fast_error_handler);
    shared_data_deinit();
    return;
  }

  ret = initialize_hardware_buses(&bus_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Hardware bus initialization failed");
    star_bus_manager_deinit(&bus_manager);
    free(error_iface);
    error_handler_destroy_default(fast_error_handler);
    shared_data_deinit();
    return;
  }

  pca9685_handle_t pca9685_handle;
  ret = initialize_pca9685(&bus_manager, error_iface, &pca9685_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "PCA9685 initialization failed");
    star_bus_manager_deinit(&bus_manager);
    free(error_iface);
    error_handler_destroy_default(fast_error_handler);
    shared_data_deinit();
    return;
  }

  ret = start_system_tasks(&bus_manager, error_iface, &pca9685_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Task initialization failed");
    star_sensor_pca9685_deinit(&pca9685_handle);
    star_bus_manager_deinit(&bus_manager);
    free(error_iface);
    error_handler_destroy_default(fast_error_handler);
    shared_data_deinit();
    return;
  }

  shared_data_mark_system_ready();
  ESP_LOGI(s_TAG, "System initialization complete - all tasks are now running");
  ESP_LOGI(s_TAG, "Main task has completed initialization and will now exit");
  ESP_LOGI(s_TAG, "System is now fully managed by FreeRTOS tasks:");
  ESP_LOGI(s_TAG, "  - DHT22 task: Temperature monitoring every %dms",
           STAR_SYSTEM_CONFIG.tasks.dht22.interval_ms);
  ESP_LOGI(s_TAG, "  - Sensor task: Distance sensing every %dms",
           STAR_SYSTEM_CONFIG.tasks.sensors.interval_ms);
  ESP_LOGI(s_TAG, "  - LED task: Visual feedback every %dms",
           STAR_SYSTEM_CONFIG.tasks.leds.interval_ms);
  ESP_LOGI(s_TAG, "  - Watchdog task: Health monitoring every %dms",
           STAR_SYSTEM_CONFIG.tasks.watchdog.interval_ms);

  ESP_LOGI(s_TAG,
           "System ready - use watchdog for health monitoring and recovery");

  /* Keep main task alive to prevent FreeRTOS handle corruption */
  /* The system is now fully managed by the created tasks */
  while (1) {
    vTaskDelay(
        pdMS_TO_TICKS(s_main_loop_sleep_ms)); // Sleep for 10 seconds at a time
  }
}
