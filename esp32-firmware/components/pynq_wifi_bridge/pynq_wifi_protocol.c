/* esp32-firmware/components/pynq_wifi_bridge/pynq_wifi_protocol.c */

#include "pynq_wifi_protocol.h"

#include <string.h>

#include "esp_log.h"

static const char* TAG = "protocol";

/* Forward declarations for transport layer (implemented elsewhere) */
extern int32_t transport_send(const uint8_t* data, uint16_t len);

uint16_t
protocol_create_packet(uint8_t cmd, const void* payload, uint16_t payload_len, uint8_t* packet_out)
{
  if (!packet_out) {
    ESP_LOGE(TAG, "packet_out is NULL");
    return 0;
  }

  if (payload_len > PROTOCOL_MAX_PAYLOAD_SIZE) {
    ESP_LOGE(TAG, "Payload too large: %d bytes", payload_len);
    return 0;
  }

  /* Build packet header */
  packet_out[0] = PROTOCOL_START_MARKER;
  packet_out[1] = cmd;
  packet_out[2] = (uint8_t)(payload_len & 0xFF);        /* Low byte */
  packet_out[3] = (uint8_t)((payload_len >> 8) & 0xFF); /* High byte */

  /* Copy payload if present */
  if (payload && payload_len > 0) {
    memcpy(&packet_out[PROTOCOL_HEADER_SIZE], payload, payload_len);
  }

  uint16_t total_size = PROTOCOL_HEADER_SIZE + payload_len;
  ESP_LOGD(TAG,
           "Created packet: cmd=0x%02X, payload_len=%d, total=%d",
           cmd,
           payload_len,
           total_size);

  return total_size;
}

bool protocol_parse_packet(const uint8_t* data, uint16_t data_len, protocol_packet_t** packet_out)
{
  if (!data || !packet_out) {
    ESP_LOGE(TAG, "Invalid parameters");
    return false;
  }

  if (data_len < PROTOCOL_HEADER_SIZE) {
    ESP_LOGW(TAG, "Data too short: %d bytes", data_len);
    return false;
  }

  /* Verify start marker */
  if (data[0] != PROTOCOL_START_MARKER) {
    ESP_LOGW(TAG, "Invalid start marker: 0x%02X", data[0]);
    return false;
  }

  /* Extract payload length */
  uint16_t payload_len = data[2] | ((uint16_t)data[3] << 8);

  if (payload_len > PROTOCOL_MAX_PAYLOAD_SIZE) {
    ESP_LOGE(TAG, "Payload length too large: %d bytes", payload_len);
    return false;
  }

  /* Verify we have complete packet */
  uint16_t expected_len = PROTOCOL_HEADER_SIZE + payload_len;
  if (data_len < expected_len) {
    ESP_LOGW(TAG, "Incomplete packet: got %d bytes, expected %d", data_len, expected_len);
    return false;
  }

  /* Cast data to packet structure */
  *packet_out = (protocol_packet_t*)data;

  ESP_LOGD(TAG, "Parsed packet: cmd=0x%02X, payload_len=%d", (*packet_out)->cmd, payload_len);

  return true;
}

void protocol_send_response(uint8_t status, const void* data, uint16_t data_len)
{
  uint8_t buffer[PROTOCOL_HEADER_SIZE + sizeof(response_payload_t) + PROTOCOL_MAX_PAYLOAD_SIZE];

  /* Create response payload */
  response_payload_t response;
  response.status   = status;
  response.data_len = data_len;

  /* Copy response payload header */
  memcpy(buffer + PROTOCOL_HEADER_SIZE, &response, sizeof(response_payload_t));

  /* Copy response data if present */
  if (data && data_len > 0) {
    memcpy(buffer + PROTOCOL_HEADER_SIZE + sizeof(response_payload_t), data, data_len);
  }

  /* Create packet */
  uint16_t total_payload_len = sizeof(response_payload_t) + data_len;
  uint16_t packet_len        = protocol_create_packet(k_cmd_response,
                                               buffer + PROTOCOL_HEADER_SIZE,
                                               total_payload_len,
                                               buffer);

  if (packet_len > 0) {
    int32_t ret = transport_send(buffer, packet_len);
    if (ret < 0) {
      ESP_LOGE(TAG, "Failed to send response: %d", ret);
    }
  }
}

void protocol_send_error(uint8_t error_code)
{
  ESP_LOGW(TAG, "Sending error response: 0x%02X", error_code);
  protocol_send_response(error_code, NULL, 0);
}
