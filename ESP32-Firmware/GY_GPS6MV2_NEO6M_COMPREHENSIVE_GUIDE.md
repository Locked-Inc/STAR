# GY-GPS6MV2 (NEO-6M) GPS Module: Comprehensive Technical Guide
## For C Driver Development on ESP32-IDF

---

## 1. MODULE OVERVIEW

### Device Specifications
- **Module**: GY-GPS6MV2 (u-blox NEO-6M)
- **Interface**: UART TTL (3.3V signaling)
- **Default Baud Rate**: 9600 bps
- **Supported Baud Rates**: 4800, 9600, 19200, 38400, 57600, 115200, 230400 bps
- **Update Rate**: 1-5 Hz (default 1 Hz)
- **Horizontal Accuracy**: 2.5m CEP (SBAS: 2.0m CEP)
- **Time-to-First-Fix**: Cold 32s, Warm 23s, Hot <1s
- **Operating Voltage**: 2.7V - 5.0V
- **Operating Current**: ~45mA
- **Protocols Supported**: NMEA 0183 (default), UBX binary

### Pinout
| Pin | Function |
|-----|----------|
| VCC | 3.3V/5.0V Power Supply |
| GND | Ground Reference |
| TXD | Serial Transmit (GPS -> MCU) |
| RXD | Serial Receive (MCU -> GPS) |
| PPS | Pulse-Per-Second (optional clock output) |

### Communication Specifications
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: None (hardware flow control optional for high speeds)
- **TX/RX Impedance**: 510 Ohms
- **Protocol Selection**: Software-configurable via UBX commands

---

## 2. UART PROTOCOL SPECIFICATIONS

### 2.1 Default Configuration
```
Baud Rate: 9600 bps
Data Format: 8N1 (8 bits, No parity, 1 stop bit)
Character Framing: Standard UART
```

### 2.2 UART Hardware Configuration Example (ESP32-IDF)
```c
#include "driver/uart.h"
#include "esp_log.h"

#define GPS_UART_PORT UART_NUM_1
#define GPS_TXD_PIN   GPIO_NUM_17
#define GPS_RXD_PIN   GPIO_NUM_16
#define GPS_BAUDRATE  9600

/**
 * Initialize UART for GPS module communication
 * Configures ring buffers, baud rate, and event handling
 */
void gps_uart_init(void)
{
    // UART configuration structure
    uart_config_t uart_config = {
        .baud_rate = GPS_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  // NEO-6M does not use flow control
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Apply configuration
    uart_param_config(GPS_UART_PORT, &uart_config);

    // Set pin assignments
    uart_set_pin(GPS_UART_PORT, GPS_TXD_PIN, GPS_RXD_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Install UART driver
    // RX buffer: 1024 bytes, TX buffer: 512 bytes
    // Event queue: 10 events
    uart_driver_install(GPS_UART_PORT, 1024, 512, 10, NULL, 0);
}
```

### 2.3 Baud Rate Switching Protocol

To change baud rate at runtime:

1. Send UBX-CFG-PRT message with new baud rate
2. Module acknowledges with UBX-ACK-ACK
3. Switch MCU UART to new baud rate
4. Optionally save configuration to flash with UBX-CFG-CFG

---

## 3. NMEA SENTENCE FORMATS (0183 PROTOCOL)

### 3.1 NMEA Sentence Structure

```
Format: $[TALKER][SENTENCE],[Field1],[Field2],...,[FieldN]*[CHECKSUM]<CR><LF>

Components:
  $ = Start delimiter (0x24)
  TALKER = 2 characters (e.g., "GP" for GPS)
  SENTENCE = 3 characters (identifier: GGA, RMC, GSA, GSV, VTG, GLL)
  Comma-separated fields with variable count
  * = Checksum delimiter (0x2A)
  CHECKSUM = 2 hex digits (XOR of all chars between $ and *)
  <CR><LF> = Carriage return + Line feed (0x0D 0x0A)

Maximum Length: 82 characters (including $ and <CR><LF>)
Maximum payload: 79 characters between $ and <CR><LF>
```

### 3.2 Supported NMEA Sentence Types

#### $GPGGA - Global Positioning System Fix Data
```
Format: $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x,xxxx*hh<CR><LF>

Fields:
  1. UTC Time (hhmmss.ss) - hours, minutes, seconds, decimal
  2. Latitude (llll.ll) - degrees and decimal minutes
  3. North/South indicator (N/S)
  4. Longitude (yyyyy.yy) - degrees and decimal minutes
  5. East/West indicator (E/W)
  6. Fix Quality (0=invalid, 1=GPS fix, 2=DGPS, 3=PPS, 4=RTK fixed, 5=RTK float, 6=estimated, 7=manual, 8=simulation)
  7. Number of Satellites (00-12)
  8. Horizontal Dilution of Precision (HDOP) - 0.5 to 99.9
  9. Altitude above mean sea level (meters)
  10. Units (M = meters)
  11. Geoid Separation (meters)
  12. Geoid Separation Units (M = meters)
  13. Age of differential data (seconds) - empty if not using DGPS
  14. Differential station ID (0000-4095) - empty if not using DGPS

Example: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
```

#### $GPRMC - Recommended Minimum Specific GPS/Transit Data
```
Format: $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a,m*hh<CR><LF>

Fields:
  1. UTC Time (hhmmss.ss)
  2. Status (A=active/valid, V=void/invalid)
  3. Latitude (llll.ll)
  4. North/South indicator (N/S)
  5. Longitude (yyyyy.yy)
  6. East/West indicator (E/W)
  7. Speed over ground (knots) - 0.0 to 999.9
  8. Track made good (degrees true) - 0.0 to 359.9
  9. Date (ddmmyy)
  10. Magnetic Variation (degrees) - 0.0 to 180.0
  11. Variation direction (E/W)
  12. Mode indicator (A=autonomous, D=differential, E=DR only) - NEO-6M: Mode field often absent

Example: $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W,A*3C
```

#### $GPGSA - GPS DOP and Active Satellites
```
Format: $GPGSA,a,x,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,xx,x.x,x.x,x.x*hh<CR><LF>

Fields:
  1. Selection Mode (M=manual, A=automatic)
  2. Fix Type (1=no fix, 2=2D fix, 3=3D fix)
  3-14. Satellite PRN numbers used (up to 12 satellites, empty fields for unused slots)
  15. Position Dilution of Precision (PDOP) - 0.5 to 99.9
  16. Horizontal Dilution of Precision (HDOP)
  17. Vertical Dilution of Precision (VDOP)

Example: $GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*30
```

