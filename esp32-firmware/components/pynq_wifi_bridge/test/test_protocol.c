/* esp32-firmware/components/pynq_wifi_bridge/test/test_protocol.c */

/**
 * @file test_protocol.c
 * @brief STAR_TEST comprehensive tests for pynq_wifi_bridge protocol
 */

#include <string.h>

#include "pynq_wifi_protocol.h"
#include "star_test.h"

/* Mock transport for testing send functions */
static uint8_t  g_transport_buffer[2048];
static uint16_t g_transport_len   = 0;
static uint8_t  g_transport_error = 0;

/* Weak symbol to override real transport_send in test environment */
__attribute__((weak)) int32_t transport_send(const uint8_t* data, uint16_t len)
{
  if (g_transport_error) {
    return -1;
  }
  if (len > sizeof(g_transport_buffer)) {
    return -1;
  }
  memcpy(g_transport_buffer, data, len);
  g_transport_len = len;
  return (int32_t)len;
}

static void reset_transport_mock(void)
{
  memset(g_transport_buffer, 0, sizeof(g_transport_buffer));
  g_transport_len   = 0;
  g_transport_error = 0;
}

/* ========================================================================
 * Packet Creation Tests (12 tests)
 * ======================================================================== */

STAR_TEST_CASE(protocol, create_packet_no_payload)
{
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_ping, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(PROTOCOL_START_MARKER, packet[0]);
  STAR_ASSERT_EQUAL(k_cmd_ping, packet[1]);
  STAR_ASSERT_EQUAL(0, packet[2]); /* Payload len low byte */
  STAR_ASSERT_EQUAL(0, packet[3]); /* Payload len high byte */
}

STAR_TEST_CASE(protocol, create_packet_with_small_payload)
{
  uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
  uint8_t packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];

  uint16_t len = protocol_create_packet(k_cmd_wifi_connect, payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
  STAR_ASSERT_EQUAL(PROTOCOL_START_MARKER, packet[0]);
  STAR_ASSERT_EQUAL(k_cmd_wifi_connect, packet[1]);
  STAR_ASSERT_EQUAL(4, packet[2]); /* Payload len = 4 */
  STAR_ASSERT_EQUAL(0, packet[3]);

  /* Verify payload */
  STAR_ASSERT_EQUAL(0x01, packet[4]);
  STAR_ASSERT_EQUAL(0x02, packet[5]);
  STAR_ASSERT_EQUAL(0x03, packet[6]);
  STAR_ASSERT_EQUAL(0x04, packet[7]);
}

STAR_TEST_CASE(protocol, create_packet_with_large_payload)
{
  uint8_t payload[256];
  for (int i = 0; i < 256; i++) {
    payload[i] = (uint8_t)i;
  }

  uint8_t  packet[PROTOCOL_HEADER_SIZE + 256];
  uint16_t len = protocol_create_packet(k_cmd_http_post, payload, 256, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + 256, len);
  STAR_ASSERT_EQUAL(PROTOCOL_START_MARKER, packet[0]);
  STAR_ASSERT_EQUAL(k_cmd_http_post, packet[1]);
  STAR_ASSERT_EQUAL(0, packet[2]); /* 256 & 0xFF = 0 */
  STAR_ASSERT_EQUAL(1, packet[3]); /* 256 >> 8 = 1 */

  /* Verify some payload bytes */
  STAR_ASSERT_EQUAL(0, packet[4]);
  STAR_ASSERT_EQUAL(128, packet[132]);
  STAR_ASSERT_EQUAL(255, packet[259]);
}

STAR_TEST_CASE(protocol, create_packet_max_payload)
{
  uint8_t payload[PROTOCOL_MAX_PAYLOAD_SIZE];
  memset(payload, 0xAA, PROTOCOL_MAX_PAYLOAD_SIZE);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_tcp_send, payload, PROTOCOL_MAX_PAYLOAD_SIZE, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE, len);
  STAR_ASSERT_EQUAL(PROTOCOL_START_MARKER, packet[0]);

  /* Verify payload length encoding (1024 = 0x0400) */
  STAR_ASSERT_EQUAL(0x00, packet[2]); /* Low byte */
  STAR_ASSERT_EQUAL(0x04, packet[3]); /* High byte */

  /* Verify payload content */
  STAR_ASSERT_EQUAL(0xAA, packet[4]);
  STAR_ASSERT_EQUAL(0xAA, packet[PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE - 1]);
}

