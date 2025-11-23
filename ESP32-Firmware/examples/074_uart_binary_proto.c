/**
 * @file 074_uart_binary_proto.c
 * @brief Binary protocol parsing example
 *
 * Demonstrates:
 * - Binary frame structure
 * - Start/stop bytes and framing
 * - Length encoding
 * - Checksum calculation (CRC, XOR, sum)
 * - Byte stuffing / escaping
 * - Multi-byte value encoding (big/little endian)
 */

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_uart.h"

#include <esp_log.h>
#include <string.h>

static const char *s_tag = "UART_BINARY_PROTO";

#define UART_TX_PIN (GPIO_NUM_17)
#define UART_RX_PIN (GPIO_NUM_16)
#define UART_NUM    (UART_NUM_1)
#define BUS_NAME    "binary_proto"

/* Protocol constants */
#define PROTO_START_BYTE (0x7E)
#define PROTO_STOP_BYTE (0x7F)
#define PROTO_ESCAPE_BYTE (0x7D)
#define PROTO_XOR_KEY (0x20)

/* Command types */
#define CMD_PING (0x01)
#define CMD_READ_SENSOR (0x02)
#define CMD_WRITE_CONFIG (0x03)
#define CMD_GET_STATUS (0x04)
#define CMD_RESPONSE (0x80)

/* Maximum frame size */
#define MAX_FRAME_SIZE (256)

/**
 * @brief Calculate simple checksum (XOR)
 */
static uint8_t priv_calc_checksum_xor(const uint8_t *data, size_t length)
{
  uint8_t checksum = 0;
  for (size_t i = 0; i < length; i++) {
    checksum ^= data[i];
  }
  return checksum;
}

/**
 * @brief Calculate simple checksum (Sum)
 */
static uint8_t priv_calc_checksum_sum(const uint8_t *data, size_t length)
{
  uint8_t checksum = 0;
  for (size_t i = 0; i < length; i++) {
    checksum += data[i];
  }
  return ~checksum + 1; /* Two's complement */
}

/**
 * @brief Build binary frame with checksum
 */
static size_t priv_build_frame(uint8_t *frame, uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
  size_t idx = 0;

  /* Start byte */
  frame[idx++] = PROTO_START_BYTE;

  /* Length (command + data) */
  uint8_t length = 1 + data_len;
  frame[idx++]   = length;

  /* Command */
  frame[idx++] = cmd;

  /* Data */
  if (data != NULL && data_len > 0) {
    memcpy(&frame[idx], data, data_len);
    idx += data_len;
  }

  /* Checksum (XOR of length + command + data) */
  uint8_t checksum = priv_calc_checksum_xor(&frame[1], length + 1);
  frame[idx++]     = checksum;

  /* Stop byte */
  frame[idx++] = PROTO_STOP_BYTE;

  return idx;
}

/**
 * @brief Parse binary frame and verify checksum
 */
static esp_err_t priv_parse_frame(const uint8_t *frame,
                                  size_t frame_len,
                                  uint8_t *cmd,
                                  uint8_t *data,
                                  uint8_t *data_len)
{
  /* Minimum frame: START + LEN + CMD + CHECKSUM + STOP = 5 bytes */
  if (frame_len < 5) {
    ESP_LOGE(s_tag, "Frame too short: %zu bytes", frame_len);
    return ESP_ERR_INVALID_SIZE;
  }

  /* Verify start byte */
  if (frame[0] != PROTO_START_BYTE) {
    ESP_LOGE(s_tag, "Invalid start byte: 0x%02X", frame[0]);
    return ESP_ERR_INVALID_ARG;
  }

  /* Verify stop byte */
  if (frame[frame_len - 1] != PROTO_STOP_BYTE) {
    ESP_LOGE(s_tag, "Invalid stop byte: 0x%02X", frame[frame_len - 1]);
    return ESP_ERR_INVALID_ARG;
  }

  /* Extract length */
  uint8_t length = frame[1];

  /* Verify frame length */
  if (frame_len != (size_t)(length + 4)) { /* length + start + len + checksum + stop */
    ESP_LOGE(s_tag, "Length mismatch: expected %d, got %zu", length + 4, frame_len);
    return ESP_ERR_INVALID_SIZE;
  }

  /* Extract command */
  *cmd = frame[2];

  /* Extract data */
  *data_len = length - 1;
  if (*data_len > 0) {
    memcpy(data, &frame[3], *data_len);
  }

  /* Verify checksum */
  uint8_t recv_checksum = frame[frame_len - 2];
  uint8_t calc_checksum = priv_calc_checksum_xor(&frame[1], length + 1);

  if (recv_checksum != calc_checksum) {
    ESP_LOGE(s_tag, "Checksum error: expected 0x%02X, got 0x%02X", calc_checksum, recv_checksum);
    return ESP_ERR_INVALID_CRC;
  }

  return ESP_OK;
}

/**
 * @brief Encode 16-bit value (big-endian)
 */
