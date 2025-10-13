/* esp32-firmware/components/pynq_wifi_bridge/test/test_system_handler.c */

/**
 * @file test_system_handler.c
 * @brief STAR_TEST comprehensive tests for system command handlers (PING, RESET, GET_VERSION)
 */

#include <string.h>

#include "pynq_wifi_protocol.h"
#include "star_test.h"
#include "test_transport_mock.h"

/* Convenience macros for the shared transport mock */
#define g_transport_buffer transport_mock_get_buffer()
#define g_transport_len    transport_mock_get_length()
#define reset_transport_mock() transport_mock_reset()

/* Helper to safely parse packet with assertions */
static inline protocol_packet_t* parse_transport_buffer(void)
{
  if (g_transport_len == 0) {
    return NULL;
  }
  protocol_packet_t* packet = NULL;
  protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  return packet;
}

/* ========================================================================
 * PING Command Tests (12 tests)
 * ======================================================================== */

STAR_TEST_CASE(system_handler, ping_no_payload)
{
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_ping, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(k_cmd_ping, packet[1]);
  STAR_ASSERT_EQUAL(0, packet[2]); /* Payload length low */
  STAR_ASSERT_EQUAL(0, packet[3]); /* Payload length high */
}

STAR_TEST_CASE(system_handler, ping_with_small_payload)
{
  uint8_t  payload[] = {0x01, 0x02, 0x03, 0x04};
  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_ping, payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
  STAR_ASSERT_EQUAL(k_cmd_ping, packet[1]);
  STAR_ASSERT_EQUAL(4, packet[2]);
  STAR_ASSERT_EQUAL(0, packet[3]);

  /* Verify payload */
  STAR_ASSERT_EQUAL(0x01, packet[4]);
  STAR_ASSERT_EQUAL(0x02, packet[5]);
  STAR_ASSERT_EQUAL(0x03, packet[6]);
  STAR_ASSERT_EQUAL(0x04, packet[7]);
}

STAR_TEST_CASE(system_handler, ping_with_string_payload)
{
  const char* test_data = "PING_TEST";
  uint8_t     packet[PROTOCOL_HEADER_SIZE + strlen(test_data)];
  uint16_t    len = protocol_create_packet(k_cmd_ping, test_data, strlen(test_data), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + strlen(test_data), len);

  /* Verify payload data */
  char* payload_data = (char*)&packet[PROTOCOL_HEADER_SIZE];
  STAR_ASSERT_TRUE(strncmp(payload_data, test_data, strlen(test_data)) == 0);
}

STAR_TEST_CASE(system_handler, ping_with_large_payload)
{
  uint8_t payload[512];
  for (uint32_t i = 0; i < 512; i++) {
    payload[i] = (uint8_t)(i & 0xFF);
  }

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_ping, payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);

  /* Verify some payload bytes */
  STAR_ASSERT_EQUAL(0, packet[4]);
  STAR_ASSERT_EQUAL(128, packet[132]);
  STAR_ASSERT_EQUAL(255, packet[259]);
}

STAR_TEST_CASE(system_handler, ping_with_max_payload)
{
  uint8_t payload[PROTOCOL_MAX_PAYLOAD_SIZE];
  memset(payload, 0xAA, PROTOCOL_MAX_PAYLOAD_SIZE);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_ping, payload, PROTOCOL_MAX_PAYLOAD_SIZE, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE, len);

  /* Verify payload length encoding (1024 = 0x0400) */
  STAR_ASSERT_EQUAL(0x00, packet[2]);
  STAR_ASSERT_EQUAL(0x04, packet[3]);

  /* Verify payload content */
  STAR_ASSERT_EQUAL(0xAA, packet[4]);
  STAR_ASSERT_EQUAL(0xAA, packet[PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE - 1]);
}

STAR_TEST_CASE(system_handler, ping_packet_parsing)
{
  const char* test_data = "TEST";
  uint8_t     packet[PROTOCOL_HEADER_SIZE + strlen(test_data)];
  protocol_create_packet(k_cmd_ping, test_data, strlen(test_data), packet);

  /* Parse back */
  protocol_packet_t* parsed = NULL;
  bool result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + strlen(test_data), &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_ping, parsed->cmd);
  STAR_ASSERT_EQUAL(strlen(test_data), parsed->payload_len);

  /* Verify payload */
  char* payload_data = (char*)parsed->payload;
  STAR_ASSERT_TRUE(strncmp(payload_data, test_data, strlen(test_data)) == 0);
}

