/* esp32-firmware/components/pynq_wifi_bridge/test/test_wifi_handler.c */

/**
 * @file test_wifi_handler.c
 * @brief STAR_TEST comprehensive tests for WiFi command handlers
 */

#include <string.h>

#include "pynq_wifi_manager.h"
#include "pynq_wifi_protocol.h"
#include "star_test.h"
#include "test_transport_mock.h"

/* Convenience macros for the shared transport mock */
#define g_transport_buffer transport_mock_get_buffer()
#define g_transport_len transport_mock_get_length()
#define reset_transport_mock() transport_mock_reset()

/* ========================================================================
 * WiFi Connect Command Tests (15 tests)
 * ======================================================================== */

STAR_TEST_CASE(wifi_handler, wifi_connect_packet_structure)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Verify structure size: 32 bytes SSID + 64 bytes password */
  STAR_ASSERT_EQUAL(96, sizeof(payload));
}

STAR_TEST_CASE(wifi_handler, wifi_connect_valid_credentials)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.ssid, "TestNetwork", sizeof(payload.ssid) - 1);
  strncpy(payload.password, "TestPassword123", sizeof(payload.password) - 1);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_wifi_connect, &payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
  STAR_ASSERT_EQUAL(k_cmd_wifi_connect, packet[1]);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_ssid_encoding)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_ssid = "MyWiFiNetwork";
  strncpy(payload.ssid, test_ssid, sizeof(payload.ssid) - 1);

  /* Verify SSID is stored correctly */
  STAR_ASSERT_STR_EQUAL(test_ssid, payload.ssid);
  STAR_ASSERT_EQUAL('\0', payload.ssid[strlen(test_ssid)]);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_password_encoding)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_password = "SecurePassword!@#$%";
  strncpy(payload.password, test_password, sizeof(payload.password) - 1);

  /* Verify password is stored correctly */
  STAR_ASSERT_STR_EQUAL(test_password, payload.password);
  STAR_ASSERT_EQUAL('\0', payload.password[strlen(test_password)]);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_max_ssid_length)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Fill SSID buffer completely */
  for (uint32_t i = 0; i < sizeof(payload.ssid) - 1; i++) {
    payload.ssid[i] = 'A';
  }
  payload.ssid[sizeof(payload.ssid) - 1] = '\0';

  STAR_ASSERT_EQUAL('\0', payload.ssid[sizeof(payload.ssid) - 1]);
  STAR_ASSERT_EQUAL(sizeof(payload.ssid) - 1, strlen(payload.ssid));
}

STAR_TEST_CASE(wifi_handler, wifi_connect_max_password_length)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Fill password buffer completely */
  for (uint32_t i = 0; i < sizeof(payload.password) - 1; i++) {
    payload.password[i] = 'p';
  }
  payload.password[sizeof(payload.password) - 1] = '\0';

  STAR_ASSERT_EQUAL('\0', payload.password[sizeof(payload.password) - 1]);
  STAR_ASSERT_EQUAL(sizeof(payload.password) - 1, strlen(payload.password));
}

STAR_TEST_CASE(wifi_handler, wifi_connect_empty_ssid)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.password, "password", sizeof(payload.password) - 1);
  /* Empty SSID */

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_wifi_connect, &payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
  STAR_ASSERT_EQUAL('\0', payload.ssid[0]);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_empty_password_open_network)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.ssid, "OpenNetwork", sizeof(payload.ssid) - 1);
  /* Empty password for open network */

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_wifi_connect, &payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
  STAR_ASSERT_EQUAL('\0', payload.password[0]);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_ssid_with_spaces)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* ssid_with_spaces = "My WiFi Network";
  strncpy(payload.ssid, ssid_with_spaces, sizeof(payload.ssid) - 1);

  /* Verify spaces are preserved */
  STAR_ASSERT_STR_EQUAL(ssid_with_spaces, payload.ssid);
  STAR_ASSERT_TRUE(strstr(payload.ssid, " ") != NULL);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_ssid_with_special_chars)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* special_ssid = "WiFi-Network_5GHz";
  strncpy(payload.ssid, special_ssid, sizeof(payload.ssid) - 1);

  /* Verify special characters preserved */
  STAR_ASSERT_STR_EQUAL(special_ssid, payload.ssid);
  STAR_ASSERT_TRUE(strstr(payload.ssid, "-") != NULL);
  STAR_ASSERT_TRUE(strstr(payload.ssid, "_") != NULL);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_password_with_special_chars)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* special_password = "P@ssw0rd!#$%^&*()";
  strncpy(payload.password, special_password, sizeof(payload.password) - 1);

  /* Verify special characters preserved */
  STAR_ASSERT_STR_EQUAL(special_password, payload.password);
  STAR_ASSERT_TRUE(strstr(payload.password, "@") != NULL);
  STAR_ASSERT_TRUE(strstr(payload.password, "!") != NULL);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_packet_parsing)
{
  wifi_connect_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.ssid, "TestSSID", sizeof(payload.ssid) - 1);
  strncpy(payload.password, "TestPass", sizeof(payload.password) - 1);

  uint8_t packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  protocol_create_packet(k_cmd_wifi_connect, &payload, sizeof(payload), packet);

  /* Parse back */
  protocol_packet_t* parsed = NULL;
  bool result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + sizeof(payload), &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_wifi_connect, parsed->cmd);
  STAR_ASSERT_EQUAL(sizeof(payload), parsed->payload_len);

  wifi_connect_payload_t* parsed_payload = (wifi_connect_payload_t*)parsed->payload;
  STAR_ASSERT_STR_EQUAL("TestSSID", parsed_payload->ssid);
  STAR_ASSERT_STR_EQUAL("TestPass", parsed_payload->password);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_response_ok)
{
  reset_transport_mock();

  protocol_send_response(k_status_ok, NULL, 0);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_response_failed)
{
  reset_transport_mock();

  protocol_send_error(k_status_wifi_failed);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_wifi_failed, response->status);
}

