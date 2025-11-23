/**
 * @file 010_uart_advanced.c
 * @brief Advanced UART features example
 *
 * Demonstrates:
 * - Flow control (RTS/CTS)
 * - RS-485 mode
 * - Pattern detection
 * - Event callbacks
 * - Different UART modes
 */

#include "star_bus_manager.h"
#include "star_bus_uart.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *s_tag = "UART_ADVANCED_EXAMPLE";

#define UART_TX_PIN  (GPIO_NUM_17)
#define UART_RX_PIN  (GPIO_NUM_16)
#define UART_RTS_PIN (GPIO_NUM_18)
#define UART_CTS_PIN (GPIO_NUM_19)
#define UART_NUM     (UART_NUM_1)

static star_bus_manager_t s_manager;

/* Event callback for UART events */
static void priv_uart_event_callback(const char*       bus_name,
                                     uart_event_type_t event_type,
                                     size_t            size,
                                     void*             context)
{
  const char *name = (const char *)context;

  switch (event_type) {
    case UART_DATA:
      ESP_LOGI(s_tag, "[%s] Data received (%d bytes)", name, (int)size);
      break;
    case UART_BREAK:
      ESP_LOGW(s_tag, "[%s] Break detected", name);
      break;
    case UART_BUFFER_FULL:
      ESP_LOGW(s_tag, "[%s] Buffer full", name);
      break;
    case UART_FIFO_OVF:
      ESP_LOGE(s_tag, "[%s] FIFO overflow", name);
      break;
    case UART_FRAME_ERR:
      ESP_LOGE(s_tag, "[%s] Frame error", name);
      break;
    case UART_PARITY_ERR:
      ESP_LOGE(s_tag, "[%s] Parity error", name);
      break;
    case UART_PATTERN_DET:
      ESP_LOGI(s_tag, "[%s] Pattern detected", name);
      break;
    default:
      ESP_LOGI(s_tag, "[%s] Unknown event: %d", name, event_type);
      break;
  }
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Advanced UART Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "uart_adv_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* --- Hardware Flow Control --- */
  ESP_LOGI(s_tag, "--- Hardware Flow Control ---");

  star_uart_config_t flow_config = STAR_UART_CONFIG_DEFAULT();
  flow_config.port      = UART_NUM;
  flow_config.tx_pin    = UART_TX_PIN;
  flow_config.rx_pin    = UART_RX_PIN;
  flow_config.rts_pin   = UART_RTS_PIN;
  flow_config.cts_pin   = UART_CTS_PIN;
  flow_config.baud_rate = STAR_UART_BAUD_115200;
  flow_config.flow_ctrl = STAR_UART_FLOW_CTRL_RTS_CTS;

  ESP_LOGI(s_tag, "Flow control: CTS/RTS");
  ESP_LOGI(s_tag, "RTS pin: %d", flow_config.rts_pin);
  ESP_LOGI(s_tag, "CTS pin: %d", flow_config.cts_pin);

  ret = star_bus_uart_init(&s_manager, "uart_flow", &flow_config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "UART with flow control initialized");

    /* Write data - will respect CTS signal */
    const char *msg = "Flow controlled message\r\n";
    star_bus_uart_write_string(&s_manager, "uart_flow", msg);
    star_bus_uart_flush(&s_manager, "uart_flow");

    star_bus_uart_deinit(&s_manager, "uart_flow");
  } else {
    ESP_LOGW(s_tag, "Flow control init failed: %s", esp_err_to_name(ret));
  }

  /* --- RS-485 Mode --- */
  ESP_LOGI(s_tag, "\n--- RS-485 Mode ---");

  star_uart_config_t rs485_config = STAR_UART_CONFIG_DEFAULT();
  rs485_config.port      = UART_NUM;
  rs485_config.tx_pin    = UART_TX_PIN;
  rs485_config.rx_pin    = UART_RX_PIN;
  rs485_config.rts_pin   = UART_RTS_PIN;  /* Used as DE (Driver Enable) */
  rs485_config.baud_rate = STAR_UART_BAUD_9600;
  rs485_config.mode      = STAR_UART_MODE_RS485;

  ESP_LOGI(s_tag, "Mode: RS-485 half-duplex");
  ESP_LOGI(s_tag, "DE pin: %d (active high during TX)", rs485_config.rts_pin);

  ret = star_bus_uart_init(&s_manager, "uart_rs485", &rs485_config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "RS-485 UART initialized");

    /* Send Modbus-like frame */
    uint8_t modbus_frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD};
    ret = star_bus_uart_write(&s_manager, "uart_rs485", modbus_frame, sizeof(modbus_frame));
    ESP_LOGI(s_tag, "Sent %d byte Modbus frame: %s", (int)sizeof(modbus_frame), esp_err_to_name(ret));

    star_bus_uart_deinit(&s_manager, "uart_rs485");
  } else {
    ESP_LOGW(s_tag, "RS-485 init failed: %s", esp_err_to_name(ret));
  }

  /* --- Pattern Detection --- */
  ESP_LOGI(s_tag, "\n--- Pattern Detection ---");

  star_uart_config_t pattern_config = STAR_UART_CONFIG_DEFAULT();
  pattern_config.port      = UART_NUM;
  pattern_config.tx_pin    = UART_TX_PIN;
  pattern_config.rx_pin    = UART_RX_PIN;
  pattern_config.baud_rate = STAR_UART_BAUD_115200;

  ret = star_bus_uart_init(&s_manager, "uart_pattern", &pattern_config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "UART initialized for pattern detection");

    /* Enable pattern detection for newline character */
    ret = star_bus_uart_enable_pattern(&s_manager, "uart_pattern", '\n', 3, 10, 10);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Pattern detection enabled for '\\n' (3 occurrences)");
    }

    /* Disable pattern detection */
    ret = star_bus_uart_disable_pattern(&s_manager, "uart_pattern");
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Pattern detection disabled");
    }

    /* Re-enable for different pattern */
    ret = star_bus_uart_enable_pattern(&s_manager, "uart_pattern", '+', 3, 10, 10);
    if (ret == ESP_OK) {
      ESP_LOGI(s_tag, "Pattern detection enabled for '+++' (escape sequence)");
    }

    star_bus_uart_deinit(&s_manager, "uart_pattern");
  }

  /* --- Event Callbacks --- */
  ESP_LOGI(s_tag, "\n--- Event Callbacks ---");

  star_uart_config_t event_config = STAR_UART_CONFIG_DEFAULT();
  event_config.port            = UART_NUM;
  event_config.tx_pin          = UART_TX_PIN;
  event_config.rx_pin          = UART_RX_PIN;
  event_config.baud_rate       = STAR_UART_BAUD_115200;
  event_config.event_callback  = priv_uart_event_callback;
  event_config.event_context   = (void*)"EventUART";

  ret = star_bus_uart_init(&s_manager, "uart_event", &event_config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "UART with event callback initialized");

    /* Send some data to trigger events */
    star_bus_uart_write_string(&s_manager, "uart_event", "Event test\r\n");

    /* Wait a bit for any events */
    vTaskDelay(pdMS_TO_TICKS(100));

    star_bus_uart_deinit(&s_manager, "uart_event");
  }

  /* --- IrDA Mode --- */
  ESP_LOGI(s_tag, "\n--- IrDA Mode ---");

  star_uart_config_t irda_config = STAR_UART_CONFIG_DEFAULT();
  irda_config.port      = UART_NUM;
  irda_config.tx_pin    = UART_TX_PIN;
  irda_config.rx_pin    = UART_RX_PIN;
  irda_config.baud_rate = STAR_UART_BAUD_9600;  /* IrDA typically uses lower speeds */
  irda_config.mode      = STAR_UART_MODE_IRDA;

  ESP_LOGI(s_tag, "Mode: IrDA");
  ESP_LOGI(s_tag, "Note: IrDA uses inverted, pulsed signaling");

  ret = star_bus_uart_init(&s_manager, "uart_irda", &irda_config);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "IrDA UART initialized");
    star_bus_uart_deinit(&s_manager, "uart_irda");
  } else {
    ESP_LOGW(s_tag, "IrDA init failed: %s", esp_err_to_name(ret));
  }

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  /* --- Baud Rate Options --- */
  ESP_LOGI(s_tag, "\n--- Available Baud Rates ---");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_300     = 300");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_1200    = 1200");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_2400    = 2400");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_4800    = 4800");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_9600    = 9600");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_19200   = 19200");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_38400   = 38400");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_57600   = 57600");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_115200  = 115200");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_230400  = 230400");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_460800  = 460800");
  ESP_LOGI(s_tag, "STAR_UART_BAUD_921600  = 921600");

  /* --- Configuration Summary --- */
  ESP_LOGI(s_tag, "\n--- Configuration Summary ---");
  ESP_LOGI(s_tag, "Data bits: 5, 6, 7, 8");
  ESP_LOGI(s_tag, "Stop bits: 1, 1.5, 2");
  ESP_LOGI(s_tag, "Parity: None, Even, Odd");
  ESP_LOGI(s_tag, "Flow control: None, RTS, CTS, RTS+CTS");
  ESP_LOGI(s_tag, "Modes: Standard, RS-485, IrDA");

  ESP_LOGI(s_tag, "\nAdvanced UART example complete");
}
