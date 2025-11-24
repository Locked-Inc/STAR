/* lib/star_bus/include/star_bus_dht22_proprietary.h */

#ifndef STAR_BUS_DHT22_PROPRIETARY_H
#define STAR_BUS_DHT22_PROPRIETARY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <driver/gpio.h>
#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"

/**
 * @file star_bus_dht22_proprietary.h
 * @brief DHT22 (AM2302) proprietary single-wire protocol implementation
 *
 * This module implements the DHT22/AM2302 proprietary single-wire protocol
 * for communication with DHT22 temperature and humidity sensors. This protocol
 * is NOT compatible with Dallas 1-Wire protocol despite using a single data pin.
 *
 * Protocol Overview:
 * - Single bidirectional data line
 * - Host initiates communication with start signal
 * - Sensor responds with 40 bits of data
 * - Data format: 16-bit humidity + 16-bit temperature + 8-bit checksum
 *
 * Timing Specifications:
 * - Start signal: Host pulls low >= 1ms (18ms typical), then high 20-40us
 * - Response: Sensor pulls low 80us, then high 80us
 * - Bit 0: 50us low + 26-28us high
 * - Bit 1: 50us low + 70us high
 *
 * Sensor Specifications (DHT22/AM2302):
 * - Temperature range: -40 to +80C (accuracy +/- 0.5C)
 * - Humidity range: 0-100% RH (accuracy +/- 2-5% RH)
 * - Sampling period: minimum 2 seconds between readings
 * - Operating voltage: 3.3V - 5.5V
 *
 * Example Usage:
 * @code
 * // === Basic DHT22 Reading ===
 *
 * // 1. Create error handler and bus manager (DIP pattern)
 * error_handler_t error_handler;
 * error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);
 *
 * star_error_interface_t error_iface;
 * star_pin_interface_t pin_iface;
 * error_handler_get_interface(&error_iface, &error_handler);
 * pin_validator_get_interface(&pin_iface);
 *
 * star_bus_manager_t bus_manager;
 * star_bus_manager_init(&bus_manager, "main", &error_iface, &pin_iface);
 *
 * // 2. Initialize DHT22 bus
 * star_dht22_config_t config = STAR_DHT22_CONFIG_DEFAULT();
 * config.gpio_pin = GPIO_NUM_4;
 * star_bus_dht22_init(&bus_manager, "dht22_sensor", &config);
 *
 * // 3. Read sensor data
 * star_dht22_data_t data;
 * esp_err_t ret = star_bus_dht22_read(&bus_manager, "dht22_sensor", &data);
 * if (ret == ESP_OK && data.checksum_valid) {
 *     printf("Temperature: %.1f C (%.1f F)\n",
 *            data.temperature_c,
 *            star_bus_dht22_celsius_to_fahrenheit(data.temperature_c));
 *     printf("Humidity: %.1f %%\n", data.humidity_percent);
 *
 *     // Calculate derived values
 *     float heat_index = star_bus_dht22_calculate_heat_index(
 *         data.temperature_c, data.humidity_percent);
 *     float dew_point = star_bus_dht22_calculate_dew_point(
 *         data.temperature_c, data.humidity_percent);
 *     printf("Heat Index: %.1f C, Dew Point: %.1f C\n", heat_index, dew_point);
 * }
 *
 * // 4. Periodic reading (respects 2-second minimum interval)
 * while (running) {
 *     vTaskDelay(pdMS_TO_TICKS(2000));  // DHT22 requires 2s between reads
 *     ret = star_bus_dht22_read(&bus_manager, "dht22_sensor", &data);
 *     if (ret == ESP_OK) {
 *         // Process data...
 *     }
 * }
 *
 * // 5. Cleanup
 * star_bus_dht22_deinit(&bus_manager, "dht22_sensor");
 * star_bus_manager_deinit(&bus_manager);
 * error_handler_deinit(&error_handler);
 *
 *
 * // === Using DHT11 Sensor ===
 *
 * star_dht22_config_t dht11_config = STAR_DHT11_CONFIG_DEFAULT();
 * dht11_config.gpio_pin = GPIO_NUM_5;
 * star_bus_dht22_init(&bus_manager, "dht11_sensor", &dht11_config);
 *
 *
 * // === Multiple DHT22 Sensors ===
 *
 * star_dht22_config_t config1 = STAR_DHT22_CONFIG_DEFAULT();
 * config1.gpio_pin = GPIO_NUM_4;
 * star_bus_dht22_init(&bus_manager, "indoor_dht", &config1);
 *
 * star_dht22_config_t config2 = STAR_DHT22_CONFIG_DEFAULT();
 * config2.gpio_pin = GPIO_NUM_5;
 * star_bus_dht22_init(&bus_manager, "outdoor_dht", &config2);
 *
 * // Read from each sensor
 * star_dht22_data_t indoor_data, outdoor_data;
 * star_bus_dht22_read(&bus_manager, "indoor_dht", &indoor_data);
 * star_bus_dht22_read(&bus_manager, "outdoor_dht", &outdoor_data);
 *
 *
 * // === Statistics and Diagnostics ===
 *
 * star_dht22_stats_t stats;
 * star_bus_dht22_get_stats(&bus_manager, "dht22_sensor", &stats);
 * star_bus_dht22_print_stats("dht22_sensor", &stats);
 *
 * printf("Success rate: %.1f%%\n",
 *        (float)stats.successful_reads / stats.total_reads * 100.0f);
 * @endcode
 */

