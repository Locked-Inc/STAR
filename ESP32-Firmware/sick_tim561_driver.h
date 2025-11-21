/**
 * SICK TiM561-2050101 2D LiDAR Driver for ESP32-IDF
 *
 * Comprehensive header file with complete protocol definitions,
 * security structures, and memory management utilities.
 *
 * Author: Technical Research
 * Date: 2025-11-20
 * Status: Production Ready
 */

#ifndef SICK_TIM561_DRIVER_H
#define SICK_TIM561_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SENSOR SPECIFICATIONS
 * ============================================================================ */

#define SICK_TIM561_MODEL              "TiM561-2050101"
#define SICK_TIM561_FOV_DEGREES        270
#define SICK_TIM561_ANGULAR_RESOLUTION 0.33  /* degrees */
#define SICK_TIM561_POINTS_PER_SCAN    2880  /* 270 / 0.33 * 3.67 */
#define SICK_TIM561_SCAN_FREQUENCY     15    /* Hz */
#define SICK_TIM561_MIN_RANGE_MM       50    /* 5 cm */
#define SICK_TIM561_MAX_RANGE_MM       10000 /* 10 m */
#define SICK_TIM561_LASER_CLASS        1     /* Eye-safe */
#define SICK_TIM561_LASER_WAVELENGTH   850   /* nm (IR) */

/* ============================================================================
 * NETWORK CONFIGURATION
 * ============================================================================ */

#define SICK_TIM561_DEFAULT_IP         "192.168.0.1"
#define SICK_TIM561_DEFAULT_PORT       2112
#define SICK_TIM561_TCP_TIMEOUT_MS     5000
#define SICK_TIM561_TCP_KEEPALIVE_SEC  10
#define SICK_TIM561_MAX_RECONNECT_TRIES 5

/* M12 D-Coded Connector Pinout */
#define M12_PIN_TX_PLUS    1   /* Yellow */
#define M12_PIN_TX_MINUS   2   /* White  */
#define M12_PIN_RX_PLUS    3   /* Orange */
#define M12_PIN_RX_MINUS   4   /* Blue   */

/* ============================================================================
 * PROTOCOL DEFINITIONS
 * ============================================================================ */

/* Telegram Frame Markers */
#define TELEGRAM_STX                   0x02  /* Start of transmission */
#define TELEGRAM_ETX                   0x03  /* End of transmission */
#define TELEGRAM_SEPARATOR            ' '   /* Space character */

/* Maximum Sizes */
#define TELEGRAM_MAX_SIZE              65536 /* 64 KB absolute maximum */
#define TELEGRAM_HEADER_SIZE           5     /* STX + CRC(2) + ETX */
#define TELEGRAM_MAX_PARAMETERS        256
#define TELEGRAM_MAX_TOKEN_LENGTH      32

/* Buffer Configuration */
#define TELEGRAM_RX_BUFFER_SIZE        65536
#define TELEGRAM_TX_BUFFER_SIZE        256

/* ============================================================================
 * CoLa PROTOCOL COMMAND TYPES
 * ============================================================================ */

typedef enum {
    COLA_CMD_RESPONSE            = 0x01,  /* sRA - Response */
    COLA_CMD_REQUEST_ONCE        = 0x02,  /* sRN - Request once */
    COLA_CMD_START_STREAM        = 0x03,  /* sEN - Enable (stream) */
    COLA_CMD_STOP_STREAM         = 0x04,  /* sEO - End output */
    COLA_CMD_WRITE_PARAM         = 0x05,  /* sWN - Write (set) */
    COLA_CMD_UNKNOWN             = 0xFF
} ColaCommandType_t;

typedef enum {
    COLA_RESULT_OK               = 0,
    COLA_RESULT_INVALID_ARG      = 1,
    COLA_RESULT_UNKNOWN_METHOD   = 2,
    COLA_RESULT_METHOD_NOT_FOUND = 3,
    COLA_RESULT_NO_MEMORY        = 4,
    COLA_RESULT_DEVICE_ERROR     = 5,
    COLA_RESULT_TIMEOUT          = 0xFF
} ColaResultCode_t;