#### $GPGSV - GPS Satellites in View
```
Format: $GPGSV,x,x,xx,xx,xx,xxx,xx,xx,xx,xxx,xx,xx,xx,xxx,xx,xx,xx,xxx,xx*hh<CR><LF>

Fields:
  1. Total number of GSV sentences
  2. Sentence number (1-3)
  3. Total number of satellites in view (00-12)
  For each satellite (up to 4 per sentence):
    4. Satellite PRN number
    5. Elevation angle (0-90 degrees)
    6. Azimuth angle (0-359 degrees)
    7. Signal-to-Noise Ratio (SNR) - 0-99 dB (empty if not tracking)

Example: $GPGSV,2,1,08,01,40,083,46,02,17,308,41,12,07,344,39,14,22,228,45*75
```

#### $GPVTG - Track Made Good and Ground Speed
```
Format: $GPVTG,x.x,T,x.x,M,x.x,N,x.x,K,a*hh<CR><LF>

Fields:
  1. Track made good (True) - 0.0 to 359.9 degrees
  2. T = True
  3. Track made good (Magnetic) - 0.0 to 359.9 degrees
  4. M = Magnetic
  5. Ground speed (knots)
  6. N = Knots
  7. Ground speed (km/h)
  8. K = Kilometers per hour
  9. Mode indicator (A=autonomous, D=differential, E=DR, N=not valid) - may be absent on NEO-6M

Example: $GPVTG,054.7,T,034.4,M,005.5,N,010.2,K,A*48
```

#### $GPGLL - Geographic Position - Latitude/Longitude
```
Format: $GPGLL,llll.ll,a,yyyyy.yy,a,hhmmss.ss,A,m*hh<CR><LF>

Fields:
  1. Latitude (llll.ll)
  2. North/South indicator (N/S)
  3. Longitude (yyyyy.yy)
  4. East/West indicator (E/W)
  5. UTC Time (hhmmss.ss)
  6. Status (A=active, V=void)
  7. Mode indicator (A=autonomous, D=differential, E=DR, N=not valid) - may be absent

Example: $GPGLL,4807.038,N,01131.000,E,123519,A,A*5C
```

### 3.3 NMEA Checksum Calculation

The checksum is the bitwise XOR of all characters between (and not including) the $ and * delimiters.

```c
/**
 * Calculate NMEA checksum (XOR of all bytes between $ and *)
 *
 * @param sentence: NMEA sentence (without $ and * characters)
 * @param length: Length of sentence string
 * @return: Calculated checksum as 2-digit hex string
 */
uint8_t nmea_calculate_checksum(const uint8_t *sentence, size_t length)
{
    uint8_t checksum = 0;

    // XOR all characters in the sentence
    for (size_t i = 0; i < length; i++) {
        checksum ^= sentence[i];
    }

    return checksum;
}

/**
 * Validate NMEA sentence checksum
 *
 * @param sentence: Complete NMEA sentence with checksum (e.g., "$GPGGA,...*3F")
 * @param length: Total length including $ and checksum
 * @return: true if checksum is valid, false otherwise
 */
bool nmea_validate_checksum(const char *sentence, size_t length)
{
    // Find asterisk position
    const char *asterisk = strchr(sentence, '*');
    if (!asterisk || asterisk == sentence) {
        return false;  // No asterisk or it's at the start
    }

    // Extract provided checksum (2 hex digits after asterisk)
    if (asterisk + 2 >= sentence + length) {
        return false;  // Not enough characters for checksum
    }

    char checksum_str[3];
    checksum_str[0] = asterisk[1];
    checksum_str[1] = asterisk[2];
    checksum_str[2] = '\0';

    // Parse provided checksum
    uint8_t provided_checksum = (uint8_t)strtol(checksum_str, NULL, 16);

    // Calculate expected checksum (skip the $ character)
    size_t payload_length = asterisk - sentence - 1;
    uint8_t calculated_checksum = nmea_calculate_checksum(
        (const uint8_t *)(sentence + 1),
        payload_length
    );

    return (provided_checksum == calculated_checksum);
}

/**
 * Format NMEA sentence with checksum
 *
 * @param output: Output buffer for complete sentence
 * @param output_size: Size of output buffer
 * @param format: Format string (without $ and *XX)
 * @return: true if successful, false if buffer too small
 */
bool nmea_format_sentence(char *output, size_t output_size,
                          const char *format, ...)
{
    char temp_buffer[256];
    va_list args;
    va_start(args, format);
    int written = vsnprintf(temp_buffer, sizeof(temp_buffer), format, args);
    va_end(args);

    if (written < 0 || written >= (int)sizeof(temp_buffer)) {
        return false;  // Format error or buffer too small
    }

    // Calculate checksum
    uint8_t checksum = nmea_calculate_checksum(
        (const uint8_t *)temp_buffer,
        written
    );

    // Build final sentence with $ and checksum
    int total_written = snprintf(output, output_size, "$%s*%02X\r\n",
                                 temp_buffer, checksum);

    return (total_written > 0 && total_written < (int)output_size);
}
```

### 3.4 NMEA Sentence Parsing Best Practices

#### Vulnerability: Buffer Overflow in Field Extraction

```c
/**
 * UNSAFE - Vulnerable to buffer overflow!
 * Do NOT use this pattern in production code
 */
void unsafe_nmea_parse(const char *sentence)
{
    char field[50];
    // If sentence has a field longer than 50 chars, buffer overflow occurs
    sscanf(sentence, "$GPGGA,%[^,]", field);  // DANGEROUS!
}

/**
 * SAFE - Bounded string operations
 * Use this pattern for production code
 */
void safe_nmea_parse(const char *sentence, size_t sentence_len)
{
    // Verify sentence length before processing
    if (sentence_len > 82) {
        // Malformed sentence - exceeds NMEA maximum length
        ESP_LOGW("GPS", "Sentence too long: %zu bytes", sentence_len);
        return;
    }

    // Use strnlen to safely get string length
    size_t actual_len = strnlen(sentence, 82);

    // Verify sentence structure
    if (sentence[0] != '$' || actual_len < 10) {
        ESP_LOGW("GPS", "Invalid sentence format");
        return;
    }

    // Find asterisk (checksum delimiter)
    const char *asterisk = strchr(sentence, '*');
    if (!asterisk) {
        ESP_LOGW("GPS", "Missing checksum");
        return;
    }

    // Validate checksum before parsing
    if (!nmea_validate_checksum(sentence, actual_len)) {
        ESP_LOGW("GPS", "Checksum validation failed");
        return;
    }

    // Safe field extraction using strtok_r
    char buffer[83];  // +1 for null terminator
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *saveptr;
    const char *field = strtok_r(buffer, ",", &saveptr);
    int field_index = 0;

    while (field && field_index < 20) {  // Limit field count
        // Remove asterisk and checksum from last field
        char *asterisk_in_field = strchr(field, '*');
        if (asterisk_in_field) {
            *asterisk_in_field = '\0';
        }

        // Process field with bounds checking
        if (field[0] != '\0') {
            // Field is not empty
            ESP_LOGD("GPS", "Field %d: %s", field_index, field);
        }

        field = strtok_r(NULL, ",", &saveptr);
        field_index++;
    }
}
```

