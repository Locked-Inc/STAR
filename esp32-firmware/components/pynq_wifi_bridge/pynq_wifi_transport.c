/* esp32-firmware/components/pynq_wifi_bridge/pynq_wifi_transport.c */

#include "pynq_wifi_transport.h"

#include "driver/spi_slave.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#include "esp_log.h"
#include "star_bus_config.h"
#include "star_bus_spi_peripheral.h"
#include "star_bus_uart.h"
#include "star_error_handler.h"
#include "star_pin_validator.h"

static const char* TAG = "transport";

/* Task configuration */
#define RX_TASK_STACK_SIZE (4096)
#define RX_TASK_PRIORITY (10)

/* Receive state machine */
typedef enum {
  k_rx_state_wait_start,
  k_rx_state_wait_cmd,
  k_rx_state_wait_len_lo,
  k_rx_state_wait_len_hi,
  k_rx_state_wait_payload,
} uart_rx_state_t;

/* Module state */
static bool                    g_initialized    = false;
static transport_type_t        g_type           = k_transport_uart;
static star_bus_manager_t*     g_bus_manager    = NULL;
static char                    g_bus_name[32]   = {0};
static transport_rx_callback_t g_rx_callback    = NULL;
static TaskHandle_t            g_rx_task_handle = NULL;
static error_handler_t         g_error_handler; /**< Error handler for transport operations */

/* UART receive state */
static uart_rx_state_t g_uart_rx_state = k_rx_state_wait_start;
static uint8_t         g_uart_rx_buffer[PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE];
static uint16_t        g_uart_rx_index       = 0;
static uint16_t        g_uart_rx_payload_len = 0;

/* SPI receive buffer */
static uint8_t* g_spi_rx_buffer   = NULL;
static uint8_t* g_spi_tx_buffer   = NULL;
static size_t   g_spi_buffer_size = 0;

/**
 * @brief Reset UART receive state machine
 */
static void uart_rx_reset(void)
{
  g_uart_rx_state       = k_rx_state_wait_start;
  g_uart_rx_index       = 0;
  g_uart_rx_payload_len = 0;
}

/**
 * @brief Process received byte through UART state machine
 * @param byte Received byte
 */
static void uart_rx_process_byte(uint8_t byte)
{
  switch (g_uart_rx_state) {
    case k_rx_state_wait_start: {
      if (byte == PROTOCOL_START_MARKER) {
        g_uart_rx_buffer[0] = byte;
        g_uart_rx_index     = 1;
        g_uart_rx_state     = k_rx_state_wait_cmd;
        ESP_LOGD(TAG, "Start marker received");
      }
      break;
    }

    case k_rx_state_wait_cmd: {
      g_uart_rx_buffer[g_uart_rx_index++] = byte;
      g_uart_rx_state                     = k_rx_state_wait_len_lo;
      break;
    }

    case k_rx_state_wait_len_lo: {
      g_uart_rx_buffer[g_uart_rx_index++] = byte;
      g_uart_rx_payload_len               = byte;
      g_uart_rx_state                     = k_rx_state_wait_len_hi;
      break;
    }

    case k_rx_state_wait_len_hi: {
      g_uart_rx_buffer[g_uart_rx_index++] = byte;
      g_uart_rx_payload_len |= ((uint16_t)byte << 8);

      if (g_uart_rx_payload_len > PROTOCOL_MAX_PAYLOAD_SIZE) {
        ESP_LOGW(TAG, "Payload too large: %d bytes, resetting", g_uart_rx_payload_len);
        uart_rx_reset();
      } else if (g_uart_rx_payload_len == 0) {
        /* Packet complete (no payload) */
        protocol_packet_t* packet = NULL;
        if (protocol_parse_packet(g_uart_rx_buffer, g_uart_rx_index, &packet)) {
          if (g_rx_callback) {
            g_rx_callback(packet);
          }
        }
        uart_rx_reset();
      } else {
        g_uart_rx_state = k_rx_state_wait_payload;
      }
      break;
    }

    case k_rx_state_wait_payload: {
      g_uart_rx_buffer[g_uart_rx_index++] = byte;

      /* Check if packet is complete */
      if (g_uart_rx_index >= PROTOCOL_HEADER_SIZE + g_uart_rx_payload_len) {
        protocol_packet_t* packet = NULL;
        if (protocol_parse_packet(g_uart_rx_buffer, g_uart_rx_index, &packet)) {
          if (g_rx_callback) {
            g_rx_callback(packet);
          }
        }
        uart_rx_reset();
      }
      break;
    }
  }
}

