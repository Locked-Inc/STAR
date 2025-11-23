/**
 * @file 070_uart_protocols.c
 * @brief UART example demonstrating line protocol parsing (CSV, JSON lines, etc.)
 *
 * This example shows how to parse various line-based protocols:
 * - CSV (Comma-Separated Values)
 * - JSON Lines (JSONL)
 * - Key-Value pairs
 * - NMEA (GPS protocol)
 * - Custom delimited protocols
 * - AT commands
 *
 * Hardware Setup:
 * - UART1 TX: GPIO 17
 * - UART1 RX: GPIO 16
 */

#include "star_bus_uart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_PORT_NUM    (UART_NUM_1)
#define UART_TX_PIN (17)
#define UART_RX_PIN (16)
#define BUF_SIZE (2048)
#define TEST_BAUD_RATE (115200)
#define LINE_MAX_LEN (256)

static QueueHandle_t s_uart_queue;

/**
 * @brief Read a line from UART (up to newline)
 */
static int priv_read_line(char *line, size_t max_len, uint32_t timeout_ms)
{
    size_t pos = 0;
    uint8_t c;

    uint32_t start_time = xTaskGetTickCount();

    while (pos < max_len - 1) {
        int len = uart_read_bytes(UART_PORT_NUM, &c, 1, pdMS_TO_TICKS(100));

        if (len > 0) {
            if (c == '\n') {
                line[pos] = '\0';
                return pos;
            } else if (c != '\r') {  // Skip CR
                line[pos++] = c;
            }
        }

        // Check timeout
        uint32_t elapsed = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
        if (elapsed > timeout_ms) {
            line[pos] = '\0';
            return -1;  // Timeout
        }
    }

    line[pos] = '\0';
    return pos;
}

/**
 * @brief Parse CSV line
 */
static void priv_parse_csv(const char *line)
{
    printf("  Parsing CSV: %s\n", line);

    char buffer[LINE_MAX_LEN];
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    int field_count = 0;
    char *token = strtok(buffer, ",");

    while (token != NULL) {
        printf("    Field %d: [%s]\n", field_count++, token);
        token = strtok(NULL, ",");
    }

    printf("    Total fields: %d\n", field_count);
}

/**
 * @brief Parse JSON line (simple key-value extraction)
 */
static void priv_parse_json_line(const char *line)
{
    printf("  Parsing JSON: %s\n", line);

    // Simple JSON parsing (for demo purposes)
    // In production, use a proper JSON library like cJSON

    const char *ptr = line;
    while (*ptr) {
        // Look for "key": pattern
        if (*ptr == '"') {
            ptr++;
            const char *key_start = ptr;
            while (*ptr && *ptr != '"') ptr++;

            if (*ptr == '"') {
                size_t key_len = ptr - key_start;
                char key[64];
                strncpy(key, key_start, key_len);
                key[key_len] = '\0';

                ptr++;
                // Skip whitespace and colon
                while (*ptr && (*ptr == ' ' || *ptr == ':')) ptr++;

                // Get value
                if (*ptr == '"') {
                    // String value
                    ptr++;
                    const char *val_start = ptr;
                    while (*ptr && *ptr != '"') ptr++;
                    size_t val_len = ptr - val_start;
                    char value[64];
                    strncpy(value, val_start, val_len);
                    value[val_len] = '\0';
                    printf("    %s: \"%s\"\n", key, value);
                } else {
                    // Number or boolean
                    const char *val_start = ptr;
                    while (*ptr && *ptr != ',' && *ptr != '}') ptr++;
                    size_t val_len = ptr - val_start;
                    char value[64];
                    strncpy(value, val_start, val_len);
                    value[val_len] = '\0';
                    printf("    %s: %s\n", key, value);
                }
            }
        }
        ptr++;
    }
}

/**
 * @brief Parse key-value pairs (KEY=VALUE format)
 */
static void priv_parse_key_value(const char *line)
{
    printf("  Parsing Key-Value: %s\n", line);

    char buffer[LINE_MAX_LEN];
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *pair = strtok(buffer, " ");
    while (pair != NULL) {
        char *equals = strchr(pair, '=');
        if (equals != NULL) {
            *equals = '\0';
            const char *key = pair;
            const char *value = equals + 1;
            printf("    %s = %s\n", key, value);
        }
        pair = strtok(NULL, " ");
    }
}

/**
 * @brief Parse NMEA sentence (GPS protocol)
 */
