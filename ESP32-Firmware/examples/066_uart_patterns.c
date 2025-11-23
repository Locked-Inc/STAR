/**
 * @file 066_uart_patterns.c
 * @brief UART example demonstrating pattern detection for framing
 *
 * This example shows how to use UART pattern detection to identify:
 * - Newline characters (CR, LF, CRLF)
 * - Custom delimiters
 * - Escape sequences
 * - Frame markers
 * - Protocol-specific patterns
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
#define BUF_SIZE (2048)
#define TEST_BAUD_RATE (115200)
#define PATTERN_QUEUE_SIZE (20)

static QueueHandle_t s_uart_queue;

/**
 * @brief UART event handler task
 */
static void priv_uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *data_buffer = (uint8_t *)malloc(BUF_SIZE);

    while (1) {
        if (xQueueReceive(s_uart_queue, (void *)&event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    printf("[EVENT] UART_DATA: %d bytes available\n", event.size);
                    break;

                case UART_PATTERN_DET:
                    printf("[EVENT] UART_PATTERN_DET detected\n");

                    // Get pattern position
                    int pos = uart_pattern_pop_pos(UART_PORT_NUM);
                    if (pos == -1) {
                        printf("  Pattern queue full, flushing...\n");
                        uart_flush_input(UART_PORT_NUM);
                    } else {
                        printf("  Pattern found at position: %d\n", pos);

                        // Read data up to pattern
                        int read_len = uart_read_bytes(UART_PORT_NUM, data_buffer,
                                                       pos + 1, pdMS_TO_TICKS(100));
                        if (read_len > 0) {
                            data_buffer[read_len] = 0;
                            printf("  Data before pattern: [%s]\n", data_buffer);
                        }
                    }
                    break;

                case UART_FIFO_OVF:
                    printf("[EVENT] UART_FIFO_OVF - FIFO overflow\n");
                    uart_flush_input(UART_PORT_NUM);
                    break;

                case UART_BUFFER_FULL:
                    printf("[EVENT] UART_BUFFER_FULL - Ring buffer full\n");
                    uart_flush_input(UART_PORT_NUM);
                    break;

                case UART_BREAK:
                    printf("[EVENT] UART_BREAK detected\n");
                    break;

                case UART_PARITY_ERR:
                    printf("[EVENT] UART_PARITY_ERR\n");
                    break;

                case UART_FRAME_ERR:
                    printf("[EVENT] UART_FRAME_ERR\n");
                    break;

                default:
                    printf("[EVENT] Unknown event type: %d\n", event.type);
                    break;
            }
        }
    }

    free(data_buffer);
    vTaskDelete(NULL);
}

/**
 * @brief Initialize UART with pattern detection
 */
static esp_err_t priv_uart_pattern_init(void)
{
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
                                        PATTERN_QUEUE_SIZE, &s_uart_queue, 0);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to install driver: %s\n", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to configure UART: %s\n", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to set pins: %s\n", esp_err_to_name(ret));
        return ret;
    }

    // Create event handler task
    xTaskCreate(priv_uart_event_task, "uart_event_task", 4096, NULL, 12, NULL);

    printf("UART pattern detection initialized\n");
    return ESP_OK;
}

/**
 * @brief Test newline pattern detection (LF)
 */