/**
 * @brief UART receive task
 * @param arg Task argument (unused)
 */
static void uart_rx_task(void* arg)
{
  uint8_t data[256];

  ESP_LOGI(TAG, "UART receive task started");

  while (1) {
    size_t    received = 0;
    esp_err_t ret =
      star_bus_uart_read(g_bus_manager, g_bus_name, data, sizeof(data), &received, 100);

    if (ret == ESP_OK && received > 0) {
      ESP_LOGD(TAG, "Received %d bytes", received);
      for (size_t i = 0; i < received; i++) {
        uart_rx_process_byte(data[i]);
      }
    }
  }
}

/**
 * @brief SPI receive task
 * @param arg Task argument (unused)
 */
static void spi_rx_task(void* arg)
{
  ESP_LOGI(TAG, "SPI receive task started");

  while (1) {
    /* Clear buffers */
    memset(g_spi_rx_buffer, 0, g_spi_buffer_size);
    memset(g_spi_tx_buffer, 0, g_spi_buffer_size);

    /* Wait for transaction from controller */
    esp_err_t ret = star_bus_spi_peripheral_transceive(g_bus_manager,
                                                       g_bus_name,
                                                       g_spi_tx_buffer,
                                                       g_spi_rx_buffer,
                                                       g_spi_buffer_size,
                                                       portMAX_DELAY);

    if (ret == ESP_OK) {
      /* Check if received data contains a valid packet */
      protocol_packet_t* packet = NULL;
      if (protocol_parse_packet(g_spi_rx_buffer, g_spi_buffer_size, &packet)) {
        ESP_LOGD(TAG, "Received valid packet via SPI");
        if (g_rx_callback) {
          g_rx_callback(packet);
        }
      }
    } else {
      ESP_LOGW(TAG, "SPI transceive failed: %s", esp_err_to_name(ret));
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

bool transport_init(const transport_config_t* config, transport_rx_callback_t rx_callback)
{
  if (g_initialized) {
    ESP_LOGW(TAG, "Already initialized");
    return true;
  }

  if (!config || !config->bus_manager) {
    ESP_LOGE(TAG, "Invalid configuration");
    return false;
  }

  ESP_LOGI(TAG, "Initializing transport (type: %d)", config->type);

  g_type        = config->type;
  g_bus_manager = config->bus_manager;
  g_rx_callback = rx_callback;

  /* Initialize error handler for transport operations */
  esp_err_t ret = error_handler_init(&g_error_handler,
                                     5,     /* max_retries - higher for network operations */
                                     100,   /* base_retry_delay (ms) */
                                     1000,  /* max_retry_delay (ms) - longer for transport */
                                     NULL,  /* no reset function */
                                     NULL); /* no reset context */
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return false;
  }

  if (g_type == k_transport_uart) {
    /* Initialize UART transport */
    const uart_transport_config_t* uart_cfg = &config->config.uart;

    strncpy(g_bus_name, uart_cfg->bus_name, sizeof(g_bus_name) - 1);
    g_bus_name[sizeof(g_bus_name) - 1] = '\0';

    /* Configure UART */
    star_uart_config_t star_uart_cfg = STAR_UART_CONFIG_DEFAULT();
    star_uart_cfg.baud_rate          = uart_cfg->baud_rate;
    star_uart_cfg.tx_pin             = uart_cfg->tx_pin;
    star_uart_cfg.rx_pin             = uart_cfg->rx_pin;
    star_uart_cfg.rx_buffer_size     = uart_cfg->rx_buf_size;
    star_uart_cfg.tx_buffer_size     = uart_cfg->tx_buf_size;

    ret = star_bus_uart_init(g_bus_manager, g_bus_name, &star_uart_cfg);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize UART: %s", esp_err_to_name(ret));
      return false;
    }

    /* Register pins with validator */
    ret = star_register_pin(uart_cfg->tx_pin, "PYNQ UART TX", false);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register TX pin: %s", esp_err_to_name(ret));
      star_bus_uart_deinit(g_bus_manager, g_bus_name);
      return false;
    }

    ret = star_register_pin(uart_cfg->rx_pin, "PYNQ UART RX", false);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register RX pin: %s", esp_err_to_name(ret));
      star_bus_uart_deinit(g_bus_manager, g_bus_name);
      return false;
    }

    /* Reset receive state */
    uart_rx_reset();

    /* Create receive task */
    BaseType_t task_ret = xTaskCreate(uart_rx_task,
                                      "uart_rx",
                                      RX_TASK_STACK_SIZE,
                                      NULL,
                                      RX_TASK_PRIORITY,
                                      &g_rx_task_handle);
    if (task_ret != pdPASS) {
      ESP_LOGE(TAG, "Failed to create UART receive task");
      star_bus_uart_deinit(g_bus_manager, g_bus_name);
      return false;
    }

    ESP_LOGI(TAG,
             "UART transport initialized: TX=%d, RX=%d, baud=%lu",
             uart_cfg->tx_pin,
             uart_cfg->rx_pin,
             (unsigned long)uart_cfg->baud_rate);

  } else if (g_type == k_transport_spi) {
    /* Initialize SPI peripheral transport */
    const spi_transport_config_t* spi_cfg = &config->config.spi;

    strncpy(g_bus_name, spi_cfg->bus_name, sizeof(g_bus_name) - 1);
    g_bus_name[sizeof(g_bus_name) - 1] = '\0';

    /* Allocate SPI buffers */
    g_spi_buffer_size = spi_cfg->transaction_len;
    g_spi_rx_buffer   = (uint8_t*)malloc(g_spi_buffer_size);
    g_spi_tx_buffer   = (uint8_t*)malloc(g_spi_buffer_size);

    if (!g_spi_rx_buffer || !g_spi_tx_buffer) {
      ESP_LOGE(TAG, "Failed to allocate SPI buffers");
      free(g_spi_rx_buffer);
      free(g_spi_tx_buffer);
      return false;
    }

    memset(g_spi_rx_buffer, 0, g_spi_buffer_size);
    memset(g_spi_tx_buffer, 0, g_spi_buffer_size);

    /* Configure SPI peripheral bus */
    star_bus_config_t* bus_config = star_bus_config_create_spi_peripheral(spi_cfg->bus_name,
                                                                          spi_cfg->spi_host,
                                                                          spi_cfg->copi_pin,
                                                                          spi_cfg->cipo_pin,
                                                                          spi_cfg->sclk_pin,
                                                                          spi_cfg->cs_pin,
                                                                          spi_cfg->queue_size,
                                                                          spi_cfg->mode);
    if (!bus_config) {
      ESP_LOGE(TAG, "Failed to create SPI peripheral config");
      free(g_spi_rx_buffer);
      free(g_spi_tx_buffer);
      return false;
    }

    ret = star_bus_manager_add_bus(g_bus_manager, bus_config);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to add SPI peripheral bus: %s", esp_err_to_name(ret));
      star_bus_config_destroy(bus_config);
      free(g_spi_rx_buffer);
      free(g_spi_tx_buffer);
      return false;
    }

    /* Register pins with validator */
    ret = star_register_pin(spi_cfg->copi_pin, "PYNQ SPI COPI", false);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register COPI pin: %s", esp_err_to_name(ret));
      star_bus_manager_remove_bus(g_bus_manager, g_bus_name);
      free(g_spi_rx_buffer);
      free(g_spi_tx_buffer);
      return false;
    }

    ret = star_register_pin(spi_cfg->cipo_pin, "PYNQ SPI CIPO", false);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register CIPO pin: %s", esp_err_to_name(ret));
      star_bus_manager_remove_bus(g_bus_manager, g_bus_name);
      free(g_spi_rx_buffer);
      free(g_spi_tx_buffer);
      return false;
    }

    ret = star_register_pin(spi_cfg->sclk_pin, "PYNQ SPI SCLK", false);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register SCLK pin: %s", esp_err_to_name(ret));
      star_bus_manager_remove_bus(g_bus_manager, g_bus_name);
      free(g_spi_rx_buffer);
      free(g_spi_tx_buffer);
      return false;
    }

    ret = star_register_pin(spi_cfg->cs_pin, "PYNQ SPI CS", false);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register CS pin: %s", esp_err_to_name(ret));
      star_bus_manager_remove_bus(g_bus_manager, g_bus_name);
      free(g_spi_rx_buffer);
      free(g_spi_tx_buffer);
      return false;
    }

    /* Create receive task */
    BaseType_t task_ret = xTaskCreate(spi_rx_task,
                                      "spi_rx",
                                      RX_TASK_STACK_SIZE,
                                      NULL,
                                      RX_TASK_PRIORITY,
                                      &g_rx_task_handle);
    if (task_ret != pdPASS) {
      ESP_LOGE(TAG, "Failed to create SPI receive task");
      star_bus_manager_remove_bus(g_bus_manager, g_bus_name);
      free(g_spi_rx_buffer);
      free(g_spi_tx_buffer);
      return false;
    }

    ESP_LOGI(TAG,
             "SPI peripheral transport initialized: COPI=%d, CIPO=%d, SCLK=%d, CS=%d",
             spi_cfg->copi_pin,
             spi_cfg->cipo_pin,
             spi_cfg->sclk_pin,
             spi_cfg->cs_pin);

  } else {
    ESP_LOGE(TAG, "Unknown transport type: %d", g_type);
    return false;
  }

  g_initialized = true;
  ESP_LOGI(TAG, "Transport initialized successfully");

  return true;
}

