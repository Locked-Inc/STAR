/**
 * @file 072_uart_at_commands.c
 * @brief AT command parser implementation example
 *
 * Demonstrates:
 * - AT command state machine
 * - Command parsing and response handling
 * - Timeout management
 * - Unsolicited result codes (URC)
 * - GSM/LTE modem communication patterns
 */

#include "star_bus_uart.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "UART_AT_COMMANDS";

#define UART_TX_PIN (GPIO_NUM_17)
#define UART_RX_PIN (GPIO_NUM_16)
#define UART_NUM    (UART_NUM_1)

typedef enum {
  AT_STATE_IDLE,
  AT_STATE_WAITING_RESPONSE,
  AT_STATE_COMPLETE,
  AT_STATE_ERROR,
  AT_STATE_TIMEOUT
} at_state_t;

typedef struct {
  at_state_t state;
  char       response[512];
  size_t     response_len;
} at_context_t;

/**
 * @brief Send AT command
 */
static esp_err_t priv_at_send_command(star_bus_manager_t *manager, const char *bus_name, const char *cmd)
{
  char      command[128];
  esp_err_t ret;

  /* Format command with CR+LF */
  snprintf(command, sizeof(command), "%s\r\n", cmd);

  ESP_LOGI(s_tag, "Sending: %s", cmd);

  ret = star_bus_uart_write_string(manager, bus_name, command);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to send command: %s", esp_err_to_name(ret));
    return ret;
  }

  return ESP_OK;
}

/**
 * @brief Wait for AT response
 */