---

## 4. UBX PROTOCOL SPECIFICATIONS

### 4.1 UBX Message Structure

UBX is a binary protocol used for module configuration and advanced communication.

```
Structure:
  [Sync1][Sync2][Class][ID][Length_LSB][Length_MSB][Payload][CK_A][CK_B]

  Bytes:
    Sync1 = 0xB5 (181)
    Sync2 = 0x62 (98)
    Class = 1 byte (message class identifier)
    ID = 1 byte (message type within class)
    Length = 2 bytes (little-endian, payload length)
    Payload = 0-65535 bytes (message data)
    CK_A = 1 byte (checksum byte A)
    CK_B = 1 byte (checksum byte B)

Total minimum: 8 bytes (sync + class + id + length + checksum)
Total maximum: 8 + 65535 = 65543 bytes
```

### 4.2 Message Classes

| Class | Value | Purpose |
|-------|-------|---------|
| NAV | 0x01 | Navigation results (position, speed, time) |
| RXM | 0x02 | Receiver manager (signal quality, raw measurements) |
| INF | 0x04 | Information (ASCII strings and debug output) |
| ACK | 0x05 | Acknowledge (ACK/NACK responses to commands) |
| CFG | 0x06 | Configuration (set/get module parameters) |
| MON | 0x0A | Monitoring (module status and diagnostics) |
| AID | 0x0B | Aiding (assist with signal acquisition) |
| TIM | 0x0D | Timing (time reference and pulsing) |

### 4.3 Common Configuration Messages (CFG Class: 0x06)

#### UBX-CFG-PRT (0x06, 0x00) - Port Configuration

Used to set serial port parameters (baud rate, protocol, input/output settings).

```
Payload Format:
  Byte 0-3: Port ID (0=I2C, 1=UART1, 2=USB, 3=SPI, 4=UART2)
  Byte 4-5: Reserved
  Byte 6-7: TX Ready (Bitmask)
  Byte 8-11: In Protocol Mask (Bitmask for input protocols)
  Byte 12-15: Out Protocol Mask (Bitmask for output protocols)
  Byte 16-19: Flags (reserved)
  Byte 20-23: Reserved

For UART1 (Port ID = 1), additional fields:
  Byte 4-7: Reserved
  Byte 8-11: UART Mode (0x000 = disabled, 0x0D8 = 8N1)
  Byte 12-15: UART Baud Rate (little-endian, 9600 = 0x2580)
  Byte 16-17: Input Protocol Mask
  Byte 18-19: Output Protocol Mask
  Byte 20-23: Flags
  Byte 24-27: Reserved
```

Baud Rate Values:
- 4800 = 0x12C0
- 9600 = 0x2580
- 19200 = 0x4B00
- 38400 = 0x9600
- 57600 = 0xE100
- 115200 = 0xC200
- 230400 = 0x8401

Protocol Masks:
- 0x01 = UBX binary protocol
- 0x02 = NMEA protocol
- 0x04 = RTCM protocol (not supported on NEO-6M)

#### UBX-CFG-MSG (0x06, 0x01) - Message Enable/Disable

Controls which NMEA or UBX sentences the module outputs.

```
Payload Format:
  Byte 0: Message Class (0xF0=NMEA, 0x01=NAV, etc.)
  Byte 1: Message ID
  Byte 2: Output rate on UART1 (0=disabled, 1-255=rate in position fixes)
  Byte 3: Output rate on UART2 (typically 0)
  Byte 4: Output rate on USB (typically 0)
  Byte 5: Output rate on SPI (typically 0)
  Byte 6: Reserved
  Byte 7: Reserved

NMEA Message Classes (Class = 0xF0):
  ID 0x00 = $GPGGA (GGA)
  ID 0x01 = $GPRMC (GLL - Geographic Position)
  ID 0x02 = $GPGSA (GSA - DOP and Active Satellites)
  ID 0x03 = $GPGSV (GSV - Satellites in View)
  ID 0x04 = $GPRMC (RMC - Recommended Minimum)
  ID 0x05 = $GPVTG (VTG - Track and Speed)

Example to enable GPGGA every fix on UART1:
  0xF0, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
```

#### UBX-CFG-RATE (0x06, 0x08) - Navigation/Measurement Rate

Controls the position update frequency.

```
Payload Format:
  Byte 0-1: Measurement Rate (little-endian milliseconds)
  Byte 2-3: Navigation Rate (in measurement cycles)
  Byte 4-5: Time Reference (0=UTC, 1=GPS, 2=GLONASS, 5=SBAS)

Example for 1Hz (1000ms) update rate:
  0xE8, 0x03 (1000), 0x01, 0x00 (1 cycle), 0x00, 0x00 (UTC time)

Example for 5Hz (200ms) update rate:
  0xC8, 0x00 (200), 0x01, 0x00 (1 cycle), 0x00, 0x00 (UTC time)
```

#### UBX-CFG-NAV5 (0x06, 0x24) - Navigation Engine Settings

Configures the navigation engine (dimension mode, altitude hold, etc.).

```
Payload Format:
  Byte 0-1: Mask (bit 0=dynamic model, bit 2=fix mode)
  Byte 2: Dynamic Platform Model (0=portable, 2=stationary, 3=pedestrian, etc.)
  Byte 3: Fix Mode (1=2D only, 2=3D only, 3=auto 2D/3D)
  Byte 4-7: Fixed Altitude (2D mode) in meters
  Byte 8-11: Fixed Altitude Variance (2D mode)
  Byte 12-15: Min Elevation (degrees)
  Byte 16-19: P/DR Dead Reckoning timeout (seconds)
  Byte 20-23: Track turn rate threshold (degrees/second)
  Byte 24-27: Turn rate hysteresis (degrees)
```

#### UBX-CFG-CFG (0x06, 0x09) - Configuration (Save/Load/Clear)

Saves or loads configuration from module's non-volatile memory.

```
Payload Format:
  Byte 0-3: Bitmask (bit 0=ioPort, bit 1=msgOut, bit 2=infMsg, bit 3=navBuf,
                      bit 4=rxmBuf, bit 5=senConfig, bit 6=rinvDel, bit 7=antSet)
  Byte 4-7: Save Mask (what to save to flash)
  Byte 8-11: Load Mask (what to load from flash)
  Byte 12-15: Device Mask (bit 0=flash, bit 1=EEPROM, bit 2=SRAMBackup)

Example to save current configuration to flash:
  0xFF, 0xFF, 0xFF, 0xFF (all configs), 0xFF, 0xFF, 0xFF, 0xFF (save all),
  0x00, 0x00, 0x00, 0x00 (don't load), 0x07, 0x00, 0x00, 0x00 (flash+EEPROM+SRAM)
```

