/* esp32-firmware/components/pynq_wifi_bridge/include/pynq_wifi_protocol.h */

#ifndef PYNQ_WIFI_PROTOCOL_H
#define PYNQ_WIFI_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

/* Protocol Constants */
#define PROTOCOL_START_MARKER (0xA5)
#define PROTOCOL_MAX_PAYLOAD_SIZE (1024)
#define PROTOCOL_HEADER_SIZE (4) /* start + cmd + len_lo + len_hi */

/**
 * @brief Command IDs for PYNQ-ESP32 communication protocol
 *
 * Commands are organized by category with specific ID ranges
 */
typedef enum {
  /* System Commands (0x01-0x0F) */
  k_cmd_ping        = 0x01, /* Test connectivity */
  k_cmd_reset       = 0x02, /* Reset ESP32 */
  k_cmd_get_version = 0x03, /* Get firmware version */

  /* WiFi Commands (0x10-0x1F) */
  k_cmd_wifi_connect    = 0x10, /* Connect to WiFi network */
  k_cmd_wifi_disconnect = 0x11, /* Disconnect from WiFi */
  k_cmd_wifi_status     = 0x12, /* Get WiFi connection status */
  k_cmd_wifi_scan       = 0x13, /* Scan for WiFi networks */

  /* Network/Data Commands (0x20-0x2F) */
  k_cmd_http_get    = 0x20, /* HTTP GET request */
  k_cmd_http_post   = 0x21, /* HTTP POST request (for data upload) */
  k_cmd_tcp_connect = 0x22, /* Open TCP connection */
  k_cmd_tcp_send    = 0x23, /* Send TCP data */
  k_cmd_tcp_close   = 0x24, /* Close TCP connection */
  k_cmd_udp_create  = 0x25, /* Create UDP socket */
  k_cmd_udp_send    = 0x26, /* Send UDP datagram */
  k_cmd_udp_close   = 0x27, /* Close UDP socket */

  /* OTA Update Commands (0x28-0x2F) */
  k_cmd_ota_check_update = 0x28, /* Check if update available */
  k_cmd_ota_start_update = 0x29, /* Start OTA update process */
  k_cmd_ota_get_status   = 0x2a, /* Get OTA update status */

  /* Future expansion (placeholders, not implemented) */
  k_cmd_motor_control = 0x30, /* Motor control command */
  k_cmd_sensor_read   = 0x40, /* Read sensor data */

  /* Response/Status (0xF0-0xFF) */
  k_cmd_response = 0xF0, /* Generic response */
  k_cmd_error    = 0xFF, /* Error response */
} protocol_cmd_t;

/**
 * @brief Response status codes
 */
typedef enum {
  k_status_ok           = 0x00, /* Success */
  k_status_error        = 0x01, /* Generic error */
  k_status_invalid_cmd  = 0x02, /* Invalid command */
  k_status_timeout      = 0x03, /* Operation timeout */
  k_status_wifi_failed  = 0x10, /* WiFi operation failed */
  k_status_http_failed  = 0x20, /* HTTP operation failed */
  k_status_ota_failed   = 0x30, /* OTA update failed (generic) */
  k_status_ota_progress = 0x31, /* OTA update in progress */

  /* Detailed OTA error codes */
  k_status_ota_wifi_disconnected = 0x32, /* WiFi disconnected during OTA */
  k_status_ota_sha256_mismatch   = 0x33, /* SHA256 hash verification failed */
  k_status_ota_version_downgrade = 0x34, /* Version downgrade not allowed */
  k_status_ota_partition_error   = 0x35, /* Partition error */
  k_status_ota_write_error       = 0x36, /* Flash write error */
  k_status_ota_http_error        = 0x37, /* HTTP download error */
  k_status_ota_no_update         = 0x38, /* No update available */
} protocol_status_t;

/**
 * @brief WiFi connection status
 */
typedef enum {
  k_wifi_disconnected = 0x00, /* Not connected */
  k_wifi_connecting   = 0x01, /* Connection in progress */
  k_wifi_connected    = 0x02, /* Connected successfully */
  k_wifi_failed       = 0x03, /* Connection failed */
} wifi_status_t;

/**
 * Packet Structure:
 * +-------+--------+--------+--------+-----------+
 * | Start |  CMD   | Len_Lo | Len_Hi |  Payload  |
 * +-------+--------+--------+--------+-----------+
 * | 0xA5  | 1 byte | 1 byte | 1 byte |  N bytes  |
 * +-------+--------+--------+--------+-----------+
 *
 * Start: Always 0xA5
 * CMD: Command ID from protocol_cmd_t
 * Len_Lo: Low byte of payload length
 * Len_Hi: High byte of payload length
 * Payload: Command-specific data (0-1024 bytes)
 */
typedef struct __attribute__((packed)) {
  uint8_t  start;       /* Start marker (0xA5) */
  uint8_t  cmd;         /* Command ID */
  uint16_t payload_len; /* Payload length (little-endian) */
  uint8_t  payload[];   /* Variable length payload */
} protocol_packet_t;

/* Command Payload Structures */

/**
 * @brief Payload for CMD_WIFI_CONNECT command
 */
typedef struct __attribute__((packed)) {
  char ssid[32];     /* WiFi SSID (null-terminated) */
  char password[64]; /* WiFi password (null-terminated) */
} wifi_connect_payload_t;

/**
 * @brief Response payload for CMD_WIFI_STATUS
 */
typedef struct __attribute__((packed)) {
  uint8_t status;     /* wifi_status_t */
  uint8_t ip_addr[4]; /* IP address (if connected) */
  int8_t  rssi;       /* Signal strength (dBm) */
} wifi_status_payload_t;

