/* esp32-firmware/components/pynq_wifi_bridge/pynq_wifi_uart.c */

#include "pynq_wifi_uart.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#include "esp_log.h"
#include "star_pin_validator.h"

static const char* TAG = "uart_transport";

/* UART configuration */
#define UART_PORT_NUM (UART_NUM_1)
#define UART_BUF_SIZE (1024)
#define UART_TASK_STACK_SIZE (4096)
#define UART_TASK_PRIORITY (10)

/* Receive state machine */
typedef enum {
  k_rx_state_wait_start,
  k_rx_state_wait_cmd,
  k_rx_state_wait_len_lo,
  k_rx_state_wait_len_hi,
  k_rx_state_wait_payload,
} uart_rx_state_t;

/* Module state */
static bool               g_initialized    = false;
static uart_rx_callback_t g_rx_callback    = NULL;
static TaskHandle_t       g_rx_task_handle = NULL;

/* Receive state */
static uart_rx_state_t g_rx_state = k_rx_state_wait_start;
static uint8_t         g_rx_buffer[PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE];
static uint16_t        g_rx_index       = 0;
static uint16_t        g_rx_payload_len = 0;

/**
 * @brief Reset receive state machine
 */
static void uart_rx_reset(void)
{
  g_rx_state       = k_rx_state_wait_start;
  g_rx_index       = 0;
  g_rx_payload_len = 0;
}

/**
 * @brief Process received byte through state machine
 * @param byte Received byte
 */
static void uart_rx_process_byte(uint8_t byte)
{
  switch (g_rx_state) {
    case k_rx_state_wait_start:
      if (byte == PROTOCOL_START_MARKER) {
        g_rx_buffer[0] = byte;
        g_rx_index     = 1;
        g_rx_state     = k_rx_state_wait_cmd;
        ESP_LOGD(TAG, "Start marker received");
      }
      break;

    case k_rx_state_wait_cmd:
      g_rx_buffer[g_rx_index++] = byte;
      g_rx_state                = k_rx_state_wait_len_lo;
      break;

    case k_rx_state_wait_len_lo:
      g_rx_buffer[g_rx_index++] = byte;
      g_rx_payload_len          = byte;
      g_rx_state                = k_rx_state_wait_len_hi;
      break;

    case k_rx_state_wait_len_hi:
      g_rx_buffer[g_rx_index++] = byte;
      g_rx_payload_len |= ((uint16_t)byte << 8);

      if (g_rx_payload_len > PROTOCOL_MAX_PAYLOAD_SIZE) {
        ESP_LOGW(TAG, "Payload too large: %d bytes, resetting", g_rx_payload_len);
        uart_rx_reset();
      } else if (g_rx_payload_len == 0) {
        /* Packet complete (no payload) */
        protocol_packet_t* packet = NULL;
        if (protocol_parse_packet(g_rx_buffer, g_rx_index, &packet)) {
          if (g_rx_callback) {
            g_rx_callback(packet);
          }
        }
        uart_rx_reset();
      } else {
        g_rx_state = k_rx_state_wait_payload;
      }
      break;

    case k_rx_state_wait_payload:
      g_rx_buffer[g_rx_index++] = byte;

      /* Check if packet is complete */
      if (g_rx_index >= PROTOCOL_HEADER_SIZE + g_rx_payload_len) {
        protocol_packet_t* packet = NULL;
        if (protocol_parse_packet(g_rx_buffer, g_rx_index, &packet)) {
          if (g_rx_callback) {
            g_rx_callback(packet);
          }
        }
        uart_rx_reset();
      }
      break;
  }
}

/**
 * @brief UART receive task
 * @param arg Task argument (unused)
 */