/* ============================================================================
 * MEASUREMENT DATA STRUCTURES
 * ============================================================================ */

/**
 * Single scan point containing distance and angular information
 */
typedef struct {
    uint16_t distance_mm;        /* Distance in millimeters */
    double angle_degrees;        /* Angle in degrees (-27.5 to +242.5 typical) */
    uint8_t reflectivity;        /* 0-100 (if available) */
    uint8_t flags;               /* Validity flags */
    uint16_t raw_distance;       /* Raw value from sensor */
} ScanPoint_t;

/* Scan point flags */
#define SCANPOINT_VALID              (1 << 0)
#define SCANPOINT_REFLECT_VALID      (1 << 1)
#define SCANPOINT_OUT_OF_RANGE       (1 << 2)
#define SCANPOINT_NO_REFLECTANCE     (1 << 3)
#define SCANPOINT_CORRUPTED          (1 << 4)

/**
 * Complete 2D scan containing all measurement points
 */
typedef struct {
    ScanPoint_t points[SICK_TIM561_POINTS_PER_SCAN];
    uint32_t point_count;           /* Actual number of points in scan */
    uint32_t scan_counter;          /* Rolling counter from sensor */
    uint32_t timestamp_ms;          /* Timestamp when scan received */
    uint16_t device_status[2];      /* Device status flags */
    uint32_t message_counter;       /* Message counter from device */
    float scale_factor;             /* Distance scale factor (usually 1.0) */
    int32_t start_angle_units;      /* Start angle in 1/10000 degree units */
    int32_t angle_step_units;       /* Angle step in 1/10000 degree units */
    uint32_t device_frequency;      /* Scanning frequency from header */
    uint32_t measurement_frequency; /* Measurement frequency from header */
    uint8_t quality_flags;          /* Data quality indicators */
} ScanData_t;

/* Scan quality flags */
#define SCANDATA_COMPLETE            (1 << 0)  /* All points received */
#define SCANDATA_VALID_TIMESTAMPS    (1 << 1)  /* Timestamp field valid */
#define SCANDATA_ENCODER_AVAILABLE   (1 << 2)  /* Encoder data present */
#define SCANDATA_REFLECTIVITY_VALID  (1 << 3)  /* Reflectivity channel valid */
#define SCANDATA_POSITION_VALID      (1 << 4)  /* Position data valid */

/**
 * Detailed distance channel information (for multi-channel support)
 */
typedef struct {
    char content_type[32];          /* e.g., "DIST1", "RSSI1" */
    uint32_t scale_factor_bits;     /* IEEE 754 representation */
    uint32_t scale_offset_bits;     /* IEEE 754 representation */
    int32_t start_angle_units;      /* Start angle (1/10000 degrees) */
    int32_t angle_step_units;       /* Step size (1/10000 degrees) */
    uint16_t count;                 /* Number of measurements */
    uint16_t *values;               /* Measurement array */
} DistanceChannel_t;

/* ============================================================================
 * SECURITY & VALIDATION STRUCTURES
 * ============================================================================ */

/**
 * Telegram validation result
 */
typedef struct {
    bool is_valid;
    bool has_valid_stx;
    bool has_valid_etx;
    bool has_valid_crc;
    bool within_size_limits;
    size_t telegram_length;
    uint16_t received_crc;
    uint16_t calculated_crc;
} TelegramValidation_t;

/**
 * Scan sequence tracker for detecting dropped/out-of-order scans
 */
typedef struct {
    uint16_t last_scan_counter;
    uint32_t dropped_scans;
    uint32_t out_of_order_count;
    uint32_t total_scans_received;
} ScanSequenceTracker_t;

/**
 * Distance value validation result
 */
