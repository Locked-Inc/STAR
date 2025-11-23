/**
 * @file 061_uart_all_bauds.c
 * @brief UART example demonstrating all standard baud rates from 300 to 921600
 *
 * This example shows how to configure UART with different standard baud rates:
 * 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200, 230400, 460800, 921600
 *
 * Hardware Setup:
 * - UART1 TX: GPIO 17
 * - UART1 RX: GPIO 16
 * - Connect to USB-to-UART adapter and adjust terminal baud rate accordingly
 */

#include "star_bus_uart.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define UART_PORT_NUM    (UART_NUM_1)
#define UART_TX_PIN (17)
#define UART_RX_PIN (16)
#define BUF_SIZE (1024)

// Standard baud rates to test
static const uint32_t s_standard_bauds[] = {
    300,
    600,
    1200,
    2400,
    4800,
    9600,
    14400,
    19200,
    38400,
    57600,
    115200,
    230400,
    460800,
    921600
};

/**
 * @brief Test UART at a specific baud rate
 */
static void priv_test_baud_rate(uint32_t baud_rate)
{
    printf("\n========================================\n");
    printf("Testing UART at %lu baud\n", baud_rate);
    printf("========================================\n");

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install UART driver
    esp_err_t ret = uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        printf("Failed to install UART driver: %s\n", esp_err_to_name(ret));
        return;
    }

    ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        printf("Failed to configure UART parameters: %s\n", esp_err_to_name(ret));
        uart_driver_delete(UART_PORT_NUM);
        return;
    }

    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        printf("Failed to set UART pins: %s\n", esp_err_to_name(ret));
        uart_driver_delete(UART_PORT_NUM);
        return;
    }

    // Calculate theoretical transmission time for test message
    const char *test_msg = "UART Test Message at %lu baud - The quick brown fox jumps over the lazy dog!\n";
    char buffer[128];
    snprintf(buffer, sizeof(buffer), test_msg, baud_rate);

    size_t msg_len = strlen(buffer);
    // Each character: 1 start bit + 8 data bits + 1 stop bit = 10 bits
    float bits_per_char = 10.0f;
    float time_ms = (msg_len * bits_per_char * 1000.0f) / baud_rate;

    printf("Message length: %d bytes\n", msg_len);
    printf("Theoretical transmission time: %.2f ms\n", time_ms);

    // Send test message
    printf("Sending test message...\n");
    uint32_t start_time = xTaskGetTickCount();
    int txBytes = uart_write_bytes(UART_PORT_NUM, buffer, msg_len);
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(1000));
    uint32_t end_time = xTaskGetTickCount();

    float actual_time_ms = (end_time - start_time) * portTICK_PERIOD_MS;
    printf("Sent %d bytes in %.2f ms\n", txBytes, actual_time_ms);
    printf("Actual baud rate: %.0f baud\n", (txBytes * bits_per_char * 1000.0f) / actual_time_ms);

    // Wait for echo (if connected in loopback)
    printf("Waiting for data (2 seconds)...\n");
    uint8_t rx_buffer[128];
    int rxBytes = uart_read_bytes(UART_PORT_NUM, rx_buffer, sizeof(rx_buffer) - 1, pdMS_TO_TICKS(2000));
    if (rxBytes > 0) {
        rx_buffer[rxBytes] = 0;
        printf("Received %d bytes: %s", rxBytes, rx_buffer);
    } else {
        printf("No data received (normal if not in loopback mode)\n");
    }

    // Clean up
    uart_driver_delete(UART_PORT_NUM);
    printf("Test completed for %lu baud\n", baud_rate);

    // Delay between tests
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief Test custom baud rates
 */
static void priv_test_custom_baud_rates(void)
{
    printf("\n========================================\n");
    printf("Testing Custom Baud Rates\n");
    printf("========================================\n");

    // Some custom baud rates that might be used in specific applications
    uint32_t custom_bauds[] = {
        7200,      // Old modems
        31250,     // MIDI
        62500,     // Custom industrial
        250000,    // High speed custom
        500000,    // Very high speed
        1000000,   // 1 Mbaud
        1500000,   // 1.5 Mbaud
        2000000    // 2 Mbaud
    };

    for (int i = 0; i < sizeof(custom_bauds) / sizeof(custom_bauds[0]); i++) {
        printf("\nTesting custom baud: %lu\n", custom_bauds[i]);

        uart_config_t uart_config = {
            .baud_rate = custom_bauds[i],
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };

        esp_err_t ret = uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
        if (ret == ESP_OK) {
            ret = uart_param_config(UART_PORT_NUM, &uart_config);
            if (ret == ESP_OK) {
                ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
                if (ret == ESP_OK) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Custom baud test: %lu\n", custom_bauds[i]);
                    uart_write_bytes(UART_PORT_NUM, msg, strlen(msg));
                    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(500));
                    printf("Successfully configured and tested %lu baud\n", custom_bauds[i]);
                } else {
                    printf("Failed to set pins: %s\n", esp_err_to_name(ret));
                }
            } else {
                printf("Failed to configure: %s\n", esp_err_to_name(ret));
            }
            uart_driver_delete(UART_PORT_NUM);
        } else {
            printf("Failed to install driver: %s\n", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("UART All Baud Rates Example\n");
    printf("========================================\n");
    printf("This example tests all standard UART baud rates\n");
    printf("from 300 to 921600 baud.\n");
    printf("\n");
    printf("Hardware Configuration:\n");
    printf("  UART Port: UART%d\n", UART_PORT_NUM);
    printf("  TX Pin:    GPIO%d\n", UART_TX_PIN);
    printf("  RX Pin:    GPIO%d\n", UART_RX_PIN);
    printf("\n");
    printf("Note: Connect your serial terminal and adjust\n");
    printf("      the baud rate to match each test.\n");
    printf("========================================\n");

    // Test all standard baud rates
    for (int i = 0; i < sizeof(s_standard_bauds) / sizeof(s_standard_bauds[0]); i++) {
        priv_test_baud_rate(s_standard_bauds[i]);
    }

    // Test custom baud rates
    priv_test_custom_baud_rates();

    printf("\n========================================\n");
    printf("All baud rate tests completed!\n");
    printf("========================================\n");

    // Final test at common debug baud rate (115200)
    printf("\nReturning to 115200 baud for normal operation...\n");
    priv_test_baud_rate(115200);

    printf("\nExample finished. System will remain at 115200 baud.\n");
}
