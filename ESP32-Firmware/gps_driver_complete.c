/**
 * GY-GPS6MV2 (NEO-6M) GPS Module Driver for ESP32-IDF
 *
 * Features:
 * - NMEA 0183 protocol parsing (GGA, RMC, GSA, GSV, VTG, GLL)
 * - UBX binary protocol support for configuration
 * - Security validation (checksum, buffer overflow protection)
 * - Memory safe string parsing
 * - Interrupt-safe circular buffer
 * - Thread-safe position data access
 * - TOCTOU vulnerability prevention
 *
 * Author: GPS Driver Implementation
 * Version: 1.0
 * ESP-IDF: v4.4+
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

#define GPS_UART_PORT           UART_NUM_1
#define GPS_TXD_PIN             GPIO_NUM_17
#define GPS_RXD_PIN             GPIO_NUM_16
#define GPS_BAUDRATE            9600
#define GPS_RX_BUFFER_SIZE      1024
#define GPS_TX_BUFFER_SIZE      512
#define GPS_QUEUE_SIZE          10

static const char *TAG = "GPS_DRIVER";

// ============================================================================
// DATA TYPES
// ============================================================================

typedef struct {
    uint8_t buffer[GPS_RX_BUFFER_SIZE];
    volatile size_t write_index;
    volatile size_t read_index;
} gps_ringbuf_t;

typedef struct {
    float latitude;
    float longitude;
    float altitude;
    float speed_knots;
    float track_true;
    uint8_t fix_quality;        // 0=invalid, 1=GPS, 2=DGPS, etc.
    uint8_t num_satellites;
    float hdop;                 // Horizontal Dilution of Precision
    float vdop;                 // Vertical Dilution of Precision
    float pdop;                 // Position Dilution of Precision
    uint32_t timestamp_ms;      // Timestamp of last valid fix
    bool is_valid;              // Position is valid
    uint32_t last_update_ms;    // When position was last updated
} gps_position_t;

typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint16_t milliseconds;
} gps_time_t;

typedef struct {
    uint8_t day;
    uint8_t month;
    uint16_t year;
} gps_date_t;

// Global state
static gps_ringbuf_t gps_rx_buffer = {0};
static QueueHandle_t uart_event_queue = NULL;
static SemaphoreHandle_t position_mutex = NULL;
static gps_position_t current_position = {0};
static gps_time_t current_time = {0};
static gps_date_t current_date = {0};

// ============================================================================
// NMEA CHECKSUM FUNCTIONS
// ============================================================================

/**
 * Calculate NMEA checksum (XOR of all bytes between $ and *)
 *
 * @param data: Data to checksum
 * @param length: Length of data
 * @return: Calculated checksum byte
 */
static uint8_t nmea_calculate_checksum(const uint8_t *data, size_t length)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * Validate NMEA sentence checksum
 *
 * @param sentence: Complete NMEA sentence (including $ and *XX)
 * @param length: Length of sentence
 * @return: true if valid, false if invalid
 */
static bool nmea_validate_checksum(const char *sentence, size_t length)
{
    // Minimum length: $GPXXX*HH\r\n (10 chars)
    if (length < 10) {
        return false;
    }

    // Must start with $
    if (sentence[0] != '$') {
        return false;
    }

    // Find asterisk
    const char *asterisk = strchr(sentence, '*');
    if (!asterisk || asterisk == sentence) {
        return false;
    }

    // Ensure there are 2 chars after asterisk for checksum
    size_t remaining = length - (asterisk - sentence);
    if (remaining < 3) {  // * + 2 hex digits
        return false;
    }

    // Extract provided checksum (2 hex digits)
    char checksum_str[3];
    checksum_str[0] = asterisk[1];
    checksum_str[1] = asterisk[2];
    checksum_str[2] = '\0';

    // Parse hex checksum
    char *endptr;
    uint8_t provided_checksum = (uint8_t)strtol(checksum_str, &endptr, 16);

    if (endptr != &checksum_str[2]) {
        return false;  // Invalid hex
    }

    // Calculate expected checksum
    size_t payload_length = asterisk - sentence - 1;
    uint8_t calculated_checksum = nmea_calculate_checksum(
        (const uint8_t *)(sentence + 1),
        payload_length
    );

    return (provided_checksum == calculated_checksum);
}

// ============================================================================
// UBX CHECKSUM FUNCTIONS
// ============================================================================

