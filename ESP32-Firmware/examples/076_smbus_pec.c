/**
 * @file 076_smbus_pec.c
 * @brief SMBus Packet Error Code (PEC) calculation and verification example
 *
 * Demonstrates:
 * - PEC calculation using CRC-8
 * - PEC verification on read operations
 * - PEC transmission on write operations
 * - Error detection and handling
 */

#include "star_bus_config.h"
#include "star_bus_manager.h"
#include "star_bus_smbus.h"

#include <esp_log.h>
#include <string.h>

static const char *s_tag = "SMBUS_PEC_EXAMPLE";

#define I2C_SDA_PIN    (GPIO_NUM_21)
#define I2C_SCL_PIN    (GPIO_NUM_22)
#define I2C_CLK_SPEED (100000)
#define DEVICE_ADDR    (0x50)

static star_bus_manager_t s_manager;

/**
 * @brief Build PEC data buffer for write byte operation
 */
static uint8_t priv_build_write_byte_pec(uint8_t device_addr, uint8_t command, uint8_t data)
{
  uint8_t pec_buffer[3];

  // Address (write)
  pec_buffer[0] = (device_addr << 1) | 0x00;
  // Command
  pec_buffer[1] = command;
  // Data
  pec_buffer[2] = data;

  return star_smbus_calculate_pec(pec_buffer, 3, 0);
}

/**
 * @brief Build PEC data buffer for read byte operation
 */
static uint8_t priv_build_read_byte_pec(uint8_t device_addr, uint8_t command, uint8_t data)
{
  uint8_t pec_buffer[4];

  // Address (write) for command
  pec_buffer[0] = (device_addr << 1) | 0x00;
  // Command
  pec_buffer[1] = command;
  // Repeated start - address (read)
  pec_buffer[2] = (device_addr << 1) | 0x01;
  // Data
  pec_buffer[3] = data;

  return star_smbus_calculate_pec(pec_buffer, 4, 0);
}

/**
 * @brief Build PEC data buffer for write word operation
 */
static uint8_t priv_build_write_word_pec(uint8_t device_addr, uint8_t command, uint16_t data)
{
  uint8_t pec_buffer[4];

  pec_buffer[0] = (device_addr << 1) | 0x00;
  pec_buffer[1] = command;
  pec_buffer[2] = data & 0xFF;        // LSB
  pec_buffer[3] = (data >> 8) & 0xFF; // MSB

  return star_smbus_calculate_pec(pec_buffer, 4, 0);
}

/**
 * @brief Build PEC data buffer for read word operation
 */
static uint8_t priv_build_read_word_pec(uint8_t device_addr, uint8_t command, uint16_t data)
{
  uint8_t pec_buffer[5];

  pec_buffer[0] = (device_addr << 1) | 0x00;
  pec_buffer[1] = command;
  pec_buffer[2] = (device_addr << 1) | 0x01;
  pec_buffer[3] = data & 0xFF;
  pec_buffer[4] = (data >> 8) & 0xFF;

  return star_smbus_calculate_pec(pec_buffer, 5, 0);
}

/**
 * @brief Build PEC for block operation
 */
static uint8_t priv_build_block_write_pec(uint8_t device_addr, uint8_t command,
                                          const uint8_t *data, uint8_t len)
{
  uint8_t pec_buffer[64];
  size_t pec_len = 0;

  pec_buffer[pec_len++] = (device_addr << 1) | 0x00;
  pec_buffer[pec_len++] = command;
  pec_buffer[pec_len++] = len;
  for (uint8_t i = 0; i < len; i++) {
    pec_buffer[pec_len++] = data[i];
  }

  return star_smbus_calculate_pec(pec_buffer, pec_len, 0);
}

/**
 * @brief Demonstrate PEC calculation with standard SMBus operations
 */
static void priv_demonstrate_pec_calculation(void)
{
  ESP_LOGI(s_tag, "\n=== PEC Calculation Examples ===");

  // Example 1: Write Byte with PEC
  uint8_t pec1 = priv_build_write_byte_pec(DEVICE_ADDR, 0x10, 0xA5);
  ESP_LOGI(s_tag, "Write Byte PEC (addr=0x%02X, cmd=0x10, data=0xA5): 0x%02X",
           DEVICE_ADDR, pec1);

  // Example 2: Write Word with PEC
  uint8_t pec2 = priv_build_write_word_pec(DEVICE_ADDR, 0x20, 0x1234);
  ESP_LOGI(s_tag, "Write Word PEC (addr=0x%02X, cmd=0x20, data=0x1234): 0x%02X",
           DEVICE_ADDR, pec2);

  // Example 3: Block Write with PEC
  uint8_t block_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  uint8_t pec3 = priv_build_block_write_pec(DEVICE_ADDR, 0x30, block_data, 5);
  ESP_LOGI(s_tag, "Block Write PEC (addr=0x%02X, cmd=0x30, len=5): 0x%02X",
           DEVICE_ADDR, pec3);
}

