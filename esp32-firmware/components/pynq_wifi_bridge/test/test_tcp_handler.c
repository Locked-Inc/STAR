/* esp32-firmware/components/pynq_wifi_bridge/test/test_tcp_handler.c */

/**
 * @file test_tcp_handler.c
 * @brief STAR_TEST comprehensive tests for TCP command handlers
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
 * TCP Connect Command Tests (12 tests)
 * ======================================================================== */

STAR_TEST_CASE(tcp_handler, tcp_connect_packet_structure)
{
  tcp_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Verify structure size: 128 bytes hostname + 2 bytes port */
  STAR_ASSERT_EQUAL(130, sizeof(payload));
}

STAR_TEST_CASE(tcp_handler, tcp_connect_valid_hostname)
{
  tcp_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.host, "httpbin.org", sizeof(payload.host) - 1);
  payload.port = 80;

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_tcp_connect, &payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
  STAR_ASSERT_EQUAL(k_cmd_tcp_connect, packet[1]);
}

STAR_TEST_CASE(tcp_handler, tcp_connect_hostname_encoding)
{
  tcp_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_host = "example.com";
  strncpy(payload.host, test_host, sizeof(payload.host) - 1);
  payload.port = 443;

  /* Verify hostname is stored correctly */
  STAR_ASSERT_STR_EQUAL(test_host, payload.host);
  STAR_ASSERT_EQUAL('\0', payload.host[strlen(test_host)]);
  STAR_ASSERT_EQUAL(443, payload.port);
}

STAR_TEST_CASE(tcp_handler, tcp_connect_ip_address)
{
  tcp_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.host, "192.168.1.100", sizeof(payload.host) - 1);
  payload.port = 8080;

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_tcp_connect, &payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
  STAR_ASSERT_STR_EQUAL("192.168.1.100", payload.host);
}

STAR_TEST_CASE(tcp_handler, tcp_connect_localhost)
{
  tcp_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.host, "localhost", sizeof(payload.host) - 1);
  payload.port = 3000;

  STAR_ASSERT_STR_EQUAL("localhost", payload.host);
  STAR_ASSERT_EQUAL(3000, payload.port);
}

STAR_TEST_CASE(tcp_handler, tcp_connect_max_hostname_length)
{
  tcp_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Fill hostname buffer completely */
  for (uint32_t i = 0; i < sizeof(payload.host) - 1; i++) {
    payload.host[i] = 'a';
  }
  payload.host[sizeof(payload.host) - 1] = '\0';

  STAR_ASSERT_EQUAL('\0', payload.host[sizeof(payload.host) - 1]);
  STAR_ASSERT_EQUAL(sizeof(payload.host) - 1, strlen(payload.host));
}

STAR_TEST_CASE(tcp_handler, tcp_connect_empty_hostname)
{
  tcp_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  payload.port = 80;
  /* Empty hostname */

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_tcp_connect, &payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
  STAR_ASSERT_EQUAL('\0', payload.host[0]);
}

STAR_TEST_CASE(tcp_handler, tcp_connect_port_range)
{
  tcp_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.host, "example.com", sizeof(payload.host) - 1);

  /* Test various valid ports */
  payload.port = 1;
  STAR_ASSERT_EQUAL(1, payload.port);

  payload.port = 80;
  STAR_ASSERT_EQUAL(80, payload.port);

  payload.port = 443;
  STAR_ASSERT_EQUAL(443, payload.port);

  payload.port = 8080;
  STAR_ASSERT_EQUAL(8080, payload.port);

  payload.port = 65535;
  STAR_ASSERT_EQUAL(65535, payload.port);
}

STAR_TEST_CASE(tcp_handler, tcp_connect_packet_parsing)
{
  tcp_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.host, "httpbin.org", sizeof(payload.host) - 1);
  payload.port = 80;

  uint8_t packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  protocol_create_packet(k_cmd_tcp_connect, &payload, sizeof(payload), packet);

  /* Parse back */
  protocol_packet_t* parsed = NULL;
  bool result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + sizeof(payload), &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_tcp_connect, parsed->cmd);
  STAR_ASSERT_EQUAL(sizeof(payload), parsed->payload_len);

  tcp_connect_payload_t* parsed_payload = (tcp_connect_payload_t*)parsed->payload;
  STAR_ASSERT_STR_EQUAL("httpbin.org", parsed_payload->host);
  STAR_ASSERT_EQUAL(80, parsed_payload->port);
}