### 4.4 Fletcher Checksum (CK_A and CK_B)

UBX uses an 8-bit Fletcher checksum (two 8-bit accumulators CK_A and CK_B).

```c
/**
 * Calculate UBX Fletcher checksum (CK_A and CK_B)
 *
 * The checksum is computed over the Class, ID, Length, and Payload fields.
 * NOT over the Sync bytes (0xB5, 0x62).
 *
 * @param data: Pointer to start of Class field (after sync bytes)
 * @param length: Length of data to checksum (class + id + length + payload)
 * @param ck_a: Output parameter for CK_A checksum
 * @param ck_b: Output parameter for CK_B checksum
 */
void ubx_checksum(const uint8_t *data, size_t length,
                  uint8_t *ck_a, uint8_t *ck_b)
{
    *ck_a = 0;
    *ck_b = 0;

    for (size_t i = 0; i < length; i++) {
        *ck_a = (*ck_a + data[i]) & 0xFF;  // Keep as 8-bit value
        *ck_b = (*ck_b + *ck_a) & 0xFF;    // Keep as 8-bit value
    }
}

/**
 * Validate UBX message checksum
 *
 * @param ubx_message: Complete UBX message including sync bytes
 * @param total_length: Total message length (8 + payload_length)
 * @return: true if checksum is valid, false otherwise
 */
bool ubx_validate_checksum(const uint8_t *ubx_message, size_t total_length)
{
    // Minimum message: 8 bytes (sync + class + id + length + checksum)
    if (total_length < 8) {
        return false;
    }

    // Verify sync bytes
    if (ubx_message[0] != 0xB5 || ubx_message[1] != 0x62) {
        return false;
    }

    // Extract payload length (little-endian at bytes 4-5)
    uint16_t payload_length = ubx_message[4] | (ubx_message[5] << 8);

    // Verify total length matches expected
    if (total_length != 8 + payload_length) {
        return false;
    }

    // Calculate expected checksum over Class, ID, Length, and Payload
    uint8_t expected_ck_a = 0, expected_ck_b = 0;
    ubx_checksum(&ubx_message[2], 2 + 2 + payload_length,
                 &expected_ck_a, &expected_ck_b);

    // Extract provided checksum
    uint8_t provided_ck_a = ubx_message[6 + payload_length];
    uint8_t provided_ck_b = ubx_message[7 + payload_length];

    return (expected_ck_a == provided_ck_a && expected_ck_b == provided_ck_b);
}

/**
 * Build UBX message with checksum
 *
 * @param output: Output buffer for complete message
 * @param output_size: Size of output buffer
 * @param class: Message class
 * @param id: Message ID
 * @param payload: Payload data (NULL if no payload)
 * @param payload_length: Length of payload
 * @return: Total message length if successful, 0 if error
 */
size_t ubx_build_message(uint8_t *output, size_t output_size,
                         uint8_t class, uint8_t id,
                         const uint8_t *payload, uint16_t payload_length)
{
    // Check buffer size
    size_t total_length = 8 + payload_length;
    if (total_length > output_size) {
        return 0;  // Buffer too small
    }

    // Add sync bytes
    output[0] = 0xB5;
    output[1] = 0x62;

    // Add class and ID
    output[2] = class;
    output[3] = id;

    // Add length (little-endian)
    output[4] = payload_length & 0xFF;
    output[5] = (payload_length >> 8) & 0xFF;

    // Copy payload
    if (payload && payload_length > 0) {
        memcpy(&output[6], payload, payload_length);
    }

    // Calculate and add checksum
    uint8_t ck_a = 0, ck_b = 0;
    ubx_checksum(&output[2], 2 + 2 + payload_length, &ck_a, &ck_b);
    output[6 + payload_length] = ck_a;
    output[7 + payload_length] = ck_b;

    return total_length;
}
```

### 4.5 UBX-ACK Messages (Acknowledgement/Negative Acknowledgement)

Responses to configuration commands:

```
UBX-ACK-ACK (Class 0x05, ID 0x01):
  Payload: 2 bytes (Class of message acknowledged, ID of message acknowledged)
  Indicates: Command executed successfully

UBX-ACK-NACK (Class 0x05, ID 0x00):
  Payload: 2 bytes (Class of message not acknowledged, ID)
  Indicates: Command failed or invalid
```

---

## 5. SECURITY CONSIDERATIONS

### 5.1 NMEA Parsing Vulnerabilities

#### Vulnerability 1: Buffer Overflow in Fixed-Size Arrays

```c
/**
 * VULNERABLE CODE - DO NOT USE IN PRODUCTION
 */
typedef struct {
    float latitude;
    float longitude;
    char fix_type[10];  // TOO SMALL!
} gps_data_vulnerable_t;

void vulnerable_parse_gga(const char *sentence)
{
    gps_data_vulnerable_t data;
    // If fix_type field exceeds 10 chars (including null terminator),
    // buffer overflow occurs
    sscanf(sentence, "$GPGGA,%f,%f,%9[^,]",
           &data.latitude, &data.longitude, data.fix_type);  // Still risky!
}

/**
 * SAFE CODE - PRODUCTION READY
 */
typedef struct {
    float latitude;
    float longitude;
    char fix_type[32];  // Reasonable size
    size_t fix_type_len;
} gps_data_safe_t;

bool safe_parse_gga(const char *sentence, size_t sentence_len,
                    gps_data_safe_t *data)
{
    // 1. Length validation
    if (sentence_len > 82) {
        ESP_LOGE("GPS", "Sentence exceeds max length: %zu", sentence_len);
        return false;
    }

    // 2. Checksum validation
    if (!nmea_validate_checksum(sentence, sentence_len)) {
        ESP_LOGE("GPS", "Checksum failed");
        return false;
    }

    // 3. Sentence format verification
    if (strncmp(sentence, "$GPGGA", 6) != 0) {
        ESP_LOGE("GPS", "Not a GGA sentence");
        return false;
    }

    // 4. Safe field extraction with bounded operations
    char buffer[83];
    if (strnlen(sentence, 83) >= 83) {
        return false;  // Over maximum length
    }
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // 5. Parse using strtok_r (thread-safe)
    char *saveptr;
    char *token = strtok_r(buffer, ",", &saveptr);
    int field_num = 0;

    while (token && field_num < 15) {
        // Remove asterisk from last field
        size_t token_len = strlen(token);
        if (token_len > 0 && token[token_len - 1] == '*') {
            token[token_len - 1] = '\0';
            // Note: This is simplified; real code would extract checksum
        }

        // Use bounds-checked string copy
        if (field_num == 6) {  // Fix type field
            strncpy(data->fix_type, token, sizeof(data->fix_type) - 1);
            data->fix_type[sizeof(data->fix_type) - 1] = '\0';
            data->fix_type_len = strlen(data->fix_type);
        }

        token = strtok_r(NULL, ",", &saveptr);
        field_num++;
    }

    return true;
}
```