/**
 * @brief Extended WiFi status response structure (for testing)
 */
typedef struct __attribute__((packed)) {
  uint8_t status;        /* wifi_status_t */
  char    ssid[32];      /* Connected SSID (null-terminated) */
  uint8_t ip_address[4]; /* IP address */
} wifi_status_response_t;

/**
 * @brief Payload for CMD_HTTP_GET command
 */
typedef struct __attribute__((packed)) {
  char url[256]; /* URL (null-terminated) */
} http_get_payload_t;

/**
 * @brief Payload for CMD_HTTP_POST command
 */
typedef struct __attribute__((packed)) {
  uint16_t url_len;  /* Length of URL string */
  uint16_t data_len; /* Length of POST data */
  /* Followed by: url (url_len bytes) + data (data_len bytes) */
} http_post_payload_t;

/**
 * @brief Payload for CMD_TCP_CONNECT command
 */
typedef struct __attribute__((packed)) {
  char     host[128]; /* Hostname or IP address (null-terminated) */
  uint16_t port;      /* TCP port */
} tcp_connect_payload_t;

/**
 * @brief Response payload for CMD_TCP_CONNECT
 */
typedef struct __attribute__((packed)) {
  uint8_t socket_id; /* Socket ID for subsequent operations */
} tcp_connect_response_t;

/**
 * @brief Payload for CMD_TCP_SEND command
 */
typedef struct __attribute__((packed)) {
  uint8_t  socket_id; /* Socket ID from TCP_CONNECT */
  uint16_t data_len;  /* Length of data to send */
  /* Followed by: data (data_len bytes) */
} tcp_send_payload_t;

/**
 * @brief Payload for CMD_TCP_CLOSE command
 */
typedef struct __attribute__((packed)) {
  uint8_t socket_id; /* Socket ID to close */
} tcp_close_payload_t;

/**
 * @brief Payload for CMD_UDP_CREATE command
 */
typedef struct __attribute__((packed)) {
  uint16_t local_port; /* Local port to bind (0 for auto-assign) */
} udp_create_payload_t;

/**
 * @brief Response payload for CMD_UDP_CREATE
 */
typedef struct __attribute__((packed)) {
  uint8_t  socket_id;  /* UDP socket ID for subsequent operations */
  uint16_t local_port; /* Actual local port bound */
} udp_create_response_t;

/**
 * @brief Payload for CMD_UDP_SEND command
 */
typedef struct __attribute__((packed)) {
  uint8_t  socket_id; /* Socket ID from UDP_CREATE */
  char     host[128]; /* Destination hostname or IP (null-terminated) */
  uint16_t port;      /* Destination port */
  uint16_t data_len;  /* Length of data to send */
  /* Followed by: data (data_len bytes) */
} udp_send_payload_t;

/**
 * @brief Payload for CMD_UDP_CLOSE command
 */
typedef struct __attribute__((packed)) {
  uint8_t socket_id; /* UDP socket ID to close */
} udp_close_payload_t;

/**
 * @brief Response payload structure
 */
typedef struct __attribute__((packed)) {
  uint8_t  status;   /* protocol_status_t */
  uint16_t data_len; /* Length of response data */
  /* Followed by: response data (data_len bytes) */
} response_payload_t;

/**
 * @brief Payload for CMD_OTA_START_UPDATE command
 */
typedef struct __attribute__((packed)) {
  char    url[256];        /* Update URL (null-terminated) */
  char    sha256[65];      /* Expected SHA256 hash (64 hex chars + null), or empty to skip */
  uint8_t allow_downgrade; /* 1 to allow version downgrade, 0 to enforce upgrade only */
} ota_start_payload_t;

/**
 * @brief Response payload for CMD_OTA_CHECK_UPDATE
 */
typedef struct __attribute__((packed)) {
  uint8_t update_available;   /* 1 if update available, 0 otherwise */
  uint8_t current_version[3]; /* major.minor.patch */
  uint8_t new_version[3];     /* major.minor.patch */
} ota_check_response_t;

/**
 * @brief Response payload for CMD_OTA_GET_STATUS
 */
typedef struct __attribute__((packed)) {
  uint8_t  state;            /* OTA state (idle/downloading/installing/complete/failed) */
  uint8_t  progress;         /* Progress percentage (0-100) */
  uint32_t bytes_downloaded; /* Bytes downloaded so far */
  uint32_t total_bytes;      /* Total bytes to download */
  uint8_t  error_code;       /* Detailed error code (protocol_status_t) if state is failed */
} ota_status_response_t;

/**
 * @brief Create a protocol packet
 * @param cmd Command ID
 * @param payload Pointer to payload data (can be NULL if payload_len is 0)
 * @param payload_len Length of payload
 * @param packet_out Output buffer for packet (must be large enough)
 * @return Total packet size in bytes
 */
uint16_t
protocol_create_packet(uint8_t cmd, const void* payload, uint16_t payload_len, uint8_t* packet_out);

/**
 * @brief Parse received packet
 * @param data Received data buffer
 * @param data_len Length of received data
 * @param packet_out Parsed packet structure
 * @return true if valid packet, false otherwise
 */
bool protocol_parse_packet(const uint8_t* data, uint16_t data_len, protocol_packet_t** packet_out);

/**
 * @brief Send response packet
 * @param status Status code
 * @param data Response data (can be NULL)
 * @param data_len Length of response data
 */
void protocol_send_response(uint8_t status, const void* data, uint16_t data_len);

/**
 * @brief Send error response
 * @param error_code Error status code
 */
void protocol_send_error(uint8_t error_code);

#endif /* PYNQ_WIFI_PROTOCOL_H */