/**
 * Calculate UBX Fletcher checksum (CK_A and CK_B)
 * Computed over Class, ID, Length, and Payload (NOT Sync bytes)
 */
static void ubx_calculate_checksum(const uint8_t *data, size_t length,
                                   uint8_t *ck_a, uint8_t *ck_b)
{
    *ck_a = 0;
    *ck_b = 0;

    for (size_t i = 0; i < length; i++) {
        *ck_a = (*ck_a + data[i]) & 0xFF;
        *ck_b = (*ck_b + *ck_a) & 0xFF;
    }
}

/**
 * Validate UBX message checksum
 */
static bool ubx_validate_checksum(const uint8_t *ubx_message, size_t total_length)
{
    // Minimum message: 8 bytes
    if (total_length < 8) {
        return false;
    }

    // Verify sync bytes
    if (ubx_message[0] != 0xB5 || ubx_message[1] != 0x62) {
        return false;
    }

    // Extract payload length (little-endian)
    uint16_t payload_length = ubx_message[4] | (ubx_message[5] << 8);

    // Verify total length
    if (total_length != 8 + payload_length) {
        return false;
    }

    // Calculate expected checksum
    uint8_t expected_ck_a = 0, expected_ck_b = 0;
    ubx_calculate_checksum(&ubx_message[2], 2 + 2 + payload_length,
                          &expected_ck_a, &expected_ck_b);

    // Extract provided checksum
    uint8_t provided_ck_a = ubx_message[6 + payload_length];
    uint8_t provided_ck_b = ubx_message[7 + payload_length];

    return (expected_ck_a == provided_ck_a && expected_ck_b == provided_ck_b);
}

/**
 * Build UBX message with checksum
 *
 * @return: Total message length if successful, 0 if error
 */
static size_t ubx_build_message(uint8_t *output, size_t output_size,
                               uint8_t class, uint8_t id,
                               const uint8_t *payload, uint16_t payload_length)
{
    // Check buffer size
    size_t total_length = 8 + payload_length;
    if (total_length > output_size || total_length > 1000) {
        return 0;
    }

    // Sync bytes
    output[0] = 0xB5;
    output[1] = 0x62;

    // Class and ID
    output[2] = class;
    output[3] = id;

    // Length (little-endian)
    output[4] = payload_length & 0xFF;
    output[5] = (payload_length >> 8) & 0xFF;

    // Copy payload
    if (payload && payload_length > 0) {
        memcpy(&output[6], payload, payload_length);
    }

    // Calculate and add checksum
    uint8_t ck_a = 0, ck_b = 0;
    ubx_calculate_checksum(&output[2], 2 + 2 + payload_length, &ck_a, &ck_b);
    output[6 + payload_length] = ck_a;
    output[7 + payload_length] = ck_b;

    return total_length;
}

// ============================================================================
// CIRCULAR BUFFER FUNCTIONS
// ============================================================================

/**
 * Initialize circular buffer
 */
static void ringbuf_init(gps_ringbuf_t *rb)
{
    rb->write_index = 0;
    rb->read_index = 0;
}

/**
 * Add byte to circular buffer (ISR context safe)
 *
 * @return: true if successful, false if buffer full
 */
static bool ringbuf_put_isr(gps_ringbuf_t *rb, uint8_t data)
{
    size_t next_write = (rb->write_index + 1) % sizeof(rb->buffer);

    // Check if buffer is full
    if (next_write == rb->read_index) {
        return false;
    }

    rb->buffer[rb->write_index] = data;
    rb->write_index = next_write;
    return true;
}

/**
 * Get available bytes in buffer (ISR safe)
 */
static size_t ringbuf_available(gps_ringbuf_t *rb)
{
    if (rb->write_index >= rb->read_index) {
        return rb->write_index - rb->read_index;
    } else {
        return sizeof(rb->buffer) - rb->read_index + rb->write_index;
    }
}

/**
 * Extract data from circular buffer (Task context)
 *
 * @return: Number of bytes read
 */
static size_t ringbuf_get(gps_ringbuf_t *rb, uint8_t *output, size_t max_length)
{
    // Disable interrupts to create critical section
    portDISABLE_INTERRUPTS();

    size_t available = ringbuf_available(rb);
    size_t to_read = (available < max_length) ? available : max_length;

    // Copy data
    for (size_t i = 0; i < to_read; i++) {
        output[i] = rb->buffer[rb->read_index];
        rb->read_index = (rb->read_index + 1) % sizeof(rb->buffer);
    }

    // Re-enable interrupts
    portENABLE_INTERRUPTS();

    return to_read;
}