#### Vulnerability 2: Checksum Bypass

```c
/**
 * Checksum validation prevents malformed or injected sentences
 *
 * Attack: Attacker injects NMEA sentence with false position
 * Defense: Always validate checksum before processing
 */
bool is_checksum_valid(const char *sentence)
{
    // This function MUST be called before any position use
    return nmea_validate_checksum(sentence, strlen(sentence));
}
```

#### Vulnerability 3: Incomplete Sentences / FIFO Overflow

```c
/**
 * UART FIFO can overflow if sentences are processed too slowly
 * Solution: Use adequate buffer sizes and timely processing
 */
#define GPS_RX_BUFFER_SIZE 2048  // Should handle ~20-24 typical sentences

/**
 * Check for buffer overflow condition
 */
bool check_uart_fifo_overflow(uart_port_t uart_num)
{
    size_t buffered = 0;
    uart_get_buffered_data_len(uart_num, &buffered);

    // Monitor if we're approaching buffer limits
    if (buffered > (GPS_RX_BUFFER_SIZE * 90 / 100)) {
        ESP_LOGW("GPS", "UART buffer near capacity: %zu/%d bytes",
                 buffered, GPS_RX_BUFFER_SIZE);
        return true;
    }
    return false;
}
```

### 5.2 Time-of-Check Time-of-Use (TOCTOU) Vulnerabilities

Position data can be altered between validation and use.

```c
/**
 * VULNERABLE PATTERN - Validation and use are separate
 */
gps_position_t position;
bool position_valid = false;

void vulnerable_pattern(void)
{
    // Check: Validate position
    if (is_position_valid(&position)) {
        position_valid = true;
    }

    // ... time passes ...
    // ... other tasks run ...
    // ... position might be updated by GPS task ...

    // Use: Rely on position
    if (position_valid) {
        // TOCTOU RACE CONDITION!
        // Position may have been updated since validation
        navigate_to(position.latitude, position.longitude);
    }
}

/**
 * SAFE PATTERN - Atomic read with validation
 */
typedef struct {
    float latitude;
    float longitude;
    uint32_t timestamp;
    bool is_valid;
} gps_position_safe_t;

// Protect position with mutex
static SemaphoreHandle_t position_mutex = NULL;

/**
 * Atomic read of position data
 * Returns snapshot that cannot change after read
 */
bool safe_get_position(gps_position_safe_t *pos_out)
{
    // Use mutex to ensure atomic read
    if (xSemaphoreTake(position_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(pos_out, &position, sizeof(gps_position_safe_t));
        xSemaphoreGive(position_mutex);
        return true;
    }
    return false;
}

/**
 * Safe navigation with atomic position read
 */
void safe_pattern(void)
{
    gps_position_safe_t pos;

    // Atomic check and copy
    if (safe_get_position(&pos) && pos.is_valid) {
        // Position is guaranteed to be valid and unchanged
        // during the navigate_to() call
        navigate_to(pos.latitude, pos.longitude);
    }
}
```

### 5.3 GPS Spoofing and Injection Detection

```c
/**
 * Spoofing detection: Monitor for unrealistic position changes
 */
typedef struct {
    float last_latitude;
    float last_longitude;
    uint32_t last_update_time;
    float max_speed_knots;  // Maximum realistic speed
} gps_spoof_detector_t;

/**
 * Check for unrealistic position jumps (spoofing indicator)
 *
 * Calculation based on haversine formula for great-circle distance
 */
bool is_position_realistic(gps_spoof_detector_t *detector,
                          float latitude, float longitude,
                          uint32_t current_time)
{
    if (!detector->last_update_time) {
        // First update, accept it
        detector->last_latitude = latitude;
        detector->last_longitude = longitude;
        detector->last_update_time = current_time;
        return true;
    }

    // Calculate time difference in hours
    uint32_t time_diff_ms = current_time - detector->last_update_time;
    float time_diff_hours = time_diff_ms / (3600.0f * 1000.0f);

    // Calculate great-circle distance (simplified)
    // Real implementation would use haversine formula
    float lat_diff = latitude - detector->last_latitude;
    float lon_diff = longitude - detector->last_longitude;
    float distance_degrees = sqrt(lat_diff * lat_diff + lon_diff * lon_diff);

    // Roughly 1 degree = 69 nautical miles (simplified)
    float distance_nm = distance_degrees * 69.0f;

    // Calculate implied speed
    float implied_speed = time_diff_hours > 0 ?
        (distance_nm / time_diff_hours) : 0;

    // Check against maximum realistic speed
    if (implied_speed > detector->max_speed_knots) {
        ESP_LOGW("GPS", "Unrealistic speed detected: %.1f knots", implied_speed);
        return false;  // Likely spoofing
    }

    // Update baseline
    detector->last_latitude = latitude;
    detector->last_longitude = longitude;
    detector->last_update_time = current_time;
    return true;
}
```

### 5.4 Protocol Selection Validation

```c
/**
 * Ensure module operates with expected protocol
 * Prevents accidental UBX command injection in NMEA mode
 */
typedef enum {
    GPS_PROTOCOL_NMEA = 0x02,
    GPS_PROTOCOL_UBX = 0x01,
    GPS_PROTOCOL_BOTH = 0x03
} gps_protocol_t;

/**
 * Verify that module outputs expected protocol
 */
bool verify_gps_protocol(uart_port_t uart_num, gps_protocol_t expected)
{
    // Send UBX-MON-VER to get version/protocol info
    uint8_t ubx_mon_ver[] = {0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00, 0x0E, 0x34};

    // Send command and wait for response
    uart_write_bytes(uart_num, (const char *)ubx_mon_ver, sizeof(ubx_mon_ver));

    // Parse response to verify protocol settings
    // Implementation details depend on response parsing

    return true;  // Simplified
}
```

---

## 6. MEMORY SAFETY IMPLEMENTATION

### 6.1 Circular Buffer for NMEA Sentence Reception

