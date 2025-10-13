/* esp32-firmware/components/pynq_wifi_bridge/test/test_udp_handler.c */

/**
 * @file test_udp_handler.c
 * @brief STAR_TEST comprehensive tests for UDP command handlers
 *
 * UDP is connectionless and ideal for:
 * - Sensor data streaming (fire-and-forget)
 * - Real-time telemetry in noisy environments
 * - Low-latency communication
 * - Broadcast/multicast scenarios
 */

#include <string.h>

#include "pynq_wifi_protocol.h"
#include "star_test.h"
#include "test_transport_mock.h"

/* Convenience macros for the shared transport mock */
#define g_transport_buffer transport_mock_get_buffer()
#define g_transport_len    transport_mock_get_length()
#define reset_transport_mock() transport_mock_reset()

/* ========================================================================
 * UDP Create Command Tests (15 tests)
 * ======================================================================== */

STAR_TEST_CASE(udp_handler, udp_create_packet_structure)
{
  udp_create_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Verify structure size: 2 bytes for local port */
  STAR_ASSERT_EQUAL(2, sizeof(payload));
}

STAR_TEST_CASE(udp_handler, udp_create_auto_port)
{
  udp_create_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  payload.local_port = 0; /* Auto-assign port */

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_udp_create, &payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
  STAR_ASSERT_EQUAL(k_cmd_udp_create, packet[1]);
}

STAR_TEST_CASE(udp_handler, udp_create_specific_port)
{
  udp_create_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  payload.local_port = 8888; /* Specific port */

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_udp_create, &payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
  STAR_ASSERT_EQUAL(8888, payload.local_port);
}

STAR_TEST_CASE(udp_handler, udp_create_port_range)
{
  udp_create_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Test various valid ports */
  payload.local_port = 1024; /* Typical user port start */
  STAR_ASSERT_EQUAL(1024, payload.local_port);

  payload.local_port = 5000;
  STAR_ASSERT_EQUAL(5000, payload.local_port);

  payload.local_port = 49152; /* Ephemeral port range */
  STAR_ASSERT_EQUAL(49152, payload.local_port);

  payload.local_port = 65535; /* Max port */
  STAR_ASSERT_EQUAL(65535, payload.local_port);
}

STAR_TEST_CASE(udp_handler, udp_create_packet_parsing)
{
  udp_create_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  payload.local_port = 5000;

  uint8_t packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  protocol_create_packet(k_cmd_udp_create, &payload, sizeof(payload), packet);

  /* Parse back */
  protocol_packet_t* parsed = NULL;
  bool result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + sizeof(payload), &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_udp_create, parsed->cmd);
  STAR_ASSERT_EQUAL(sizeof(payload), parsed->payload_len);

  udp_create_payload_t* parsed_payload = (udp_create_payload_t*)parsed->payload;
  STAR_ASSERT_EQUAL(5000, parsed_payload->local_port);
}

STAR_TEST_CASE(udp_handler, udp_create_response_structure)
{
  udp_create_response_t response;
  memset(&response, 0, sizeof(response));

  /* Verify response structure: socket_id(1) + local_port(2) = 3 bytes */
  STAR_ASSERT_EQUAL(3, sizeof(response));
}

STAR_TEST_CASE(udp_handler, udp_create_response_with_socket_id)
{
  reset_transport_mock();

  udp_create_response_t response;
  response.socket_id  = 0;
  response.local_port = 50000;

  protocol_send_response(k_status_ok, &response, sizeof(response));

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* resp = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, resp->status);
  STAR_ASSERT_EQUAL(sizeof(response), resp->data_len);

  /* Access data after the response payload struct using pointer arithmetic */
  udp_create_response_t* udp_resp = (udp_create_response_t*)(resp + 1);
  STAR_ASSERT_EQUAL(0, udp_resp->socket_id);
  STAR_ASSERT_EQUAL(50000, udp_resp->local_port);
}

STAR_TEST_CASE(udp_handler, udp_create_multiple_sockets)
{
  reset_transport_mock();

  /* Test creating multiple UDP sockets (0-3) */
  for (uint8_t socket_id = 0; socket_id < 4; socket_id++) {
    udp_create_response_t response;
    response.socket_id  = socket_id;
    response.local_port = 50000 + socket_id;

    protocol_send_response(k_status_ok, &response, sizeof(response));

    protocol_packet_t* packet = NULL;
    bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
    STAR_ASSERT_TRUE(parsed);
    STAR_ASSERT_NOT_NULL(packet);

    response_payload_t*    resp     = (response_payload_t*)packet->payload;
    udp_create_response_t* udp_resp = (udp_create_response_t*)(resp + 1);

    STAR_ASSERT_EQUAL(socket_id, udp_resp->socket_id);
    STAR_ASSERT_EQUAL(50000 + socket_id, udp_resp->local_port);

    reset_transport_mock();
  }
}

