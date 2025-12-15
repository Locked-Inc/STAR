/* lib/star_bus/include/star_bus_stats.h */

#ifndef STAR_BUS_STATS_H
#define STAR_BUS_STATS_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_stats.h
 * @brief Bus statistics and performance monitoring
 *
 * This module provides comprehensive statistics and monitoring capabilities
 * for all bus operations, enabling performance analysis, debugging, and
 * system health monitoring.
 *
 * Features:
 * - Per-bus operation counters (read, write, errors)
 * - Byte transfer tracking
 * - Timing statistics (min, max, average)
 * - Error rate monitoring
 * - Bus utilization metrics
 * - Reset and snapshot capabilities
 *
 * Use Cases:
 * - Performance optimization and benchmarking
 * - System health monitoring
 * - Debugging communication issues
 * - Quality assurance testing
 * - Resource usage analysis
 *
 * Example Usage:
 * @code
 * // === Enable Statistics Collection ===
 *
 * #include "star_bus_stats.h"
 * #include "star_bus_manager.h"
 *
 * // Enable stats with default config
 * star_stats_config_t stats_cfg = STAR_STATS_CONFIG_DEFAULT();
 * esp_err_t ret = star_bus_stats_enable(&bus_manager, "imu_i2c", &stats_cfg);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Statistics collection enabled");
 * }
 *
 * // Enable with minimal config (counters only, less overhead)
 * star_stats_config_t minimal_cfg = STAR_STATS_CONFIG_MINIMAL();
 * star_bus_stats_enable(&bus_manager, "sensor_i2c", &minimal_cfg);
 *
 *
 * // === Get Current Statistics ===
 *
 * star_bus_stats_t stats;
 * ret = star_bus_stats_get(&bus_manager, "imu_i2c", &stats);
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Total operations: %llu", stats.total_operations);
 *     ESP_LOGI(TAG, "Reads: %llu, Writes: %llu", stats.read_operations, stats.write_operations);
 *     ESP_LOGI(TAG, "Bytes read: %llu, written: %llu", stats.bytes_read, stats.bytes_written);
 *     ESP_LOGI(TAG, "Failed: %llu", stats.failed_operations);
 *     ESP_LOGI(TAG, "Timing (us): min=%lu, max=%lu, avg=%lu",
 *              stats.min_operation_time_us, stats.max_operation_time_us, stats.avg_operation_time_us);
 * }
 *
 *
 * // === Analyze Statistics ===
 *
 * // Calculate error rate
 * float error_rate = star_bus_stats_error_rate(&stats);
 * ESP_LOGI(TAG, "Error rate: %.2f%%", error_rate);
 *
 * // Calculate bus utilization
 * float utilization = star_bus_stats_utilization(&stats);
 * ESP_LOGI(TAG, "Bus utilization: %.2f%%", utilization);
 *
 * // Calculate throughput
 * uint32_t throughput = star_bus_stats_throughput(&stats);
 * ESP_LOGI(TAG, "Throughput: %lu bytes/sec", throughput);
 *
 * // Calculate operations per second
 * uint32_t ops_sec = star_bus_stats_ops_per_second(&stats);
 * ESP_LOGI(TAG, "Operations: %lu ops/sec", ops_sec);
 *
 *
 * // === Snapshot for Differential Analysis ===
 *
 * // Take snapshot before test
 * star_stats_snapshot_t snapshot;
 * star_bus_stats_snapshot(&bus_manager, "imu_i2c", &snapshot);
 *
 * // Run some operations...
 * for (int i = 0; i < 1000; i++) {
 *     star_bus_i2c_read(&bus_manager, "imu_i2c", buffer, 6, 0x3B, NULL);
 * }
 *
 * // Calculate difference
 * star_bus_stats_t diff;
 * star_bus_stats_diff(&bus_manager, "imu_i2c", &snapshot, &diff);
 *
 * ESP_LOGI(TAG, "Test results: %llu ops, %llu bytes, avg %lu us",
 *          diff.total_operations, diff.bytes_read, diff.avg_operation_time_us);
 *
 *
 * // === Reset Statistics ===
 *
 * // Reset single bus
 * star_bus_stats_reset(&bus_manager, "imu_i2c");
 *
 * // Reset all buses
 * star_bus_stats_reset_all(&bus_manager);
 *
 *
 * // === Print Statistics ===
 *
 * // Print stats for one bus
 * star_bus_stats_get(&bus_manager, "imu_i2c", &stats);
 * star_bus_stats_print("imu_i2c", &stats);
 *
 * // Print stats for all buses
 * star_bus_stats_print_all(&bus_manager);
 *
 *
 * // === Export as JSON ===
 *
 * char json_buffer[512];
 * ret = star_bus_stats_to_json(&stats, json_buffer, sizeof(json_buffer));
 * if (ret == ESP_OK) {
 *     ESP_LOGI(TAG, "Stats JSON: %s", json_buffer);
 *     // Send to server, save to file, etc.
 * }
 *
 *
 * // === Periodic Monitoring Task ===
 *
 * void stats_monitor_task(void* arg) {
 *     star_bus_manager_t* manager = (star_bus_manager_t*)arg;
 *
 *     while (1) {
 *         star_bus_stats_t stats;
 *         if (star_bus_stats_get(manager, "imu_i2c", &stats) == ESP_OK) {
 *             float error_rate = star_bus_stats_error_rate(&stats);
 *             if (error_rate > 5.0f) {
 *                 ESP_LOGW(TAG, "High error rate: %.2f%%", error_rate);
 *             }
 *         }
 *         vTaskDelay(pdMS_TO_TICKS(10000));  // Check every 10 seconds
 *     }
 * }
 *
 *
 * // === Check if Statistics Enabled ===
 *
 * bool enabled;
 * star_bus_stats_is_enabled(&bus_manager, "imu_i2c", &enabled);
 * if (enabled) {
 *     ESP_LOGI(TAG, "Stats collection is active");
 * }
 *
 *
 * // === Disable Statistics ===
 *
 * star_bus_stats_disable(&bus_manager, "imu_i2c");
 * @endcode
 */