static void priv_parse_nmea(const char *line)
{
    printf("  Parsing NMEA: %s\n", line);

    // NMEA format: $GPGGA,time,lat,N/S,lon,E/W,quality,sats,hdop,alt,M,geoid,M,age,station*checksum

    if (line[0] != '$') {
        printf("    ERROR: Invalid NMEA (no $)\n");
        return;
    }

    // Verify checksum
    const char *star = strchr(line, '*');
    if (star != NULL) {
        uint8_t calc_checksum = 0;
        for (const char *p = line + 1; p < star; p++) {
            calc_checksum ^= *p;
        }
        uint8_t recv_checksum = (uint8_t)strtol(star + 1, NULL, 16);
        printf("    Checksum: 0x%02X (calculated: 0x%02X) %s\n",
               recv_checksum, calc_checksum,
               recv_checksum == calc_checksum ? "OK" : "FAIL");
    }

    // Parse sentence type
    char sentence_type[8];
    strncpy(sentence_type, line + 1, 5);
    sentence_type[5] = '\0';
    printf("    Type: %s\n", sentence_type);

    // Parse fields
    char buffer[LINE_MAX_LEN];
    strncpy(buffer, line + 1, sizeof(buffer) - 1);

    // Remove checksum
    char *star_pos = strchr(buffer, '*');
    if (star_pos) *star_pos = '\0';

    int field_count = 0;
    char *token = strtok(buffer, ",");
    while (token != NULL) {
        if (strlen(token) > 0) {
            printf("    Field %d: %s\n", field_count, token);
        }
        field_count++;
        token = strtok(NULL, ",");
    }
}

/**
 * @brief Parse AT command response
 */
static void priv_parse_at_response(const char *line)
{
    printf("  Parsing AT Response: %s\n", line);

    if (strcmp(line, "OK") == 0) {
        printf("    Status: OK\n");
    } else if (strcmp(line, "ERROR") == 0) {
        printf("    Status: ERROR\n");
    } else if (strncmp(line, "+", 1) == 0) {
        // Extended response
        char *colon = strchr(line, ':');
        if (colon != NULL) {
            *colon = '\0';
            printf("    Command: %s\n", line);
            printf("    Response: %s\n", colon + 1);
        }
    } else {
        printf("    Data: %s\n", line);
    }
}

/**
 * @brief Test CSV protocol
 */
static void priv_test_csv_protocol(void)
{
    printf("\n========================================\n");
    printf("Test 1: CSV Protocol\n");
    printf("========================================\n");

    const char *csv_lines[] = {
        "timestamp,sensor,value,unit\n",
        "1234567890,temperature,25.5,celsius\n",
        "1234567891,humidity,60.2,percent\n",
        "1234567892,pressure,1013.25,hPa\n",
    };

    for (int i = 0; i < 4; i++) {
        printf("\nSending: %s", csv_lines[i]);
        uart_write_bytes(UART_PORT_NUM, csv_lines[i], strlen(csv_lines[i]));

        vTaskDelay(pdMS_TO_TICKS(200));

        char line[LINE_MAX_LEN];
        if (priv_read_line(line, sizeof(line), 1000) > 0) {
            priv_parse_csv(line);
        }
    }
}

/**
 * @brief Test JSON Lines protocol
 */
static void priv_test_json_protocol(void)
{
    printf("\n========================================\n");
    printf("Test 2: JSON Lines Protocol\n");
    printf("========================================\n");

    const char *json_lines[] = {
        "{\"sensor\":\"temp\",\"value\":25.5,\"unit\":\"C\"}\n",
        "{\"sensor\":\"humidity\",\"value\":60.2,\"unit\":\"%\"}\n",
        "{\"id\":123,\"status\":\"active\",\"count\":42}\n",
    };

    for (int i = 0; i < 3; i++) {
        printf("\nSending: %s", json_lines[i]);
        uart_write_bytes(UART_PORT_NUM, json_lines[i], strlen(json_lines[i]));

        vTaskDelay(pdMS_TO_TICKS(200));

        char line[LINE_MAX_LEN];
        if (priv_read_line(line, sizeof(line), 1000) > 0) {
            priv_parse_json_line(line);
        }
    }
}

/**
 * @brief Test key-value protocol
 */
static void priv_test_keyvalue_protocol(void)
{
    printf("\n========================================\n");
    printf("Test 3: Key-Value Protocol\n");
    printf("========================================\n");

    const char *kv_lines[] = {
        "temp=25.5 humidity=60.2 pressure=1013.25\n",
        "status=OK count=42 mode=auto\n",
        "voltage=3.3 current=0.5 power=1.65\n",
    };

    for (int i = 0; i < 3; i++) {
        printf("\nSending: %s", kv_lines[i]);
        uart_write_bytes(UART_PORT_NUM, kv_lines[i], strlen(kv_lines[i]));

        vTaskDelay(pdMS_TO_TICKS(200));

        char line[LINE_MAX_LEN];
        if (priv_read_line(line, sizeof(line), 1000) > 0) {
            priv_parse_key_value(line);
        }
    }
}

/**
 * @brief Test NMEA protocol
 */
