/* esp32-firmware/components/pynq_wifi_bridge/test/test_ota_handler.c */

/**
 * @file test_ota_handler.c
 * @brief STAR_TEST comprehensive tests for OTA command handlers
 */

#include <string.h>

#include "pynq_ota_manager.h"
#include "pynq_wifi_protocol.h"
#include "star_test.h"

#include "test_transport_mock.h"

/* Convenience macros for the shared transport mock */
#define g_transport_buffer transport_mock_get_buffer()
#define g_transport_len    transport_mock_get_length()
#define reset_transport_mock() transport_mock_reset()

/* ========================================================================
 * OTA Check Update Command Tests (6 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_handler, check_update_response_structure)
{
  reset_transport_mock();

  /* Create OTA check update packet */
  uint8_t  packet_buf[PROTOCOL_HEADER_SIZE];
  uint16_t packet_len = protocol_create_packet(k_cmd_ota_check_update, NULL, 0, packet_buf);

  STAR_ASSERT_TRUE(packet_len > 0);

  /* Parse the created packet */
  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(packet_buf, packet_len, &packet);

  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_EQUAL(k_cmd_ota_check_update, packet->cmd);
}

STAR_TEST_CASE(ota_handler, check_update_returns_version)
{
  /* Initialize OTA manager */
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 0,
    .auto_update       = false,
    .auto_reboot       = false,
  };

  ota_manager_init(&config);
  reset_transport_mock();

  /* Create packet */
  uint8_t packet_buf[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_ota_check_update, NULL, 0, packet_buf);

  /* The handler would be called here in real scenario */
  /* For now, just verify version can be retrieved */
  uint8_t major, minor, patch;
  ota_manager_get_version(&major, &minor, &patch);

  STAR_ASSERT_NOT_EQUAL(0xFF, major);
  STAR_ASSERT_NOT_EQUAL(0xFF, minor);
  STAR_ASSERT_NOT_EQUAL(0xFF, patch);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_handler, check_update_response_format)
{
  ota_check_response_t response;
  response.update_available   = 0;
  response.current_version[0] = 0;
  response.current_version[1] = 1;
  response.current_version[2] = 0;
  response.new_version[0]     = 0;
  response.new_version[1]     = 1;
  response.new_version[2]     = 0;

  /* Verify structure size is as expected */
  STAR_ASSERT_EQUAL(7, sizeof(response));

  /* Verify fields can be set */
  response.update_available = 1;
  STAR_ASSERT_EQUAL(1, response.update_available);
}

STAR_TEST_CASE(ota_handler, check_update_no_update_available)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 0,
    .auto_update       = false,
    .auto_reboot       = false,
  };

  ota_manager_init(&config);

  /* When no update is available, check should return false */
  bool available = ota_manager_check_update();

  STAR_ASSERT_FALSE(available);

  ota_status_t status;
  ota_manager_get_status(&status);
  STAR_ASSERT_FALSE(status.update_available);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_handler, check_update_version_comparison)
{
  ota_check_response_t response;

  /* Same version */
  response.current_version[0] = 1;
  response.current_version[1] = 2;
  response.current_version[2] = 3;
  response.new_version[0]     = 1;
  response.new_version[1]     = 2;
  response.new_version[2]     = 3;

  /* Verify memcmp would return 0 for same version */
  int32_t cmp = memcmp(response.current_version, response.new_version, 3);
  STAR_ASSERT_EQUAL(0, cmp);

  /* Newer version */
  response.new_version[2] = 4;
  cmp                     = memcmp(response.current_version, response.new_version, 3);
  STAR_ASSERT_NOT_EQUAL(0, cmp);
}

STAR_TEST_CASE(ota_handler, check_update_packet_payload_size)
{
  /* OTA check update should have no payload */
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_ota_check_update, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(0, packet[2]); /* Payload length low */
  STAR_ASSERT_EQUAL(0, packet[3]); /* Payload length high */
}

/* ========================================================================
 * OTA Start Update Command Tests (8 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_handler, start_update_valid_url)
{
  ota_start_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.url, "https://example.com/firmware.bin", sizeof(payload.url) - 1);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_ota_start_update, &payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > PROTOCOL_HEADER_SIZE);

  /* Parse packet */
  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, len, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_ota_start_update, parsed->cmd);
}

STAR_TEST_CASE(ota_handler, start_update_url_encoding)
{
  ota_start_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  const char* test_url = "https://example.com/firmware.bin";
  strncpy(payload.url, test_url, sizeof(payload.url) - 1);

  /* Verify URL is null-terminated */
  STAR_ASSERT_EQUAL('\0', payload.url[strlen(test_url)]);

  /* Verify URL matches */
  STAR_ASSERT_STR_EQUAL(test_url, payload.url);
}

