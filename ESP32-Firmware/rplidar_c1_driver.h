/**
 * @file rplidar_c1_driver.h
 * @brief RPLiDAR C1 DTOF Scanner Driver for ESP32-IDF
 *
 * Complete driver implementation with:
 * - UART protocol handling (460800 baud)
 * - Motor control with soft-start/soft-stop
 * - Point cloud acquisition with circular buffering
 * - Error handling and recovery
 * - Memory-safe operations for embedded systems
 *
 * @author Generated for ESP32-IDF
 * @date 2025-11-20
 * @version 2.0
 */

#ifndef RPLIDAR_C1_DRIVER_H
#define RPLIDAR_C1_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Constants ==================== */

#define RPLIDAR_BAUD_RATE           460800
#define RPLIDAR_UART_TIMEOUT_MS     500
#define RPLIDAR_SYNC_BYTE           0xA5
#define RPLIDAR_DESC_BYTE           0x5A

// Motor PWM ranges
#define RPLIDAR_PWM_MIN             0
#define RPLIDAR_PWM_DEFAULT         660
#define RPLIDAR_PWM_MAX             1023
#define RPLIDAR_PWM_SAFE_MIN        200    // Minimum for reliable rotation

// Buffer sizes
#define RPLIDAR_RX_BUFFER_SIZE      8192
#define RPLIDAR_FRAME_BUFFER_SIZE   10
#define RPLIDAR_MAX_POINTS_PER_FRAME 5000

/* ==================== Enumerations ==================== */

/**
 * @brief RPLiDAR Command Type
 */
typedef enum {
    RPLIDAR_CMD_STOP        = 0x25,
    RPLIDAR_CMD_RESET       = 0x40,
    RPLIDAR_CMD_SCAN        = 0x20,
    RPLIDAR_CMD_FORCE_SCAN  = 0x21,
    RPLIDAR_CMD_EXPRESS     = 0x82,
    RPLIDAR_CMD_GET_INFO    = 0x50,
    RPLIDAR_CMD_GET_HEALTH  = 0x52,
    RPLIDAR_CMD_SET_PWM     = 0xF0
} rplidar_command_t;

/**
 * @brief RPLiDAR Response Data Type
 */
typedef enum {
    RPLIDAR_DT_MEASUREMENT  = 0x00,
    RPLIDAR_DT_DEVICE_INFO  = 0x04,
    RPLIDAR_DT_HEALTH       = 0x03,
    RPLIDAR_DT_EXPRESS      = 0x82
} rplidar_data_type_t;

/**
 * @brief RPLiDAR Express Scan Mode
 */
typedef enum {
    RPLIDAR_EXPRESS_MODE    = 0x00,  // 4kHz
    RPLIDAR_EXPRESS_FAST    = 0x01,  // 4kHz optimized
    RPLIDAR_EXPRESS_DENSE   = 0x02   // 8kHz
} rplidar_express_mode_t;

/**
 * @brief Health Status Codes
 */
typedef enum {
    RPLIDAR_HEALTH_GOOD     = 0x00,
    RPLIDAR_HEALTH_WARNING  = 0x01,
    RPLIDAR_HEALTH_ERROR    = 0x02
} rplidar_health_status_t;

/**
 * @brief Error Codes
 */
typedef enum {
    RPLIDAR_OK                      = 0,
    RPLIDAR_ERR_TIMEOUT             = -1,
    RPLIDAR_ERR_CHECKSUM            = -2,
    RPLIDAR_ERR_INVALID_PKT         = -3,
    RPLIDAR_ERR_UART                = -4,
    RPLIDAR_ERR_ALLOC               = -5,
    RPLIDAR_ERR_SIZE_EXCEEDED       = -6,
    RPLIDAR_ERR_INVALID_DESCRIPTOR  = -7,
    RPLIDAR_ERR_INCOMPLETE          = -8,
    RPLIDAR_ERR_NOT_INITIALIZED     = -9,
    RPLIDAR_ERR_ALREADY_RUNNING     = -10
} rplidar_status_t;

