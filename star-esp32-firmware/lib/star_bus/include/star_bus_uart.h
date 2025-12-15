/* lib/star_bus/include/star_bus_uart.h */

#ifndef STAR_BUS_UART_H
#define STAR_BUS_UART_H

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "star_bus_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_bus_uart.h
 * @brief UART/Serial bus communication implementation
 *
 * This module provides UART communication capabilities with support for
 * various protocols, framing options, and hardware flow control.
 *
 * Features:
 * - Multiple UART ports (UART0, UART1, UART2)
 * - Configurable baud rates (300 - 5000000 bps)
 * - Hardware and software flow control
 * - DMA support for high-speed transfers
 * - Configurable data bits, stop bits, parity
 * - RS-232, RS-485, RS-422 support
 * - Pattern detection for framing
 * - Event callbacks (RX, TX, errors)
 * - Circular RX buffer for continuous reception
 *
 * Common Use Cases:
 * - Serial console/debug output
 * - GPS module communication (NMEA)
 * - Modbus RTU/ASCII
 * - AT command interfaces (GSM/WiFi modules)
 * - Industrial sensors (RS-485)
 * - Serial protocols (UART-based)
 *
 * Example:
 * @code
 * // Initialize UART
 * star_uart_config_t config = STAR_UART_CONFIG_DEFAULT();
 * config.baud_rate = 115200;
 * config.tx_pin = GPIO_NUM_17;
 * config.rx_pin = GPIO_NUM_16;
 * star_bus_uart_init(manager, "uart1", &config);
 *
 * // Send data
 * const char* msg = "Hello\r\n";
 * star_bus_uart_write(manager, "uart1", (uint8_t*)msg, strlen(msg));
 *
 * // Receive data
 * uint8_t buffer[128];
 * size_t received;
 * star_bus_uart_read(manager, "uart1", buffer, sizeof(buffer), &received, 1000);
 * @endcode
 */

/* --- Types --- */

/**
 * @brief UART data bits
 */
typedef enum {
  k_star_uart_data_5_bits = UART_DATA_5_BITS, /**< 5 data bits */
  k_star_uart_data_6_bits = UART_DATA_6_BITS, /**< 6 data bits */
  k_star_uart_data_7_bits = UART_DATA_7_BITS, /**< 7 data bits */
  k_star_uart_data_8_bits = UART_DATA_8_BITS, /**< 8 data bits (default) */
} star_uart_data_bits_t;

/**
 * @brief UART stop bits
 */
typedef enum {
  k_star_uart_stop_1_bit    = UART_STOP_BITS_1,   /**< 1 stop bit (default) */
  k_star_uart_stop_1_5_bits = UART_STOP_BITS_1_5, /**< 1.5 stop bits */
  k_star_uart_stop_2_bits   = UART_STOP_BITS_2,   /**< 2 stop bits */
} star_uart_stop_bits_t;

/**
 * @brief UART parity
 */
typedef enum {
  k_star_uart_parity_none = UART_PARITY_DISABLE, /**< No parity (default) */
  k_star_uart_parity_even = UART_PARITY_EVEN,    /**< Even parity */
  k_star_uart_parity_odd  = UART_PARITY_ODD,     /**< Odd parity */
} star_uart_parity_t;

/**
 * @brief UART flow control
 */
typedef enum {
  k_star_uart_flow_ctrl_none    = UART_HW_FLOWCTRL_DISABLE, /**< No flow control */
  k_star_uart_flow_ctrl_rts     = UART_HW_FLOWCTRL_RTS,     /**< RTS only */
  k_star_uart_flow_ctrl_cts     = UART_HW_FLOWCTRL_CTS,     /**< CTS only */
  k_star_uart_flow_ctrl_rts_cts = UART_HW_FLOWCTRL_CTS_RTS, /**< RTS/CTS */
} star_uart_flow_ctrl_t;

/**
 * @brief UART mode
 */
typedef enum {
  k_star_uart_mode_uart  = UART_MODE_UART,              /**< Standard UART */
  k_star_uart_mode_rs485 = UART_MODE_RS485_HALF_DUPLEX, /**< RS-485 half duplex */
  k_star_uart_mode_irda  = UART_MODE_IRDA,              /**< IrDA mode */
} star_uart_mode_t;

