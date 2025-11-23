/**
 * @file 071_uart_modbus.c
 * @brief Modbus RTU implementation example
 *
 * Demonstrates:
 * - Modbus RTU protocol implementation
 * - CRC calculation and verification
 * - Function codes (Read/Write registers)
 * - Master and slave communication
 * - Error handling
 */

#include "star_bus_uart.h"
#include "star_bus_manager.h"

#include <esp_log.h>
#include <string.h>

static const char *s_tag = "UART_MODBUS";

#define UART_TX_PIN   (GPIO_NUM_17)
#define UART_RX_PIN   (GPIO_NUM_16)
#define UART_RTS_PIN  (GPIO_NUM_18)
#define UART_NUM      (UART_NUM_1)

/* Modbus function codes */
#define MODBUS_FC_READ_COILS (0x01)
#define MODBUS_FC_READ_DISCRETE_INPUTS (0x02)
#define MODBUS_FC_READ_HOLDING_REGS (0x03)
#define MODBUS_FC_READ_INPUT_REGS (0x04)
#define MODBUS_FC_WRITE_SINGLE_COIL (0x05)
#define MODBUS_FC_WRITE_SINGLE_REG (0x06)
#define MODBUS_FC_WRITE_MULTIPLE_COILS (0x0F)
#define MODBUS_FC_WRITE_MULTIPLE_REGS (0x10)

/* Modbus exception codes */
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION (0x01)
#define MODBUS_EXCEPTION_ILLEGAL_ADDRESS (0x02)
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE (0x03)
#define MODBUS_EXCEPTION_SLAVE_DEVICE_FAIL (0x04)

/**
 * @brief Calculate Modbus RTU CRC16
 */
static uint16_t priv_modbus_crc16(const uint8_t *data, size_t length)
{
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

/**
 * @brief Send Modbus RTU request
 */
static esp_err_t priv_modbus_send_request(star_bus_manager_t *manager,
                                          const char *bus_name,
                                          uint8_t slave_addr,
                                          uint8_t function_code,
                                          const uint8_t *data,
                                          size_t data_len)
{
  uint8_t frame[256];
  size_t  frame_len = 0;

  /* Build frame: Slave Address + Function Code + Data */
  frame[frame_len++] = slave_addr;
  frame[frame_len++] = function_code;

  if (data != NULL && data_len > 0) {
    memcpy(&frame[frame_len], data, data_len);
    frame_len += data_len;
  }

  /* Calculate and append CRC (little-endian) */
  uint16_t crc       = priv_modbus_crc16(frame, frame_len);
  frame[frame_len++] = crc & 0xFF;
  frame[frame_len++] = (crc >> 8) & 0xFF;

  /* Send frame */
  esp_err_t ret = star_bus_uart_write(manager, bus_name, frame, frame_len);

  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Sent Modbus request:");
    ESP_LOG_BUFFER_HEX(s_tag, frame, frame_len);
  }

  return ret;
}

/**
 * @brief Receive Modbus RTU response
 */
static esp_err_t priv_modbus_receive_response(star_bus_manager_t *manager,
                                              const char *bus_name,
                                              uint8_t *buffer,
                                              size_t buffer_size,
                                              size_t *received_len,
                                              uint32_t timeout_ms)
{
  esp_err_t ret =
    star_bus_uart_read(manager, bus_name, buffer, buffer_size, received_len, timeout_ms);

  if (ret != ESP_OK || *received_len < 4) {
    return ESP_ERR_TIMEOUT;
  }

  /* Verify CRC */
  uint16_t recv_crc = buffer[*received_len - 2] | (buffer[*received_len - 1] << 8);
  uint16_t calc_crc = priv_modbus_crc16(buffer, *received_len - 2);

  if (recv_crc != calc_crc) {
    ESP_LOGE(s_tag, "CRC error: expected 0x%04X, got 0x%04X", calc_crc, recv_crc);
    return ESP_ERR_INVALID_CRC;
  }

  ESP_LOGI(s_tag, "Received valid Modbus response:");
  ESP_LOG_BUFFER_HEX(s_tag, buffer, *received_len);

  return ESP_OK;
}