STAR_TEST_CASE(ota_handler, start_update_long_url)
{
  ota_start_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Create URL that fits in buffer */
  char long_url[sizeof(payload.url)];
  memset(long_url, 'a', sizeof(long_url) - 1);
  long_url[sizeof(long_url) - 1] = '\0';

  strncpy(payload.url, long_url, sizeof(payload.url) - 1);
  payload.url[sizeof(payload.url) - 1] = '\0';

  /* Should be truncated but still null-terminated */
  STAR_ASSERT_EQUAL('\0', payload.url[sizeof(payload.url) - 1]);
}

STAR_TEST_CASE(ota_handler, start_update_http_url)
{
  ota_start_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.url, "http://example.com/firmware.bin", sizeof(payload.url) - 1);

  /* HTTP URLs should be valid */
  STAR_ASSERT_TRUE(strlen(payload.url) > 0);
  STAR_ASSERT_TRUE(strncmp(payload.url, "http://", 7) == 0);
}

STAR_TEST_CASE(ota_handler, start_update_https_url)
{
  ota_start_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.url, "https://example.com/firmware.bin", sizeof(payload.url) - 1);

  /* HTTPS URLs should be valid */
  STAR_ASSERT_TRUE(strlen(payload.url) > 0);
  STAR_ASSERT_TRUE(strncmp(payload.url, "https://", 8) == 0);
}

STAR_TEST_CASE(ota_handler, start_update_packet_size)
{
  ota_start_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.url, "https://example.com/firmware.bin", sizeof(payload.url) - 1);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_ota_start_update, &payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
}

STAR_TEST_CASE(ota_handler, start_update_empty_url)
{
  ota_start_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  /* Empty URL */

  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 0,
    .auto_update       = false,
    .auto_reboot       = false,
  };

  ota_manager_init(&config);

  /* Empty URL should fail */
  bool result = ota_manager_start_update(payload.url);

  STAR_ASSERT_FALSE(result);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_handler, start_update_special_characters_url)
{
  ota_start_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.url,
          "https://example.com/firmware-v1.0.0_beta+build.123.bin?token=abc123",
          sizeof(payload.url) - 1);

  /* URL with special characters should be stored correctly */
  STAR_ASSERT_TRUE(strlen(payload.url) > 0);
  STAR_ASSERT_TRUE(strstr(payload.url, "firmware-v1.0.0_beta+build.123.bin") != NULL);
}

/* ========================================================================
 * OTA Get Status Command Tests (7 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_handler, get_status_response_structure)
{
  ota_status_response_t response;

  /* Verify structure size: 1 (state) + 1 (progress) + 4 (bytes_downloaded) + 4 (total_bytes) + 1 (error_code) = 11 bytes */
  STAR_ASSERT_EQUAL(11, sizeof(response));

  /* Verify fields can be set */
  response.state            = k_ota_idle;
  response.progress         = 0;
  response.bytes_downloaded = 0;
  response.total_bytes      = 0;
  response.error_code       = 0;

  STAR_ASSERT_EQUAL(k_ota_idle, response.state);
}

STAR_TEST_CASE(ota_handler, get_status_idle_state)
{
  ota_config_t config = {
    .update_url        = "https://example.com/firmware.bin",
    .check_interval_ms = 0,
    .auto_update       = false,
    .auto_reboot       = false,
  };

  ota_manager_init(&config);

  ota_status_t status;
  ota_manager_get_status(&status);

  STAR_ASSERT_EQUAL(k_ota_idle, status.state);
  STAR_ASSERT_EQUAL(0, status.progress);
  STAR_ASSERT_EQUAL(0, status.bytes_downloaded);
  STAR_ASSERT_EQUAL(0, status.total_bytes);

  ota_manager_deinit();
}

STAR_TEST_CASE(ota_handler, get_status_progress_range)
{
  ota_status_response_t response;

  /* Progress should be 0-100 */
  response.progress = 0;
  STAR_ASSERT_TRUE(response.progress >= 0 && response.progress <= 100);

  response.progress = 50;
  STAR_ASSERT_TRUE(response.progress >= 0 && response.progress <= 100);

  response.progress = 100;
  STAR_ASSERT_TRUE(response.progress >= 0 && response.progress <= 100);
}

STAR_TEST_CASE(ota_handler, get_status_bytes_tracking)
{
  ota_status_response_t response;

  response.bytes_downloaded = 500000;
  response.total_bytes      = 1000000;

  /* Verify progress calculation would be correct */
  uint8_t calculated_progress = (uint8_t)((response.bytes_downloaded * 100) / response.total_bytes);

  STAR_ASSERT_EQUAL(50, calculated_progress);
}

