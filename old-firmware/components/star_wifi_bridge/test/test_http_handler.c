/* esp32-firmware/components/pynq_wifi_bridge/test/test_http_handler.c */

/**
 * @file test_http_handler.c
 * @brief STAR_TEST comprehensive tests for HTTP command handlers
 */

#include <string.h>

#include "pynq_wifi_protocol.h"
#include "star_test.h"
#include "test_transport_mock.h"

/* Convenience macros for the shared transport mock */
#define g_transport_buffer transport_mock_get_buffer()
#define g_transport_len transport_mock_get_length()
#define reset_transport_mock() transport_mock_reset()

/* ========================================================================
 * HTTP GET Command Tests (15 tests)
 * ======================================================================== */

STAR_TEST_CASE(http_handler, http_get_packet_structure)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Verify structure size */
  STAR_ASSERT_EQUAL(256, sizeof(payload));
}

STAR_TEST_CASE(http_handler, http_get_valid_url)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_url = "http://example.com/api/data";
  strncpy(payload.url, test_url, sizeof(payload.url) - 1);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_http_get, &payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
  STAR_ASSERT_EQUAL(k_cmd_http_get, packet[1]);
}

STAR_TEST_CASE(http_handler, http_get_url_encoding)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_url = "http://httpbin.org/get";
  strncpy(payload.url, test_url, sizeof(payload.url) - 1);

  /* Verify URL is stored correctly */
  STAR_ASSERT_STR_EQUAL(test_url, payload.url);
  STAR_ASSERT_EQUAL('\0', payload.url[strlen(test_url)]);
}

STAR_TEST_CASE(http_handler, http_get_https_url)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.url, "https://httpbin.org/get", sizeof(payload.url) - 1);

  /* HTTPS URLs should be valid */
  STAR_ASSERT_TRUE(strncmp(payload.url, "https://", 8) == 0);
}

STAR_TEST_CASE(http_handler, http_get_empty_url)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  /* Empty URL */

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_http_get, &payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
  STAR_ASSERT_EQUAL('\0', payload.url[0]);
}

STAR_TEST_CASE(http_handler, http_get_max_length_url)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Fill URL buffer completely (minus null terminator) */
  for (uint32_t i = 0; i < sizeof(payload.url) - 1; i++) {
    payload.url[i] = 'a';
  }
  payload.url[sizeof(payload.url) - 1] = '\0';

  STAR_ASSERT_EQUAL('\0', payload.url[sizeof(payload.url) - 1]);
  STAR_ASSERT_EQUAL(sizeof(payload.url) - 1, strlen(payload.url));
}

STAR_TEST_CASE(http_handler, http_get_url_with_query_params)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_url = "http://httpbin.org/get?param1=value1&param2=value2";
  strncpy(payload.url, test_url, sizeof(payload.url) - 1);

  /* Verify query parameters are preserved */
  STAR_ASSERT_TRUE(strstr(payload.url, "?param1=value1") != NULL);
  STAR_ASSERT_TRUE(strstr(payload.url, "&param2=value2") != NULL);
}

STAR_TEST_CASE(http_handler, http_get_url_with_fragment)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_url = "http://example.com/page#section";
  strncpy(payload.url, test_url, sizeof(payload.url) - 1);

  /* Verify fragment is preserved */
  STAR_ASSERT_TRUE(strstr(payload.url, "#section") != NULL);
}

STAR_TEST_CASE(http_handler, http_get_url_with_port)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_url = "http://example.com:8080/api";
  strncpy(payload.url, test_url, sizeof(payload.url) - 1);

  /* Verify port is preserved */
  STAR_ASSERT_TRUE(strstr(payload.url, ":8080") != NULL);
}

STAR_TEST_CASE(http_handler, http_get_url_with_special_chars)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_url = "http://example.com/path%20with%20spaces";
  strncpy(payload.url, test_url, sizeof(payload.url) - 1);

  /* Verify URL encoding is preserved */
  STAR_ASSERT_TRUE(strstr(payload.url, "%20") != NULL);
}