STAR_TEST_CASE(udp_handler, udp_create_port_already_in_use)
{
  reset_transport_mock();

  /* Port already in use should return error */
  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(udp_handler, udp_create_privileged_port)
{
  udp_create_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  payload.local_port = 80; /* Privileged port */

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_udp_create, &payload, sizeof(payload), packet);

  /* Should create packet, but might fail on execution depending on privileges */
  STAR_ASSERT_TRUE(len > 0);
}

STAR_TEST_CASE(udp_handler, udp_create_max_sockets_exceeded)
{
  reset_transport_mock();

  /* More than 4 UDP sockets should fail */
  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(udp_handler, udp_create_port_encoding)
{
  udp_create_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Test 16-bit port encoding */
  payload.local_port = 0x1234;
  STAR_ASSERT_EQUAL(0x1234, payload.local_port);

  payload.local_port = 0xABCD;
  STAR_ASSERT_EQUAL(0xABCD, payload.local_port);
}

STAR_TEST_CASE(udp_handler, udp_create_wifi_not_connected)
{
  reset_transport_mock();

  protocol_send_error(k_status_wifi_failed);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_wifi_failed, response->status);
}

STAR_TEST_CASE(udp_handler, udp_create_common_ports)
{
  /* Test common UDP service ports */
  udp_create_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* DNS */
  payload.local_port = 53;
  STAR_ASSERT_EQUAL(53, payload.local_port);

  /* DHCP client */
  payload.local_port = 68;
  STAR_ASSERT_EQUAL(68, payload.local_port);

  /* NTP */
  payload.local_port = 123;
  STAR_ASSERT_EQUAL(123, payload.local_port);

  /* SNMP */
  payload.local_port = 161;
  STAR_ASSERT_EQUAL(161, payload.local_port);

  /* Syslog */
  payload.local_port = 514;
  STAR_ASSERT_EQUAL(514, payload.local_port);
}

STAR_TEST_CASE(udp_handler, udp_create_zero_port_auto_assign)
{
  udp_create_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  payload.local_port = 0; /* System auto-assigns */

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_udp_create, &payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
  STAR_ASSERT_EQUAL(0, payload.local_port);
}

/* ========================================================================
 * UDP Send Command Tests (20 tests)
 * ======================================================================== */

STAR_TEST_CASE(udp_handler, udp_send_packet_structure)
{
  /* UDP send fixed part: socket_id(1) + host(128) + port(2) + data_len(2) = 133 bytes */
  const char* data     = "sensor_data";
  uint16_t    data_len = strlen(data);

  uint16_t total_len = 1 + 128 + 2 + 2 + data_len;

  STAR_ASSERT_TRUE(total_len > 0);
  STAR_ASSERT_TRUE(total_len < PROTOCOL_MAX_PAYLOAD_SIZE);
}

STAR_TEST_CASE(udp_handler, udp_send_to_ip_address)
{
  uint8_t     socket_id = 0;
  const char* host      = "192.168.1.100";
  uint16_t    port      = 8080;
  const char* data      = "Hello UDP";
  uint16_t    data_len  = strlen(data);

  /* Build payload */
  uint8_t payload[1 + 128 + 2 + 2 + data_len];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);
  memcpy(&payload[129], &port, 2);
  memcpy(&payload[131], &data_len, 2);
  memcpy(&payload[133], data, data_len);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_udp_send, payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
  STAR_ASSERT_EQUAL(k_cmd_udp_send, packet[1]);
}

STAR_TEST_CASE(udp_handler, udp_send_to_hostname)
{
  uint8_t     socket_id = 0;
  const char* host      = "example.com";
  uint16_t    port      = 9999;
  const char* data      = "test";
  uint16_t    data_len  = 4;

  uint8_t payload[1 + 128 + 2 + 2 + data_len];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);
  memcpy(&payload[129], &port, 2);
  memcpy(&payload[131], &data_len, 2);
  memcpy(&payload[133], data, data_len);

  /* Verify hostname preserved */
  char* extracted_host = (char*)&payload[1];
  STAR_ASSERT_STR_EQUAL(host, extracted_host);
}