```c
/**
 * Circular buffer for UART data with interrupt safety
 * Uses volatile to prevent compiler optimizations
 */
typedef struct {
    uint8_t buffer[2048];
    volatile size_t write_index;
    volatile size_t read_index;
    size_t size;
} circular_buffer_t;

/**
 * Initialize circular buffer
 */
void circular_buffer_init(circular_buffer_t *cb)
{
    cb->write_index = 0;
    cb->read_index = 0;
    cb->size = sizeof(cb->buffer);
}

/**
 * Add byte to circular buffer (ISR context)
 * Returns true if successful, false if full
 */
bool circular_buffer_put_isr(circular_buffer_t *cb, uint8_t data)
{
    size_t next_write = (cb->write_index + 1) % cb->size;

    // Check if buffer is full
    if (next_write == cb->read_index) {
        return false;  // Buffer full
    }

    cb->buffer[cb->write_index] = data;
    cb->write_index = next_write;

    return true;
}

/**
 * Extract data from circular buffer (Task context)
 * Returns number of bytes read, 0 if empty
 */
size_t circular_buffer_get(circular_buffer_t *cb, uint8_t *output,
                          size_t max_length)
{
    // Disable interrupt to create critical section
    portDISABLE_INTERRUPTS();

    size_t available = 0;
    if (cb->write_index >= cb->read_index) {
        available = cb->write_index - cb->read_index;
    } else {
        available = cb->size - cb->read_index + cb->write_index;
    }

    size_t to_read = (available < max_length) ? available : max_length;

    // Copy data
    for (size_t i = 0; i < to_read; i++) {
        output[i] = cb->buffer[cb->read_index];
        cb->read_index = (cb->read_index + 1) % cb->size;
    }

    // Re-enable interrupt
    portENABLE_INTERRUPTS();

    return to_read;
}

/**
 * Get number of available bytes (ISR safe)
 */
size_t circular_buffer_available(circular_buffer_t *cb)
{
    if (cb->write_index >= cb->read_index) {
        return cb->write_index - cb->read_index;
    } else {
        return cb->size - cb->read_index + cb->write_index;
    }
}
```

### 6.2 UART ISR with Pattern Detection

```c
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Event queue for UART events
static QueueHandle_t uart_queue = NULL;

/**
 * UART interrupt handler (runs in ISR context)
 * Detects NMEA sentence terminators (\n)
 */
static void IRAM_ATTR uart_event_task(void *pvParameters)
{
    uart_event_t event;

    while (1) {
        // Wait for UART event (with timeout to allow cleanup)
        if (xQueueReceive(uart_queue, (void * )&event, pdMS_TO_TICKS(100))) {
            switch (event.type) {
                case UART_DATA:
                    // Data received in RX buffer
                    ESP_LOGD("UART", "Data event, length: %d", event.size);
                    // Handle data processing
                    break;

                case UART_FIFO_OVF:
                    // RX FIFO overflow detected
                    ESP_LOGW("UART", "RX FIFO overflow!");
                    uart_flush_input(GPS_UART_PORT);
                    break;

                case UART_PATTERN_DET:
                    // Pattern detected (sentence terminator)
                    ESP_LOGD("UART", "Pattern detected");
                    // Read complete sentence and parse
                    break;

                case UART_FRAME_ERR:
                    ESP_LOGW("UART", "Frame error");
                    uart_flush_input(GPS_UART_PORT);
                    break;

                case UART_PARITY_ERR:
                    ESP_LOGW("UART", "Parity error");
                    uart_flush_input(GPS_UART_PORT);
                    break;

                default:
                    ESP_LOGD("UART", "Unknown event type: %d", event.type);
                    break;
            }
        }
    }
}

/**
 * Initialize UART with pattern detection for NMEA sentences
 */
void gps_uart_init_with_pattern(void)
{
    uart_config_t uart_config = {
        .baud_rate = GPS_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Apply configuration
    uart_param_config(GPS_UART_PORT, &uart_config);

    // Set pin assignments
    uart_set_pin(GPS_UART_PORT, GPS_TXD_PIN, GPS_RXD_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Install driver with appropriate buffer sizes
    uart_driver_install(GPS_UART_PORT, 1024, 512, 20, &uart_queue, 0);

    // Configure pattern detection for newline character (NMEA sentence terminator)
    uart_enable_pattern_det_intr(GPS_UART_PORT, '\n', 1,
                                 10000,  // tout_thr (timeout threshold)
                                 10,     // post_idle_ticks
                                 5);     // pre_idle_ticks

    // Configure RX interrupt threshold
    uart_intr_config_t uart_intr = {
        .intr_enable_mask = UART_INTR_RXFIFO_FULL |
                           UART_INTR_RXFIFO_TOUT |
                           UART_INTR_FRAM_ERR |
                           UART_INTR_PARITY_ERR,
        .rxfifo_full_thresh = 120,      // Trigger at 120 bytes
        .rx_timeout_thresh = 10,         // Characters to timeout
        .txfifo_empty_intr_thresh = 10,
    };
    uart_intr_config(GPS_UART_PORT, &uart_intr);

    // Create task to handle UART events
    xTaskCreatePinnedToCore(uart_event_task, "uart_event_task",
                           4096, NULL, 12, NULL, 0);
}
```

### 6.3 Safe NMEA Sentence Reader

```c
/**
 * Read complete NMEA sentence from UART with safety checks
 */
typedef struct {
    char sentence[84];  // 82 max + null terminator + extra byte
    size_t length;
} nmea_sentence_t;

/**
 * Read and validate complete NMEA sentence
 *
 * Returns: Number of bytes read, 0 if no complete sentence available
 */
size_t gps_read_sentence(nmea_sentence_t *sentence_out)
{
    uint8_t buffer[128];
    size_t bytes_read = uart_read_bytes(GPS_UART_PORT, buffer,
                                        sizeof(buffer) - 1, 0);

    if (bytes_read == 0) {
        return 0;
    }

    // Ensure null termination
    buffer[bytes_read] = '\0';

    // Find complete NMEA sentence (starts with $, ends with \r\n)
    const char *start = (const char *)buffer;
    const char *end = strchr(start, '\n');

    if (!end) {
        ESP_LOGD("GPS", "Incomplete sentence");
        return 0;  // No complete sentence
    }

    // Calculate sentence length
    size_t sentence_len = (end - start);

    // Validate length
    if (sentence_len > 82) {
        ESP_LOGW("GPS", "Sentence too long: %zu bytes", sentence_len);
        return 0;
    }

    if (sentence_len < 10) {
        ESP_LOGW("GPS", "Sentence too short: %zu bytes", sentence_len);
        return 0;
    }

    // Check format (must start with $)
    if (start[0] != '$') {
        ESP_LOGW("GPS", "Invalid sentence start character");
        return 0;
    }

    // Remove \r\n from end
    size_t actual_len = sentence_len;
    if (actual_len > 0 && start[actual_len - 1] == '\r') {
        actual_len--;
    }
    if (actual_len > 0 && start[actual_len - 1] == '\n') {
        actual_len--;
    }

    // Copy to output buffer with bounds checking
    if (actual_len >= sizeof(sentence_out->sentence)) {
        ESP_LOGW("GPS", "Sentence buffer too small");
        return 0;
    }

    strncpy(sentence_out->sentence, start, actual_len);
    sentence_out->sentence[actual_len] = '\0';
    sentence_out->length = actual_len;

    return actual_len;
}
```

---

## 7. COMPLETE ESP32-IDF DRIVER EXAMPLE