/* --- Constants --- */

/** Minimum interval between readings (ms) - DHT22 spec requires 2 seconds */
#define STAR_DHT22_MIN_INTERVAL_MS (2000)

/** Default start signal low time (ms) */
#define STAR_DHT22_START_LOW_MS (18)

/** Default timeout for read operations (ms) */
#define STAR_DHT22_READ_TIMEOUT_MS (100)

/** Maximum number of DHT22 buses */
#define STAR_DHT22_MAX_BUSES (8)

/* --- Types --- */

/* Note: star_dht22_model_t, star_dht22_callbacks_t, star_dht22_ops_t, and
 * star_dht22_bus_config_t are defined in star_bus_protocol_types.h */

/**
 * @brief DHT22 bus configuration (for user-facing initialization)
 */
typedef struct {
  gpio_num_t         gpio_pin;        /**< GPIO pin for data line */
  star_dht22_model_t model;           /**< Sensor model (DHT22, DHT11, etc.) */
  uint32_t           start_low_ms;    /**< Start signal low duration (ms) */
  uint32_t           read_timeout_ms; /**< Read timeout (ms) */
} star_dht22_config_t;

/**
 * @brief DHT22 sensor reading data
 */
typedef struct star_dht22_data {
  float    humidity_percent; /**< Relative humidity (0-100%) */
  float    temperature_c;    /**< Temperature in Celsius */
  uint16_t raw_humidity;     /**< Raw 16-bit humidity value */
  uint16_t raw_temperature;  /**< Raw 16-bit temperature value */
  uint8_t  checksum;         /**< Received checksum byte */
  bool     checksum_valid;   /**< True if checksum verification passed */
  int64_t  timestamp_us;     /**< Timestamp when reading was taken */
} star_dht22_data_t;

/**
 * @brief DHT22 bus statistics
 */
typedef struct {
  uint64_t total_reads;            /**< Total read attempts */
  uint64_t successful_reads;       /**< Successful reads */
  uint64_t failed_reads;           /**< Failed reads */
  uint64_t checksum_errors;        /**< Checksum verification failures */
  uint64_t timeout_errors;         /**< Timeout errors */
  uint64_t no_response_errors;     /**< No sensor response errors */
  uint32_t last_read_time_us;      /**< Last read operation time (us) */
  float    last_humidity;          /**< Last successful humidity reading */
  float    last_temperature;       /**< Last successful temperature reading */
  int64_t  last_read_timestamp_us; /**< Timestamp of last successful read */
} star_dht22_stats_t;

/* --- Bus Initialization --- */

/**
 * @brief Initialize DHT22 bus
 *
 * @param[in] manager  Pointer to initialized bus manager
 * @param[in] bus_name Name for this DHT22 bus instance
 * @param[in] config   DHT22 configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_dht22_init(star_bus_manager_t*        manager,
                              const char*                bus_name,
                              const star_dht22_config_t* config);

/**
 * @brief Deinitialize DHT22 bus
 *
 * @param[in] manager  Pointer to initialized bus manager
 * @param[in] bus_name Name of DHT22 bus to deinitialize
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_dht22_deinit(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Initialize default DHT22 operations function pointers
 *
 * @param[out] ops Pointer to operations structure to initialize
 */
void star_bus_dht22_init_default_ops(star_dht22_ops_t* ops);

/* --- Data Reading --- */

/**
 * @brief Read temperature and humidity from DHT22 sensor
 *
 * This function performs a complete read cycle:
 * 1. Sends start signal to sensor
 * 2. Waits for sensor response
 * 3. Reads 40 bits of data (humidity, temperature, checksum)
 * 4. Verifies checksum
 * 5. Converts raw data to physical values
 *
 * Note: DHT22 requires minimum 2 seconds between readings.
 * If called more frequently, it will return the cached data
 * from the previous successful read.
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of DHT22 bus
 * @param[out] data     Pointer to store sensor data
 *
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if sensor didn't respond,
 *         ESP_ERR_INVALID_CRC if checksum failed, error code otherwise
 */
esp_err_t
star_bus_dht22_read(star_bus_manager_t* manager, const char* bus_name, star_dht22_data_t* data);

/**
 * @brief Read temperature only (uses cached humidity)
 *
 * @param[in]  manager     Pointer to initialized bus manager
 * @param[in]  bus_name    Name of DHT22 bus
 * @param[out] temperature Pointer to store temperature in Celsius
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_dht22_read_temperature(star_bus_manager_t* manager,
                                          const char*         bus_name,
                                          float*              temperature);

/**
 * @brief Read humidity only (uses cached temperature)
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of DHT22 bus
 * @param[out] humidity Pointer to store humidity percentage
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bus_dht22_read_humidity(star_bus_manager_t* manager, const char* bus_name, float* humidity);

/* --- Sensor Control --- */