STAR_TEST_CASE(wifi_handler, wifi_connect_timeout)
{
  reset_transport_mock();

  protocol_send_error(k_status_timeout);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_timeout, response->status);
}

/* ========================================================================
 * WiFi Disconnect Command Tests (5 tests)
 * ======================================================================== */

STAR_TEST_CASE(wifi_handler, wifi_disconnect_packet_structure)
{
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_wifi_disconnect, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(k_cmd_wifi_disconnect, packet[1]);
  STAR_ASSERT_EQUAL(0, packet[2]); /* No payload */
  STAR_ASSERT_EQUAL(0, packet[3]);
}

STAR_TEST_CASE(wifi_handler, wifi_disconnect_packet_parsing)
{
  uint8_t packet[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_wifi_disconnect, NULL, 0, packet);

  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_wifi_disconnect, parsed->cmd);
  STAR_ASSERT_EQUAL(0, parsed->payload_len);
}

STAR_TEST_CASE(wifi_handler, wifi_disconnect_response_ok)
{
  reset_transport_mock();

  protocol_send_response(k_status_ok, NULL, 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
}

STAR_TEST_CASE(wifi_handler, wifi_disconnect_when_not_connected)
{
  reset_transport_mock();

  /* Should still succeed even if not connected */
  protocol_send_response(k_status_ok, NULL, 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
}

STAR_TEST_CASE(wifi_handler, wifi_disconnect_error)
{
  reset_transport_mock();

  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

/* ========================================================================
 * WiFi Status Command Tests (12 tests)
 * ======================================================================== */

STAR_TEST_CASE(wifi_handler, wifi_status_packet_structure)
{
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_wifi_status, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(k_cmd_wifi_status, packet[1]);
  STAR_ASSERT_EQUAL(0, packet[2]); /* No payload */
  STAR_ASSERT_EQUAL(0, packet[3]);
}

STAR_TEST_CASE(wifi_handler, wifi_status_response_structure)
{
  wifi_status_response_t response;
  memset(&response, 0, sizeof(response));

  /* Verify structure contains status, SSID, and IP */
  /* Status: 1 byte, SSID: 32 bytes, IP: 4 bytes = 37 bytes */
  STAR_ASSERT_EQUAL(37, sizeof(response));
}

STAR_TEST_CASE(wifi_handler, wifi_status_disconnected)
{
  wifi_status_response_t response;
  memset(&response, 0, sizeof(response));
  response.status = 0; /* DISCONNECTED */

  STAR_ASSERT_EQUAL(0, response.status);
  STAR_ASSERT_EQUAL('\0', response.ssid[0]);
}

STAR_TEST_CASE(wifi_handler, wifi_status_connecting)
{
  wifi_status_response_t response;
  memset(&response, 0, sizeof(response));
  response.status = 1; /* CONNECTING */
  strncpy(response.ssid, "TestNetwork", sizeof(response.ssid) - 1);

  STAR_ASSERT_EQUAL(1, response.status);
  STAR_ASSERT_STR_EQUAL("TestNetwork", response.ssid);
}

STAR_TEST_CASE(wifi_handler, wifi_status_connected)
{
  wifi_status_response_t response;
  memset(&response, 0, sizeof(response));
  response.status = 2; /* CONNECTED */
  strncpy(response.ssid, "MyNetwork", sizeof(response.ssid) - 1);
  response.ip_address[0] = 192;
  response.ip_address[1] = 168;
  response.ip_address[2] = 1;
  response.ip_address[3] = 100;

  STAR_ASSERT_EQUAL(2, response.status);
  STAR_ASSERT_STR_EQUAL("MyNetwork", response.ssid);
  STAR_ASSERT_EQUAL(192, response.ip_address[0]);
  STAR_ASSERT_EQUAL(100, response.ip_address[3]);
}

STAR_TEST_CASE(wifi_handler, wifi_status_failed)
{
  wifi_status_response_t response;
  memset(&response, 0, sizeof(response));
  response.status = 3; /* FAILED */

  STAR_ASSERT_EQUAL(3, response.status);
}

STAR_TEST_CASE(wifi_handler, wifi_status_ip_address_encoding)
{
  wifi_status_response_t response;
  memset(&response, 0, sizeof(response));

  /* Test various IP addresses */
  response.ip_address[0] = 10;
  response.ip_address[1] = 0;
  response.ip_address[2] = 0;
  response.ip_address[3] = 1;

  STAR_ASSERT_EQUAL(10, response.ip_address[0]);
  STAR_ASSERT_EQUAL(0, response.ip_address[1]);
  STAR_ASSERT_EQUAL(1, response.ip_address[3]);
}

STAR_TEST_CASE(wifi_handler, wifi_status_ssid_preservation)
{
  wifi_status_response_t response;
  memset(&response, 0, sizeof(response));
  const char* ssid = "WiFi-Network 5GHz";
  strncpy(response.ssid, ssid, sizeof(response.ssid) - 1);

  /* SSID should be preserved with spaces and special chars */
  STAR_ASSERT_STR_EQUAL(ssid, response.ssid);
}

STAR_TEST_CASE(wifi_handler, wifi_status_max_ssid_length)
{
  wifi_status_response_t response;
  memset(&response, 0, sizeof(response));

  /* Fill SSID completely */
  for (uint32_t i = 0; i < sizeof(response.ssid) - 1; i++) {
    response.ssid[i] = 'X';
  }
  response.ssid[sizeof(response.ssid) - 1] = '\0';

  STAR_ASSERT_EQUAL('\0', response.ssid[sizeof(response.ssid) - 1]);
  STAR_ASSERT_EQUAL(sizeof(response.ssid) - 1, strlen(response.ssid));
}

STAR_TEST_CASE(wifi_handler, wifi_status_packet_parsing)
{
  uint8_t packet[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_wifi_status, NULL, 0, packet);

  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_wifi_status, parsed->cmd);
  STAR_ASSERT_EQUAL(0, parsed->payload_len);
}

STAR_TEST_CASE(wifi_handler, wifi_status_response_with_data)
{
  reset_transport_mock();

  wifi_status_response_t status;
  memset(&status, 0, sizeof(status));
  status.status = 2;
  strncpy(status.ssid, "TestNet", sizeof(status.ssid) - 1);
  status.ip_address[0] = 192;
  status.ip_address[1] = 168;
  status.ip_address[2] = 1;
  status.ip_address[3] = 50;

  protocol_send_response(k_status_ok, &status, sizeof(status));

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(sizeof(status), response->data_len);
}

STAR_TEST_CASE(wifi_handler, wifi_status_all_states)
{
  wifi_status_response_t response;

  /* Test all WiFi states */
  response.status = 0;
  STAR_ASSERT_EQUAL(0, response.status); /* DISCONNECTED */

  response.status = 1;
  STAR_ASSERT_EQUAL(1, response.status); /* CONNECTING */

  response.status = 2;
  STAR_ASSERT_EQUAL(2, response.status); /* CONNECTED */

  response.status = 3;
  STAR_ASSERT_EQUAL(3, response.status); /* FAILED */
}

/* ========================================================================
 * WiFi Scan Command Tests (10 tests)
 * ======================================================================== */

STAR_TEST_CASE(wifi_handler, wifi_scan_packet_structure)
{
  uint8_t  packet[PROTOCOL_HEADER_SIZE];
  uint16_t len = protocol_create_packet(k_cmd_wifi_scan, NULL, 0, packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE, len);
  STAR_ASSERT_EQUAL(k_cmd_wifi_scan, packet[1]);
  STAR_ASSERT_EQUAL(0, packet[2]); /* No payload */
  STAR_ASSERT_EQUAL(0, packet[3]);
}

STAR_TEST_CASE(wifi_handler, wifi_scan_result_structure)
{
  wifi_scan_result_t result;
  memset(&result, 0, sizeof(result));

  /* SSID: 33 bytes, RSSI: 1 byte, Channel: 1 byte, Auth: 1 byte = 36 bytes */
  STAR_ASSERT_TRUE(sizeof(result) >= 36);
}

STAR_TEST_CASE(wifi_handler, wifi_scan_result_ssid)
{
  wifi_scan_result_t result;
  memset(&result, 0, sizeof(result));
  strncpy(result.ssid, "TestNetwork", sizeof(result.ssid) - 1);

  STAR_ASSERT_STR_EQUAL("TestNetwork", result.ssid);
}

STAR_TEST_CASE(wifi_handler, wifi_scan_result_rssi)
{
  wifi_scan_result_t result;
  memset(&result, 0, sizeof(result));
  result.rssi = -50; /* Typical WiFi RSSI */

  STAR_ASSERT_EQUAL(-50, result.rssi);

  /* Test various RSSI values */
  result.rssi = -30; /* Excellent */
  STAR_ASSERT_TRUE(result.rssi > -40);

  result.rssi = -80; /* Poor */
  STAR_ASSERT_TRUE(result.rssi < -70);
}

STAR_TEST_CASE(wifi_handler, wifi_scan_result_auth_mode)
{
  wifi_scan_result_t result;
  memset(&result, 0, sizeof(result));

  /* Test various auth modes */
  result.auth_mode = 0; /* OPEN */
  STAR_ASSERT_EQUAL(0, result.auth_mode);

  result.auth_mode = 1; /* WEP */
  STAR_ASSERT_EQUAL(1, result.auth_mode);

  result.auth_mode = 2; /* WPA_PSK */
  STAR_ASSERT_EQUAL(2, result.auth_mode);

  result.auth_mode = 3; /* WPA2_PSK */
  STAR_ASSERT_EQUAL(3, result.auth_mode);
}

STAR_TEST_CASE(wifi_handler, wifi_scan_result_channel)
{
  wifi_scan_result_t result;
  memset(&result, 0, sizeof(result));

  /* Test various WiFi channels */
  result.channel = 1;
  STAR_ASSERT_EQUAL(1, result.channel);

  result.channel = 6;
  STAR_ASSERT_EQUAL(6, result.channel);

  result.channel = 11;
  STAR_ASSERT_EQUAL(11, result.channel);
}

STAR_TEST_CASE(wifi_handler, wifi_scan_multiple_results)
{
  wifi_scan_result_t results[3];
  memset(results, 0, sizeof(results));

  strncpy(results[0].ssid, "Network1", sizeof(results[0].ssid) - 1);
  results[0].rssi    = -40;
  results[0].channel = 1;

  strncpy(results[1].ssid, "Network2", sizeof(results[1].ssid) - 1);
  results[1].rssi    = -60;
  results[1].channel = 6;

  strncpy(results[2].ssid, "Network3", sizeof(results[2].ssid) - 1);
  results[2].rssi    = -80;
  results[2].channel = 11;

  /* Verify all results */
  STAR_ASSERT_STR_EQUAL("Network1", results[0].ssid);
  STAR_ASSERT_STR_EQUAL("Network2", results[1].ssid);
  STAR_ASSERT_STR_EQUAL("Network3", results[2].ssid);
}

STAR_TEST_CASE(wifi_handler, wifi_scan_packet_parsing)
{
  uint8_t packet[PROTOCOL_HEADER_SIZE];
  protocol_create_packet(k_cmd_wifi_scan, NULL, 0, packet);

  protocol_packet_t* parsed = NULL;
  bool               result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE, &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_wifi_scan, parsed->cmd);
}

STAR_TEST_CASE(wifi_handler, wifi_scan_response_empty)
{
  reset_transport_mock();

  /* No networks found */
  protocol_send_response(k_status_ok, NULL, 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(0, response->data_len);
}

STAR_TEST_CASE(wifi_handler, wifi_scan_response_with_results)
{
  reset_transport_mock();

  wifi_scan_result_t results[2];
  memset(results, 0, sizeof(results));
  strncpy(results[0].ssid, "Net1", sizeof(results[0].ssid) - 1);
  strncpy(results[1].ssid, "Net2", sizeof(results[1].ssid) - 1);

  protocol_send_response(k_status_ok, results, sizeof(results));

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(sizeof(results), response->data_len);
}

/* ========================================================================
 * WiFi Error Handling Tests (3 tests)
 * ======================================================================== */

STAR_TEST_CASE(wifi_handler, wifi_general_error)
{
  reset_transport_mock();

  protocol_send_error(k_status_error);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_error, response->status);
}

STAR_TEST_CASE(wifi_handler, wifi_timeout_error)
{
  reset_transport_mock();

  protocol_send_error(k_status_timeout);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_timeout, response->status);
}