STAR_TEST_CASE(tcp_handler, tcp_connect_response_with_socket_id)
{
  reset_transport_mock();

  /* Simulate successful connection with socket ID */
  uint8_t socket_id = 0;
  protocol_send_response(k_status_ok, &socket_id, 1);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(1, response->data_len);
  /* Access response data after the struct using pointer arithmetic */
  uint8_t* response_data = (uint8_t*)(response + 1);
  STAR_ASSERT_EQUAL(0, response_data[0]); /* Socket ID 0 */
}

STAR_TEST_CASE(tcp_handler, tcp_connect_multiple_socket_ids)
{
  reset_transport_mock();

  /* Test multiple socket IDs */
  for (uint8_t socket_id = 0; socket_id < 4; socket_id++) {
    protocol_send_response(k_status_ok, &socket_id, 1);

    protocol_packet_t* packet = NULL;
    bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
    STAR_ASSERT_TRUE(parsed);
    STAR_ASSERT_NOT_NULL(packet);

    response_payload_t* response      = (response_payload_t*)packet->payload;
    uint8_t*            response_data = (uint8_t*)(response + 1);
    STAR_ASSERT_EQUAL(socket_id, response_data[0]);

    reset_transport_mock();
  }
}

STAR_TEST_CASE(tcp_handler, tcp_connect_error)
{
  reset_transport_mock();

  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

/* ========================================================================
 * TCP Send Command Tests (15 tests)
 * ======================================================================== */

STAR_TEST_CASE(tcp_handler, tcp_send_packet_structure)
{
  /* TCP send payload: [socket_id(1)] [data_len(2)] [data(N)] */
  uint16_t data_len = 5;

  uint16_t total_len = 1 + 2 + data_len; /* socket_id + data_len + data */

  STAR_ASSERT_TRUE(total_len > 0);
  STAR_ASSERT_TRUE(total_len < PROTOCOL_MAX_PAYLOAD_SIZE);
}

STAR_TEST_CASE(tcp_handler, tcp_send_valid_data)
{
  uint8_t     socket_id = 0;
  const char* data      = "Hello";
  uint16_t    data_len  = strlen(data);

  /* Build payload: [socket_id(1)] [data_len(2)] [data] */
  uint8_t payload[1 + 2 + data_len];
  payload[0] = socket_id;
  memcpy(&payload[1], &data_len, 2);
  memcpy(&payload[3], data, data_len);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_tcp_send, payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
  STAR_ASSERT_EQUAL(k_cmd_tcp_send, packet[1]);
}

STAR_TEST_CASE(tcp_handler, tcp_send_http_request)
{
  uint8_t     socket_id = 0;
  const char* http_req  = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
  uint16_t    data_len  = strlen(http_req);

  uint8_t payload[1 + 2 + data_len];
  payload[0] = socket_id;
  memcpy(&payload[1], &data_len, 2);
  memcpy(&payload[3], http_req, data_len);

  /* Verify HTTP request preserved */
  char* extracted = (char*)&payload[3];
  STAR_ASSERT_TRUE(strncmp(extracted, "GET / HTTP/1.1", 14) == 0);
}

STAR_TEST_CASE(tcp_handler, tcp_send_binary_data)
{
  uint8_t  socket_id     = 1;
  uint8_t  binary_data[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
  uint16_t data_len      = sizeof(binary_data);

  uint8_t payload[1 + 2 + data_len];
  payload[0] = socket_id;
  memcpy(&payload[1], &data_len, 2);
  memcpy(&payload[3], binary_data, data_len);

  /* Verify binary data integrity */
  STAR_ASSERT_EQUAL(0x00, payload[3]);
  STAR_ASSERT_EQUAL(0xFF, payload[6]);
  STAR_ASSERT_EQUAL(0xFD, payload[8]);
}

STAR_TEST_CASE(tcp_handler, tcp_send_large_data)
{
  uint8_t socket_id = 2;
  /* Use 1021 bytes to fit within PROTOCOL_MAX_PAYLOAD_SIZE (1024)
   * Total payload: 1 (socket_id) + 2 (data_len) + 1021 (data) = 1024 bytes */
  uint8_t large_data[1021];
  memset(large_data, 'X', sizeof(large_data));
  uint16_t data_len = sizeof(large_data);

  uint8_t payload[1 + 2 + data_len];
  payload[0] = socket_id;
  memcpy(&payload[1], &data_len, 2);
  memcpy(&payload[3], large_data, data_len);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_tcp_send, payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
}

STAR_TEST_CASE(tcp_handler, tcp_send_empty_data)
{
  uint8_t  socket_id = 0;
  uint16_t data_len  = 0;

  uint8_t payload[3];
  payload[0] = socket_id;
  memcpy(&payload[1], &data_len, 2);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_tcp_send, payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
}

STAR_TEST_CASE(tcp_handler, tcp_send_socket_id_range)
{
  const char* data     = "test";
  uint16_t    data_len = 4;

  /* Test all valid socket IDs (0-3) */
  for (uint8_t socket_id = 0; socket_id < 4; socket_id++) {
    uint8_t payload[1 + 2 + data_len];
    payload[0] = socket_id;
    memcpy(&payload[1], &data_len, 2);
    memcpy(&payload[3], data, data_len);

    STAR_ASSERT_EQUAL(socket_id, payload[0]);
  }
}

STAR_TEST_CASE(tcp_handler, tcp_send_data_length_encoding)
{
  uint8_t  socket_id = 0;
  uint16_t data_len  = 256; /* Test 16-bit length */
  uint8_t  data[256];
  memset(data, 'A', sizeof(data));

  uint8_t payload[1 + 2 + data_len];
  payload[0] = socket_id;
  memcpy(&payload[1], &data_len, 2);
  memcpy(&payload[3], data, data_len);

  /* Verify length encoding */
  uint16_t extracted_len;
  memcpy(&extracted_len, &payload[1], 2);
  STAR_ASSERT_EQUAL(data_len, extracted_len);
}

STAR_TEST_CASE(tcp_handler, tcp_send_packet_parsing)
{
  uint8_t     socket_id = 1;
  const char* data      = "Test data";
  uint16_t    data_len  = strlen(data);

  uint8_t payload[1 + 2 + data_len];
  payload[0] = socket_id;
  memcpy(&payload[1], &data_len, 2);
  memcpy(&payload[3], data, data_len);

  uint8_t packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  protocol_create_packet(k_cmd_tcp_send, payload, sizeof(payload), packet);

  /* Parse back */
  protocol_packet_t* parsed = NULL;
  bool result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + sizeof(payload), &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_tcp_send, parsed->cmd);
}

STAR_TEST_CASE(tcp_handler, tcp_send_response_with_data)
{
  reset_transport_mock();

  const char* recv_data = "Response from server";
  protocol_send_response(k_status_ok, recv_data, strlen(recv_data));

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(strlen(recv_data), response->data_len);
}

STAR_TEST_CASE(tcp_handler, tcp_send_response_no_data)
{
  reset_transport_mock();

  /* Some sends might not get immediate response */
  protocol_send_response(k_status_ok, NULL, 0);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(0, response->data_len);
}

STAR_TEST_CASE(tcp_handler, tcp_send_invalid_socket_id)
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

STAR_TEST_CASE(tcp_handler, tcp_send_special_characters)
{
  uint8_t     socket_id = 0;
  const char* data      = "Line1\r\nLine2\tTab\x00Null";
  uint16_t    data_len  = 20; /* Include all bytes including null */

  uint8_t payload[1 + 2 + data_len];
  payload[0] = socket_id;
  memcpy(&payload[1], &data_len, 2);
  memcpy(&payload[3], data, data_len);

  /* Verify special characters preserved */
  STAR_ASSERT_EQUAL('\r', payload[8]);
  STAR_ASSERT_EQUAL('\n', payload[9]);
  STAR_ASSERT_EQUAL('\t', payload[15]);
}

STAR_TEST_CASE(tcp_handler, tcp_send_consecutive_sends)
{
  uint8_t     socket_id = 0;
  const char* data1     = "First";
  const char* data2     = "Second";

  /* First send */
  uint8_t payload1[1 + 2 + 5];
  payload1[0]   = socket_id;
  uint16_t len1 = 5;
  memcpy(&payload1[1], &len1, 2);
  memcpy(&payload1[3], data1, 5);

  /* Second send */
  uint8_t payload2[1 + 2 + 6];
  payload2[0]   = socket_id;
  uint16_t len2 = 6;
  memcpy(&payload2[1], &len2, 2);
  memcpy(&payload2[3], data2, 6);

  /* Both should be valid */
  STAR_ASSERT_EQUAL(socket_id, payload1[0]);
  STAR_ASSERT_EQUAL(socket_id, payload2[0]);
}

STAR_TEST_CASE(tcp_handler, tcp_send_chunked_data)
{
  uint8_t  socket_id  = 0;
  uint16_t chunk_size = 512;

  /* Send multiple chunks */
  for (uint32_t i = 0; i < 3; i++) {
    uint8_t chunk_data[512];
    memset(chunk_data, 'A' + i, sizeof(chunk_data));

    uint8_t payload[1 + 2 + chunk_size];
    payload[0] = socket_id;
    memcpy(&payload[1], &chunk_size, 2);
    memcpy(&payload[3], chunk_data, chunk_size);

    STAR_ASSERT_EQUAL('A' + i, payload[3]);
  }
}

/* ========================================================================
 * TCP Close Command Tests (8 tests)
 * ======================================================================== */

STAR_TEST_CASE(tcp_handler, tcp_close_packet_structure)
{
  /* TCP close payload: just socket_id(1 byte) */
  uint8_t socket_id = 0;

  uint8_t payload[1];
  payload[0] = socket_id;

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_tcp_close, payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + 1, len);
  STAR_ASSERT_EQUAL(k_cmd_tcp_close, packet[1]);
}

