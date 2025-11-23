/* lib/star_sensor_gps6mv2/src/star_sensor_gps6mv2.c */

#include "star_sensor_gps6mv2.h"

#include <esp_log.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_TAG = "gps6mv2";

#define DEG_TO_RAD (M_PI / 180.0)
#define EARTH_RADIUS_M (6371000.0)  // Earth's radius in meters

// Calculate NMEA checksum
static uint8_t calculate_nmea_checksum(const char* sentence)
{
  uint8_t checksum = 0;
  // Skip the '$' and calculate XOR of all characters until '*'
  for (const char* p = sentence + 1; *p != '*' && *p != '\0'; p++) {
    checksum ^= *p;
  }
  return checksum;
}

// Verify NMEA checksum
static bool verify_nmea_checksum(const char* sentence)
{
  const char* checksum_pos = strchr(sentence, '*');
  if (checksum_pos == NULL) {
    return false;
  }

  uint8_t calculated = calculate_nmea_checksum(sentence);
  uint8_t received = (uint8_t)strtol(checksum_pos + 1, NULL, 16);

  return (calculated == received);
}

// Parse latitude/longitude from NMEA format (DDMM.MMMM) to decimal degrees
static double parse_coordinate(const char* coord_str, const char* direction)
{
  if (coord_str == NULL || direction == NULL || strlen(coord_str) == 0) {
    return 0.0;
  }

  double coord = atof(coord_str);
  int degrees = (int)(coord / 100.0);
  double minutes = coord - (degrees * 100.0);
  double decimal = degrees + (minutes / 60.0);

  // Apply direction
  if (direction[0] == 'S' || direction[0] == 'W') {
    decimal = -decimal;
  }

  return decimal;
}

// Parse time from NMEA format (HHMMSS.SSS)
static void parse_time(const char* time_str, struct tm* tm_time)
{
  if (time_str == NULL || strlen(time_str) < 6 || tm_time == NULL) {
    return;
  }

  char hour[3] = {time_str[0], time_str[1], '\0'};
  char min[3] = {time_str[2], time_str[3], '\0'};
  char sec[3] = {time_str[4], time_str[5], '\0'};

  tm_time->tm_hour = atoi(hour);
  tm_time->tm_min = atoi(min);
  tm_time->tm_sec = atoi(sec);
}

// Parse date from NMEA format (DDMMYY)
static void parse_date(const char* date_str, struct tm* tm_time)
{
  if (date_str == NULL || strlen(date_str) < 6 || tm_time == NULL) {
    return;
  }

  char day[3] = {date_str[0], date_str[1], '\0'};
  char month[3] = {date_str[2], date_str[3], '\0'};
  char year[3] = {date_str[4], date_str[5], '\0'};

  tm_time->tm_mday = atoi(day);
  tm_time->tm_mon = atoi(month) - 1;  // Months are 0-11
  tm_time->tm_year = atoi(year) + 100;  // Years since 1900
}

// Parse $GPGGA sentence
static esp_err_t parse_gpgga(gps6mv2_handle_t* handle, char* sentence)
{
  char* token;
  char* saveptr;
  int field = 0;

  token = strtok_r(sentence, ",", &saveptr);

  while (token != NULL && field < 15) {
    switch (field) {
      case 1:  // Time
        parse_time(token, &handle->current_data.timestamp);
        break;
      case 2:  // Latitude
        if (strlen(token) > 0) {
          char* lat_dir = strtok_r(NULL, ",", &saveptr);
          handle->current_data.latitude = parse_coordinate(token, lat_dir);
          field++;  // Skip direction field
        }
        break;
      case 4:  // Longitude
        if (strlen(token) > 0) {
          char* lon_dir = strtok_r(NULL, ",", &saveptr);
          handle->current_data.longitude = parse_coordinate(token, lon_dir);
          field++;  // Skip direction field
        }
        break;
      case 6:  // Fix quality
        handle->current_data.fix_quality = atoi(token) > 0 ? GPS6MV2_FIX_3D : GPS6MV2_FIX_NONE;
        handle->current_data.valid = (atoi(token) > 0);
        break;
      case 7:  // Number of satellites
        handle->current_data.satellites_used = atoi(token);
        break;
      case 8:  // HDOP
        handle->current_data.hdop = atof(token);
        break;
      case 9:  // Altitude
        handle->current_data.altitude = atof(token);
        break;
    }
    token = strtok_r(NULL, ",", &saveptr);
    field++;
  }

  return ESP_OK;
}

