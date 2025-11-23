/**
 * @file 009_uart_basic.c
 * @brief Basic UART operations example
 *
 * Demonstrates:
 * - UART configuration and initialization
 * - Writing and reading data
 * - String operations
 * - Buffer management
 * - Line reading
 */

#include "star_bus_manager.h"
#include "star_bus_uart.h"

#include <esp_log.h>
#include <string.h>

static const char *s_tag = "UART_BASIC_EXAMPLE";

#define UART_TX_PIN (GPIO_NUM_17)
#define UART_RX_PIN (GPIO_NUM_16)
#define UART_NUM    (UART_NUM_1)

static star_bus_manager_t s_manager;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Basic UART Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "uart_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* --- Default Configuration --- */
  ESP_LOGI(s_tag, "--- Default Configuration ---");

  star_uart_config_t config = STAR_UART_CONFIG_DEFAULT();
  config.port      = UART_NUM;
  config.tx_pin    = UART_TX_PIN;
  config.rx_pin    = UART_RX_PIN;
  config.baud_rate = STAR_UART_BAUD_115200;

  ESP_LOGI(s_tag, "UART Port: %d", config.port);
  ESP_LOGI(s_tag, "TX Pin: %d", config.tx_pin);
  ESP_LOGI(s_tag, "RX Pin: %d", config.rx_pin);
  ESP_LOGI(s_tag, "Baud: %d", config.baud_rate);
  ESP_LOGI(s_tag, "Data bits: %d", config.data_bits);
  ESP_LOGI(s_tag, "Stop bits: %d", config.stop_bits);
  ESP_LOGI(s_tag, "Parity: %d", config.parity);

  /* Initialize UART */
  ret = star_bus_uart_init(&s_manager, "uart1", &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init UART: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(s_tag, "UART initialized successfully");

  /* --- Write Operations --- */
  ESP_LOGI(s_tag, "\n--- Write Operations ---");

  /* Write raw bytes */
  uint8_t raw_data[] = {0x55, 0xAA, 0x00, 0xFF};
  ret = star_bus_uart_write(&s_manager, "uart1", raw_data, sizeof(raw_data));
  ESP_LOGI(s_tag, "Wrote %d raw bytes: %s", (int)sizeof(raw_data),
           esp_err_to_name(ret));

  /* Write string */
  const char *message = "Hello UART!\r\n";
  ret = star_bus_uart_write_string(&s_manager, "uart1", message);
  ESP_LOGI(s_tag, "Wrote string: %s", esp_err_to_name(ret));

  /* --- Read Operations --- */
  ESP_LOGI(s_tag, "\n--- Read Operations ---");

  /* Check available data */
  size_t available;
  ret = star_bus_uart_get_available(&s_manager, "uart1", &available);
  ESP_LOGI(s_tag, "Available bytes in RX buffer: %d", (int)available);

  /* Read raw bytes */
  uint8_t read_buffer[64];
  size_t  bytes_read;
  ret = star_bus_uart_read(&s_manager, "uart1", read_buffer, sizeof(read_buffer),
                           &bytes_read, 100);
  if (ret == ESP_OK && bytes_read > 0) {
    ESP_LOGI(s_tag, "Read %d bytes", (int)bytes_read);
    ESP_LOG_BUFFER_HEX(s_tag, read_buffer, bytes_read);
  } else {
    ESP_LOGI(s_tag, "No data available or timeout");
  }

  /* Read line (until newline) */
  char   line_buffer[128];
  size_t line_len;
  ret = star_bus_uart_read_line(&s_manager, "uart1", line_buffer, sizeof(line_buffer),
                                &line_len, 100);
  if (ret == ESP_OK && line_len > 0) {
    ESP_LOGI(s_tag, "Read line (%d chars): %s", (int)line_len, line_buffer);
  } else {
    ESP_LOGI(s_tag, "No line available or timeout");
  }

  /* --- Buffer Management --- */
  ESP_LOGI(s_tag, "\n--- Buffer Management ---");

  /* Flush TX buffer (wait for all data to be sent) */
  ret = star_bus_uart_flush(&s_manager, "uart1");
  ESP_LOGI(s_tag, "Flush TX: %s", esp_err_to_name(ret));

  /* Clear RX buffer */
  ret = star_bus_uart_clear_rx(&s_manager, "uart1");
  ESP_LOGI(s_tag, "Clear RX: %s", esp_err_to_name(ret));

  ret = star_bus_uart_get_available(&s_manager, "uart1", &available);
  ESP_LOGI(s_tag, "Available after clear: %d", (int)available);

  /* --- Different Configurations --- */
  ESP_LOGI(s_tag, "\n--- Configuration Options ---");

  /* Deinit first */
  star_bus_uart_deinit(&s_manager, "uart1");

  /* 9600 baud, 7 data bits, even parity, 2 stop bits */
  star_uart_config_t custom_config = STAR_UART_CONFIG_DEFAULT();
  custom_config.port      = UART_NUM;
  custom_config.tx_pin    = UART_TX_PIN;
  custom_config.rx_pin    = UART_RX_PIN;
  custom_config.baud_rate = STAR_UART_BAUD_9600;
  custom_config.data_bits = STAR_UART_DATA_7_BITS;
  custom_config.parity    = STAR_UART_PARITY_EVEN;
  custom_config.stop_bits = STAR_UART_STOP_2_BITS;

  ESP_LOGI(s_tag, "Custom config: 9600 7E2");
  ret = star_bus_uart_init(&s_manager, "uart1", &custom_config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Custom UART initialized");
    star_bus_uart_deinit(&s_manager, "uart1");
  }

  /* High speed configuration */
  star_uart_config_t fast_config = STAR_UART_CONFIG_DEFAULT();
  fast_config.port      = UART_NUM;
  fast_config.tx_pin    = UART_TX_PIN;
  fast_config.rx_pin    = UART_RX_PIN;
  fast_config.baud_rate = STAR_UART_BAUD_921600;

  ESP_LOGI(s_tag, "High speed config: 921600 8N1");
  ret = star_bus_uart_init(&s_manager, "uart1", &fast_config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "High speed UART initialized");
    star_bus_uart_deinit(&s_manager, "uart1");
  }

  /* --- Statistics --- */
  ESP_LOGI(s_tag, "\n--- UART Statistics ---");

  /* Re-init for stats demo */
  ret = star_bus_uart_init(&s_manager, "uart1", &config);
  if (ret == ESP_OK) {
    /* Do some operations */
    star_bus_uart_write_string(&s_manager, "uart1", "Test 1\r\n");
    star_bus_uart_write_string(&s_manager, "uart1", "Test 2\r\n");
    star_bus_uart_flush(&s_manager, "uart1");

    /* Get statistics */
    star_uart_stats_t stats;
    ret = star_bus_uart_get_stats(&s_manager, "uart1", &stats);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "TX bytes: %lu", (unsigned long)stats.bytes_sent);
      ESP_LOGI(s_tag, "RX bytes: %lu", (unsigned long)stats.bytes_received);
      ESP_LOGI(s_tag, "TX ops: %lu", (unsigned long)stats.tx_operations);
      ESP_LOGI(s_tag, "RX ops: %lu", (unsigned long)stats.rx_operations);
      ESP_LOGI(s_tag, "Overruns: %lu", (unsigned long)stats.overrun_errors);
    }

    /* Print formatted stats */
    star_bus_uart_print_stats("uart1", &stats);

    /* Reset statistics */
    star_bus_uart_reset_stats(&s_manager, "uart1");
    ESP_LOGI(s_tag, "Statistics reset");
  }

  /* Cleanup */
  star_bus_uart_deinit(&s_manager, "uart1");
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nBasic UART example complete");
}