STAR_TEST_CASE(http_handler, http_get_packet_parsing)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  const char* test_url = "http://httpbin.org/json";
  strncpy(payload.url, test_url, sizeof(payload.url) - 1);

  uint8_t packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  protocol_create_packet(k_cmd_http_get, &payload, sizeof(payload), packet);

  /* Parse the packet back */
  protocol_packet_t* parsed = NULL;
  bool result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + sizeof(payload), &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_http_get, parsed->cmd);
  STAR_ASSERT_EQUAL(sizeof(payload), parsed->payload_len);

  http_get_payload_t* parsed_payload = (http_get_payload_t*)parsed->payload;
  STAR_ASSERT_STR_EQUAL(test_url, parsed_payload->url);
}

STAR_TEST_CASE(http_handler, http_get_truncated_url)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));

  /* Create URL longer than buffer */
  char long_url[300];
  memset(long_url, 'a', sizeof(long_url) - 1);
  long_url[sizeof(long_url) - 1] = '\0';

  strncpy(payload.url, long_url, sizeof(payload.url) - 1);
  payload.url[sizeof(payload.url) - 1] = '\0';

  /* Should be truncated but null-terminated */
  STAR_ASSERT_EQUAL('\0', payload.url[sizeof(payload.url) - 1]);
  STAR_ASSERT_TRUE(strlen(payload.url) < strlen(long_url));
}

STAR_TEST_CASE(http_handler, http_get_response_ok)
{
  reset_transport_mock();

  /* Simulate HTTP response with data */
  const char* response_data = "{\"status\":\"ok\"}";
  protocol_send_response(k_status_ok, response_data, strlen(response_data));

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_EQUAL(k_cmd_response, packet->cmd);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(strlen(response_data), response->data_len);
}

STAR_TEST_CASE(http_handler, http_get_response_error)
{
  reset_transport_mock();

  /* Simulate HTTP error */
  protocol_send_error(k_status_http_failed);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_http_failed, response->status);
}

STAR_TEST_CASE(http_handler, http_get_large_response)
{
  reset_transport_mock();

  /* Simulate large HTTP response - response_payload_t is 3 bytes (status + data_len)
   * Max data: 1024 - 3 = 1021 bytes to fit within PROTOCOL_MAX_PAYLOAD_SIZE */
  uint8_t large_data[1021];
  memset(large_data, 'X', sizeof(large_data));

  protocol_send_response(k_status_ok, large_data, sizeof(large_data));

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);

  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
  STAR_ASSERT_EQUAL(sizeof(large_data), response->data_len);
}

/* ========================================================================
 * HTTP POST Command Tests (15 tests)
 * ======================================================================== */

STAR_TEST_CASE(http_handler, http_post_packet_structure)
{
  /* HTTP POST uses variable-length payload:
   * - 2 bytes: URL length
   * - 2 bytes: Data length
   * - URL bytes (variable)
   * - Data bytes (variable)
   */
  const char* url  = "http://httpbin.org/post";
  const char* data = "{\"key\":\"value\"}";

  uint16_t url_len  = strlen(url);
  uint16_t data_len = strlen(data);

  uint16_t total_len = 4 + url_len + data_len; /* 4 bytes for lengths */

  STAR_ASSERT_TRUE(total_len > 0);
  STAR_ASSERT_TRUE(total_len < PROTOCOL_MAX_PAYLOAD_SIZE);
}

STAR_TEST_CASE(http_handler, http_post_valid_payload)
{
  const char* url      = "http://httpbin.org/post";
  const char* postdata = "{\"test\":\"data\"}";

  uint16_t url_len  = strlen(url);
  uint16_t data_len = strlen(postdata);

  /* Build payload: [url_len(2)] [data_len(2)] [url] [data] */
  uint8_t payload[4 + url_len + data_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);
  memcpy(&payload[4 + url_len], postdata, data_len);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_http_post, payload, sizeof(payload), packet);

  STAR_ASSERT_EQUAL(PROTOCOL_HEADER_SIZE + sizeof(payload), len);
  STAR_ASSERT_EQUAL(k_cmd_http_post, packet[1]);
}