static void uart_rx_task(void* arg)
{
  uint8_t data[UART_BUF_SIZE];

  ESP_LOGI(TAG, "UART receive task started");

  while (1) {
    int len = uart_read_bytes(UART_PORT_NUM, data, UART_BUF_SIZE, pdMS_TO_TICKS(100));

    if (len > 0) {
      ESP_LOGD(TAG, "Received %d bytes", len);
      for (int i = 0; i < len; i++) {
        uart_rx_process_byte(data[i]);
      }
    }
  }
}

bool uart_transport_init(uart_rx_callback_t rx_callback)
{
  if (g_initialized) {
    ESP_LOGW(TAG, "Already initialized");
    return true;
  }

  ESP_LOGI(TAG, "Initializing UART transport");

  /* Configure UART parameters */
  uart_config_t uart_config = {
    .baud_rate  = CONFIG_PYNQ_UART_BAUD_RATE,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };

  esp_err_t err =
    uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 0, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install UART driver: %d", err);
    return false;
  }

  err = uart_param_config(UART_PORT_NUM, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure UART: %d", err);
    uart_driver_delete(UART_PORT_NUM);
    return false;
  }

  err = uart_set_pin(UART_PORT_NUM,
                     CONFIG_PYNQ_UART_TX_GPIO,
                     CONFIG_PYNQ_UART_RX_GPIO,
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART pins: %d", err);
    uart_driver_delete(UART_PORT_NUM);
    return false;
  }

  ESP_LOGI(TAG,
           "UART configured: TX=%d, RX=%d, baud=%d",
           CONFIG_PYNQ_UART_TX_GPIO,
           CONFIG_PYNQ_UART_RX_GPIO,
           CONFIG_PYNQ_UART_BAUD_RATE);

  /* Register pins with validator */
  err = star_register_pin(CONFIG_PYNQ_UART_TX_GPIO, "UART1 TX (PYNQ communication)", false);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register TX pin: %d", err);
    uart_driver_delete(UART_PORT_NUM);
    return false;
  }

  err = star_register_pin(CONFIG_PYNQ_UART_RX_GPIO, "UART1 RX (PYNQ communication)", false);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register RX pin: %d", err);
    uart_driver_delete(UART_PORT_NUM);
    return false;
  }

  /* Store callback */
  g_rx_callback = rx_callback;

  /* Reset receive state */
  uart_rx_reset();

  /* Create receive task */
  BaseType_t ret = xTaskCreate(uart_rx_task,
                               "uart_rx",
                               UART_TASK_STACK_SIZE,
                               NULL,
                               UART_TASK_PRIORITY,
                               &g_rx_task_handle);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create UART receive task");
    uart_driver_delete(UART_PORT_NUM);
    return false;
  }

  g_initialized = true;
  ESP_LOGI(TAG, "UART transport initialized successfully");

  return true;
}

int transport_send(const uint8_t* data, uint16_t len)
{
  if (!g_initialized) {
    ESP_LOGE(TAG, "Transport not initialized");
    return -1;
  }

  if (!data || len == 0) {
    ESP_LOGE(TAG, "Invalid parameters");
    return -1;
  }

  int written = uart_write_bytes(UART_PORT_NUM, data, len);

  if (written < 0) {
    ESP_LOGE(TAG, "UART write failed");
    return -1;
  }

  if (written != len) {
    ESP_LOGW(TAG, "Partial write: %d/%d bytes", written, len);
  }

  ESP_LOGD(TAG, "Sent %d bytes", written);

  return written;
}

void uart_transport_deinit(void)
{
  if (!g_initialized) {
    return;
  }

  ESP_LOGI(TAG, "Deinitializing UART transport");

  /* Delete receive task */
  if (g_rx_task_handle) {
    vTaskDelete(g_rx_task_handle);
    g_rx_task_handle = NULL;
  }

  /* Uninstall UART driver */
  uart_driver_delete(UART_PORT_NUM);

  g_initialized = false;
  g_rx_callback = NULL;
  uart_rx_reset();

  ESP_LOGI(TAG, "UART transport deinitialized");
}
