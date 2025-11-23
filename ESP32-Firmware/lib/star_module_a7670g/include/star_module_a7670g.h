/* lib/star_module_a7670g/include/star_module_a7670g.h */

#ifndef STAR_MODULE_A7670G_H
#define STAR_MODULE_A7670G_H

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "star_error_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file star_module_a7670g.h
 * @brief A7670G 4G LTE CAT-1 module driver
 *
 * Features:
 * - AT command interface
 * - Network registration (2G/3G/4G)
 * - TCP/IP stack
 * - MQTT client
 * - HTTP/HTTPS client
 * - SMS send/receive
 * - GPS/GNSS positioning
 * - File system operations
 *
 * Protocol: UART AT commands (115200 baud default)
 */

#define A7670G_UART_BAUD_RATE (115200)
#define A7670G_UART_BUF_SIZE (2048)
#define A7670G_AT_RESPONSE_TIMEOUT_MS (10000)
#define A7670G_MAX_RESPONSE_LEN (512)

typedef enum {
  A7670G_NET_STATUS_UNKNOWN = 0,
  A7670G_NET_STATUS_SEARCHING,
  A7670G_NET_STATUS_DENIED,
  A7670G_NET_STATUS_REGISTERED_HOME,
  A7670G_NET_STATUS_REGISTERED_ROAMING,
} a7670g_network_status_t;

typedef struct {
  char     operator_name[32];
  int8_t   signal_quality;  // 0-31 (99=unknown)
  uint8_t  network_type;     // 0=no service, 7=LTE
  a7670g_network_status_t network_status;
  bool     gprs_attached;
} a7670g_network_info_t;

typedef struct {
  float    latitude;
  float    longitude;
  float    altitude;
  float    speed;
  uint8_t  satellites;
  bool     fix_valid;
} a7670g_gps_data_t;

typedef struct {
  uart_port_t uart_port;
  int         tx_pin;
  int         rx_pin;
  gpio_num_t  power_pin;    // Power key pin
  gpio_num_t  reset_pin;    // Reset pin (optional)
  const char* apn;          // APN for data connection
} a7670g_config_t;

typedef struct a7670g_handle {
  uart_port_t     uart_port;
  gpio_num_t      power_pin;
  gpio_num_t      reset_pin;
  error_handler_t error_handler;
  bool            initialized;
  char            response_buffer[A7670G_MAX_RESPONSE_LEN];
  uint16_t        response_index;
  bool            command_pending;
  const char*     apn;
} a7670g_handle_t;

/**
 * @brief Initialize A7670G module
 *
 * @param[out] handle Pointer to handle structure
 * @param[in]  config Device configuration
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_init(a7670g_handle_t* handle, const a7670g_config_t* config);

/**
 * @brief Deinitialize A7670G module
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_deinit(a7670g_handle_t* handle);

/**
 * @brief Power on module
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_power_on(const a7670g_handle_t* handle);

/**
 * @brief Power off module
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_power_off(const a7670g_handle_t* handle);

/**
 * @brief Reset module
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_reset(const a7670g_handle_t* handle);

/**
 * @brief Send AT command and wait for response
 *
 * @param[in]  handle       Pointer to initialized handle
 * @param[in]  command      AT command string (without CRLF)
 * @param[out] response     Buffer for response
 * @param[in]  response_len Response buffer size
 * @param[in]  timeout_ms   Timeout in milliseconds
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_send_at_command(a7670g_handle_t* handle,
                                              const char*      command,
                                              char*            response,
                                              size_t           response_len,
                                              uint32_t         timeout_ms);

/**
 * @brief Check if module is ready
 *
 * @param[in]  handle   Pointer to initialized handle
 * @param[out] ready    true if module responds to AT
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_is_ready(a7670g_handle_t* handle, bool* ready);

/**
 * @brief Get network registration status
 *
 * @param[in]  handle Pointer to initialized handle
 * @param[out] info   Network information
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_get_network_info(a7670g_handle_t* handle, a7670g_network_info_t* info);

/**
 * @brief Attach to GPRS network
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_attach_gprs(a7670g_handle_t* handle);

/**
 * @brief Connect to network with APN
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_connect_network(a7670g_handle_t* handle);

/**
 * @brief Send SMS
 *
 * @param[in] handle  Pointer to initialized handle
 * @param[in] number  Phone number
 * @param[in] message SMS message text
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_send_sms(a7670g_handle_t* handle,
                                       const char*      number,
                                       const char*      message);

/**
 * @brief Start GPS
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_gps_start(a7670g_handle_t* handle);

/**
 * @brief Get GPS position
 *
 * @param[in]  handle  Pointer to initialized handle
 * @param[out] gps_data GPS position data
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_gps_get_position(a7670g_handle_t* handle, a7670g_gps_data_t* gps_data);

/**
 * @brief Stop GPS
 *
 * @param[in] handle Pointer to initialized handle
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_gps_stop(a7670g_handle_t* handle);

/**
 * @brief HTTP GET request
 *
 * @param[in]  handle       Pointer to initialized handle
 * @param[in]  url          URL to fetch
 * @param[out] response     Response buffer
 * @param[in]  response_len Response buffer size
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t star_module_a7670g_http_get(a7670g_handle_t* handle,
                                       const char*      url,
                                       char*            response,
                                       size_t           response_len);

#ifdef __cplusplus
}
#endif

#endif /* STAR_MODULE_A7670G_H */