static void priv_encode_uint16_be(uint8_t *buf, uint16_t value)
{
  buf[0] = (value >> 8) & 0xFF;
  buf[1] = value & 0xFF;
}

/**
 * @brief Decode 16-bit value (big-endian)
 */
static uint16_t priv_decode_uint16_be(const uint8_t *buf)
{
  return ((uint16_t)buf[0] << 8) | buf[1];
}

/**
 * @brief Encode 32-bit value (little-endian)
 */
static void priv_encode_uint32_le(uint8_t *buf, uint32_t value)
{
  buf[0] = value & 0xFF;
  buf[1] = (value >> 8) & 0xFF;
  buf[2] = (value >> 16) & 0xFF;
  buf[3] = (value >> 24) & 0xFF;
}

/**
 * @brief Decode 32-bit value (little-endian)
 */
static uint32_t priv_decode_uint32_le(const uint8_t *buf)
{
  return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
         ((uint32_t)buf[3] << 24);
}

/**
 * @brief Send binary frame
 */
static esp_err_t priv_send_frame(star_bus_manager_t *manager,
                                 const char *bus_name,
                                 uint8_t cmd,
                                 const uint8_t *data,
                                 uint8_t data_len)
{
  uint8_t frame[MAX_FRAME_SIZE];
  size_t  frame_len = priv_build_frame(frame, cmd, data, data_len);

  ESP_LOGI(s_tag, "Sending frame (cmd=0x%02X, len=%d):", cmd, data_len);
  ESP_LOG_BUFFER_HEX(s_tag, frame, frame_len);

  esp_err_t ret = star_bus_uart_write(manager, bus_name, frame, frame_len);

  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to send frame: %s", esp_err_to_name(ret));
  }

  return ret;
}

/**
 * @brief Receive binary frame
 */