/* --- Types --- */

/**
 * @brief Bus operation statistics
 */
typedef struct {
  /* Operation Counters */
  uint64_t total_operations;  /**< Total operations performed */
  uint64_t read_operations;   /**< Number of read operations */
  uint64_t write_operations;  /**< Number of write operations */
  uint64_t failed_operations; /**< Number of failed operations */

  /* Data Transfer */
  uint64_t bytes_read;    /**< Total bytes read */
  uint64_t bytes_written; /**< Total bytes written */

  /* Timing Statistics (microseconds) */
  uint32_t min_operation_time_us; /**< Minimum operation time */
  uint32_t max_operation_time_us; /**< Maximum operation time */
  uint32_t avg_operation_time_us; /**< Average operation time */

  /* Error Statistics */
  uint32_t timeout_errors; /**< Number of timeout errors */
  uint32_t nack_errors;    /**< Number of NACK errors (I2C) */
  uint32_t bus_errors;     /**< Number of bus errors */
  uint32_t other_errors;   /**< Number of other errors */

  /* Bus Utilization */
  uint32_t total_bus_time_ms; /**< Total time bus was active (ms) */
  uint32_t idle_time_ms;      /**< Total time bus was idle (ms) */

  /* Last Operation */
  esp_err_t last_error;             /**< Last error code */
  uint32_t  last_operation_time_us; /**< Last operation time */

  /* Timestamps */
  uint32_t stats_start_time_ms; /**< When stats collection started */
  uint32_t last_reset_time_ms;  /**< When stats were last reset */
} star_bus_stats_t;

/**
 * @brief Statistics collection configuration
 */
typedef struct {
  bool collect_timing;         /**< Collect timing statistics */
  bool collect_errors;         /**< Collect error statistics */
  bool collect_utilization;    /**< Collect bus utilization */
  bool auto_reset_on_overflow; /**< Auto reset counters on overflow */
} star_stats_config_t;

/**
 * @brief Statistics snapshot (for differential analysis)
 */
typedef struct {
  star_bus_stats_t stats;        /**< Statistics at snapshot time */
  uint32_t         timestamp_ms; /**< Snapshot timestamp */
  char             bus_name[32]; /**< Bus name */
} star_stats_snapshot_t;

/* --- Statistics Management --- */

/**
 * @brief Enable statistics collection for a bus
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of bus
 * @param[in] config Statistics configuration (NULL for defaults)
 *
 * @return ESP_OK on success, error code otherwise
 *
 * @note Statistics collection has minimal overhead but uses memory
 */