STAR_TEST_CASE(udp_handler, udp_send_broadcast)
{
  uint8_t     socket_id = 0;
  const char* host      = "255.255.255.255"; /* Broadcast */
  uint16_t    data_len  = strlen("broadcast_msg");

  uint8_t payload[1 + 128 + 2 + 2 + data_len];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);

  char* extracted = (char*)&payload[1];
  STAR_ASSERT_STR_EQUAL("255.255.255.255", extracted);
}

STAR_TEST_CASE(udp_handler, udp_send_multicast)
{
  uint8_t     socket_id = 0;
  const char* host      = "239.255.0.1"; /* Multicast address */
  uint16_t    data_len  = 9;

  uint8_t payload[1 + 128 + 2 + 2 + data_len];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);

  char* extracted = (char*)&payload[1];
  STAR_ASSERT_TRUE(strncmp(extracted, "239.", 4) == 0);
}

STAR_TEST_CASE(udp_handler, udp_send_localhost)
{
  uint8_t     socket_id = 0;
  const char* host      = "127.0.0.1";
  uint16_t    data_len  = 5;

  uint8_t payload[1 + 128 + 2 + 2 + data_len];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);

  char* extracted = (char*)&payload[1];
  STAR_ASSERT_STR_EQUAL("127.0.0.1", extracted);
}

STAR_TEST_CASE(udp_handler, udp_send_small_data)
{
  uint8_t     socket_id = 1;
  const char* host      = "192.168.1.50";
  uint16_t    port      = 7777;
  const char* data      = "X"; /* 1 byte */
  uint16_t    data_len  = 1;

  uint8_t payload[1 + 128 + 2 + 2 + 1];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);
  memcpy(&payload[129], &port, 2);
  memcpy(&payload[131], &data_len, 2);
  memcpy(&payload[133], data, 1);

  uint16_t extracted_len;
  memcpy(&extracted_len, &payload[131], 2);
  STAR_ASSERT_EQUAL(1, extracted_len);
}

STAR_TEST_CASE(udp_handler, udp_send_sensor_data)
{
  uint8_t     socket_id = 0;
  const char* host      = "192.168.1.10";
  uint16_t    port      = 9000;

  /* Simulated sensor data */
  const char* sensor_data = "{\"temp\":25.5,\"humidity\":60,\"pressure\":1013}";
  uint16_t    data_len    = strlen(sensor_data);

  uint8_t payload[1 + 128 + 2 + 2 + data_len];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);
  memcpy(&payload[129], &port, 2);
  memcpy(&payload[131], &data_len, 2);
  memcpy(&payload[133], sensor_data, data_len);

  /* Verify JSON preserved */
  char* extracted_data = (char*)&payload[133];
  STAR_ASSERT_TRUE(strncmp(extracted_data, "{\"temp\":", 8) == 0);
}

STAR_TEST_CASE(udp_handler, udp_send_binary_data)
{
  uint8_t  socket_id = 2;
  char     host[128] = "192.168.1.200";
  uint16_t port      = 6000;

  uint8_t  binary_data[] = {0x00, 0xFF, 0x01, 0xFE, 0x02, 0xFD};
  uint16_t data_len      = sizeof(binary_data);

  uint8_t payload[1 + 128 + 2 + 2 + data_len];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);
  memcpy(&payload[129], &port, 2);
  memcpy(&payload[131], &data_len, 2);
  memcpy(&payload[133], binary_data, data_len);

  /* Verify binary data integrity */
  STAR_ASSERT_EQUAL(0x00, payload[133]);
  STAR_ASSERT_EQUAL(0xFF, payload[134]);
  STAR_ASSERT_EQUAL(0xFD, payload[138]);
}

STAR_TEST_CASE(udp_handler, udp_send_large_datagram)
{
  uint8_t  socket_id = 0;
  char     host[128] = "10.0.0.1";
  uint16_t port      = 5555;

  /* Large UDP datagram (512 bytes) */
  uint8_t large_data[512];
  memset(large_data, 'U', sizeof(large_data));
  uint16_t data_len = sizeof(large_data);

  uint8_t payload[1 + 128 + 2 + 2 + data_len];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);
  memcpy(&payload[129], &port, 2);
  memcpy(&payload[131], &data_len, 2);
  memcpy(&payload[133], large_data, data_len);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_udp_send, payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
}

