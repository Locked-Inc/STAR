/* esp32-firmware/components/star_bus/star_bus_smbus.c */

#include "star_bus_smbus.h"

#include <esp_err.h>
#include <esp_log.h>
#include <string.h>

#include "star_bus_i2c.h"
#include "star_bus_manager.h"

/* --- Constants --- */

static const char* s_TAG = "STAR_SMBUS";

/* CRC-8 polynomial for SMBus PEC: x^8 + x^2 + x + 1 */
#define STAR_SMBUS_PEC_POLYNOMIAL (0x07)

/* --- Helper Functions --- */

/**
 * @brief Validate common parameters for SMBus operations
 */
static esp_err_t priv_validate_smbus_params(star_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL) {
    ESP_LOGE(s_TAG, "Manager is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  if (bus_name == NULL) {
    ESP_LOGE(s_TAG, "Bus name is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  return ESP_OK;
}

/* --- Public Functions --- */

/* Note: addr parameter is kept for API compatibility but currently unused.
 * The address from the bus configuration is used by the underlying I2C layer. */

esp_err_t star_smbus_quick_command(star_bus_manager_t* manager,
                                   const char*         bus_name,
                                   uint8_t             addr,
                                   bool                write)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  /* Quick command: write=send command byte 0, read=read raw with 0 length */
  if (write) {
    return star_bus_i2c_write_command(manager, bus_name, 0);
  } else {
    uint8_t dummy;
    return star_bus_i2c_read_raw(manager, bus_name, &dummy, 0, NULL);
  }
}

esp_err_t
star_smbus_send_byte(star_bus_manager_t* manager, const char* bus_name, uint8_t addr, uint8_t data)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  return star_bus_i2c_write_command(manager, bus_name, data);
}

esp_err_t star_smbus_receive_byte(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  uint8_t             addr,
                                  uint8_t*            data)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  if (data == NULL) {
    ESP_LOGE(s_TAG, "Data pointer is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  return star_bus_i2c_read_raw(manager, bus_name, data, 1, NULL);
}

esp_err_t star_smbus_write_byte(star_bus_manager_t* manager,
                                const char*         bus_name,
                                uint8_t             addr,
                                uint8_t             command,
                                uint8_t             data)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  return star_bus_i2c_write(manager, bus_name, &data, 1, command, NULL);
}

esp_err_t star_smbus_read_byte(star_bus_manager_t* manager,
                               const char*         bus_name,
                               uint8_t             addr,
                               uint8_t             command,
                               uint8_t*            data)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  if (data == NULL) {
    ESP_LOGE(s_TAG, "Data pointer is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  return star_bus_i2c_read(manager, bus_name, data, 1, command, NULL);
}

esp_err_t star_smbus_write_word(star_bus_manager_t* manager,
                                const char*         bus_name,
                                uint8_t             addr,
                                uint8_t             command,
                                uint16_t            data)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  /* SMBus word format: low byte, high byte (little-endian) */
  uint8_t buffer[2] = {
    (uint8_t)(data & 0xFF),        /* Low byte */
    (uint8_t)((data >> 8) & 0xFF), /* High byte */
  };

  return star_bus_i2c_write(manager, bus_name, buffer, 2, command, NULL);
}