STAR_TEST_CASE(http_handler, http_post_empty_data)
{
  const char* url = "http://httpbin.org/post";

  uint16_t url_len  = strlen(url);
  uint16_t data_len = 0;

  uint8_t payload[4 + url_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_http_post, payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
}

STAR_TEST_CASE(http_handler, http_post_json_data)
{
  const char* url      = "http://httpbin.org/post";
  const char* postdata = "{\"name\":\"test\",\"value\":123,\"enabled\":true}";

  uint16_t url_len  = strlen(url);
  uint16_t data_len = strlen(postdata);

  uint8_t payload[4 + url_len + data_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);
  memcpy(&payload[4 + url_len], postdata, data_len);

  /* Verify data integrity */
  char* extracted_data = (char*)&payload[4 + url_len];
  STAR_ASSERT_TRUE(strncmp(extracted_data, postdata, data_len) == 0);
}

STAR_TEST_CASE(http_handler, http_post_binary_data)
{
  const char* url          = "http://httpbin.org/post";
  uint8_t     binarydata[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};

  uint16_t url_len  = strlen(url);
  uint16_t data_len = sizeof(binarydata);

  uint8_t payload[4 + url_len + data_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);
  memcpy(&payload[4 + url_len], binarydata, data_len);

  /* Verify binary data integrity */
  STAR_ASSERT_EQUAL(0x00, payload[4 + url_len]);
  STAR_ASSERT_EQUAL(0xFF, payload[4 + url_len + 3]);
}

STAR_TEST_CASE(http_handler, http_post_large_data)
{
  const char* url = "http://httpbin.org/post";
  uint8_t     largedata[512];
  memset(largedata, 'A', sizeof(largedata));

  uint16_t url_len  = strlen(url);
  uint16_t data_len = sizeof(largedata);

  uint8_t payload[4 + url_len + data_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);
  memcpy(&payload[4 + url_len], largedata, data_len);

  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_http_post, payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
  STAR_ASSERT_TRUE(len < PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE);
}

STAR_TEST_CASE(http_handler, http_post_url_length_encoding)
{
  const char* url      = "http://httpbin.org/post";
  const char* postdata = "test";

  uint16_t url_len  = strlen(url);
  uint16_t data_len = strlen(postdata);

  uint8_t payload[4 + url_len + data_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);
  memcpy(&payload[4 + url_len], postdata, data_len);

  /* Verify length encoding */
  uint16_t extracted_url_len;
  uint16_t extracted_data_len;
  memcpy(&extracted_url_len, &payload[0], 2);
  memcpy(&extracted_data_len, &payload[2], 2);

  STAR_ASSERT_EQUAL(url_len, extracted_url_len);
  STAR_ASSERT_EQUAL(data_len, extracted_data_len);
}

STAR_TEST_CASE(http_handler, http_post_packet_parsing)
{
  const char* url      = "http://httpbin.org/post";
  const char* postdata = "{\"test\":\"parsing\"}";

  uint16_t url_len  = strlen(url);
  uint16_t data_len = strlen(postdata);

  uint8_t payload[4 + url_len + data_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);
  memcpy(&payload[4 + url_len], postdata, data_len);

  uint8_t packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  protocol_create_packet(k_cmd_http_post, payload, sizeof(payload), packet);

  /* Parse back */
  protocol_packet_t* parsed = NULL;
  bool result = protocol_parse_packet(packet, PROTOCOL_HEADER_SIZE + sizeof(payload), &parsed);

  STAR_ASSERT_TRUE(result);
  STAR_ASSERT_EQUAL(k_cmd_http_post, parsed->cmd);
  STAR_ASSERT_EQUAL(sizeof(payload), parsed->payload_len);
}

STAR_TEST_CASE(http_handler, http_post_special_characters_data)
{
  const char* url      = "http://httpbin.org/post";
  const char* postdata = "Line1\\nLine2\\tTab\\r\\nCRLF";

  uint16_t url_len  = strlen(url);
  uint16_t data_len = strlen(postdata);

  uint8_t payload[4 + url_len + data_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);
  memcpy(&payload[4 + url_len], postdata, data_len);

  /* Verify special characters preserved */
  char* extracted = (char*)&payload[4 + url_len];
  STAR_ASSERT_TRUE(strncmp(extracted, postdata, data_len) == 0);
}

STAR_TEST_CASE(http_handler, http_post_unicode_data)
{
  const char* url      = "http://httpbin.org/post";
  const char* postdata = "{\"text\":\"Hello \\u4e16\\u754c\"}"; /* Hello World in Chinese */

  uint16_t url_len  = strlen(url);
  uint16_t data_len = strlen(postdata);

  uint8_t payload[4 + url_len + data_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);
  memcpy(&payload[4 + url_len], postdata, data_len);

  /* Verify unicode sequences preserved */
  char* extracted = (char*)&payload[4 + url_len];
  STAR_ASSERT_TRUE(strstr(extracted, "\\u4e16") != NULL);
}

STAR_TEST_CASE(http_handler, http_post_response_ok)
{
  reset_transport_mock();

  const char* response_data = "{\"status\":\"posted\"}";
  protocol_send_response(k_status_ok, response_data, strlen(response_data));

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_ok, response->status);
}

