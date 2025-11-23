/* lib/star_sensor_rplidar_c1/include/star_sensor_rplidar_c1.h */

#ifndef STAR_SENSOR_RPLIDAR_C1_H
#define STAR_SENSOR_RPLIDAR_C1_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_rplidar_c1.h
 * @brief RPLiDAR C1 360° laser range scanner driver
 *
 * This driver provides interface to the RPLiDAR C1 DTOF LiDAR.
 * Features:
 * - 360° scanning range
 * - 12m max distance
 * - 10000 samples/sec
 * - Binary UART protocol (256000 baud)
 * - Motor PWM control
 * - Express scan mode support
 *
 * Protocol commands:
 * - STOP: Stop scanning
 * - RESET: Soft reset
 * - SCAN: Start standard scan
 * - EXPRESS_SCAN: Start express scan (higher speed)
 * - FORCE_SCAN: Force scan without motor check
 * - GET_INFO: Get device info
 * - GET_HEALTH: Get health status
 */

#define RPLIDAR_UART_BAUD_RATE (256000)
#define RPLIDAR_UART_BUF_SIZE (2048)
#define RPLIDAR_MAX_SCAN_POINTS (8192)

// Protocol bytes
#define RPLIDAR_SYNC_BYTE1 (0xA5)
#define RPLIDAR_SYNC_BYTE2 (0x5A)

// Commands
#define RPLIDAR_CMD_STOP (0x25)
#define RPLIDAR_CMD_RESET (0x40)
#define RPLIDAR_CMD_SCAN (0x20)
#define RPLIDAR_CMD_EXPRESS_SCAN (0x82)
#define RPLIDAR_CMD_FORCE_SCAN (0x21)
#define RPLIDAR_CMD_GET_INFO (0x50)
#define RPLIDAR_CMD_GET_HEALTH (0x52)

// Response types
#define RPLIDAR_ANS_TYPE_MEASUREMENT (0x81)
#define RPLIDAR_ANS_TYPE_DEVINFO (0x04)
#define RPLIDAR_ANS_TYPE_DEVHEALTH (0x06)

typedef enum {
  RPLIDAR_HEALTH_OK = 0,
  RPLIDAR_HEALTH_WARNING = 1,
  RPLIDAR_HEALTH_ERROR = 2,
} rplidar_health_t;

typedef struct {
  float    distance_m;   // Distance in meters
  float    angle_deg;    // Angle in degrees (0-359.99)
  uint8_t  quality;      // Signal quality (0-255)
  bool     start_flag;   // true if start of new scan rotation
  uint64_t timestamp_us; // Measurement timestamp
} rplidar_scan_point_t;

typedef struct {
  uint8_t  model;
  uint8_t  firmware_major;
  uint8_t  firmware_minor;
  uint8_t  hardware;
  uint8_t  serial_number[16];
} rplidar_device_info_t;

typedef struct {
  rplidar_health_t status;
  uint16_t         error_code;
} rplidar_health_info_t;

typedef struct {
  uart_port_t uart_port;
  int         tx_pin;
  int         rx_pin;
  gpio_num_t  motor_pwm_pin;  // Motor control pin (GPIO_NUM_NC if not used)
  uint16_t    motor_pwm_duty; // PWM duty cycle (0-1023, typically 600-700)
} rplidar_config_t;

typedef struct rplidar_handle {
  uart_port_t            uart_port;
  gpio_num_t             motor_pwm_pin;
  error_handler_t        error_handler;
  bool                   initialized;
  bool                   scanning;
  rplidar_device_info_t  device_info;
  rplidar_health_info_t  health_info;
  rplidar_scan_point_t*  scan_buffer;
  uint16_t               scan_count;
  uint16_t               scan_capacity;
  uint8_t                response_buffer[512];
  uint16_t               response_index;
  bool                   waiting_for_response;
  uint8_t                expected_response_type;
} rplidar_handle_t;

/**
 * @brief Initialize RPLiDAR scanner
 *
 * @param[out] handle Pointer to handle structure
 * @param[in]  config Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_init(rplidar_handle_t* handle, const rplidar_config_t* config);

/**
 * @brief Deinitialize RPLiDAR scanner
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_deinit(rplidar_handle_t* handle);

/**
 * @brief Get device information
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] info   Device information
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_get_info(rplidar_handle_t* handle, rplidar_device_info_t* info);

/**
 * @brief Get device health status
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] health Health status
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_get_health(rplidar_handle_t* handle, rplidar_health_info_t* health);

/**
 * @brief Start motor
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_start_motor(const rplidar_handle_t* handle);

/**
 * @brief Stop motor
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_stop_motor(const rplidar_handle_t* handle);

/**
 * @brief Start scanning
 *
 * @param[in] handle       Pointer to initialized handle
 * @param[in] express_mode true for express scan (higher speed)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_start_scan(rplidar_handle_t* handle, bool express_mode);

/**
 * @brief Stop scanning
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_stop_scan(rplidar_handle_t* handle);

/**
 * @brief Process incoming scan data
 *
 * Call this regularly in a loop to process scan points.
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_process(rplidar_handle_t* handle);

/**
 * @brief Get scan data points
 *
 * @param[in]  handle     Pointer to initialized handle
 * @param[out] points     Array to store scan points
 * @param[in]  max_points Maximum points to return
 * @param[out] count      Number of points returned
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_get_scan_data(const rplidar_handle_t* handle,
                                             rplidar_scan_point_t*   points,
                                             uint16_t                max_points,
                                             uint16_t*               count);

/**
 * @brief Reset device
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_reset(rplidar_handle_t* handle);

/**
 * @brief Set scan buffer capacity
 *
 * @param[in] handle   Pointer to initialized handle
 * @param[in] capacity Maximum scan points to buffer
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_rplidar_set_scan_buffer_size(rplidar_handle_t* handle, uint16_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_RPLIDAR_C1_H */