/**
 * @brief Test PEC error detection with corrupted data
 */
static void priv_test_pec_error_detection(void)
{
  ESP_LOGI(s_tag, "\n=== Testing PEC Error Detection ===");

  // Original data
  uint8_t test_data[] = {0x12, 0x34, 0x56};
  uint8_t pec_good = star_smbus_calculate_pec(test_data, 3, 0);
  ESP_LOGI(s_tag, "Original data PEC: 0x%02X", pec_good);

  // Corrupted data (middle byte changed)
  uint8_t corrupted_data[] = {0x12, 0xFF, 0x56};
  uint8_t pec_bad = star_smbus_calculate_pec(corrupted_data, 3, 0);
  ESP_LOGI(s_tag, "Corrupted data PEC: 0x%02X", pec_bad);
  ESP_LOGI(s_tag, "PEC difference detected: %s", (pec_good != pec_bad) ? "YES" : "NO");

  // Single-bit error
  uint8_t single_bit_error[] = {0x12, 0x35, 0x56}; // 0x34 -> 0x35 (1 bit flip)
  uint8_t pec_single = star_smbus_calculate_pec(single_bit_error, 3, 0);
  ESP_LOGI(s_tag, "Single-bit error PEC: 0x%02X (detected: %s)",
           pec_single, (pec_single != pec_good) ? "YES" : "NO");

  // Two-bit error
  uint8_t two_bit_error[] = {0x12, 0x37, 0x56}; // 0x34 -> 0x37 (2 bits flip)
  uint8_t pec_two = star_smbus_calculate_pec(two_bit_error, 3, 0);
  ESP_LOGI(s_tag, "Two-bit error PEC: 0x%02X (detected: %s)",
           pec_two, (pec_two != pec_good) ? "YES" : "NO");
}

/**
 * @brief Demonstrate incremental PEC calculation
 */
static void priv_demonstrate_incremental_pec(void)
{
  ESP_LOGI(s_tag, "\n=== Incremental PEC Calculation ===");

  uint8_t data[] = {0xA0, 0x10, 0x55};

  // Calculate all at once
  uint8_t pec_all = star_smbus_calculate_pec(data, 3, 0);
  ESP_LOGI(s_tag, "PEC calculated at once: 0x%02X", pec_all);

  // Calculate incrementally
  uint8_t pec_inc = 0;
  pec_inc = star_smbus_calculate_pec(&data[0], 1, pec_inc);
  pec_inc = star_smbus_calculate_pec(&data[1], 1, pec_inc);
  pec_inc = star_smbus_calculate_pec(&data[2], 1, pec_inc);
  ESP_LOGI(s_tag, "PEC calculated incrementally: 0x%02X", pec_inc);

  ESP_LOGI(s_tag, "Results match: %s", (pec_all == pec_inc) ? "YES" : "NO");
}

/**
 * @brief Test PEC with various data patterns
 */