STAR_TEST_CASE(protocol, create_packet_oversized_payload)
{
  uint8_t payload[PROTOCOL_MAX_PAYLOAD_SIZE + 100];
  memset(payload, 0, sizeof(payload));
  uint8_t packet[2048];

  uint16_t len =
    protocol_create_packet(k_cmd_tcp_send, payload, PROTOCOL_MAX_PAYLOAD_SIZE + 100, packet);

  /* Should fail and return 0 */
  STAR_ASSERT_EQUAL(0, len);
}

STAR_TEST_CASE(protocol, create_packet_null_output)
{
  uint8_t  payload[] = {0x01, 0x02};
  uint16_t len       = protocol_create_packet(k_cmd_ping, payload, 2, NULL);

  /* Should fail and return 0 */
  STAR_ASSERT_EQUAL(0, len);
}

STAR_TEST_CASE(protocol, create_packet_different_commands)
{
  uint8_t packet1[PROTOCOL_HEADER_SIZE];
  uint8_t packet2[PROTOCOL_HEADER_SIZE];
  uint8_t packet3[PROTOCOL_HEADER_SIZE];

  protocol_create_packet(k_cmd_wifi_scan, NULL, 0, packet1);
  protocol_create_packet(k_cmd_reset, NULL, 0, packet2);
  protocol_create_packet(k_cmd_get_version, NULL, 0, packet3);

  STAR_ASSERT_EQUAL(k_cmd_wifi_scan, packet1[1]);
  STAR_ASSERT_EQUAL(k_cmd_reset, packet2[1]);
  STAR_ASSERT_EQUAL(k_cmd_get_version, packet3[1]);
}

STAR_TEST_CASE(protocol, create_packet_wifi_connect_payload)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.ssid, "TestNetwork", sizeof(payload.ssid) - 1);
  strncpy(payload.password, "TestPassword123", sizeof(payload.password) - 1);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_wifi_connect, &payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);

  /* Verify payload */
  wifi_connect_payload_t* recv_payload = (wifi_connect_payload_t*)&packet[PROTOCOL_HEADER_SIZE];
  STAR_ASSERT_STR_EQUAL("TestNetwork", recv_payload->ssid);
  STAR_ASSERT_STR_EQUAL("TestPassword123", recv_payload->password);
}

STAR_TEST_CASE(protocol, create_packet_http_get_payload)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_url = "http://example.com/api/data";
  strncpy(payload.url, test_url, sizeof(payload.url) - 1);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_http_get, &payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);

  http_get_payload_t* recv_payload = (http_get_payload_t*)&packet[PROTOCOL_HEADER_SIZE];
  /* Verify URL matches */
  STAR_ASSERT_EQUAL(0, strncmp(test_url, recv_payload->url, strlen(test_url)));
}

STAR_TEST_CASE(protocol, create_packet_payload_len_encoding)
{
  /* Test various payload lengths to verify encoding */
  uint8_t payload[512];
  memset(payload, 0, sizeof(payload));
  uint8_t packet[PROTOCOL_HEADER_SIZE + 512];

  /* Length = 255 (0x00FF) */
  protocol_create_packet(k_cmd_tcp_send, payload, 255, packet);
  STAR_ASSERT_EQUAL(0xFF, packet[2]);
  STAR_ASSERT_EQUAL(0x00, packet[3]);

  /* Length = 256 (0x0100) */
  protocol_create_packet(k_cmd_tcp_send, payload, 256, packet);
  STAR_ASSERT_EQUAL(0x00, packet[2]);
  STAR_ASSERT_EQUAL(0x01, packet[3]);

  /* Length = 512 (0x0200) */
  protocol_create_packet(k_cmd_tcp_send, payload, 512, packet);
  STAR_ASSERT_EQUAL(0x00, packet[2]);
  STAR_ASSERT_EQUAL(0x02, packet[3]);
}

