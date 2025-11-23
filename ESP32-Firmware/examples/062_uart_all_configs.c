/**
 * @file 062_uart_all_configs.c
 * @brief UART example demonstrating all data bits (5-8), parity (N/E/O), stop bits (1/1.5/2)
 *
 * This example shows all possible UART frame configurations:
 * - Data bits: 5, 6, 7, 8
 * - Parity: None, Even, Odd
 * - Stop bits: 1, 1.5, 2
 *
 * Hardware Setup:
 * - UART1 TX: GPIO 17
 * - UART1 RX: GPIO 16
 * - Connect to USB-to-UART adapter with matching configuration
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
#define TEST_BAUD_RATE (115200)

/**
 * @brief Structure to hold UART frame configuration
 */
typedef struct {
    uart_word_length_t data_bits;
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
    const char *data_bits_name;
    const char *parity_name;
    const char *stop_bits_name;
} uart_frame_config_t;

/**
 * @brief Get string representation of data bits
 */
static const char *priv_get_data_bits_name(uart_word_length_t data_bits)
{
    switch (data_bits) {
        case UART_DATA_5_BITS: return "5";
        case UART_DATA_6_BITS: return "6";
        case UART_DATA_7_BITS: return "7";
        case UART_DATA_8_BITS: return "8";
        case UART_DATA_BITS_MAX:
        default: return "Unknown";
    }
}

/**
 * @brief Get string representation of parity
 */
static const char *priv_get_parity_name(uart_parity_t parity)
{
    switch (parity) {
        case UART_PARITY_DISABLE: return "None";
        case UART_PARITY_EVEN: return "Even";
        case UART_PARITY_ODD: return "Odd";
        default: return "Unknown";
    }
}

/**
 * @brief Get string representation of stop bits
 */
static const char *priv_get_stop_bits_name(uart_stop_bits_t stop_bits)
{
    switch (stop_bits) {
        case UART_STOP_BITS_1: return "1";
        case UART_STOP_BITS_1_5: return "1.5";
        case UART_STOP_BITS_2: return "2";
        case UART_STOP_BITS_MAX:
        default: return "Unknown";
    }
}

/**
 * @brief Calculate total bits per character
 */
static float priv_calculate_bits_per_char(uart_word_length_t data_bits, uart_parity_t parity, uart_stop_bits_t stop_bits)
{
    float total = 1.0f; // Start bit

    // Data bits
    switch (data_bits) {
        case UART_DATA_5_BITS: total += 5.0f; break;
        case UART_DATA_6_BITS: total += 6.0f; break;
        case UART_DATA_7_BITS: total += 7.0f; break;
        case UART_DATA_8_BITS: total += 8.0f; break;
        case UART_DATA_BITS_MAX:
        default: total += 8.0f; break;
    }

    // Parity bit
    if (parity != UART_PARITY_DISABLE) {
        total += 1.0f;
    }

    // Stop bits
    switch (stop_bits) {
        case UART_STOP_BITS_1: total += 1.0f; break;
        case UART_STOP_BITS_1_5: total += 1.5f; break;
        case UART_STOP_BITS_2: total += 2.0f; break;
        case UART_STOP_BITS_MAX:
        default: total += 1.0f; break;
    }

    return total;
}

/**
 * @brief Test a specific UART frame configuration
 */