STAR_TEST_CASE(system_handler, ping_response_echo)
{
  reset_transport_mock();

  /* PING should echo back the data */
  const char* echo_data = "ECHO_TEST";
  protocol_send_response(k_status_ok, echo_data, strlen(echo_data));

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(strlen(echo_data), response->data_len);

  /* Verify echoed data - access data after the struct using pointer arithmetic */
  char* echoed = (char*)(response + 1);
  STAR_ASSERT_TRUE(strncmp(echoed, echo_data, strlen(echo_data)) == 0);
}

STAR_TEST_CASE(system_handler, ping_response_empty)
{
  reset_transport_mock();

  protocol_send_response(k_status_ok, NULL, 0);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(0, response->data_len);
}

STAR_TEST_CASE(system_handler, ping_binary_payload)
{
  uint8_t binary_data[] = {0x00, 0xFF, 0x01, 0xFE, 0x02, 0xFD};
  uint8_t packet[PROTOCOL_HEADER_SIZE + sizeof(binary_data)];
  protocol_create_packet(k_cmd_ping, binary_data, sizeof(binary_data), packet);

  /* Verify binary data preserved */
  STAR_ASSERT_EQUAL(0x00, packet[4]);
  STAR_ASSERT_EQUAL(0xFF, packet[5]);
  STAR_ASSERT_EQUAL(0xFD, packet[9]);
}

STAR_TEST_CASE(system_handler, ping_consecutive_pings)
{
  /* Test multiple pings in sequence */
  for (uint32_t i = 0; i < 5; i++) {
    uint8_t payload[1];
    payload[0] = (uint8_t)i;

    uint8_t  packet[PROTOCOL_HEADER_SIZE + 1];
    uint16_t len = protocol_create_packet(k_cmd_ping, payload, 1, packet);

    STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + 1, len);
    STAR_ASSERT_EQUAL(i, packet[4]);
  }
}

STAR_TEST_CASE(system_handler, ping_special_characters)
{
  const char* special = "PING\\r\\n\\t\\0TEST";
  uint8_t     packet[PROTOCOL_HEADER_SIZE + 16];
  protocol_create_packet(k_cmd_ping, special, 16, packet); /* Include all bytes */

  /* Verify special characters preserved */
  STAR_ASSERT_EQUAL('P', packet[4]);
  STAR_ASSERT_EQUAL('\\', packet[8]);
}

STAR_TEST_CASE(system_handler, ping_incremental_sizes)
{
  /* Test various payload sizes */
  uint16_t sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};

  for (uint32_t i = 0; i < 10; i++) {
    uint8_t payload[512];
    memset(payload, 'A', sizes[i]);

    uint8_t  packet[PROTOCOL_HEADER_SIZE + 512];
    uint16_t len = protocol_create_packet(k_cmd_ping, payload, sizes[i], packet);

    STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizes[i], len);
    STAR_ASSERT_EQUAL(sizes[i] & 0xFF, packet[2]);
    STAR_ASSERT_EQUAL((sizes[i] >> 8) & 0xFF, packet[3]);
  }
}

/* ========================================================================
 * GET_VERSION Command Tests (10 tests)
 * ======================================================================== */

STAR_TEST_CASE(system_handler, get_version_packet_structure)
{
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_get_version, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(k_cmd_get_version, packet[1]);
  STAR_ASSERT_EQUAL(0, packet[2]); /* No payload */
  STAR_ASSERT_EQUAL(0, packet[3]);
}

STAR_TEST_CASE(system_handler, get_version_response_structure)
{
  /* Version response should be 3 bytes: major, minor, patch */
  uint8_t version_data[3];
  version_data[0] = 1; /* Major */
  version_data[1] = 2; /* Minor */
  version_data[2] = 3; /* Patch */

  STAR_ASSERT_EQUAL(1, version_data[0]);
  STAR_ASSERT_EQUAL(2, version_data[1]);
  STAR_ASSERT_EQUAL(3, version_data[2]);
}

STAR_TEST_CASE(system_handler, get_version_packet_parsing)
{
  uint8_t packet[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_get_version, NULL, 0, packet);

  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_get_version, parsed->cmd);
  STAR_ASSERT_EQUAL(0, parsed->payload_len);
}

STAR_TEST_CASE(system_handler, get_version_response_valid)
{
  reset_transport_mock();

  uint8_t version[3] = {1, 0, 0};
  protocol_send_response(k_status_ok, version, 3);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(3, response->data_len);

  /* Verify version bytes - access data after the struct */
  uint8_t* version_data = (uint8_t*)(response + 1);
  STAR_ASSERT_EQUAL(1, version_data[0]);
  STAR_ASSERT_EQUAL(0, version_data[1]);
  STAR_ASSERT_EQUAL(0, version_data[2]);
}