int32_t transport_send(const uint8_t* data, uint16_t len)
{
  if (!g_initialized) {
    ESP_LOGE(TAG, "Transport not initialized");
    return -1;
  }

  if (!data || len == 0) {
    ESP_LOGE(TAG, "Invalid parameters");
    return -1;
  }

  esp_err_t ret;

  if (g_type == k_transport_uart) {
    /* Retry UART writes with error handler */
    do {
      ret = star_bus_uart_write(g_bus_manager, g_bus_name, data, len);

      if (ret == ESP_OK) {
        /* Success - reset error state */
        error_handler_reset_state(&g_error_handler);
        ESP_LOGD(TAG, "Sent %d bytes via UART", len);
        return (int32_t)len;
      }

      /* Determine if error is transient */
      bool is_transient = false;
      switch (ret) {
        case ESP_ERR_TIMEOUT:
        case ESP_FAIL:
        case ESP_ERR_INVALID_STATE:
          is_transient = true;
          break;
        default:
          is_transient = false;
          break;
      }

      if (!is_transient) {
        ESP_LOGE(TAG, "UART write permanent error: %s", esp_err_to_name(ret));
        return -1;
      }

      /* Record error and check if we can retry */
      RECORD_ERROR(&g_error_handler, ret, "UART write");

      if (error_handler_can_retry(&g_error_handler)) {
        ESP_LOGW(TAG,
                 "UART write failed: %s - retry %lu/%lu after %lu ms",
                 esp_err_to_name(ret),
                 (unsigned long)g_error_handler.current_retry,
                 (unsigned long)g_error_handler.max_retries,
                 (unsigned long)g_error_handler.current_retry_delay);
        vTaskDelay(pdMS_TO_TICKS(g_error_handler.current_retry_delay));
      }
    } while (error_handler_can_retry(&g_error_handler));

    ESP_LOGE(TAG,
             "UART write failed after %lu retries",
             (unsigned long)g_error_handler.max_retries);
    return -1;

  } else if (g_type == k_transport_spi) {
    /* For SPI peripheral, prepare response in TX buffer for next transaction */
    if (len > g_spi_buffer_size) {
      ESP_LOGE(TAG, "Data too large for SPI buffer: %d > %d", len, g_spi_buffer_size);
      return -1;
    }

    memcpy(g_spi_tx_buffer, data, len);
    ESP_LOGD(TAG, "Prepared %d bytes for SPI transmission", len);
    return (int32_t)len;
  }

  return -1;
}

void transport_deinit(void)
{
  if (!g_initialized) {
    return;
  }

  ESP_LOGI(TAG, "Deinitializing transport");

  /* Delete receive task */
  if (g_rx_task_handle) {
    vTaskDelete(g_rx_task_handle);
    g_rx_task_handle = NULL;
  }

  if (g_type == k_transport_uart) {
    star_bus_uart_deinit(g_bus_manager, g_bus_name);
    uart_rx_reset();

  } else if (g_type == k_transport_spi) {
    star_bus_manager_remove_bus(g_bus_manager, g_bus_name);
    free(g_spi_rx_buffer);
    free(g_spi_tx_buffer);
    g_spi_rx_buffer   = NULL;
    g_spi_tx_buffer   = NULL;
    g_spi_buffer_size = 0;
  }

  /* Deinitialize error handler */
  error_handler_deinit(&g_error_handler);

  g_initialized = false;
  g_rx_callback = NULL;
  g_bus_manager = NULL;
  memset(g_bus_name, 0, sizeof(g_bus_name));

  ESP_LOGI(TAG, "Transport deinitialized");
}

transport_type_t transport_get_type(void)
{
  return g_type;
}