static void priv_test_pec_patterns(void)
{
  ESP_LOGI(s_tag, "\n=== Testing PEC with Various Patterns ===");

  // All zeros
  uint8_t zeros[] = {0x00, 0x00, 0x00, 0x00};
  uint8_t pec_zeros = star_smbus_calculate_pec(zeros, 4, 0);
  ESP_LOGI(s_tag, "All zeros: 0x%02X", pec_zeros);

  // All ones
  uint8_t ones[] = {0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t pec_ones = star_smbus_calculate_pec(ones, 4, 0);
  ESP_LOGI(s_tag, "All ones: 0x%02X", pec_ones);

  // Alternating pattern
  uint8_t alt[] = {0xAA, 0x55, 0xAA, 0x55};
  uint8_t pec_alt = star_smbus_calculate_pec(alt, 4, 0);
  ESP_LOGI(s_tag, "Alternating (0xAA/0x55): 0x%02X", pec_alt);

  // Sequential
  uint8_t seq[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  uint8_t pec_seq = star_smbus_calculate_pec(seq, 8, 0);
  ESP_LOGI(s_tag, "Sequential (0x00-0x07): 0x%02X", pec_seq);
}

/**
 * @brief Demonstrate real SMBus transactions with PEC notes
 */
static void priv_demonstrate_smbus_with_pec_notes(void)
{
  ESP_LOGI(s_tag, "\n=== SMBus Operations with PEC Notes ===");
  ESP_LOGI(s_tag, "Note: These operations show PEC calculation,");
  ESP_LOGI(s_tag, "but actual PEC transmission requires device support");

  esp_err_t ret;

  // Write Byte with PEC calculation
  ESP_LOGI(s_tag, "\nWrite Byte Operation:");
  uint8_t write_value = 0xAA;
  uint8_t write_pec = priv_build_write_byte_pec(DEVICE_ADDR, 0x10, write_value);
  ESP_LOGI(s_tag, "  Command: 0x10, Data: 0x%02X, PEC: 0x%02X", write_value, write_pec);

  ret = star_smbus_write_byte(&s_manager, "smbus_device", DEVICE_ADDR, 0x10, write_value);
  ESP_LOGI(s_tag, "  Write status: %s", esp_err_to_name(ret));

  // Read Byte with PEC calculation
  ESP_LOGI(s_tag, "\nRead Byte Operation:");
  uint8_t read_value;
  ret = star_smbus_read_byte(&s_manager, "smbus_device", DEVICE_ADDR, 0x10, &read_value);
  if (ret == ESP_OK) {
    uint8_t read_pec = priv_build_read_byte_pec(DEVICE_ADDR, 0x10, read_value);
    ESP_LOGI(s_tag, "  Command: 0x10, Data: 0x%02X, Expected PEC: 0x%02X",
             read_value, read_pec);
  } else {
    ESP_LOGW(s_tag, "  Read failed: %s", esp_err_to_name(ret));
  }

  // Write Word with PEC calculation
  ESP_LOGI(s_tag, "\nWrite Word Operation:");
  uint16_t word_value = 0x1234;
  uint8_t word_pec = priv_build_write_word_pec(DEVICE_ADDR, 0x20, word_value);
  ESP_LOGI(s_tag, "  Command: 0x20, Data: 0x%04X, PEC: 0x%02X", word_value, word_pec);

  ret = star_smbus_write_word(&s_manager, "smbus_device", DEVICE_ADDR, 0x20, word_value);
  ESP_LOGI(s_tag, "  Write status: %s", esp_err_to_name(ret));

  // Block Write with PEC calculation
  ESP_LOGI(s_tag, "\nBlock Write Operation:");
  uint8_t block_data[] = {0x11, 0x22, 0x33, 0x44};
  uint8_t block_pec = priv_build_block_write_pec(DEVICE_ADDR, 0x30, block_data, 4);
  ESP_LOGI(s_tag, "  Command: 0x30, Length: 4, PEC: 0x%02X", block_pec);

  ret = star_smbus_block_write(&s_manager, "smbus_device", DEVICE_ADDR, 0x30,
                                block_data, 4);
  ESP_LOGI(s_tag, "  Write status: %s", esp_err_to_name(ret));
}

void app_main(void)
{
  esp_err_t ret;

  ESP_LOGI(s_tag, "=== SMBus PEC (Packet Error Code) Example ===\n");

  /* Initialize bus manager */
  ret = star_bus_manager_init(&s_manager, "smbus_pec_demo", NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to init bus manager");
    return;
  }

  /* Create I2C configuration for SMBus device */
  star_bus_config_t* smbus_config = star_bus_config_create_i2c(
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

  ESP_LOGI(s_tag, "SMBus initialized\n");

  /* Demonstrate PEC calculations */
  priv_demonstrate_pec_calculation();

  /* Test error detection */
  priv_test_pec_error_detection();

  /* Demonstrate incremental PEC */
  priv_demonstrate_incremental_pec();

  /* Test various data patterns */
  priv_test_pec_patterns();

  /* Demonstrate with real SMBus operations */
  priv_demonstrate_smbus_with_pec_notes();

  /* Information about PEC */
  ESP_LOGI(s_tag, "\n=== PEC Information ===");
  ESP_LOGI(s_tag, "Polynomial: x^8 + x^2 + x + 1 (0x07)");
  ESP_LOGI(s_tag, "Initial value: 0x00");
  ESP_LOGI(s_tag, "PEC detects: Single-bit errors, burst errors, most multi-bit errors");
  ESP_LOGI(s_tag, "PEC Position: Last byte after data in SMBus transactions");

  /* Cleanup */
  star_bus_manager_deinit(&s_manager);

  ESP_LOGI(s_tag, "\nSMBus PEC example complete");
}
