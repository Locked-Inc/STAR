/* esp32-firmware/components/pynq_wifi_bridge/include/pynq_wifi_uart.h */

#ifndef PYNQ_WIFI_UART_H
#define PYNQ_WIFI_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "pynq_wifi_protocol.h"

/**
 * @brief Callback function type for received packets
 * @param packet Pointer to parsed packet
 */
typedef void (*uart_rx_callback_t)(protocol_packet_t* packet);

/**
 * @brief Initialize UART transport layer
 * @param rx_callback Callback function for received packets (can be NULL)
 * @return true on success, false on failure
 */
bool uart_transport_init(uart_rx_callback_t rx_callback);

/**
 * @brief Send data over UART
 * @param data Pointer to data buffer
 * @param len Length of data to send
 * @return Number of bytes sent, or -1 on error
 */
int transport_send(const uint8_t* data, uint16_t len);

/**
 * @brief Deinitialize UART transport layer
 */
void uart_transport_deinit(void);

#endif /* PYNQ_WIFI_UART_H */
