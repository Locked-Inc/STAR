/* lib/star_sensor_hcsr04/include/star_sensor_hcsr04.h */

#ifndef STAR_SENSOR_HCSR04_H
#define STAR_SENSOR_HCSR04_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <esp_err.h>
#include <star_bus_manager.h>
#include <star_error_interface.h>
#include <stdbool.h>
#include <stdint.h>

#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_hcsr04.h
 * @brief HC-SR04 ultrasonic distance sensor driver
 *
 * This driver provides an interface to the HC-SR04 ultrasonic distance sensor.
 * It uses ISR-based timing for accurate echo pulse measurement and supports
 * temperature compensation for improved accuracy.
 *
 * Key Features:
 * - Distance measurement range: 2-400 cm
 * - Temperature compensation for speed of sound
 * - ISR-based echo timing for accuracy
 * - Blocking and non-blocking measurement modes
 * - Thread-safe with mutex protection
 * - Timeout protection (prevents hanging)
 *
 * Hardware Connections:
 * - VCC: 5V (sensor needs 5V, but echo is 5V tolerant on most ESP32)
 * - GND: Ground
 * - TRIG: GPIO output (10us pulse triggers measurement)
 * - ECHO: GPIO input (pulse width proportional to distance)
 *
 * Example Usage:
 * @code
 * // === Basic Setup with Bus Manager ===
 *
 * #include "star_sensor_hcsr04.h"
 * #include "star_bus_manager.h"
 * #include "star_bus_config.h"
 *
 * // 1. Setup bus manager (see star_bus_manager.h for full setup)
 * star_bus_manager_t bus_manager;
 * star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 *
 * // 2. Create GPIO bus for the sensor pins
 * gpio_num_t hcsr04_pins[] = {GPIO_NUM_5, GPIO_NUM_18};  // Trigger, Echo
 * star_bus_config_t* gpio_bus = star_bus_config_create_gpio(
 *     "hcsr04_gpio", hcsr04_pins, 2);
 * star_bus_manager_add_bus(&bus_manager, gpio_bus);
 *
 * // 3. Initialize HC-SR04 sensor
 * hcsr04_handle_t sensor;
 * hcsr04_config_t config = {
 *     .trigger_pin = GPIO_NUM_5,
 *     .echo_pin = GPIO_NUM_18,
 *     .temperature_c = 25.0f  // Room temperature for speed of sound
 * };
 *
 * esp_err_t ret = star_sensor_hcsr04_init(
 *     &sensor,
 *     &bus_manager,
 *     "hcsr04_gpio",
 *     NULL,              // Use default error handler
 *     &config
 * );
 *
 * if (ret != ESP_OK) {
 *     ESP_LOGE(TAG, "Failed to init HC-SR04: %s", esp_err_to_name(ret));
 *     return;
 * }
 *
 *
 * // === Reading Distance (Blocking) ===
 *
 * float distance_cm;
 * ret = star_sensor_hcsr04_read_distance(&sensor, &distance_cm);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Distance: %.2f cm", distance_cm);
 * } else if (ret == HCSR04_ERR_TIMEOUT) {
 *     ESP_LOGW(TAG, "No echo received (object too far or not present)");
 * } else if (ret == HCSR04_ERR_OUT_OF_RANGE_MAX) {
 *     ESP_LOGW(TAG, "Object beyond 400cm range");
 * }
 *
 *
 * // === Non-Blocking Measurement ===
 *
 * // Trigger measurement
 * star_sensor_hcsr04_trigger(&sensor);
 *
 * // Do other work...
 * process_other_sensors();
 *
 * // Check if measurement complete
 * bool complete;
 * star_sensor_hcsr04_is_complete(&sensor, &complete);
 * if (complete) {
 *     star_sensor_hcsr04_get_result(&sensor, &distance_cm);
 *     ESP_LOGI(TAG, "Distance: %.2f cm", distance_cm);
 * }
 *
 *
 * // === Temperature Compensation ===
 *
 * // Update temperature from external sensor (e.g., DHT22)
 * star_dht22_data_t dht_data;
 * star_bus_dht22_read(&bus_manager, "dht22", &dht_data);
 * star_sensor_hcsr04_set_temperature(&sensor, dht_data.temperature_c);
 *
 * // Subsequent readings will use corrected speed of sound
 * star_sensor_hcsr04_read_distance(&sensor, &distance_cm);
 *
 *
 * // === Multiple Sensors ===
 *
 * hcsr04_handle_t left_sensor, right_sensor;
 *
 * hcsr04_config_t left_config = {
 *     .trigger_pin = GPIO_NUM_5, .echo_pin = GPIO_NUM_18, .temperature_c = 25.0f};
 * hcsr04_config_t right_config = {
 *     .trigger_pin = GPIO_NUM_19, .echo_pin = GPIO_NUM_21, .temperature_c = 25.0f};
 *
 * star_sensor_hcsr04_init(&left_sensor, &bus_manager, "gpio_bus", NULL, &left_config);
 * star_sensor_hcsr04_init(&right_sensor, &bus_manager, "gpio_bus", NULL, &right_config);
 *
 * // Read from each
 * float left_dist, right_dist;
 * star_sensor_hcsr04_read_distance(&left_sensor, &left_dist);
 * star_sensor_hcsr04_read_distance(&right_sensor, &right_dist);
 *
 *
 * // === Cleanup ===
 *
 * star_sensor_hcsr04_deinit(&sensor);
 * star_bus_manager_remove_bus(&bus_manager, "hcsr04_gpio");
 * @endcode
 */

