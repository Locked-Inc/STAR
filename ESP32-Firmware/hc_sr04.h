/*
 * HC-SR04 Ultrasonic Sensor Driver for ESP32
 * ============================================
 *
 * Comprehensive C driver with:
 * - Accurate timing measurements
 * - Temperature compensation
 * - Edge case handling
 * - Security (TOCTOU prevention, race condition safety)
 * - Memory safety (overflow protection, atomicity)
 *
 * Author: Embedded Systems Reference
 * License: MIT
 */

#ifndef HC_SR04_H
#define HC_SR04_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/atomic.h>
#include <driver/gpio.h>
#include <esp_timer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================= */
/* Configuration Constants       */
/* ============================= */

/* Timing specifications (microseconds) */
#define HC_SR04_TRIGGER_WIDTH_US        10      /* 10 μs trigger pulse */
#define HC_SR04_TRIGGER_SETUP_US        5       /* Setup time before pulse */
#define HC_SR04_TRIGGER_CLEANUP_US      5       /* Cleanup time after pulse */
#define HC_SR04_ECHO_TIMEOUT_US         30000   /* 30 ms timeout (5.1 m range) */
#define HC_SR04_MIN_ECHO_WIDTH_US       116     /* ~2 cm minimum distance */
#define HC_SR04_MAX_ECHO_WIDTH_US       23200   /* ~400 cm maximum distance */
#define HC_SR04_MIN_MEASUREMENT_CYCLE_MS 20     /* Minimum 20 ms between measurements */
#define HC_SR04_RECOMMENDED_CYCLE_MS    60      /* Recommended 60+ ms between measurements */

/* Measurement specifications */
#define HC_SR04_MIN_RANGE_CM            2       /* 2 cm blind zone */
#define HC_SR04_MAX_RANGE_CM            400     /* 400 cm maximum range */
#define HC_SR04_ACCURACY_MM             3       /* ±3 mm typical accuracy */
#define HC_SR04_OPERATING_FREQUENCY_KHZ 40      /* 40 kHz ultrasonic */

/* Temperature compensation */
#define HC_SR04_REFERENCE_TEMP_C        20      /* Reference temperature (Celsius) */
#define HC_SR04_REFERENCE_SPEED_M_S     343.2f  /* Speed of sound at 20°C */

/* Distance calculation constants */
#define HC_SR04_DISTANCE_DIVISOR_CM     58      /* Echo(μs) / 58 = distance(cm) */
#define HC_SR04_DISTANCE_DIVISOR_INCH   148     /* Echo(μs) / 148 = distance(inch) */
#define HC_SR04_SPEED_M_US              0.34320f /* Speed of sound in cm/μs at 20°C */

/* Filtering defaults */
#define HC_SR04_DEFAULT_FILTER_SIZE     5       /* Median filter window */
#define HC_SR04_JITTER_CM               4       /* Expected jitter indoors */

/* ============================= */
/* Type Definitions              */
/* ============================= */

/**
 * @brief Measurement status/error codes
 */
typedef enum {
    HC_SR04_OK = 0,                         /* Measurement successful */
    HC_SR04_ERR_TIMEOUT = -1,               /* Echo timeout - no pulse received */
    HC_SR04_ERR_OUT_OF_RANGE_MIN = -2,      /* Measurement below minimum range */
    HC_SR04_ERR_OUT_OF_RANGE_MAX = -3,      /* Measurement above maximum range */
    HC_SR04_ERR_INVALID_PULSE = -4,         /* Pulse too short (noise) */
    HC_SR04_ERR_IMPLAUSIBLE_CHANGE = -5,    /* Distance changed too rapidly */
    HC_SR04_ERR_NOT_INITIALIZED = -6,       /* Sensor not initialized */
    HC_SR04_ERR_INVALID_PARAM = -7,         /* Invalid parameter */
    HC_SR04_ERR_GPIO_CONFIG = -8,           /* GPIO configuration failed */
} hc_sr04_status_t;

/**
 * @brief Measurement result structure (thread-safe)
 */
typedef struct {
    uint32_t distance_cm;                   /* Distance in centimeters */
    uint32_t echo_width_us;                 /* Raw echo pulse width (microseconds) */
    int32_t temperature_c;                  /* Temperature used for compensation (°C) */
    uint32_t timestamp_us;                  /* Measurement timestamp */
    bool temperature_compensated;           /* True if temperature correction applied */
} hc_sr04_measurement_t;

