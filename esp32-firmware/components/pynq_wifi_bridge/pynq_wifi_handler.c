/* esp32-firmware/components/pynq_wifi_bridge/pynq_wifi_handler.c */

#include "pynq_wifi_handler.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <fcntl.h>
#include <inttypes.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "pynq_ota_manager.h"
#include "pynq_wifi_manager.h"

static const char* TAG = "command_handler";

/* Firmware version */
#define FIRMWARE_VERSION_MAJOR (0)
#define FIRMWARE_VERSION_MINOR (1)
#define FIRMWARE_VERSION_PATCH (0)

/* HTTP client configuration */
#define HTTP_RESPONSE_BUFFER_SIZE (1024)
#define HTTP_TIMEOUT_MS (10000)

/* TCP socket configuration */
#define MAX_TCP_SOCKETS (4)
#define TCP_RECV_BUFFER_SIZE (1024)
#define TCP_CONNECT_TIMEOUT_MS (10000)

/* UDP socket configuration */
#define MAX_UDP_SOCKETS (4)
#define UDP_RECV_BUFFER_SIZE (1024)

static bool g_initialized = false;

/**
 * @brief TCP socket state
 */
typedef struct {
  int32_t socket_fd; /* Socket file descriptor, -1 if unused */
  uint8_t socket_id; /* Socket ID for protocol */
  bool    in_use;    /* Is this socket slot in use */
} tcp_socket_state_t;

/* TCP socket pool */
static tcp_socket_state_t g_tcp_sockets[MAX_TCP_SOCKETS] = {0};

/**
 * @brief UDP socket state
 */
typedef struct {
  int32_t  socket_fd;  /* Socket file descriptor, -1 if unused */
  uint8_t  socket_id;  /* Socket ID for protocol */
  uint16_t local_port; /* Bound local port */
  bool     in_use;     /* Is this socket slot in use */
} udp_socket_state_t;

/* UDP socket pool */
static udp_socket_state_t g_udp_sockets[MAX_UDP_SOCKETS] = {0};

/**
 * @brief HTTP response buffer structure
 */
typedef struct {
  uint8_t* buffer;
  uint16_t size;
  uint16_t offset;
} http_response_buffer_t;

/**
 * @brief HTTP client event handler
 */
static esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
  http_response_buffer_t* buffer = (http_response_buffer_t*)evt->user_data;

  switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
      /* Append data to buffer if there's space */
      if (buffer && evt->data_len > 0) {
        int32_t bytes_to_copy = evt->data_len;
        if (buffer->offset + (uint16_t)bytes_to_copy > buffer->size) {
          bytes_to_copy = buffer->size - buffer->offset;
        }
        if (bytes_to_copy > 0) {
          memcpy(buffer->buffer + buffer->offset, evt->data, (uint16_t)bytes_to_copy);
          buffer->offset += (uint16_t)bytes_to_copy;
        }
      }
      break;

    default:
      break;
  }

  return ESP_OK;
}

/**
 * @brief Allocate a TCP socket slot
 * @return Socket ID (0-3) or 0xFF if none available
 */
static uint8_t tcp_allocate_socket(void)
{
  for (uint8_t i = 0; i < MAX_TCP_SOCKETS; i++) {
    if (!g_tcp_sockets[i].in_use) {
      g_tcp_sockets[i].in_use    = true;
      g_tcp_sockets[i].socket_id = i;
      g_tcp_sockets[i].socket_fd = -1;
      return i;
    }
  }
  return 0xFF; /* No socket available */
}

/**
 * @brief Free a TCP socket slot
 * @param socket_id Socket ID to free
 */
static void tcp_free_socket(uint8_t socket_id)
{
  if (socket_id < MAX_TCP_SOCKETS) {
    if (g_tcp_sockets[socket_id].socket_fd >= 0) {
      close(g_tcp_sockets[socket_id].socket_fd);
    }
    g_tcp_sockets[socket_id].in_use    = false;
    g_tcp_sockets[socket_id].socket_fd = -1;
  }
}

/**
 * @brief Get TCP socket by ID
 * @param socket_id Socket ID
 * @return Pointer to socket state or NULL if invalid
 */
static tcp_socket_state_t* tcp_get_socket(uint8_t socket_id)
{
  if (socket_id < MAX_TCP_SOCKETS && g_tcp_sockets[socket_id].in_use) {
    return &g_tcp_sockets[socket_id];
  }
  return NULL;
}

