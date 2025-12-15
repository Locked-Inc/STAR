#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * System Configuration Header
 * Centralizes all hardware configuration, timing parameters, and system constants
 * Following STAR embedded best practices for maintainable configuration
 *
 * This header provides type-safe configuration using structs, enums, and const variables.
 * Access all system configuration through the STAR_SYSTEM_CONFIG global struct.
 */

/* Forward declarations */
typedef struct star_system_config star_system_config_t;

/* ========== Enums for Type Safety ========== */

/* Task Priorities */
typedef enum {
  k_star_task_priority_sensors  = 1, /* Lowest - data collection */
  k_star_task_priority_dht22    = 2, /* Low - optional sensor */
  k_star_task_priority_leds     = 3, /* HIGH - user responsiveness */
  k_star_task_priority_watchdog = 4  /* Highest - system safety */
} star_task_priority_t;

/* LED Channel Mappings */
typedef enum {
  k_star_led_channel_left_red    = 0,
  k_star_led_channel_left_green  = 1,
  k_star_led_channel_left_blue   = 2,
  k_star_led_channel_right_red   = 3,
  k_star_led_channel_right_green = 4,
  k_star_led_channel_right_blue  = 5,
  k_star_led_channel_max         = 6
} star_led_channel_t;

/* System Health Status */
typedef enum {
  k_star_health_status_ok,
  k_star_health_status_degraded,
  k_star_health_status_critical
} star_health_status_t;

/* ========== Configuration Structures ========== */

/* GPIO Pin Configuration */
typedef struct {
  /* HC-SR04 Sensors */
  const gpio_num_t left_trig;
  const gpio_num_t left_echo;
  const gpio_num_t right_trig;
  const gpio_num_t right_echo;

  /* DHT22 Sensor */
  const gpio_num_t dht22_data;

  /* PCA9685 I2C */
  const gpio_num_t pca9685_sda;
  const gpio_num_t pca9685_scl;
  const gpio_num_t pca9685_oe;
} star_gpio_config_t;

/* PCA9685 Configuration */
typedef struct {
  const uint8_t    i2c_addr;
  const uint32_t   i2c_frequency;
  const i2c_port_t i2c_port;
  const uint32_t   pwm_frequency_hz;
} star_pca9685_config_t;

/* Distance Sensing Configuration */
typedef struct {
  const float min_distance_cm;
  const float max_distance_cm;
  const float extension_factor;
  const float close_boost_threshold;
} star_distance_config_t;

/* PWM Configuration */
typedef struct {
  const float min_pwm_red;
  const float min_pwm_green;
  const float min_pwm_blue;
  const float max_pwm_all;
  const float off_threshold;
} star_pwm_config_t;

/* Task Configuration */
typedef struct {
  const char*                name;
  const uint32_t             stack_size;
  const star_task_priority_t priority;
  const uint32_t             interval_ms;
} star_task_config_t;

/* Task Set Configuration */
typedef struct {
  const star_task_config_t dht22;
  const star_task_config_t sensors;
  const star_task_config_t leds;
  const star_task_config_t watchdog;
} star_tasks_config_t;

/* System Health Configuration */
typedef struct {
  const uint32_t max_sensor_failures;
  const float    default_temperature_c;
  const uint32_t restart_delay_ms;
  const float    failure_rate_degraded;
  const float    failure_rate_critical;
  const uint32_t task_unresponsive_threshold_ms;
  const uint32_t health_check_log_interval;
  const uint32_t main_loop_log_interval;
} star_health_config_t;

/* FreeRTOS Configuration */
typedef struct {
  const uint32_t sensor_data_queue_size;
  const uint32_t temperature_queue_size;
  const uint32_t mutex_timeout_ms;
  const uint32_t notification_timeout_ms;
} star_freertos_config_t;

/* Bus Names Configuration */
typedef struct {
  const char* const dht22;
  const char* const hcsr04;
  const char* const pca9685;
} star_bus_names_t;

/* Logging Tags Configuration */
typedef struct {
  const char* const main;
  const char* const dht22;
  const char* const sensors;
  const char* const leds;
  const char* const watchdog;
  const char* const led_ctrl;
} star_log_tags_t;

