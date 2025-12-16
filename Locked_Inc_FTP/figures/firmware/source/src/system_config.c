/**
 * @file system_config.c
 * @brief Global system configuration instance
 *
 * This file defines the primary configuration structure using type-safe
 * constants following modern embedded C best practices.
 */

#include "include/system_config.h"

/* ========================================================================== */
/* ======================== GLOBAL CONFIGURATION =========================== */
/* ========================================================================== */

/**
 * @brief Primary system configuration
 *
 * This const structure contains all system configuration in a type-safe,
 * organized manner.
 *
 * Example usage:
 * @code
 * // Access GPIO pin
 * gpio_num_t trig_pin = STAR_SYSTEM_CONFIG.gpio.left_trig;
 * 
 * // Access task configuration
 * uint32_t stack_size = STAR_SYSTEM_CONFIG.tasks.sensors.stack_size;
 * 
 * // Access PWM configuration
 * float max_pwm = STAR_SYSTEM_CONFIG.pwm.max_pwm_all;
 * @endcode
 */
const star_system_config_t STAR_SYSTEM_CONFIG = {
  /* GPIO Pin Configuration */
  .gpio =
    {/* HC-SR04 Sensors */
     .left_trig  = GPIO_NUM_18,
     .left_echo  = GPIO_NUM_19,
     .right_trig = GPIO_NUM_4,
#ifdef CONFIG_STAR_BOARD_ESP32_WROOM_32
     .right_echo = GPIO_NUM_23,
#elif defined(CONFIG_STAR_BOARD_ESP32_S3_WROOM_1_N16)
     .right_echo = GPIO_NUM_47, // Use available GPIO on ESP32-S3
#else
     .right_echo = GPIO_NUM_23, // Default fallback
#endif

     /* DHT22 Sensor */
     .dht22_data = GPIO_NUM_5,

/* PCA9685 I2C */
#ifdef CONFIG_STAR_BOARD_ESP32_WROOM_32
     .pca9685_sda = GPIO_NUM_21,
     .pca9685_scl = GPIO_NUM_22,
#elif defined(CONFIG_STAR_BOARD_ESP32_S3_WROOM_1_N16)
     .pca9685_sda = GPIO_NUM_8,
     .pca9685_scl = GPIO_NUM_9,
#else
     .pca9685_sda = GPIO_NUM_21, // Default fallback
     .pca9685_scl = GPIO_NUM_22, // Default fallback
#endif
     .pca9685_oe = GPIO_NUM_15},

  /* PCA9685 PWM Controller Configuration */
  .pca9685 = {.i2c_addr         = 0x40,
              .i2c_frequency    = 400000,
              .i2c_port         = I2C_NUM_0,
              .pwm_frequency_hz = 50},

  /* Distance Sensing Configuration */
  .distance = {.min_distance_cm       = 5.0f,
               .max_distance_cm       = 50.0f,
               .extension_factor      = 0.7f,
               .close_boost_threshold = 0.25f},

  /* PWM Configuration */
  .pwm = {.min_pwm_red   = 5.0f,
          .min_pwm_green = 5.0f,
          .min_pwm_blue  = 5.0f,
          .max_pwm_all   = 75.0f,
          .off_threshold = 0.001f},

  /* Task Configuration */
  .tasks = {.dht22    = {.name        = "dht22_task",
                         .stack_size  = 2048,
                         .priority    = k_star_task_priority_dht22,
                         .interval_ms = 30000},
            .sensors  = {.name        = "sensors_task",
                         .stack_size  = 3072,
                         .priority    = k_star_task_priority_sensors,
                         .interval_ms = 30},
            .leds     = {.name        = "leds_task",
                         .stack_size  = 4096,
                         .priority    = k_star_task_priority_leds,
                         .interval_ms = 15},
            .watchdog = {.name        = "watchdog_task",
                         .stack_size  = 3072,
                         .priority    = k_star_task_priority_watchdog,
                         .interval_ms = 1000}},

  /* System Health Configuration */
  .health = {.max_sensor_failures            = 5,
             .default_temperature_c          = 25.0f,
             .restart_delay_ms               = 5000,
             .failure_rate_degraded          = 0.4f,
             .failure_rate_critical          = 0.7f,
             .task_unresponsive_threshold_ms = 30000,
             .health_check_log_interval      = 60,
             .main_loop_log_interval         = 100},

  /* FreeRTOS Configuration */
  .freertos = {.sensor_data_queue_size  = 64,
               .temperature_queue_size  = 4,
               .mutex_timeout_ms        = 1000,
               .notification_timeout_ms = 5000},

  /* Bus Names */
  .bus_names = {.dht22 = "dht22_bus", .hcsr04 = "hcsr04_bus", .pca9685 = "pca9685_bus"},

  /* Logging Tags */
  .log_tags = {.main     = "MAIN",
               .dht22    = "DHT22_TASK",
               .sensors  = "SENSORS_TASK",
               .leds     = "LEDS_TASK",
               .watchdog = "WATCHDOG_TASK",
               .led_ctrl = "LED_CTRL"},

  /* Sensor Count */
  .num_hcsr04_sensors = STAR_SYSTEM_HCSR04_SENSOR_COUNT};

/* ========================================================================== */
/* ========================= CONFIGURATION HELPERS ========================= */
/* ========================================================================== */

/**
 * @brief Get task configuration by priority
 * @param priority Task priority enum value
 * @return Pointer to task configuration or NULL if not found
 */
const star_task_config_t* star_get_task_config_by_priority(const star_task_priority_t priority)
{
  switch (priority) {
    case k_star_task_priority_dht22:
      return &STAR_SYSTEM_CONFIG.tasks.dht22;
    case k_star_task_priority_sensors:
      return &STAR_SYSTEM_CONFIG.tasks.sensors;
    case k_star_task_priority_leds:
      return &STAR_SYSTEM_CONFIG.tasks.leds;
    case k_star_task_priority_watchdog:
      return &STAR_SYSTEM_CONFIG.tasks.watchdog;
    default:
      return NULL;
  }
}

/**
 * @brief Check if LED channel is valid
 * @param channel LED channel enum value
 * @return true if valid, false otherwise
 */
bool star_is_valid_led_channel(const star_led_channel_t channel)
{
  return channel >= k_star_led_channel_left_red && channel < k_star_led_channel_max;
}

/**
 * @brief Get health status based on failure rate
 * @param failure_rate Current failure rate (0.0 to 1.0)
 * @return Health status enum
 */
star_health_status_t star_get_health_status(const float failure_rate)
{
  if (failure_rate >= STAR_SYSTEM_CONFIG.health.failure_rate_critical) {
    return k_star_health_status_critical;
  } else if (failure_rate >= STAR_SYSTEM_CONFIG.health.failure_rate_degraded) {
    return k_star_health_status_degraded;
  }
  return k_star_health_status_ok;
}