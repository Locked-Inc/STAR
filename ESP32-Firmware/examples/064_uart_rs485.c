/**
 * @file 064_uart_rs485.c
 * @brief UART example demonstrating RS-485 half-duplex communication with DE/RE control
 *
 * This example shows RS-485 mode configuration:
 * - Half-duplex communication
 * - Driver Enable (DE) and Receiver Enable (RE) control
 * - Collision detection
 * - Multi-drop addressing
 *
 * Hardware Setup:
 * - UART1 TX: GPIO 17
 * - UART1 RX: GPIO 16
 * - RS-485 DE/RE: GPIO 4 (Driver Enable / Receiver Enable)
 * - Connect to RS-485 transceiver (e.g., MAX485, SN65HVD75)
 */

#include "star_bus_uart.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_PORT_NUM    (UART_NUM_1)
#define UART_TX_PIN (17)
#define UART_RX_PIN (16)
#define RS485_DE_RE_PIN  (4)      // DE and RE tied together (typical configuration)
#define BUF_SIZE (1024)
#define TEST_BAUD_RATE   (9600)   // Common for RS-485 industrial networks

// RS-485 protocol defines
#define RS485_START_BYTE    (0x02)  // STX
#define RS485_END_BYTE      (0x03)  // ETX
#define RS485_BROADCAST_ADDR (0xFF)

/**
 * @brief RS-485 frame structure
 */
typedef struct {
    uint8_t start;      // Start byte
    uint8_t address;    // Device address
    uint8_t function;   // Function code
    uint8_t length;     // Data length
    uint8_t data[32];   // Data payload
    uint8_t checksum;   // Simple checksum
    uint8_t end;        // End byte
} __attribute__((packed)) rs485_frame_t;

/**
 * @brief Calculate simple checksum
 */
static uint8_t priv_calculate_checksum(const uint8_t *data, size_t len)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief Initialize UART in RS-485 mode
 */
static esp_err_t priv_rs485_init(void)
{
    printf("Initializing RS-485 mode...\n");

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = TEST_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,  // Can use EVEN for more robust communication
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    printf("  Baud rate: %d\n", uart_config.baud_rate);
    printf("  Data format: 8N1\n");

    // Install driver
    esp_err_t ret = uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to install driver: %s\n", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to configure UART: %s\n", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to set pins: %s\n", esp_err_to_name(ret));
        return ret;
    }

    // Configure RS-485 mode with DE/RE pin
    ret = uart_set_mode(UART_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to set RS-485 mode: %s\n", esp_err_to_name(ret));
        return ret;
    }

    // Set DE/RE pin
    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, RS485_DE_RE_PIN, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to set DE/RE pin: %s\n", esp_err_to_name(ret));
        return ret;
    }

    printf("  RS-485 mode: Half-duplex\n");
    printf("  DE/RE Pin: GPIO%d\n", RS485_DE_RE_PIN);
    printf("RS-485 initialized successfully\n");

    return ESP_OK;
}

/**
 * @brief Build RS-485 frame
 */
static size_t priv_rs485_build_frame(uint8_t address, uint8_t function, const uint8_t *data,
                                     uint8_t data_len, uint8_t *frame_buffer)
{
    rs485_frame_t *frame = (rs485_frame_t *)frame_buffer;

    frame->start = RS485_START_BYTE;
    frame->address = address;
    frame->function = function;
    frame->length = data_len;

    if (data_len > 0 && data != NULL) {
        memcpy(frame->data, data, data_len);
    }

    // Calculate checksum over address, function, length, and data
    uint8_t checksum_data[35];
    checksum_data[0] = frame->address;
    checksum_data[1] = frame->function;
    checksum_data[2] = frame->length;
    memcpy(&checksum_data[3], frame->data, data_len);
    frame->checksum = priv_calculate_checksum(checksum_data, 3 + data_len);

    frame->end = RS485_END_BYTE;

    return sizeof(rs485_frame_t) - 32 + data_len; // Actual frame size
}

/**
 * @brief Send RS-485 frame
 */