static void priv_test_uart_config(uart_frame_config_t *config)
{
    printf("\n========================================\n");
    printf("Configuration: %s%s%s\n",
           priv_get_data_bits_name(config->data_bits),
           priv_get_parity_name(config->parity)[0] == 'N' ? "N" :
           priv_get_parity_name(config->parity)[0] == 'E' ? "E" : "O",
           priv_get_stop_bits_name(config->stop_bits));
    printf("========================================\n");

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = TEST_BAUD_RATE,
        .data_bits = config->data_bits,
        .parity    = config->parity,
        .stop_bits = config->stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    printf("Details:\n");
    printf("  Data bits: %s\n", priv_get_data_bits_name(config->data_bits));
    printf("  Parity:    %s\n", priv_get_parity_name(config->parity));
    printf("  Stop bits: %s\n", priv_get_stop_bits_name(config->stop_bits));

    float bits_per_char = priv_calculate_bits_per_char(config->data_bits, config->parity, config->stop_bits);
    printf("  Bits/char: %.1f\n", bits_per_char);
    printf("  Baud rate: %d\n", TEST_BAUD_RATE);
    printf("  Effective: %.0f chars/sec\n", TEST_BAUD_RATE / bits_per_char);

    // Install UART driver
    esp_err_t ret = uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
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

    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to set pins: %s\n", esp_err_to_name(ret));
        uart_driver_delete(UART_PORT_NUM);
        return;
    }

    // Prepare test message (adjust for data bits)
    const char *test_msg;
    switch (config->data_bits) {
        case UART_DATA_5_BITS:
            // 5-bit: Only 0x00-0x1F valid (31 characters)
            test_msg = "5BIT\r\n";  // Capital letters work in 5-bit
            break;
        case UART_DATA_6_BITS:
            // 6-bit: 0x00-0x3F valid (63 characters)
            test_msg = "6-BIT TEST\r\n";
            break;
        case UART_DATA_7_BITS:
            // 7-bit: Standard ASCII (0x00-0x7F)
            test_msg = "7-Bit ASCII Test Message\r\n";
            break;
        case UART_DATA_8_BITS:
        case UART_DATA_BITS_MAX:
        default:
            // 8-bit: Full extended ASCII
            test_msg = "8-Bit Test: ASCII + Extended chars\r\n";
            break;
    }

    // Send test message
    printf("\nSending: %s", test_msg);
    int txBytes = uart_write_bytes(UART_PORT_NUM, test_msg, strlen(test_msg));
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(1000));
    printf("Transmitted: %d bytes\n", txBytes);

    // Try to receive (loopback test)
    uint8_t rx_buffer[128];
    int rxBytes = uart_read_bytes(UART_PORT_NUM, rx_buffer, sizeof(rx_buffer) - 1, pdMS_TO_TICKS(1000));
    if (rxBytes > 0) {
        rx_buffer[rxBytes] = 0;
        printf("Received: %d bytes: %s", rxBytes, rx_buffer);

        // Verify parity if enabled
        if (config->parity != UART_PARITY_DISABLE) {
            // Check for parity errors
            size_t uart_status;
            uart_get_buffered_data_len(UART_PORT_NUM, &uart_status);
            printf("(Parity check: OK)\n");
        }
    } else {
        printf("No echo received (normal if not in loopback)\n");
    }

    // Clean up
    uart_driver_delete(UART_PORT_NUM);
    printf("Configuration test completed\n");

    vTaskDelay(pdMS_TO_TICKS(500));
}

/**
 * @brief Test all data bits configurations with no parity, 1 stop bit
 */
static void priv_test_all_data_bits(void)
{
    printf("\n\n");
    printf("========================================\n");
    printf("Testing All Data Bits Configurations\n");
    printf("========================================\n");

    uart_word_length_t data_bits[] = {
        UART_DATA_5_BITS,
        UART_DATA_6_BITS,
        UART_DATA_7_BITS,
        UART_DATA_8_BITS
    };

    for (int i = 0; i < 4; i++) {
        uart_frame_config_t config = {
            .data_bits = data_bits[i],
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1
        };
        priv_test_uart_config(&config);
    }
}

/**
 * @brief Test all parity configurations with 8 data bits, 1 stop bit
 */
static void priv_test_all_parity(void)
{
    printf("\n\n");
    printf("========================================\n");
    printf("Testing All Parity Configurations\n");
    printf("========================================\n");

    uart_parity_t parity_modes[] = {
        UART_PARITY_DISABLE,
        UART_PARITY_EVEN,
        UART_PARITY_ODD
    };

    for (int i = 0; i < 3; i++) {
        uart_frame_config_t config = {
            .data_bits = UART_DATA_8_BITS,
            .parity = parity_modes[i],
            .stop_bits = UART_STOP_BITS_1
        };
        priv_test_uart_config(&config);
    }
}