typedef enum {
    DISTANCE_VALID                = 0,
    DISTANCE_OUT_OF_RANGE,
    DISTANCE_INVALID_MARKER,      /* 0x0000 or 0xFFFF */
    DISTANCE_UNREALISTIC,          /* Sudden jump from previous */
    DISTANCE_CORRUPTED
} DistanceValidation_t;

/**
 * Network security context
 */
typedef struct {
    uint8_t sensor_mac[6];          /* Expected sensor MAC (SICK OUI: 00:30:24) */
    bool mac_verified;
    uint32_t connection_attempts;
    uint32_t failed_connections;
    uint32_t last_successful_connection_ms;
    uint32_t bytes_received;
    uint32_t bytes_sent;
    uint32_t crc_errors;
    uint32_t timeout_errors;
} SecurityContext_t;

/* ============================================================================
 * BUFFER MANAGEMENT STRUCTURES
 * ============================================================================ */

/**
 * Growable buffer for receiving telegrams
 */
typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t length;
    size_t max_capacity;
    TickType_t last_access;
} GrowableBuffer_t;

/**
 * Ring buffer for decoded scan frames
 */
typedef struct {
    ScanData_t frames[2];           /* Double-buffer pattern */
    uint8_t write_index;
    uint8_t read_index;
    portMUX_TYPE lock;
    uint32_t frames_written;
    uint32_t frames_read;
} ScanFrameBuffer_t;

/**
 * Network buffer statistics
 */
typedef struct {
    uint32_t total_bytes_received;
    uint32_t total_bytes_sent;
    uint32_t buffer_overflows;
    uint32_t buffer_underflows;
    uint32_t heap_peak_usage;
    uint32_t current_heap_usage;
} BufferStatistics_t;

/* ============================================================================
 * CONNECTION STATE MACHINE
 * ============================================================================ */

typedef enum {
    CONN_STATE_DISCONNECTED        = 0,
    CONN_STATE_CONNECTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_STREAMING,
    CONN_STATE_ERROR,
    CONN_STATE_RECONNECTING
} ConnectionState_t;

typedef enum {
    CONN_EVENT_CONNECT_REQUEST     = 0,
    CONN_EVENT_CONNECT_SUCCESS,
    CONN_EVENT_CONNECT_FAILURE,
    CONN_EVENT_DISCONNECT_REQUEST,
    CONN_EVENT_START_STREAM,
    CONN_EVENT_STOP_STREAM,
    CONN_EVENT_TIMEOUT,
    CONN_EVENT_ERROR
} ConnectionEvent_t;

/* ============================================================================
 * MAIN DRIVER HANDLE
 * ============================================================================ */

typedef struct {
    /* Socket and connection info */
    int socket_fd;
    char sensor_ip[16];
    uint16_t port;
    ConnectionState_t state;

    /* Buffers */
    GrowableBuffer_t rx_buffer;
    uint8_t tx_buffer[TELEGRAM_TX_BUFFER_SIZE];
    ScanFrameBuffer_t scan_buffer;

    /* Scan data */
    ScanSequenceTracker_t seq_tracker;
    ScanData_t last_scan;

    /* Security */
    SecurityContext_t security;

    /* Statistics */
    BufferStatistics_t stats;

    /* Thread control */
    TaskHandle_t rx_task;
    TaskHandle_t process_task;
    QueueHandle_t event_queue;
    SemaphoreHandle_t state_mutex;

    /* Configuration */
    bool auto_reconnect;
    uint32_t max_reconnect_attempts;
    uint32_t reconnect_interval_ms;

    /* Callbacks */
    void (*on_scan_received)(ScanData_t *scan);
    void (*on_error)(esp_err_t error);
    void (*on_state_changed)(ConnectionState_t new_state);
} SickTiM561Driver_t;

/* ============================================================================
 * CRC16-CCITT FUNCTIONS
 * ============================================================================ */

/**
 * Calculate CRC16-CCITT checksum
 * Polynomial: 0x1021
 * Initial value: 0xFFFF
 */