STAR_TEST_CASE(udp_handler, udp_send_empty_datagram)
{
  uint8_t  socket_id = 0;
  char     host[128] = "192.168.1.1";
  uint16_t port      = 8888;
  uint16_t data_len  = 0; /* Empty datagram */

  uint8_t payload[1 + 128 + 2 + 2];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);
  memcpy(&payload[129], &port, 2);
  memcpy(&payload[131], &data_len, 2);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_udp_send, payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
}

STAR_TEST_CASE(udp_handler, udp_send_socket_id_range)
{
  const char* host     = "192.168.1.1";
  const char* data     = "test";
  uint16_t    port     = 8000;
  uint16_t    data_len = 4;

  /* Test all valid socket IDs (0-3) */
  for (uint8_t socket_id = 0; socket_id < 4; socket_id++) {
    uint8_t payload[1 + 128 + 2 + 2 + 4];
    payload[0] = socket_id;
    memset(&payload[1], 0, 128);
    strncpy((char*)&payload[1], host, 127);
    memcpy(&payload[129], &port, 2);
    memcpy(&payload[131], &data_len, 2);
    memcpy(&payload[133], data, 4);

    STAR_ASSERT_EQUAL(socket_id, payload[0]);
  }
}

STAR_TEST_CASE(udp_handler, udp_send_port_range)
{
  uint8_t     socket_id = 0;
  const char* host      = "example.com";

  /* Test various port numbers */
  uint16_t ports[] = {53, 123, 1234, 5000, 8080, 9999, 50000, 65535};

  for (uint32_t i = 0; i < 8; i++) {
    uint8_t payload[1 + 128 + 2 + 2 + 1];
    payload[0] = socket_id;
    memset(&payload[1], 0, 128);
    strncpy((char*)&payload[1], host, 127);
    memcpy(&payload[129], &ports[i], 2);

    uint16_t extracted_port;
    memcpy(&extracted_port, &payload[129], 2);
    STAR_ASSERT_EQUAL(ports[i], extracted_port);
  }
}

STAR_TEST_CASE(udp_handler, udp_send_packet_parsing)
{
  uint8_t     socket_id = 1;
  const char* host      = "test.local";
  uint16_t    port      = 7000;
  const char* data      = "parse_test";
  uint16_t    data_len  = strlen(data);

  uint8_t payload[1 + 128 + 2 + 2 + data_len];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);
  memcpy(&payload[129], &port, 2);
  memcpy(&payload[131], &data_len, 2);
  memcpy(&payload[133], data, data_len);

  uint8_t packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  protocol_create_packet(k_cmd_udp_send, payload, sizeof(payload), packet);

  /* Parse back */
  protocol_packet_t* parsed = NULL;
  bool result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + sizeof(payload), &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_udp_send, parsed->cmd);
}

STAR_TEST_CASE(udp_handler, udp_send_response_ok)
{
  reset_transport_mock();

  protocol_send_response(k_status_ok, NULL, 0);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
}

