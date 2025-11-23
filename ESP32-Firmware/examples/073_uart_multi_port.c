/**
 * @file 073_uart_multi_port.c
 * @brief Multiple UART ports example
 *
 * Demonstrates:
 * - Using multiple UART ports simultaneously
 * - Independent configuration per port
 * - Data routing between ports
 * - Port management and statistics
 * - UART bridge implementation
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_uart.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char *s_tag = "UART_MULTI_PORT";

/* UART0 - Console (usually used by logger) */
#define UART0_TX_PIN (GPIO_NUM_1)
#define UART0_RX_PIN (GPIO_NUM_3)

/* UART1 - External device */
#define UART1_TX_PIN (GPIO_NUM_17)
#define UART1_RX_PIN (GPIO_NUM_16)

/* UART2 - Another external device */
#define UART2_TX_PIN (GPIO_NUM_25)
#define UART2_RX_PIN (GPIO_NUM_26)

typedef struct {
  const char *bus_name;
  const char *name;
  uint32_t   msg_count;
} multi_uart_port_t;

/**
 * @brief Initialize UART port
 */
static esp_err_t priv_init_uart_port(star_bus_manager_t *manager, multi_uart_port_t *port, const star_uart_config_t *config)
{
  esp_err_t ret = star_bus_uart_init(manager, port->bus_name, config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init %s: %s", port->name, esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_tag, "%s initialized (TX:%d RX:%d, %d baud)",
           port->name,
           config->tx_pin,
           config->rx_pin,
           config->baud_rate);

  return ESP_OK;
}

/**
 * @brief Send message on port
 */
static esp_err_t priv_send_on_port(star_bus_manager_t *manager, multi_uart_port_t *port, const char *message)
{
  esp_err_t ret = star_bus_uart_write_string(manager, port->bus_name, message);
  if (ret == ESP_OK) {
    port->msg_count++;
    ESP_LOGI(s_tag, "[%s] TX: %s", port->name, message);
  } else {
    ESP_LOGE(s_tag, "[%s] TX failed: %s", port->name, esp_err_to_name(ret));
  }
  return ret;
}

/**
 * @brief Receive message on port
 */
static esp_err_t priv_receive_on_port(star_bus_manager_t *manager, multi_uart_port_t *port, char *buffer, size_t buffer_size)
{
  size_t    line_len;
  esp_err_t ret =
    star_bus_uart_read_line(manager, port->bus_name, buffer, buffer_size, &line_len, 100);

  if (ret == ESP_OK && line_len > 0) {
    ESP_LOGI(s_tag, "[%s] RX: %s", port->name, buffer);
    return ESP_OK;
  }

  return ESP_ERR_TIMEOUT;
}

/**
 * @brief Print port statistics
 */
static void priv_print_port_stats(star_bus_manager_t *manager, multi_uart_port_t *port)
{
  star_uart_stats_t stats;
  esp_err_t         ret = star_bus_uart_get_stats(manager, port->bus_name, &stats);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "[%s] Statistics:", port->name);
    ESP_LOGI(s_tag, "  Messages sent: %lu", (unsigned long)port->msg_count);
    ESP_LOGI(s_tag, "  TX bytes: %lu", (unsigned long)stats.bytes_sent);
    ESP_LOGI(s_tag, "  RX bytes: %lu", (unsigned long)stats.bytes_received);
    ESP_LOGI(s_tag, "  Parity errors: %lu", (unsigned long)stats.parity_errors);
    ESP_LOGI(s_tag, "  Frame errors: %lu", (unsigned long)stats.frame_errors);
    ESP_LOGI(s_tag, "  Overrun errors: %lu", (unsigned long)stats.overrun_errors);
  }
}

/**
 * @brief Bridge data between two ports
 */
