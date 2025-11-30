#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <driver/gpio.h>
#include <driver/i2c.h>

/*
 * System Configuration Header
 * Centralizes all hardware configuration, timing parameters, and system constants
 * Following STAR embedded best practices for maintainable configuration
 */

/* ========== Hardware Configuration ========== */

/* HC-SR04 Ultrasonic Sensors */
#define STAR_SYSTEM_NUM_HCSR04 (2)

/* Sensor Array Indices */
typedef enum { STAR_SYSTEM_HCSR04_LEFT = 0, STAR_SYSTEM_HCSR04_RIGHT = 1 } hcsr04_sensor_index_t;

/* Left Sensor Pins */
#define STAR_SYSTEM_GPIO_LEFT_TRIG (GPIO_NUM_18)
#define STAR_SYSTEM_GPIO_LEFT_ECHO (GPIO_NUM_19)

/* Right Sensor Pins */
#define STAR_SYSTEM_GPIO_RIGHT_TRIG (GPIO_NUM_4)
#define STAR_SYSTEM_GPIO_RIGHT_ECHO (GPIO_NUM_23)

/* DHT22 Temperature/Humidity Sensor */
#define STAR_SYSTEM_GPIO_DHT22_PIN (GPIO_NUM_5)

/* PCA9685 PWM Controller */
#define STAR_SYSTEM_GPIO_PCA9685_I2C_SDA (GPIO_NUM_21)
#define STAR_SYSTEM_GPIO_PCA9685_I2C_SCL (GPIO_NUM_22)
#define STAR_SYSTEM_GPIO_PCA9685_OE (GPIO_NUM_15)
#define STAR_SYSTEM_PCA9685_I2C_ADDR (0x40)
#define STAR_SYSTEM_PCA9685_I2C_FREQUENCY (400000)
#define STAR_SYSTEM_PCA9685_I2C_NUM (I2C_NUM_0)
#define STAR_SYSTEM_PCA9685_PWM_FREQUENCY_kHZ (50)

/* ========== RGB LED Channel Mappings ========== */

/* Left LED Channels */
#define STAR_SYSTEM_LED_LEFT_RED (0)
#define STAR_SYSTEM_LED_LEFT_GREEN (1)
#define STAR_SYSTEM_LED_LEFT_BLUE (2)

/* Right LED Channels */
#define STAR_SYSTEM_LED_RIGHT_RED (3)
#define STAR_SYSTEM_LED_RIGHT_GREEN (4)
#define STAR_SYSTEM_LED_RIGHT_BLUE (5)

/* ========== Distance Sensing Parameters ========== */
#define STAR_SYSTEM_LED_COLOR_DISTANCE_MIN (5.0f)   /* Minimum distance for color mapping (cm) */
#define STAR_SYSTEM_LED_COLOR_DISTANCE_MAX (100.0f)  /* Maximum distance for color mapping (cm) */

/* Advanced Range Control (optional fine-tuning) */
#define STAR_SYSTEM_LED_COLOR_EXTENSION_FACTOR (0.7f) /* Extend min range by this factor for ultra-smooth transitions */
#define STAR_SYSTEM_LED_COLOR_CLOSE_BOOST_THRESHOLD (0.25f) /* Fraction of range for brightness boost (0.0-1.0) */

/* ========== LED PWM Configuration ========== */

/* PWM scaling parameters (lower = brighter, higher = dimmer) */
/* Safe range without current limiting resistors: 5-75% PWM */
#define STAR_SYSTEM_MIN_PWM_RED (5.0f)    /* Red LED minimum PWM (very dim) */
#define STAR_SYSTEM_MIN_PWM_GREEN (5.0f)  /* Green LED minimum PWM (very dim) */
#define STAR_SYSTEM_MIN_PWM_BLUE (5.0f)   /* Blue LED minimum PWM (very dim) */
#define STAR_SYSTEM_MAX_PWM_ALL (75.0f)   /* Maximum PWM for all colors (safe without resistors) */

/* LED brightness threshold for "off" detection - much lower for smooth gradients */
#define STAR_SYSTEM_LED_OFF_THRESHOLD (0.001f)  /* 0.001% threshold for nearly seamless transitions */

/* ========== Task Configuration ========== */

/* Task priorities (higher number = higher priority) */
#define STAR_SYSTEM_TASK_PRIORITY_DHT22 (2)
#define STAR_SYSTEM_TASK_PRIORITY_SENSORS (3)
#define STAR_SYSTEM_TASK_PRIORITY_LEDS (1)
#define STAR_SYSTEM_TASK_PRIORITY_WATCHDOG (4)

