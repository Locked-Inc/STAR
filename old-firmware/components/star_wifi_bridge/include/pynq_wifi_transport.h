/* esp32-firmware/components/pynq_wifi_bridge/include/pynq_wifi_transport.h */

#ifndef PYNQ_WIFI_TRANSPORT_H
#define PYNQ_WIFI_TRANSPORT_H

#include "driver/gpio.h"
#include "driver/spi_common.h"

#include <stdbool.h>
#include <stdint.h>

#include "pynq_wifi_protocol.h"
#include "star_bus_manager.h"
#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Transport type selection
 */
typedef enum {
  k_transport_uart, /**< UART transport using star_bus_uart */
  k_transport_spi,  /**< SPI peripheral transport using star_bus_spi_peripheral */
} transport_type_t;

/**
 * @brief Callback function type for received packets
 * @param packet Pointer to parsed packet
 */
typedef void (*transport_rx_callback_t)(protocol_packet_t* packet);

/**
 * @brief UART transport configuration
 */
typedef struct {
  const char* bus_name;    /**< Star bus name for UART */
  uint32_t    baud_rate;   /**< Baud rate */
  gpio_num_t  tx_pin;      /**< TX GPIO pin */
  gpio_num_t  rx_pin;      /**< RX GPIO pin */
  size_t      rx_buf_size; /**< RX buffer size */
  size_t      tx_buf_size; /**< TX buffer size */
} uart_transport_config_t;

/**
 * @brief SPI peripheral transport configuration
 */
typedef struct {
  const char*       bus_name;        /**< Star bus name for SPI peripheral */
  spi_host_device_t spi_host;        /**< SPI host (SPI2_HOST or SPI3_HOST) */
  gpio_num_t        copi_pin;        /**< COPI GPIO pin */
  gpio_num_t        cipo_pin;        /**< CIPO GPIO pin */
  gpio_num_t        sclk_pin;        /**< SCLK GPIO pin */
  gpio_num_t        cs_pin;          /**< CS GPIO pin */
  size_t            transaction_len; /**< Maximum transaction length in bytes */
  uint8_t           queue_size;      /**< SPI transaction queue size (typically 1-3) */
  uint8_t           mode;            /**< SPI mode (0-3) */
} spi_transport_config_t;

/**
 * @brief Transport configuration
 */
typedef struct {
  transport_type_t    type;        /**< Transport type */
  star_bus_manager_t* bus_manager; /**< Pointer to star_bus manager */
  union {
    uart_transport_config_t uart; /**< UART configuration */
    spi_transport_config_t  spi;  /**< SPI configuration */
  } config;
  error_handler_t error_handler; /**< Error handler for transport retry logic */
} transport_config_t;

/**
 * @brief Initialize transport layer
 * @param config Transport configuration
 * @param rx_callback Callback function for received packets (can be NULL)
 * @return true on success, false on failure
 */
bool transport_init(const transport_config_t* config, transport_rx_callback_t rx_callback);

/**
 * @brief Send data over transport
 * @param data Pointer to data buffer
 * @param len Length of data to send
 * @return Number of bytes sent, or -1 on error
 */
int32_t transport_send(const uint8_t* data, uint16_t len);

/**
 * @brief Deinitialize transport layer
 */
void transport_deinit(void);

/**
 * @brief Get current transport type
 * @return Current transport type
 */
transport_type_t transport_get_type(void);

/* --- Helper Macros --- */

/**
 * @brief Create default UART transport configuration
 */
#define UART_TRANSPORT_CONFIG_DEFAULT()                                                            \
  {                                                                                                \
    .type = k_transport_uart, .bus_manager = NULL,                                                 \
    .config =                                                                                      \
    {.uart = {                                                                                     \
       .bus_name    = "pynq_uart",                                                                 \
       .baud_rate   = 115200,                                                                      \
       .tx_pin      = GPIO_NUM_NC,                                                                 \
       .rx_pin      = GPIO_NUM_NC,                                                                 \
       .rx_buf_size = 2048,                                                                        \
       .tx_buf_size = 2048,                                                                        \
     } }                                                                                           \
  }

/**
 * @brief Create default SPI transport configuration
 */
#define SPI_TRANSPORT_CONFIG_DEFAULT()                                                             \
  {                                                                                                \
    .type = k_transport_spi, .bus_manager = NULL,                                                  \
    .config =                                                                                      \
    {.spi = {                                                                                      \
       .bus_name        = "pynq_spi",                                                              \
       .spi_host        = SPI2_HOST,                                                               \
       .copi_pin        = GPIO_NUM_NC,                                                             \
       .cipo_pin        = GPIO_NUM_NC,                                                             \
       .sclk_pin        = GPIO_NUM_NC,                                                             \
       .cs_pin          = GPIO_NUM_NC,                                                             \
       .transaction_len = 2048,                                                                    \
       .queue_size      = 3,                                                                       \
       .mode            = 0,                                                                       \
     } }                                                                                           \
  }

#ifdef __cplusplus
}
#endif

#endif /* PYNQ_WIFI_TRANSPORT_H */
