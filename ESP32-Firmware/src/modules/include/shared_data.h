#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdbool.h>
#include <time.h>

#include "../../include/system_config.h"

/*
 * Shared Data Structures
 * Defines thread-safe data structures for inter-task communication
 * Following FreeRTOS best practices for embedded systems
 */

/* ========== Sensor Data Types ========== */

typedef struct {
  uint8_t   sensor_index; /* Sensor identifier (left/right) */
  float     distance_cm;  /* Distance measurement in centimeters */
  esp_err_t read_result;  /* ESP error code from sensor read */
  uint32_t  timestamp_ms; /* Timestamp of measurement */
  bool      is_valid;     /* Data validity flag */
} sensor_data_t;

typedef struct {
  float     temperature_c;    /* Temperature in Celsius */
  float     humidity_percent; /* Relative humidity percentage */
  esp_err_t read_result;      /* ESP error code from sensor read */
  uint32_t  timestamp_ms;     /* Timestamp of measurement */
  bool      is_valid;         /* Data validity flag */
  bool      checksum_valid;   /* DHT22 checksum validation result */
} temperature_data_t;

/* ========== System State Types ========== */

typedef enum {
  SYSTEM_STATE_INITIALIZING,
  SYSTEM_STATE_RUNNING,
  SYSTEM_STATE_ERROR_RECOVERY,
  SYSTEM_STATE_FAULT
} system_state_t;

typedef struct {
  uint32_t       sensor_read_failures[NUM_HCSR04];
  uint32_t       temperature_read_failures;
  uint32_t       led_update_failures;
  uint32_t       total_sensor_reads;
  uint32_t       total_temperature_reads;
  uint32_t       uptime_ms;
  system_state_t current_state;
} system_health_t;

/* ========== Global Shared Data Context ========== */

typedef struct {
  /* FreeRTOS Communication Objects */
  QueueHandle_t sensor_data_queue;
  QueueHandle_t temperature_data_queue;

  /* Mutexes for shared resource protection */
  SemaphoreHandle_t health_mutex;
  SemaphoreHandle_t state_mutex;

  /* Task handles for system management */
  TaskHandle_t dht22_task_handle;
  TaskHandle_t sensors_task_handle;
  TaskHandle_t leds_task_handle;
  TaskHandle_t watchdog_task_handle;

  /* Shared system data */
  system_health_t health_data;
  system_state_t  system_state;

  /* Current temperature for sound speed correction */
  float current_temperature_c;
  bool  temperature_available;

  /* Latest sensor readings */
  sensor_data_t latest_sensors[NUM_HCSR04];

  /* System initialization flag */
  bool system_ready;

} shared_context_t;

/* ========== Global Context Access ========== */

/**
 * @brief Initialize shared data context
 * 
 * Creates all FreeRTOS objects and initializes shared data structures.
 * Must be called before any tasks are created.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t shared_data_init(void);

/**
 * @brief Deinitialize shared data context
 * 
 * Cleans up all FreeRTOS objects and resets shared data.
 * Should be called during system shutdown.
 */
void shared_data_deinit(void);

/**
 * @brief Get pointer to global shared context
 * 
 * @return shared_context_t* Pointer to global context, NULL if not initialized
 */
shared_context_t* shared_data_get_context(void);

/* ========== Thread-Safe Data Access Functions ========== */

/**
 * @brief Update system health statistics
 * 
 * @param sensor_index Sensor that had a failure (or NUM_HCSR04 for temperature)
 * @param success True if operation succeeded, false if failed
 */
void shared_data_update_health(uint8_t sensor_index, bool success);

/**
 * @brief Get current system state
 * 
 * @return system_state_t Current system state (thread-safe)
 */
system_state_t shared_data_get_system_state(void);

/**
 * @brief Set system state
 * 
 * @param state New system state to set
 */
void shared_data_set_system_state(system_state_t state);

/**
 * @brief Get current temperature for sound speed correction
 * 
 * @param temperature_c Pointer to store temperature value
 * @return true if valid temperature is available
 */
bool shared_data_get_temperature(float* temperature_c);

/**
 * @brief Update current temperature
 * 
 * @param temperature_c New temperature value
 * @param valid True if temperature reading is valid
 */
void shared_data_set_temperature(float temperature_c, bool valid);

/**
 * @brief Get latest sensor reading
 * 
 * @param sensor_index Sensor index (0 = left, 1 = right)
 * @param data Pointer to store sensor data
 * @return true if valid data is available
 */
bool shared_data_get_sensor_reading(uint8_t sensor_index, sensor_data_t* data);

/**
 * @brief Update sensor reading
 * 
 * @param sensor_index Sensor index
 * @param data New sensor data
 */
void shared_data_set_sensor_reading(uint8_t sensor_index, const sensor_data_t* data);

/* ========== Helper Functions ========== */

/**
 * @brief Create sensor data structure
 * 
 * @param sensor_index Sensor identifier
 * @param distance_cm Distance measurement
 * @param result ESP error code
 * @return sensor_data_t Initialized sensor data structure
 */
sensor_data_t
shared_data_create_sensor_data(uint8_t sensor_index, float distance_cm, esp_err_t result);

/**
 * @brief Create temperature data structure
 * 
 * @param temperature_c Temperature in Celsius
 * @param humidity_percent Humidity percentage
 * @param result ESP error code
 * @param checksum_valid DHT22 checksum validity
 * @return temperature_data_t Initialized temperature data structure
 */
temperature_data_t shared_data_create_temperature_data(float     temperature_c,
                                                       float     humidity_percent,
                                                       esp_err_t result,
                                                       bool      checksum_valid);

/**
 * @brief Get system uptime in milliseconds
 * 
 * @return uint32_t System uptime since initialization
 */
uint32_t shared_data_get_uptime_ms(void);

/**
 * @brief Check if system is ready for normal operation
 * 
 * @return true if all subsystems are initialized and ready
 */
bool shared_data_is_system_ready(void);

/**
 * @brief Mark system as ready for operation
 * 
 * Should be called after all hardware initialization is complete
 */
void shared_data_mark_system_ready(void);

#endif /* SHARED_DATA_H */