static esp_err_t priv_at_wait_response(star_bus_manager_t *manager,
                                       const char *bus_name,
                                       at_context_t *ctx,
                                       uint32_t timeout_ms)
{
  char   line[256];
  size_t line_len;
  uint32_t start_time = xTaskGetTickCount();

  ctx->state        = AT_STATE_WAITING_RESPONSE;
  ctx->response_len = 0;
  memset(ctx->response, 0, sizeof(ctx->response));

  while (1) {
    /* Read line */
    esp_err_t ret = star_bus_uart_read_line(manager, bus_name, line, sizeof(line), &line_len, 100);

    if (ret == ESP_OK && line_len > 0) {
      ESP_LOGI(s_tag, "Received: %s", line);

      /* Append to response buffer */
      if (ctx->response_len + line_len + 1 < sizeof(ctx->response)) {
        strcat(ctx->response, line);
        strcat(ctx->response, "\n");
        ctx->response_len += line_len + 1;
      }

      /* Check for final result codes */
      if (strcmp(line, "OK") == 0) {
        ctx->state = AT_STATE_COMPLETE;
        return ESP_OK;
      } else if (strcmp(line, "ERROR") == 0) {
        ctx->state = AT_STATE_ERROR;
        return ESP_FAIL;
      } else if (strncmp(line, "+CME ERROR:", 11) == 0 ||
                 strncmp(line, "+CMS ERROR:", 11) == 0) {
        ctx->state = AT_STATE_ERROR;
        return ESP_FAIL;
      }
    }

    /* Check timeout */
    uint32_t elapsed = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
    if (elapsed > timeout_ms) {
      ctx->state = AT_STATE_TIMEOUT;
      ESP_LOGW(s_tag, "Command timeout after %lu ms", (unsigned long)elapsed);
      return ESP_ERR_TIMEOUT;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/**
 * @brief Execute AT command and wait for response
 */
static esp_err_t priv_at_execute_command(star_bus_manager_t *manager,
                                         const char *bus_name,
                                         const char *cmd,
                                         at_context_t *ctx,
                                         uint32_t timeout_ms)
{
  esp_err_t ret;

  /* Clear RX buffer */
  star_bus_uart_clear_rx(manager, bus_name);

  /* Send command */
  ret = priv_at_send_command(manager, bus_name, cmd);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Wait for response */
  return priv_at_wait_response(manager, bus_name, ctx, timeout_ms);
}

/**
 * @brief Parse extended response
 */
static void priv_parse_extended_response(const char *response, const char *prefix)
{
  char* line = strstr(response, prefix);
  if (line != NULL) {
    char* colon = strchr(line, ':');
    if (colon != NULL) {
      colon++; /* Skip colon */
      while (*colon == ' ')
        colon++; /* Skip spaces */

      /* Find end of line */
      char* eol = strchr(colon, '\n');
      if (eol != NULL) {
        *eol = '\0';
      }

      ESP_LOGI(s_tag, "  %s: %s", prefix, colon);
    }
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== AT Command Parser Example ===\n");

  /* --- UART Configuration --- */
  ESP_LOGI(s_tag, "--- Configuration ---");

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "at_demo", NULL, NULL);

  star_uart_config_t config = STAR_UART_CONFIG_DEFAULT();
  config.port               = UART_NUM;
  config.tx_pin             = UART_TX_PIN;
  config.rx_pin             = UART_RX_PIN;
  config.baud_rate          = STAR_UART_BAUD_115200;

  ret = star_bus_uart_init(&manager, "uart_at", &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init UART: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(s_tag, "AT command UART initialized\n");

  at_context_t ctx;

  /* --- Example 1: Basic AT Command --- */
  ESP_LOGI(s_tag, "--- Basic AT Command ---");

  ret = priv_at_execute_command(&manager, "uart_at", "AT", &ctx, 1000);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Modem is responding");
  } else {
    ESP_LOGW(s_tag, "No response from modem");
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  /* --- Example 2: Information Query --- */
  ESP_LOGI(s_tag, "\n--- Information Query ---");

  ret = priv_at_execute_command(&manager, "uart_at", "ATI", &ctx, 2000);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Manufacturer information:");
    ESP_LOGI(s_tag, "%s", ctx.response);
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  /* --- Example 3: Extended Command --- */
  ESP_LOGI(s_tag, "\n--- Extended Command (CGMI) ---");

  ret = priv_at_execute_command(&manager, "uart_at", "AT+CGMI", &ctx, 2000);
  if (ret == ESP_OK) {
    priv_parse_extended_response(ctx.response, "+CGMI");
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  /* --- Example 4: Command with Parameters --- */
  ESP_LOGI(s_tag, "\n--- Command with Parameters ---");

  ret = priv_at_execute_command(&manager, "uart_at", "AT+CSQ", &ctx, 2000);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Signal quality response:");
    priv_parse_extended_response(ctx.response, "+CSQ");
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  /* --- Example 5: Set Command --- */
  ESP_LOGI(s_tag, "\n--- Set Command ---");

  ret = priv_at_execute_command(&manager, "uart_at", "AT+CMGF=1", &ctx, 2000);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SMS text mode set successfully");
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  /* --- Example 6: Read Command --- */
  ESP_LOGI(s_tag, "\n--- Read Command ---");

  ret = priv_at_execute_command(&manager, "uart_at", "AT+CMGF?", &ctx, 2000);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "SMS format:");
    priv_parse_extended_response(ctx.response, "+CMGF");
  }

  vTaskDelay(pdMS_TO_TICKS(500));

  /* --- Example 7: Test Command --- */
  ESP_LOGI(s_tag, "\n--- Test Command ---");

  ret = priv_at_execute_command(&manager, "uart_at", "AT+CMGF=?", &ctx, 2000);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Supported SMS formats:");
    priv_parse_extended_response(ctx.response, "+CMGF");
  }

  /* --- Example 8: Long Response Command --- */
  ESP_LOGI(s_tag, "\n--- Long Response Command ---");

  ret = priv_at_execute_command(&manager, "uart_at", "AT+COPS=?", &ctx, 30000);  /* 30s timeout */
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Available operators found");
  } else if (ret == ESP_ERR_TIMEOUT) {
    ESP_LOGW(s_tag, "Command timed out (expected for network scan)");
  }

  /* --- AT Command Summary --- */
  ESP_LOGI(s_tag, "\n--- AT Command Format ---");
  ESP_LOGI(s_tag, "Basic: AT");
  ESP_LOGI(s_tag, "Extended: AT+<CMD>");
  ESP_LOGI(s_tag, "Set: AT+<CMD>=<value>");
  ESP_LOGI(s_tag, "Read: AT+<CMD>?");
  ESP_LOGI(s_tag, "Test: AT+<CMD>=?");

  ESP_LOGI(s_tag, "\n--- Response Format ---");
  ESP_LOGI(s_tag, "Success: OK");
  ESP_LOGI(s_tag, "Error: ERROR");
  ESP_LOGI(s_tag, "Extended: +<CMD>: <data>");
  ESP_LOGI(s_tag, "CME Error: +CME ERROR: <code>");
  ESP_LOGI(s_tag, "CMS Error: +CMS ERROR: <code>");

  ESP_LOGI(s_tag, "\n--- Common Commands ---");
  ESP_LOGI(s_tag, "AT       - Test command");
  ESP_LOGI(s_tag, "ATI      - Identification");
  ESP_LOGI(s_tag, "ATZ      - Reset");
  ESP_LOGI(s_tag, "AT&F     - Factory defaults");
  ESP_LOGI(s_tag, "ATE0/1   - Echo off/on");
  ESP_LOGI(s_tag, "AT+CGMI  - Manufacturer");
  ESP_LOGI(s_tag, "AT+CGMM  - Model");
  ESP_LOGI(s_tag, "AT+CGSN  - Serial number");
  ESP_LOGI(s_tag, "AT+CSCS  - Character set");
  ESP_LOGI(s_tag, "AT+CSQ   - Signal quality");
  ESP_LOGI(s_tag, "AT+COPS  - Operator selection");

  ESP_LOGI(s_tag, "\n--- Best Practices ---");
  ESP_LOGI(s_tag, "1. Use appropriate timeouts (1s for basic, 30s+ for network)");
  ESP_LOGI(s_tag, "2. Clear RX buffer before sending commands");
  ESP_LOGI(s_tag, "3. Handle unsolicited result codes (URC)");
  ESP_LOGI(s_tag, "4. Implement retry logic for critical commands");
  ESP_LOGI(s_tag, "5. Echo commands for debugging (ATE1)");
  ESP_LOGI(s_tag, "6. Disable echo for production (ATE0)");

  /* Cleanup */
  star_bus_uart_deinit(&manager, "uart_at");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nAT command parser example complete");
}