static esp_err_t priv_rs485_send_frame(uint8_t address, uint8_t function, const uint8_t *data, uint8_t data_len)
{
    uint8_t frame_buffer[64];
    size_t frame_len = priv_rs485_build_frame(address, function, data, data_len, frame_buffer);

    printf("Sending RS-485 frame:\n");
    printf("  Address: 0x%02X\n", address);
    printf("  Function: 0x%02X\n", function);
    printf("  Data length: %d\n", data_len);
    printf("  Frame size: %d bytes\n", frame_len);

    // Send frame (DE will be automatically asserted during transmission)
    int written = uart_write_bytes(UART_PORT_NUM, (const char *)frame_buffer, frame_len);
    if (written != frame_len) {
        printf("ERROR: Failed to send complete frame\n");
        return ESP_FAIL;
    }

    // Wait for transmission to complete
    esp_err_t ret = uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        printf("ERROR: TX timeout: %s\n", esp_err_to_name(ret));
        return ret;
    }

    printf("Frame sent successfully\n");
    return ESP_OK;
}

/**
 * @brief Receive RS-485 frame
 */
static esp_err_t priv_rs485_receive_frame(rs485_frame_t *frame, uint32_t timeout_ms)
{
    uint8_t buffer[64];
    int len = uart_read_bytes(UART_PORT_NUM, buffer, sizeof(buffer), pdMS_TO_TICKS(timeout_ms));

    if (len <= 0) {
        return ESP_ERR_TIMEOUT;
    }

    printf("Received %d bytes\n", len);

    // Find start byte
    int start_idx = -1;
    for (int i = 0; i < len; i++) {
        if (buffer[i] == RS485_START_BYTE) {
            start_idx = i;
            break;
        }
    }

    if (start_idx == -1) {
        printf("ERROR: No start byte found\n");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Parse frame
    rs485_frame_t *rx_frame = (rs485_frame_t *)&buffer[start_idx];

    // Verify end byte
    size_t expected_len = sizeof(rs485_frame_t) - 32 + rx_frame->length;
    if (start_idx + expected_len > len) {
        printf("ERROR: Incomplete frame\n");
        return ESP_ERR_INVALID_SIZE;
    }

    if (buffer[start_idx + expected_len - 1] != RS485_END_BYTE) {
        printf("ERROR: Invalid end byte\n");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Verify checksum
    uint8_t checksum_data[35];
    checksum_data[0] = rx_frame->address;
    checksum_data[1] = rx_frame->function;
    checksum_data[2] = rx_frame->length;
    memcpy(&checksum_data[3], rx_frame->data, rx_frame->length);
    uint8_t calc_checksum = priv_calculate_checksum(checksum_data, 3 + rx_frame->length);

    if (calc_checksum != rx_frame->checksum) {
        printf("ERROR: Checksum mismatch (expected: 0x%02X, got: 0x%02X)\n",
               calc_checksum, rx_frame->checksum);
        return ESP_ERR_INVALID_CRC;
    }

    // Copy valid frame
    memcpy(frame, rx_frame, expected_len);

    printf("Frame received successfully:\n");
    printf("  Address: 0x%02X\n", frame->address);
    printf("  Function: 0x%02X\n", frame->function);
    printf("  Data length: %d\n", frame->length);

    return ESP_OK;
}

/**
 * @brief Test basic RS-485 communication
 */
static void priv_test_rs485_basic(void)
{
    printf("\n========================================\n");
    printf("Test 1: Basic RS-485 Communication\n");
    printf("========================================\n");

    // Send a simple command
    const uint8_t cmd_data[] = {0x01, 0x02, 0x03, 0x04};
    esp_err_t ret = priv_rs485_send_frame(0x01, 0x10, cmd_data, sizeof(cmd_data));

    if (ret == ESP_OK) {
        printf("Command sent successfully\n");

        // Try to receive response (if in loopback or network)
        rs485_frame_t response;
        ret = priv_rs485_receive_frame(&response, 1000);

        if (ret == ESP_OK) {
            printf("Response received\n");
        } else if (ret == ESP_ERR_TIMEOUT) {
            printf("No response (normal if not in network)\n");
        } else {
            printf("Error receiving response: %s\n", esp_err_to_name(ret));
        }
    }
}

/**
 * @brief Test RS-485 broadcast
 */
static void priv_test_rs485_broadcast(void)
{
    printf("\n========================================\n");
    printf("Test 2: RS-485 Broadcast\n");
    printf("========================================\n");

    const uint8_t broadcast_data[] = "BROADCAST";
    printf("Sending broadcast message to all devices...\n");

    esp_err_t ret = priv_rs485_send_frame(RS485_BROADCAST_ADDR, 0xFF, broadcast_data, sizeof(broadcast_data));

    if (ret == ESP_OK) {
        printf("Broadcast sent (no response expected)\n");
    }
}

/**
 * @brief Test RS-485 polling multiple devices
 */
static void priv_test_rs485_polling(void)
{
    printf("\n========================================\n");
    printf("Test 3: RS-485 Device Polling\n");
    printf("========================================\n");

    const uint8_t num_devices = 5;
    const uint8_t poll_cmd[] = {0x00}; // Status request

    printf("Polling %d devices...\n", num_devices);

    for (uint8_t addr = 1; addr <= num_devices; addr++) {
        printf("\nPolling device 0x%02X...\n", addr);

        esp_err_t ret = priv_rs485_send_frame(addr, 0x01, poll_cmd, sizeof(poll_cmd));
        if (ret != ESP_OK) {
            printf("  Failed to send\n");
            continue;
        }

        // Wait for response
        rs485_frame_t response;
        ret = priv_rs485_receive_frame(&response, 500);

        if (ret == ESP_OK) {
            printf("  Device 0x%02X responded\n", addr);
        } else if (ret == ESP_ERR_TIMEOUT) {
            printf("  Device 0x%02X no response\n", addr);
        } else {
            printf("  Device 0x%02X error: %s\n", addr, esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between polls
    }
}

/**
 * @brief Test RS-485 collision detection
 */
static void priv_test_rs485_collision(void)
{
    printf("\n========================================\n");
    printf("Test 4: RS-485 Collision Detection\n");
    printf("========================================\n");

    printf("In a real RS-485 network, collision detection\n");
    printf("happens when multiple devices transmit simultaneously.\n");
    printf("\nSimulating rapid successive transmissions...\n");

    const uint8_t test_data[] = {0xAA, 0xBB, 0xCC};

    for (int i = 0; i < 3; i++) {
        printf("\nTransmission %d:\n", i + 1);
        priv_rs485_send_frame(0x01, 0x20 + i, test_data, sizeof(test_data));
        vTaskDelay(pdMS_TO_TICKS(5)); // Minimal delay
    }

    printf("\nNote: Proper collision handling requires:\n");
    printf("  1. Listen-before-talk (carrier sense)\n");
    printf("  2. Random backoff on collision\n");
    printf("  3. Retry mechanism\n");
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("UART RS-485 Example\n");
    printf("========================================\n");
    printf("This example demonstrates RS-485 half-duplex\n");
    printf("communication with automatic DE/RE control.\n");
    printf("\n");
    printf("Hardware Configuration:\n");
    printf("  UART Port: UART%d\n", UART_PORT_NUM);
    printf("  TX Pin:    GPIO%d (connect to DI on transceiver)\n", UART_TX_PIN);
    printf("  RX Pin:    GPIO%d (connect to RO on transceiver)\n", UART_RX_PIN);
    printf("  DE/RE Pin: GPIO%d (connect to DE and RE)\n", RS485_DE_RE_PIN);
    printf("  Baud Rate: %d\n", TEST_BAUD_RATE);
    printf("\n");
    printf("RS-485 Transceiver (e.g., MAX485):\n");
    printf("  DI  - Driver Input (from TX)\n");
    printf("  RO  - Receiver Output (to RX)\n");
    printf("  DE  - Driver Enable (from GPIO%d)\n", RS485_DE_RE_PIN);
    printf("  RE  - Receiver Enable (from GPIO%d)\n", RS485_DE_RE_PIN);
    printf("  A/B - RS-485 bus lines\n");
    printf("========================================\n");

    // Initialize RS-485
    if (priv_rs485_init() != ESP_OK) {
        printf("Failed to initialize RS-485\n");
        return;
    }

    // Run tests
    priv_test_rs485_basic();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_rs485_broadcast();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_rs485_polling();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_rs485_collision();

    printf("\n========================================\n");
    printf("RS-485 Tests Completed!\n");
    printf("========================================\n");
    printf("\nRS-485 Key Points:\n");
    printf("1. Half-duplex: Can't send and receive simultaneously\n");
    printf("2. DE/RE control: Automatically managed by driver\n");
    printf("3. Multi-drop: Up to 32 devices (standard) or 256 (extended)\n");
    printf("4. Termination: 120-ohm resistors at both ends of bus\n");
    printf("5. Distance: Up to 1200m (4000ft) at lower baud rates\n");
    printf("\nExample finished.\n");

    // Clean up
    uart_driver_delete(UART_PORT_NUM);
}