// Parse $GPRMC sentence
static esp_err_t parse_gprmc(gps6mv2_handle_t* handle, char* sentence)
{
  char* token;
  char* saveptr;
  int field = 0;

  token = strtok_r(sentence, ",", &saveptr);

  while (token != NULL && field < 12) {
    switch (field) {
      case 1:  // Time
        parse_time(token, &handle->current_data.timestamp);
        break;
      case 2:  // Status (A=active, V=void)
        handle->current_data.valid = (token[0] == 'A');
        break;
      case 3:  // Latitude
        if (strlen(token) > 0) {
          char* lat_dir = strtok_r(NULL, ",", &saveptr);
          handle->current_data.latitude = parse_coordinate(token, lat_dir);
          field++;  // Skip direction field
        }
        break;
      case 5:  // Longitude
        if (strlen(token) > 0) {
          char* lon_dir = strtok_r(NULL, ",", &saveptr);
          handle->current_data.longitude = parse_coordinate(token, lon_dir);
          field++;  // Skip direction field
        }
        break;
      case 7:  // Speed in knots
        handle->current_data.speed_kmh = atof(token) * 1.852f;  // Convert knots to km/h
        break;
      case 8:  // Course
        handle->current_data.course = atof(token);
        break;
      case 9:  // Date
        parse_date(token, &handle->current_data.timestamp);
        break;
    }
    token = strtok_r(NULL, ",", &saveptr);
    field++;
  }

  return ESP_OK;
}

// Parse $GPGSA sentence
static esp_err_t parse_gpgsa(gps6mv2_handle_t* handle, char* sentence)
{
  char* token;
  char* saveptr;
  int field = 0;

  token = strtok_r(sentence, ",", &saveptr);

  while (token != NULL && field < 18) {
    switch (field) {
      case 2:  // Fix type
        handle->current_data.fix_quality = atoi(token);
        break;
      case 15:  // PDOP
        handle->current_data.pdop = atof(token);
        break;
      case 16:  // HDOP
        handle->current_data.hdop = atof(token);
        break;
      case 17:  // VDOP
        handle->current_data.vdop = atof(token);
        break;
    }
    token = strtok_r(NULL, ",", &saveptr);
    field++;
  }

  return ESP_OK;
}

// Parse NMEA sentence
static esp_err_t parse_nmea_sentence(gps6mv2_handle_t* handle, char* sentence)
{
  if (!verify_nmea_checksum(sentence)) {
    ESP_LOGW(s_TAG, "Invalid checksum for: %s", sentence);
    return ESP_ERR_INVALID_CRC;
  }

  if (handle->enable_gga && strncmp(sentence, "$GPGGA", 6) == 0) {
    return parse_gpgga(handle, sentence);
  } else if (handle->enable_rmc && strncmp(sentence, "$GPRMC", 6) == 0) {
    return parse_gprmc(handle, sentence);
  } else if (handle->enable_gsa && strncmp(sentence, "$GPGSA", 6) == 0) {
    return parse_gpgsa(handle, sentence);
  }

  return ESP_OK;  // Ignore other sentences
}