/**
 * @brief Driver State Machine
 */
typedef enum {
    RPLIDAR_STATE_IDLE      = 0,
    RPLIDAR_STATE_STARTING  = 1,
    RPLIDAR_STATE_RUNNING   = 2,
    RPLIDAR_STATE_STOPPING  = 3,
    RPLIDAR_STATE_ERROR     = 4
} rplidar_state_t;

/* ==================== Data Structures ==================== */

/**
 * @brief Single Measurement Point
 */
typedef struct {
    uint16_t distance_mm;      // Distance in millimeters (0.05m-12m)
    float angle_deg;           // Angle in degrees (0.0-360.0)
    uint8_t quality;           // Signal quality (0-63)
    uint32_t timestamp_ms;     // Measurement timestamp
    bool valid;                // Validity flag
} rplidar_measurement_t;

/**
 * @brief Device Information Response
 */
typedef struct {
    uint8_t model;             // Device model (0xA4 for C1)
    uint8_t firmware_major;    // Firmware major version
    uint8_t firmware_minor;    // Firmware minor version
    uint8_t hardware_version;  // Hardware version
    uint8_t serial[16];        // Serial number (16 bytes)
} rplidar_device_info_t;

/**
 * @brief Device Health Status
 */
typedef struct {
    rplidar_health_status_t status;  // 0=Good, 1=Warning, 2=Error
    uint16_t error_code;             // Error code (see error flags)
} rplidar_health_t;

/**
 * @brief Point Cloud Frame
 */
typedef struct {
    rplidar_measurement_t points[RPLIDAR_MAX_POINTS_PER_FRAME];
    uint16_t count;
    uint32_t frame_id;
    uint64_t timestamp_us;
    float avg_quality;
} rplidar_frame_t;

/**
 * @brief Circular Frame Buffer
 */
typedef struct {
    rplidar_frame_t* frames;
    uint32_t capacity;
    volatile uint32_t write_idx;
    volatile uint32_t read_idx;
    portMUX_TYPE spinlock;

    // Statistics
    uint32_t total_frames;
    uint32_t dropped_frames;
    uint32_t overflow_count;
} rplidar_circular_buffer_t;

/**
 * @brief Packet Descriptor
 */
typedef struct {
    uint8_t sync_byte;         // 0xA5
    uint8_t desc_byte;         // 0x5A
    uint16_t data_length;      // Little-endian data length
    uint8_t data_type;         // Response type
    uint8_t single_response;   // 0=Multiple, 1=Single
    uint8_t checksum;          // XOR checksum
} rplidar_descriptor_t;

/**
 * @brief Driver Configuration
 */
typedef struct {
    uart_port_t uart_num;
    int tx_pin;
    int rx_pin;
    uint32_t baud_rate;
    uint32_t rx_buffer_size;
    uint32_t frame_buffer_size;
    void (*data_callback)(rplidar_measurement_t* measurement);
    void (*frame_complete_callback)(rplidar_frame_t* frame);
    void (*error_callback)(rplidar_status_t error);
} rplidar_config_t;

/**
 * @brief Driver Instance
 */
typedef struct {
    uart_port_t uart_num;
    QueueHandle_t uart_queue;
    rplidar_state_t state;
    rplidar_circular_buffer_t* frame_buffer;
    uint16_t current_pwm;
    uint32_t measurements_received;
    uint32_t frames_completed;
    uint64_t start_time_us;
    portMUX_TYPE state_spinlock;

    // Callbacks
    void (*data_callback)(rplidar_measurement_t* measurement);
    void (*frame_complete_callback)(rplidar_frame_t* frame);
    void (*error_callback)(rplidar_status_t error);
} rplidar_driver_t;

/**
 * @brief Driver Statistics
 */