static void priv_bridge_ports(star_bus_manager_t *manager, multi_uart_port_t *port1, multi_uart_port_t *port2, uint32_t duration_ms)
{
  ESP_LOGI(s_tag, "Bridging %s <-> %s for %lu ms",
           port1->name,
           port2->name,
           (unsigned long)duration_ms);

  uint32_t start_time  = xTaskGetTickCount();
  uint32_t bridge_time = 0;
  char     buffer[256];
  size_t   bytes_read;

  while (bridge_time < duration_ms) {
    /* Port1 -> Port2 */
    esp_err_t ret =
      star_bus_uart_read(manager, port1->bus_name, (uint8_t*)buffer, sizeof(buffer), &bytes_read, 10);
    if (ret == ESP_OK && bytes_read > 0) {
      star_bus_uart_write(manager, port2->bus_name, (uint8_t*)buffer, bytes_read);
      ESP_LOGI(s_tag, "Bridge: %s -> %s (%zu bytes)", port1->name, port2->name, bytes_read);
    }

    /* Port2 -> Port1 */
    ret = star_bus_uart_read(manager, port2->bus_name, (uint8_t*)buffer, sizeof(buffer), &bytes_read, 10);
    if (ret == ESP_OK && bytes_read > 0) {
      star_bus_uart_write(manager, port1->bus_name, (uint8_t*)buffer, bytes_read);
      ESP_LOGI(s_tag, "Bridge: %s -> %s (%zu bytes)", port2->name, port1->name, bytes_read);
    }

    bridge_time = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  ESP_LOGI(s_tag, "Bridge completed");
}

void app_main(void)
{
  ESP_LOGI(s_tag, "=== Multiple UART Ports Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  esp_err_t ret = star_bus_manager_init(&manager, "multi_uart", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* --- Port Configuration --- */
  ESP_LOGI(s_tag, "--- Configuring Ports ---");

  multi_uart_port_t uart1 = {
    .bus_name = "uart1",
    .name = "UART1",
    .msg_count = 0
  };

  star_uart_config_t config1 = STAR_UART_CONFIG_DEFAULT();
  config1.port      = UART_NUM_1;
  config1.tx_pin    = UART1_TX_PIN;
  config1.rx_pin    = UART1_RX_PIN;
  config1.baud_rate = STAR_UART_BAUD_115200;

  multi_uart_port_t uart2 = {
    .bus_name = "uart2",
    .name = "UART2",
    .msg_count = 0
  };

  star_uart_config_t config2 = STAR_UART_CONFIG_DEFAULT();
  config2.port      = UART_NUM_2;
  config2.tx_pin    = UART2_TX_PIN;
  config2.rx_pin    = UART2_RX_PIN;
  config2.baud_rate = STAR_UART_BAUD_9600;

  /* Initialize ports */
  if (priv_init_uart_port(&manager, &uart1, &config1) != ESP_OK ||
      priv_init_uart_port(&manager, &uart2, &config2) != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to initialize ports");
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "All ports initialized\n");

  /* --- Example 1: Independent Communication --- */
  ESP_LOGI(s_tag, "--- Independent Communication ---");

  priv_send_on_port(&manager, &uart1, "Hello from UART1!\r\n");
  priv_send_on_port(&manager, &uart2, "Hello from UART2!\r\n");

  vTaskDelay(pdMS_TO_TICKS(500));

  char buffer[128];
  priv_receive_on_port(&manager, &uart1, buffer, sizeof(buffer));
  priv_receive_on_port(&manager, &uart2, buffer, sizeof(buffer));

  /* --- Example 2: Broadcast Message --- */
  ESP_LOGI(s_tag, "\n--- Broadcast Message ---");

  const char* broadcast_msg = "Broadcast from ESP32\r\n";
  ESP_LOGI(s_tag, "Broadcasting: %s", broadcast_msg);

  priv_send_on_port(&manager, &uart1, broadcast_msg);
  priv_send_on_port(&manager, &uart2, broadcast_msg);

  vTaskDelay(pdMS_TO_TICKS(500));

  /* --- Example 3: Port-Specific Operations --- */
  ESP_LOGI(s_tag, "\n--- Port-Specific Operations ---");

  /* High speed data on UART1 */
  ESP_LOGI(s_tag, "Sending high-speed data on UART1...");
  for (int i = 0; i < 5; i++) {
    char msg[32];
    snprintf(msg, sizeof(msg), "Fast data %d\r\n", i);
    priv_send_on_port(&manager, &uart1, msg);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  /* Low speed commands on UART2 */
  ESP_LOGI(s_tag, "Sending low-speed commands on UART2...");
  for (int i = 0; i < 3; i++) {
    char msg[32];
    snprintf(msg, sizeof(msg), "CMD%d\r\n", i);
    priv_send_on_port(&manager, &uart2, msg);
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  /* --- Example 4: Buffer Status --- */
  ESP_LOGI(s_tag, "\n--- Buffer Status ---");

  size_t avail1 = 0;
  size_t avail2 = 0;
  star_bus_uart_get_available(&manager, uart1.bus_name, &avail1);
  star_bus_uart_get_available(&manager, uart2.bus_name, &avail2);

  ESP_LOGI(s_tag, "UART1 RX buffer: %zu bytes available", avail1);
  ESP_LOGI(s_tag, "UART2 RX buffer: %zu bytes available", avail2);

  /* --- Example 5: Buffer Management --- */
  ESP_LOGI(s_tag, "\n--- Buffer Management ---");

  ESP_LOGI(s_tag, "Flushing UART1 TX buffer...");
  star_bus_uart_flush(&manager, uart1.bus_name);

  ESP_LOGI(s_tag, "Clearing UART2 RX buffer...");
  star_bus_uart_clear_rx(&manager, uart2.bus_name);

  star_bus_uart_get_available(&manager, uart2.bus_name, &avail2);
  ESP_LOGI(s_tag, "UART2 RX buffer after clear: %zu bytes", avail2);

  /* --- Example 6: UART Bridge --- */
  ESP_LOGI(s_tag, "\n--- UART Bridge Mode ---");
  ESP_LOGI(s_tag, "Note: In real application, this would forward data between ports");

  /* Simulate bridge for 2 seconds */
  // priv_bridge_ports(&manager, &uart1, &uart2, 2000);

  /* --- Example 7: Different Configurations --- */
  ESP_LOGI(s_tag, "\n--- Configuration Summary ---");

  ESP_LOGI(s_tag, "UART1:");
  ESP_LOGI(s_tag, "  Port: %d", config1.port);
  ESP_LOGI(s_tag, "  Baud: %d", config1.baud_rate);
  ESP_LOGI(s_tag, "  Format: %dN1", config1.data_bits);

  ESP_LOGI(s_tag, "UART2:");
  ESP_LOGI(s_tag, "  Port: %d", config2.port);
  ESP_LOGI(s_tag, "  Baud: %d", config2.baud_rate);
  ESP_LOGI(s_tag, "  Format: %dN1", config2.data_bits);

  /* --- Statistics --- */
  ESP_LOGI(s_tag, "\n--- Port Statistics ---");

  priv_print_port_stats(&manager, &uart1);
  ESP_LOGI(s_tag, "");
  priv_print_port_stats(&manager, &uart2);

  /* --- Multi-Port Use Cases --- */
  ESP_LOGI(s_tag, "\n--- Multi-Port Use Cases ---");
  ESP_LOGI(s_tag, "1. UART Bridge: Forward data between devices");
  ESP_LOGI(s_tag, "2. Protocol Converter: Convert between protocols");
  ESP_LOGI(s_tag, "3. Data Logger: Log data from multiple sources");
  ESP_LOGI(s_tag, "4. Debugging: Monitor communication on one port");
  ESP_LOGI(s_tag, "5. Gateway: Connect multiple devices to network");
  ESP_LOGI(s_tag, "6. Multi-Sensor: Read from multiple UART sensors");

  ESP_LOGI(s_tag, "\n--- Best Practices ---");
  ESP_LOGI(s_tag, "1. Use appropriate baud rates for each device");
  ESP_LOGI(s_tag, "2. Configure adequate RX/TX buffer sizes");
  ESP_LOGI(s_tag, "3. Monitor buffer usage to prevent overflow");
  ESP_LOGI(s_tag, "4. Use separate tasks for independent ports");
  ESP_LOGI(s_tag, "5. Implement proper error handling per port");
  ESP_LOGI(s_tag, "6. Consider using UART events for efficiency");

  /* Cleanup */
  star_bus_uart_deinit(&manager, uart1.bus_name);
  star_bus_uart_deinit(&manager, uart2.bus_name);
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nMultiple UART ports example complete");
}
