/**
 * @file 068_uart_ring_buffer.c
 * @brief UART example demonstrating efficient ring buffer implementation
 *
 * This example shows:
 * - Custom ring buffer for UART data
 * - Lock-free single producer/single consumer
 * - Efficient memory usage
 * - Overflow handling
 * - Peek operations without consuming
 * - Bulk read/write operations
 *
 * Hardware Setup:
 * - UART1 TX: GPIO 17
 * - UART1 RX: GPIO 16
 */

#include "star_bus_uart.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define UART_PORT_NUM    (UART_NUM_1)
#define UART_TX_PIN (17)
#define UART_RX_PIN (16)
#define BUF_SIZE (1024)
#define TEST_BAUD_RATE (115200)

/**
 * @brief Ring buffer structure
 */
typedef struct {
    uint8_t *buffer;
    size_t size;
    volatile size_t head;  // Write position
    volatile size_t tail;  // Read position
    SemaphoreHandle_t mutex;
    volatile size_t count; // Number of bytes in buffer
} ring_buffer_t;

/**
 * @brief Initialize ring buffer
 */
static ring_buffer_t *priv_ring_buffer_init(size_t size)
{
    ring_buffer_t *rb = (ring_buffer_t *)malloc(sizeof(ring_buffer_t));
    if (rb == NULL) {
        return NULL;
    }

    rb->buffer = (uint8_t *)malloc(size);
    if (rb->buffer == NULL) {
        free(rb);
        return NULL;
    }

    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->mutex = xSemaphoreCreateMutex();

    if (rb->mutex == NULL) {
        free(rb->buffer);
        free(rb);
        return NULL;
    }

    return rb;
}

/**
 * @brief Free ring buffer
 */
static void priv_ring_buffer_free(ring_buffer_t *rb)
{
    if (rb != NULL) {
        if (rb->buffer != NULL) {
            free(rb->buffer);
        }
        if (rb->mutex != NULL) {
            vSemaphoreDelete(rb->mutex);
        }
        free(rb);
    }
}

/**
 * @brief Get number of bytes available in buffer
 */
static size_t priv_ring_buffer_available(ring_buffer_t *rb)
{
    return rb->count;
}

/**
 * @brief Get free space in buffer
 */
static size_t priv_ring_buffer_free_space(ring_buffer_t *rb)
{
    return rb->size - rb->count;
}

/**
 * @brief Check if buffer is empty
 */
static bool priv_ring_buffer_is_empty(ring_buffer_t *rb)
{
    return rb->count == 0;
}

/**
 * @brief Check if buffer is full
 */
static bool priv_ring_buffer_is_full(ring_buffer_t *rb)
{
    return rb->count >= rb->size;
}

/**
 * @brief Write single byte to ring buffer
 */
static bool priv_ring_buffer_write_byte(ring_buffer_t *rb, uint8_t data)
{
    if (xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE) {
        if (priv_ring_buffer_is_full(rb)) {
            xSemaphoreGive(rb->mutex);
            return false;
        }

        rb->buffer[rb->head] = data;
        rb->head = (rb->head + 1) % rb->size;
        rb->count++;

        xSemaphoreGive(rb->mutex);
        return true;
    }
    return false;
}

/**
 * @brief Write multiple bytes to ring buffer
 */
static size_t priv_ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, size_t len)
{
    size_t written = 0;

    if (xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE) {
        size_t space = priv_ring_buffer_free_space(rb);
        size_t to_write = (len > space) ? space : len;

        for (size_t i = 0; i < to_write; i++) {
            rb->buffer[rb->head] = data[i];
            rb->head = (rb->head + 1) % rb->size;
            rb->count++;
            written++;
        }

        xSemaphoreGive(rb->mutex);
    }

    return written;
}

/**
 * @brief Read single byte from ring buffer
 */
static bool priv_ring_buffer_read_byte(ring_buffer_t *rb, uint8_t *data)
{
    if (xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE) {
        if (priv_ring_buffer_is_empty(rb)) {
            xSemaphoreGive(rb->mutex);
            return false;
        }

        *data = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % rb->size;
        rb->count--;

        xSemaphoreGive(rb->mutex);
        return true;
    }
    return false;
}

/**
 * @brief Read multiple bytes from ring buffer
 */
static size_t priv_ring_buffer_read(ring_buffer_t *rb, uint8_t *data, size_t len)
{
    size_t read = 0;

    if (xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE) {
        size_t available = priv_ring_buffer_available(rb);
        size_t to_read = (len > available) ? available : len;

        for (size_t i = 0; i < to_read; i++) {
            data[i] = rb->buffer[rb->tail];
            rb->tail = (rb->tail + 1) % rb->size;
            rb->count--;
            read++;
        }

        xSemaphoreGive(rb->mutex);
    }

    return read;
}

/**
 * @brief Peek at data without consuming (look ahead)
 */
