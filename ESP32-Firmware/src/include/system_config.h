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
#define NUM_HCSR04 (2)

/* Sensor Array Indices */
typedef enum { HCSR04_LEFT = 0, HCSR04_RIGHT = 1 } hcsr04_sensor_index_t;

/* Left Sensor Pins */
#define GPIO_LEFT_TRIG (GPIO_NUM_18)
#define GPIO_LEFT_ECHO (GPIO_NUM_19)

/* Right Sensor Pins */
#define GPIO_RIGHT_TRIG (GPIO_NUM_4)
#define GPIO_RIGHT_ECHO (GPIO_NUM_23)

/* DHT22 Temperature/Humidity Sensor */
#define GPIO_DHT22_PIN (GPIO_NUM_5)

/* PCA9685 PWM Controller */
#define GPIO_PCA9685_I2C_SDA (GPIO_NUM_21)
#define GPIO_PCA9685_I2C_SCL (GPIO_NUM_22)
#define GPIO_PCA9685_OE (GPIO_NUM_15)
#define PCA9685_I2C_FREQUENCY (400000)
#define PCA9685_I2C_NUM (I2C_NUM_0)
#define PCA9685_PWM_FREQUENCY_kHZ (50)

/* ========== RGB LED Channel Mappings ========== */

/* Left LED Channels */
#define LED_LEFT_RED (0)
#define LED_LEFT_GREEN (1)
#define LED_LEFT_BLUE (2)

/* Right LED Channels */
#define LED_RIGHT_RED (3)
#define LED_RIGHT_GREEN (4)
#define LED_RIGHT_BLUE (5)

/* ========== Distance Sensing Parameters ========== */

/* LED Distance Range Configuration - EASILY CONFIGURABLE 
 * 
 * Quick Setup Examples:
 * ┌─────────────────┬──────────┬──────────┬─────────────────────────────┐
 * │ Use Case        │ MIN (cm) │ MAX (cm) │ Color Transitions           │
 * ├─────────────────┼──────────┼──────────┼─────────────────────────────┤
 * │ Desktop sensing │   0.0    │   40.0   │ Red→Yellow→Green→Cyan→Blue  │
 * │ Room monitoring │   10.0   │   200.0  │ Red→Yellow→Green→Cyan→Blue  │
 * │ Close proximity │   0.0    │   15.0   │ Red→Yellow→Green→Cyan→Blue  │
 * │ Wide area       │   50.0   │   400.0  │ Red→Yellow→Green→Cyan→Blue  │
 * └─────────────────┴──────────┴──────────┴─────────────────────────────┘
 * 
 * Just change these two values below for instant range adjustment!
 */
#define LED_COLOR_DISTANCE_MIN (0.0f)   /* Minimum distance for color mapping (cm) */
#define LED_COLOR_DISTANCE_MAX (40.0f)  /* Maximum distance for color mapping (cm) */

/* Advanced Range Control (optional fine-tuning) */
#define LED_COLOR_EXTENSION_FACTOR (0.7f) /* Extend min range by this factor for ultra-smooth transitions */
#define LED_COLOR_CLOSE_BOOST_THRESHOLD (0.25f) /* Fraction of range for brightness boost (0.0-1.0) */

/* ========== LED PWM Configuration ========== */

/* PWM scaling parameters (lower = brighter, higher = dimmer) */
/* Safe range without current limiting resistors: 5-75% PWM */
#define MIN_PWM_RED (5.0f)    /* Red LED minimum PWM (very dim) */
#define MIN_PWM_GREEN (5.0f)  /* Green LED minimum PWM (very dim) */
#define MIN_PWM_BLUE (5.0f)   /* Blue LED minimum PWM (very dim) */
#define MAX_PWM_ALL (75.0f)   /* Maximum PWM for all colors (safe without resistors) */