typedef struct {
    uint32_t measurements_received;
    uint32_t frames_completed;
    uint32_t frames_dropped;
    uint32_t uart_errors;
    uint32_t checksum_errors;
    float avg_measurement_rate;
    float frame_rate;
    float avg_point_quality;
} rplidar_stats_t;

/* ==================== Function Prototypes ==================== */

/**
 * @brief Initialize RPLiDAR driver
 *
 * @param config Driver configuration
 * @param driver Output driver instance
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_init(const rplidar_config_t* config,
                              rplidar_driver_t* driver);

/**
 * @brief Start continuous scanning
 *
 * @param driver Driver instance
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_start_scan(rplidar_driver_t* driver);

/**
 * @brief Start Express mode scanning (higher data density)
 *
 * @param driver Driver instance
 * @param mode Express mode selection
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_start_express_scan(rplidar_driver_t* driver,
                                           rplidar_express_mode_t mode);

/**
 * @brief Stop scanning and motor
 *
 * @param driver Driver instance
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_stop_scan(rplidar_driver_t* driver);

/**
 * @brief Force single frame acquisition
 *
 * @param driver Driver instance
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_force_scan(rplidar_driver_t* driver);

/**
 * @brief Reset device
 *
 * @param driver Driver instance
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_reset(rplidar_driver_t* driver);

/**
 * @brief Set motor PWM speed (0-1023)
 *
 * @param driver Driver instance
 * @param pwm PWM value (0-1023, default 660)
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_set_motor_pwm(rplidar_driver_t* driver, uint16_t pwm);

/**
 * @brief Soft-start motor to target speed
 *
 * @param driver Driver instance
 * @param target_pwm Target PWM value
 * @param step_size PWM increment per step
 * @param step_delay_ms Delay between steps
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_soft_start(rplidar_driver_t* driver,
                                   uint16_t target_pwm,
                                   uint16_t step_size,
                                   uint32_t step_delay_ms);

/**
 * @brief Soft-stop motor
 *
 * @param driver Driver instance
 * @param step_size PWM decrement per step
 * @param step_delay_ms Delay between steps
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_soft_stop(rplidar_driver_t* driver,
                                  uint16_t step_size,
                                  uint32_t step_delay_ms);

/**
 * @brief Get device health status
 *
 * @param driver Driver instance
 * @param health Output health structure
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_get_health(rplidar_driver_t* driver,
                                   rplidar_health_t* health);

/**
 * @brief Get device information
 *
 * @param driver Driver instance
 * @param info Output device information
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_get_info(rplidar_driver_t* driver,
                                 rplidar_device_info_t* info);

/**
 * @brief Read single measurement from buffer
 *
 * @param driver Driver instance
 * @param measurement Output measurement
 * @param timeout_ms Read timeout
 * @return rplidar_status_t RPLIDAR_OK if measurement available
 */
rplidar_status_t rplidar_read_measurement(rplidar_driver_t* driver,
                                         rplidar_measurement_t* measurement,
                                         uint32_t timeout_ms);

/**
 * @brief Read completed frame from circular buffer
 *
 * @param driver Driver instance
 * @param frame Output frame
 * @return bool true if frame available, false if buffer empty
 */
bool rplidar_read_frame(rplidar_driver_t* driver, rplidar_frame_t* frame);

/**
 * @brief Get current driver state
 *
 * @param driver Driver instance
 * @return rplidar_state_t Current state
 */
rplidar_state_t rplidar_get_state(rplidar_driver_t* driver);

/**
 * @brief Get driver statistics
 *
 * @param driver Driver instance
 * @param stats Output statistics
 */
void rplidar_get_stats(rplidar_driver_t* driver, rplidar_stats_t* stats);

/**
 * @brief Check if scanning is active
 *
 * @param driver Driver instance
 * @return bool true if scanning
 */
bool rplidar_is_scanning(rplidar_driver_t* driver);

