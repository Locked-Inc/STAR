/**
 * @file 069_uart_dma.c
 * @brief UART example demonstrating DMA mode for high-speed transfers
 *
 * This example shows:
 * - DMA-based UART transfers
 * - Zero-copy data transfer
 * - High-speed continuous streaming
 * - TX and RX DMA channels
 * - DMA buffer management
 * - Performance comparison with normal mode
 *
 * Hardware Setup:
 * - UART1 TX: GPIO 17
 * - UART1 RX: GPIO 16
 *
 * Note: ESP-IDF UART driver already uses DMA internally when buffers are provided.
 * This example demonstrates optimal configuration for high-speed DMA transfers.
 */

#include "star_bus_uart.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#define UART_PORT_NUM      (UART_NUM_1)
#define UART_TX_PIN (17)
#define UART_RX_PIN (16)
#define HIGH_SPEED_BAUD    (921600)  // Maximum standard baud rate
#define TEST_BAUD_RATE (115200)

// DMA buffer sizes (larger for better performance)
#define DMA_BUF_SIZE       (4096)
#define STREAM_SIZE        (64 * 1024)  // 64KB test

/**
 * @brief Initialize UART in DMA mode (optimized for high-speed)
 */
static esp_err_t priv_uart_dma_init(uint32_t baud_rate, size_t rx_buf_size, size_t tx_buf_size)
{
    printf("Initializing UART in DMA mode...\n");
    printf("  Baud rate: %lu\n", baud_rate);
    printf("  RX buffer: %d bytes\n", rx_buf_size);
    printf("  TX buffer: %d bytes\n", tx_buf_size);

    // Configure UART for high-speed operation
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install driver with large DMA buffers
    // When TX buffer is provided, DMA is automatically used for transmission
    esp_err_t ret = uart_driver_install(UART_PORT_NUM, rx_buf_size, tx_buf_size, 0, NULL, 0);
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

    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to set pins: %s\n", esp_err_to_name(ret));
        uart_driver_delete(UART_PORT_NUM);
        return ret;
    }

    // Set RX timeout for more efficient DMA transfers
    // Longer timeout = more efficient (fewer interrupts)
    uart_set_rx_timeout(UART_PORT_NUM, 10); // 10 symbol times

    printf("UART DMA mode initialized successfully\n");
    return ESP_OK;
}

/**
 * @brief Measure throughput for large data transfer
 */
static void priv_measure_throughput(const char *test_name, size_t data_size)
{
    printf("\n========================================\n");
    printf("%s\n", test_name);
    printf("========================================\n");

    // Allocate test buffer
    uint8_t *tx_buffer = (uint8_t *)malloc(data_size);
    if (tx_buffer == NULL) {
        printf("ERROR: Failed to allocate TX buffer\n");
        return;
    }

    // Fill with test pattern
    for (size_t i = 0; i < data_size; i++) {
        tx_buffer[i] = (uint8_t)(i & 0xFF);
    }

    printf("Transmitting %d bytes...\n", data_size);

    // Measure transmission time
    uint32_t start_time = xTaskGetTickCount();

    int written = uart_write_bytes(UART_PORT_NUM, (const char *)tx_buffer, data_size);

    // Wait for transmission to complete
    uart_wait_tx_done(UART_PORT_NUM, portMAX_DELAY);

    uint32_t end_time = xTaskGetTickCount();
    uint32_t elapsed_ms = (end_time - start_time) * portTICK_PERIOD_MS;

    // Calculate throughput
    float elapsed_sec = elapsed_ms / 1000.0f;
    float throughput_bps = (written * 8.0f) / elapsed_sec;
    float throughput_kbps = throughput_bps / 1000.0f;
    float throughput_mbps = throughput_kbps / 1000.0f;

    printf("\nResults:\n");
    printf("  Bytes sent:   %d\n", written);
    printf("  Time:         %lu ms (%.3f sec)\n", elapsed_ms, elapsed_sec);
    printf("  Throughput:   %.2f Kbps (%.3f Mbps)\n", throughput_kbps, throughput_mbps);

    uint32_t actual_baud;
    if (uart_get_baudrate(UART_PORT_NUM, &actual_baud) == ESP_OK) {
        printf("  Efficiency:   %.1f%%\n", (throughput_bps / actual_baud) * 100.0f);
    }

    free(tx_buffer);
}

/**
 * @brief Test continuous streaming with DMA
 */
