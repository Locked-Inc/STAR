/**
 * @file mock_rx_bus_onewire.c
 * @brief Mock OneWire bus operations for unit testing
 *
 * Provides controllable mock implementations of OneWire bus operations
 * for testing the DS18B20 driver without hardware dependencies.
 *
 * STAR Project - Texas A&M University
 * January 2026
 */

#include "rx_bus_onewire.h"   /* Real header for function signatures */
#include "mock_rx_bus_onewire.h" /* Mock control functions */
#include "rx_crc.h"
#include <string.h>

/* =============================================================================
 * Mock State
 * =============================================================================
 */

static mock_onewire_state_t s_mock_state = {0};

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_onewire_reset(void)
{
  memset(&s_mock_state, 0, sizeof(s_mock_state));
  s_mock_state.device_present = true; /* Default: device present */
}

void mock_onewire_set_device_present(bool present)
{
  s_mock_state.device_present = present;
}

void mock_onewire_set_scratchpad(const uint8_t scratchpad[9])
{
  memcpy(s_mock_state.scratchpad, scratchpad, 9);
}

void mock_onewire_set_rom(const uint8_t rom[8])
{
  memcpy(s_mock_state.rom, rom, 8);
}

void mock_onewire_set_parasitic_power(bool parasitic)
{
  s_mock_state.parasitic_power = parasitic;
}

void mock_onewire_get_last_command(uint8_t* cmd)
{
  *cmd = s_mock_state.last_command;
}

void mock_onewire_get_write_buffer(uint8_t* buffer, uint32_t* length)
{
  memcpy(buffer, s_mock_state.write_buffer, s_mock_state.write_count);
  *length = s_mock_state.write_count;
}

/* =============================================================================
 * Mock OneWire Bus Operations
 * =============================================================================
 */

rx_err_t rx_bus_onewire_init(rx_bus_manager_t* manager, const char* bus_name)
{
  (void)manager;
  (void)bus_name;
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_reset(rx_bus_manager_t* manager, const char* bus_name, bool* presence)
{
  (void)manager;
  (void)bus_name;

  if (!presence) {
    return k_rx_err_null_pointer;
  }

  *presence = s_mock_state.device_present;
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_write_bit(rx_bus_manager_t* manager, const char* bus_name, bool bit)
{
  (void)manager;
  (void)bus_name;
  (void)bit;
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_read_bit(rx_bus_manager_t* manager, const char* bus_name, bool* bit)
{
  (void)manager;
  (void)bus_name;

  if (!bit) {
    return k_rx_err_null_pointer;
  }

  /* Return parasitic power status when reading power supply */
  if (s_mock_state.last_command == 0xB4) {
    *bit = !s_mock_state.parasitic_power; /* 0 = parasitic, 1 = external */
  } else {
    *bit = false;
  }

  return k_rx_ok;
}

rx_err_t rx_bus_onewire_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t byte)
{
  (void)manager;
  (void)bus_name;

  /* Store command bytes */
  s_mock_state.last_command = byte;

  /* Store to write buffer */
  if (s_mock_state.write_count < sizeof(s_mock_state.write_buffer)) {
    s_mock_state.write_buffer[s_mock_state.write_count++] = byte;
  }

  return k_rx_ok;
}

rx_err_t rx_bus_onewire_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* byte)
{
  (void)manager;
  (void)bus_name;

  if (!byte) {
    return k_rx_err_null_pointer;
  }

  /* Return scratchpad data sequentially */
  if (s_mock_state.read_index < 9) {
    *byte = s_mock_state.scratchpad[s_mock_state.read_index++];
  } else {
    *byte = 0xFF; /* Default read value */
  }

  return k_rx_ok;
}

rx_err_t rx_bus_onewire_write(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               const uint8_t*    data,
                               uint32_t          length)
{
  (void)manager;
  (void)bus_name;

  if (!data || length == 0) {
    return k_rx_err_null_pointer;
  }

  for (uint32_t i = 0; i < length; ++i) {
    rx_bus_onewire_write_byte(manager, bus_name, data[i]);
  }

  return k_rx_ok;
}

rx_err_t rx_bus_onewire_read(rx_bus_manager_t* manager,
                              const char*       bus_name,
                              uint8_t*          data,
                              uint32_t          length)
{
  (void)manager;
  (void)bus_name;

  if (!data || length == 0) {
    return k_rx_err_null_pointer;
  }

  for (uint32_t i = 0; i < length; ++i) {
    rx_err_t err = rx_bus_onewire_read_byte(manager, bus_name, &data[i]);
    if (err != k_rx_ok) {
      return err;
    }
  }

  return k_rx_ok;
}

rx_err_t rx_bus_onewire_skip_rom(rx_bus_manager_t* manager, const char* bus_name)
{
  (void)manager;
  (void)bus_name;

  /* Reset read index when starting new transaction */
  s_mock_state.read_index = 0;

  return k_rx_ok;
}

rx_err_t rx_bus_onewire_match_rom(rx_bus_manager_t* manager,
                                   const char*       bus_name,
                                   const uint8_t     rom[8])
{
  (void)manager;
  (void)bus_name;
  (void)rom;

  /* Reset read index when starting new transaction */
  s_mock_state.read_index = 0;

  return k_rx_ok;
}

rx_err_t rx_bus_onewire_read_rom(rx_bus_manager_t* manager,
                                  const char*       bus_name,
                                  uint8_t           rom[8])
{
  (void)manager;
  (void)bus_name;

  if (!rom) {
    return k_rx_err_null_pointer;
  }

  memcpy(rom, s_mock_state.rom, 8);
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_search(rx_bus_manager_t* manager,
                                const char*       bus_name,
                                uint8_t*          roms,
                                uint32_t          max_devices,
                                uint32_t*         num_devices)
{
  (void)manager;
  (void)bus_name;

  if (!roms || !num_devices) {
    return k_rx_err_null_pointer;
  }

  if (max_devices == 0) {
    *num_devices = 0;
    return k_rx_ok;
  }

  /* Return single device ROM */
  memcpy(roms, s_mock_state.rom, 8);
  *num_devices = 1;

  return k_rx_ok;
}