/**
 * @brief Allocate a UDP socket slot
 * @return Socket ID (0-3) or 0xFF if none available
 */
static uint8_t udp_allocate_socket(void)
{
  for (uint8_t i = 0; i < MAX_UDP_SOCKETS; i++) {
    if (!g_udp_sockets[i].in_use) {
      g_udp_sockets[i].in_use     = true;
      g_udp_sockets[i].socket_id  = i;
      g_udp_sockets[i].socket_fd  = -1;
      g_udp_sockets[i].local_port = 0;
      return i;
    }
  }
  return 0xFF; /* No socket available */
}

/**
 * @brief Free a UDP socket slot
 * @param socket_id Socket ID to free
 */
static void udp_free_socket(uint8_t socket_id)
{
  if (socket_id < MAX_UDP_SOCKETS) {
    if (g_udp_sockets[socket_id].socket_fd >= 0) {
      close(g_udp_sockets[socket_id].socket_fd);
    }
    g_udp_sockets[socket_id].in_use     = false;
    g_udp_sockets[socket_id].socket_fd  = -1;
    g_udp_sockets[socket_id].local_port = 0;
  }
}

/**
 * @brief Get UDP socket by ID
 * @param socket_id Socket ID
 * @return Pointer to socket state or NULL if invalid
 */
static udp_socket_state_t* udp_get_socket(uint8_t socket_id)
{
  if (socket_id < MAX_UDP_SOCKETS && g_udp_sockets[socket_id].in_use) {
    return &g_udp_sockets[socket_id];
  }
  return NULL;
}

/**
 * @brief Handle CMD_PING command
 * @param packet Received packet
 */
static void handle_cmd_ping(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received PING command");

  /* Echo back any payload data */
  uint16_t payload_len = packet->payload_len;
  protocol_send_response(k_status_ok, packet->payload, payload_len);
}

/**
 * @brief Handle CMD_RESET command
 * @param packet Received packet
 */
static void handle_cmd_reset(protocol_packet_t* packet)
{
  ESP_LOGW(TAG, "Received RESET command - restarting in 1 second");

  protocol_send_response(k_status_ok, NULL, 0);

  /* Delay to allow response to be sent */
  vTaskDelay(pdMS_TO_TICKS(1000));

  esp_restart();
}

/**
 * @brief Handle CMD_GET_VERSION command
 * @param packet Received packet
 */
static void handle_cmd_get_version(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received GET_VERSION command");

  /* Create version response: major.minor.patch */
  uint8_t version[3] = {
    FIRMWARE_VERSION_MAJOR,
    FIRMWARE_VERSION_MINOR,
    FIRMWARE_VERSION_PATCH,
  };

  protocol_send_response(k_status_ok, version, sizeof(version));
}

/**
 * @brief Handle CMD_WIFI_CONNECT command
 * @param packet Received packet
 */