esp_err_t star_sensor_gps6mv2_init(gps6mv2_handle_t* handle, const gps6mv2_config_t* config)
{
  if (handle == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  memset(handle, 0, sizeof(gps6mv2_handle_t));
  handle->uart_port = config->uart_port;
  handle->enable_gga = config->enable_gga;
  handle->enable_rmc = config->enable_rmc;
  handle->enable_gsa = config->enable_gsa;
  handle->enable_gsv = config->enable_gsv;

  esp_err_t ret = error_handler_init(&handle->error_handler, 3, 10, 100, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG,
           "Initializing GPS (UART=%d, TX=%d, RX=%d)",
           config->uart_port,
           config->tx_pin,
           config->rx_pin);

  // Configure UART
  uart_config_t uart_config = {
    .baud_rate = GPS6MV2_UART_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_APB,
  };

  ret = uart_param_config(config->uart_port, &uart_config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  ret = uart_set_pin(config->uart_port,
                     config->tx_pin,
                     config->rx_pin,
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  ret = uart_driver_install(config->uart_port, GPS6MV2_UART_BUF_SIZE, 0, 0, NULL, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "GPS initialization successful");

  return ESP_OK;
}

esp_err_t star_sensor_gps6mv2_deinit(gps6mv2_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uart_driver_delete(handle->uart_port);
  error_handler_deinit(&handle->error_handler);
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_sensor_gps6mv2_process(gps6mv2_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t data;
  int len = uart_read_bytes(handle->uart_port, &data, 1, 0);

  if (len <= 0) {
    return ESP_OK;  // No data available
  }

  // Process byte
  if (data == '$') {
    // Start of new sentence
    handle->nmea_index = 0;
    handle->nmea_buffer[handle->nmea_index++] = data;
  } else if (data == '\r' || data == '\n') {
    // End of sentence
    if (handle->nmea_index > 0) {
      handle->nmea_buffer[handle->nmea_index] = '\0';
      parse_nmea_sentence(handle, handle->nmea_buffer);
      handle->nmea_index = 0;
    }
  } else if (handle->nmea_index > 0) {
    // Accumulate sentence data
    if (handle->nmea_index < GPS6MV2_NMEA_MAX_LENGTH) {
      handle->nmea_buffer[handle->nmea_index++] = data;
    } else {
      // Buffer overflow, reset
      ESP_LOGW(s_TAG, "NMEA buffer overflow");
      handle->nmea_index = 0;
    }
  }

  return ESP_OK;
}

esp_err_t star_sensor_gps6mv2_get_data(const gps6mv2_handle_t* handle, gps6mv2_data_t* data)
{
  if (handle == NULL || data == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!handle->current_data.valid) {
    return ESP_ERR_INVALID_STATE;
  }

  *data = handle->current_data;
  return ESP_OK;
}

esp_err_t star_sensor_gps6mv2_has_fix(const gps6mv2_handle_t* handle, bool* has_fix)
{
  if (handle == NULL || has_fix == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  *has_fix = handle->current_data.valid;
  return ESP_OK;
}

esp_err_t star_sensor_gps6mv2_send_command(const gps6mv2_handle_t* handle, const char* command)
{
  if (handle == NULL || command == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  // Calculate and append checksum
  uint8_t checksum = calculate_nmea_checksum(command);
  char full_command[128];
  snprintf(full_command, sizeof(full_command), "%s*%02X\r\n", command, checksum);

  int len = uart_write_bytes(handle->uart_port, full_command, strlen(full_command));
  if (len < 0) {
    ESP_LOGE(s_TAG, "Failed to send command");
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t star_sensor_gps6mv2_calculate_distance(double  lat1,
                                                  double  lon1,
                                                  double  lat2,
                                                  double  lon2,
                                                  float* distance)
{
  if (distance == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Haversine formula
  double dlat = (lat2 - lat1) * DEG_TO_RAD;
  double dlon = (lon2 - lon1) * DEG_TO_RAD;
  double lat1_rad = lat1 * DEG_TO_RAD;
  double lat2_rad = lat2 * DEG_TO_RAD;

  double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
             cos(lat1_rad) * cos(lat2_rad) * sin(dlon / 2.0) * sin(dlon / 2.0);
  double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

  *distance = (float)(EARTH_RADIUS_M * c);

  return ESP_OK;
}

esp_err_t star_sensor_gps6mv2_get_satellites(const gps6mv2_handle_t* handle,
                                              gps6mv2_satellite_t*    satellites,
                                              uint8_t                 max_count,
                                              uint8_t*                count)
{
  if (handle == NULL || satellites == NULL || count == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t copy_count = (handle->satellite_count < max_count) ? handle->satellite_count : max_count;

  memcpy(satellites, handle->satellites, copy_count * sizeof(gps6mv2_satellite_t));
  *count = copy_count;

  return ESP_OK;
}
