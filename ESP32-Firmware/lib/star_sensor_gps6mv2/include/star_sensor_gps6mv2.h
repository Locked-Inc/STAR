/* lib/star_sensor_gps6mv2/include/star_sensor_gps6mv2.h */

#ifndef STAR_SENSOR_GPS6MV2_H
#define STAR_SENSOR_GPS6MV2_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "driver/uart.h"
#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_sensor_gps6mv2.h
 * @brief GY-GPS6MV2 GPS module driver (NEO-6M chipset)
 *
 * This driver provides NMEA sentence parsing for GPS data.
 * Supported sentences:
 * - $GPGGA: Fix data (position, altitude, satellites)
 * - $GPRMC: Recommended minimum (position, speed, course, date/time)
 * - $GPGSA: DOP and active satellites
 * - $GPGSV: Satellites in view
 *
 * Protocol:
 * - UART 9600 baud, 8N1
 * - NMEA 0183 format
 * - Update rate: 1 Hz default (configurable to 5 Hz)
 * - Cold start: 27s, Warm start: 1s, Hot start: 1s
 */

#define GPS6MV2_UART_BAUD_RATE (9600)
#define GPS6MV2_UART_BUF_SIZE (1024)
#define GPS6MV2_NMEA_MAX_LENGTH (82)  // NMEA max sentence length
#define GPS6MV2_MAX_SATELLITES (12)

typedef enum {
  GPS6MV2_FIX_NONE = 0,
  GPS6MV2_FIX_2D = 2,
  GPS6MV2_FIX_3D = 3,
} gps6mv2_fix_t;

typedef struct {
  uint8_t prn;        // Satellite PRN number
  uint8_t elevation;  // Elevation in degrees (0-90)
  uint16_t azimuth;   // Azimuth in degrees (0-359)
  uint8_t snr;        // Signal-to-noise ratio in dB (0-99)
} gps6mv2_satellite_t;

typedef struct {
  double latitude;   // Decimal degrees, positive = North
  double longitude;  // Decimal degrees, positive = East
  float  altitude;   // Altitude above mean sea level in meters
  float  speed_kmh;  // Ground speed in km/h
  float  course;     // Course over ground in degrees (0-359.99)
  float  hdop;       // Horizontal dilution of precision
  float  vdop;       // Vertical dilution of precision
  float  pdop;       // Position dilution of precision
  uint8_t satellites_used;  // Number of satellites used in fix
  uint8_t satellites_view;  // Number of satellites in view
  gps6mv2_fix_t fix_quality;  // Fix quality
  struct tm timestamp;        // UTC date/time
  bool valid;                 // true if fix is valid
} gps6mv2_data_t;

typedef struct {
  uart_port_t uart_port;
  int         tx_pin;
  int         rx_pin;
  bool        enable_gga;  // Enable $GPGGA parsing
  bool        enable_rmc;  // Enable $GPRMC parsing
  bool        enable_gsa;  // Enable $GPGSA parsing
  bool        enable_gsv;  // Enable $GPGSV parsing
} gps6mv2_config_t;

typedef struct gps6mv2_handle {
  uart_port_t     uart_port;
  error_handler_t error_handler;
  bool            initialized;
  gps6mv2_data_t  current_data;
  gps6mv2_satellite_t satellites[GPS6MV2_MAX_SATELLITES];
  uint8_t         satellite_count;
  char            nmea_buffer[GPS6MV2_NMEA_MAX_LENGTH + 1];
  uint16_t        nmea_index;
  bool            enable_gga;
  bool            enable_rmc;
  bool            enable_gsa;
  bool            enable_gsv;
} gps6mv2_handle_t;

/**
 * @brief Initialize GPS module
 *
 * @param[out] handle Pointer to handle structure
 * @param[in]  config Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_gps6mv2_init(gps6mv2_handle_t* handle, const gps6mv2_config_t* config);

/**
 * @brief Deinitialize GPS module
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_gps6mv2_deinit(gps6mv2_handle_t* handle);

/**
 * @brief Process incoming UART data and parse NMEA sentences
 *
 * Call this regularly (e.g., in a task loop) to process GPS data.
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_gps6mv2_process(gps6mv2_handle_t* handle);

/**
 * @brief Get current GPS data
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] data   Current GPS position and status
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if no valid fix
 */
esp_err_t star_sensor_gps6mv2_get_data(const gps6mv2_handle_t* handle, gps6mv2_data_t* data);

/**
 * @brief Get satellites in view
 *
 * @param[in]  handle     Pointer to initialized handle
 * @param[out] satellites Array to store satellite info
 * @param[in]  max_count  Maximum satellites to return
 * @param[out] count      Number of satellites returned
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_gps6mv2_get_satellites(const gps6mv2_handle_t* handle,
                                              gps6mv2_satellite_t*    satellites,
                                              uint8_t                 max_count,
                                              uint8_t*                count);

/**
 * @brief Check if GPS has valid fix
 *
 * @param[in]  handle   Pointer to initialized handle
 * @param[out] has_fix  true if valid fix available
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_gps6mv2_has_fix(const gps6mv2_handle_t* handle, bool* has_fix);

/**
 * @brief Send command to GPS module
 *
 * @param[in] handle  Pointer to initialized handle
 * @param[in] command NMEA command string (without checksum)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_gps6mv2_send_command(const gps6mv2_handle_t* handle, const char* command);

/**
 * @brief Calculate distance between two GPS coordinates
 *
 * Uses Haversine formula to calculate great-circle distance.
 *
 * @param[in]  lat1     Latitude of point 1 in decimal degrees
 * @param[in]  lon1     Longitude of point 1 in decimal degrees
 * @param[in]  lat2     Latitude of point 2 in decimal degrees
 * @param[in]  lon2     Longitude of point 2 in decimal degrees
 * @param[out] distance Distance in meters
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_sensor_gps6mv2_calculate_distance(double  lat1,
                                                  double  lon1,
                                                  double  lat2,
                                                  double  lon2,
                                                  float* distance);

#ifdef __cplusplus
}
#endif

#endif /* STAR_SENSOR_GPS6MV2_H */