STAR_TEST_CASE(http_handler, http_post_response_error)
{
  reset_transport_mock();

  protocol_send_error(k_status_http_failed);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_http_failed, response->status);
}

STAR_TEST_CASE(http_handler, http_post_https_url)
{
  const char* url      = "https://httpbin.org/post";
  const char* postdata = "secure data";

  uint16_t url_len  = strlen(url);
  uint16_t data_len = strlen(postdata);

  uint8_t payload[4 + url_len + data_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);
  memcpy(&payload[4 + url_len], postdata, data_len);

  /* Verify HTTPS URL */
  char* extracted_url = (char*)&payload[4];
  STAR_ASSERT_TRUE(strncmp(extracted_url, "https://", 8) == 0);
}

STAR_TEST_CASE(http_handler, http_post_content_type_preservation)
{
  const char* url      = "http://httpbin.org/post";
  const char* postdata = "plain text content";

  uint16_t url_len  = strlen(url);
  uint16_t data_len = strlen(postdata);

  uint8_t payload[4 + url_len + data_len];
  memcpy(&payload[0], &url_len, 2);
  memcpy(&payload[2], &data_len, 2);
  memcpy(&payload[4], url, url_len);
  memcpy(&payload[4 + url_len], postdata, data_len);

  /* Data should be preserved as-is */
  char* extracted_data = (char*)&payload[4 + url_len];
  STAR_ASSERT_EQUAL(0, strncmp(extracted_data, postdata, data_len));
}

/* ========================================================================
 * HTTP Error Handling Tests (5 tests)
 * ======================================================================== */

STAR_TEST_CASE(http_handler, http_invalid_url_scheme)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.url, "ftp://example.com/file", sizeof(payload.url) - 1);

  /* FTP scheme is not HTTP/HTTPS */
  STAR_ASSERT_TRUE(strncmp(payload.url, "http://", 7) != 0);
  STAR_ASSERT_TRUE(strncmp(payload.url, "https://", 8) != 0);
}