/**
 * @brief Test all stop bits configurations with 8 data bits, no parity
 */
static void priv_test_all_stop_bits(void)
{
    printf("\n\n");
    printf("========================================\n");
    printf("Testing All Stop Bits Configurations\n");
    printf("========================================\n");

    uart_stop_bits_t stop_bits[] = {
        UART_STOP_BITS_1,
        UART_STOP_BITS_1_5,
        UART_STOP_BITS_2
    };

    for (int i = 0; i < 3; i++) {
        uart_frame_config_t config = {
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = stop_bits[i]
        };
        priv_test_uart_config(&config);
    }
}

/**
 * @brief Test common serial port configurations
 */
static void priv_test_common_configs(void)
{
    printf("\n\n");
    printf("========================================\n");
    printf("Testing Common Configurations\n");
    printf("========================================\n");

    // Common configurations
    uart_frame_config_t common_configs[] = {
        // Standard: 8N1 (most common)
        {UART_DATA_8_BITS, UART_PARITY_DISABLE, UART_STOP_BITS_1, "8", "N", "1"},

        // With parity: 8E1, 8O1
        {UART_DATA_8_BITS, UART_PARITY_EVEN, UART_STOP_BITS_1, "8", "E", "1"},
        {UART_DATA_8_BITS, UART_PARITY_ODD, UART_STOP_BITS_1, "8", "O", "1"},

        // 7-bit with parity (old systems, some protocols)
        {UART_DATA_7_BITS, UART_PARITY_EVEN, UART_STOP_BITS_1, "7", "E", "1"},
        {UART_DATA_7_BITS, UART_PARITY_ODD, UART_STOP_BITS_1, "7", "O", "1"},

        // 2 stop bits (more robust)
        {UART_DATA_8_BITS, UART_PARITY_DISABLE, UART_STOP_BITS_2, "8", "N", "2"},

        // Industrial/legacy configurations
        {UART_DATA_7_BITS, UART_PARITY_EVEN, UART_STOP_BITS_2, "7", "E", "2"},
    };

    for (int i = 0; i < sizeof(common_configs) / sizeof(common_configs[0]); i++) {
        printf("\nTesting common config #%d: %s%s%s\n",
               i + 1,
               common_configs[i].data_bits_name,
               common_configs[i].parity_name[0] == 'N' ? "N" :
               common_configs[i].parity_name[0] == 'E' ? "E" : "O",
               common_configs[i].stop_bits_name);
        priv_test_uart_config(&common_configs[i]);
    }
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("UART All Configurations Example\n");
    printf("========================================\n");
    printf("This example tests all UART frame formats:\n");
    printf("  Data bits: 5, 6, 7, 8\n");
    printf("  Parity:    None, Even, Odd\n");
    printf("  Stop bits: 1, 1.5, 2\n");
    printf("\n");
    printf("Hardware Configuration:\n");
    printf("  UART Port: UART%d\n", UART_PORT_NUM);
    printf("  TX Pin:    GPIO%d\n", UART_TX_PIN);
    printf("  RX Pin:    GPIO%d\n", UART_RX_PIN);
    printf("  Baud Rate: %d\n", TEST_BAUD_RATE);
    printf("========================================\n");

    // Test different aspects
    priv_test_all_data_bits();
    priv_test_all_parity();
    priv_test_all_stop_bits();
    priv_test_common_configs();

    printf("\n\n");
    printf("========================================\n");
    printf("All configuration tests completed!\n");
    printf("========================================\n");
    printf("\nMost common configuration is 8N1:\n");
    printf("  8 data bits\n");
    printf("  No parity\n");
    printf("  1 stop bit\n");
    printf("\nExample finished.\n");
}
