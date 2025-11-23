/**
 * @file 008_smbus_protocol.c
 * @brief SMBus protocol operations example
 *
 * Demonstrates all SMBus protocol commands:
 * - Quick command
 * - Send/Receive byte
 * - Read/Write byte
 * - Read/Write word
 * - Process call
 * - Block read/write
 * - Block process call
 * - PEC calculation
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>

static const char *s_tag = "SMBUS_PROTOCOL_EXAMPLE";

#define I2C_SDA_PIN   (GPIO_NUM_21)
#define I2C_SCL_PIN   (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)
#define DEVICE_ADDR   (0x50)

static star_bus_manager_t s_manager;

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== SMBus Protocol Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "smbus_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create I2C configuration for SMBus device */
  star_bus_config_t *smbus_config = star_bus_config_create_i2c(
    "smbus_device",
    I2C_NUM_0,
    DEVICE_ADDR,
    I2C_SDA_PIN,
    I2C_SCL_PIN,
    I2C_CLK_SPEED
  );

  if (smbus_config == NULL) {
    ESP_LOGE(s_tag, "Failed to create config");
    return;
  }

  ret = star_bus_manager_add_bus(&s_manager, smbus_config);
  ret = star_bus_config_init(smbus_config, &s_manager);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init SMBus");
    return;
  }

  ESP_LOGI(s_tag, "SMBus initialized");

  /* --- Quick Command --- */
  ESP_LOGI(s_tag, "\n--- Quick Command ---");
  ESP_LOGI(s_tag, "Quick command sends only address + R/W bit");

  ret = star_smbus_quick_command(&s_manager, "smbus_device", DEVICE_ADDR, true);
  ESP_LOGI(s_tag, "Quick command (read): %s", esp_err_to_name(ret));

  ret = star_smbus_quick_command(&s_manager, "smbus_device", DEVICE_ADDR, false);
  ESP_LOGI(s_tag, "Quick command (write): %s", esp_err_to_name(ret));

  /* --- Send/Receive Byte --- */
  ESP_LOGI(s_tag, "\n--- Send/Receive Byte ---");
  ESP_LOGI(s_tag, "Send byte: address + data byte (no command)");
  ESP_LOGI(s_tag, "Receive byte: address + read byte (no command)");

  ret = star_smbus_send_byte(&s_manager, "smbus_device", DEVICE_ADDR, 0x55);
  ESP_LOGI(s_tag, "Send byte 0x55: %s", esp_err_to_name(ret));

  uint8_t received;
  ret = star_smbus_receive_byte(&s_manager, "smbus_device", DEVICE_ADDR,
                                &received);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Receive byte: 0x%02X", received);
  } else {
    ESP_LOGW(s_tag, "Receive byte: %s", esp_err_to_name(ret));
  }

  /* --- Read/Write Byte --- */
  ESP_LOGI(s_tag, "\n--- Read/Write Byte ---");
  ESP_LOGI(s_tag, "Write byte: address + command + data byte");
  ESP_LOGI(s_tag, "Read byte: address + command + read byte");

  ret = star_smbus_write_byte(&s_manager, "smbus_device", DEVICE_ADDR, 0x00,
                              0xAA);
  ESP_LOGI(s_tag, "Write byte 0xAA to cmd 0x00: %s", esp_err_to_name(ret));

  uint8_t read_byte;
  ret = star_smbus_read_byte(&s_manager, "smbus_device", DEVICE_ADDR, 0x00,
                             &read_byte);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read byte from cmd 0x00: 0x%02X", read_byte);
  } else {
    ESP_LOGW(s_tag, "Read byte: %s", esp_err_to_name(ret));
  }

  /* --- Read/Write Word --- */
  ESP_LOGI(s_tag, "\n--- Read/Write Word (16-bit) ---");
  ESP_LOGI(s_tag, "SMBus words are little-endian (LSB first)");

  uint16_t write_word = 0x1234;
  ret = star_smbus_write_word(&s_manager, "smbus_device", DEVICE_ADDR, 0x10,
                              write_word);
  ESP_LOGI(s_tag, "Write word 0x%04X to cmd 0x10: %s", write_word,
           esp_err_to_name(ret));

  uint16_t read_word;
  ret = star_smbus_read_word(&s_manager, "smbus_device", DEVICE_ADDR, 0x10,
                             &read_word);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Read word from cmd 0x10: 0x%04X", read_word);
  } else {
    ESP_LOGW(s_tag, "Read word: %s", esp_err_to_name(ret));
  }

  /* --- Process Call --- */
  ESP_LOGI(s_tag, "\n--- Process Call ---");
  ESP_LOGI(s_tag, "Process call: write word, then read word (atomic)");

  uint16_t send_value = 0xABCD;
  uint16_t response;
  ret = star_smbus_process_call(&s_manager, "smbus_device", DEVICE_ADDR, 0x20,
                                send_value, &response);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Process call: sent 0x%04X, received 0x%04X", send_value,
             response);
  } else {
    ESP_LOGW(s_tag, "Process call: %s", esp_err_to_name(ret));
  }

  /* --- Block Write --- */
  ESP_LOGI(s_tag, "\n--- Block Write ---");
  ESP_LOGI(s_tag, "Block write: command + count + data bytes");

  uint8_t block_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  ret = star_smbus_block_write(&s_manager, "smbus_device", DEVICE_ADDR, 0x30,
                               block_data, sizeof(block_data));
  ESP_LOGI(s_tag, "Block write %d bytes to cmd 0x30: %s",
           (int)sizeof(block_data), esp_err_to_name(ret));

  /* --- Block Read --- */
  ESP_LOGI(s_tag, "\n--- Block Read ---");
  ESP_LOGI(s_tag, "Block read: command + count + data bytes");

  uint8_t block_buffer[32];
  uint8_t block_len;
  ret = star_smbus_block_read(&s_manager, "smbus_device", DEVICE_ADDR, 0x30,
                              block_buffer, 32, &block_len);
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Block read %d bytes from cmd 0x30:", block_len);
    for (int i = 0; i < block_len && i < 8; i++) {
      ESP_LOGI(s_tag, "  [%d] = 0x%02X", i, block_buffer[i]);
    }
  } else {
    ESP_LOGW(s_tag, "Block read: %s", esp_err_to_name(ret));
  }

  /* --- Block Process Call --- */
  ESP_LOGI(s_tag, "\n--- Block Process Call ---");
  ESP_LOGI(s_tag, "Block process call: write block, then read block (atomic)");

  uint8_t send_block[] = {0xAA, 0xBB, 0xCC};
  uint8_t recv_block[32];
  uint8_t recv_len;
  ret = star_smbus_block_process_call(
    &s_manager, "smbus_device", DEVICE_ADDR, 0x40,
    send_block, sizeof(send_block),
    recv_block, 32, &recv_len
  );
  if (ret == ESP_OK) {
    ESP_LOGI(s_tag, "Block process call: sent %d bytes, received %d bytes",
             (int)sizeof(send_block), recv_len);
  } else {
    ESP_LOGW(s_tag, "Block process call: %s", esp_err_to_name(ret));
  }

  /* --- PEC Calculation --- */
  ESP_LOGI(s_tag, "\n--- PEC (Packet Error Checking) ---");
  ESP_LOGI(s_tag, "PEC uses CRC-8 polynomial");

  uint8_t pec_data[] = {DEVICE_ADDR << 1, 0x00, 0x55};  /* addr + cmd + data */
  uint8_t pec = star_smbus_calculate_pec(pec_data, sizeof(pec_data), 0);
  ESP_LOGI(s_tag, "PEC for data: 0x%02X", pec);

  /* Different data */
  pec_data[2] = 0xAA;
  pec = star_smbus_calculate_pec(pec_data, sizeof(pec_data), 0);
  ESP_LOGI(s_tag, "PEC for modified data: 0x%02X", pec);

  /* --- SMBus Command Summary --- */
  ESP_LOGI(s_tag, "\n--- SMBus Command Summary ---");
  ESP_LOGI(s_tag, "Quick Command:     S Addr Rd/Wr A P");
  ESP_LOGI(s_tag, "Send Byte:         S Addr Wr A Data A P");
  ESP_LOGI(s_tag, "Receive Byte:      S Addr Rd A Data N P");
  ESP_LOGI(s_tag, "Write Byte:        S Addr Wr A Comm A Data A P");
  ESP_LOGI(s_tag, "Read Byte:         S Addr Wr A Comm A Sr Addr Rd A Data N P");
  ESP_LOGI(s_tag, "Write Word:        S Addr Wr A Comm A DataL A DataH A P");
  ESP_LOGI(s_tag, "Read Word:         S Addr Wr A Comm A Sr Addr Rd A DataL A DataH N P");
  ESP_LOGI(s_tag, "Process Call:      S Addr Wr A Comm A DataL A DataH A Sr Addr Rd A DataL A DataH N P");
  ESP_LOGI(s_tag, "Block Write:       S Addr Wr A Comm A Count A Data... P");
  ESP_LOGI(s_tag, "Block Read:        S Addr Wr A Comm A Sr Addr Rd A Count A Data... P");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nSMBus protocol example complete");
}