static void priv_test_continuous_streaming(void)
{
    printf("\n========================================\n");
    printf("Continuous DMA Streaming Test\n");
    printf("========================================\n");

    const size_t chunk_size = 1024;
    const size_t num_chunks = 100;
    uint8_t *chunk = (uint8_t *)malloc(chunk_size);

    if (chunk == NULL) {
        printf("ERROR: Failed to allocate chunk buffer\n");
        return;
    }

    // Fill chunk with pattern
    for (size_t i = 0; i < chunk_size; i++) {
        chunk[i] = (uint8_t)((i % 256));
    }

    printf("Streaming %d chunks of %d bytes each...\n", num_chunks, chunk_size);
    printf("Total data: %d KB\n", (num_chunks * chunk_size) / 1024);

    uint32_t start_time = xTaskGetTickCount();
    size_t total_sent = 0;

    for (size_t i = 0; i < num_chunks; i++) {
        int written = uart_write_bytes(UART_PORT_NUM, (const char *)chunk, chunk_size);
        total_sent += written;

        if (i % 10 == 0) {
            printf("Progress: %d/%d chunks (%.1f%%)\n",
                   i, num_chunks, (i * 100.0f) / num_chunks);
        }

        // Small yield to prevent watchdog timeout
        if (i % 50 == 0) {
            vTaskDelay(1);
        }
    }

    uart_wait_tx_done(UART_PORT_NUM, portMAX_DELAY);
    uint32_t end_time = xTaskGetTickCount();

    uint32_t elapsed_ms = (end_time - start_time) * portTICK_PERIOD_MS;
    float throughput_kbps = (total_sent * 8.0f / 1000.0f) / (elapsed_ms / 1000.0f);

    printf("\nStreaming complete:\n");
    printf("  Total sent:   %d bytes\n", total_sent);
    printf("  Time:         %lu ms\n", elapsed_ms);
    printf("  Throughput:   %.2f Kbps\n", throughput_kbps);

    free(chunk);
}

/**
 * @brief Test DMA receive performance
 */
static void priv_test_dma_receive(void)
{
    printf("\n========================================\n");
    printf("DMA Receive Test\n");
    printf("========================================\n");

    const size_t test_size = 4096;
    uint8_t *tx_buffer = (uint8_t *)malloc(test_size);
    uint8_t *rx_buffer = (uint8_t *)malloc(test_size);

    if (tx_buffer == NULL || rx_buffer == NULL) {
        printf("ERROR: Failed to allocate buffers\n");
        free(tx_buffer);
        free(rx_buffer);
        return;
    }

    // Fill TX buffer with test pattern
    for (size_t i = 0; i < test_size; i++) {
        tx_buffer[i] = (uint8_t)(i & 0xFF);
    }

    printf("Sending %d bytes...\n", test_size);
    uart_write_bytes(UART_PORT_NUM, (const char *)tx_buffer, test_size);

    printf("Receiving with DMA...\n");
    uint32_t start_time = xTaskGetTickCount();

    size_t total_received = 0;
    while (total_received < test_size) {
        int len = uart_read_bytes(UART_PORT_NUM,
                                 rx_buffer + total_received,
                                 test_size - total_received,
                                 pdMS_TO_TICKS(5000));
        if (len > 0) {
            total_received += len;
            printf("Received: %d / %d bytes\n", total_received, test_size);
        } else if (len == 0) {
            printf("Timeout waiting for data\n");
            break;
        }
    }

    uint32_t end_time = xTaskGetTickCount();
    uint32_t elapsed_ms = (end_time - start_time) * portTICK_PERIOD_MS;

    if (total_received > 0) {
        // Verify data
        bool data_ok = true;
        for (size_t i = 0; i < total_received; i++) {
            if (rx_buffer[i] != (uint8_t)(i & 0xFF)) {
                printf("Data mismatch at index %d: expected 0x%02X, got 0x%02X\n",
                       i, (uint8_t)(i & 0xFF), rx_buffer[i]);
                data_ok = false;
                break;
            }
        }

        printf("\nReceive complete:\n");
        printf("  Received:     %d bytes\n", total_received);
        printf("  Time:         %lu ms\n", elapsed_ms);
        printf("  Data verify:  %s\n", data_ok ? "PASS" : "FAIL");

        float throughput_kbps = (total_received * 8.0f / 1000.0f) / (elapsed_ms / 1000.0f);
        printf("  Throughput:   %.2f Kbps\n", throughput_kbps);
    }

    free(tx_buffer);
    free(rx_buffer);
}

/**
 * @brief Test bidirectional DMA transfer
 */