/**
 * @brief Read holding registers (Function Code 0x03)
 */
static esp_err_t priv_modbus_read_holding_regs(star_bus_manager_t *manager,
                                               const char *bus_name,
                                               uint8_t slave_addr,
                                               uint16_t start_addr,
                                               uint16_t num_regs,
                                               uint16_t *registers)
{
  /* Build request data */
  uint8_t request[4] = {(start_addr >> 8) & 0xFF,
                        start_addr & 0xFF,
                        (num_regs >> 8) & 0xFF,
                        num_regs & 0xFF};

  /* Clear RX buffer */
  star_bus_uart_clear_rx(manager, bus_name);

  /* Send request */
  esp_err_t ret = priv_modbus_send_request(
    manager, bus_name, slave_addr, MODBUS_FC_READ_HOLDING_REGS, request, sizeof(request));
  if (ret != ESP_OK) {
    return ret;
  }

  /* Wait for response */
  uint8_t response[256];
  size_t  resp_len;
  ret = priv_modbus_receive_response(manager, bus_name, response, sizeof(response), &resp_len, 1000);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Parse response */
  if (response[0] != slave_addr) {
    ESP_LOGE(s_tag, "Slave address mismatch");
    return ESP_FAIL;
  }

  /* Check for exception */
  if (response[1] & 0x80) {
    ESP_LOGE(s_tag, "Modbus exception: 0x%02X", response[2]);
    return ESP_FAIL;
  }

  if (response[1] != MODBUS_FC_READ_HOLDING_REGS) {
    ESP_LOGE(s_tag, "Function code mismatch");
    return ESP_FAIL;
  }

  uint8_t byte_count = response[2];
  if (byte_count != num_regs * 2) {
    ESP_LOGE(s_tag, "Byte count mismatch");
    return ESP_FAIL;
  }

  /* Extract register values (big-endian) */
  for (int i = 0; i < num_regs; i++) {
    registers[i] = (response[3 + i * 2] << 8) | response[4 + i * 2];
  }

  return ESP_OK;
}

/**
 * @brief Write single register (Function Code 0x06)
 */