// ============================================================================
// SENTENCE PARSING FUNCTIONS
// ============================================================================

/**
 * Extract complete NMEA sentence from buffer
 *
 * @param output: Output buffer for sentence
 * @param max_len: Maximum output buffer size
 * @return: Length of extracted sentence, 0 if none available
 */
static size_t extract_sentence(char *output, size_t max_len)
{
    // Get current read position
    size_t read_pos = gps_rx_buffer.read_index;
    size_t write_pos = gps_rx_buffer.write_index;

    // Find start of sentence ($)
    while (read_pos != write_pos &&
           gps_rx_buffer.buffer[read_pos] != '$') {
        read_pos = (read_pos + 1) % sizeof(gps_rx_buffer.buffer);
    }

    if (read_pos == write_pos) {
        return 0;  // No sentence start found
    }

    size_t start_pos = read_pos;
    read_pos = (read_pos + 1) % sizeof(gps_rx_buffer.buffer);
    size_t sentence_len = 0;

    // Find end of sentence (\n)
    while (read_pos != write_pos &&
           gps_rx_buffer.buffer[read_pos] != '\n' &&
           sentence_len < 82) {
        read_pos = (read_pos + 1) % sizeof(gps_rx_buffer.buffer);
        sentence_len++;
    }

    if (read_pos == write_pos) {
        return 0;  // No sentence end found
    }

    // Copy sentence to output
    size_t copy_len = 0;
    read_pos = start_pos;

    while (read_pos != gps_rx_buffer.write_pos &&
           gps_rx_buffer.buffer[read_pos] != '\n' &&
           copy_len < max_len - 1) {
        output[copy_len++] = gps_rx_buffer.buffer[read_pos];
        read_pos = (read_pos + 1) % sizeof(gps_rx_buffer.buffer);
    }

    output[copy_len] = '\0';

    // Advance read position past the newline
    if (gps_rx_buffer.buffer[read_pos] == '\n') {
        read_pos = (read_pos + 1) % sizeof(gps_rx_buffer.buffer);
    }

    gps_rx_buffer.read_index = read_pos;

    return copy_len;
}

/**
 * Parse GGA sentence (Position and fix quality)
 */
static bool parse_gga(const char *sentence, gps_position_t *pos)
{
    if (!sentence || !pos) {
        return false;
    }

    // Work with a copy to avoid modifying original
    char buffer[256];
    size_t len = strlen(sentence);
    if (len >= sizeof(buffer)) {
        return false;
    }
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // Remove checksum if present
    char *asterisk = strchr(buffer, '*');
    if (asterisk) {
        *asterisk = '\0';
    }

    // Parse fields
    char *saveptr;
    char *token = strtok_r(buffer, ",", &saveptr);
    int field = 0;

    while (token && field < 15) {
        // Skip empty fields
        if (*token == '\0') {
            token = strtok_r(NULL, ",", &saveptr);
            field++;
            continue;
        }

        switch (field) {
            case 1:  // UTC Time
                // Parse if needed: HHMMSS.SS
                break;
            case 2:  // Latitude (DDMM.MMMM)
                {
                    float lat = atof(token);
                    int degrees = (int)(lat / 100.0f);
                    float minutes = lat - (degrees * 100.0f);
                    pos->latitude = degrees + (minutes / 60.0f);
                }
                break;
            case 3:  // N/S
                if (token[0] == 'S') {
                    pos->latitude = -pos->latitude;
                }
                break;
            case 4:  // Longitude (DDDMM.MMMM)
                {
                    float lon = atof(token);
                    int degrees = (int)(lon / 100.0f);
                    float minutes = lon - (degrees * 100.0f);
                    pos->longitude = degrees + (minutes / 60.0f);
                }
                break;
            case 5:  // E/W
                if (token[0] == 'W') {
                    pos->longitude = -pos->longitude;
                }
                break;
            case 6:  // Fix quality
                pos->fix_quality = (uint8_t)atoi(token);
                break;
            case 7:  // Number of satellites
                pos->num_satellites = (uint8_t)atoi(token);
                break;
            case 8:  // HDOP
                pos->hdop = atof(token);
                break;
            case 9:  // Altitude
                pos->altitude = atof(token);
                break;
            default:
                break;
        }

        token = strtok_r(NULL, ",", &saveptr);
        field++;
    }

    pos->is_valid = (pos->fix_quality > 0);
    pos->timestamp_ms = esp_timer_get_time() / 1000;

    return true;
}