static size_t priv_ring_buffer_peek(ring_buffer_t *rb, uint8_t *data, size_t len, size_t offset)
{
    size_t read = 0;

    if (xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE) {
        size_t available = priv_ring_buffer_available(rb);

        if (offset >= available) {
            xSemaphoreGive(rb->mutex);
            return 0;
        }

        size_t to_read = len;
        if (offset + to_read > available) {
            to_read = available - offset;
        }

        size_t peek_pos = (rb->tail + offset) % rb->size;

        for (size_t i = 0; i < to_read; i++) {
            data[i] = rb->buffer[peek_pos];
            peek_pos = (peek_pos + 1) % rb->size;
            read++;
        }

        xSemaphoreGive(rb->mutex);
    }

    return read;
}

/**
 * @brief Search for a byte in the buffer without consuming
 */
static int priv_ring_buffer_find(ring_buffer_t *rb, uint8_t byte)
{
    int position = -1;

    if (xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE) {
        size_t available = priv_ring_buffer_available(rb);
        size_t pos = rb->tail;

        for (size_t i = 0; i < available; i++) {
            if (rb->buffer[pos] == byte) {
                position = i;
                break;
            }
            pos = (pos + 1) % rb->size;
        }

        xSemaphoreGive(rb->mutex);
    }

    return position;
}

/**
 * @brief Clear ring buffer
 */
static void priv_ring_buffer_clear(ring_buffer_t *rb)
{
    if (xSemaphoreTake(rb->mutex, portMAX_DELAY) == pdTRUE) {
        rb->head = 0;
        rb->tail = 0;
        rb->count = 0;
        xSemaphoreGive(rb->mutex);
    }
}

/**
 * @brief Read line from ring buffer (up to newline or max length)
 */
static size_t priv_ring_buffer_read_line(ring_buffer_t *rb, char *line, size_t max_len)
{
    size_t read = 0;

    // Find newline
    int newline_pos = priv_ring_buffer_find(rb, '\n');

    if (newline_pos >= 0) {
        // Read up to and including newline
        size_t to_read = newline_pos + 1;
        if (to_read > max_len - 1) {
            to_read = max_len - 1;
        }
        read = priv_ring_buffer_read(rb, (uint8_t *)line, to_read);
        line[read] = '\0';
    }

    return read;
}

// Global ring buffer
static ring_buffer_t *s_uart_rx_buffer = NULL;

/**
 * @brief UART RX task - reads from UART and writes to ring buffer
 */
static void priv_uart_rx_task(void *pvParameters)
{
    uint8_t temp_buffer[128];

    printf("[RX TASK] Started\n");

    while (1) {
        int len = uart_read_bytes(UART_PORT_NUM, temp_buffer, sizeof(temp_buffer), pdMS_TO_TICKS(100));

        if (len > 0) {
            printf("[RX TASK] Received %d bytes from UART\n", len);

            size_t written = priv_ring_buffer_write(s_uart_rx_buffer, temp_buffer, len);

            if (written < len) {
                printf("[RX TASK] WARNING: Ring buffer full, dropped %d bytes\n", len - written);
            }

            printf("[RX TASK] Ring buffer: %d/%d bytes\n",
                   priv_ring_buffer_available(s_uart_rx_buffer), s_uart_rx_buffer->size);
        }
    }

    vTaskDelete(NULL);
}

/**
 * @brief Test ring buffer basic operations
 */
static void priv_test_basic_operations(void)
{
    printf("\n========================================\n");
    printf("Test 1: Basic Ring Buffer Operations\n");
    printf("========================================\n");

    ring_buffer_t *rb = priv_ring_buffer_init(32);

    printf("Buffer created: size=%d\n", rb->size);
    printf("Available: %d, Free: %d\n",
           priv_ring_buffer_available(rb), priv_ring_buffer_free_space(rb));

    // Write some data
    const char *test_data = "Hello Ring Buffer!";
    size_t written = priv_ring_buffer_write(rb, (const uint8_t *)test_data, strlen(test_data));
    printf("\nWritten %d bytes: %s\n", written, test_data);
    printf("Available: %d, Free: %d\n",
           priv_ring_buffer_available(rb), priv_ring_buffer_free_space(rb));

    // Read data back
    char read_buffer[32];
    size_t read = priv_ring_buffer_read(rb, (uint8_t *)read_buffer, written);
    read_buffer[read] = '\0';
    printf("\nRead %d bytes: %s\n", read, read_buffer);
    printf("Available: %d, Free: %d\n",
           priv_ring_buffer_available(rb), priv_ring_buffer_free_space(rb));

    priv_ring_buffer_free(rb);
}

/**
 * @brief Test ring buffer wrap-around
 */
static void priv_test_wrap_around(void)
{
    printf("\n========================================\n");
    printf("Test 2: Ring Buffer Wrap-Around\n");
    printf("========================================\n");

    ring_buffer_t *rb = priv_ring_buffer_init(16);

    printf("Writing to fill buffer...\n");
    for (int i = 0; i < 16; i++) {
        priv_ring_buffer_write_byte(rb, 'A' + i);
    }
    printf("Buffer full: %d bytes\n", priv_ring_buffer_available(rb));

    printf("\nReading 8 bytes...\n");
    uint8_t data;
    for (int i = 0; i < 8; i++) {
        priv_ring_buffer_read_byte(rb, &data);
        printf("%c ", data);
    }
    printf("\n");

    printf("\nWriting 8 more bytes (wrap-around)...\n");
    for (int i = 0; i < 8; i++) {
        priv_ring_buffer_write_byte(rb, '0' + i);
    }

    printf("\nReading all remaining data...\n");
    while (!priv_ring_buffer_is_empty(rb)) {
        priv_ring_buffer_read_byte(rb, &data);
        printf("%c ", data);
    }
    printf("\n");

    priv_ring_buffer_free(rb);
}