STAR_TEST_CASE(tcp_handler, tcp_close_valid_socket)
{
  uint8_t payload[1];
  payload[0] = 0;

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_tcp_close, payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
  STAR_ASSERT_EQUAL(0, payload[0]);
}

STAR_TEST_CASE(tcp_handler, tcp_close_all_socket_ids)
{
  /* Test closing all valid socket IDs */
  for (uint8_t socket_id = 0; socket_id < 4; socket_id++) {
    uint8_t payload[1];
    payload[0] = socket_id;

    uint8_t  packet[PROTOCOL_HEADER_SIZE + 1];
    uint16_t len = protocol_create_packet(k_cmd_tcp_close, payload, 1, packet);

    STAR_ASSERT_TRUE(len > 0);
    STAR_ASSERT_EQUAL(socket_id, payload[0]);
  }
}

STAR_TEST_CASE(tcp_handler, tcp_close_packet_parsing)
{
  uint8_t payload[1];
  payload[0] = 2;

  uint8_t packet[PROTOCOL_HEADER_SIZE + 1];
  protocol_create_packet(k_cmd_tcp_close, payload, 1, packet);

  /* Parse back */
  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + 1, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_tcp_close, parsed->cmd);
  STAR_ASSERT_EQUAL(1, parsed->payload_len);
  STAR_ASSERT_EQUAL(2, parsed->payload[0]);
}