### 7.1 Header File (gps_driver.h)

```c
#ifndef GPS_DRIVER_H
#define GPS_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/uart.h"

// Configuration
#define GPS_UART_PORT UART_NUM_1
#define GPS_TXD_PIN   GPIO_NUM_17
#define GPS_RXD_PIN   GPIO_NUM_16
#define GPS_BAUDRATE  9600

// Data structures
typedef struct {
    float latitude;
    float longitude;
    float altitude;
    float speed_knots;
    float track_true;
    uint32_t timestamp_ms;
    uint8_t fix_quality;      // 0=invalid, 1=GPS, 2=DGPS, etc.
    uint8_t num_satellites;
    float hdop;               // Horizontal DOP
    bool is_valid;
} gps_position_t;

typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint16_t milliseconds;
} gps_time_t;

// Function prototypes
void gps_init(void);
bool gps_read_position(gps_position_t *pos);
bool gps_read_time(gps_time_t *time);
void gps_set_baud_rate(uint32_t baud_rate);
void gps_disable_nmea_message(uint8_t message_id);
void gps_enable_nmea_message(uint8_t message_id, uint8_t rate);
void gps_deinit(void);

#endif // GPS_DRIVER_H
```

### 7.2 Implementation File (gps_driver.c)

```c
#include "gps_driver.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "GPS_DRIVER";

// Circular buffer for UART data
typedef struct {
    uint8_t buffer[2048];
    volatile size_t write_pos;
    volatile size_t read_pos;
} uart_ringbuf_t;

static uart_ringbuf_t gps_ringbuf = {0};
static QueueHandle_t uart_event_queue = NULL;
static SemaphoreHandle_t gps_data_mutex = NULL;
static gps_position_t gps_current_position = {0};
static gps_time_t gps_current_time = {0};

/**
 * UART event handler task
 */
static void uart_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t data[512];

    while (1) {
        // Wait for UART event
        if (xQueueReceive(uart_event_queue, (void *)&event,
                         pdMS_TO_TICKS(100)) == pdTRUE) {

            switch (event.type) {
                case UART_DATA:
                    {
                        int len = uart_read_bytes(GPS_UART_PORT, data,
                                                 event.size, 0);
                        for (int i = 0; i < len; i++) {
                            // Add to circular buffer
                            size_t next_pos =
                                (gps_ringbuf.write_pos + 1) % sizeof(gps_ringbuf.buffer);
                            if (next_pos != gps_ringbuf.read_pos) {
                                gps_ringbuf.buffer[gps_ringbuf.write_pos] = data[i];
                                gps_ringbuf.write_pos = next_pos;
                            }
                        }
                    }
                    break;

                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "RX FIFO overflow");
                    uart_flush_input(GPS_UART_PORT);
                    break;

                case UART_FRAME_ERR:
                case UART_PARITY_ERR:
                    ESP_LOGW(TAG, "UART error");
                    uart_flush_input(GPS_UART_PORT);
                    break;

                default:
                    break;
            }
        }
    }
}

/**
 * Parse GGA sentence
 */
static bool parse_gga(const char *sentence, gps_position_t *pos)
{
    char buffer[256];
    if (strlen(sentence) >= sizeof(buffer)) {
        return false;
    }
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *saveptr;
    char *token = strtok_r(buffer, ",", &saveptr);
    int field = 0;

    while (token && field < 15) {
        switch (field) {
            case 1:  // UTC Time
                // Parse time field
                break;
            case 2:  // Latitude
                pos->latitude = atof(token);
                break;
            case 4:  // Longitude
                pos->longitude = atof(token);
                break;
            case 6:  // Fix quality
                pos->fix_quality = atoi(token);
                break;
            case 7:  // Number of satellites
                pos->num_satellites = atoi(token);
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
 * Parse RMC sentence
 */
static bool parse_rmc(const char *sentence, gps_position_t *pos)
{
    char buffer[256];
    if (strlen(sentence) >= sizeof(buffer)) {
        return false;
    }
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *saveptr;
    char *token = strtok_r(buffer, ",", &saveptr);
    int field = 0;

    while (token && field < 10) {
        switch (field) {
            case 2:  // Status (A = active)
                pos->is_valid = (token[0] == 'A');
                break;
            case 3:  // Latitude
                pos->latitude = atof(token);
                break;
            case 5:  // Longitude
                pos->longitude = atof(token);
                break;
            case 7:  // Speed in knots
                pos->speed_knots = atof(token);
                break;
            case 8:  // Track true
                pos->track_true = atof(token);
                break;
            default:
                break;
        }

        token = strtok_r(NULL, ",", &saveptr);
        field++;
    }

    return true;
}

/**
 * Extract complete sentence from circular buffer
 */
static bool extract_sentence(char *sentence_out, size_t max_len)
{
    size_t sentence_len = 0;
    size_t read_pos = gps_ringbuf.read_pos;

    // Find start of sentence ($)
    while (read_pos != gps_ringbuf.write_pos &&
           gps_ringbuf.buffer[read_pos] != '$') {
        read_pos = (read_pos + 1) % sizeof(gps_ringbuf.buffer);
    }

    if (read_pos == gps_ringbuf.write_pos) {
        return false;  // No sentence start found
    }

    size_t start_pos = read_pos;
    read_pos = (read_pos + 1) % sizeof(gps_ringbuf.buffer);

    // Find end of sentence (\n)
    while (read_pos != gps_ringbuf.write_pos &&
           gps_ringbuf.buffer[read_pos] != '\n' &&
           sentence_len < 82) {
        read_pos = (read_pos + 1) % sizeof(gps_ringbuf.buffer);
        sentence_len++;
    }

    if (read_pos == gps_ringbuf.write_pos) {
        return false;  // No sentence end found
    }

    // Copy sentence
    sentence_len = 0;
    read_pos = start_pos;
    while (read_pos != gps_ringbuf.write_pos &&
           gps_ringbuf.buffer[read_pos] != '\n' &&
           sentence_len < max_len - 1) {
        sentence_out[sentence_len++] = gps_ringbuf.buffer[read_pos];
        read_pos = (read_pos + 1) % sizeof(gps_ringbuf.buffer);
    }

    sentence_out[sentence_len] = '\0';

    // Update read position
    gps_ringbuf.read_pos = (read_pos + 1) % sizeof(gps_ringbuf.buffer);

    return sentence_len > 10;  // Minimum sentence length
}

// Public API
void gps_init(void)
{
    // Create mutex for GPS data
    gps_data_mutex = xSemaphoreCreateMutex();

    // UART configuration
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

    // Install driver
    uart_driver_install(GPS_UART_PORT, 1024, 512, 20, &uart_event_queue, 0);

    // Initialize ring buffer
    gps_ringbuf.write_pos = 0;
    gps_ringbuf.read_pos = 0;

    // Create UART handling task
    xTaskCreatePinnedToCore(uart_task, "gps_uart_task", 4096, NULL, 10, NULL, 0);
}

bool gps_read_position(gps_position_t *pos)
{
    if (!pos) {
        return false;
    }

    char sentence[128];
    if (!extract_sentence(sentence, sizeof(sentence))) {
        return false;
    }

    // Validate sentence structure
    if (sentence[0] != '$') {
        return false;
    }

    // Remove \r if present
    size_t len = strlen(sentence);
    if (len > 0 && sentence[len - 1] == '\r') {
        sentence[len - 1] = '\0';
        len--;
    }

    // Validate checksum
    if (!nmea_validate_checksum(sentence, len)) {
        ESP_LOGW(TAG, "Checksum validation failed");
        return false;
    }

    // Determine sentence type and parse
    if (strncmp(sentence + 3, "GGA", 3) == 0) {
        parse_gga(sentence, pos);
    } else if (strncmp(sentence + 3, "RMC", 3) == 0) {
        parse_rmc(sentence, pos);
    } else {
        return false;
    }

    // Store in safe location
    if (xSemaphoreTake(gps_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&gps_current_position, pos, sizeof(gps_position_t));
        xSemaphoreGive(gps_data_mutex);
    }

    return true;
}

bool gps_read_time(gps_time_t *time)
{
    if (!time) {
        return false;
    }

    if (xSemaphoreTake(gps_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(time, &gps_current_time, sizeof(gps_time_t));
        xSemaphoreGive(gps_data_mutex);
        return true;
    }

    return false;
}

void gps_deinit(void)
{
    uart_driver_delete(GPS_UART_PORT);
    if (gps_data_mutex) {
        vSemaphoreDelete(gps_data_mutex);
    }
}
```