static void priv_test_newline_pattern(void)
{
    printf("\n========================================\n");
    printf("Test 1: Newline Pattern (LF = 0x0A)\n");
    printf("========================================\n");

    // Enable pattern detection for LF (newline)
    esp_err_t ret = uart_enable_pattern_det_baud_intr(UART_PORT_NUM, '\n', 1, 9, 0, 0);
    if (ret != ESP_OK) {
        printf("ERROR: Failed to enable pattern detection: %s\n", esp_err_to_name(ret));
        return;
    }

    printf("Pattern detection enabled for LF (\\n)\n");
    printf("Sending test lines...\n\n");

    const char *lines[] = {
        "First line\n",
        "Second line\n",
        "Third line with more text\n",
        "Final line\n"
    };

    for (int i = 0; i < 4; i++) {
        printf("TX: %s", lines[i]);
        uart_write_bytes(UART_PORT_NUM, lines[i], strlen(lines[i]));
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    uart_disable_pattern_det_intr(UART_PORT_NUM);
    printf("\nPattern detection disabled\n");
}

/**
 * @brief Test CR+LF pattern detection
 */
static void priv_test_crlf_pattern(void)
{
    printf("\n========================================\n");
    printf("Test 2: CRLF Pattern (0x0D 0x0A)\n");
    printf("========================================\n");

    // For CRLF, we detect LF and handle CR manually
    uart_enable_pattern_det_baud_intr(UART_PORT_NUM, '\n', 1, 9, 0, 0);

    printf("Pattern detection enabled for CRLF (\\r\\n)\n");
    printf("Sending DOS-style lines...\n\n");

    const char *dos_lines[] = {
        "DOS line 1\r\n",
        "DOS line 2\r\n",
        "DOS line 3\r\n"
    };

    for (int i = 0; i < 3; i++) {
        printf("TX: [%s]", dos_lines[i]);
        uart_write_bytes(UART_PORT_NUM, dos_lines[i], strlen(dos_lines[i]));
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    uart_disable_pattern_det_intr(UART_PORT_NUM);
    printf("\nPattern detection disabled\n");
}

/**
 * @brief Test custom delimiter pattern
 */
static void priv_test_custom_delimiter(void)
{
    printf("\n========================================\n");
    printf("Test 3: Custom Delimiter (|)\n");
    printf("========================================\n");

    // Enable pattern detection for pipe character
    uart_enable_pattern_det_baud_intr(UART_PORT_NUM, '|', 1, 9, 0, 0);

    printf("Pattern detection enabled for '|' delimiter\n");
    printf("Sending delimited data...\n\n");

    const char *test_data = "field1|field2|field3|field4|field5|";

    printf("TX: %s\n", test_data);
    uart_write_bytes(UART_PORT_NUM, test_data, strlen(test_data));

    vTaskDelay(pdMS_TO_TICKS(2000));
    uart_disable_pattern_det_intr(UART_PORT_NUM);
    printf("\nPattern detection disabled\n");
}

/**
 * @brief Test frame marker pattern
 */
static void priv_test_frame_markers(void)
{
    printf("\n========================================\n");
    printf("Test 4: Frame Markers (STX/ETX)\n");
    printf("========================================\n");

    // Detect ETX (0x03) as frame end marker
    uart_enable_pattern_det_baud_intr(UART_PORT_NUM, 0x03, 1, 9, 0, 0);

    printf("Pattern detection enabled for ETX (0x03)\n");
    printf("Sending framed messages...\n\n");

    // STX (0x02) ... data ... ETX (0x03)
    const char frame1[] = {0x02, 'F', 'r', 'a', 'm', 'e', ' ', '1', 0x03, 0};
    const char frame2[] = {0x02, 'F', 'r', 'a', 'm', 'e', ' ', '2', 0x03, 0};
    const char frame3[] = {0x02, 'F', 'r', 'a', 'm', 'e', ' ', '3', 0x03, 0};

    printf("TX: Frame 1 with STX/ETX markers\n");
    uart_write_bytes(UART_PORT_NUM, frame1, strlen(frame1));
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("TX: Frame 2 with STX/ETX markers\n");
    uart_write_bytes(UART_PORT_NUM, frame2, strlen(frame2));
    vTaskDelay(pdMS_TO_TICKS(500));

    printf("TX: Frame 3 with STX/ETX markers\n");
    uart_write_bytes(UART_PORT_NUM, frame3, strlen(frame3));
    vTaskDelay(pdMS_TO_TICKS(500));

    vTaskDelay(pdMS_TO_TICKS(1000));
    uart_disable_pattern_det_intr(UART_PORT_NUM);
    printf("\nPattern detection disabled\n");
}

/**
 * @brief Test escape sequence pattern
 */
static void priv_test_escape_sequences(void)
{
    printf("\n========================================\n");
    printf("Test 5: Escape Sequence Pattern (ESC)\n");
    printf("========================================\n");

    // Detect ESC character (0x1B)
    uart_enable_pattern_det_baud_intr(UART_PORT_NUM, 0x1B, 1, 9, 0, 0);

    printf("Pattern detection enabled for ESC (0x1B)\n");
    printf("Sending ANSI escape sequences...\n\n");

    // ANSI color codes
    const char *sequences[] = {
        "Normal text\x1B[31mRed text\x1B[0m\n",      // Red
        "Normal text\x1B[32mGreen text\x1B[0m\n",    // Green
        "Normal text\x1B[33mYellow text\x1B[0m\n",   // Yellow
    };

    for (int i = 0; i < 3; i++) {
        printf("TX: ANSI sequence %d\n", i + 1);
        uart_write_bytes(UART_PORT_NUM, sequences[i], strlen(sequences[i]));
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    uart_disable_pattern_det_intr(UART_PORT_NUM);
    printf("\nPattern detection disabled\n");
}

/**
 * @brief Test multiple character pattern
 */
static void priv_test_multi_char_pattern(void)
{
    printf("\n========================================\n");
    printf("Test 6: Multi-Character Pattern (+++)\n");
    printf("========================================\n");

    // Detect three consecutive '+' characters (like Hayes AT command guard)
    uart_enable_pattern_det_baud_intr(UART_PORT_NUM, '+', 3, 9, 0, 0);

    printf("Pattern detection enabled for '+++' (3 consecutive)\n");
    printf("Sending test data...\n\n");

    const char *test_strings[] = {
        "Normal text without pattern\n",
        "Text with single + character\n",
        "Text with ++ two characters\n",
        "+++",  // This should trigger pattern detection
        "Text after pattern\n",
    };

    for (int i = 0; i < 5; i++) {
        printf("TX: %s", test_strings[i]);
        uart_write_bytes(UART_PORT_NUM, test_strings[i], strlen(test_strings[i]));
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    uart_disable_pattern_det_intr(UART_PORT_NUM);
    printf("\nPattern detection disabled\n");
}

/**
 * @brief Test pattern with timeout (gap detection)
 */
static void priv_test_pattern_with_gap(void)
{
    printf("\n========================================\n");
    printf("Test 7: Pattern with Gap Detection\n");
    printf("========================================\n");

    // Detect newline with gap timeout
    // post_idle: character timeout after pattern (in bit times)
    // pre_idle: character timeout before pattern (in bit times)
    uart_enable_pattern_det_baud_intr(UART_PORT_NUM, '\n', 1, 9, 10, 10);

    printf("Pattern detection with gap analysis enabled\n");
    printf("Sending lines with different gaps...\n\n");

    printf("TX: Fast transmission...\n");
    uart_write_bytes(UART_PORT_NUM, "Line1\n", 6);
    uart_write_bytes(UART_PORT_NUM, "Line2\n", 6);
    uart_write_bytes(UART_PORT_NUM, "Line3\n", 6);

    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("\nTX: Slow transmission with gaps...\n");
    uart_write_bytes(UART_PORT_NUM, "Slow1\n", 6);
    vTaskDelay(pdMS_TO_TICKS(200));
    uart_write_bytes(UART_PORT_NUM, "Slow2\n", 6);
    vTaskDelay(pdMS_TO_TICKS(200));
    uart_write_bytes(UART_PORT_NUM, "Slow3\n", 6);

    vTaskDelay(pdMS_TO_TICKS(1000));
    uart_disable_pattern_det_intr(UART_PORT_NUM);
    printf("\nPattern detection disabled\n");
}

/**
 * @brief Test protocol-specific pattern (AT commands)
 */
static void priv_test_at_command_pattern(void)
{
    printf("\n========================================\n");
    printf("Test 8: AT Command Pattern\n");
    printf("========================================\n");

    // Detect CR (0x0D) which terminates AT commands
    uart_enable_pattern_det_baud_intr(UART_PORT_NUM, '\r', 1, 9, 0, 0);

    printf("Pattern detection for AT command terminator (CR)\n");
    printf("Sending AT commands...\n\n");

    const char *at_commands[] = {
        "AT\r",
        "AT+CGMI\r",
        "AT+CGMM\r",
        "AT+CGMR\r",
        "AT+CGSN\r",
        "ATI\r"
    };

    for (int i = 0; i < 6; i++) {
        printf("TX: %s\n", at_commands[i]);
        uart_write_bytes(UART_PORT_NUM, at_commands[i], strlen(at_commands[i]));
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    uart_disable_pattern_det_intr(UART_PORT_NUM);
    printf("\nPattern detection disabled\n");
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("UART Pattern Detection Example\n");
    printf("========================================\n");
    printf("This example demonstrates UART pattern\n");
    printf("detection for various framing scenarios.\n");
    printf("\n");
    printf("Hardware Configuration:\n");
    printf("  UART Port: UART%d\n", UART_PORT_NUM);
    printf("  TX Pin:    GPIO%d\n", UART_TX_PIN);
    printf("  RX Pin:    GPIO%d\n", UART_RX_PIN);
    printf("  Baud Rate: %d\n", TEST_BAUD_RATE);
    printf("\n");
    printf("Pattern Detection Features:\n");
    printf("  - Single/multi-character patterns\n");
    printf("  - Newline detection (LF, CR, CRLF)\n");
    printf("  - Custom delimiters\n");
    printf("  - Frame markers (STX, ETX)\n");
    printf("  - Escape sequences\n");
    printf("  - Gap detection (idle time)\n");
    printf("========================================\n");

    // Initialize UART with pattern detection
    if (priv_uart_pattern_init() != ESP_OK) {
        printf("Failed to initialize UART\n");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    // Run all pattern tests
    priv_test_newline_pattern();
    priv_test_crlf_pattern();
    priv_test_custom_delimiter();
    priv_test_frame_markers();
    priv_test_escape_sequences();
    priv_test_multi_char_pattern();
    priv_test_pattern_with_gap();
    priv_test_at_command_pattern();

    printf("\n========================================\n");
    printf("Pattern Detection Tests Completed!\n");
    printf("========================================\n");
    printf("\nPattern Detection Benefits:\n");
    printf("1. Efficient line/frame parsing\n");
    printf("2. Hardware-accelerated detection\n");
    printf("3. Reduces CPU overhead\n");
    printf("4. Handles high-speed data streams\n");
    printf("5. Supports protocol parsing\n");
    printf("\nCommon Use Cases:\n");
    printf("  - AT command parsing\n");
    printf("  - Line-based protocols (text)\n");
    printf("  - Frame synchronization\n");
    printf("  - CSV/delimiter parsing\n");
    printf("  - Terminal emulation\n");
    printf("\nExample finished.\n");
}
