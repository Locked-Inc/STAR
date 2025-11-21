# GY-GPS6MV2 (NEO-6M) GPS Module: Quick Reference & Code Examples
## ESP32-IDF Driver Implementation

---

## UART CONFIGURATION QUICK START

### Default Settings
```
Baud Rate: 9600 bps
Data Bits: 8
Parity: None
Stop Bits: 1
No Flow Control
```

### Minimum ESP32-IDF Setup
```c
#include "driver/uart.h"

void gps_uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_1, &cfg);
    uart_set_pin(UART_NUM_1, GPIO_NUM_17, GPIO_NUM_16,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, 1024, 512, 10, NULL, 0);
}
```

---

## NMEA SENTENCE FORMATS (REFERENCE)

### GGA - Position, Time, Fix Quality
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47

Fields:
  1. UTC Time: 123519 (12:35:19)
  2. Latitude: 4807.038 (48°07.038')
  3. N/S: North
  4. Longitude: 01131.000 (11°31.000')
  5. E/W: East
  6. Fix Quality: 1 (GPS fix)
  7. Satellites: 08
  8. HDOP: 0.9
  9. Altitude: 545.4 meters
  10. Alt Unit: M
  11. Geoid Sep: 46.9 M
```

### RMC - Position, Speed, Course, Date
```
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*3C

Fields:
  1. UTC Time: 123519
  2. Status: A (Active) / V (Void)
  3. Latitude: 4807.038
  4. N/S: North
  5. Longitude: 01131.000
  6. E/W: East
  7. Speed: 022.4 knots
  8. Track: 084.4 degrees
  9. Date: 230394 (23/03/94)
  10. Mag Var: 003.1
  11. Var Dir: W
```

### GSA - Dilution of Precision & Active Satellites
```
$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*30

Fields:
  1. Mode: A (Auto) / M (Manual)
  2. Fix Type: 1 (No fix), 2 (2D), 3 (3D)
  3-14. Satellite PRNs (up to 12)
  15. PDOP: 2.5
  16. HDOP: 1.3
  17. VDOP: 2.1
```

### Checksum Calculation
```
Checksum = XOR of all characters between $ and *
Transmitted as 2-digit hex: *XX

Example: $GPGGA,123519...*47
  Checksum of "GPGGA,123519..." = 0x47
```

---

## NMEA PARSING - SAFE IMPLEMENTATION

### Validation Function
```c
#include <string.h>
#include <stdlib.h>

bool validate_nmea_sentence(const char *sentence, size_t len)
{
    // Check length (max 82 chars including $ and <CR><LF>)
    if (len > 82 || len < 10) return false;

    // Check start character
    if (sentence[0] != '$') return false;

    // Find asterisk
    const char *ast = strchr(sentence, '*');
    if (!ast) return false;

    // Extract and verify checksum
    if (ast[1] && ast[2]) {
        char csum_hex[3] = {ast[1], ast[2], '\0'};
        uint8_t provided = (uint8_t)strtol(csum_hex, NULL, 16);

        // Calculate expected checksum
        uint8_t calculated = 0;
        for (const char *p = sentence + 1; p < ast; p++) {
            calculated ^= *p;
        }

        if (calculated != provided) {
            return false;  // Checksum mismatch
        }
    }

    return true;
}
```

### Safe Field Extraction
```c
typedef struct {
    char sentence[84];
    size_t length;
} nmea_sentence_t;

// Safe extraction of GGA data
typedef struct {
    float latitude;
    float longitude;
    float altitude;
    int fix_quality;
    int num_satellites;
} gps_gga_data_t;

bool parse_gga_safe(const nmea_sentence_t *sentence, gps_gga_data_t *data)
{
    if (!validate_nmea_sentence(sentence->sentence, sentence->length)) {
        return false;
    }

    // Create working copy to avoid modifying original
    char working[84];
    strncpy(working, sentence->sentence, 83);
    working[83] = '\0';

    // Remove checksum for parsing
    char *ast = strchr(working, '*');
    if (ast) *ast = '\0';

    // Parse fields with bounds checking
    char *token;
    char *saveptr;
    int field_num = 0;

    // Skip $ and sentence type
    token = strtok_r(working + 6, ",", &saveptr);

    while (token && field_num < 14) {
        if (*token == '\0') {
            // Empty field, skip
            token = strtok_r(NULL, ",", &saveptr);
            field_num++;
            continue;
        }

        switch (field_num + 1) {  // Adjust for skipped header
            case 2:  // Latitude
                data->latitude = atof(token);
                break;
            case 4:  // Longitude
                data->longitude = atof(token);
                break;
            case 6:  // Fix quality
                data->fix_quality = atoi(token);
                break;
            case 7:  // Number of satellites
                data->num_satellites = atoi(token);
                break;
            case 9:  // Altitude
                data->altitude = atof(token);
                break;
            default:
                break;
        }

        token = strtok_r(NULL, ",", &saveptr);
        field_num++;
    }

    return (data->fix_quality > 0);  // Valid if has fix
}
```

---

## UBX PROTOCOL - QUICK REFERENCE

### Message Structure
```
Byte 0-1:   0xB5 0x62 (Sync)
Byte 2:     Class
Byte 3:     ID
Byte 4-5:   Payload Length (little-endian)
Byte 6+:    Payload
Last 2:     Checksum (CK_A, CK_B)
```

### Fletcher Checksum
```c
void ubx_checksum(const uint8_t *data, size_t len,
                  uint8_t *ck_a, uint8_t *ck_b)
{
    *ck_a = 0;
    *ck_b = 0;
    for (size_t i = 0; i < len; i++) {
        *ck_a = (*ck_a + data[i]) & 0xFF;
        *ck_b = (*ck_b + *ck_a) & 0xFF;
    }
}
```

### Common Message Classes
| Class | Hex | Purpose |
|-------|-----|---------|
| NAV | 0x01 | Navigation results |
| ACK | 0x05 | Acknowledgement |
| CFG | 0x06 | Configuration |
| MON | 0x0A | Monitoring |

### Common Configuration Messages
```
CFG-PRT (0x06, 0x00) - Port settings (baud rate)
CFG-MSG (0x06, 0x01) - NMEA message enable/disable
CFG-RATE (0x06, 0x08) - Position update rate
CFG-NAV5 (0x06, 0x24) - Navigation engine
CFG-CFG (0x06, 0x09) - Save/load configuration
```

---

## UBX COMMAND EXAMPLES

### Build and Send UBX Message
```c
#include "driver/uart.h"

size_t ubx_build(uint8_t *buf, size_t max_len,
                 uint8_t cls, uint8_t id,
                 const uint8_t *payload, uint16_t plen)
{
    if (max_len < plen + 8) return 0;

    buf[0] = 0xB5;
    buf[1] = 0x62;
    buf[2] = cls;
    buf[3] = id;
    buf[4] = plen & 0xFF;
    buf[5] = (plen >> 8) & 0xFF;

    if (payload) {
        memcpy(&buf[6], payload, plen);
    }

    // Calculate checksum
    uint8_t ck_a = 0, ck_b = 0;
    for (uint16_t i = 0; i < plen + 2; i++) {
        ck_a += buf[2 + i];
        ck_b += ck_a;
    }
    ck_a &= 0xFF;
    ck_b &= 0xFF;

    buf[6 + plen] = ck_a;
    buf[7 + plen] = ck_b;

    return plen + 8;
}
```

### Change Baud Rate to 115200
```c
void set_gps_baud_115200(uart_port_t port)
{
    uint8_t payload[20] = {
        0x01,                    // Port ID (UART1)
        0x00, 0x00, 0x00,       // Reserved
        0x00, 0x00,             // Reserved
        0x00, 0x00,             // Reserved
        0x00, 0x00,             // Reserved
        0xD8, 0x00, 0x00, 0x00, // UART mode (8N1)
        0x00, 0xC2, 0x01, 0x00, // Baud: 115200 (little-endian)
        0x03, 0x00,             // In protocols
        0x03, 0x00              // Out protocols
    };

    uint8_t msg[32];
    size_t len = ubx_build(msg, 32, 0x06, 0x00, payload, 20);
    uart_write_bytes(port, (const char *)msg, len);
}
```

### Enable GGA Message
```c
void enable_gga(uart_port_t port)
{
    uint8_t payload[8] = {
        0xF0,           // NMEA class
        0x00,           // GGA message
        0x01, 0x00, 0x00, 0x00, // Output rates
        0x00, 0x00      // Reserved
    };

    uint8_t msg[32];
    size_t len = ubx_build(msg, 32, 0x06, 0x01, payload, 8);
    uart_write_bytes(port, (const char *)msg, len);
}
```

### Disable RMC Message
```c
void disable_rmc(uart_port_t port)
{
    uint8_t payload[8] = {
        0xF0,           // NMEA class
        0x04,           // RMC message
        0x00, 0x00, 0x00, 0x00, // Disabled
        0x00, 0x00      // Reserved
    };

    uint8_t msg[32];
    size_t len = ubx_build(msg, 32, 0x06, 0x01, payload, 8);
    uart_write_bytes(port, (const char *)msg, len);
}
```

---

## SECURITY: COMMON PITFALLS

### Pitfall 1: No Length Validation
```c
// WRONG - Can cause buffer overflow
char buffer[64];
sscanf(sentence, "$GPGGA,%s", buffer);  // No size limit!

// CORRECT
char buffer[64];
sscanf(sentence, "$GPGGA,%63s", buffer);  // Size specified
```

### Pitfall 2: Missing Checksum Validation
```c
// WRONG - Accepts forged sentences
void parse_any_sentence(const char *sentence)
{
    // Parse without checksum validation
    process_position(sentence);
}

// CORRECT
if (validate_nmea_sentence(sentence, strlen(sentence))) {
    process_position(sentence);  // Only process valid sentences
}
```

### Pitfall 3: Race Conditions in Position Updates
```c
// WRONG - TOCTOU vulnerability
if (gps_data.is_valid) {
    navigate(gps_data.lat, gps_data.lon);  // Data might change!
}

// CORRECT - Atomic read with mutex
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
typedef struct { float lat; float lon; bool valid; } pos_t;

pos_t get_position_safe(void)
{
    pos_t copy;
    xSemaphoreTake(mutex, portMAX_DELAY);
    copy = gps_data;  // Atomic copy
    xSemaphoreGive(mutex);
    return copy;
}

// Usage
pos_t pos = get_position_safe();
if (pos.valid) {
    navigate(pos.lat, pos.lon);  // Safe!
}
```

### Pitfall 4: Incomplete Sentence Handling
```c
// WRONG - Doesn't wait for complete sentence
void bad_parsing(void)
{
    uint8_t byte;
    if (uart_read_bytes(UART_NUM_1, &byte, 1, 0) > 0) {
        parse_single_byte(byte);  // Fragments!
    }
}

// CORRECT - Accumulate complete sentences
char sentence[84];
int pos = 0;

void good_parsing(void)
{
    uint8_t byte;
    while (uart_read_bytes(UART_NUM_1, &byte, 1, 0) > 0) {
        if (byte == '$') pos = 0;  // Start of sentence

        if (pos < 83) {
            sentence[pos++] = byte;
        }

        if (byte == '\n' && pos > 10) {
            sentence[pos] = '\0';
            if (validate_nmea_sentence(sentence, pos)) {
                parse_sentence(sentence);  // Complete, validated!
            }
            pos = 0;
        }
    }
}
```

---

## PERFORMANCE TIPS

### Ring Buffer Sizing
```c
// Typical: 1Hz update, ~82 bytes per sentence
// With margin for baud rate delays and interrupt latency
#define GPS_RX_BUFFER_SIZE 1024  // ~12 sentences
```

### Update Rate Configuration
```c
// Default: 1Hz (1000ms measurement rate)
// For higher rate (5Hz):
uint8_t rate_payload[6] = {
    0xC8, 0x00,  // 200ms measurement rate
    0x01, 0x00,  // 1 nav cycle
    0x00, 0x00   // UTC time reference
};
```

### Thread-Safe Position Access
```c
// Use FreeRTOS primitives for GPS data sharing
static SemaphoreHandle_t gps_mutex = NULL;
static gps_position_t current_position = {0};

void gps_task(void *arg)
{
    gps_position_t new_pos;
    while (1) {
        if (read_gps(&new_pos)) {
            xSemaphoreTake(gps_mutex, portMAX_DELAY);
            current_position = new_pos;
            xSemaphoreGive(gps_mutex);
        }
    }
}

bool app_get_position(gps_position_t *pos)
{
    if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(100))) {
        *pos = current_position;
        xSemaphoreGive(gps_mutex);
        return true;
    }
    return false;
}
```

---

## DEBUGGING

### Common Issues & Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| No data received | Wrong UART port/pins | Verify GPIO mapping |
| Garbled output | Baud rate mismatch | Default 9600 bps |
| Checksum failures | Sensor malfunction | Check UART wiring |
| FIFO overflow | Buffer too small | Increase buffer size |
| Incomplete sentences | ISR not responsive | Lower threshold value |

### Enable Logging
```c
#include "esp_log.h"