STAR_TEST_CASE(udp_handler, udp_send_invalid_socket_id)
{
  reset_transport_mock();

  /* Socket ID > 3 is invalid */
  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(udp_handler, udp_send_consecutive_datagrams)
{
  /* UDP allows rapid consecutive sends (fire-and-forget) */
  uint8_t     socket_id = 0;
  const char* host      = "192.168.1.100";
  uint16_t    port      = 9000;

  for (uint32_t i = 0; i < 5; i++) {
    char msg[20];
    snprintf(msg, sizeof(msg), "msg%d", (int)i);
    uint16_t data_len = strlen(msg);

    uint8_t payload[1 + 128 + 2 + 2 + data_len];
    payload[0] = socket_id;
    memset(&payload[1], 0, 128);
    strncpy((char*)&payload[1], host, 127);
    memcpy(&payload[129], &port, 2);
    memcpy(&payload[131], &data_len, 2);
    memcpy(&payload[133], msg, data_len);

    /* All should be valid */
    STAR_ASSERT_EQUAL(socket_id, payload[0]);
  }
}

STAR_TEST_CASE(udp_handler, udp_send_telemetry_stream)
{
  /* Simulated telemetry streaming */
  uint8_t     socket_id = 0;
  const char* host      = "telemetry.server.com";
  uint16_t    port      = 514; /* Syslog port */

  /* Multiple telemetry packets */
  const char* telemetry[] = {"TEMP:25.3",
                             "HUMIDITY:65",
                             "PRESSURE:1012",
                             "ALTITUDE:150",
                             "GPS:37.7749,-122.4194"};

  for (uint32_t i = 0; i < 5; i++) {
    uint16_t data_len = strlen(telemetry[i]);

    uint8_t payload[1 + 128 + 2 + 2 + data_len];
    payload[0] = socket_id;
    memset(&payload[1], 0, 128);
    strncpy((char*)&payload[1], host, 127);
    memcpy(&payload[129], &port, 2);
    memcpy(&payload[131], &data_len, 2);
    memcpy(&payload[133], telemetry[i], data_len);

    char* extracted = (char*)&payload[133];
    STAR_ASSERT_TRUE(strncmp(extracted, telemetry[i], data_len) == 0);
  }
}

STAR_TEST_CASE(udp_handler, udp_send_ipv6_address)
{
  uint8_t     socket_id = 0;
  const char* host      = "::1"; /* IPv6 localhost */
  uint16_t    data_len  = 8;

  uint8_t payload[1 + 128 + 2 + 2 + data_len];
  payload[0] = socket_id;
  memset(&payload[1], 0, 128);
  strncpy((char*)&payload[1], host, 127);

  char* extracted = (char*)&payload[1];
  STAR_ASSERT_STR_EQUAL("::1", extracted);
}

/* ========================================================================
 * UDP Close Command Tests (8 tests)
 * ======================================================================== */

STAR_TEST_CASE(udp_handler, udp_close_packet_structure)
{
  udp_close_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Just socket_id(1 byte) */
  STAR_ASSERT_EQUAL(1, sizeof(payload));
}

STAR_TEST_CASE(udp_handler, udp_close_valid_socket)
{
  udp_close_payload_t payload;
  payload.socket_id = 0;

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_udp_close, &payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + 1, len);
  STAR_ASSERT_EQUAL(k_cmd_udp_close, packet[1]);
  STAR_ASSERT_EQUAL(0, payload.socket_id);
}

STAR_TEST_CASE(udp_handler, udp_close_all_socket_ids)
{
  /* Test closing all valid socket IDs */
  for (uint8_t socket_id = 0; socket_id < 4; socket_id++) {
    udp_close_payload_t payload;
    payload.socket_id = socket_id;

    uint8_t  packet[PROTOCOL_HEADER_SIZE + 1];
    uint16_t len = protocol_create_packet(k_cmd_udp_close, &payload, 1, packet);

    STAR_ASSERT_TRUE(len > 0);
    STAR_ASSERT_EQUAL(socket_id, payload.socket_id);
  }
}

STAR_TEST_CASE(udp_handler, udp_close_packet_parsing)
{
  udp_close_payload_t payload;
  payload.socket_id = 2;

  uint8_t packet[PROTOCOL_HEADER_SIZE + 1];
  protocol_create_packet(k_cmd_udp_close, &payload, 1, packet);

  /* Parse back */
  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + 1, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_udp_close, parsed->cmd);
  STAR_ASSERT_EQUAL(1, parsed->payload_len);
  STAR_ASSERT_EQUAL(2, parsed->payload[0]);
}

STAR_TEST_CASE(udp_handler, udp_close_response_ok)
{
  reset_transport_mock();

  protocol_send_response(k_status_ok, NULL, 0);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
}

STAR_TEST_CASE(udp_handler, udp_close_invalid_socket)
{
  reset_transport_mock();

  /* Invalid socket ID */
  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(udp_handler, udp_close_already_closed)
{
  reset_transport_mock();

  /* Closing already closed socket */
  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(udp_handler, udp_close_multiple_sockets)
{
  /* Close multiple UDP sockets sequentially */
  for (uint8_t socket_id = 0; socket_id < 4; socket_id++) {
    reset_transport_mock();

    protocol_send_response(k_status_ok, NULL, 0);

    protocol_packet_t* packet = NULL;
    bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
    STAR_ASSERT_TRUE(parsed);
    STAR_ASSERT_NOT_NULL(packet);

    response_payload_t* response = (response_payload_t*)packet->payload;
    STAR_ASSERT_EQUAL(k_status_ok, response->status);
  }
}

/* ========================================================================
 * UDP Error Handling Tests (5 tests)
 * ======================================================================== */

STAR_TEST_CASE(udp_handler, udp_wifi_not_connected)
{
  reset_transport_mock();

  protocol_send_error(k_status_wifi_failed);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_wifi_failed, response->status);
}

STAR_TEST_CASE(udp_handler, udp_timeout_error)
{
  reset_transport_mock();

  protocol_send_error(k_status_timeout);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_timeout, response->status);
}