static void priv_test_nmea_protocol(void)
{
    printf("\n========================================\n");
    printf("Test 4: NMEA Protocol (GPS)\n");
    printf("========================================\n");

    const char *nmea_sentences[] = {
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\n",
        "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39\n",
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\n",
    };

    for (int i = 0; i < 3; i++) {
        printf("\nSending: %s", nmea_sentences[i]);
        uart_write_bytes(UART_PORT_NUM, nmea_sentences[i], strlen(nmea_sentences[i]));

        vTaskDelay(pdMS_TO_TICKS(200));

        char line[LINE_MAX_LEN];
        if (priv_read_line(line, sizeof(line), 1000) > 0) {
            priv_parse_nmea(line);
        }
    }
}

/**
 * @brief Test AT command protocol
 */
static void priv_test_at_protocol(void)
{
    printf("\n========================================\n");
    printf("Test 5: AT Command Protocol\n");
    printf("========================================\n");

    const char *at_responses[] = {
        "OK\n",
        "+CGMI: Manufacturer Name\n",
        "+CSQ: 25,0\n",
        "ERROR\n",
    };

    for (int i = 0; i < 4; i++) {
        printf("\nSending: %s", at_responses[i]);
        uart_write_bytes(UART_PORT_NUM, at_responses[i], strlen(at_responses[i]));

        vTaskDelay(pdMS_TO_TICKS(200));

        char line[LINE_MAX_LEN];
        if (priv_read_line(line, sizeof(line), 1000) > 0) {
            priv_parse_at_response(line);
        }
    }
}

/**
 * @brief Test custom protocol with multiple delimiters
 */
static void priv_test_custom_protocol(void)
{
    printf("\n========================================\n");
    printf("Test 6: Custom Protocol\n");
    printf("========================================\n");

    // Custom format: [TAG]|field1|field2|field3
    const char *custom_lines[] = {
        "[DATA]|sensor1|25.5|active\n",
        "[STATUS]|system|ok|100\n",
        "[ERROR]|device2|timeout|0\n",
    };

    for (int i = 0; i < 3; i++) {
        printf("\nSending: %s", custom_lines[i]);
        uart_write_bytes(UART_PORT_NUM, custom_lines[i], strlen(custom_lines[i]));

        vTaskDelay(pdMS_TO_TICKS(200));

        char line[LINE_MAX_LEN];
        if (priv_read_line(line, sizeof(line), 1000) > 0) {
            printf("  Parsing Custom: %s\n", line);

            // Extract tag
            char *tag_end = strchr(line, ']');
            if (tag_end != NULL) {
                *tag_end = '\0';
                printf("    Tag: %s\n", line + 1);

                // Parse fields
                char *field = strtok(tag_end + 2, "|");
                int field_num = 0;
                while (field != NULL) {
                    printf("    Field %d: %s\n", field_num++, field);
                    field = strtok(NULL, "|");
                }
            }
        }
    }
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("UART Protocol Parsing Example\n");
    printf("========================================\n");
    printf("This example demonstrates parsing of\n");
    printf("various line-based protocols over UART.\n");
    printf("\n");
    printf("Hardware Configuration:\n");
    printf("  UART Port: UART%d\n", UART_PORT_NUM);
    printf("  TX Pin:    GPIO%d\n", UART_TX_PIN);
    printf("  RX Pin:    GPIO%d\n", UART_RX_PIN);
    printf("  Baud Rate: %d\n", TEST_BAUD_RATE);
    printf("\n");
    printf("Protocols Demonstrated:\n");
    printf("  1. CSV (Comma-Separated Values)\n");
    printf("  2. JSON Lines (JSONL)\n");
    printf("  3. Key-Value pairs\n");
    printf("  4. NMEA (GPS protocol)\n");
    printf("  5. AT commands\n");
    printf("  6. Custom delimited\n");
    printf("========================================\n");

    // Initialize UART
    uart_config_t uart_config = {
        .baud_rate = TEST_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, BUF_SIZE * 2,
                                        20, &s_uart_queue, 0);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to install driver\n");
        return;
    }

    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN,
                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    printf("\nUART initialized successfully\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Run protocol tests
    priv_test_csv_protocol();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_json_protocol();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_keyvalue_protocol();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_nmea_protocol();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_at_protocol();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_custom_protocol();

    printf("\n========================================\n");
    printf("Protocol Parsing Tests Completed!\n");
    printf("========================================\n");
    printf("\nProtocol Design Best Practices:\n");
    printf("1. Use clear delimiters (comma, pipe, etc.)\n");
    printf("2. Include line terminators (\\n or \\r\\n)\n");
    printf("3. Add checksums for critical data (NMEA)\n");
    printf("4. Keep lines reasonably short (<256 bytes)\n");
    printf("5. Use consistent field ordering\n");
    printf("6. Handle errors gracefully\n");
    printf("7. Document your protocol format\n");
    printf("\nCommon Use Cases:\n");
    printf("  - Sensor data logging (CSV)\n");
    printf("  - Configuration data (JSON)\n");
    printf("  - GPS receivers (NMEA)\n");
    printf("  - Cellular modems (AT commands)\n");
    printf("  - Custom industrial protocols\n");
    printf("\nExample finished.\n");

    // Clean up
    uart_driver_delete(UART_PORT_NUM);
}
