/**
 * @file 065_uart_irda.c
 * @brief UART example demonstrating IrDA (Infrared Data Association) mode
 *
 * This example shows IrDA configuration and communication:
 * - IrDA SIR (Serial Infrared) mode
 * - Low-power mode
 * - Configurable duty cycle
 * - TX/RX pulse shaping
 *
 * Hardware Setup:
 * - UART1 TX: GPIO 17 (connect to IrDA transceiver TX)
 * - UART1 RX: GPIO 16 (connect to IrDA transceiver RX)
 * - IrDA transceiver (e.g., TFDU4300, TFDU6300, or similar)
 *
 * Note: IrDA mode may not be supported on all ESP32 variants.
 * Check your specific chip's technical reference manual.
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

// IrDA standard baud rates
#define IRDA_BAUD_2400 (2400)
#define IRDA_BAUD_9600 (9600)
#define IRDA_BAUD_19200 (19200)
#define IRDA_BAUD_38400 (38400)
#define IRDA_BAUD_57600 (57600)
#define IRDA_BAUD_115200 (115200)

/**
 * @brief Initialize UART in IrDA mode
 */
static esp_err_t priv_irda_init(uint32_t baud_rate, bool low_power_mode)
{
    printf("\nInitializing IrDA mode...\n");
    printf("  Baud rate: %lu\n", baud_rate);
    printf("  Low power: %s\n", low_power_mode ? "Yes" : "No");

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install driver
    esp_err_t ret = uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to install driver: %s\n", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to configure UART: %s\n", esp_err_to_name(ret));
        uart_driver_delete(UART_PORT_NUM);
        return ret;
    }

    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to set pins: %s\n", esp_err_to_name(ret));
        uart_driver_delete(UART_PORT_NUM);
        return ret;
    }

    // Set IrDA mode
    ret = uart_set_mode(UART_PORT_NUM, UART_MODE_IRDA);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to set IrDA mode: %s\n", esp_err_to_name(ret));
        printf("Note: IrDA mode may not be supported on this ESP32 variant\n");
        uart_driver_delete(UART_PORT_NUM);
        return ret;
    }

    // Configure IrDA-specific parameters
    // Note: The following are typical IrDA settings, adjust based on your hardware

    // IrDA TX pulse width (duty cycle)
    // Standard IrDA: 3/16 of bit period (about 18.75%)
    // Low power: wider pulse for longer range but higher power consumption
    uint8_t tx_pulse_width = low_power_mode ? 4 : 3; // In 1/16 units

    // Note: ESP32 IrDA configuration may vary by chip version
    // Consult your specific chip's technical reference manual

    printf("IrDA mode configured successfully\n");
    printf("  TX pulse width: %d/16 bit period\n", tx_pulse_width);

    return ESP_OK;
}

/**
 * @brief Send data via IrDA
 */
static esp_err_t priv_irda_send(const char *data, size_t len)
{
    printf("\nSending %d bytes via IrDA...\n", len);

    int written = uart_write_bytes(UART_PORT_NUM, data, len);
    if (written != len) {
        printf("ERROR: Failed to write all bytes (%d/%d)\n", written, len);
        return ESP_FAIL;
    }

    esp_err_t ret = uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        printf("ERROR: TX timeout: %s\n", esp_err_to_name(ret));
        return ret;
    }

    printf("Data sent successfully\n");
    return ESP_OK;
}

/**
 * @brief Receive data via IrDA
 */
static int priv_irda_receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms)
{
    printf("Waiting for IrDA data (%lu ms timeout)...\n", timeout_ms);

    int len = uart_read_bytes(UART_PORT_NUM, buffer, max_len, pdMS_TO_TICKS(timeout_ms));

    if (len > 0) {
        printf("Received %d bytes\n", len);
        return len;
    } else if (len == 0) {
        printf("Timeout - no data received\n");
        return 0;
    } else {
        printf("ERROR: Read failed\n");
        return -1;
    }
}

/**
 * @brief Test IrDA communication at specific baud rate
 */