esp_err_t star_bus_stats_enable(star_bus_manager_t*        manager,
                                const char*                bus_name,
                                const star_stats_config_t* config);

/**
 * @brief Disable statistics collection for a bus
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_stats_disable(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Check if statistics collection is enabled for a bus
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of bus
 * @param[out] enabled Pointer to store enabled status
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bus_stats_is_enabled(const star_bus_manager_t* manager, const char* bus_name, bool* enabled);

/* --- Statistics Retrieval --- */

/**
 * @brief Get current statistics for a bus
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of bus
 * @param[out] stats Pointer to store statistics
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_stats_get(const star_bus_manager_t* manager,
                             const char*               bus_name,
                             star_bus_stats_t*         stats);

/**
 * @brief Reset statistics for a bus
 *
 * Clears all counters and resets timing statistics.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_stats_reset(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Reset statistics for all buses
 *
 * @param[in] manager Pointer to initialized bus manager
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_stats_reset_all(star_bus_manager_t* manager);

/* --- Snapshot Management --- */

/**
 * @brief Create a statistics snapshot
 *
 * Captures current statistics for differential analysis.
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of bus
 * @param[out] snapshot Pointer to store snapshot
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_stats_snapshot(const star_bus_manager_t* manager,
                                  const char*               bus_name,
                                  star_stats_snapshot_t*    snapshot);

/**
 * @brief Calculate difference between current stats and snapshot
 *
 * Useful for measuring performance over a specific time period.
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of bus
 * @param[in]  snapshot Previous snapshot
 * @param[out] diff Pointer to store difference
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_stats_diff(const star_bus_manager_t*    manager,
                              const char*                  bus_name,
                              const star_stats_snapshot_t* snapshot,
                              star_bus_stats_t*            diff);

/* --- Analysis Functions --- */

/**
 * @brief Calculate error rate (percentage)
 *
 * @param[in] stats Pointer to statistics
 *
 * @return Error rate as percentage (0.0 - 100.0)
 */
float star_bus_stats_error_rate(const star_bus_stats_t* stats);

/**
 * @brief Calculate bus utilization (percentage)
 *
 * Ratio of active time to total time.
 *
 * @param[in] stats Pointer to statistics
 *
 * @return Bus utilization as percentage (0.0 - 100.0)
 */
float star_bus_stats_utilization(const star_bus_stats_t* stats);

/**
 * @brief Calculate average throughput (bytes per second)
 *
 * @param[in] stats Pointer to statistics
 *
 * @return Throughput in bytes per second
 */
uint32_t star_bus_stats_throughput(const star_bus_stats_t* stats);

/**
 * @brief Calculate operations per second
 *
 * @param[in] stats Pointer to statistics
 *
 * @return Operations per second
 */
uint32_t star_bus_stats_ops_per_second(const star_bus_stats_t* stats);

/* --- Reporting --- */

/**
 * @brief Print statistics to console
 *
 * Formats and prints statistics in human-readable format.
 *
 * @param[in] bus_name Name of bus
 * @param[in] stats Pointer to statistics
 */
void star_bus_stats_print(const char* bus_name, const star_bus_stats_t* stats);

/**
 * @brief Print statistics for all buses
 *
 * @param[in] manager Pointer to initialized bus manager
 */
void star_bus_stats_print_all(const star_bus_manager_t* manager);

/**
 * @brief Format statistics as JSON string
 *
 * @param[in]  stats Pointer to statistics
 * @param[out] buffer Buffer to store JSON string
 * @param[in]  buffer_size Size of buffer
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_stats_to_json(const star_bus_stats_t* stats, char* buffer, size_t buffer_size);

/* --- Helper Macros --- */

/**
 * @brief Create default statistics configuration
 */
#define STAR_STATS_CONFIG_DEFAULT()                                                                \
  {                                                                                                \
    .collect_timing         = true,                                                                \
    .collect_errors         = true,                                                                \
    .collect_utilization    = true,                                                                \
    .auto_reset_on_overflow = false,                                                               \
  }

/**
 * @brief Create minimal statistics configuration (counters only)
 */
#define STAR_STATS_CONFIG_MINIMAL()                                                                \
  {                                                                                                \
    .collect_timing         = false,                                                               \
    .collect_errors         = true,                                                                \
    .collect_utilization    = false,                                                               \
    .auto_reset_on_overflow = false,                                                               \
  }

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_STATS_H */