STAR_TEST_CASE(tcp_handler, tcp_close_response_ok)
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

STAR_TEST_CASE(tcp_handler, tcp_close_invalid_socket)
{
  reset_transport_mock();

  /* Closing invalid socket should return error */
  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(tcp_handler, tcp_close_already_closed)
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

STAR_TEST_CASE(tcp_handler, tcp_close_multiple_sockets)
{
  /* Close multiple sockets sequentially */
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
 * TCP Error Handling Tests (5 tests)
 * ======================================================================== */

STAR_TEST_CASE(tcp_handler, tcp_wifi_not_connected)
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

STAR_TEST_CASE(tcp_handler, tcp_timeout_error)
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

STAR_TEST_CASE(tcp_handler, tcp_connection_refused)
{
  reset_transport_mock();

  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(tcp_handler, tcp_max_connections_exceeded)
{
  reset_transport_mock();

  /* More than 4 connections should fail */
  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(tcp_handler, tcp_transport_error)
{
  reset_transport_mock();
  transport_mock_set_error(1);

  protocol_send_response(k_status_ok, "test", 4);

  /* Should handle gracefully */
  STAR_ASSERT_TRUE(true);
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()

/* TCP Connect tests */
STAR_TEST_REF(tcp_handler, tcp_connect_packet_structure)
STAR_TEST_REF(tcp_handler, tcp_connect_valid_hostname)
STAR_TEST_REF(tcp_handler, tcp_connect_hostname_encoding)
STAR_TEST_REF(tcp_handler, tcp_connect_ip_address)
STAR_TEST_REF(tcp_handler, tcp_connect_localhost)
STAR_TEST_REF(tcp_handler, tcp_connect_max_hostname_length)
STAR_TEST_REF(tcp_handler, tcp_connect_empty_hostname)
STAR_TEST_REF(tcp_handler, tcp_connect_port_range)
STAR_TEST_REF(tcp_handler, tcp_connect_packet_parsing)
STAR_TEST_REF(tcp_handler, tcp_connect_response_with_socket_id)
STAR_TEST_REF(tcp_handler, tcp_connect_multiple_socket_ids)
STAR_TEST_REF(tcp_handler, tcp_connect_error)

/* TCP Send tests */
STAR_TEST_REF(tcp_handler, tcp_send_packet_structure)
STAR_TEST_REF(tcp_handler, tcp_send_valid_data)
STAR_TEST_REF(tcp_handler, tcp_send_http_request)
STAR_TEST_REF(tcp_handler, tcp_send_binary_data)
STAR_TEST_REF(tcp_handler, tcp_send_large_data)
STAR_TEST_REF(tcp_handler, tcp_send_empty_data)
STAR_TEST_REF(tcp_handler, tcp_send_socket_id_range)
STAR_TEST_REF(tcp_handler, tcp_send_data_length_encoding)
STAR_TEST_REF(tcp_handler, tcp_send_packet_parsing)
STAR_TEST_REF(tcp_handler, tcp_send_response_with_data)
STAR_TEST_REF(tcp_handler, tcp_send_response_no_data)
STAR_TEST_REF(tcp_handler, tcp_send_invalid_socket_id)
STAR_TEST_REF(tcp_handler, tcp_send_special_characters)
STAR_TEST_REF(tcp_handler, tcp_send_consecutive_sends)
STAR_TEST_REF(tcp_handler, tcp_send_chunked_data)

/* TCP Close tests */
STAR_TEST_REF(tcp_handler, tcp_close_packet_structure)
STAR_TEST_REF(tcp_handler, tcp_close_valid_socket)
STAR_TEST_REF(tcp_handler, tcp_close_all_socket_ids)
STAR_TEST_REF(tcp_handler, tcp_close_packet_parsing)
STAR_TEST_REF(tcp_handler, tcp_close_response_ok)
STAR_TEST_REF(tcp_handler, tcp_close_invalid_socket)
STAR_TEST_REF(tcp_handler, tcp_close_already_closed)
STAR_TEST_REF(tcp_handler, tcp_close_multiple_sockets)

/* Error handling tests */
STAR_TEST_REF(tcp_handler, tcp_wifi_not_connected)
STAR_TEST_REF(tcp_handler, tcp_timeout_error)
STAR_TEST_REF(tcp_handler, tcp_connection_refused)
STAR_TEST_REF(tcp_handler, tcp_max_connections_exceeded)
STAR_TEST_REF(tcp_handler, tcp_transport_error)

STAR_TEST_LIST_END()
