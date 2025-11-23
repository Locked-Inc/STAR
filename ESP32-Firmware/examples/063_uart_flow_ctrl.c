/**
 * @file 063_uart_flow_ctrl.c
 * @brief UART example demonstrating hardware flow control (RTS/CTS)
 *
 * This example shows different flow control modes:
 * - No flow control
 * - RTS only
 * - CTS only
 * - RTS+CTS (full handshaking)
 *
 * Hardware Setup:
 * - UART1 TX: GPIO 17
 * - UART1 RX: GPIO 16
 * - UART1 RTS: GPIO 5 (Request To Send - output)
 * - UART1 CTS: GPIO 18 (Clear To Send - input)
 *
 * For loopback testing: Connect RTS to CTS
 */

#include "star_bus_uart.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_PORT_NUM    (UART_NUM_1)
#define UART_TX_PIN (17)
#define UART_RX_PIN (16)
#define UART_RTS_PIN (5)
#define UART_CTS_PIN (18)
#define BUF_SIZE (2048)
#define TEST_BAUD_RATE (115200)

/**
 * @brief Get flow control mode name
 */
static const char *priv_get_flow_control_name(uart_hw_flowcontrol_t mode)
{
    switch (mode) {
        case UART_HW_FLOWCTRL_DISABLE: return "Disabled";
        case UART_HW_FLOWCTRL_RTS: return "RTS only";
        case UART_HW_FLOWCTRL_CTS: return "CTS only";
        case UART_HW_FLOWCTRL_CTS_RTS: return "RTS+CTS";
        default: return "Unknown";
    }
}

/**
 * @brief Test UART with specific flow control configuration
 */
