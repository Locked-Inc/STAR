/**
 * GY-GPS6MV2 (NEO-6M) GPS Module Driver Header
 *
 * Public API for GPS module communication and data parsing
 * Provides thread-safe access to position, time, and configuration
 *
 * Version: 1.0
 * ESP-IDF: v4.4+
 */

#ifndef GPS_DRIVER_H
#define GPS_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DATA TYPES
// ============================================================================

/**
 * GPS position and fix quality data
 */
typedef struct {
    float latitude;             // Decimal degrees (-90 to +90)
    float longitude;            // Decimal degrees (-180 to +180)
    float altitude;             // Meters above mean sea level
    float speed_knots;          // Speed over ground in knots
    float track_true;           // True course in degrees (0-359.9)
    float hdop;                 // Horizontal Dilution of Precision
    float vdop;                 // Vertical Dilution of Precision
    float pdop;                 // Position Dilution of Precision
    uint8_t fix_quality;        // 0=invalid, 1=GPS, 2=DGPS, 3=PPS, 4=RTK fixed, 5=RTK float, 6=estimated, 7=manual, 8=simulation
    uint8_t num_satellites;     // Number of satellites used in fix (0-12)
    uint32_t timestamp_ms;      // Timestamp in milliseconds when fix was obtained
    uint32_t last_update_ms;    // Milliseconds since last valid fix
    bool is_valid;              // True if position data is valid
} gps_position_t;

/**
 * GPS time data
 */
typedef struct {
    uint8_t hours;              // 0-23
    uint8_t minutes;            // 0-59
    uint8_t seconds;            // 0-59
    uint16_t milliseconds;      // 0-999
} gps_time_t;

/**
 * GPS date data
 */
typedef struct {
    uint8_t day;                // 1-31
    uint8_t month;              // 1-12
    uint16_t year;              // 2000-2099 (4-digit year)
} gps_date_t;

/**
 * Supported baud rates for GPS module
 */
typedef enum {
    GPS_BAUD_4800 = 4800,
    GPS_BAUD_9600 = 9600,
    GPS_BAUD_19200 = 19200,
    GPS_BAUD_38400 = 38400,
    GPS_BAUD_57600 = 57600,
    GPS_BAUD_115200 = 115200,
    GPS_BAUD_230400 = 230400
} gps_baud_rate_t;

/**
 * NMEA message identifiers
 */
typedef enum {
    GPS_NMEA_GGA = 0x00,        // Global Positioning System Fix Data
    GPS_NMEA_GLL = 0x01,        // Geographic Position - Latitude/Longitude
    GPS_NMEA_GSA = 0x02,        // GPS DOP and Active Satellites
    GPS_NMEA_GSV = 0x03,        // GPS Satellites in View
    GPS_NMEA_RMC = 0x04,        // Recommended Minimum Specific GPS/Transit Data
    GPS_NMEA_VTG = 0x05         // Track Made Good and Ground Speed
} gps_nmea_message_t;

/**
 * Fix quality levels
 */
typedef enum {
    GPS_FIX_INVALID = 0,        // No fix
    GPS_FIX_GPS = 1,            // GPS fix (SPS)
    GPS_FIX_DGPS = 2,           // Differential GPS fix
    GPS_FIX_PPS = 3,            // PPS fix
    GPS_FIX_RTK_FIXED = 4,      // Real Time Kinematic (fixed)
    GPS_FIX_RTK_FLOAT = 5,      // Real Time Kinematic (float)
    GPS_FIX_ESTIMATED = 6,      // Estimated/Dead Reckoning
    GPS_FIX_MANUAL = 7,         // Manual input mode
    GPS_FIX_SIMULATION = 8      // Simulation mode
} gps_fix_quality_t;

// ============================================================================
// INITIALIZATION & CLEANUP
// ============================================================================