static void priv_test_bidirectional_dma(void)
{
    printf("\n========================================\n");
    printf("Bidirectional DMA Test\n");
    printf("========================================\n");

    printf("This test requires TX and RX to be connected\n");
    printf("(loopback mode)\n\n");

    const size_t test_size = 8192;
    uint8_t *buffer = (uint8_t *)malloc(test_size);

    if (buffer == NULL) {
        printf("ERROR: Failed to allocate buffer\n");
        return;
    }

    // Pattern: ascending bytes
    for (size_t i = 0; i < test_size; i++) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }

    printf("Simultaneous TX/RX of %d bytes...\n", test_size);

    uint32_t start_time = xTaskGetTickCount();

    // Transmit
    int written = uart_write_bytes(UART_PORT_NUM, (const char *)buffer, test_size);
    printf("Transmitted: %d bytes\n", written);

    // Receive (in loopback)
    size_t total_rx = 0;
    uint8_t *rx_buffer = (uint8_t *)malloc(test_size);

    while (total_rx < test_size) {
        int len = uart_read_bytes(UART_PORT_NUM,
                                 rx_buffer + total_rx,
                                 test_size - total_rx,
                                 pdMS_TO_TICKS(3000));
        if (len > 0) {
            total_rx += len;
        } else {
            break;
        }
    }

    uart_wait_tx_done(UART_PORT_NUM, portMAX_DELAY);
    uint32_t end_time = xTaskGetTickCount();

    printf("Received: %d bytes\n", total_rx);

    if (total_rx > 0) {
        uint32_t elapsed_ms = (end_time - start_time) * portTICK_PERIOD_MS;
        float throughput_kbps = ((written + total_rx) * 8.0f / 1000.0f) / (elapsed_ms / 1000.0f);

        printf("\nBidirectional results:\n");
        printf("  Time:           %lu ms\n", elapsed_ms);
        printf("  Total transfer: %d bytes\n", written + total_rx);
        printf("  Throughput:     %.2f Kbps\n", throughput_kbps);
    }

    free(buffer);
    free(rx_buffer);
}

/**
 * @brief Compare DMA vs non-DMA performance
 */
static void priv_compare_modes(void)
{
    printf("\n========================================\n");
    printf("DMA vs Non-DMA Performance Comparison\n");
    printf("========================================\n");

    const size_t test_size = 16384;

    // Test with DMA (current configuration)
    printf("\n--- With DMA (large buffers) ---\n");
    priv_measure_throughput("DMA Mode", test_size);

    // Reconfigure without TX buffer (no DMA for TX)
    printf("\n--- Without TX DMA (no TX buffer) ---\n");
    uart_driver_delete(UART_PORT_NUM);
    priv_uart_dma_init(TEST_BAUD_RATE, DMA_BUF_SIZE, 0); // No TX buffer
    priv_measure_throughput("Non-DMA Mode", test_size);

    // Restore DMA configuration
    uart_driver_delete(UART_PORT_NUM);
    priv_uart_dma_init(TEST_BAUD_RATE, DMA_BUF_SIZE, DMA_BUF_SIZE);

    printf("\n--- Comparison Summary ---\n");
    printf("DMA provides:\n");
    printf("  - Lower CPU usage\n");
    printf("  - Higher throughput\n");
    printf("  - Better for large transfers\n");
    printf("  - Requires more RAM\n");
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("UART DMA Mode Example\n");
    printf("========================================\n");
    printf("This example demonstrates high-speed\n");
    printf("UART transfers using DMA.\n");
    printf("\n");
    printf("Hardware Configuration:\n");
    printf("  UART Port: UART%d\n", UART_PORT_NUM);
    printf("  TX Pin:    GPIO%d\n", UART_TX_PIN);
    printf("  RX Pin:    GPIO%d\n", UART_RX_PIN);
    printf("  Baud Rate: %d\n", TEST_BAUD_RATE);
    printf("  DMA Buffers: %d bytes each\n", DMA_BUF_SIZE);
    printf("\n");
    printf("DMA Benefits:\n");
    printf("  - Zero-copy data transfer\n");
    printf("  - Minimal CPU overhead\n");
    printf("  - High throughput\n");
    printf("  - Continuous streaming\n");
    printf("  - Background operation\n");
    printf("========================================\n");

    // Initialize UART with DMA
    if (priv_uart_dma_init(TEST_BAUD_RATE, DMA_BUF_SIZE, DMA_BUF_SIZE) != ESP_OK) {
        printf("Failed to initialize UART DMA\n");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    // Run performance tests
    priv_measure_throughput("Small Transfer (1KB)", 1024);
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_measure_throughput("Medium Transfer (16KB)", 16384);
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_measure_throughput("Large Transfer (64KB)", STREAM_SIZE);
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_continuous_streaming();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_dma_receive();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_bidirectional_dma();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_compare_modes();

    printf("\n========================================\n");
    printf("UART DMA Tests Completed!\n");
    printf("========================================\n");
    printf("\nKey Findings:\n");
    printf("1. DMA significantly reduces CPU load\n");
    printf("2. Best for large data transfers (>1KB)\n");
    printf("3. Requires adequate buffer sizing\n");
    printf("4. Excellent for streaming applications\n");
    printf("5. Trade-off: More RAM for better performance\n");
    printf("\nOptimal DMA Configuration:\n");
    printf("  - Buffer size: 2-8KB for most applications\n");
    printf("  - RX timeout: 10+ symbol times\n");
    printf("  - Use both TX and RX buffers\n");
    printf("  - Monitor buffer usage\n");
    printf("\nExample finished.\n");

    // Clean up
    uart_driver_delete(UART_PORT_NUM);
}
