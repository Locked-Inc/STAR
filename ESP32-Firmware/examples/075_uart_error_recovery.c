/**
 * @file 075_uart_error_recovery.c
 * @brief UART error recovery strategies example
 *
 * Demonstrates:
 * - Error detection (frame, parity, overflow)
 * - Error recovery techniques
 * - Retry mechanisms
 * - Buffer overflow handling
 * - Communication resilience
 * - Error statistics and monitoring
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_uart.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "UART_ERROR_RECOVERY";

#define UART_TX_PIN (GPIO_NUM_17)
#define UART_RX_PIN (GPIO_NUM_16)
#define UART_NUM    (UART_NUM_1)
#define BUS_NAME    "error_recovery"

#define MAX_RETRIES (3)
#define RETRY_DELAY_MS (100)
#define COMMAND_TIMEOUT (1000)

typedef enum {
  ERROR_NONE,
  ERROR_TIMEOUT,
  ERROR_FRAME,
  ERROR_PARITY,
  ERROR_OVERFLOW,
  ERROR_CRC,
  ERROR_PROTOCOL
} error_type_t;

typedef struct {
  uint32_t total_commands;
  uint32_t successful_commands;
  uint32_t failed_commands;
  uint32_t timeouts;
  uint32_t retries;
  uint32_t frame_errors;
  uint32_t parity_errors;
  uint32_t overflows;
  uint32_t crc_errors;
} error_stats_t;

static error_stats_t s_stats = {0};

/**
 * @brief Calculate simple CRC for demonstration
 */
static uint8_t priv_calc_simple_crc(const uint8_t *data, size_t len)
{
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
  }
  return crc;
}

/**
 * @brief Send command with CRC
 */
static esp_err_t priv_send_command_with_crc(star_bus_manager_t *manager, const char *bus_name, const char *cmd)
{
  char    packet[128];
  size_t  cmd_len = strlen(cmd);
  uint8_t crc     = priv_calc_simple_crc((const uint8_t*)cmd, cmd_len);

  /* Format: <CMD><CRC>\r\n */
  snprintf(packet, sizeof(packet), "%s%02X\r\n", cmd, crc);

  ESP_LOGI(s_tag, "TX: %s (CRC: 0x%02X)", cmd, crc);

  return star_bus_uart_write_string(manager, bus_name, packet);
}

/**
 * @brief Receive response with validation
 */
static esp_err_t priv_receive_response(star_bus_manager_t *manager,
                                       const char *bus_name,
                                       char *response,
                                       size_t max_len,
                                       uint32_t timeout_ms)
{
  size_t    line_len;
  esp_err_t ret =
    star_bus_uart_read_line(manager, bus_name, response, max_len, &line_len, timeout_ms);

  if (ret != ESP_OK) {
    if (ret == ESP_ERR_TIMEOUT) {
      s_stats.timeouts++;
      ESP_LOGW(s_tag, "Response timeout");
    }
    return ret;
  }

  if (line_len < 3) {
    ESP_LOGE(s_tag, "Response too short");
    return ESP_ERR_INVALID_SIZE;
  }

  /* Extract CRC from end of response */
  char crc_str[3] = {response[line_len - 2], response[line_len - 1], '\0'};
  uint8_t recv_crc = (uint8_t)strtol(crc_str, NULL, 16);

  /* Remove CRC from response */
  response[line_len - 2] = '\0';
  line_len -= 2;

  /* Calculate expected CRC */
  uint8_t calc_crc = priv_calc_simple_crc((const uint8_t*)response, line_len);

  if (recv_crc != calc_crc) {
    s_stats.crc_errors++;
    ESP_LOGE(s_tag, "CRC error: expected 0x%02X, got 0x%02X", calc_crc, recv_crc);
    return ESP_ERR_INVALID_CRC;
  }

  ESP_LOGI(s_tag, "RX: %s (CRC OK)", response);
  return ESP_OK;
}

/**
 * @brief Execute command with retry mechanism
 */
static esp_err_t priv_execute_command_with_retry(star_bus_manager_t *manager,
                                                 const char *bus_name,
                                                 const char *cmd,
                                                 char *response,
                                                 size_t response_size)
{
  esp_err_t ret;
  int       attempt = 0;

  s_stats.total_commands++;

  while (attempt < MAX_RETRIES) {
    if (attempt > 0) {
      s_stats.retries++;
      ESP_LOGW(s_tag, "Retry attempt %d/%d", attempt + 1, MAX_RETRIES);
      vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS * attempt)); /* Exponential backoff */
    }

    /* Clear RX buffer before sending */
    star_bus_uart_clear_rx(manager, bus_name);

    /* Send command */
    ret = priv_send_command_with_crc(manager, bus_name, cmd);
    if (ret != ESP_OK) {
      ESP_LOGE(s_tag, "Failed to send command");
      attempt++;
      continue;
    }

    /* Wait for response */
    ret = priv_receive_response(manager, bus_name, response, response_size, COMMAND_TIMEOUT);
    if (ret == ESP_OK) {
      s_stats.successful_commands++;
      return ESP_OK;
    }

    /* Handle specific errors */
    if (ret == ESP_ERR_TIMEOUT) {
      ESP_LOGW(s_tag, "Timeout, retrying...");
    } else if (ret == ESP_ERR_INVALID_CRC) {
      ESP_LOGW(s_tag, "CRC error, retrying...");
    } else {
      ESP_LOGW(s_tag, "Error: %s, retrying...", esp_err_to_name(ret));
    }

    attempt++;
  }

  s_stats.failed_commands++;
  ESP_LOGE(s_tag, "Command failed after %d attempts", MAX_RETRIES);
  return ESP_FAIL;
}