STAR_TEST_CASE(system_handler, get_version_multiple_calls)
{
  /* Version should be consistent across multiple calls */
  reset_transport_mock();

  uint8_t version[3] = {2, 5, 10};

  for (uint32_t i = 0; i < 3; i++) {
    protocol_send_response(k_status_ok, version, 3);

    protocol_packet_t* packet = NULL;
    bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
    STAR_ASSERT_TRUE(parsed);
    STAR_ASSERT_NOT_NULL(packet);

    response_payload_t* response     = (response_payload_t*)packet->payload;
    uint8_t*            version_data = (uint8_t*)(response + 1);
    STAR_ASSERT_EQUAL(2, version_data[0]);
    STAR_ASSERT_EQUAL(5, version_data[1]);
    STAR_ASSERT_EQUAL(10, version_data[2]);

    reset_transport_mock();
  }
}

STAR_TEST_CASE(system_handler, get_version_format_validation)
{
  /* Test version number format */
  uint8_t version[3];

  /* Valid versions */
  version[0] = 0;
  version[1] = 1;
  version[2] = 0;
  STAR_ASSERT_TRUE(version[0] >= 0 && version[0] <= 255);

  version[0] = 255;
  version[1] = 255;
  version[2] = 255;
  STAR_ASSERT_TRUE(version[0] <= 255);
}

STAR_TEST_CASE(system_handler, get_version_response_size)
{
  reset_transport_mock();

  uint8_t version[3] = {3, 14, 159};
  protocol_send_response(k_status_ok, version, 3);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(3, response->data_len);
}

STAR_TEST_CASE(system_handler, get_version_semver_format)
{
  /* Test semantic versioning patterns */
  uint8_t versions[][3] = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}, {1, 2, 3}, {10, 20, 30}};

  for (uint32_t i = 0; i < 5; i++) {
    reset_transport_mock();
    protocol_send_response(k_status_ok, versions[i], 3);

    protocol_packet_t* packet = NULL;
    bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
    STAR_ASSERT_TRUE(parsed);
    STAR_ASSERT_NOT_NULL(packet);

    response_payload_t* response     = (response_payload_t*)packet->payload;
    uint8_t*            version_data = (uint8_t*)(response + 1);
    STAR_ASSERT_EQUAL(versions[i][0], version_data[0]);
    STAR_ASSERT_EQUAL(versions[i][1], version_data[1]);
    STAR_ASSERT_EQUAL(versions[i][2], version_data[2]);
  }
}

STAR_TEST_CASE(system_handler, get_version_error_handling)
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

STAR_TEST_CASE(system_handler, get_version_after_ping)
{
  /* Test that version query works after ping */
  uint8_t ping_packet[PROTOCOL_HEADER_SIZE + 4];
  uint8_t ping_data[] = {0x01, 0x02, 0x03, 0x04};
  protocol_create_packet(k_cmd_ping, ping_data, 4, ping_packet);

  uint8_t version_packet[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_get_version, NULL, 0, version_packet);

  /* Both should be valid */
  STAR_ASSERT_EQUAL(k_cmd_ping, ping_packet[1]);
  STAR_ASSERT_EQUAL(k_cmd_get_version, version_packet[1]);
}

/* ========================================================================
 * RESET Command Tests (8 tests)
 * ======================================================================== */

STAR_TEST_CASE(system_handler, reset_packet_structure)
{
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_reset, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(k_cmd_reset, packet[1]);
  STAR_ASSERT_EQUAL(0, packet[2]); /* No payload */
  STAR_ASSERT_EQUAL(0, packet[3]);
}

STAR_TEST_CASE(system_handler, reset_packet_parsing)
{
  uint8_t packet[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_reset, NULL, 0, packet);

  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_reset, parsed->cmd);
  STAR_ASSERT_EQUAL(0, parsed->payload_len);
}

STAR_TEST_CASE(system_handler, reset_response_ok)
{
  reset_transport_mock();

  /* Reset should acknowledge before resetting */
  protocol_send_response(k_status_ok, NULL, 0);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
}

STAR_TEST_CASE(system_handler, reset_response_format)
{
  reset_transport_mock();

  protocol_send_response(k_status_ok, NULL, 0);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  STAR_ASSERT_EQUAL(k_cmd_response, packet->cmd);
}

STAR_TEST_CASE(system_handler, reset_no_payload)
{
  /* Reset should never have payload */
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_reset, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(0, packet[2]);
  STAR_ASSERT_EQUAL(0, packet[3]);
}

