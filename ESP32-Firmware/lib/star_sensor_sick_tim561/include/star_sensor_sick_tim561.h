/* lib/star_sensor_sick_tim561/include/star_sensor_sick_tim561.h */

#ifndef STAR_SENSOR_SICK_TIM561_H
#define STAR_SENSOR_SICK_TIM561_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_sick_tim561.h
 * @brief SICK TiM561-2050101 2D LiDAR scanner driver
 *
 * Features:
 * - 270° scanning angle
 * - 10m detection range (white 90% @ 10m)
 * - 15 Hz scan frequency
 * - 2880 measurement points per scan (0.094° angular resolution)
 * - TCP/IP over Ethernet
 * - COLA ASCII/Binary protocol
 *
 * Protocol: SICK COLA (Command Language) over TCP
 * Default IP: 192.168.1.1
 * Port: 2111 (COLA-A), 2112 (COLA-B)
 */

#define TIM561_DEFAULT_IP "192.168.1.1"
#define TIM561_DEFAULT_PORT (2111)
#define TIM561_MAX_SCAN_POINTS (2880)
#define TIM561_MAX_RESPONSE_LEN (16384)  // 16KB for scan data

typedef struct {
  uint16_t distance_mm;  // Distance in millimeters
  uint8_t  rssi;         // Signal strength (0-255)
} tim561_scan_point_t;

typedef struct {
  uint32_t timestamp_ms;                            // Scan timestamp
  uint16_t scan_counter;                            // Incremental scan counter
  uint16_t telegram_counter;                        // Telegram counter
  uint16_t point_count;                             // Number of valid points
  float    start_angle_deg;                         // Start angle in degrees
  float    angular_resolution_deg;                  // Angular resolution
  tim561_scan_point_t points[TIM561_MAX_SCAN_POINTS]; // Scan points
} tim561_scan_data_t;

typedef struct {
  char    device_name[32];
  char    version[16];
  uint32_t serial_number;
} tim561_device_info_t;

typedef struct {
  char*  host;  // IP address or hostname
  uint16_t port;  // TCP port (typically 2111)
} tim561_config_t;

typedef struct tim561_handle {
  char*           host;
  uint16_t        port;
  int             socket_fd;
  error_handler_t error_handler;
  bool            initialized;
  bool            scanning;
  char*           response_buffer;
  size_t          response_capacity;
} tim561_handle_t;

/**
 * @brief Initialize SICK TiM561 scanner
 *
 * @param[out] handle Pointer to handle structure
 * @param[in]  config Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_tim561_init(tim561_handle_t* handle, const tim561_config_t* config);

/**
 * @brief Deinitialize SICK TiM561 scanner
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_tim561_deinit(tim561_handle_t* handle);

/**
 * @brief Connect to scanner
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_tim561_connect(tim561_handle_t* handle);

/**
 * @brief Disconnect from scanner
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_tim561_disconnect(tim561_handle_t* handle);

/**
 * @brief Get device information
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] info   Device information
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_tim561_get_device_info(tim561_handle_t* handle, tim561_device_info_t* info);

/**
 * @brief Start continuous scanning
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_tim561_start_scan(tim561_handle_t* handle);

/**
 * @brief Stop continuous scanning
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_tim561_stop_scan(tim561_handle_t* handle);

/**
 * @brief Read one scan frame
 *
 * @param[in]  handle    Pointer to initialized handle
 * @param[out] scan_data Scan data structure
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_tim561_read_scan(tim561_handle_t* handle, tim561_scan_data_t* scan_data);

/**
 * @brief Send COLA command and receive response
 *
 * @param[in]  handle       Pointer to initialized handle
 * @param[in]  command      COLA command string
 * @param[out] response     Response buffer
 * @param[in]  response_len Response buffer size
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_tim561_send_command(tim561_handle_t* handle,
                                           const char*      command,
                                           char*            response,
                                           size_t           response_len);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_SICK_TIM561_H */