/**
 * @brief Handle buffer overflow
 */
static void priv_handle_buffer_overflow(star_bus_manager_t *manager, const char *bus_name)
{
  ESP_LOGW(s_tag, "Buffer overflow detected!");
  s_stats.overflows++;

  /* Strategy 1: Clear buffer */
  star_bus_uart_clear_rx(manager, bus_name);
  ESP_LOGI(s_tag, "RX buffer cleared");

  /* Strategy 2: Reset statistics (optional) */
  // star_bus_uart_reset_stats(config);

  /* Strategy 3: Increase buffer size (requires reinit) */
  ESP_LOGI(s_tag, "Consider increasing RX buffer size in configuration");
}

/**
 * @brief Monitor UART health
 */
static void priv_monitor_uart_health(star_bus_manager_t *manager, const char *bus_name)
{
  star_uart_stats_t stats;
  esp_err_t         ret = star_bus_uart_get_stats(manager, bus_name, &stats);

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to get UART stats");
    return;
  }

  ESP_LOGI(s_tag, "--- UART Health Status ---");
  ESP_LOGI(s_tag, "TX bytes: %lu", (unsigned long)stats.bytes_sent);
  ESP_LOGI(s_tag, "RX bytes: %lu", (unsigned long)stats.bytes_received);
  ESP_LOGI(s_tag, "Parity errors: %lu", (unsigned long)stats.parity_errors);
  ESP_LOGI(s_tag, "Frame errors: %lu", (unsigned long)stats.frame_errors);
  ESP_LOGI(s_tag, "Overrun errors: %lu", (unsigned long)stats.overrun_errors);

  /* Check for errors */
  if (stats.parity_errors > 0) {
    ESP_LOGW(s_tag, "Parity errors detected!");
    s_stats.parity_errors += stats.parity_errors;
  }

  if (stats.frame_errors > 0) {
    ESP_LOGW(s_tag, "Frame errors detected!");
    s_stats.frame_errors += stats.frame_errors;
  }

  if (stats.overrun_errors > 0) {
    ESP_LOGW(s_tag, "Buffer overruns detected!");
    priv_handle_buffer_overflow(manager, bus_name);
  }

  /* Calculate error rate */
  if (stats.bytes_sent + stats.bytes_received > 0) {
    float error_rate = (float)(stats.parity_errors + stats.frame_errors) /
                       (float)(stats.bytes_sent + stats.bytes_received) * 100.0f;
    ESP_LOGI(s_tag, "Error rate: %.2f%%", error_rate);

    if (error_rate > 1.0f) {
      ESP_LOGW(s_tag, "High error rate! Check connections and baud rate");
    }
  }
}

/**
 * @brief Print error statistics
 */