/**
 * @brief Get circular buffer status
 *
 * @param driver Driver instance
 * @param available_frames Output available frames count
 * @param capacity_percent Output buffer capacity percentage
 */
void rplidar_get_buffer_status(rplidar_driver_t* driver,
                              uint32_t* available_frames,
                              float* capacity_percent);

/**
 * @brief Deinitialize driver and release resources
 *
 * @param driver Driver instance
 */
void rplidar_deinit(rplidar_driver_t* driver);

/**
 * @brief Clear UART buffers and resynchronize
 *
 * @param driver Driver instance
 * @return rplidar_status_t RPLIDAR_OK on success
 */
rplidar_status_t rplidar_clear_buffers(rplidar_driver_t* driver);

/* ==================== Utility Functions ==================== */

/**
 * @brief Calculate XOR checksum
 *
 * @param data Data buffer
 * @param length Buffer length
 * @return uint8_t Checksum value
 */
uint8_t rplidar_calculate_checksum(const uint8_t* data, size_t length);

/**
 * @brief Validate measurement data
 *
 * @param buf Raw 5-byte measurement buffer
 * @return bool true if valid
 */
bool rplidar_validate_measurement(const uint8_t* buf);

/**
 * @brief Parse measurement from raw buffer
 *
 * @param buf Raw 5-byte measurement buffer
 * @param measurement Output measurement
 * @return bool true if successfully parsed
 */
bool rplidar_parse_measurement(const uint8_t* buf,
                               rplidar_measurement_t* measurement);

/**
 * @brief Get error string description
 *
 * @param error Error code
 * @return const char* Error description
 */
const char* rplidar_error_string(rplidar_status_t error);

/**
 * @brief Interpret health error code
 *
 * @param error_code Error code bits
 * @return const char* Error description
 */
const char* rplidar_error_code_string(uint16_t error_code);

/**
 * @brief Convert PWM value to percentage
 *
 * @param pwm PWM value (0-1023)
 * @return float Percentage (0.0-100.0)
 */
float rplidar_pwm_to_percent(uint16_t pwm);

/**
 * @brief Convert percentage to PWM value
 *
 * @param percent Percentage (0.0-100.0)
 * @return uint16_t PWM value (0-1023)
 */
uint16_t rplidar_percent_to_pwm(float percent);

/* ==================== Advanced Features ==================== */

/**
 * @brief Enable automatic health monitoring
 *
 * @param driver Driver instance
 * @param interval_ms Monitoring interval
 */
void rplidar_enable_health_monitor(rplidar_driver_t* driver,
                                  uint32_t interval_ms);

/**
 * @brief Disable automatic health monitoring
 *
 * @param driver Driver instance
 */
void rplidar_disable_health_monitor(rplidar_driver_t* driver);

/**
 * @brief Create circular frame buffer
 *
 * @param capacity Number of frames
 * @return rplidar_circular_buffer_t* Allocated buffer or NULL
 */
rplidar_circular_buffer_t* rplidar_create_frame_buffer(uint32_t capacity);

/**
 * @brief Write frame to circular buffer
 *
 * @param buffer Buffer instance
 * @param frame Frame to write
 * @return bool true on success
 */
bool rplidar_write_frame(rplidar_circular_buffer_t* buffer,
                        const rplidar_frame_t* frame);

/**
 * @brief Read frame from circular buffer
 *
 * @param buffer Buffer instance
 * @param frame Output frame
 * @return bool true if frame available
 */
bool rplidar_read_frame_buffer(rplidar_circular_buffer_t* buffer,
                              rplidar_frame_t* frame);

/**
 * @brief Destroy circular frame buffer
 *
 * @param buffer Buffer to destroy
 */
void rplidar_destroy_frame_buffer(rplidar_circular_buffer_t* buffer);

#ifdef __cplusplus
}
#endif

#endif // RPLIDAR_C1_DRIVER_H