/**
 * @brief Measurement statistics for validation
 */
typedef struct {
    uint32_t min_distance_cm;               /* Minimum distance in series */
    uint32_t max_distance_cm;               /* Maximum distance in series */
    uint32_t avg_distance_cm;               /* Average distance */
    uint32_t median_distance_cm;            /* Median distance (noise robust) */
    uint32_t measurement_count;             /* Number of measurements */
} hc_sr04_stats_t;

/**
 * @brief Temperature compensation structure
 */
typedef struct {
    bool enabled;                           /* Enable temperature compensation */
    int32_t ambient_temperature_c;          /* Current ambient temperature */
    float speed_of_sound_m_s;               /* Calculated speed at ambient temp */
} hc_sr04_temp_config_t;

/**
 * @brief Sensor configuration structure
 */
typedef struct {
    gpio_num_t trigger_pin;                 /* GPIO pin for trigger signal */
    gpio_num_t echo_pin;                    /* GPIO pin for echo signal */
    uint32_t echo_timeout_us;               /* Echo measurement timeout */
    bool use_interrupts;                    /* Use interrupt-driven measurement */
    hc_sr04_temp_config_t temperature;      /* Temperature compensation config */
    uint32_t measurement_cycle_ms;          /* Minimum time between measurements */
} hc_sr04_config_t;

/**
 * @brief Sensor state structure (opaque to user)
 */
typedef struct {
    hc_sr04_config_t config;                /* Configuration copy */
    volatile uint32_t echo_width_us;        /* Latest echo width (atomic access) */
    volatile uint32_t timestamp_us;         /* Measurement timestamp */
    volatile bool measurement_valid;        /* Measurement validity flag */
    uint32_t last_measurement_ms;           /* Time of last measurement */
    uint32_t measurement_count;             /* Total measurements taken */
    int32_t last_distance_cm;               /* Previous distance (for sanity checks) */
    bool initialized;                       /* Initialization flag */
} hc_sr04_sensor_t;

/* ============================= */
/* Function Prototypes           */
/* ============================= */

/**
 * @brief Initialize HC-SR04 sensor with specified configuration
 *
 * @param[out] sensor       Pointer to sensor state structure
 * @param[in]  config       Sensor configuration
 * @return                  HC_SR04_OK on success, error code otherwise
 *
 * @note Must be called before any measurements
 * @note Configures GPIO pins and interrupt handlers if enabled
 */
hc_sr04_status_t hc_sr04_init(hc_sr04_sensor_t *sensor,
                              const hc_sr04_config_t *config);

/**
 * @brief Deinitialize sensor and release resources
 *
 * @param[in] sensor        Pointer to sensor state structure
 * @return                  HC_SR04_OK on success, error code otherwise
 */
hc_sr04_status_t hc_sr04_deinit(hc_sr04_sensor_t *sensor);

/**
 * @brief Trigger measurement and wait for echo (blocking)
 *
 * This function is NOT interrupt-safe and should be called from
 * the same task that initialized the sensor.
 *
 * @param[in]  sensor       Pointer to sensor state structure
 * @param[out] measurement  Pointer to measurement result structure
 * @return                  HC_SR04_OK on success, error code otherwise
 *
 * @note Blocking operation - duration depends on distance (30 ms max)
 * @note Safe against TOCTOU: atomically captures echo width
 * @note Safe against GPIO races: uses ISR-safe edge detection
 */
hc_sr04_status_t hc_sr04_measure(hc_sr04_sensor_t *sensor,
                                 hc_sr04_measurement_t *measurement);

/**
 * @brief Non-blocking trigger pulse generation
 *
 * Sends trigger pulse and returns immediately. Result will be available
 * when measurement_ready() returns true.
 *
 * @param[in] sensor        Pointer to sensor state structure
 * @return                  HC_SR04_OK on success, error code otherwise
 */
hc_sr04_status_t hc_sr04_trigger_async(hc_sr04_sensor_t *sensor);

/**
 * @brief Check if async measurement is ready
 *
 * @param[in] sensor        Pointer to sensor state structure
 * @return                  true if measurement complete and valid
 */
bool hc_sr04_measurement_ready(hc_sr04_sensor_t *sensor);