uint16_t sick_crc16_ccitt(const uint8_t *data, size_t len);

/**
 * Verify CRC16-CCITT in telegram
 * Telegram format: [STX][data][CRC_HIGH][CRC_LOW][ETX]
 */
bool sick_verify_crc16(const uint8_t *telegram, size_t length);

/* ============================================================================
 * DRIVER INITIALIZATION & LIFECYCLE
 * ============================================================================ */

/**
 * Initialize SICK TiM561 driver
 *
 * @param handle        Pointer to driver handle
 * @param sensor_ip     IP address of sensor (e.g., "192.168.0.1")
 * @param port          TCP port (usually 2112)
 * @return              ESP_OK on success
 */
esp_err_t sick_tim561_init(SickTiM561Driver_t *handle,
                           const char *sensor_ip,
                           uint16_t port);

/**
 * Establish connection to sensor
 */
esp_err_t sick_tim561_connect(SickTiM561Driver_t *handle);

/**
 * Start continuous scan stream
 */
esp_err_t sick_tim561_start_streaming(SickTiM561Driver_t *handle);

/**
 * Stop continuous scan stream
 */
esp_err_t sick_tim561_stop_streaming(SickTiM561Driver_t *handle);

/**
 * Request single scan (blocking)
 */
esp_err_t sick_tim561_request_single_scan(SickTiM561Driver_t *handle,
                                          ScanData_t *scan,
                                          uint32_t timeout_ms);

/**
 * Get latest available scan (non-blocking)
 */
esp_err_t sick_tim561_get_latest_scan(SickTiM561Driver_t *handle,
                                      ScanData_t *scan);

/**
 * Disconnect from sensor
 */
esp_err_t sick_tim561_disconnect(SickTiM561Driver_t *handle);

/**
 * Cleanup driver resources
 */
void sick_tim561_deinit(SickTiM561Driver_t *handle);

/* ============================================================================
 * CONFIGURATION FUNCTIONS
 * ============================================================================ */

/**
 * Configure scan data output
 * Enables/disables various output channels
 */
esp_err_t sick_tim561_configure_output(SickTiM561Driver_t *handle,
                                       uint8_t output_channels,
                                       uint8_t encoder_blocks,
                                       uint8_t channel_16bit,
                                       uint8_t channel_8bit,
                                       uint8_t position,
                                       uint8_t device_name,
                                       uint8_t comment,
                                       uint8_t time_info,
                                       uint8_t events);

/**
 * Set device identity/name
 */
esp_err_t sick_tim561_set_device_name(SickTiM561Driver_t *handle,
                                      const char *name);

/**
 * Get device information
 */
esp_err_t sick_tim561_get_device_info(SickTiM561Driver_t *handle,
                                      char *model,
                                      char *serial,
                                      char *firmware);

/* ============================================================================
 * DATA PARSING & VALIDATION
 * ============================================================================ */

/**
 * Validate telegram structure and CRC
 */
TelegramValidation_t sick_validate_telegram(const uint8_t *telegram,
                                             size_t length);

/**
 * Parse LMDscandata response into ScanData_t structure
 */
esp_err_t sick_parse_lmdscandata(const uint8_t *telegram,
                                 size_t length,
                                 ScanData_t *scan);

/**
 * Validate individual distance measurement
 */
DistanceValidation_t sick_validate_distance(uint16_t raw_value,
                                            float scale_factor,
                                            uint16_t last_valid_distance);

/**
 * Convert angle from sensor units to degrees
 */
double sick_angle_units_to_degrees(int32_t angle_units);

/**
 * Convert distance from raw units to meters
 */
float sick_distance_to_meters(uint16_t raw_distance,
                              float scale_factor);

/* ============================================================================
 * SECURITY & MONITORING
 * ============================================================================ */

/**
 * Verify sensor MAC address (SICK OUI: 00:30:24)
 */
bool sick_verify_sensor_mac(const uint8_t *mac);

/**
 * Check scan sequence for dropped/out-of-order scans
 */