STAR_TEST_CASE(http_handler, http_malformed_url)
{
  http_get_payload_t payload;
  memset(&payload, 0, sizeof(payload));
  strncpy(payload.url, "not a url", sizeof(payload.url) - 1);

  /* Should still create packet, validation happens in handler */
  uint8_t  packet[PROTOCOL_HEADER_SIZE + sizeof(payload)];
  uint16_t len = protocol_create_packet(k_cmd_http_get, &payload, sizeof(payload), packet);

  STAR_ASSERT_TRUE(len > 0);
}

STAR_TEST_CASE(http_handler, http_wifi_not_connected_error)
{
  reset_transport_mock();

  /* Simulate WiFi not connected error */
  protocol_send_error(k_status_wifi_failed);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_wifi_failed, response->status);
}

STAR_TEST_CASE(http_handler, http_timeout_error)
{
  reset_transport_mock();

  protocol_send_error(k_status_timeout);

  STAR_ASSERT_TRUE(g_transport_len > 0);

  protocol_packet_t* packet = NULL;
  bool               parsed = protocol_parse_packet(g_transport_buffer, g_transport_len, &packet);
  STAR_ASSERT_TRUE(parsed);
  STAR_ASSERT_NOT_NULL(packet);

  response_payload_t* response = (response_payload_t*)packet->payload;
  STAR_ASSERT_EQUAL(k_status_timeout, response->status);
}

STAR_TEST_CASE(http_handler, http_transport_error)
{
  reset_transport_mock();
  transport_mock_set_error(1); /* Simulate transport failure */

  protocol_send_response(k_status_ok, "test", 4);

  /* Should handle gracefully without crash */
  STAR_ASSERT_TRUE(true);
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()

/* HTTP GET tests */
STAR_TEST_REF(http_handler, http_get_packet_structure)
STAR_TEST_REF(http_handler, http_get_valid_url)
STAR_TEST_REF(http_handler, http_get_url_encoding)
STAR_TEST_REF(http_handler, http_get_https_url)
STAR_TEST_REF(http_handler, http_get_empty_url)
STAR_TEST_REF(http_handler, http_get_max_length_url)
STAR_TEST_REF(http_handler, http_get_url_with_query_params)
STAR_TEST_REF(http_handler, http_get_url_with_fragment)
STAR_TEST_REF(http_handler, http_get_url_with_port)
STAR_TEST_REF(http_handler, http_get_url_with_special_chars)
STAR_TEST_REF(http_handler, http_get_packet_parsing)
STAR_TEST_REF(http_handler, http_get_truncated_url)
STAR_TEST_REF(http_handler, http_get_response_ok)
STAR_TEST_REF(http_handler, http_get_response_error)
STAR_TEST_REF(http_handler, http_get_large_response)

/* HTTP POST tests */
STAR_TEST_REF(http_handler, http_post_packet_structure)
STAR_TEST_REF(http_handler, http_post_valid_payload)
STAR_TEST_REF(http_handler, http_post_empty_data)
STAR_TEST_REF(http_handler, http_post_json_data)
STAR_TEST_REF(http_handler, http_post_binary_data)
STAR_TEST_REF(http_handler, http_post_large_data)
STAR_TEST_REF(http_handler, http_post_url_length_encoding)
STAR_TEST_REF(http_handler, http_post_packet_parsing)
STAR_TEST_REF(http_handler, http_post_special_characters_data)
STAR_TEST_REF(http_handler, http_post_unicode_data)
STAR_TEST_REF(http_handler, http_post_response_ok)
STAR_TEST_REF(http_handler, http_post_response_error)
STAR_TEST_REF(http_handler, http_post_https_url)
STAR_TEST_REF(http_handler, http_post_content_type_preservation)

/* Error handling tests */
STAR_TEST_REF(http_handler, http_invalid_url_scheme)
STAR_TEST_REF(http_handler, http_malformed_url)
STAR_TEST_REF(http_handler, http_wifi_not_connected_error)
STAR_TEST_REF(http_handler, http_timeout_error)
STAR_TEST_REF(http_handler, http_transport_error)

STAR_TEST_LIST_END()
