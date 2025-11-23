/**
 * @file 067_uart_events.c
 * @brief UART example demonstrating all UART event types and handling
 *
 * This example shows how to handle all UART events:
 * - UART_DATA: Data available
 * - UART_BREAK: Break signal detected
 * - UART_BUFFER_FULL: Ring buffer full
 * - UART_FIFO_OVF: FIFO overflow
 * - UART_FRAME_ERR: Frame error
 * - UART_PARITY_ERR: Parity error
 * - UART_DATA_BREAK: Data + break
 * - UART_PATTERN_DET: Pattern detected
 * - UART_EVENT_MAX: Event queue full
 *
 * Hardware Setup:
 * - UART1 TX: GPIO 17
 * - UART1 RX: GPIO 16
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
#define QUEUE_SIZE (20)

static QueueHandle_t s_uart_queue;

// Event statistics
typedef struct {
    uint32_t data_events;
    uint32_t break_events;
    uint32_t buffer_full_events;
    uint32_t fifo_overflow_events;
    uint32_t frame_error_events;
    uint32_t parity_error_events;
    uint32_t pattern_detect_events;
    uint32_t other_events;
    uint32_t total_bytes_received;
} uart_event_stats_t;

static uart_event_stats_t s_stats = {0};

/**
 * @brief Get event type name
 */
static const char *priv_get_event_name(uart_event_type_t type)
{
    switch (type) {
        case UART_DATA: return "UART_DATA";
        case UART_BREAK: return "UART_BREAK";
        case UART_BUFFER_FULL: return "UART_BUFFER_FULL";
        case UART_FIFO_OVF: return "UART_FIFO_OVF";
        case UART_FRAME_ERR: return "UART_FRAME_ERR";
        case UART_PARITY_ERR: return "UART_PARITY_ERR";
        case UART_DATA_BREAK: return "UART_DATA_BREAK";
        case UART_PATTERN_DET: return "UART_PATTERN_DET";
        case UART_EVENT_MAX: return "UART_EVENT_MAX";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Print event statistics
 */
static void priv_print_statistics(void)
{
    printf("\n========================================\n");
    printf("UART Event Statistics\n");
    printf("========================================\n");
    printf("Data events:          %lu\n", s_stats.data_events);
    printf("Break events:         %lu\n", s_stats.break_events);
    printf("Buffer full events:   %lu\n", s_stats.buffer_full_events);
    printf("FIFO overflow events: %lu\n", s_stats.fifo_overflow_events);
    printf("Frame error events:   %lu\n", s_stats.frame_error_events);
    printf("Parity error events:  %lu\n", s_stats.parity_error_events);
    printf("Pattern detect events:%lu\n", s_stats.pattern_detect_events);
    printf("Other events:         %lu\n", s_stats.other_events);
    printf("----------------------------------------\n");
    printf("Total bytes received: %lu\n", s_stats.total_bytes_received);
    printf("========================================\n");
}

/**
 * @brief UART event handler task
 */
static void priv_uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *data_buffer = (uint8_t *)malloc(BUF_SIZE);
    size_t buffered_size;

    printf("[TASK] UART event task started\n");

    while (1) {
        if (xQueueReceive(s_uart_queue, (void *)&event, portMAX_DELAY)) {
            printf("\n[EVENT] %s (size: %d)\n", priv_get_event_name(event.type), event.size);

            switch (event.type) {
                case UART_DATA:
                    s_stats.data_events++;
                    printf("  Available: %d bytes\n", event.size);

                    // Read available data
                    int len = uart_read_bytes(UART_PORT_NUM, data_buffer,
                                             event.size, pdMS_TO_TICKS(100));
                    if (len > 0) {
                        s_stats.total_bytes_received += len;
                        data_buffer[len] = 0;
                        printf("  Data: [%s]\n", data_buffer);

                        // Echo back
                        uart_write_bytes(UART_PORT_NUM, (const char *)data_buffer, len);
                    }
                    break;

                case UART_BREAK:
                    s_stats.break_events++;
                    printf("  Break signal detected on RX line\n");
                    printf("  This indicates a prolonged LOW state\n");
                    break;

                case UART_BUFFER_FULL:
                    s_stats.buffer_full_events++;
                    printf("  WARNING: Ring buffer is full!\n");
                    uart_get_buffered_data_len(UART_PORT_NUM, &buffered_size);
                    printf("  Buffered: %d bytes\n", buffered_size);
                    printf("  Flushing input buffer...\n");
                    uart_flush_input(UART_PORT_NUM);
                    xQueueReset(s_uart_queue);
                    break;

                case UART_FIFO_OVF:
                    s_stats.fifo_overflow_events++;
                    printf("  WARNING: Hardware FIFO overflow!\n");
                    printf("  Data was received faster than it could be processed\n");
                    printf("  Flushing input buffer...\n");
                    uart_flush_input(UART_PORT_NUM);
                    xQueueReset(s_uart_queue);
                    break;

                case UART_FRAME_ERR:
                    s_stats.frame_error_events++;
                    printf("  ERROR: Frame error detected\n");
                    printf("  Possible causes:\n");
                    printf("    - Mismatched baud rate\n");
                    printf("    - Wrong stop bits configuration\n");
                    printf("    - Noisy line or bad connection\n");
                    break;

                case UART_PARITY_ERR:
                    s_stats.parity_error_events++;
                    printf("  ERROR: Parity error detected\n");
                    printf("  Possible causes:\n");
                    printf("    - Mismatched parity configuration\n");
                    printf("    - Data corruption\n");
                    printf("    - Electromagnetic interference\n");
                    break;

                case UART_DATA_BREAK:
                    s_stats.data_events++;
                    s_stats.break_events++;
                    printf("  Data received along with break signal\n");
                    break;

                case UART_PATTERN_DET:
                    s_stats.pattern_detect_events++;
                    printf("  Pattern detected!\n");

                    int pos = uart_pattern_pop_pos(UART_PORT_NUM);
                    if (pos == -1) {
                        printf("  Pattern queue is full\n");
                    } else {
                        printf("  Pattern position: %d\n", pos);

                        // Read data up to pattern
                        len = uart_read_bytes(UART_PORT_NUM, data_buffer,
                                             pos + 1, pdMS_TO_TICKS(100));
                        if (len > 0) {
                            s_stats.total_bytes_received += len;
                            data_buffer[len] = 0;
                            printf("  Data with pattern: [%s]\n", data_buffer);
                        }
                    }
                    break;

                case UART_EVENT_MAX:
                    printf("  Event queue overflow!\n");
                    break;

                default:
                    s_stats.other_events++;
                    printf("  Unknown event type: %d\n", event.type);
                    break;
            }
        }
    }

    free(data_buffer);
    vTaskDelete(NULL);
}

/**
 * @brief Initialize UART with event queue
 */
static esp_err_t priv_uart_events_init(void)
{
    printf("Initializing UART with event queue...\n");

    uart_config_t uart_config = {
        .baud_rate = TEST_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install driver with event queue
    esp_err_t ret = uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, BUF_SIZE * 2,
                                        QUEUE_SIZE, &s_uart_queue, 0);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to install driver: %s\n", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to configure: %s\n", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to set pins: %s\n", esp_err_to_name(ret));
        return ret;
    }

    printf("UART initialized successfully\n");
    printf("  Queue size: %d events\n", QUEUE_SIZE);
    printf("  RX buffer:  %d bytes\n", BUF_SIZE * 2);
    printf("  TX buffer:  %d bytes\n", BUF_SIZE * 2);

    return ESP_OK;
}