/**
 * @brief UART event callback
 */
typedef void (*star_uart_event_cb_t)(const char*       bus_name,
                                     uart_event_type_t event_type,
                                     size_t            size,
                                     void*             context);

/**
 * @brief UART configuration
 */
typedef struct {
  /* Basic Configuration */
  uart_port_t           port;      /**< UART port number */
  uint32_t              baud_rate; /**< Baud rate */
  star_uart_data_bits_t data_bits; /**< Data bits */
  star_uart_stop_bits_t stop_bits; /**< Stop bits */
  star_uart_parity_t    parity;    /**< Parity */
  star_uart_flow_ctrl_t flow_ctrl; /**< Flow control */
  star_uart_mode_t      mode;      /**< UART mode */

  /* Pin Configuration */
  gpio_num_t tx_pin;  /**< TX GPIO pin */
  gpio_num_t rx_pin;  /**< RX GPIO pin */
  gpio_num_t rts_pin; /**< RTS GPIO pin (GPIO_NUM_NC if not used) */
  gpio_num_t cts_pin; /**< CTS GPIO pin (GPIO_NUM_NC if not used) */

  /* Buffer Configuration */
  size_t rx_buffer_size; /**< RX buffer size (bytes) */
  size_t tx_buffer_size; /**< TX buffer size (bytes) */

  /* Event Configuration */
  star_uart_event_cb_t event_callback;   /**< Event callback (optional) */
  void*                event_context;    /**< Context for callback */
  size_t               event_queue_size; /**< Event queue size */

  /* Advanced Options */
  bool    use_ref_tick; /**< Use REF_TICK instead of APB */
  uint8_t rx_thresh;    /**< RX FIFO threshold */
  uint8_t tx_thresh;    /**< TX FIFO threshold */
} star_uart_config_t;

/**
 * @brief UART statistics
 */
typedef struct {
  uint64_t bytes_sent;       /**< Total bytes sent */
  uint64_t bytes_received;   /**< Total bytes received */
  uint64_t tx_operations;    /**< Number of write operations */
  uint64_t rx_operations;    /**< Number of read operations */
  uint64_t parity_errors;    /**< Parity errors */
  uint64_t frame_errors;     /**< Frame errors */
  uint64_t overrun_errors;   /**< Buffer overrun errors */
  uint64_t break_conditions; /**< Break conditions detected */
  uint32_t last_tx_time_us;  /**< Last transmit time (us) */
  uint32_t last_rx_time_us;  /**< Last receive time (us) */
} star_uart_stats_t;

/* --- Initialization --- */

/**
 * @brief Initialize UART bus
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name for this UART bus
 * @param[in] config UART configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_init(star_bus_manager_t*       manager,
                             const char*               bus_name,
                             const star_uart_config_t* config);

/**
 * @brief Deinitialize UART bus
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of UART bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_deinit(star_bus_manager_t* manager, const char* bus_name);

/* --- Basic I/O --- */

/**
 * @brief Write data to UART
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of UART bus
 * @param[in] data Data to send
 * @param[in] length Number of bytes to send
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_write(star_bus_manager_t* manager,
                              const char*         bus_name,
                              const uint8_t*      data,
                              size_t              length);

/**
 * @brief Read data from UART
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of UART bus
 * @param[out] data Buffer to store received data
 * @param[in]  buffer_size Size of buffer
 * @param[out] received Number of bytes actually received
 * @param[in]  timeout_ms Timeout in milliseconds
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_read(star_bus_manager_t* manager,
                             const char*         bus_name,
                             uint8_t*            data,
                             size_t              buffer_size,
                             size_t*             received,
                             uint32_t            timeout_ms);

/**
 * @brief Write string to UART (null-terminated)
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of UART bus
 * @param[in] str Null-terminated string
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t
star_bus_uart_write_string(star_bus_manager_t* manager, const char* bus_name, const char* str);

/**
 * @brief Read line from UART (until newline or buffer full)
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of UART bus
 * @param[out] buffer Buffer to store line
 * @param[in]  buffer_size Size of buffer
 * @param[out] length Length of line read
 * @param[in]  timeout_ms Timeout in milliseconds
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_read_line(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  char*               buffer,
                                  size_t              buffer_size,
                                  size_t*             length,
                                  uint32_t            timeout_ms);

/* --- Buffer Management --- */