---

## 8. CONFIGURATION COMMAND EXAMPLES

### 8.1 Change Baud Rate via UBX Command

```c
/**
 * Send UBX-CFG-PRT to change baud rate to 115200
 */
void gps_set_baud_rate(uint32_t baud_rate)
{
    // Payload structure for UART1 (port ID = 1)
    uint8_t payload[20] = {
        0x01,                           // Port ID: 1 = UART1
        0x00, 0x00, 0x00,              // Reserved
        0x00, 0x00,                    // TX Ready (reserved)
        0x00, 0x00,                    // Reserved
        0x00, 0x00,                    // Reserved
        0xD8, 0x00, 0x00, 0x00,       // UART Mode: 0x000000D8 = 8N1
        0x00, 0x2C, 0x01, 0x00,       // Baud rate (little-endian) - 115200
        0x03, 0x00,                    // Input protocols: UBX + NMEA
        0x03, 0x00                     // Output protocols: UBX + NMEA
    };

    // For different baud rates, modify bytes 16-19:
    // 9600:   0x80, 0x25, 0x00, 0x00
    // 19200:  0x00, 0x4B, 0x00, 0x00
    // 38400:  0x00, 0x96, 0x00, 0x00
    // 57600:  0x00, 0xE1, 0x00, 0x00
    // 115200: 0x00, 0xC2, 0x01, 0x00 (0x0001C200 in little-endian)

    uint8_t buffer[32];
    size_t len = ubx_build_message(buffer, sizeof(buffer),
                                   0x06, 0x00, payload, sizeof(payload));

    if (len > 0) {
        uart_write_bytes(GPS_UART_PORT, (const char *)buffer, len);
    }
}
```

### 8.2 Enable Specific NMEA Messages

```c
/**
 * Enable GPGGA message at 1Hz on UART1
 */
void gps_enable_gpgga(void)
{
    // UBX-CFG-MSG payload
    uint8_t payload[8] = {
        0xF0,           // NMEA message class
        0x00,           // GGA message ID
        0x01, 0x00, 0x00, 0x00,  // Output rate on UART1, UART2, USB, SPI
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
 * Disable RMC message
 */
void gps_disable_rmc(void)
{
    uint8_t payload[8] = {
        0xF0,           // NMEA message class
        0x04,           // RMC message ID
        0x00, 0x00, 0x00, 0x00,  // Disabled on all ports
        0x00, 0x00      // Reserved
    };

    uint8_t buffer[32];
    size_t len = ubx_build_message(buffer, sizeof(buffer),
                                   0x06, 0x01, payload, sizeof(payload));

    if (len > 0) {
        uart_write_bytes(GPS_UART_PORT, (const char *)buffer, len);
    }
}
```

### 8.3 Save Configuration to Flash

```c
/**
 * Save current configuration to module's non-volatile memory
 */
void gps_save_configuration(void)
{
    // UBX-CFG-CFG payload to save all settings to flash
    uint8_t payload[12] = {
        0xFF, 0xFF, 0xFF, 0xFF,   // Bitmask: save all configs
        0xFF, 0xFF, 0xFF, 0xFF,   // Save mask: save all
        0x00, 0x00, 0x00, 0x00    // Load mask: don't load
        // Note: Full payload also includes device mask
    };

    uint8_t buffer[32];
    size_t len = ubx_build_message(buffer, sizeof(buffer),
                                   0x06, 0x09, payload, sizeof(payload));

    if (len > 0) {
        uart_write_bytes(GPS_UART_PORT, (const char *)buffer, len);
    }
}
```

---

## 9. TESTING CHECKLIST

- [ ] UART communication at 9600 bps verified
- [ ] NMEA GGA sentence parsing validated
- [ ] Checksum calculation verified (XOR for NMEA)
- [ ] Fletcher checksum calculation verified (CK_A/CK_B for UBX)
- [ ] Buffer overflow protection tested with oversized input
- [ ] Circular buffer interrupt safety verified
- [ ] Position update atomic read tested
- [ ] Spoofing detection algorithm validated
- [ ] Baud rate change via UBX command tested
- [ ] Configuration save to flash verified
- [ ] FIFO overflow handling tested
- [ ] Malformed sentence rejection verified
- [ ] Memory usage profiled (heap and stack)
- [ ] Edge cases tested (incomplete sentences, multiple sentences, etc.)

---

## 10. REFERENCES

1. **u-blox NEO-6 Datasheet** - GPS.G6-HW-09005
2. **u-blox 6 Receiver Description** - GPS.G6-SW-10018
3. **NMEA 0183 Standard** - National Marine Electronics Association
4. **ESP-IDF UART Driver Documentation** - Espressif Systems
5. **RFC 1146 - Fletcher Checksum Algorithm** - IETF
6. **CWE-367 - TOCTOU Race Conditions** - MITRE
7. **SEI CERT C - Secure String Handling** - Carnegie Mellon
8. **Maritime GPS Spoofing Detection** - MDPI Research

---

**Document Version**: 1.0
**Last Updated**: November 2025
**Suitable for**: ESP32-IDF v4.4+, C17 standard or later