/**
 * @brief Test normal data events
 */
static void priv_test_data_events(void)
{
    printf("\n========================================\n");
    printf("Test 1: Normal Data Events\n");
    printf("========================================\n");

    const char *test_messages[] = {
        "Short message",
        "A longer message with more content to test data events",
        "Message 3",
        "Final test message for data events"
    };

    for (int i = 0; i < 4; i++) {
        printf("\nSending message %d...\n", i + 1);
        uart_write_bytes(UART_PORT_NUM, test_messages[i], strlen(test_messages[i]));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Test pattern detection events
 */
static void priv_test_pattern_events(void)
{
    printf("\n========================================\n");
    printf("Test 2: Pattern Detection Events\n");
    printf("========================================\n");

    // Enable pattern detection for newline
    uart_enable_pattern_det_baud_intr(UART_PORT_NUM, '\n', 1, 9, 0, 0);
    printf("Pattern detection enabled for newline (\\n)\n");

    const char *lines[] = {
        "Line 1\n",
        "Line 2\n",
        "Line 3\n"
    };

    for (int i = 0; i < 3; i++) {
        printf("\nSending line %d...\n", i + 1);
        uart_write_bytes(UART_PORT_NUM, lines[i], strlen(lines[i]));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    uart_disable_pattern_det_intr(UART_PORT_NUM);
    printf("\nPattern detection disabled\n");
}

/**
 * @brief Test buffer overflow events
 */
static void priv_test_buffer_overflow(void)
{
    printf("\n========================================\n");
    printf("Test 3: Buffer Overflow Events\n");
    printf("========================================\n");

    printf("Sending large burst to trigger buffer full...\n");
    printf("(Buffer size: %d bytes)\n", BUF_SIZE * 2);

    // Send data larger than buffer without reading
    char large_buffer[BUF_SIZE * 3];
    memset(large_buffer, 'X', sizeof(large_buffer));

    printf("\nSending %d bytes...\n", sizeof(large_buffer));
    int sent = 0;
    while (sent < sizeof(large_buffer)) {
        int to_send = (sizeof(large_buffer) - sent) > 256 ? 256 : (sizeof(large_buffer) - sent);
        uart_write_bytes(UART_PORT_NUM, &large_buffer[sent], to_send);
        sent += to_send;
    }

    printf("Data sent. Waiting for buffer events...\n");
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief Test error events (frame, parity)
 */
static void priv_test_error_events(void)
{
    printf("\n========================================\n");
    printf("Test 4: Error Events\n");
    printf("========================================\n");

    printf("Frame and parity errors typically occur with:\n");
    printf("  - Mismatched configuration between devices\n");
    printf("  - Poor quality connections\n");
    printf("  - Electromagnetic interference\n");
    printf("  - Incorrect baud rate\n");
    printf("\n");
    printf("To test these errors, you would need to:\n");
    printf("  1. Configure peer device with different settings\n");
    printf("  2. Introduce noise on the line\n");
    printf("  3. Use mismatched baud rates\n");
    printf("\n");
    printf("In this demo, we'll reconfigure UART with parity...\n");

    // Clean up current instance
    uart_driver_delete(UART_PORT_NUM);

    // Reinitialize with parity enabled
    uart_config_t uart_config = {
        .baud_rate = TEST_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_EVEN,  // Enable parity checking
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, BUF_SIZE * 2,
                       QUEUE_SIZE, &s_uart_queue, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN,
                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    printf("UART reconfigured with EVEN parity\n");
    printf("If peer sends with different parity, errors will be detected\n");

    vTaskDelay(pdMS_TO_TICKS(2000));

    // Restore normal configuration
    uart_driver_delete(UART_PORT_NUM);
    priv_uart_events_init();
    xTaskCreate(priv_uart_event_task, "uart_event_task", 4096, NULL, 12, NULL);
}

/**
 * @brief Test break signal events
 */
static void priv_test_break_events(void)
{
    printf("\n========================================\n");
    printf("Test 5: Break Signal Events\n");
    printf("========================================\n");

    printf("Break signal is a prolonged LOW state on TX line\n");
    printf("Used for:\n");
    printf("  - Attention/interrupt signaling\n");
    printf("  - Protocol synchronization\n");
    printf("  - Error recovery\n");
    printf("\n");

    // Send break signal
    printf("Sending break signal...\n");
    uart_set_line_inverse(UART_PORT_NUM, UART_SIGNAL_TXD_INV);
    vTaskDelay(pdMS_TO_TICKS(10));  // Hold LOW for 10ms
    uart_set_line_inverse(UART_PORT_NUM, 0);

    printf("Break signal sent\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief Test rapid event generation
 */
static void priv_test_rapid_events(void)
{
    printf("\n========================================\n");
    printf("Test 6: Rapid Event Generation\n");
    printf("========================================\n");

    printf("Sending many small messages rapidly...\n");

    for (int i = 0; i < 20; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Rapid %d\n", i);
        uart_write_bytes(UART_PORT_NUM, msg, strlen(msg));
        vTaskDelay(pdMS_TO_TICKS(50));  // Very short delay
    }

    printf("\nRapid send complete. Processing events...\n");
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("UART Events Example\n");
    printf("========================================\n");
    printf("This example demonstrates all UART event\n");
    printf("types and proper event handling.\n");
    printf("\n");
    printf("Hardware Configuration:\n");
    printf("  UART Port: UART%d\n", UART_PORT_NUM);
    printf("  TX Pin:    GPIO%d\n", UART_TX_PIN);
    printf("  RX Pin:    GPIO%d\n", UART_RX_PIN);
    printf("  Baud Rate: %d\n", TEST_BAUD_RATE);
    printf("\n");
    printf("Event Types:\n");
    printf("  UART_DATA:        Data received\n");
    printf("  UART_BREAK:       Break signal\n");
    printf("  UART_BUFFER_FULL: Buffer overflow\n");
    printf("  UART_FIFO_OVF:    FIFO overflow\n");
    printf("  UART_FRAME_ERR:   Frame error\n");
    printf("  UART_PARITY_ERR:  Parity error\n");
    printf("  UART_PATTERN_DET: Pattern detected\n");
    printf("========================================\n");

    // Initialize UART
    if (priv_uart_events_init() != ESP_OK) {
        printf("Failed to initialize UART\n");
        return;
    }

    // Create event handler task
    xTaskCreate(priv_uart_event_task, "uart_event_task", 4096, NULL, 12, NULL);

    vTaskDelay(pdMS_TO_TICKS(2000));

    // Run all event tests
    priv_test_data_events();
    vTaskDelay(pdMS_TO_TICKS(2000));

    priv_test_pattern_events();
    vTaskDelay(pdMS_TO_TICKS(2000));

    priv_test_buffer_overflow();
    vTaskDelay(pdMS_TO_TICKS(2000));

    priv_test_error_events();
    vTaskDelay(pdMS_TO_TICKS(2000));

    priv_test_break_events();
    vTaskDelay(pdMS_TO_TICKS(2000));

    priv_test_rapid_events();
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Print final statistics
    priv_print_statistics();

    printf("\n========================================\n");
    printf("UART Events Tests Completed!\n");
    printf("========================================\n");
    printf("\nKey Points:\n");
    printf("1. Event queue enables asynchronous handling\n");
    printf("2. Multiple event types can be monitored\n");
    printf("3. Proper error handling prevents data loss\n");
    printf("4. Buffer management is critical\n");
    printf("5. Pattern detection simplifies parsing\n");
    printf("\nExample finished. Event task continues running.\n");
}