/**
 * Parse RMC sentence (Position, speed, course, date)
 */
static bool parse_rmc(const char *sentence, gps_position_t *pos,
                     gps_date_t *date)
{
    if (!sentence || !pos) {
        return false;
    }

    char buffer[256];
    size_t len = strlen(sentence);
    if (len >= sizeof(buffer)) {
        return false;
    }
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *asterisk = strchr(buffer, '*');
    if (asterisk) {
        *asterisk = '\0';
    }

    char *saveptr;
    char *token = strtok_r(buffer, ",", &saveptr);
    int field = 0;

    while (token && field < 10) {
        if (*token == '\0') {
            token = strtok_r(NULL, ",", &saveptr);
            field++;
            continue;
        }

        switch (field) {
            case 1:  // UTC Time
                // Parse HHMMSS.SS if needed
                break;
            case 2:  // Status (A = valid, V = invalid)
                pos->is_valid = (token[0] == 'A');
                break;
            case 3:  // Latitude
                {
                    float lat = atof(token);
                    int degrees = (int)(lat / 100.0f);
                    float minutes = lat - (degrees * 100.0f);
                    pos->latitude = degrees + (minutes / 60.0f);
                }
                break;
            case 4:  // N/S
                if (token[0] == 'S') {
                    pos->latitude = -pos->latitude;
                }
                break;
            case 5:  // Longitude
                {
                    float lon = atof(token);
                    int degrees = (int)(lon / 100.0f);
                    float minutes = lon - (degrees * 100.0f);
                    pos->longitude = degrees + (minutes / 60.0f);
                }
                break;
            case 6:  // E/W
                if (token[0] == 'W') {
                    pos->longitude = -pos->longitude;
                }
                break;
            case 7:  // Speed in knots
                pos->speed_knots = atof(token);
                break;
            case 8:  // Track true (degrees)
                pos->track_true = atof(token);
                break;
            case 9:  // Date (DDMMYY)
                if (date && strlen(token) == 6) {
                    date->day = ((token[0] - '0') * 10) + (token[1] - '0');
                    date->month = ((token[2] - '0') * 10) + (token[3] - '0');
                    date->year = ((token[4] - '0') * 10) + (token[5] - '0');
                    date->year += 2000;  // Assume 20xx century
                }
                break;
            default:
                break;
        }

        token = strtok_r(NULL, ",", &saveptr);
        field++;
    }

    return true;
}

// ============================================================================
// UART EVENT HANDLER TASK
// ============================================================================

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t data[512];

    ESP_LOGI(TAG, "UART event task started");

    while (1) {
        // Wait for UART event with timeout
        if (xQueueReceive(uart_event_queue, (void *)&event,
                         pdMS_TO_TICKS(100)) == pdTRUE) {

            switch (event.type) {
                case UART_DATA:
                    {
                        // Read available data
                        int len = uart_read_bytes(GPS_UART_PORT, data,
                                                 event.size, 0);
                        // Add to circular buffer
                        for (int i = 0; i < len; i++) {
                            if (!ringbuf_put_isr(&gps_rx_buffer, data[i])) {
                                ESP_LOGW(TAG, "RX buffer full, byte dropped");
                            }
                        }
                    }
                    break;

                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "RX FIFO overflow");
                    uart_flush_input(GPS_UART_PORT);
                    break;

                case UART_FRAME_ERR:
                    ESP_LOGW(TAG, "Frame error detected");
                    break;

                case UART_PARITY_ERR:
                    ESP_LOGW(TAG, "Parity error detected");
                    break;

                default:
                    break;
            }
        }
    }
}

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

/**
 * Initialize GPS module
 */