STAR_TEST_CASE(protocol, create_packet_zero_payload_with_pointer)
{
  uint8_t payload[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  uint8_t packet[PROTOCOL_HEADER_SIZE];

  /* Payload pointer present but length is 0 */
  uint16_t len = protocol_create_packet(k_cmd_ping, payload, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(0, packet[2]);
  STAR_ASSERT_EQUAL(0, packet[3]);
}

STAR_TEST_CASE(protocol, create_packet_response_command)
{
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_response, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(k_cmd_response, packet[1]);
}

/* ========================================================================
 * Packet Parsing Tests (13 tests)
 * ======================================================================== */

STAR_TEST_CASE(protocol, parse_valid_packet_no_payload)
{
  uint8_t            data[] = {PROTOCOL_START_MARKER, k_cmd_ping, 0x00, 0x00};
  protocol_packet_t* packet = NULL;

  bool result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_NOT_NULL(packet);
  STAR_ASSERT_EQUAL(PROTOCOL_START_MARKER, packet->start);
  STAR_ASSERT_EQUAL(k_cmd_ping, packet->cmd);
  STAR_ASSERT_EQUAL(0, packet->payload_len);
}

STAR_TEST_CASE(protocol, parse_valid_packet_with_payload)
{
  uint8_t data[] = {PROTOCOL_START_MARKER, k_cmd_wifi_connect, 0x04, 0x00, 0xAA, 0xBB, 0xCC, 0xDD};
  protocol_packet_t* packet = NULL;

  bool result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_NOT_NULL(packet);
  STAR_ASSERT_EQUAL(k_cmd_wifi_connect, packet->cmd);
  STAR_ASSERT_EQUAL(4, packet->payload_len);
  STAR_ASSERT_EQUAL(0xAA, packet->payload[0]);
  STAR_ASSERT_EQUAL(0xDD, packet->payload[3]);
}

STAR_TEST_CASE(protocol, parse_packet_invalid_start_marker)
{
  uint8_t            data[] = {0xFF, k_cmd_ping, 0x00, 0x00};
  protocol_packet_t* packet = NULL;

  bool result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(protocol, parse_packet_too_short)
{
  uint8_t            data[] = {PROTOCOL_START_MARKER, k_cmd_ping, 0x00};
  protocol_packet_t* packet = NULL;

  bool result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(protocol, parse_packet_null_data)
{
  protocol_packet_t* packet = NULL;

  bool result = protocol_parse_packet(NULL, 100, &packet);

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(protocol, parse_packet_null_output)
{
  uint8_t data[] = {PROTOCOL_START_MARKER, k_cmd_ping, 0x00, 0x00};

  bool result = protocol_parse_packet(data, sizeof(data), NULL);

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(protocol, parse_packet_incomplete)
{
  /* Header says payload length is 10, but only 4 bytes provided */
  uint8_t data[] = {PROTOCOL_START_MARKER, k_cmd_tcp_send, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04};
  protocol_packet_t* packet = NULL;

  bool result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(protocol, parse_packet_oversized_payload_len)
{
  /* Payload length exceeds maximum */
  uint8_t data[] = {PROTOCOL_START_MARKER, k_cmd_tcp_send, 0x01, 0x05}; /* 0x0501 = 1281 bytes */
  protocol_packet_t* packet = NULL;

  bool result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_FALSE(result);
}

STAR_TEST_CASE(protocol, parse_packet_max_payload)
{
  /* Create packet with max payload */
  uint8_t data[PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE];
  data[0] = PROTOCOL_START_MARKER;
  data[1] = k_cmd_tcp_send;
  data[2] = 0x00; /* 1024 & 0xFF */
  data[3] = 0x04; /* 1024 >> 8 */

  for (int i = 0; i < PROTOCOL_MAX_PAYLOAD_SIZE; i++) {
    data[PROTOCOL_HEADER_SIZE + i] = (uint8_t)(i & 0xFF);
  }

  protocol_packet_t* packet = NULL;
  bool               result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(PROTOCOL_MAX_PAYLOAD_SIZE, packet->payload_len);
  STAR_ASSERT_EQUAL(0, packet->payload[0]);
  STAR_ASSERT_EQUAL(255, packet->payload[255]);
}

STAR_TEST_CASE(protocol, parse_packet_extra_data)
{
  /* Packet has extra data beyond payload length - should still parse */
  uint8_t data[] = {PROTOCOL_START_MARKER, k_cmd_ping, 0x02, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  protocol_packet_t* packet = NULL;

  bool result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(2, packet->payload_len);
  STAR_ASSERT_EQUAL(0xAA, packet->payload[0]);
  STAR_ASSERT_EQUAL(0xBB, packet->payload[1]);
}

STAR_TEST_CASE(protocol, parse_packet_large_payload_encoding)
{
  /* Test 256-byte payload (0x0100) */
  uint8_t data[PROTOCOL_HEADER_SIZE + 256];
  data[0] = PROTOCOL_START_MARKER;
  data[1] = k_cmd_http_post;
  data[2] = 0x00; /* Low byte */
  data[3] = 0x01; /* High byte */
  memset(&data[4], 0xFF, 256);

  protocol_packet_t* packet = NULL;
  bool               result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(256, packet->payload_len);
}

STAR_TEST_CASE(protocol, parse_wifi_connect_packet)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.ssid, "MyNetwork", sizeof(payload.ssid) - 1);
  strncpy(payload.password, "SecurePass", sizeof(payload.password) - 1);

  uint8_t data[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  data[0] = PROTOCOL_START_MARKER;
  data[1] = k_cmd_wifi_connect;
  data[2] = sizeof(payload) & 0xFF;
  data[3] = (sizeof(payload) >> 8) & 0xFF;
  memcpy(&data[4], &payload, sizeof(payload));

  protocol_packet_t* packet = NULL;
  bool               result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_wifi_connect, packet->cmd);

  wifi_connect_payload_t* parsed = (wifi_connect_payload_t*)packet->payload;
  STAR_ASSERT_STR_EQUAL("MyNetwork", parsed->ssid);
  STAR_ASSERT_STR_EQUAL("SecurePass", parsed->password);
}

STAR_TEST_CASE(protocol, parse_response_packet)
{
  response_payload_t response;
  response.status         = k_status_ok;
  response.data_len       = 5;
  uint8_t response_data[] = "Hello";

  uint8_t data[PROTOCOL_HEADER_SIZE + sizeof(response) + 5];
  data[0] = PROTOCOL_START_MARKER;
  data[1] = k_cmd_response;
  data[2] = (sizeof(response) + 5) & 0xFF;
  data[3] = ((sizeof(response) + 5) >> 8) & 0xFF;
  memcpy(&data[4], &response, sizeof(response));
  memcpy(&data[4 + sizeof(response)], response_data, 5);

  protocol_packet_t* packet = NULL;
  bool               result = protocol_parse_packet(data, sizeof(data), &packet);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_response, packet->cmd);

  response_payload_t* parsed = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, parsed->status);
  STAR_ASSERT_EQUAL(5, parsed->data_len);
}

/* ========================================================================
 * Protocol Send Tests (10 tests)
 * ======================================================================== */

STAR_TEST_CASE(protocol, send_response_no_data)
{
  reset_transport_mock();

  protocol_send_response(k_status_ok, NULL, 0);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  /* Parse sent packet */
  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_EQUAL(k_cmd_response, packet->cmd);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(0, response->data_len);
}

STAR_TEST_CASE(protocol, send_response_with_data)
{
  reset_transport_mock();

  const char* test_data = "Test Response";
  protocol_send_response(k_status_ok, test_data, strlen(test_data));

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_EQUAL(k_cmd_response, packet->cmd);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(strlen(test_data), response->data_len);

  /* Verify data follows response header */
  const char* data_ptr = (const char*)(packet->payload + sizeof(response_payload_t));
  STAR_ASSERT_EQUAL('T', data_ptr[0]);
  STAR_ASSERT_EQUAL('t', data_ptr[3]);
}

STAR_TEST_CASE(protocol, send_error_response)
{
  reset_transport_mock();

  protocol_send_error(k_status_error);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_EQUAL(k_cmd_response, packet->cmd);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
  STAR_ASSERT_EQUAL(0, response->data_len);
}

STAR_TEST_CASE(protocol, send_error_invalid_cmd)
{
  reset_transport_mock();

  protocol_send_error(k_status_invalid_cmd);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  STAR_ASSERT_TRUE(parsed);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_invalid_cmd, response->status);
}

STAR_TEST_CASE(protocol, send_error_timeout)
{
  reset_transport_mock();

  protocol_send_error(k_status_timeout);

  protocol_packet_t* packet = NULL;
  protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_timeout, response->status);
}

STAR_TEST_CASE(protocol, send_error_wifi_failed)
{
  reset_transport_mock();

  protocol_send_error(k_status_wifi_failed);

  protocol_packet_t* packet = NULL;
  protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_wifi_failed, response->status);
}

STAR_TEST_CASE(protocol, send_response_large_data)
{
  reset_transport_mock();

  uint8_t large_data[512];
  for (int i = 0; i < 512; i++) {
    large_data[i] = (uint8_t)i;
  }

  protocol_send_response(k_status_ok, large_data, 512);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  STAR_ASSERT_TRUE(parsed);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(512, response->data_len);
}

STAR_TEST_CASE(protocol, send_response_transport_error)
{
  reset_transport_mock();
  g_transport_error = 1; /* Simulate transport failure */

  protocol_send_response(k_status_ok, NULL, 0);

  /* Should handle error gracefully - no crash */
  STAR_ASSERT_TRUE(true);
}

STAR_TEST_CASE(protocol, send_multiple_responses)
{
  reset_transport_mock();

  protocol_send_response(k_status_ok, "First", 5);
  uint16_t first_len = g_transport_len;

  reset_transport_mock();
  protocol_send_response(k_status_error, "Second", 6);
  uint16_t second_len = g_transport_len;

  STAR_ASSERT_TRUE(first_len > 0);
  STAR_ASSERT_TRUE(second_len > 0);
}

STAR_TEST_CASE(protocol, send_response_different_statuses)
{
  /* Test k_status_ok */
  reset_transport_mock();
  protocol_send_response(k_status_ok, NULL, 0);
  protocol_packet_t* packet1 = NULL;
  protocol_parse_packet(g_transport_buffer, g_transport_len, &packet1);
  response_payload_t* resp1 = (response_payload_t*)packet1->payload;
  STAR_ASSERT_EQUAL(k_status_ok, resp1->status);

  /* Test k_status_http_failed */
  reset_transport_mock();
  protocol_send_response(k_status_http_failed, NULL, 0);
  protocol_packet_t* packet2 = NULL;
  protocol_parse_packet(g_transport_buffer, g_transport_len, &packet2);
  response_payload_t* resp2 = (response_payload_t*)packet2->payload;
  STAR_ASSERT_EQUAL(k_status_http_failed, resp2->status);
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()
/* Packet creation tests */
STAR_TEST_REF(protocol, create_packet_no_payload)
STAR_TEST_REF(protocol, create_packet_with_small_payload)
STAR_TEST_REF(protocol, create_packet_with_large_payload)
STAR_TEST_REF(protocol, create_packet_max_payload)
STAR_TEST_REF(protocol, create_packet_oversized_payload)
STAR_TEST_REF(protocol, create_packet_null_output)
STAR_TEST_REF(protocol, create_packet_different_commands)
STAR_TEST_REF(protocol, create_packet_wifi_connect_payload)
STAR_TEST_REF(protocol, create_packet_http_get_payload)
STAR_TEST_REF(protocol, create_packet_payload_len_encoding)
STAR_TEST_REF(protocol, create_packet_zero_payload_with_pointer)
STAR_TEST_REF(protocol, create_packet_response_command)

/* Packet parsing tests */
STAR_TEST_REF(protocol, parse_valid_packet_no_payload)
STAR_TEST_REF(protocol, parse_valid_packet_with_payload)
STAR_TEST_REF(protocol, parse_packet_invalid_start_marker)
STAR_TEST_REF(protocol, parse_packet_too_short)
STAR_TEST_REF(protocol, parse_packet_null_data)
STAR_TEST_REF(protocol, parse_packet_null_output)
STAR_TEST_REF(protocol, parse_packet_incomplete)
STAR_TEST_REF(protocol, parse_packet_oversized_payload_len)
STAR_TEST_REF(protocol, parse_packet_max_payload)
STAR_TEST_REF(protocol, parse_packet_extra_data)
STAR_TEST_REF(protocol, parse_packet_large_payload_encoding)
STAR_TEST_REF(protocol, parse_wifi_connect_packet)
STAR_TEST_REF(protocol, parse_response_packet)

/* Protocol send tests */
STAR_TEST_REF(protocol, send_response_no_data)
STAR_TEST_REF(protocol, send_response_with_data)
STAR_TEST_REF(protocol, send_error_response)
STAR_TEST_REF(protocol, send_error_invalid_cmd)
STAR_TEST_REF(protocol, send_error_timeout)
STAR_TEST_REF(protocol, send_error_wifi_failed)
STAR_TEST_REF(protocol, send_response_large_data)
STAR_TEST_REF(protocol, send_response_transport_error)
STAR_TEST_REF(protocol, send_multiple_responses)
STAR_TEST_REF(protocol, send_response_different_statuses)
STAR_TEST_LIST_END()