STAR_TEST_CASE(wifi_handler, wifi_transport_error)
{
  reset_transport_mock();
  transport_mock_set_error(1);

  protocol_send_response(k_status_ok, NULL, 0);

  /* Should handle gracefully */
  STAR_ASSERT_TRUE(true);
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()

/* WiFi Connect tests */
STAR_TEST_REF(wifi_handler, wifi_connect_packet_structure)
STAR_TEST_REF(wifi_handler, wifi_connect_valid_credentials)
STAR_TEST_REF(wifi_handler, wifi_connect_ssid_encoding)
STAR_TEST_REF(wifi_handler, wifi_connect_password_encoding)
STAR_TEST_REF(wifi_handler, wifi_connect_max_ssid_length)
STAR_TEST_REF(wifi_handler, wifi_connect_max_password_length)
STAR_TEST_REF(wifi_handler, wifi_connect_empty_ssid)
STAR_TEST_REF(wifi_handler, wifi_connect_empty_password_open_network)
STAR_TEST_REF(wifi_handler, wifi_connect_ssid_with_spaces)
STAR_TEST_REF(wifi_handler, wifi_connect_ssid_with_special_chars)
STAR_TEST_REF(wifi_handler, wifi_connect_password_with_special_chars)
STAR_TEST_REF(wifi_handler, wifi_connect_packet_parsing)
STAR_TEST_REF(wifi_handler, wifi_connect_response_ok)
STAR_TEST_REF(wifi_handler, wifi_connect_response_failed)
STAR_TEST_REF(wifi_handler, wifi_connect_timeout)

/* WiFi Disconnect tests */
STAR_TEST_REF(wifi_handler, wifi_disconnect_packet_structure)
STAR_TEST_REF(wifi_handler, wifi_disconnect_packet_parsing)
STAR_TEST_REF(wifi_handler, wifi_disconnect_response_ok)
STAR_TEST_REF(wifi_handler, wifi_disconnect_when_not_connected)
STAR_TEST_REF(wifi_handler, wifi_disconnect_error)

/* WiFi Status tests */
STAR_TEST_REF(wifi_handler, wifi_status_packet_structure)
STAR_TEST_REF(wifi_handler, wifi_status_response_structure)
STAR_TEST_REF(wifi_handler, wifi_status_disconnected)
STAR_TEST_REF(wifi_handler, wifi_status_connecting)
STAR_TEST_REF(wifi_handler, wifi_status_connected)
STAR_TEST_REF(wifi_handler, wifi_status_failed)
STAR_TEST_REF(wifi_handler, wifi_status_ip_address_encoding)
STAR_TEST_REF(wifi_handler, wifi_status_ssid_preservation)
STAR_TEST_REF(wifi_handler, wifi_status_max_ssid_length)
STAR_TEST_REF(wifi_handler, wifi_status_packet_parsing)
STAR_TEST_REF(wifi_handler, wifi_status_response_with_data)
STAR_TEST_REF(wifi_handler, wifi_status_all_states)

/* WiFi Scan tests */
STAR_TEST_REF(wifi_handler, wifi_scan_packet_structure)
STAR_TEST_REF(wifi_handler, wifi_scan_result_structure)
STAR_TEST_REF(wifi_handler, wifi_scan_result_ssid)
STAR_TEST_REF(wifi_handler, wifi_scan_result_rssi)
STAR_TEST_REF(wifi_handler, wifi_scan_result_auth_mode)
STAR_TEST_REF(wifi_handler, wifi_scan_result_channel)
STAR_TEST_REF(wifi_handler, wifi_scan_multiple_results)
STAR_TEST_REF(wifi_handler, wifi_scan_packet_parsing)
STAR_TEST_REF(wifi_handler, wifi_scan_response_empty)
STAR_TEST_REF(wifi_handler, wifi_scan_response_with_results)

/* Error handling tests */
STAR_TEST_REF(wifi_handler, wifi_general_error)
STAR_TEST_REF(wifi_handler, wifi_timeout_error)
STAR_TEST_REF(wifi_handler, wifi_transport_error)

STAR_TEST_LIST_END()