static void priv_test_flow_control(uart_hw_flowcontrol_t flow_ctrl, bool use_flow_pins)
{
    printf("\n========================================\n");
    printf("Flow Control Mode: %s\n", priv_get_flow_control_name(flow_ctrl));
    printf("========================================\n");

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = TEST_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = flow_ctrl,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Set RX FIFO threshold for flow control
    if (flow_ctrl != UART_HW_FLOWCTRL_DISABLE) {
        uart_config.rx_flow_ctrl_thresh = 122; // Assert RTS when FIFO has 122 bytes
    }

    printf("Configuration:\n");
    printf("  Baud rate: %d\n", uart_config.baud_rate);
    printf("  Flow control: %s\n", priv_get_flow_control_name(flow_ctrl));
    if (flow_ctrl != UART_HW_FLOWCTRL_DISABLE) {
        printf("  RX threshold: %d bytes\n", uart_config.rx_flow_ctrl_thresh);
    }

    // Install UART driver with larger buffer for flow control testing
    esp_err_t ret = uart_driver_install(UART_PORT_NUM, BUF_SIZE, BUF_SIZE, 0, NULL, 0);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to install driver: %s\n", esp_err_to_name(ret));
        return;
    }

    ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to configure: %s\n", esp_err_to_name(ret));
        uart_driver_delete(UART_PORT_NUM);
        return;
    }

    // Set pins
    if (use_flow_pins) {
        printf("  TX Pin:  GPIO%d\n", UART_TX_PIN);
        printf("  RX Pin:  GPIO%d\n", UART_RX_PIN);
        printf("  RTS Pin: GPIO%d\n", UART_RTS_PIN);
        printf("  CTS Pin: GPIO%d\n", UART_CTS_PIN);
        ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_RTS_PIN, UART_CTS_PIN);
    } else {
        printf("  TX Pin:  GPIO%d\n", UART_TX_PIN);
        printf("  RX Pin:  GPIO%d\n", UART_RX_PIN);
        printf("  RTS/CTS: Not used\n");
        ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }

    if (ret != ESP_OK) {
        printf("ERROR: Failed to set pins: %s\n", esp_err_to_name(ret));
        uart_driver_delete(UART_PORT_NUM);
        return;
    }

    // Test 1: Send small data chunk
    printf("\n--- Test 1: Small data transfer ---\n");
    const char *small_msg = "Small test message with flow control\n";
    printf("Sending %d bytes...\n", strlen(small_msg));

    uint32_t start_time = xTaskGetTickCount();
    int txBytes = uart_write_bytes(UART_PORT_NUM, small_msg, strlen(small_msg));
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(1000));
    uint32_t end_time = xTaskGetTickCount();

    printf("Sent %d bytes in %lu ms\n", txBytes, (end_time - start_time) * portTICK_PERIOD_MS);

    // Test 2: Send large data burst to test flow control
    printf("\n--- Test 2: Large data burst ---\n");
    char large_buffer[1024];
    memset(large_buffer, 'A', sizeof(large_buffer) - 1);
    large_buffer[sizeof(large_buffer) - 1] = '\n';

    printf("Sending %d bytes in burst...\n", sizeof(large_buffer));
    start_time = xTaskGetTickCount();
    txBytes = uart_write_bytes(UART_PORT_NUM, large_buffer, sizeof(large_buffer));
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(2000));
    end_time = xTaskGetTickCount();

    uint32_t elapsed_ms = (end_time - start_time) * portTICK_PERIOD_MS;
    printf("Sent %d bytes in %lu ms\n", txBytes, elapsed_ms);
    printf("Throughput: %.2f KB/s\n", (txBytes / 1024.0f) / (elapsed_ms / 1000.0f));

    // Test 3: Multiple small sends
    printf("\n--- Test 3: Multiple small sends ---\n");
    const int num_sends = 10;
    start_time = xTaskGetTickCount();

    for (int i = 0; i < num_sends; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Message %d with flow control\n", i);
        uart_write_bytes(UART_PORT_NUM, msg, strlen(msg));
    }
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(2000));
    end_time = xTaskGetTickCount();

    printf("Sent %d messages in %lu ms\n", num_sends, (end_time - start_time) * portTICK_PERIOD_MS);

    // Try to receive if in loopback
    printf("\n--- Checking for received data ---\n");
    uint8_t rx_buffer[256];
    size_t total_rx = 0;

    while (1) {
        int rxBytes = uart_read_bytes(UART_PORT_NUM, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(500));
        if (rxBytes > 0) {
            total_rx += rxBytes;
            printf("Received chunk: %d bytes (total: %d)\n", rxBytes, total_rx);
        } else {
            break;
        }
    }

    if (total_rx > 0) {
        printf("Total received: %d bytes\n", total_rx);
    } else {
        printf("No data received (normal if not in loopback)\n");
    }

    // Get UART status
    printf("\n--- UART Status ---\n");
    size_t rx_buffered;
    uart_get_buffered_data_len(UART_PORT_NUM, &rx_buffered);
    printf("RX buffered data: %d bytes\n", rx_buffered);

    // Clean up
    uart_driver_delete(UART_PORT_NUM);
    printf("\nTest completed\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief Demonstrate flow control with continuous data stream
 */
static void priv_test_continuous_stream(void)
{
    printf("\n========================================\n");
    printf("Continuous Stream Test with RTS+CTS\n");
    printf("========================================\n");

    uart_config_t uart_config = {
        .baud_rate = TEST_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to install driver\n");
        return;
    }

    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_RTS_PIN, UART_CTS_PIN);

    printf("Streaming 10KB of data with flow control...\n");
    const size_t total_size = 10240;
    size_t sent = 0;
    uint32_t start_time = xTaskGetTickCount();

    char stream_buffer[256];
    memset(stream_buffer, 'X', sizeof(stream_buffer));

    while (sent < total_size) {
        size_t to_send = (total_size - sent) > sizeof(stream_buffer) ? sizeof(stream_buffer) : (total_size - sent);
        int txBytes = uart_write_bytes(UART_PORT_NUM, stream_buffer, to_send);
        if (txBytes > 0) {
            sent += txBytes;
            if (sent % 1024 == 0) {
                printf("Sent: %d / %d bytes (%.1f%%)\n", sent, total_size, (sent * 100.0f) / total_size);
            }
        }
        vTaskDelay(1); // Small delay to simulate real-world conditions
    }

    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(5000));
    uint32_t end_time = xTaskGetTickCount();
    uint32_t elapsed_ms = (end_time - start_time) * portTICK_PERIOD_MS;

    printf("\nStream complete!\n");
    printf("Total sent: %d bytes\n", sent);
    printf("Time: %lu ms\n", elapsed_ms);
    printf("Throughput: %.2f KB/s\n", (sent / 1024.0f) / (elapsed_ms / 1000.0f));
    printf("Effective baud rate: %.0f baud\n", (sent * 10.0f * 1000.0f) / elapsed_ms);

    uart_driver_delete(UART_PORT_NUM);
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("UART Flow Control Example\n");
    printf("========================================\n");
    printf("This example demonstrates UART hardware\n");
    printf("flow control (RTS/CTS handshaking).\n");
    printf("\n");
    printf("Hardware Configuration:\n");
    printf("  UART Port: UART%d\n", UART_PORT_NUM);
    printf("  TX Pin:    GPIO%d\n", UART_TX_PIN);
    printf("  RX Pin:    GPIO%d\n", UART_RX_PIN);
    printf("  RTS Pin:   GPIO%d (output)\n", UART_RTS_PIN);
    printf("  CTS Pin:   GPIO%d (input)\n", UART_CTS_PIN);
    printf("\n");
    printf("Flow Control Info:\n");
    printf("  RTS: Request To Send (we assert when ready)\n");
    printf("  CTS: Clear To Send (peer asserts when ready)\n");
    printf("\n");
    printf("For loopback: Connect GPIO%d to GPIO%d\n", UART_RTS_PIN, UART_CTS_PIN);
    printf("========================================\n");

    // Test 1: No flow control (baseline)
    priv_test_flow_control(UART_HW_FLOWCTRL_DISABLE, false);

    // Test 2: RTS only (we control when we're ready to receive)
    priv_test_flow_control(UART_HW_FLOWCTRL_RTS, true);

    // Test 3: CTS only (peer controls when we can send)
    priv_test_flow_control(UART_HW_FLOWCTRL_CTS, true);

    // Test 4: Full RTS+CTS handshaking
    priv_test_flow_control(UART_HW_FLOWCTRL_CTS_RTS, true);

    // Test 5: Continuous stream with flow control
    priv_test_continuous_stream();

    printf("\n========================================\n");
    printf("Flow Control Tests Completed!\n");
    printf("========================================\n");
    printf("\nKey Observations:\n");
    printf("1. Flow control prevents data loss in fast transfers\n");
    printf("2. RTS is asserted when RX FIFO has space\n");
    printf("3. CTS must be asserted for TX to proceed\n");
    printf("4. Full RTS+CTS provides bidirectional control\n");
    printf("\nExample finished.\n");
}