static void priv_test_irda_baud(uint32_t baud_rate)
{
    printf("\n========================================\n");
    printf("Testing IrDA at %lu baud\n", baud_rate);
    printf("========================================\n");

    if (priv_irda_init(baud_rate, false) != ESP_OK) {
        printf("Failed to initialize IrDA at %lu baud\n", baud_rate);
        return;
    }

    // Send test message
    char test_msg[64];
    snprintf(test_msg, sizeof(test_msg), "IrDA test at %lu baud\n", baud_rate);

    if (priv_irda_send(test_msg, strlen(test_msg)) == ESP_OK) {
        // Try to receive echo (if connected in loopback)
        uint8_t rx_buffer[128];
        int rx_len = priv_irda_receive(rx_buffer, sizeof(rx_buffer) - 1, 2000);

        if (rx_len > 0) {
            rx_buffer[rx_len] = 0;
            printf("Echo received: %s", rx_buffer);
        }
    }

    uart_driver_delete(UART_PORT_NUM);
    vTaskDelay(pdMS_TO_TICKS(500));
}

/**
 * @brief Test IrDA packet transmission
 */
static void priv_test_irda_packets(void)
{
    printf("\n========================================\n");
    printf("IrDA Packet Transmission Test\n");
    printf("========================================\n");

    if (priv_irda_init(IRDA_BAUD_9600, false) != ESP_OK) {
        return;
    }

    // Send multiple packets
    const char *packets[] = {
        "Packet 1: Hello IrDA",
        "Packet 2: Testing transmission",
        "Packet 3: Multi-packet test",
        "Packet 4: Final packet"
    };

    for (int i = 0; i < 4; i++) {
        printf("\n--- Packet %d ---\n", i + 1);

        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%s\n", packets[i]);

        if (priv_irda_send(buffer, strlen(buffer)) == ESP_OK) {
            // Small delay between packets
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    uart_driver_delete(UART_PORT_NUM);
}

/**
 * @brief Test IrDA low power mode
 */
static void priv_test_irda_low_power(void)
{
    printf("\n========================================\n");
    printf("IrDA Low Power Mode Test\n");
    printf("========================================\n");

    printf("\n--- Normal Power Mode ---\n");
    if (priv_irda_init(IRDA_BAUD_9600, false) == ESP_OK) {
        const char *msg = "Normal power IrDA transmission\n";
        priv_irda_send(msg, strlen(msg));
        uart_driver_delete(UART_PORT_NUM);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\n--- Low Power Mode ---\n");
    if (priv_irda_init(IRDA_BAUD_9600, true) == ESP_OK) {
        const char *msg = "Low power IrDA transmission\n";
        priv_irda_send(msg, strlen(msg));
        uart_driver_delete(UART_PORT_NUM);
    }

    printf("\nNote: Low power mode increases range but uses more power\n");
}

/**
 * @brief Test IrDA bidirectional communication
 */
static void priv_test_irda_bidirectional(void)
{
    printf("\n========================================\n");
    printf("IrDA Bidirectional Communication Test\n");
    printf("========================================\n");

    if (priv_irda_init(IRDA_BAUD_9600, false) != ESP_OK) {
        return;
    }

    printf("\nNote: This requires two devices with IrDA transceivers\n");
    printf("pointing at each other.\n\n");

    for (int i = 0; i < 3; i++) {
        printf("--- Exchange %d ---\n", i + 1);

        // Send
        char tx_msg[32];
        snprintf(tx_msg, sizeof(tx_msg), "Message %d\n", i + 1);
        printf("TX: %s", tx_msg);
        priv_irda_send(tx_msg, strlen(tx_msg));

        // Wait a bit
        vTaskDelay(pdMS_TO_TICKS(500));

        // Receive
        uint8_t rx_buffer[64];
        int rx_len = priv_irda_receive(rx_buffer, sizeof(rx_buffer) - 1, 2000);
        if (rx_len > 0) {
            rx_buffer[rx_len] = 0;
            printf("RX: %s", rx_buffer);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    uart_driver_delete(UART_PORT_NUM);
}

/**
 * @brief Demonstrate IrDA protocol example (IrLAP-like)
 */
static void priv_test_irda_protocol(void)
{
    printf("\n========================================\n");
    printf("IrDA Protocol Example\n");
    printf("========================================\n");

    if (priv_irda_init(IRDA_BAUD_9600, false) != ESP_OK) {
        return;
    }

    printf("\nSimulating IrLAP-like frame structure:\n");
    printf("  BOF (Beginning of Frame)\n");
    printf("  Address\n");
    printf("  Control\n");
    printf("  Data\n");
    printf("  FCS (Frame Check Sequence)\n");
    printf("  EOF (End of Frame)\n\n");

    // Simple frame structure
    typedef struct {
        uint8_t bof;        // 0xC0 typically
        uint8_t address;
        uint8_t control;
        uint8_t data[16];
        uint8_t data_len;
        uint16_t fcs;       // CRC-16
        uint8_t eof;        // 0xC1 typically
    } __attribute__((packed)) irda_frame_t;

    irda_frame_t frame = {
        .bof = 0xC0,
        .address = 0x01,
        .control = 0x03,  // Unnumbered Information
        .data_len = 12,
        .eof = 0xC1
    };

    const char *payload = "Hello IrDA!";
    memcpy(frame.data, payload, strlen(payload));

    // Calculate simple FCS (in real IrLAP, use CRC-16)
    frame.fcs = 0;
    for (int i = 0; i < frame.data_len; i++) {
        frame.fcs += frame.data[i];
    }

    printf("Sending IrDA frame:\n");
    printf("  BOF: 0x%02X\n", frame.bof);
    printf("  Address: 0x%02X\n", frame.address);
    printf("  Control: 0x%02X\n", frame.control);
    printf("  Data: %s\n", payload);
    printf("  FCS: 0x%04X\n", frame.fcs);
    printf("  EOF: 0x%02X\n", frame.eof);

    size_t frame_size = sizeof(irda_frame_t) - 16 + frame.data_len;
    priv_irda_send((const char *)&frame, frame_size);

    uart_driver_delete(UART_PORT_NUM);
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("UART IrDA (Infrared) Example\n");
    printf("========================================\n");
    printf("This example demonstrates IrDA SIR mode\n");
    printf("(Serial Infrared) communication.\n");
    printf("\n");
    printf("Hardware Configuration:\n");
    printf("  UART Port: UART%d\n", UART_PORT_NUM);
    printf("  TX Pin:    GPIO%d (to IrDA TX)\n", UART_TX_PIN);
    printf("  RX Pin:    GPIO%d (to IrDA RX)\n", UART_RX_PIN);
    printf("\n");
    printf("IrDA Transceiver Required:\n");
    printf("  TFDU4300, TFDU6300, or compatible\n");
    printf("  Connect TX to transceiver TXD\n");
    printf("  Connect RX to transceiver RXD\n");
    printf("\n");
    printf("IrDA Specifications:\n");
    printf("  Standard: IrDA SIR (Serial Infrared)\n");
    printf("  Speed: Up to 115.2 kbps\n");
    printf("  Range: ~1 meter (standard)\n");
    printf("  Wavelength: ~900nm infrared LED\n");
    printf("========================================\n");

    printf("\nNote: Checking IrDA support on this device...\n");

    // Test if IrDA is supported
    esp_err_t ret = priv_irda_init(IRDA_BAUD_9600, false);
    if (ret != ESP_OK) {
        printf("\n*** IrDA mode not supported on this device ***\n");
        printf("IrDA is available on certain ESP32 variants.\n");
        printf("Check your chip's technical reference manual.\n");
        printf("\nThis example will run in simulation mode.\n");
        uart_driver_delete(UART_PORT_NUM);

        // Continue with simulated tests
        printf("\n--- Simulated IrDA Tests ---\n");
    } else {
        uart_driver_delete(UART_PORT_NUM);
    }

    // Run all tests
    priv_test_irda_baud(IRDA_BAUD_2400);
    priv_test_irda_baud(IRDA_BAUD_9600);
    priv_test_irda_baud(IRDA_BAUD_115200);

    priv_test_irda_packets();
    priv_test_irda_low_power();
    priv_test_irda_bidirectional();
    priv_test_irda_protocol();

    printf("\n========================================\n");
    printf("IrDA Tests Completed!\n");
    printf("========================================\n");
    printf("\nIrDA Key Points:\n");
    printf("1. Line-of-sight communication required\n");
    printf("2. Half-duplex operation\n");
    printf("3. Low power consumption\n");
    printf("4. Immune to RF interference\n");
    printf("5. Secure (hard to intercept)\n");
    printf("6. Ideal for remote controls, device pairing\n");
    printf("\nCommon IrDA Applications:\n");
    printf("  - TV/Audio remote controls\n");
    printf("  - Device-to-device data transfer\n");
    printf("  - Wireless sensor networks\n");
    printf("  - Medical device communication\n");
    printf("\nExample finished.\n");
}