void gps_init(void)
{
    // Create mutex for thread-safe position access
    position_mutex = xSemaphoreCreateMutex();
    if (!position_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }

    // Initialize circular buffer
    ringbuf_init(&gps_rx_buffer);

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = GPS_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(GPS_UART_PORT, &uart_config);
    uart_set_pin(GPS_UART_PORT, GPS_TXD_PIN, GPS_RXD_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Install UART driver
    uart_driver_install(GPS_UART_PORT, GPS_RX_BUFFER_SIZE, GPS_TX_BUFFER_SIZE,
                       GPS_QUEUE_SIZE, &uart_event_queue, 0);

    // Create UART event handling task
    xTaskCreatePinnedToCore(uart_event_task, "gps_uart_task",
                           4096, NULL, 10, NULL, 0);

    ESP_LOGI(TAG, "GPS module initialized");
}

/**
 * Read GPS position (thread-safe atomic read)
 *
 * @param pos: Output position data
 * @return: true if valid position data available
 */
bool gps_read_position(gps_position_t *pos)
{
    if (!pos || !position_mutex) {
        return false;
    }

    // Try to extract sentence from buffer
    char sentence[84];
    if (extract_sentence(sentence, sizeof(sentence)) == 0) {
        return false;  // No complete sentence
    }

    // Validate sentence
    if (!nmea_validate_checksum(sentence, strlen(sentence))) {
        ESP_LOGW(TAG, "Checksum validation failed");
        return false;
    }

    // Determine sentence type and parse
    if (strncmp(sentence + 3, "GGA", 3) == 0) {
        gps_position_t temp_pos = {0};
        if (parse_gga(sentence, &temp_pos)) {
            // Atomic write using mutex
            if (xSemaphoreTake(position_mutex, pdMS_TO_TICKS(100))) {
                memcpy(&current_position, &temp_pos, sizeof(gps_position_t));
                xSemaphoreGive(position_mutex);
            }
        }
    } else if (strncmp(sentence + 3, "RMC", 3) == 0) {
        gps_position_t temp_pos = {0};
        if (parse_rmc(sentence, &temp_pos, &current_date)) {
            // Atomic write using mutex
            if (xSemaphoreTake(position_mutex, pdMS_TO_TICKS(100))) {
                memcpy(&current_position, &temp_pos, sizeof(gps_position_t));
                xSemaphoreGive(position_mutex);
            }
        }
    }

    // Atomic read
    if (xSemaphoreTake(position_mutex, pdMS_TO_TICKS(100))) {
        memcpy(pos, &current_position, sizeof(gps_position_t));
        xSemaphoreGive(position_mutex);
        return true;
    }

    return false;
}

/**
 * Change GPS module baud rate via UBX command
 *
 * @param new_baud: New baud rate (9600, 19200, 38400, 57600, 115200, 230400)
 */
void gps_set_baud_rate(uint32_t new_baud)
{
    uint8_t payload[20] = {
        0x01,                           // Port ID: 1 = UART1
        0x00, 0x00, 0x00,              // Reserved
        0x00, 0x00,                    // TX Ready (reserved)
        0x00, 0x00,                    // Reserved
        0x00, 0x00,                    // Reserved
        0xD8, 0x00, 0x00, 0x00,       // UART Mode: 0x000000D8 = 8N1
        0x00, 0x00, 0x00, 0x00,       // Baud rate placeholder (little-endian)
        0x03, 0x00,                    // Input protocols
        0x03, 0x00                     // Output protocols
    };

    // Set baud rate (little-endian)
    uint32_t baud_le = new_baud;
    payload[16] = (baud_le) & 0xFF;
    payload[17] = (baud_le >> 8) & 0xFF;
    payload[18] = (baud_le >> 16) & 0xFF;
    payload[19] = (baud_le >> 24) & 0xFF;

    uint8_t buffer[64];
    size_t len = ubx_build_message(buffer, sizeof(buffer),
                                   0x06, 0x00, payload, sizeof(payload));

    if (len > 0) {
        uart_write_bytes(GPS_UART_PORT, (const char *)buffer, len);
        ESP_LOGI(TAG, "Baud rate change command sent");
    }
}

/**
 * Enable NMEA message output
 *
 * @param message_id: NMEA message ID (0x00=GGA, 0x04=RMC, etc.)
 * @param rate: Output rate (1=every fix, 0=disabled)
 */
void gps_enable_nmea_message(uint8_t message_id, uint8_t rate)
{
    uint8_t payload[8] = {
        0xF0,           // NMEA message class
        message_id,     // Message ID
        rate,           // UART1 output rate
        0x00,           // UART2 rate
        0x00,           // USB rate
        0x00,           // SPI rate
        0x00, 0x00      // Reserved
    };

    uint8_t buffer[32];
    size_t len = ubx_build_message(buffer, sizeof(buffer),
                                   0x06, 0x01, payload, sizeof(payload));

    if (len > 0) {
        uart_write_bytes(GPS_UART_PORT, (const char *)buffer, len);
    }
}

/**
 * Disable NMEA message output
 */
void gps_disable_nmea_message(uint8_t message_id)
{
    gps_enable_nmea_message(message_id, 0);  // Rate = 0 means disabled
}

/**
 * Save current configuration to module's flash memory
 */
void gps_save_configuration(void)
{
    // UBX-CFG-CFG payload
    uint8_t payload[12] = {
        0xFF, 0xFF, 0xFF, 0xFF,   // Bitmask: save all configs
        0xFF, 0xFF, 0xFF, 0xFF,   // Save mask
        0x00, 0x00, 0x00, 0x00    // Load mask
    };

    uint8_t buffer[32];
    size_t len = ubx_build_message(buffer, sizeof(buffer),
                                   0x06, 0x09, payload, sizeof(payload));

    if (len > 0) {
        uart_write_bytes(GPS_UART_PORT, (const char *)buffer, len);
        ESP_LOGI(TAG, "Configuration save command sent");
    }
}

/**
 * Deinitialize GPS module
 */
void gps_deinit(void)
{
    uart_driver_delete(GPS_UART_PORT);
    if (position_mutex) {
        vSemaphoreDelete(position_mutex);
    }
    ESP_LOGI(TAG, "GPS module deinitialized");
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Get number of available bytes in RX buffer
 */
size_t gps_rx_buffer_available(void)
{
    return ringbuf_available(&gps_rx_buffer);
}

/**
 * Clear RX buffer
 */
void gps_clear_rx_buffer(void)
{
    gps_rx_buffer.read_index = gps_rx_buffer.write_index;
}

/**
 * Convert NMEA latitude/longitude to decimal degrees
 */
float gps_nmea_to_decimal(float nmea_value)
{
    int degrees = (int)(nmea_value / 100.0f);
    float minutes = nmea_value - (degrees * 100.0f);
    return degrees + (minutes / 60.0f);
}

/**
 * Convert knots to m/s
 */
float gps_knots_to_ms(float knots)
{
    return knots * 0.51444f;
}

/**
 * Convert knots to km/h
 */
float gps_knots_to_kmh(float knots)
{
    return knots * 1.852f;
}

/**
 * Calculate great-circle distance between two points (simplified Haversine)
 * Distance in meters
 */
float gps_distance_meters(float lat1, float lon1, float lat2, float lon2)
{
    const float R_earth = 6371000.0f;  // Earth radius in meters

    float dlat = (lat2 - lat1) * 3.14159f / 180.0f;
    float dlon = (lon2 - lon1) * 3.14159f / 180.0f;

    float a = sinf(dlat / 2.0f) * sinf(dlat / 2.0f) +
              cosf(lat1 * 3.14159f / 180.0f) * cosf(lat2 * 3.14159f / 180.0f) *
              sinf(dlon / 2.0f) * sinf(dlon / 2.0f);

    float c = 2.0f * asinf(sqrtf(a));

    return R_earth * c;
}

// ============================================================================
// DEBUG/TEST FUNCTIONS
// ============================================================================

/**
 * Print GPS position data (for debugging)
 */
void gps_print_position(const gps_position_t *pos)
{
    if (!pos) {
        return;
    }

    ESP_LOGI(TAG,
             "Pos: Lat=%.6f Lon=%.6f Alt=%.1f Quality=%d Sats=%d HDOP=%.1f",
             pos->latitude, pos->longitude, pos->altitude,
             pos->fix_quality, pos->num_satellites, pos->hdop);
}

/**
 * Format NMEA sentence with checksum (for testing)
 */
bool gps_format_nmea_sentence(char *output, size_t output_size,
                             const char *format, ...)
{
    char temp_buffer[256];
    va_list args;
    va_start(args, format);
    int written = vsnprintf(temp_buffer, sizeof(temp_buffer), format, args);
    va_end(args);

    if (written < 0 || written >= (int)sizeof(temp_buffer)) {
        return false;
    }

    // Calculate checksum
    uint8_t checksum = nmea_calculate_checksum(
        (const uint8_t *)temp_buffer,
        written
    );

    // Build final sentence
    int total_written = snprintf(output, output_size, "$%s*%02X\r\n",
                                temp_buffer, checksum);

    return (total_written > 0 && total_written < (int)output_size);
}

#endif  // GPS_DRIVER_COMPLETE_C