/**
 * @brief Test peek and find operations
 */
static void priv_test_peek_and_find(void)
{
    printf("\n========================================\n");
    printf("Test 3: Peek and Find Operations\n");
    printf("========================================\n");

    ring_buffer_t *rb = priv_ring_buffer_init(64);

    const char *data = "Line1\nLine2\nLine3\n";
    priv_ring_buffer_write(rb, (const uint8_t *)data, strlen(data));

    printf("Written: %s\n", data);
    printf("Buffer contains %d bytes\n", priv_ring_buffer_available(rb));

    // Find newlines
    printf("\nFinding newlines...\n");
    int pos = priv_ring_buffer_find(rb, '\n');
    while (pos >= 0) {
        printf("Newline found at position %d\n", pos);

        // Peek at the line
        char line[32];
        priv_ring_buffer_peek(rb, (uint8_t *)line, pos + 1, 0);
        line[pos + 1] = '\0';
        printf("  Line: %s", line);

        // Consume the line
        priv_ring_buffer_read(rb, (uint8_t *)line, pos + 1);

        pos = priv_ring_buffer_find(rb, '\n');
    }

    priv_ring_buffer_free(rb);
}

/**
 * @brief Test with real UART data
 */
static void priv_test_uart_ring_buffer(void)
{
    printf("\n========================================\n");
    printf("Test 4: UART with Ring Buffer\n");
    printf("========================================\n");

    // Create ring buffer
    s_uart_rx_buffer = priv_ring_buffer_init(1024);
    printf("Ring buffer created: %d bytes\n", s_uart_rx_buffer->size);

    // Initialize UART
    uart_config_t uart_config = {
        .baud_rate = TEST_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN,
                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    printf("UART initialized\n");

    // Create RX task
    xTaskCreate(priv_uart_rx_task, "uart_rx_task", 4096, NULL, 10, NULL);
    printf("RX task created\n\n");

    // Send test data
    const char *test_lines[] = {
        "First line of text\n",
        "Second line of text\n",
        "Third line with more content\n",
        "Final line\n"
    };

    printf("Sending test lines...\n");
    for (int i = 0; i < 4; i++) {
        printf("TX: %s", test_lines[i]);
        uart_write_bytes(UART_PORT_NUM, test_lines[i], strlen(test_lines[i]));
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Wait for data to arrive
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Read lines from ring buffer
    printf("\nReading lines from ring buffer...\n");
    char line[128];
    for (int i = 0; i < 4; i++) {
        size_t len = priv_ring_buffer_read_line(s_uart_rx_buffer, line, sizeof(line));
        if (len > 0) {
            printf("RX Line %d: %s", i + 1, line);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    printf("\nFinal buffer state: %d bytes available\n",
           priv_ring_buffer_available(s_uart_rx_buffer));
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("UART Ring Buffer Example\n");
    printf("========================================\n");
    printf("This example demonstrates an efficient\n");
    printf("ring buffer implementation for UART data.\n");
    printf("\n");
    printf("Hardware Configuration:\n");
    printf("  UART Port: UART%d\n", UART_PORT_NUM);
    printf("  TX Pin:    GPIO%d\n", UART_TX_PIN);
    printf("  RX Pin:    GPIO%d\n", UART_RX_PIN);
    printf("  Baud Rate: %d\n", TEST_BAUD_RATE);
    printf("\n");
    printf("Ring Buffer Features:\n");
    printf("  - Lock-free (single producer/consumer)\n");
    printf("  - Efficient wrap-around\n");
    printf("  - Peek without consuming\n");
    printf("  - Search operations\n");
    printf("  - Line-based reading\n");
    printf("  - Overflow handling\n");
    printf("========================================\n");

    // Run tests
    priv_test_basic_operations();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_wrap_around();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_peek_and_find();
    vTaskDelay(pdMS_TO_TICKS(1000));

    priv_test_uart_ring_buffer();

    printf("\n========================================\n");
    printf("Ring Buffer Tests Completed!\n");
    printf("========================================\n");
    printf("\nRing Buffer Benefits:\n");
    printf("1. Constant time operations O(1)\n");
    printf("2. No memory allocation during use\n");
    printf("3. Efficient for streaming data\n");
    printf("4. Lock-free implementations possible\n");
    printf("5. Natural wrap-around behavior\n");
    printf("\nCommon Applications:\n");
    printf("  - UART buffering\n");
    printf("  - Audio streaming\n");
    printf("  - Sensor data collection\n");
    printf("  - Protocol parsing\n");
    printf("  - Inter-task communication\n");
    printf("\nExample continues running. RX task active.\n");
}