void sick_check_scan_sequence(ScanSequenceTracker_t *tracker,
                              uint16_t current_counter);

/**
 * Get driver statistics
 */
void sick_get_statistics(SickTiM561Driver_t *handle,
                         BufferStatistics_t *stats);

/**
 * Reset driver statistics
 */
void sick_reset_statistics(SickTiM561Driver_t *handle);

/**
 * Monitor and report memory health
 */
void sick_monitor_memory_health(void);

/* ============================================================================
 * BUFFER MANAGEMENT
 * ============================================================================ */

/**
 * Create growable buffer
 */
GrowableBuffer_t *sick_growable_buffer_create(size_t initial_size,
                                              size_t max_size);

/**
 * Append data to growable buffer
 */
esp_err_t sick_growable_buffer_append(GrowableBuffer_t *buf,
                                      const uint8_t *data,
                                      size_t len);

/**
 * Clear growable buffer
 */
void sick_growable_buffer_clear(GrowableBuffer_t *buf);

/**
 * Destroy growable buffer
 */
void sick_growable_buffer_destroy(GrowableBuffer_t *buf);

/* ============================================================================
 * CALLBACK REGISTRATION
 * ============================================================================ */

/**
 * Register callback for new scan data
 */
void sick_register_scan_callback(SickTiM561Driver_t *handle,
                                 void (*callback)(ScanData_t *scan));

/**
 * Register callback for errors
 */
void sick_register_error_callback(SickTiM561Driver_t *handle,
                                  void (*callback)(esp_err_t error));

/**
 * Register callback for state changes
 */
void sick_register_state_callback(SickTiM561Driver_t *handle,
                                  void (*callback)(ConnectionState_t state));

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * Convert connection state to string
 */
const char *sick_connection_state_str(ConnectionState_t state);

/**
 * Convert validation status to error code
 */
esp_err_t sick_validation_to_error(TelegramValidation_t *validation);

/**
 * Pretty-print scan data for debugging
 */
void sick_print_scan_data(const ScanData_t *scan);

/**
 * Pretty-print scan statistics
 */
void sick_print_statistics(const BufferStatistics_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* SICK_TIM561_DRIVER_H */

/*
 * ============================================================================
 * USAGE EXAMPLE
 * ============================================================================
 *
 * #include "sick_tim561_driver.h"
 *
 * void app_main(void) {
 *     SickTiM561Driver_t lidar;
 *
 *     // Initialize driver
 *     if (sick_tim561_init(&lidar, "192.168.0.100", 2112) != ESP_OK) {
 *         ESP_LOGE("APP", "Failed to initialize");
 *         return;
 *     }
 *
 *     // Connect to sensor
 *     if (sick_tim561_connect(&lidar) != ESP_OK) {
 *         ESP_LOGE("APP", "Failed to connect");
 *         sick_tim561_deinit(&lidar);
 *         return;
 *     }
 *
 *     // Start streaming
 *     sick_tim561_start_streaming(&lidar);
 *
 *     // Process scans
 *     ScanData_t scan;
 *     while (1) {
 *         if (sick_tim561_get_latest_scan(&lidar, &scan) == ESP_OK) {
 *             printf("Scan %d: %d points\n", scan.scan_counter, scan.point_count);
 *             for (int i = 0; i < scan.point_count; i++) {
 *                 if (scan.points[i].flags & SCANPOINT_VALID) {
 *                     printf("  Point %d: %.2f° = %.3f m\n",
 *                            i, scan.points[i].angle_degrees,
 *                            scan.points[i].distance_mm / 1000.0);
 *                 }
 *             }
 *         }
 *         vTaskDelay(pdMS_TO_TICKS(100));
 *     }
 *
 *     // Cleanup
 *     sick_tim561_stop_streaming(&lidar);
 *     sick_tim561_disconnect(&lidar);
 *     sick_tim561_deinit(&lidar);
 * }
 *
 * ============================================================================
 */