// Set tag and level
static const char *TAG = "GPS";
esp_log_level_set(TAG, ESP_LOG_DEBUG);  // See debug messages

// Usage
ESP_LOGI(TAG, "Sentence: %s", sentence);
ESP_LOGW(TAG, "Checksum failed");
ESP_LOGE(TAG, "UART error: %d", error_code);
```

---

## CONVERSION REFERENCE

### Latitude/Longitude Conversion
```c
// NMEA format: DDMM.MMMM
// Convert to decimal degrees
float nmea_to_decimal(const char *nmea_str)
{
    // Format: 4807.038 means 48°07'02.28"
    float value = atof(nmea_str);
    int degrees = (int)(value / 100.0f);
    float minutes = value - (degrees * 100.0f);
    return degrees + (minutes / 60.0f);
}

// Example: 4807.038 N = 48.11730°
```

### Knots to m/s
```c
float knots_to_ms(float knots)
{
    return knots * 0.51444f;  // 1 knot = 0.51444 m/s
}
```

### Knots to km/h
```c
float knots_to_kmh(float knots)
{
    return knots * 1.852f;  // 1 knot = 1.852 km/h
}
```

---

## TESTING SENTENCES

### Valid Test Sentences
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*3C
$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*30
```

### Invalid Test Sentences
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*FF  (Bad checksum)
$GPGGA,123519,invalid_lat...  (Malformed)
GPGGA,123519...  (Missing $)
```

---

**Version**: 1.0
**ESP-IDF Version**: v4.4+
**Standard**: C17