/**
 * Initialize GPS module and UART communication
 *
 * Sets up UART at 9600 bps, creates event queue, initializes circular buffer,
 * and starts UART event handling task.
 *
 * Safe to call multiple times (subsequent calls are no-ops).
 */
void gps_init(void);

/**
 * Deinitialize GPS module
 *
 * Uninstalls UART driver, deletes mutexes, and frees resources.
 * No GPS operations should be performed after this call.
 */
void gps_deinit(void);

// ============================================================================
// DATA READING
// ============================================================================

/**
 * Read the most recent GPS position data
 *
 * Performs atomic read-copy-update to prevent TOCTOU race conditions.
 * Extracts and validates NMEA sentences from the receive buffer.
 *
 * @param pos: Pointer to output position structure
 * @return: true if valid position data was read, false otherwise
 *
 * @note Thread-safe: Uses mutex to ensure atomic read
 * @note Validates NMEA checksums before parsing
 * @note Returns previous valid position if current sentence is malformed
 *
 * Example:
 * ```c
 * gps_position_t position;
 * if (gps_read_position(&position)) {
 *     printf("Latitude: %.6f, Longitude: %.6f\\n",
 *            position.latitude, position.longitude);
 * }
 * ```
 */
bool gps_read_position(gps_position_t *pos);

// ============================================================================
// CONFIGURATION COMMANDS
// ============================================================================

/**
 * Set GPS module UART baud rate via UBX-CFG-PRT command
 *
 * Changes the module's UART baud rate. Note: You must manually switch
 * the ESP32's UART baud rate after sending this command.
 *
 * @param new_baud: New baud rate (use gps_baud_rate_t enum values)
 *
 * @note This sends a UBX binary configuration command
 * @note Module responds with UBX-ACK-ACK or UBX-ACK-NACK
 * @note Default baud rate is 9600 bps
 * @note Configuration is volatile (lost on power cycle) unless saved
 *
 * Example:
 * ```c
 * gps_set_baud_rate(GPS_BAUD_115200);
 * vTaskDelay(pdMS_TO_TICKS(100));  // Wait for acknowledgement
 * // Now switch ESP32 UART to 115200 bps
 * ```
 */
void gps_set_baud_rate(uint32_t new_baud);

/**
 * Enable NMEA message output via UBX-CFG-MSG command
 *
 * @param message_id: NMEA message type (from gps_nmea_message_t)
 * @param rate: Output rate per position fix (0=disabled, 1=every fix)
 *
 * @note Default: All NMEA messages output at 1Hz (every fix)
 * @note Sending rate=0 disables the message
 *
 * Example:
 * ```c
 * // Enable GGA message once per fix
 * gps_enable_nmea_message(GPS_NMEA_GGA, 1);
 * // Disable RMC message
 * gps_disable_nmea_message(GPS_NMEA_RMC);
 * ```
 */
void gps_enable_nmea_message(uint8_t message_id, uint8_t rate);

/**
 * Disable NMEA message output
 *
 * Convenience function that calls gps_enable_nmea_message() with rate=0
 *
 * @param message_id: NMEA message type to disable
 */
void gps_disable_nmea_message(uint8_t message_id);

/**
 * Save current GPS configuration to flash memory via UBX-CFG-CFG
 *
 * Persists all configuration changes to the module's non-volatile memory.
 * After saving, settings will survive power cycles.
 *
 * @note This command may take several hundred milliseconds
 * @note Baud rate changes should be saved immediately after setting
 *
 * Example:
 * ```c
 * gps_set_baud_rate(GPS_BAUD_115200);
 * vTaskDelay(pdMS_TO_TICKS(100));
 * gps_save_configuration();  // Persist baud rate change
 * ```
 */
void gps_save_configuration(void);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Get the number of bytes available in the RX buffer
 *
 * @return: Number of unread bytes in circular receive buffer
 */
size_t gps_rx_buffer_available(void);

/**
 * Clear the RX buffer
 *
 * Discards all buffered data. Useful when switching modes or recovering
 * from protocol errors.
 */
