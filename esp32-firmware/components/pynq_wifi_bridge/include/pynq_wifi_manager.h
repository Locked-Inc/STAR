/* esp32-firmware/components/pynq_wifi_bridge/include/pynq_wifi_manager.h */

#ifndef PYNQ_WIFI_MANAGER_H
#define PYNQ_WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "pynq_wifi_protocol.h"

/* WiFi scan result entry */
typedef struct {
  char    ssid[33];  /* SSID (max 32 chars + null terminator) */
  int8_t  rssi;      /* Signal strength in dBm */
  uint8_t channel;   /* WiFi channel */
  uint8_t auth_mode; /* Authentication mode */
} wifi_scan_result_t;

/**
 * @brief Initialize WiFi manager
 * @return true on success, false on failure
 */
bool wifi_manager_init(void);

/**
 * @brief Connect to WiFi network
 * @param ssid WiFi SSID (null-terminated string)
 * @param password WiFi password (null-terminated string)
 * @return true if connection started, false on error
 */
bool wifi_manager_connect(const char* ssid, const char* password);

/**
 * @brief Disconnect from WiFi network
 * @return true on success, false on failure
 */
bool wifi_manager_disconnect(void);

/**
 * @brief Get current WiFi connection status
 * @return Current WiFi status
 */
wifi_status_t wifi_manager_get_status(void);

/**
 * @brief Get IP address (if connected)
 * @param ip_addr Buffer to store IP address (4 bytes)
 * @return true if connected and IP retrieved, false otherwise
 */
bool wifi_manager_get_ip(uint8_t ip_addr[4]);

/**
 * @brief Get RSSI (signal strength)
 * @return RSSI in dBm, or 0 if not connected
 */
int8_t wifi_manager_get_rssi(void);

/**
 * @brief Start WiFi scan for available networks
 * @return true if scan started, false on error
 */
bool wifi_manager_start_scan(void);

/**
 * @brief Get scan results
 * @param results Buffer to store scan results
 * @param max_results Maximum number of results to return
 * @param num_results Output: actual number of results returned
 * @return true on success, false on failure
 */
bool wifi_manager_get_scan_results(wifi_scan_result_t* results,
                                   uint16_t            max_results,
                                   uint16_t*           num_results);

/**
 * @brief Deinitialize WiFi manager
 */
void wifi_manager_deinit(void);

#endif /* PYNQ_WIFI_MANAGER_H */