/**
 * @brief Get last completed measurement result (non-blocking)
 *
 * Safe to call from multiple tasks.
 *
 * @param[in]  sensor       Pointer to sensor state structure
 * @param[out] measurement  Pointer to measurement result structure
 * @return                  HC_SR04_OK on success, error code otherwise
 */
hc_sr04_status_t hc_sr04_get_last_measurement(hc_sr04_sensor_t *sensor,
                                              hc_sr04_measurement_t *measurement);

/**
 * @brief Apply temperature compensation to measurement
 *
 * Adjusts distance calculation based on ambient temperature.
 * Required for high-accuracy applications.
 *
 * @param[in,out] measurement   Measurement to compensate
 * @param[in]     temperature_c Ambient temperature in Celsius
 * @return                      HC_SR04_OK on success, error code otherwise
 */
hc_sr04_status_t hc_sr04_apply_temp_compensation(hc_sr04_measurement_t *measurement,
                                                 int32_t temperature_c);

/**
 * @brief Calculate distance from raw echo pulse width
 *
 * @param[in] echo_width_us     Echo pulse width in microseconds
 * @param[in] temperature_c     Ambient temperature for compensation (optional)
 * @param[out] distance_cm      Calculated distance in centimeters
 * @return                      HC_SR04_OK on success, error code otherwise
 *
 * @note If temperature_c < -100, uses default 20°C reference
 */
hc_sr04_status_t hc_sr04_echo_to_distance(uint32_t echo_width_us,
                                          int32_t temperature_c,
                                          uint32_t *distance_cm);

/**
 * @brief Perform median filtering on multiple measurements
 *
 * Useful for rejecting outliers and noise.
 *
 * @param[in]  measurements    Array of measurements
 * @param[in]  count           Number of measurements
 * @param[out] result          Filtered measurement result
 * @return                     HC_SR04_OK on success, error code otherwise
 */
hc_sr04_status_t hc_sr04_filter_measurements(const uint32_t *distances_cm,
                                             uint32_t count,
                                             uint32_t *filtered_distance_cm);

/**
 * @brief Calculate measurement statistics
 *
 * @param[in]  measurements    Array of measurements
 * @param[in]  count           Number of measurements
 * @param[out] stats           Statistics structure to fill
 * @return                     HC_SR04_OK on success, error code otherwise
 */
hc_sr04_status_t hc_sr04_calculate_stats(const uint32_t *distances_cm,
                                         uint32_t count,
                                         hc_sr04_stats_t *stats);

/**
 * @brief Get human-readable error message
 *
 * @param[in] status           Error/status code
 * @return                     Pointer to error string (static)
 */
const char* hc_sr04_strerror(hc_sr04_status_t status);

/* ============================= */
/* Inline Utility Functions      */
/* ============================= */

/**
 * @brief Convert measurement result to string for logging
 *
 * Includes distance, timestamp, and temperature compensation status.
 *
 * @param[in]  measurement    Measurement to format
 * @param[out] buffer         Output buffer (min 128 bytes recommended)
 * @param[in]  buffer_len     Size of output buffer
 * @return                    Number of bytes written (not including null terminator)
 */
int hc_sr04_format_measurement(const hc_sr04_measurement_t *measurement,
                               char *buffer, size_t buffer_len);

/**
 * @brief Validate measurement result for plausibility
 *
 * Checks:
 * - Distance within valid range
 * - Echo width within expected bounds
 * - Reasonable change from previous measurement
 *
 * @param[in] measurement    Measurement to validate
 * @param[in] last_distance_cm  Previous distance (optional, -1 to skip)
 * @return                    true if measurement appears valid
 */
bool hc_sr04_measurement_valid(const hc_sr04_measurement_t *measurement,
                               int32_t last_distance_cm);

/* ============================= */
/* Diagnostic/Debug Functions    */
/* ============================= */

/**
 * @brief Enable detailed debug logging (compile-time configurable)
 *
 * When enabled, logs trigger timing, echo width, and error conditions.
 * Can be disabled in production for performance.
 */
#define HC_SR04_DEBUG_ENABLED 1

#if HC_SR04_DEBUG_ENABLED
void hc_sr04_debug_print_config(const hc_sr04_config_t *config);
void hc_sr04_debug_print_measurement(const hc_sr04_measurement_t *measurement);
#else
#define hc_sr04_debug_print_config(x)
#define hc_sr04_debug_print_measurement(x)
#endif

#ifdef __cplusplus
}
#endif

#endif /* HC_SR04_H */