/**
 * @brief Get number of bytes available in RX buffer
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of UART bus
 * @param[out] available Number of bytes available
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_get_available(const star_bus_manager_t* manager,
                                      const char*               bus_name,
                                      size_t*                   available);

/**
 * @brief Flush UART TX buffer
 *
 * Waits until all data in TX buffer is sent.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of UART bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_flush(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Clear UART RX buffer
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of UART bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_clear_rx(star_bus_manager_t* manager, const char* bus_name);

/* --- Pattern Detection --- */

/**
 * @brief Enable pattern detection
 *
 * Triggers event when specified pattern is detected in RX stream.
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of UART bus
 * @param[in] pattern Pattern character
 * @param[in] count Number of consecutive pattern chars needed
 * @param[in] post_idle Idle time after pattern (bit times)
 * @param[in] pre_idle Idle time before pattern (bit times)
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_enable_pattern(star_bus_manager_t* manager,
                                       const char*         bus_name,
                                       char                pattern,
                                       uint8_t             count,
                                       int32_t             post_idle,
                                       int32_t             pre_idle);

/**
 * @brief Disable pattern detection
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of UART bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_disable_pattern(star_bus_manager_t* manager, const char* bus_name);

/* --- Statistics --- */

/**
 * @brief Get UART statistics
 *
 * @param[in]  manager Pointer to initialized bus manager
 * @param[in]  bus_name Name of UART bus
 * @param[out] stats Pointer to store statistics
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_get_stats(const star_bus_manager_t* manager,
                                  const char*               bus_name,
                                  star_uart_stats_t*        stats);

/**
 * @brief Reset UART statistics
 *
 * @param[in] manager Pointer to initialized bus manager
 * @param[in] bus_name Name of UART bus
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_bus_uart_reset_stats(star_bus_manager_t* manager, const char* bus_name);

/**
 * @brief Print UART statistics
 *
 * @param[in] bus_name Name of UART bus
 * @param[in] stats Pointer to statistics
 */
void star_bus_uart_print_stats(const char* bus_name, const star_uart_stats_t* stats);

/* --- Helper Macros --- */

/**
 * @brief Create default UART configuration
 */
#define STAR_UART_CONFIG_DEFAULT()                                                                 \
  {                                                                                                \
    .port             = UART_NUM_1,                                                                \
    .baud_rate        = 115200,                                                                    \
    .data_bits        = STAR_UART_DATA_8_BITS,                                                     \
    .stop_bits        = STAR_UART_STOP_1_BIT,                                                      \
    .parity           = STAR_UART_PARITY_NONE,                                                     \
    .flow_ctrl        = STAR_UART_FLOW_CTRL_NONE,                                                  \
    .mode             = STAR_UART_MODE_UART,                                                       \
    .tx_pin           = GPIO_NUM_NC,                                                               \
    .rx_pin           = GPIO_NUM_NC,                                                               \
    .rts_pin          = GPIO_NUM_NC,                                                               \
    .cts_pin          = GPIO_NUM_NC,                                                               \
    .rx_buffer_size   = 1024,                                                                      \
    .tx_buffer_size   = 1024,                                                                      \
    .event_callback   = NULL,                                                                      \
    .event_context    = NULL,                                                                      \
    .event_queue_size = 10,                                                                        \
    .use_ref_tick     = false,                                                                     \
    .rx_thresh        = 120,                                                                       \
    .tx_thresh        = 10,                                                                        \
  }

/**
 * @brief Common baud rates
 */
#define STAR_UART_BAUD_300 (300)
#define STAR_UART_BAUD_1200 (1200)
#define STAR_UART_BAUD_2400 (2400)
#define STAR_UART_BAUD_4800 (4800)
#define STAR_UART_BAUD_9600 (9600)
#define STAR_UART_BAUD_19200 (19200)
#define STAR_UART_BAUD_38400 (38400)
#define STAR_UART_BAUD_57600 (57600)
#define STAR_UART_BAUD_115200 (115200)
#define STAR_UART_BAUD_230400 (230400)
#define STAR_UART_BAUD_460800 (460800)
#define STAR_UART_BAUD_921600 (921600)

#ifdef __cplusplus
}
#endif

#endif /* STAR_BUS_UART_H */