void gps_clear_rx_buffer(void);

/**
 * Convert NMEA coordinate format to decimal degrees
 *
 * NMEA format: DDMM.MMMM (or DDDMM.MMMM for longitude)
 * Example: 4807.038 = 48 degrees 7.038 minutes = 48.117300 degrees
 *
 * @param nmea_value: Coordinate value in NMEA format
 * @return: Coordinate value in decimal degrees
 */
float gps_nmea_to_decimal(float nmea_value);

/**
 * Convert speed from knots to meters per second
 *
 * @param knots: Speed in nautical knots
 * @return: Speed in m/s (multiplied by 0.51444)
 */
float gps_knots_to_ms(float knots);

/**
 * Convert speed from knots to kilometers per hour
 *
 * @param knots: Speed in nautical knots
 * @return: Speed in km/h (multiplied by 1.852)
 */
float gps_knots_to_kmh(float knots);

/**
 * Calculate great-circle distance between two points (Haversine formula)
 *
 * Uses simplified Haversine formula with Earth radius of 6,371 km.
 *
 * @param lat1: Latitude of first point (decimal degrees)
 * @param lon1: Longitude of first point (decimal degrees)
 * @param lat2: Latitude of second point (decimal degrees)
 * @param lon2: Longitude of second point (decimal degrees)
 * @return: Distance in meters
 *
 * Example:
 * ```c
 * float distance = gps_distance_meters(
 *     40.712776, -74.005974,    // New York City
 *     51.506632, -0.126603      // London
 * );
 * printf("Distance: %.0f meters\\n", distance);  // ~5.6 million meters
 * ```
 */
float gps_distance_meters(float lat1, float lon1, float lat2, float lon2);

// ============================================================================
// DEBUG FUNCTIONS
// ============================================================================

/**
 * Print GPS position data to debug log
 *
 * Output format: "Pos: Lat=X.XXXXXX Lon=Y.YYYYYY Alt=Z.Z Quality=Q Sats=N"
 *
 * @param pos: Pointer to position structure
 */
void gps_print_position(const gps_position_t *pos);

/**
 * Format and validate an NMEA sentence with checksum
 *
 * Utility function for testing NMEA parsing. Calculates and appends
 * the checksum to a formatted sentence.
 *
 * @param output: Output buffer for complete sentence with checksum
 * @param output_size: Size of output buffer
 * @param format: Format string (comma-separated fields, no $ or checksum)
 * @param ...: Variable arguments for format string
 * @return: true if sentence was formatted successfully
 *
 * Example:
 * ```c
 * char test_sentence[128];
 * gps_format_nmea_sentence(test_sentence, sizeof(test_sentence),
 *                          "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M");
 * // Output: "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M*47\r\n"
 * ```
 */
bool gps_format_nmea_sentence(char *output, size_t output_size,
                             const char *format, ...);

// ============================================================================
// MODULE INFORMATION
// ============================================================================

/**
 * GPS Module Configuration (for reference)
 *
 * UART Port:        UART_NUM_1
 * Default Baud:     9600 bps
 * Data Bits:        8
 * Parity:           None
 * Stop Bits:        1
 * Flow Control:     Disabled
 * Max Position Rate: 5 Hz
 * Default Rate:     1 Hz
 *
 * Supported Protocols:
 * - NMEA 0183 (default output)
 * - UBX binary (for configuration)
 *
 * NMEA Sentence Types (default):
 * - $GPGGA - Global Positioning System Fix Data
 * - $GPRMC - Recommended Minimum Specific GPS/Transit Data
 * - $GPGSA - GPS DOP and Active Satellites
 * - $GPGSV - GPS Satellites in View
 * - $GPVTG - Track Made Good and Ground Speed
 * - $GPGLL - Geographic Position - Latitude/Longitude
 */

#ifdef __cplusplus
}
#endif

#endif  // GPS_DRIVER_H