static esp_err_t priv_modbus_write_single_reg(star_bus_manager_t *manager,
                                              const char *bus_name,
                                              uint8_t slave_addr,
                                              uint16_t reg_addr,
                                              uint16_t value)
{
  /* Build request data */
  uint8_t request[4] = {(reg_addr >> 8) & 0xFF,
                        reg_addr & 0xFF,
                        (value >> 8) & 0xFF,
                        value & 0xFF};

  /* Clear RX buffer */
  star_bus_uart_clear_rx(manager, bus_name);

  /* Send request */
  esp_err_t ret = priv_modbus_send_request(
    manager, bus_name, slave_addr, MODBUS_FC_WRITE_SINGLE_REG, request, sizeof(request));
  if (ret != ESP_OK) {
    return ret;
  }

  /* Wait for echo response */
  uint8_t response[256];
  size_t  resp_len;
  ret = priv_modbus_receive_response(manager, bus_name, response, sizeof(response), &resp_len, 1000);
  if (ret != ESP_OK) {
    return ret;
  }

  /* Verify echo */
  if (memcmp(request, &response[2], 4) != 0) {
    ESP_LOGE(s_tag, "Echo mismatch");
    return ESP_FAIL;
  }

  return ESP_OK;
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== Modbus RTU Example ===\n");

  /* --- RS-485 Configuration for Modbus --- */
  ESP_LOGI(s_tag, "--- Modbus Configuration ---");

  star_bus_manager_t manager;
  star_bus_manager_init(&manager, "modbus_demo", NULL, NULL);

  star_uart_config_t config = STAR_UART_CONFIG_DEFAULT();
  config.port               = UART_NUM;
  config.tx_pin             = UART_TX_PIN;
  config.rx_pin             = UART_RX_PIN;
  config.rts_pin            = UART_RTS_PIN;
  config.baud_rate          = STAR_UART_BAUD_9600;  /* Common Modbus baud rate */
  config.mode               = STAR_UART_MODE_RS485;

  ESP_LOGI(s_tag, "Mode: RS-485 (Modbus RTU)");
  ESP_LOGI(s_tag, "Baud rate: 9600");
  ESP_LOGI(s_tag, "Format: 8N1 (8 data bits, no parity, 1 stop bit)");

  ret = star_bus_uart_init(&manager, "uart_modbus", &config);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init UART: %s", esp_err_to_name(ret));
    return;
  }

  ESP_LOGI(s_tag, "Modbus RTU initialized\n");

  /* --- Example 1: Read Holding Registers --- */
  ESP_LOGI(s_tag, "--- Read Holding Registers ---");

  uint16_t registers[10];
  ret = priv_modbus_read_holding_regs(&manager, "uart_modbus", 0x01, 0x0000, 5, registers);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Successfully read 5 registers:");
    for (int i = 0; i < 5; i++) {
      ESP_LOGI(s_tag, "  Register[%d] = %u (0x%04X)", i, registers[i], registers[i]);
    }
  } else {
    ESP_LOGW(s_tag, "Failed to read registers: %s", esp_err_to_name(ret));
  }

  /* --- Example 2: Write Single Register --- */
  ESP_LOGI(s_tag, "\n--- Write Single Register ---");

  ret = priv_modbus_write_single_reg(&manager, "uart_modbus", 0x01, 0x0000, 1234);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Successfully wrote value 1234 to register 0");
  } else {
    ESP_LOGW(s_tag, "Failed to write register: %s", esp_err_to_name(ret));
  }

  /* --- Example 3: CRC Calculation --- */
  ESP_LOGI(s_tag, "\n--- CRC Calculation ---");

  uint8_t test_frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  uint16_t crc         = priv_modbus_crc16(test_frame, sizeof(test_frame));
  ESP_LOGI(s_tag, "Frame: 01 03 00 00 00 0A");
  ESP_LOGI(s_tag, "CRC: 0x%04X (bytes: 0x%02X 0x%02X)", crc, crc & 0xFF, (crc >> 8) & 0xFF);

  /* --- Modbus Protocol Summary --- */
  ESP_LOGI(s_tag, "\n--- Modbus RTU Protocol ---");
  ESP_LOGI(s_tag, "Frame format: [ADDR][FC][DATA...][CRC_L][CRC_H]");
  ESP_LOGI(s_tag, "Common baud rates: 9600, 19200, 38400, 115200");
  ESP_LOGI(s_tag, "Silent interval: 3.5 character times");
  ESP_LOGI(s_tag, "CRC: CRC-16 (polynomial 0xA001)");
  ESP_LOGI(s_tag, "Byte order: Big-endian for data, little-endian for CRC");

  ESP_LOGI(s_tag, "\nCommon function codes:");
  ESP_LOGI(s_tag, "  0x01 - Read Coils");
  ESP_LOGI(s_tag, "  0x02 - Read Discrete Inputs");
  ESP_LOGI(s_tag, "  0x03 - Read Holding Registers");
  ESP_LOGI(s_tag, "  0x04 - Read Input Registers");
  ESP_LOGI(s_tag, "  0x05 - Write Single Coil");
  ESP_LOGI(s_tag, "  0x06 - Write Single Register");
  ESP_LOGI(s_tag, "  0x0F - Write Multiple Coils");
  ESP_LOGI(s_tag, "  0x10 - Write Multiple Registers");

  /* Cleanup */
  star_bus_uart_deinit(&manager, "uart_modbus");
  star_bus_manager_deinit(&manager);

  ESP_LOGI(s_tag, "\nModbus RTU example complete");
}