static void priv_print_error_stats(void)
{
  ESP_LOGI(s_tag, "\n--- Error Statistics ---");
  ESP_LOGI(s_tag, "Total commands: %lu", (unsigned long)s_stats.total_commands);
  ESP_LOGI(s_tag, "Successful: %lu", (unsigned long)s_stats.successful_commands);
  ESP_LOGI(s_tag, "Failed: %lu", (unsigned long)s_stats.failed_commands);
  ESP_LOGI(s_tag, "Timeouts: %lu", (unsigned long)s_stats.timeouts);
  ESP_LOGI(s_tag, "Retries: %lu", (unsigned long)s_stats.retries);
  ESP_LOGI(s_tag, "Frame errors: %lu", (unsigned long)s_stats.frame_errors);
  ESP_LOGI(s_tag, "Parity errors: %lu", (unsigned long)s_stats.parity_errors);
  ESP_LOGI(s_tag, "Overflows: %lu", (unsigned long)s_stats.overflows);
  ESP_LOGI(s_tag, "CRC errors: %lu", (unsigned long)s_stats.crc_errors);

  if (s_stats.total_commands > 0) {
    float success_rate =
      (float)s_stats.successful_commands / (float)s_stats.total_commands * 100.0f;
    ESP_LOGI(s_tag, "Success rate: %.2f%%", success_rate);
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== UART Error Recovery Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "error_recovery_mgr", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* --- Configuration --- */
  star_uart_config_t config = STAR_UART_CONFIG_DEFAULT();
  config.port               = UART_NUM;
  config.tx_pin             = UART_TX_PIN;
  config.rx_pin             = UART_RX_PIN;
  config.baud_rate          = STAR_UART_BAUD_115200;

  ret = star_bus_uart_init(&manager, BUS_NAME, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init UART: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "UART initialized\n");

  /* --- Example 1: Successful Command --- */
  ESP_LOGI(s_tag, "--- Example 1: Successful Command ---");

  char response[128];
  ret = priv_execute_command_with_retry(&manager, BUS_NAME, "PING", response, sizeof(response));
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Command successful: %s", response);
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  /* --- Example 2: Timeout Recovery --- */
  ESP_LOGI(s_tag, "\n--- Example 2: Timeout Handling ---");

  ret = priv_execute_command_with_retry(&manager, BUS_NAME, "SLOW", response, sizeof(response));
  if (ret != ESP_OK) {
    ESP_LOGW(s_tag, "Command failed (expected if no device connected)");
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  /* --- Example 3: Multiple Commands --- */
  ESP_LOGI(s_tag, "\n--- Example 3: Multiple Commands ---");

  const char* commands[] = {"CMD1", "CMD2", "CMD3", "CMD4", "CMD5"};

  for (int i = 0; i < 5; i++) {
    ret = priv_execute_command_with_retry(&manager, BUS_NAME, commands[i], response, sizeof(response));
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Command %s: OK", commands[i]);
    } else {
      ESP_LOGW(s_tag, "Command %s: FAILED", commands[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  /* --- Example 4: Buffer Management --- */
  ESP_LOGI(s_tag, "\n--- Example 4: Buffer Management ---");

  size_t available = 0;
  star_bus_uart_get_available(&manager, BUS_NAME, &available);
  ESP_LOGI(s_tag, "Available bytes in RX buffer: %zu", available);

  if (available > 100) {
    ESP_LOGW(s_tag, "Large amount of unread data!");
    star_bus_uart_clear_rx(&manager, BUS_NAME);
    ESP_LOGI(s_tag, "Buffer cleared");
  }

  /* --- Example 5: Health Monitoring --- */
  ESP_LOGI(s_tag, "\n--- Example 5: Health Monitoring ---");

  priv_monitor_uart_health(&manager, BUS_NAME);

  /* --- Example 6: Error Statistics --- */
  ESP_LOGI(s_tag, "\n--- Example 6: Error Statistics ---");

  priv_print_error_stats();

  /* --- Recovery Strategies Summary --- */
  ESP_LOGI(s_tag, "\n--- Error Recovery Strategies ---");
  ESP_LOGI(s_tag, "1. Timeout Recovery:");
  ESP_LOGI(s_tag, "   - Use reasonable timeouts");
  ESP_LOGI(s_tag, "   - Implement retry with exponential backoff");
  ESP_LOGI(s_tag, "   - Clear RX buffer before retry");

  ESP_LOGI(s_tag, "\n2. CRC/Checksum Errors:");
  ESP_LOGI(s_tag, "   - Request retransmission");
  ESP_LOGI(s_tag, "   - Log error for analysis");
  ESP_LOGI(s_tag, "   - Consider improving signal integrity");

  ESP_LOGI(s_tag, "\n3. Buffer Overflow:");
  ESP_LOGI(s_tag, "   - Increase buffer size");
  ESP_LOGI(s_tag, "   - Process data more frequently");
  ESP_LOGI(s_tag, "   - Use hardware flow control");
  ESP_LOGI(s_tag, "   - Clear buffer periodically");

  ESP_LOGI(s_tag, "\n4. Frame Errors:");
  ESP_LOGI(s_tag, "   - Check baud rate configuration");
  ESP_LOGI(s_tag, "   - Verify cable quality");
  ESP_LOGI(s_tag, "   - Check for electrical interference");
  ESP_LOGI(s_tag, "   - Ensure proper grounding");

  ESP_LOGI(s_tag, "\n5. Communication Loss:");
  ESP_LOGI(s_tag, "   - Implement heartbeat/ping");
  ESP_LOGI(s_tag, "   - Auto-reconnect mechanism");
  ESP_LOGI(s_tag, "   - Watchdog timer");
  ESP_LOGI(s_tag, "   - Reset UART peripheral");

  ESP_LOGI(s_tag, "\n--- Best Practices ---");
  ESP_LOGI(s_tag, "1. Always use checksums for critical data");
  ESP_LOGI(s_tag, "2. Implement proper timeout handling");
  ESP_LOGI(s_tag, "3. Use retry mechanism with backoff");
  ESP_LOGI(s_tag, "4. Monitor error statistics");
  ESP_LOGI(s_tag, "5. Clear buffers between transactions");
  ESP_LOGI(s_tag, "6. Log errors for debugging");
  ESP_LOGI(s_tag, "7. Use hardware flow control when available");
  ESP_LOGI(s_tag, "8. Implement watchdog for hung connections");
  ESP_LOGI(s_tag, "9. Test error scenarios during development");
  ESP_LOGI(s_tag, "10. Document recovery procedures");

  /* Cleanup */
  star_bus_uart_deinit(&manager, BUS_NAME);
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nError recovery example complete");
}