/* Task stack sizes (in words, not bytes) */
#define STAR_SYSTEM_TASK_STACK_SIZE_DHT22 (2048)
#define STAR_SYSTEM_TASK_STACK_SIZE_SENSORS (3072)
#define STAR_SYSTEM_TASK_STACK_SIZE_LEDS (4096)  /* Increased for error handling and logging */
#define STAR_SYSTEM_TASK_STACK_SIZE_WATCHDOG (3072)

/* Task timing intervals (in milliseconds) */
#define STAR_SYSTEM_TASK_INTERVAL_DHT22 (30000)   /* Read temperature every 30 seconds */
#define STAR_SYSTEM_TASK_INTERVAL_SENSORS (50)    /* Read distance sensors every 50ms */
#define STAR_SYSTEM_TASK_INTERVAL_LEDS (50)       /* Update LEDs every 50ms */
#define STAR_SYSTEM_TASK_INTERVAL_WATCHDOG (1000) /* Check system health every 1 second */

/* ========== System Health Parameters ========== */

/* Maximum consecutive sensor read failures before recovery action */
#define STAR_SYSTEM_MAX_SENSOR_FAILURES (5)

/* Temperature sensor default fallback value */
#define STAR_SYSTEM_DEFAULT_TEMPERATURE_C (25.0f)

/* Watchdog timeout for system restart (in milliseconds) */
#define STAR_SYSTEM_RESTART_DELAY_MS (5000)

/* Health monitoring thresholds */
#define STAR_SYSTEM_SENSOR_FAILURE_RATE_DEGRADED (0.4f) /* 40% failure rate = degraded */
#define STAR_SYSTEM_SENSOR_FAILURE_RATE_CRITICAL (0.7f) /* 70% failure rate = critical */

/* Task response timeout thresholds */
#define STAR_SYSTEM_TASK_UNRESPONSIVE_THRESHOLD_MS (30000) /* 30 seconds */

/* System monitoring intervals */
#define STAR_SYSTEM_HEALTH_CHECK_LOG_INTERVAL (60) /* Log health every 60 checks */
#define STAR_SYSTEM_MAIN_LOOP_LOG_INTERVAL (100)   /* Log main status every 100 iterations */

/* ========== FreeRTOS Configuration ========== */

/* Queue sizes for inter-task communication */
#define STAR_SYSTEM_SENSOR_DATA_QUEUE_SIZE (16)  // Increased from 4 to handle sensor burst
#define STAR_SYSTEM_TEMPERATURE_QUEUE_SIZE (4)   // Increased from 2

/* Mutex timeouts (in milliseconds) */
#define STAR_SYSTEM_MUTEX_TIMEOUT_MS (1000)

/* Notification timeouts (in milliseconds) */
#define STAR_SYSTEM_NOTIFICATION_TIMEOUT_MS (5000)

/* ========== Bus Names ========== */

#define STAR_SYSTEM_BUS_NAME_DHT22 "dht22_bus"
#define STAR_SYSTEM_BUS_NAME_HCSR04 "hcsr04_bus"
#define STAR_SYSTEM_BUS_NAME_PCA9685 "pca9685_bus"

/* ========== Logging Tags ========== */

#define STAR_SYSTEM_TAG_MAIN "MAIN"
#define STAR_SYSTEM_TAG_DHT22 "DHT22_TASK"
#define STAR_SYSTEM_TAG_SENSORS "SENSORS_TASK"
#define STAR_SYSTEM_TAG_LEDS "LEDS_TASK"
#define STAR_SYSTEM_TAG_WATCHDOG "WATCHDOG_TASK"
#define STAR_SYSTEM_TAG_LED_CTRL "LED_CTRL"

/* ========== Validation Macros ========== */

#define STAR_SYSTEM_VALIDATE_SENSOR_INDEX(idx) ((idx) < STAR_SYSTEM_NUM_HCSR04)
#define STAR_SYSTEM_VALIDATE_DISTANCE(dist) ((dist) >= 0.0f && (dist) <= 400.0f)
#define STAR_SYSTEM_VALIDATE_TEMPERATURE(temp) ((temp) >= -40.0f && (temp) <= 80.0f)
#define STAR_SYSTEM_VALIDATE_PWM_PERCENT(pwm) ((pwm) >= 0.0f && (pwm) <= 100.0f)

#endif /* SYSTEM_CONFIG_H */