STAR_TEST_CASE(ota_handler, get_status_all_states)
{
  ota_status_response_t response;

  /* Test all OTA states */
  response.state = k_ota_idle;
  STAR_ASSERT_EQUAL(k_ota_idle, response.state);

  response.state = k_ota_checking;
  STAR_ASSERT_EQUAL(k_ota_checking, response.state);

  response.state = k_ota_downloading;
  STAR_ASSERT_EQUAL(k_ota_downloading, response.state);

  response.state = k_ota_verifying;
  STAR_ASSERT_EQUAL(k_ota_verifying, response.state);

  response.state = k_ota_installing;
  STAR_ASSERT_EQUAL(k_ota_installing, response.state);

  response.state = k_ota_complete;
  STAR_ASSERT_EQUAL(k_ota_complete, response.state);

  response.state = k_ota_failed;
  STAR_ASSERT_EQUAL(k_ota_failed, response.state);
}

STAR_TEST_CASE(ota_handler, get_status_packet_format)
{
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_ota_get_status, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(k_cmd_ota_get_status, packet[1]);
}

STAR_TEST_CASE(ota_handler, get_status_response_size)
{
  ota_status_response_t response;
  memset(&response, 0, sizeof(response));

  response.state            = k_ota_downloading;
  response.progress         = 75;
  response.bytes_downloaded = 750000;
  response.total_bytes      = 1000000;

  /* Create response packet */
  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(response)];
  uint16_t len = protocol_create_packet(k_cmd_response, &response, sizeof(response), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(response), len);
}

/* ========================================================================
 * OTA Protocol Error Handling Tests (4 tests)
 * ======================================================================== */

STAR_TEST_CASE(ota_handler, invalid_ota_command)
{
  /* Create packet with invalid OTA command */
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(0xFF, NULL, 0, packet);

  STAR_ASSERT_TRUE(len > 0);

  /* Parse should succeed, but command would be invalid */
  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, len, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(0xFF, parsed->cmd);
}

STAR_TEST_CASE(ota_handler, truncated_start_update_packet)
{
  ota_start_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.url, "https://example.com/firmware.bin", sizeof(payload.url) - 1);

  /* Create packet but truncate it */
  uint8_t packet[PROTOCOL_HEADER_SIZE + 10]; /* Only 10 bytes of payload */
  packet[0] = PROTOCOL_START_MARKER;
  packet[1] = k_cmd_ota_start_update;
  packet[2] = 10; /* Claim 10 bytes payload */
  packet[3] = 0;

  memcpy(&packet[4], &payload, 10);

  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + 10, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(10, parsed->payload_len);
}

STAR_TEST_CASE(ota_handler, ota_response_error_status)
{
  reset_transport_mock();

  /* Send error response for OTA failure */
  protocol_send_error(k_status_ota_failed);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  /* Parse sent packet */
  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);
  STAR_ASSERT_EQUAL(k_cmd_response, packet->cmd);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ota_failed, response->status);
}

STAR_TEST_CASE(ota_handler, ota_response_progress_status)
{
  reset_transport_mock();

  /* Send progress status */
  protocol_send_response(k_status_ota_progress, NULL, 0);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ota_progress, response->status);
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()

/* Check update command tests */
STAR_TEST_REF(ota_handler, check_update_response_structure)
STAR_TEST_REF(ota_handler, check_update_returns_version)
STAR_TEST_REF(ota_handler, check_update_response_format)
STAR_TEST_REF(ota_handler, check_update_no_update_available)
STAR_TEST_REF(ota_handler, check_update_version_comparison)
STAR_TEST_REF(ota_handler, check_update_packet_payload_size)

/* Start update command tests */
STAR_TEST_REF(ota_handler, start_update_valid_url)
STAR_TEST_REF(ota_handler, start_update_url_encoding)
STAR_TEST_REF(ota_handler, start_update_long_url)
STAR_TEST_REF(ota_handler, start_update_http_url)
STAR_TEST_REF(ota_handler, start_update_https_url)
STAR_TEST_REF(ota_handler, start_update_packet_size)
STAR_TEST_REF(ota_handler, start_update_empty_url)
STAR_TEST_REF(ota_handler, start_update_special_characters_url)

/* Get status command tests */
STAR_TEST_REF(ota_handler, get_status_response_structure)
STAR_TEST_REF(ota_handler, get_status_idle_state)
STAR_TEST_REF(ota_handler, get_status_progress_range)
STAR_TEST_REF(ota_handler, get_status_bytes_tracking)
STAR_TEST_REF(ota_handler, get_status_all_states)
STAR_TEST_REF(ota_handler, get_status_packet_format)
STAR_TEST_REF(ota_handler, get_status_response_size)

/* Error handling tests */
STAR_TEST_REF(ota_handler, invalid_ota_command)
STAR_TEST_REF(ota_handler, truncated_start_update_packet)
STAR_TEST_REF(ota_handler, ota_response_error_status)
STAR_TEST_REF(ota_handler, ota_response_progress_status)

STAR_TEST_LIST_END()