esp_err_t star_smbus_read_word(star_bus_manager_t* manager,
                               const char*         bus_name,
                               uint8_t             addr,
                               uint8_t             command,
                               uint16_t*           data)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  if (data == NULL) {
    ESP_LOGE(s_TAG, "Data pointer is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  /* Read two bytes (little-endian) */
  uint8_t buffer[2];
  result = star_bus_i2c_read(manager, bus_name, buffer, 2, command, NULL);
  if (result != ESP_OK) {
    return result;
  }

  /* Combine bytes: low byte + (high byte << 8) */
  *data = (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);

  return ESP_OK;
}

esp_err_t star_smbus_process_call(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  uint8_t             addr,
                                  uint8_t             command,
                                  uint16_t            write_data,
                                  uint16_t*           read_data)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  if (read_data == NULL) {
    ESP_LOGE(s_TAG, "Read data pointer is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  /* Write word (little-endian) */
  uint8_t write_buffer[2] = {
    (uint8_t)(write_data & 0xFF),        /* Low byte */
    (uint8_t)((write_data >> 8) & 0xFF), /* High byte */
  };

  result = star_bus_i2c_write(manager, bus_name, write_buffer, 2, command, NULL);
  if (result != ESP_OK) {
    return result;
  }

  /* Read response word (little-endian) - use read_raw since no new command */
  uint8_t read_buffer[2];
  result = star_bus_i2c_read_raw(manager, bus_name, read_buffer, 2, NULL);
  if (result != ESP_OK) {
    return result;
  }

  /* Combine bytes: low byte + (high byte << 8) */
  *read_data = (uint16_t)read_buffer[0] | ((uint16_t)read_buffer[1] << 8);

  return ESP_OK;
}

esp_err_t star_smbus_block_write(star_bus_manager_t* manager,
                                 const char*         bus_name,
                                 uint8_t             addr,
                                 uint8_t             command,
                                 const uint8_t*      data,
                                 uint8_t             length)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  if (data == NULL) {
    ESP_LOGE(s_TAG, "Data pointer is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  if (length == 0 || length > STAR_SMBUS_MAX_BLOCK_SIZE) {
    ESP_LOGE(s_TAG, "Invalid block length: %d (must be 1-%d)", length, STAR_SMBUS_MAX_BLOCK_SIZE);
    return ESP_ERR_INVALID_ARG;
  }

  /* Build buffer: length + data bytes */
  uint8_t buffer[1 + STAR_SMBUS_MAX_BLOCK_SIZE];
  buffer[0] = length;
  memcpy(&buffer[1], data, length);

  return star_bus_i2c_write(manager, bus_name, buffer, 1 + length, command, NULL);
}

esp_err_t star_smbus_block_read(star_bus_manager_t* manager,
                                const char*         bus_name,
                                uint8_t             addr,
                                uint8_t             command,
                                uint8_t*            data,
                                uint8_t             max_length,
                                uint8_t*            length)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  if (data == NULL || length == NULL) {
    ESP_LOGE(s_TAG, "Data or length pointer is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  if (max_length < STAR_SMBUS_MAX_BLOCK_SIZE) {
    ESP_LOGE(s_TAG,
             "Buffer too small: %d (should be >= %d)",
             max_length,
             STAR_SMBUS_MAX_BLOCK_SIZE);
    return ESP_ERR_INVALID_ARG;
  }

  /* Write command, then read length + data */
  result = star_bus_i2c_write_command(manager, bus_name, command);
  if (result != ESP_OK) {
    return result;
  }

  /* Read length byte first */
  uint8_t block_length;
  result = star_bus_i2c_read_raw(manager, bus_name, &block_length, 1, NULL);
  if (result != ESP_OK) {
    return result;
  }

  /* Validate block length */
  if (block_length == 0 || block_length > STAR_SMBUS_MAX_BLOCK_SIZE) {
    ESP_LOGE(s_TAG, "Invalid block length received: %d", block_length);
    return ESP_ERR_INVALID_RESPONSE;
  }

  /* Read block data */
  result = star_bus_i2c_read_raw(manager, bus_name, data, block_length, NULL);
  if (result != ESP_OK) {
    return result;
  }

  *length = block_length;
  return ESP_OK;
}

esp_err_t star_smbus_block_process_call(star_bus_manager_t* manager,
                                        const char*         bus_name,
                                        uint8_t             addr,
                                        uint8_t             command,
                                        const uint8_t*      write_data,
                                        uint8_t             write_length,
                                        uint8_t*            read_data,
                                        uint8_t             max_read_len,
                                        uint8_t*            read_length)
{
  (void)addr; /* Address comes from bus config */

  esp_err_t result = priv_validate_smbus_params(manager, bus_name);
  if (result != ESP_OK) {
    return result;
  }

  if (write_data == NULL || read_data == NULL || read_length == NULL) {
    ESP_LOGE(s_TAG, "Null pointer in block process call parameters");
    return ESP_ERR_INVALID_ARG;
  }

  if (write_length == 0 || write_length > STAR_SMBUS_MAX_BLOCK_SIZE) {
    ESP_LOGE(s_TAG,
             "Invalid write length: %d (must be 1-%d)",
             write_length,
             STAR_SMBUS_MAX_BLOCK_SIZE);
    return ESP_ERR_INVALID_ARG;
  }

  if (max_read_len < STAR_SMBUS_MAX_BLOCK_SIZE) {
    ESP_LOGE(s_TAG,
             "Read buffer too small: %d (should be >= %d)",
             max_read_len,
             STAR_SMBUS_MAX_BLOCK_SIZE);
    return ESP_ERR_INVALID_ARG;
  }

  /* Build write buffer: length + data */
  uint8_t write_buffer[1 + STAR_SMBUS_MAX_BLOCK_SIZE];
  write_buffer[0] = write_length;
  memcpy(&write_buffer[1], write_data, write_length);

  /* Write command, length, and data */
  result = star_bus_i2c_write(manager, bus_name, write_buffer, 1 + write_length, command, NULL);
  if (result != ESP_OK) {
    return result;
  }

  /* Read response length byte */
  uint8_t response_length;
  result = star_bus_i2c_read_raw(manager, bus_name, &response_length, 1, NULL);
  if (result != ESP_OK) {
    return result;
  }

  /* Validate response length */
  if (response_length == 0 || response_length > STAR_SMBUS_MAX_BLOCK_SIZE) {
    ESP_LOGE(s_TAG, "Invalid response length: %d", response_length);
    return ESP_ERR_INVALID_RESPONSE;
  }

  /* Read response data */
  result = star_bus_i2c_read_raw(manager, bus_name, read_data, response_length, NULL);
  if (result != ESP_OK) {
    return result;
  }

  *read_length = response_length;
  return ESP_OK;
}

/* --- PEC (Packet Error Checking) Functions --- */

uint8_t star_smbus_calculate_pec(const uint8_t* data, size_t length, uint8_t crc)
{
  if (data == NULL) {
    return crc;
  }

  /* CRC-8 calculation using polynomial 0x07 */
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];

    for (uint32_t bit = 0; bit < 8; bit++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ STAR_SMBUS_PEC_POLYNOMIAL;
      } else {
        crc = (crc << 1);
      }
    }
  }

  return crc;
}