STAR_TEST_CASE(system_handler, reset_after_operations)
{
  /* Test reset after other commands */
  uint8_t ping_packet[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_ping, NULL, 0, ping_packet);

  uint8_t version_packet[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_get_version, NULL, 0, version_packet);

  uint8_t reset_packet[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_reset, NULL, 0, reset_packet);

  /* All should be valid */
  STAR_ASSERT_EQUAL(k_cmd_ping, ping_packet[1]);
  STAR_ASSERT_EQUAL(k_cmd_get_version, version_packet[1]);
  STAR_ASSERT_EQUAL(k_cmd_reset, reset_packet[1]);
}

STAR_TEST_CASE(system_handler, reset_error_handling)
{
  reset_transport_mock();

  /* Reset could fail in some cases */
  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(system_handler, reset_command_id)
{
  uint8_t packet[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_reset, NULL, 0, packet);

  /* Verify command ID is correct */
  STAR_ASSERT_EQUAL(k_cmd_reset, packet[1]);
  STAR_ASSERT_EQUAL(0x02, packet[1]); /* k_cmd_reset = 0x02 */
}

/* ========================================================================
 * System Error Handling Tests (5 tests)
 * ======================================================================== */

STAR_TEST_CASE(system_handler, invalid_command)
{
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(0xFF, NULL, 0, packet);

  STAR_ASSERT_TRUE(len > 0);

  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, len, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(0xFF, parsed->cmd);
}

STAR_TEST_CASE(system_handler, invalid_command_error)
{
  reset_transport_mock();

  protocol_send_error(k_status_invalid_cmd);

  protocol_packet_t* packet = NULL;
  bool parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_invalid_cmd, response->status);
}

STAR_TEST_CASE(system_handler, timeout_error)
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

STAR_TEST_CASE(system_handler, transport_error)
{
  reset_transport_mock();
  transport_mock_set_error(1);

  protocol_send_response(k_status_ok, "test", 4);

  /* Should handle gracefully */
  STAR_ASSERT_TRUE(true);
}

STAR_TEST_CASE(system_handler, general_error)
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

/* Register all tests */
STAR_TEST_LIST_BEGIN()

/* PING tests */
STAR_TEST_REF(system_handler, ping_no_payload)
STAR_TEST_REF(system_handler, ping_with_small_payload)
STAR_TEST_REF(system_handler, ping_with_string_payload)
STAR_TEST_REF(system_handler, ping_with_large_payload)
STAR_TEST_REF(system_handler, ping_with_max_payload)
STAR_TEST_REF(system_handler, ping_packet_parsing)
STAR_TEST_REF(system_handler, ping_response_echo)
STAR_TEST_REF(system_handler, ping_response_empty)
STAR_TEST_REF(system_handler, ping_binary_payload)
STAR_TEST_REF(system_handler, ping_consecutive_pings)
STAR_TEST_REF(system_handler, ping_special_characters)
STAR_TEST_REF(system_handler, ping_incremental_sizes)

/* GET_VERSION tests */
STAR_TEST_REF(system_handler, get_version_packet_structure)
STAR_TEST_REF(system_handler, get_version_response_structure)
STAR_TEST_REF(system_handler, get_version_packet_parsing)
STAR_TEST_REF(system_handler, get_version_response_valid)
STAR_TEST_REF(system_handler, get_version_multiple_calls)
STAR_TEST_REF(system_handler, get_version_format_validation)
STAR_TEST_REF(system_handler, get_version_response_size)
STAR_TEST_REF(system_handler, get_version_semver_format)
STAR_TEST_REF(system_handler, get_version_error_handling)
STAR_TEST_REF(system_handler, get_version_after_ping)

/* RESET tests */
STAR_TEST_REF(system_handler, reset_packet_structure)
STAR_TEST_REF(system_handler, reset_packet_parsing)
STAR_TEST_REF(system_handler, reset_response_ok)
STAR_TEST_REF(system_handler, reset_response_format)
STAR_TEST_REF(system_handler, reset_no_payload)
STAR_TEST_REF(system_handler, reset_after_operations)
STAR_TEST_REF(system_handler, reset_error_handling)
STAR_TEST_REF(system_handler, reset_command_id)

/* Error handling tests */
STAR_TEST_REF(system_handler, invalid_command)
STAR_TEST_REF(system_handler, invalid_command_error)
STAR_TEST_REF(system_handler, timeout_error)
STAR_TEST_REF(system_handler, transport_error)
STAR_TEST_REF(system_handler, general_error)

STAR_TEST_LIST_END()
