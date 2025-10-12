/* esp32-firmware/components/pynq_wifi_bridge/include/pynq_wifi_handler.h */

#ifndef PYNQ_WIFI_HANDLER_H
#define PYNQ_WIFI_HANDLER_H

#include <stdbool.h>

#include "pynq_wifi_protocol.h"

/**
 * @brief Initialize command handler
 * @return true on success, false on failure
 */
bool command_handler_init(void);

/**
 * @brief Process received packet
 * @param packet Pointer to received packet
 *
 * This function is called by the transport layer when a complete
 * packet is received. It dispatches the packet to the appropriate
 * command handler.
 */
void command_handler_process(protocol_packet_t* packet);

/**
 * @brief Deinitialize command handler
 */
void command_handler_deinit(void);

#endif /* PYNQ_WIFI_HANDLER_H */