/**
 * @brief Force a fresh read from sensor
 *
 * Bypasses the 2-second minimum interval check and forces a new
 * read from the sensor. Use with caution as rapid reads may
 * heat up the sensor and affect accuracy.
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of DHT22 bus
 * @param[out] data     Pointer to store sensor data
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_dht22_force_read(star_bus_manager_t* manager,
                                    const char*         bus_name,
                                    star_dht22_data_t*  data);

/**
 * @brief Check if sensor is present and responding
 *
 * Sends a start signal and checks for sensor response without
 * completing a full read cycle.
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of DHT22 bus
 * @param[out] present True if sensor responded
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bus_dht22_check_presence(star_bus_manager_t* manager, const char* bus_name, bool* present);

/* --- Statistics --- */

/**
 * @brief Get DHT22 bus statistics
 *
 * @param[in]  manager  Pointer to initialized bus manager
 * @param[in]  bus_name Name of DHT22 bus
 * @param[out] stats    Pointer to store statistics
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_dht22_get_stats(const star_bus_manager_t* manager,
                                   const char*               bus_name,
                                   star_dht22_stats_t*       stats);

/**
 * @brief Reset DHT22 bus statistics
 *
 * @param[in] manager  Pointer to initialized bus manager
 * @param[in] bus_name Name of DHT22 bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_dht22_reset_stats(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Print DHT22 statistics
 *
 * @param[in] bus_name Name of DHT22 bus
 * @param[in] stats    Pointer to statistics
 */
void star_bus_dht22_print_stats(const char* bus_name, const star_dht22_stats_t* stats);

/* --- Utility Functions --- */

/**
 * @brief Convert raw temperature to Celsius
 *
 * @param[in] raw_value Raw 16-bit temperature value from sensor
 * @param[in] model     Sensor model (affects conversion formula)
 *
 * @return Temperature in Celsius
 */
float star_bus_dht22_raw_to_celsius(uint16_t raw_value, star_dht22_model_t model);

/**
 * @brief Convert raw humidity to percentage
 *
 * @param[in] raw_value Raw 16-bit humidity value from sensor
 * @param[in] model     Sensor model (affects conversion formula)
 *
 * @return Humidity percentage (0-100)
 */
float star_bus_dht22_raw_to_humidity(uint16_t raw_value, star_dht22_model_t model);

/**
 * @brief Calculate checksum from raw data
 *
 * @param[in] raw_humidity    Raw humidity bytes (high, low)
 * @param[in] raw_temperature Raw temperature bytes (high, low)
 *
 * @return Expected checksum value
 */
uint8_t star_bus_dht22_calculate_checksum(uint16_t raw_humidity, uint16_t raw_temperature);

/**
 * @brief Convert Celsius to Fahrenheit
 *
 * @param[in] celsius Temperature in Celsius
 *
 * @return Temperature in Fahrenheit
 */
static inline float star_bus_dht22_celsius_to_fahrenheit(float celsius)
{
  return (celsius * 9.0f / 5.0f) + 32.0f;
}

/**
 * @brief Calculate heat index (apparent temperature)
 *
 * Uses the Steadman formula for heat index calculation.
 *
 * @param[in] temperature Temperature in Celsius
 * @param[in] humidity    Relative humidity percentage
 *
 * @return Heat index in Celsius
 */
float star_bus_dht22_calculate_heat_index(float temperature, float humidity);

/**
 * @brief Calculate dew point
 *
 * Uses Magnus formula for dew point calculation.
 *
 * @param[in] temperature Temperature in Celsius
 * @param[in] humidity    Relative humidity percentage
 *
 * @return Dew point in Celsius
 */
float star_bus_dht22_calculate_dew_point(float temperature, float humidity);

/* --- Helper Macros --- */

/**
 * @brief Create default DHT22 configuration
 */
#define STAR_DHT22_CONFIG_DEFAULT()                                                                \
  {                                                                                                \
    .gpio_pin        = GPIO_NUM_NC,                                                                \
    .model           = k_star_dht22_model_dht22,                                                   \
    .start_low_ms    = STAR_DHT22_START_LOW_MS,                                                    \
    .read_timeout_ms = STAR_DHT22_READ_TIMEOUT_MS,                                                 \
  }

/**
 * @brief Create DHT11 configuration (for DHT11 sensors)
 */
#define STAR_DHT11_CONFIG_DEFAULT()                                                                \
  {                                                                                                \
    .gpio_pin        = GPIO_NUM_NC,                                                                \
    .model           = k_star_dht22_model_dht11,                                                   \
    .start_low_ms    = 20,                                                                         \
    .read_timeout_ms = STAR_DHT22_READ_TIMEOUT_MS,                                                 \
  }

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_DHT22_PROPRIETARY_H */