#define HCSR04_MIN_DISTANCE_CM (2)
#define HCSR04_MAX_DISTANCE_CM (400)
#define HCSR04_SPEED_OF_SOUND_CM_US (0.0343f)
#define HCSR04_TIMEOUT_US (23200) // Max echo time for 400cm

typedef enum {
  HCSR04_OK = 0,
  HCSR04_ERR_TIMEOUT,
  HCSR04_ERR_OUT_OF_RANGE_MIN,
  HCSR04_ERR_OUT_OF_RANGE_MAX,
  HCSR04_ERR_INVALID_PULSE,
  HCSR04_ERR_NOT_READY,
} hcsr04_error_t;

typedef struct {
  gpio_num_t trigger_pin;
  gpio_num_t echo_pin;
  float      temperature_c; // For speed of sound correction
} hcsr04_config_t;

/**
 * @brief HC-SR04 device handle
 *
 * Maintains state for ultrasonic distance measurements.
 * Thread-safe: All operations are protected by an internal mutex.
 */
typedef struct hcsr04_handle {
  star_bus_manager_t*     manager;              /**< Pointer to bus manager */
  const char*             bus_name;             /**< Name of GPIO bus */
  gpio_num_t              trigger_pin;          /**< Trigger GPIO pin */
  gpio_num_t              echo_pin;             /**< Echo GPIO pin */
  float                   temperature_c;        /**< Temperature for compensation */
  star_error_interface_t* error_iface;          /**< Injected error interface (NULL = default) */
  SemaphoreHandle_t       mutex;                /**< Mutex for thread-safe operations */
  bool                    initialized;          /**< Initialization state */
  bool                    owns_error_handler;   /**< True if we created default error handler */
  volatile uint32_t       echo_start_time;      /**< ISR: Echo pulse start time (us) */
  volatile uint32_t       echo_end_time;        /**< ISR: Echo pulse end time (us) */
  volatile bool           measurement_complete; /**< ISR: Measurement done flag */
  volatile bool           timeout_occurred;     /**< ISR: Timeout flag */
} hcsr04_handle_t;

/**
 * @brief Initialize HC-SR04 sensor
 *
 * Configures GPIO pins through the GPIO bus manager and sets up ISR for echo timing.
 * Thread-safe: Creates an internal mutex for protecting handle state.
 *
 * @param[out] handle      Pointer to handle structure (must be allocated by caller)
 * @param[in]  manager     Pointer to initialized bus manager
 * @param[in]  bus_name    Name of GPIO bus that manages trigger and echo pins
 * @param[in]  error_iface Error interface for error handling (NULL = create default internally)
 * @param[in]  config      Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note The handle must be deinitialized with star_sensor_hcsr04_deinit() when done
 * @note If error_iface is NULL, a default error handler will be created internally
 * @note GPIO bus must already manage both trigger_pin and echo_pin
 * @note All operations after init are protected by mutex for thread safety
 */
esp_err_t star_sensor_hcsr04_init(hcsr04_handle_t*        handle,
                                  star_bus_manager_t*     manager,
                                  const char*             bus_name,
                                  star_error_interface_t* error_iface,
                                  const hcsr04_config_t*  config);

/**
 * @brief Deinitialize HC-SR04 sensor
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_deinit(hcsr04_handle_t* handle);

/**
 * @brief Trigger measurement and read distance
 *
 * @param[in]  handle      Pointer to initialized handle
 * @param[out] distance_cm Distance in centimeters
 *
 * @return ESP_OK on success, HCSR04_ERR_* on measurement error
 */
esp_err_t star_sensor_hcsr04_read_distance(hcsr04_handle_t* handle, float* distance_cm);

/**
 * @brief Trigger measurement (non-blocking)
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_trigger(hcsr04_handle_t* handle);

/**
 * @brief Check if measurement is complete
 *
 * @param[in]  handle   Pointer to initialized handle
 * @param[out] complete true if measurement done
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_is_complete(const hcsr04_handle_t* handle, bool* complete);

/**
 * @brief Get measurement result (non-blocking)
 *
 * @param[in]  handle      Pointer to initialized handle
 * @param[out] distance_cm Distance in centimeters
 *
 * @return ESP_OK on success, HCSR04_ERR_NOT_READY if not complete
 */
esp_err_t star_sensor_hcsr04_get_result(hcsr04_handle_t* handle, float* distance_cm);

/**
 * @brief Set temperature for compensation
 *
 * @param[in] handle        Pointer to initialized handle
 * @param[in] temperature_c Temperature in Celsius
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_set_temperature(hcsr04_handle_t* handle, float temperature_c);

/**
 * @brief Calculate distance with temperature compensation
 *
 * @param[in]  echo_time_us  Echo pulse width in microseconds
 * @param[in]  temperature_c Temperature in Celsius
 * @param[out] distance_cm   Distance in centimeters
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_hcsr04_calculate_distance(uint32_t echo_time_us,
                                                float    temperature_c,
                                                float*   distance_cm);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_HCSR04_H */