static esp_err_t priv_receive_frame(star_bus_manager_t *manager,
                                    const char *bus_name,
                                    uint8_t *cmd,
                                    uint8_t *data,
                                    uint8_t *data_len)
{
  uint8_t frame[MAX_FRAME_SIZE];
  size_t  frame_len = 0;

  /* Look for start byte */
  uint8_t byte;
  size_t  read_len;

  for (int i = 0; i < 100; i++) {
    esp_err_t ret = star_bus_uart_read(manager, bus_name, &byte, 1, &read_len, 10);
    if (ret == ESP_OK && read_len > 0 && byte == PROTO_START_BYTE) {
      frame[frame_len++] = byte;
      break;
    }
  }

  if (frame_len == 0) {
    return ESP_ERR_TIMEOUT;
  }

  /* Read length byte */
  esp_err_t ret = star_bus_uart_read(manager, bus_name, &byte, 1, &read_len, 100);
  if (ret != ESP_OK || read_len == 0) {
    return ESP_ERR_TIMEOUT;
  }
  frame[frame_len++] = byte;
  uint8_t payload_len = byte;

  /* Read remaining bytes (payload + checksum + stop) */
  size_t remaining = payload_len + 2;
  ret = star_bus_uart_read(manager, bus_name, &frame[frame_len], remaining, &read_len, 500);
  if (ret != ESP_OK || read_len != remaining) {
    return ESP_ERR_TIMEOUT;
  }
  frame_len += read_len;

  ESP_LOGI(s_tag, "Received frame:");
  ESP_LOG_BUFFER_HEX(s_tag, frame, frame_len);

  /* Parse frame */
  return priv_parse_frame(frame, frame_len, cmd, data, data_len);
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Binary Protocol Example ===\n");

  /* Initialize bus manager */
  star_bus_manager_t manager;
  ret = star_bus_manager_init(&manager, "binary_proto_mgr", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* --- UART Configuration --- */
  star_uart_config_t config = STAR_UART_CONFIG_DEFAULT();
  config.port               = UART_NUM;
  config.tx_pin             = UART_TX_PIN;
  config.rx_pin             = UART_RX_PIN;
  config.baud_rate          = STAR_UART_BAUD_115200;

  ret = star_bus_uart_init(&manager, BUS_NAME, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init UART: %s", esp_err_to_name(ret));
    star_bus_manager_deinit(&manager);
    return;
  }

  ESP_LOGI(s_tag, "Binary protocol UART initialized\n");

  /* --- Example 1: Simple Command --- */
  ESP_LOGI(s_tag, "--- Simple Command (PING) ---");

  priv_send_frame(&manager, BUS_NAME, CMD_PING, NULL, 0);

  /* --- Example 2: Command with Data --- */
  ESP_LOGI(s_tag, "\n--- Command with Data ---");

  uint8_t sensor_id = 0x05;
  priv_send_frame(&manager, BUS_NAME, CMD_READ_SENSOR, &sensor_id, 1);

  /* --- Example 3: Multi-byte Values --- */
  ESP_LOGI(s_tag, "\n--- Multi-byte Values ---");

  uint8_t config_data[6];
  priv_encode_uint16_be(&config_data[0], 1234);  /* Address (big-endian) */
  priv_encode_uint32_le(&config_data[2], 987654); /* Value (little-endian) */

  priv_send_frame(&manager, BUS_NAME, CMD_WRITE_CONFIG, config_data, 6);

  ESP_LOGI(s_tag, "Encoded:");
  ESP_LOGI(s_tag, "  Address (BE): 1234 -> 0x%02X 0x%02X", config_data[0], config_data[1]);
  ESP_LOGI(s_tag, "  Value (LE): 987654 -> 0x%02X 0x%02X 0x%02X 0x%02X",
           config_data[2],
           config_data[3],
           config_data[4],
           config_data[5]);

  /* --- Example 4: Decode Values --- */
  ESP_LOGI(s_tag, "\n--- Decode Values ---");

  uint16_t decoded_addr  = priv_decode_uint16_be(&config_data[0]);
  uint32_t decoded_value = priv_decode_uint32_le(&config_data[2]);

  ESP_LOGI(s_tag, "Decoded:");
  ESP_LOGI(s_tag, "  Address: %u", decoded_addr);
  ESP_LOGI(s_tag, "  Value: %lu", (unsigned long)decoded_value);

  /* --- Example 5: Frame Parsing --- */
  ESP_LOGI(s_tag, "\n--- Frame Parsing ---");

  /* Simulate received frame */
  uint8_t test_frame[] = {0x7E, 0x04, 0x80, 0x01, 0x02, 0x03, 0x82, 0x7F};

  uint8_t parsed_cmd;
  uint8_t parsed_data[32];
  uint8_t parsed_len;

  ret = priv_parse_frame(test_frame, sizeof(test_frame), &parsed_cmd, parsed_data, &parsed_len);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Frame parsed successfully:");
    ESP_LOGI(s_tag, "  Command: 0x%02X", parsed_cmd);
    ESP_LOGI(s_tag, "  Data length: %d", parsed_len);
    if (parsed_len > 0) {
      ESP_LOG_BUFFER_HEX_LEVEL(s_tag, parsed_data, parsed_len, ESP_LOG_INFO);
    }
  }

  /* --- Example 6: Checksum Methods --- */
  ESP_LOGI(s_tag, "\n--- Checksum Methods ---");

  uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

  uint8_t xor_checksum = priv_calc_checksum_xor(test_data, sizeof(test_data));
  uint8_t sum_checksum = priv_calc_checksum_sum(test_data, sizeof(test_data));

  ESP_LOGI(s_tag, "Test data: 01 02 03 04 05");
  ESP_LOGI(s_tag, "XOR checksum: 0x%02X", xor_checksum);
  ESP_LOGI(s_tag, "Sum checksum: 0x%02X", sum_checksum);

  /* --- Protocol Summary --- */
  ESP_LOGI(s_tag, "\n--- Binary Protocol Format ---");
  ESP_LOGI(s_tag, "Frame: [START][LEN][CMD][DATA...][CHECKSUM][STOP]");
  ESP_LOGI(s_tag, "  START: 0x7E");
  ESP_LOGI(s_tag, "  LEN: Command + Data length");
  ESP_LOGI(s_tag, "  CMD: Command byte");
  ESP_LOGI(s_tag, "  DATA: Variable length payload");
  ESP_LOGI(s_tag, "  CHECKSUM: XOR of LEN+CMD+DATA");
  ESP_LOGI(s_tag, "  STOP: 0x7F");

  ESP_LOGI(s_tag, "\n--- Encoding Options ---");
  ESP_LOGI(s_tag, "Multi-byte values:");
  ESP_LOGI(s_tag, "  Big-endian (network byte order): Most significant byte first");
  ESP_LOGI(s_tag, "  Little-endian (Intel byte order): Least significant byte first");

  ESP_LOGI(s_tag, "\nChecksum types:");
  ESP_LOGI(s_tag, "  XOR: Simple and fast, poor error detection");
  ESP_LOGI(s_tag, "  Sum: Better than XOR, still simple");
  ESP_LOGI(s_tag, "  CRC-8: Good error detection, moderate complexity");
  ESP_LOGI(s_tag, "  CRC-16: Excellent error detection (recommended)");

  ESP_LOGI(s_tag, "\n--- Best Practices ---");
  ESP_LOGI(s_tag, "1. Use start/stop bytes for frame synchronization");
  ESP_LOGI(s_tag, "2. Include length field for variable-length frames");
  ESP_LOGI(s_tag, "3. Use CRC-16 for critical applications");
  ESP_LOGI(s_tag, "4. Implement byte stuffing for special characters in data");
  ESP_LOGI(s_tag, "5. Add sequence numbers for reliable delivery");
  ESP_LOGI(s_tag, "6. Use consistent byte order (big or little endian)");
  ESP_LOGI(s_tag, "7. Define clear command/response protocol");
  ESP_LOGI(s_tag, "8. Implement timeouts and retries");

  /* Cleanup */
  star_bus_uart_deinit(&manager, BUS_NAME);
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nBinary protocol example complete");
}
