/**
 * @file mock_rx_bus_onewire.h
 * @brief Mock OneWire bus operations for unit testing
 *
 * Provides controllable mock implementations of OneWire bus operations
 * for testing the DS18B20 driver without hardware dependencies.
 *
 * NOTE: This header provides only the mock control functions.
 * The actual rx_bus_onewire_* function declarations come from rx_bus_onewire.h
 *
 * STAR Project - Texas A&M University
 * January 2026
 */

#ifndef MOCK_RX_BUS_ONEWIRE_H
#define MOCK_RX_BUS_ONEWIRE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock State Structure
 * =============================================================================
 */

/**
 * @brief Mock OneWire bus state
 *
 * Allows tests to control mock behavior and inspect operations.
 */
typedef struct {
  bool     device_present;     /**< True if device should respond to reset */
  uint8_t  scratchpad[9];      /**< Mock scratchpad data */
  uint8_t  rom[8];             /**< Mock ROM address */
  bool     parasitic_power;    /**< True if parasitic power mode */
  uint8_t  last_command;       /**< Last command written */
  uint8_t  write_buffer[32];   /**< Buffer of written bytes */
  uint32_t write_count;        /**< Number of bytes written */
  uint32_t read_index;         /**< Current read position in scratchpad */
} mock_onewire_state_t;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Reset mock state to defaults
 *
 * Clears all mock state and sets device_present = true.
 */
void mock_onewire_reset(void);

/**
 * @brief Set whether device responds to reset pulse
 *
 * @param[in] present True if device should respond
 */
void mock_onewire_set_device_present(bool present);

/**
 * @brief Set mock scratchpad data
 *
 * @param[in] scratchpad 9-byte scratchpad data (including CRC)
 */
void mock_onewire_set_scratchpad(const uint8_t scratchpad[9]);

/**
 * @brief Set mock ROM address
 *
 * @param[in] rom 8-byte ROM address (including CRC)
 */
void mock_onewire_set_rom(const uint8_t rom[8]);

/**
 * @brief Set parasitic power mode
 *
 * @param[in] parasitic True for parasitic power
 */
void mock_onewire_set_parasitic_power(bool parasitic);

/**
 * @brief Get last command byte written
 *
 * @param[out] cmd Last command byte
 */
void mock_onewire_get_last_command(uint8_t* cmd);

/**
 * @brief Get all bytes written to bus
 *
 * @param[out] buffer Buffer to copy written bytes
 * @param[out] length Number of bytes written
 */
void mock_onewire_get_write_buffer(uint8_t* buffer, uint32_t* length);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RX_BUS_ONEWIRE_H */