static void handle_cmd_wifi_connect(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received WIFI_CONNECT command");

  if (packet->payload_len < sizeof(wifi_connect_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  wifi_connect_payload_t* payload = (wifi_connect_payload_t*)packet->payload;

  ESP_LOGI(TAG, "WiFi connect request: SSID='%s'", payload->ssid);

  /* Attempt to connect to WiFi */
  if (wifi_manager_connect(payload->ssid, payload->password)) {
    protocol_send_response(k_status_ok, NULL, 0);
  } else {
    protocol_send_error(k_status_error);
  }
}

/**
 * @brief Handle CMD_WIFI_DISCONNECT command
 * @param packet Received packet
 */
static void handle_cmd_wifi_disconnect(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received WIFI_DISCONNECT command");

  /* Disconnect from WiFi */
  if (wifi_manager_disconnect()) {
    protocol_send_response(k_status_ok, NULL, 0);
  } else {
    protocol_send_error(k_status_error);
  }
}

/**
 * @brief Handle CMD_WIFI_STATUS command
 * @param packet Received packet
 */
static void handle_cmd_wifi_status(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received WIFI_STATUS command");

  /* Get current WiFi status */
  wifi_status_payload_t status = {
    .status  = wifi_manager_get_status(),
    .ip_addr = {0, 0, 0, 0},
    .rssi    = 0,
  };

  /* Get IP address if connected */
  wifi_manager_get_ip(status.ip_addr);

  /* Get RSSI if connected */
  status.rssi = wifi_manager_get_rssi();

  protocol_send_response(k_status_ok, (uint8_t*)&status, sizeof(status));
}

/**
 * @brief Handle CMD_WIFI_SCAN command
 * @param packet Received packet
 */
static void handle_cmd_wifi_scan(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received WIFI_SCAN command");

  /* Start WiFi scan */
  if (!wifi_manager_start_scan()) {
    ESP_LOGE(TAG, "Failed to start WiFi scan");
    protocol_send_error(k_status_error);
    return;
  }

  /* Get scan results */
  wifi_scan_result_t scan_results[20];
  uint16_t           num_results = 0;

  if (!wifi_manager_get_scan_results(scan_results, 20, &num_results)) {
    ESP_LOGE(TAG, "Failed to get scan results");
    protocol_send_error(k_status_error);
    return;
  }

  ESP_LOGI(TAG, "Scan found %d networks", num_results);

  /* Send response with scan results */
  protocol_send_response(k_status_ok,
                         (uint8_t*)scan_results,
                         num_results * sizeof(wifi_scan_result_t));
}

/**
 * @brief Handle CMD_HTTP_GET command
 * @param packet Received packet
 */
static void handle_cmd_http_get(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received HTTP_GET command");

  if (packet->payload_len < sizeof(http_get_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  http_get_payload_t* payload = (http_get_payload_t*)packet->payload;

  /* Ensure null termination */
  payload->url[sizeof(payload->url) - 1] = '\0';

  ESP_LOGI(TAG, "HTTP GET request: URL='%s'", payload->url);

  /* Check WiFi connection */
  if (wifi_manager_get_status() != k_wifi_connected) {
    ESP_LOGE(TAG, "WiFi not connected");
    protocol_send_error(k_status_wifi_failed);
    return;
  }

  /* Allocate response buffer */
  uint8_t* response_buffer = malloc(HTTP_RESPONSE_BUFFER_SIZE);
  if (!response_buffer) {
    ESP_LOGE(TAG, "Failed to allocate response buffer");
    protocol_send_error(k_status_error);
    return;
  }

  http_response_buffer_t buffer = {
    .buffer = response_buffer,
    .size   = HTTP_RESPONSE_BUFFER_SIZE,
    .offset = 0,
  };

  /* Configure HTTP client */
  esp_http_client_config_t config = {
    .url                   = payload->url,
    .event_handler         = http_event_handler,
    .user_data             = &buffer,
    .timeout_ms            = HTTP_TIMEOUT_MS,
    .disable_auto_redirect = false,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client");
    free(response_buffer);
    protocol_send_error(k_status_error);
    return;
  }

  /* Perform HTTP GET request */
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK) {
    int32_t status_code = esp_http_client_get_status_code(client);
    int32_t content_len = esp_http_client_get_content_length(client);

    ESP_LOGI(TAG,
             "HTTP GET Status = %" PRId32 ", content_length = %" PRId32 ", bytes_received = %u",
             status_code,
             content_len,
             buffer.offset);

    /* Send response with received data */
    protocol_send_response(k_status_ok, buffer.buffer, buffer.offset);
  } else {
    ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    protocol_send_error(k_status_http_failed);
  }

  esp_http_client_cleanup(client);
  free(response_buffer);
}

/**
 * @brief Handle CMD_HTTP_POST command
 * @param packet Received packet
 */
static void handle_cmd_http_post(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received HTTP_POST command");

  if (packet->payload_len < sizeof(http_post_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  http_post_payload_t* header = (http_post_payload_t*)packet->payload;

  /* Validate payload structure */
  uint16_t expected_len = sizeof(http_post_payload_t) + header->url_len + header->data_len;
  if (packet->payload_len < expected_len) {
    ESP_LOGE(TAG, "Invalid payload: expected %u bytes, got %d", expected_len, packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  /* Extract URL and POST data */
  const char* url       = (const char*)(packet->payload + sizeof(http_post_payload_t));
  const char* post_data = url + header->url_len;

  /* Allocate temporary buffer for null-terminated URL */
  char* url_str = malloc(header->url_len + 1);
  if (!url_str) {
    ESP_LOGE(TAG, "Failed to allocate URL buffer");
    protocol_send_error(k_status_error);
    return;
  }
  memcpy(url_str, url, header->url_len);
  url_str[header->url_len] = '\0';

  ESP_LOGI(TAG, "HTTP POST request: URL='%s', data_len=%d", url_str, header->data_len);

  /* Check WiFi connection */
  if (wifi_manager_get_status() != k_wifi_connected) {
    ESP_LOGE(TAG, "WiFi not connected");
    free(url_str);
    protocol_send_error(k_status_wifi_failed);
    return;
  }

  /* Allocate response buffer */
  uint8_t* response_buffer = malloc(HTTP_RESPONSE_BUFFER_SIZE);
  if (!response_buffer) {
    ESP_LOGE(TAG, "Failed to allocate response buffer");
    free(url_str);
    protocol_send_error(k_status_error);
    return;
  }

  http_response_buffer_t buffer = {
    .buffer = response_buffer,
    .size   = HTTP_RESPONSE_BUFFER_SIZE,
    .offset = 0,
  };

  /* Configure HTTP client */
  esp_http_client_config_t config = {
    .url                   = url_str,
    .event_handler         = http_event_handler,
    .user_data             = &buffer,
    .timeout_ms            = HTTP_TIMEOUT_MS,
    .disable_auto_redirect = false,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    ESP_LOGE(TAG, "Failed to initialize HTTP client");
    free(url_str);
    free(response_buffer);
    protocol_send_error(k_status_error);
    return;
  }

  /* Set HTTP method to POST */
  esp_http_client_set_method(client, HTTP_METHOD_POST);

  /* Set POST data */
  esp_http_client_set_post_field(client, post_data, header->data_len);

  /* Perform HTTP POST request */
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK) {
    int32_t status_code = esp_http_client_get_status_code(client);
    int32_t content_len = esp_http_client_get_content_length(client);

    ESP_LOGI(TAG,
             "HTTP POST Status = %" PRId32 ", content_length = %" PRId32 ", bytes_received = %u",
             status_code,
             content_len,
             buffer.offset);

    /* Send response with received data */
    protocol_send_response(k_status_ok, buffer.buffer, buffer.offset);
  } else {
    ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    protocol_send_error(k_status_http_failed);
  }

  esp_http_client_cleanup(client);
  free(url_str);
  free(response_buffer);
}

/**
 * @brief Handle CMD_TCP_CONNECT command
 * @param packet Received packet
 */
static void handle_cmd_tcp_connect(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received TCP_CONNECT command");

  if (packet->payload_len < sizeof(tcp_connect_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  tcp_connect_payload_t* payload = (tcp_connect_payload_t*)packet->payload;

  /* Ensure null termination */
  payload->host[sizeof(payload->host) - 1] = '\0';

  ESP_LOGI(TAG, "TCP connect request: host='%s', port=%u", payload->host, payload->port);

  /* Check WiFi connection */
  if (wifi_manager_get_status() != k_wifi_connected) {
    ESP_LOGE(TAG, "WiFi not connected");
    protocol_send_error(k_status_wifi_failed);
    return;
  }

  /* Allocate socket slot */
  uint8_t socket_id = tcp_allocate_socket();
  if (socket_id == 0xFF) {
    ESP_LOGE(TAG, "No TCP sockets available");
    protocol_send_error(k_status_error);
    return;
  }

  tcp_socket_state_t* sock = tcp_get_socket(socket_id);
  if (!sock) {
    ESP_LOGE(TAG, "Failed to get socket");
    protocol_send_error(k_status_error);
    return;
  }

  /* Resolve hostname */
  struct addrinfo hints = {
    .ai_family   = AF_INET,
    .ai_socktype = SOCK_STREAM,
  };
  struct addrinfo* res = NULL;

  char port_str[8];
  snprintf(port_str, sizeof(port_str), "%u", payload->port);

  int32_t err = getaddrinfo(payload->host, port_str, &hints, &res);
  if (err != 0 || res == NULL) {
    ESP_LOGE(TAG, "DNS lookup failed: %d", err);
    tcp_free_socket(socket_id);
    protocol_send_error(k_status_error);
    return;
  }

  /* Create socket */
  sock->socket_fd = socket(res->ai_family, res->ai_socktype, 0);
  if (sock->socket_fd < 0) {
    ESP_LOGE(TAG, "Failed to create socket");
    freeaddrinfo(res);
    tcp_free_socket(socket_id);
    protocol_send_error(k_status_error);
    return;
  }

  /* Set socket timeout */
  struct timeval timeout = {
    .tv_sec  = TCP_CONNECT_TIMEOUT_MS / 1000,
    .tv_usec = (TCP_CONNECT_TIMEOUT_MS % 1000) * 1000,
  };
  setsockopt(sock->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(sock->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  /* Connect to server */
  err = connect(sock->socket_fd, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);

  if (err != 0) {
    ESP_LOGE(TAG, "TCP connect failed: %d", err);
    tcp_free_socket(socket_id);
    protocol_send_error(k_status_error);
    return;
  }

  ESP_LOGI(TAG, "TCP connected successfully, socket_id=%u", socket_id);

  /* Send response with socket ID */
  tcp_connect_response_t response = {.socket_id = socket_id};
  protocol_send_response(k_status_ok, (uint8_t*)&response, sizeof(response));
}

/**
 * @brief Handle CMD_TCP_SEND command
 * @param packet Received packet
 */
static void handle_cmd_tcp_send(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received TCP_SEND command");

  if (packet->payload_len < sizeof(tcp_send_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  tcp_send_payload_t* header = (tcp_send_payload_t*)packet->payload;

  /* Validate payload */
  uint16_t expected_len = sizeof(tcp_send_payload_t) + header->data_len;
  if (packet->payload_len < expected_len) {
    ESP_LOGE(TAG, "Invalid payload: expected %u bytes, got %d", expected_len, packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  /* Get socket */
  tcp_socket_state_t* sock = tcp_get_socket(header->socket_id);
  if (!sock) {
    ESP_LOGE(TAG, "Invalid socket ID: %u", header->socket_id);
    protocol_send_error(k_status_error);
    return;
  }

  /* Extract data */
  const uint8_t* data = packet->payload + sizeof(tcp_send_payload_t);

  ESP_LOGI(TAG, "TCP send: socket_id=%u, data_len=%u", header->socket_id, header->data_len);

  /* Send data */
  int32_t bytes_sent = send(sock->socket_fd, data, header->data_len, 0);
  if (bytes_sent < 0) {
    ESP_LOGE(TAG, "TCP send failed: %d", bytes_sent);
    protocol_send_error(k_status_error);
    return;
  }

  ESP_LOGI(TAG, "TCP sent %" PRId32 " bytes", bytes_sent);

  /* Try to receive response (non-blocking) */
  uint8_t* recv_buffer = malloc(TCP_RECV_BUFFER_SIZE);
  if (!recv_buffer) {
    ESP_LOGE(TAG, "Failed to allocate receive buffer");
    protocol_send_error(k_status_error);
    return;
  }

  /* Set non-blocking mode for receive */
  int32_t flags = fcntl(sock->socket_fd, F_GETFL, 0);
  fcntl(sock->socket_fd, F_SETFL, flags | O_NONBLOCK);

  int32_t bytes_received = recv(sock->socket_fd, recv_buffer, TCP_RECV_BUFFER_SIZE, 0);

  /* Restore blocking mode */
  fcntl(sock->socket_fd, F_SETFL, flags);

  if (bytes_received > 0) {
    ESP_LOGI(TAG, "TCP received %" PRId32 " bytes", bytes_received);
    protocol_send_response(k_status_ok, recv_buffer, (uint16_t)bytes_received);
  } else {
    /* No data received, just acknowledge send */
    protocol_send_response(k_status_ok, NULL, 0);
  }

  free(recv_buffer);
}

/**
 * @brief Handle CMD_TCP_CLOSE command
 * @param packet Received packet
 */
static void handle_cmd_tcp_close(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received TCP_CLOSE command");

  if (packet->payload_len < sizeof(tcp_close_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  tcp_close_payload_t* payload = (tcp_close_payload_t*)packet->payload;

  ESP_LOGI(TAG, "TCP close request: socket_id=%u", payload->socket_id);

  /* Get socket */
  tcp_socket_state_t* sock = tcp_get_socket(payload->socket_id);
  if (!sock) {
    ESP_LOGE(TAG, "Invalid socket ID: %u", payload->socket_id);
    protocol_send_error(k_status_error);
    return;
  }

  /* Close and free socket */
  tcp_free_socket(payload->socket_id);

  ESP_LOGI(TAG, "TCP socket closed successfully");
  protocol_send_response(k_status_ok, NULL, 0);
}

/**
 * @brief Handle CMD_UDP_CREATE command
 * @param packet Received packet
 */
static void handle_cmd_udp_create(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received UDP_CREATE command");

  if (packet->payload_len < sizeof(udp_create_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  udp_create_payload_t* payload = (udp_create_payload_t*)packet->payload;

  ESP_LOGI(TAG, "UDP create request: local_port=%u", payload->local_port);

  /* Check WiFi connection */
  if (wifi_manager_get_status() != k_wifi_connected) {
    ESP_LOGE(TAG, "WiFi not connected");
    protocol_send_error(k_status_wifi_failed);
    return;
  }

  /* Allocate socket slot */
  uint8_t socket_id = udp_allocate_socket();
  if (socket_id == 0xFF) {
    ESP_LOGE(TAG, "No UDP sockets available");
    protocol_send_error(k_status_error);
    return;
  }

  udp_socket_state_t* sock = udp_get_socket(socket_id);
  if (!sock) {
    ESP_LOGE(TAG, "Failed to get socket");
    protocol_send_error(k_status_error);
    return;
  }

  /* Create UDP socket */
  sock->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock->socket_fd < 0) {
    ESP_LOGE(TAG, "Failed to create UDP socket");
    udp_free_socket(socket_id);
    protocol_send_error(k_status_error);
    return;
  }

  /* Bind to local port if specified */
  struct sockaddr_in local_addr = {
    .sin_family      = AF_INET,
    .sin_addr.s_addr = htonl(INADDR_ANY),
    .sin_port        = htons(payload->local_port),
  };

  if (bind(sock->socket_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) != 0) {
    ESP_LOGE(TAG, "Failed to bind UDP socket to port %u", payload->local_port);
    udp_free_socket(socket_id);
    protocol_send_error(k_status_error);
    return;
  }

  /* Get the actual bound port */
  socklen_t addr_len = sizeof(local_addr);
  if (getsockname(sock->socket_fd, (struct sockaddr*)&local_addr, &addr_len) == 0) {
    sock->local_port = ntohs(local_addr.sin_port);
  } else {
    sock->local_port = payload->local_port;
  }

  ESP_LOGI(TAG,
           "UDP socket created successfully, socket_id=%u, local_port=%u",
           socket_id,
           sock->local_port);

  /* Send response with socket ID and actual bound port */
  udp_create_response_t response = {
    .socket_id  = socket_id,
    .local_port = sock->local_port,
  };
  protocol_send_response(k_status_ok, (uint8_t*)&response, sizeof(response));
}

/**
 * @brief Handle CMD_UDP_SEND command
 * @param packet Received packet
 */
static void handle_cmd_udp_send(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received UDP_SEND command");

  if (packet->payload_len < sizeof(udp_send_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  udp_send_payload_t* header = (udp_send_payload_t*)packet->payload;

  /* Validate payload */
  uint16_t expected_len = sizeof(udp_send_payload_t) + header->data_len;
  if (packet->payload_len < expected_len) {
    ESP_LOGE(TAG, "Invalid payload: expected %u bytes, got %d", expected_len, packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  /* Get socket */
  udp_socket_state_t* sock = udp_get_socket(header->socket_id);
  if (!sock) {
    ESP_LOGE(TAG, "Invalid socket ID: %u", header->socket_id);
    protocol_send_error(k_status_error);
    return;
  }

  /* Ensure null termination of host */
  header->host[sizeof(header->host) - 1] = '\0';

  ESP_LOGI(TAG,
           "UDP send: socket_id=%u, host='%s', port=%u, data_len=%u",
           header->socket_id,
           header->host,
           header->port,
           header->data_len);

  /* Extract data */
  const uint8_t* data = packet->payload + sizeof(udp_send_payload_t);

  /* Resolve hostname */
  struct addrinfo hints = {
    .ai_family   = AF_INET,
    .ai_socktype = SOCK_DGRAM,
  };
  struct addrinfo* res = NULL;

  char port_str[8];
  snprintf(port_str, sizeof(port_str), "%u", header->port);

  int32_t err = getaddrinfo(header->host, port_str, &hints, &res);
  if (err != 0 || res == NULL) {
    ESP_LOGE(TAG, "DNS lookup failed for %s: %d", header->host, err);
    protocol_send_error(k_status_error);
    return;
  }

  /* Send datagram */
  int32_t bytes_sent =
    sendto(sock->socket_fd, data, header->data_len, 0, res->ai_addr, res->ai_addrlen);
  freeaddrinfo(res);

  if (bytes_sent < 0) {
    ESP_LOGE(TAG, "UDP send failed: %d", bytes_sent);
    protocol_send_error(k_status_error);
    return;
  }

  ESP_LOGI(TAG, "UDP sent %" PRId32 " bytes", bytes_sent);

  /* UDP is connectionless, no response expected - just acknowledge send */
  protocol_send_response(k_status_ok, NULL, 0);
}

/**
 * @brief Handle CMD_UDP_CLOSE command
 * @param packet Received packet
 */
static void handle_cmd_udp_close(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received UDP_CLOSE command");

  if (packet->payload_len < sizeof(udp_close_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  udp_close_payload_t* payload = (udp_close_payload_t*)packet->payload;

  ESP_LOGI(TAG, "UDP close request: socket_id=%u", payload->socket_id);

  /* Get socket */
  udp_socket_state_t* sock = udp_get_socket(payload->socket_id);
  if (!sock) {
    ESP_LOGE(TAG, "Invalid socket ID: %u", payload->socket_id);
    protocol_send_error(k_status_error);
    return;
  }

  /* Close and free socket */
  udp_free_socket(payload->socket_id);

  ESP_LOGI(TAG, "UDP socket closed successfully");
  protocol_send_response(k_status_ok, NULL, 0);
}

/**
 * @brief Handle CMD_OTA_CHECK_UPDATE command
 * @param packet Received packet
 */
static void handle_cmd_ota_check_update(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received OTA_CHECK_UPDATE command");

  bool available = ota_manager_check_update();

  ota_check_response_t response;
  ota_manager_get_version(&response.current_version[0],
                          &response.current_version[1],
                          &response.current_version[2]);

  response.update_available = available ? 1 : 0;

  /* Get new version from OTA manager status */
  ota_status_t status;
  if (ota_manager_get_status(&status)) {
    memcpy(response.new_version, status.new_version, sizeof(response.new_version));
  } else {
    /* If status unavailable, echo current version */
    memcpy(response.new_version, response.current_version, sizeof(response.new_version));
  }

  protocol_send_response(k_status_ok, (uint8_t*)&response, sizeof(response));
}

/**
 * @brief Handle CMD_OTA_START_UPDATE command
 * @param packet Received packet
 */
static void handle_cmd_ota_start_update(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received OTA_START_UPDATE command");

  if (packet->payload_len < sizeof(ota_start_payload_t)) {
    ESP_LOGE(TAG, "Invalid payload length: %d", packet->payload_len);
    protocol_send_error(k_status_error);
    return;
  }

  ota_start_payload_t* payload = (ota_start_payload_t*)packet->payload;

  /* Ensure null termination of strings */
  payload->url[sizeof(payload->url) - 1]       = '\0';
  payload->sha256[sizeof(payload->sha256) - 1] = '\0';

  ESP_LOGI(TAG, "OTA update request: URL='%s'", payload->url);

  /* Check if SHA256 is provided */
  const char* sha256_hash     = (strlen(payload->sha256) == 64) ? payload->sha256 : NULL;
  bool        allow_downgrade = (payload->allow_downgrade != 0);

  if (sha256_hash) {
    ESP_LOGI(TAG, "SHA256 hash provided: %s", sha256_hash);
  } else {
    ESP_LOGW(TAG, "No SHA256 hash provided - skipping verification");
  }

  if (allow_downgrade) {
    ESP_LOGW(TAG, "Version downgrade allowed by PYNQ");
  }

  /* Start verified update */
  if (ota_manager_start_update_verified(payload->url, sha256_hash, allow_downgrade)) {
    protocol_send_response(k_status_ok, NULL, 0);
  } else {
    protocol_send_error(k_status_ota_failed);
  }
}

/**
 * @brief Handle CMD_OTA_GET_STATUS command
 * @param packet Received packet
 */
static void handle_cmd_ota_get_status(protocol_packet_t* packet)
{
  ESP_LOGI(TAG, "Received OTA_GET_STATUS command");

  ota_status_t status;
  if (!ota_manager_get_status(&status)) {
    protocol_send_error(k_status_error);
    return;
  }

  ota_status_response_t response = {
    .state            = status.state,
    .progress         = status.progress,
    .bytes_downloaded = status.bytes_downloaded,
    .total_bytes      = status.total_bytes,
    .error_code       = status.error_code,
  };

  protocol_send_response(k_status_ok, (uint8_t*)&response, sizeof(response));
}

/**
 * @brief Handle unknown command
 * @param packet Received packet
 */
static void handle_cmd_unknown(protocol_packet_t* packet)
{
  ESP_LOGW(TAG, "Unknown command: 0x%02X", packet->cmd);
  protocol_send_error(k_status_invalid_cmd);
}

bool command_handler_init(void)
{
  if (g_initialized) {
    ESP_LOGW(TAG, "Already initialized");
    return true;
  }

  ESP_LOGI(TAG, "Initializing command handler");

  /* Initialize WiFi manager */
  if (!wifi_manager_init()) {
    ESP_LOGE(TAG, "Failed to initialize WiFi manager");
    return false;
  }

  g_initialized = true;

  ESP_LOGI(TAG, "Command handler initialized");

  return true;
}

void command_handler_process(protocol_packet_t* packet)
{
  if (!g_initialized) {
    ESP_LOGE(TAG, "Command handler not initialized");
    return;
  }

  if (!packet) {
    ESP_LOGE(TAG, "Null packet");
    return;
  }

  ESP_LOGD(TAG, "Processing command: 0x%02X", packet->cmd);

  /* Dispatch command to handler */
  switch (packet->cmd) {
    case k_cmd_ping:
      handle_cmd_ping(packet);
      break;

    case k_cmd_reset:
      handle_cmd_reset(packet);
      break;

    case k_cmd_get_version:
      handle_cmd_get_version(packet);
      break;

    case k_cmd_wifi_connect:
      handle_cmd_wifi_connect(packet);
      break;

    case k_cmd_wifi_disconnect:
      handle_cmd_wifi_disconnect(packet);
      break;

    case k_cmd_wifi_status:
      handle_cmd_wifi_status(packet);
      break;

    case k_cmd_wifi_scan:
      handle_cmd_wifi_scan(packet);
      break;

    case k_cmd_http_get:
      handle_cmd_http_get(packet);
      break;

    case k_cmd_http_post:
      handle_cmd_http_post(packet);
      break;

    case k_cmd_tcp_connect:
      handle_cmd_tcp_connect(packet);
      break;

    case k_cmd_tcp_send:
      handle_cmd_tcp_send(packet);
      break;

    case k_cmd_tcp_close:
      handle_cmd_tcp_close(packet);
      break;

    case k_cmd_udp_create:
      handle_cmd_udp_create(packet);
      break;

    case k_cmd_udp_send:
      handle_cmd_udp_send(packet);
      break;

    case k_cmd_udp_close:
      handle_cmd_udp_close(packet);
      break;

    case k_cmd_ota_check_update:
      handle_cmd_ota_check_update(packet);
      break;

    case k_cmd_ota_start_update:
      handle_cmd_ota_start_update(packet);
      break;

    case k_cmd_ota_get_status:
      handle_cmd_ota_get_status(packet);
      break;

    default:
      handle_cmd_unknown(packet);
      break;
  }
}

void command_handler_deinit(void)
{
  if (!g_initialized) {
    return;
  }

  ESP_LOGI(TAG, "Deinitializing command handler");

  /* Close all TCP sockets */
  for (uint8_t i = 0; i < MAX_TCP_SOCKETS; i++) {
    if (g_tcp_sockets[i].in_use) {
      tcp_free_socket(i);
    }
  }

  /* Close all UDP sockets */
  for (uint8_t i = 0; i < MAX_UDP_SOCKETS; i++) {
    if (g_udp_sockets[i].in_use) {
      udp_free_socket(i);
    }
  }

  /* Deinitialize WiFi manager */
  wifi_manager_deinit();

  g_initialized = false;

  ESP_LOGI(TAG, "Command handler deinitialized");
}