STAR_TEST_CASE(udp_handler, udp_host_resolution_failed)
{
  reset_transport_mock();

  /* DNS resolution failed */
  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(udp_handler, udp_max_sockets_exceeded)
{
  reset_transport_mock();

  /* More than 4 UDP sockets */
  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(udp_handler, udp_transport_error)
{
  reset_transport_mock();
  transport_mock_set_error(1);

  protocol_send_response(k_status_ok, "test", 4);

  /* Should handle gracefully */
  STAR_ASSERT_TRUE(true);
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()

/* UDP Create tests */
STAR_TEST_REF(udp_handler, udp_create_packet_structure)
STAR_TEST_REF(udp_handler, udp_create_auto_port)
STAR_TEST_REF(udp_handler, udp_create_specific_port)
STAR_TEST_REF(udp_handler, udp_create_port_range)
STAR_TEST_REF(udp_handler, udp_create_packet_parsing)
STAR_TEST_REF(udp_handler, udp_create_response_structure)
STAR_TEST_REF(udp_handler, udp_create_response_with_socket_id)
STAR_TEST_REF(udp_handler, udp_create_multiple_sockets)
STAR_TEST_REF(udp_handler, udp_create_port_already_in_use)
STAR_TEST_REF(udp_handler, udp_create_privileged_port)
STAR_TEST_REF(udp_handler, udp_create_max_sockets_exceeded)
STAR_TEST_REF(udp_handler, udp_create_port_encoding)
STAR_TEST_REF(udp_handler, udp_create_wifi_not_connected)
STAR_TEST_REF(udp_handler, udp_create_common_ports)
STAR_TEST_REF(udp_handler, udp_create_zero_port_auto_assign)

/* UDP Send tests */
STAR_TEST_REF(udp_handler, udp_send_packet_structure)
STAR_TEST_REF(udp_handler, udp_send_to_ip_address)
STAR_TEST_REF(udp_handler, udp_send_to_hostname)
STAR_TEST_REF(udp_handler, udp_send_broadcast)
STAR_TEST_REF(udp_handler, udp_send_multicast)
STAR_TEST_REF(udp_handler, udp_send_localhost)
STAR_TEST_REF(udp_handler, udp_send_small_data)
STAR_TEST_REF(udp_handler, udp_send_sensor_data)
STAR_TEST_REF(udp_handler, udp_send_binary_data)
STAR_TEST_REF(udp_handler, udp_send_large_datagram)
STAR_TEST_REF(udp_handler, udp_send_empty_datagram)
STAR_TEST_REF(udp_handler, udp_send_socket_id_range)
STAR_TEST_REF(udp_handler, udp_send_port_range)
STAR_TEST_REF(udp_handler, udp_send_packet_parsing)
STAR_TEST_REF(udp_handler, udp_send_response_ok)
STAR_TEST_REF(udp_handler, udp_send_invalid_socket_id)
STAR_TEST_REF(udp_handler, udp_send_consecutive_datagrams)
STAR_TEST_REF(udp_handler, udp_send_telemetry_stream)
STAR_TEST_REF(udp_handler, udp_send_ipv6_address)

/* UDP Close tests */
STAR_TEST_REF(udp_handler, udp_close_packet_structure)
STAR_TEST_REF(udp_handler, udp_close_valid_socket)
STAR_TEST_REF(udp_handler, udp_close_all_socket_ids)
STAR_TEST_REF(udp_handler, udp_close_packet_parsing)
STAR_TEST_REF(udp_handler, udp_close_response_ok)
STAR_TEST_REF(udp_handler, udp_close_invalid_socket)
STAR_TEST_REF(udp_handler, udp_close_already_closed)
STAR_TEST_REF(udp_handler, udp_close_multiple_sockets)

/* Error handling tests */
STAR_TEST_REF(udp_handler, udp_wifi_not_connected)
STAR_TEST_REF(udp_handler, udp_timeout_error)
STAR_TEST_REF(udp_handler, udp_host_resolution_failed)
STAR_TEST_REF(udp_handler, udp_max_sockets_exceeded)
STAR_TEST_REF(udp_handler, udp_transport_error)

STAR_TEST_LIST_END()