/* LED brightness threshold for "off" detection - much lower for smooth gradients */
#define LED_OFF_THRESHOLD (0.001f)  /* 0.001% threshold for nearly seamless transitions */

/* ========== Task Configuration ========== */

/* Task priorities (higher number = higher priority) */
#define TASK_PRIORITY_DHT22 (2)
#define TASK_PRIORITY_SENSORS (3)
#define TASK_PRIORITY_LEDS (1)
#define TASK_PRIORITY_WATCHDOG (4)

/* Task stack sizes (in words, not bytes) */
#define TASK_STACK_SIZE_DHT22 (2048)
#define TASK_STACK_SIZE_SENSORS (3072)
#define TASK_STACK_SIZE_LEDS (4096)  /* Increased for error handling and logging */
#define TASK_STACK_SIZE_WATCHDOG (3072)

/* Task timing intervals (in milliseconds) */
#define TASK_INTERVAL_DHT22 (30000)   /* Read temperature every 30 seconds */
#define TASK_INTERVAL_SENSORS (50)    /* Read distance sensors every 50ms */
#define TASK_INTERVAL_LEDS (50)       /* Update LEDs every 50ms */
#define TASK_INTERVAL_WATCHDOG (1000) /* Check system health every 1 second */

/* ========== System Health Parameters ========== */

/* Maximum consecutive sensor read failures before recovery action */
#define MAX_SENSOR_FAILURES (5)

/* Temperature sensor default fallback value */
#define DEFAULT_TEMPERATURE_C (25.0f)

/* Watchdog timeout for system restart (in milliseconds) */
#define SYSTEM_RESTART_DELAY_MS (5000)

/* Health monitoring thresholds */
#define SENSOR_FAILURE_RATE_DEGRADED (0.4f) /* 40% failure rate = degraded */
#define SENSOR_FAILURE_RATE_CRITICAL (0.7f) /* 70% failure rate = critical */

/* Task response timeout thresholds */
#define TASK_UNRESPONSIVE_THRESHOLD_MS (30000) /* 30 seconds */

/* System monitoring intervals */
#define HEALTH_CHECK_LOG_INTERVAL (60) /* Log health every 60 checks */
#define MAIN_LOOP_LOG_INTERVAL (100)   /* Log main status every 100 iterations */

/* ========== FreeRTOS Configuration ========== */

/* Queue sizes for inter-task communication */
#define SENSOR_DATA_QUEUE_SIZE (16)  // Increased from 4 to handle sensor burst
#define TEMPERATURE_QUEUE_SIZE (4)   // Increased from 2

/* Mutex timeouts (in milliseconds) */
#define MUTEX_TIMEOUT_MS (1000)

/* Notification timeouts (in milliseconds) */
#define NOTIFICATION_TIMEOUT_MS (5000)

/* ========== Bus Names ========== */

#define BUS_NAME_DHT22 "dht22_bus"
#define BUS_NAME_HCSR04 "hcsr04_bus"
#define BUS_NAME_PCA9685 "pca9685_bus"

/* ========== Logging Tags ========== */

#define TAG_MAIN "MAIN"
#define TAG_DHT22 "DHT22_TASK"
#define TAG_SENSORS "SENSORS_TASK"
#define TAG_LEDS "LEDS_TASK"
#define TAG_WATCHDOG "WATCHDOG_TASK"
#define TAG_LED_CTRL "LED_CTRL"

/* ========== Validation Macros ========== */

#define VALIDATE_SENSOR_INDEX(idx) ((idx) < NUM_HCSR04)
#define VALIDATE_DISTANCE(dist) ((dist) >= 0.0f && (dist) <= 400.0f)
#define VALIDATE_TEMPERATURE(temp) ((temp) >= -40.0f && (temp) <= 80.0f)
#define VALIDATE_PWM_PERCENT(pwm) ((pwm) >= 0.0f && (pwm) <= 100.0f)

#endif /* SYSTEM_CONFIG_H */