/* Primary Configuration Structure */
struct star_system_config {
  const star_gpio_config_t     gpio;
  const star_pca9685_config_t  pca9685;
  const star_distance_config_t distance;
  const star_pwm_config_t      pwm;
  const star_tasks_config_t    tasks;
  const star_health_config_t   health;
  const star_freertos_config_t freertos;
  const star_bus_names_t       bus_names;
  const star_log_tags_t        log_tags;
  const uint32_t               num_hcsr04_sensors;
};

/* ========== Global Configuration Instance ========== */

/* Type-safe global configuration - use this in new code */
extern const star_system_config_t STAR_SYSTEM_CONFIG;

/* ========== Array Size Macros ========== */

/**
 * @brief Number of HC-SR04 ultrasonic sensors in the system
 * 
 * This macro provides compile-time access to the sensor count for array sizing.
 * IMPORTANT: This value must match STAR_SYSTEM_CONFIG.num_hcsr04_sensors in system_config.c
 */
#define STAR_SYSTEM_HCSR04_SENSOR_COUNT (2)

/* ========== Validation Constants ========== */

/* Distance validation limits (cm) */
static const float s_star_distance_min_cm = 0.0f;   /**< Minimum valid distance in centimeters */
static const float s_star_distance_max_cm = 400.0f; /**< Maximum valid distance in centimeters */

/* Temperature validation limits (Celsius) */
static const float s_star_temperature_min_c = -40.0f; /**< Minimum valid temperature in Celsius */
static const float s_star_temperature_max_c = 80.0f;  /**< Maximum valid temperature in Celsius */

/* PWM validation limits (percent) */
static const float s_star_pwm_min_percent = 0.0f;   /**< Minimum valid PWM percentage */
static const float s_star_pwm_max_percent = 100.0f; /**< Maximum valid PWM percentage */

/* ========== Inline Validation Functions ========== */

/**
 * @brief Validate sensor index
 * @param idx Sensor index to validate
 * @return true if valid, false otherwise
 */
static inline bool star_validate_sensor_index(uint32_t idx)
{
  return idx < STAR_SYSTEM_CONFIG.num_hcsr04_sensors;
}

/**
 * @brief Validate distance measurement
 * @param distance Distance in cm
 * @return true if valid, false otherwise
 */
static inline bool star_validate_distance(float distance)
{
  return (distance >= s_star_distance_min_cm && distance <= s_star_distance_max_cm);
}

/**
 * @brief Validate temperature reading
 * @param temperature Temperature in Celsius
 * @return true if valid, false otherwise
 */
static inline bool star_validate_temperature(float temperature)
{
  return (temperature >= s_star_temperature_min_c && temperature <= s_star_temperature_max_c);
}

/**
 * @brief Validate PWM percentage
 * @param pwm PWM value in percent
 * @return true if valid, false otherwise
 */
static inline bool star_validate_pwm_percent(float pwm)
{
  return (pwm >= s_star_pwm_min_percent && pwm <= s_star_pwm_max_percent);
}

/* ========== Helper Function Declarations ========== */

/**
 * @brief Get task configuration by priority
 * @param priority Task priority enum value
 * @return Pointer to task configuration or NULL if not found
 */
const star_task_config_t* star_get_task_config_by_priority(star_task_priority_t priority);

/**
 * @brief Check if LED channel is valid
 * @param channel LED channel enum value
 * @return true if valid, false otherwise
 */
bool star_is_valid_led_channel(star_led_channel_t channel);

/**
 * @brief Get health status based on failure rate
 * @param failure_rate Current failure rate (0.0 to 1.0)
 * @return Health status enum
 */
star_health_status_t star_get_health_status(const float failure_rate);

/* ========== Sensor Array Indices ========== */

/* HC-SR04 sensor indices for array access */
typedef enum {
  k_star_system_hcsr04_left  = 0,
  k_star_system_hcsr04_right = 1
} hcsr04_sensor_index_t;

#endif /* SYSTEM_CONFIG_H */