/* tests/mocks/mock_rx_bus_smbus.c */

/**
 * @file mock_rx_bus_smbus.c
 * @brief Mock SMBus Driver Implementation for Unit Testing BQ4050
 * @details
 * Provides mock implementation of SMBus driver for host-side testing.
 * Replaces rx_bus_smbus.c for testing BQ4050 driver.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_bus_smbus.h"

#include <string.h>

#include "rx_bus_manager.h"
#include "rx_bus_smbus.h"

/* =============================================================================
 * Static State
 * =============================================================================
 */

/**
 * @brief Mock register storage (word values)
 */
static uint16_t s_register_values[k_mock_smbus_max_regs] = {0};

/**
 * @brief Error to inject for specific commands (k_rx_ok = no error)
 */
static rx_err_t s_command_errors[k_mock_smbus_max_regs] = {k_rx_ok};

/**
 * @brief Error to inject on next operation (one-shot)
 */
static rx_err_t s_next_error = k_rx_ok;

/**
 * @brief Bus initialized state
 */
static bool s_initialized = false;

/**
 * @brief Init was called flag
 */
static bool s_init_called = false;

/**
 * @brief Count of read operations
 */
static uint32_t s_read_count = 0;

/**
 * @brief Last command code that was read
 */
static uint8_t s_last_command = 0;

/* =============================================================================
 * Mock Configuration Functions
 * =============================================================================
 */

void mock_smbus_reset(void)
{
  memset(s_register_values, 0, sizeof(s_register_values));
  memset(s_command_errors, 0, sizeof(s_command_errors));
  s_next_error   = k_rx_ok;
  s_initialized  = false;
  s_init_called  = false;
  s_read_count   = 0;
  s_last_command = 0;
}

void mock_smbus_set_word_response(uint8_t command, uint16_t value)
{
  s_register_values[command] = value;
}

void mock_smbus_set_byte_response(uint8_t command, uint8_t value)
{
  s_register_values[command] = (uint16_t)value;
}

void mock_smbus_set_next_error(rx_err_t err)
{
  s_next_error = err;
}

void mock_smbus_set_command_error(uint8_t command, rx_err_t err)
{
  s_command_errors[command] = err;
}

void mock_smbus_clear_command_error(uint8_t command)
{
  s_command_errors[command] = k_rx_ok;
}

void mock_smbus_set_initialized(bool initialized)
{
  s_initialized = initialized;
}

uint32_t mock_smbus_get_read_count(void)
{
  return s_read_count;
}

uint8_t mock_smbus_get_last_command(void)
{
  return s_last_command;
}

bool mock_smbus_was_init_called(void)
{
  return s_init_called;
}

/* =============================================================================
 * Mock SMBus API Implementation
 * =============================================================================
 */

rx_err_t rx_bus_smbus_init(rx_bus_manager_t* manager, const char* bus_name)
{
  if (manager == NULL || bus_name == NULL) {
    return k_rx_err_null_pointer;
  }

  s_init_called = true;

  /* Check for one-shot error */
  if (s_next_error != k_rx_ok) {
    rx_err_t err = s_next_error;
    s_next_error = k_rx_ok;
    return err;
  }

  s_initialized = true;
  return k_rx_ok;
}

rx_err_t rx_bus_smbus_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t command)
{
  if (manager == NULL || bus_name == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for one-shot error */
  if (s_next_error != k_rx_ok) {
    rx_err_t err = s_next_error;
    s_next_error = k_rx_ok;
    return err;
  }

  /* Check for command-specific error */
  if (s_command_errors[command] != k_rx_ok) {
    return s_command_errors[command];
  }

  return k_rx_ok;
}

rx_err_t rx_bus_smbus_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data)
{
  if (manager == NULL || bus_name == NULL || data == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for one-shot error */
  if (s_next_error != k_rx_ok) {
    rx_err_t err = s_next_error;
    s_next_error = k_rx_ok;
    return err;
  }

  s_read_count++;
  *data = 0;

  return k_rx_ok;
}

rx_err_t rx_bus_smbus_write_byte_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint8_t           data)
{
  if (manager == NULL || bus_name == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for one-shot error */
  if (s_next_error != k_rx_ok) {
    rx_err_t err = s_next_error;
    s_next_error = k_rx_ok;
    return err;
  }

  /* Check for command-specific error */
  if (s_command_errors[command] != k_rx_ok) {
    return s_command_errors[command];
  }

  /* Store value */
  s_register_values[command] = (uint16_t)data;

  return k_rx_ok;
}

rx_err_t rx_bus_smbus_read_byte_data(rx_bus_manager_t* manager,
                                     const char*       bus_name,
                                     uint8_t           command,
                                     uint8_t*          data)
{
  if (manager == NULL || bus_name == NULL || data == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for one-shot error */
  if (s_next_error != k_rx_ok) {
    rx_err_t err = s_next_error;
    s_next_error = k_rx_ok;
    return err;
  }

  /* Check for command-specific error */
  if (s_command_errors[command] != k_rx_ok) {
    return s_command_errors[command];
  }

  s_read_count++;
  s_last_command = command;
  *data          = (uint8_t)s_register_values[command];

  return k_rx_ok;
}

rx_err_t rx_bus_smbus_write_word_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint16_t          data)
{
  if (manager == NULL || bus_name == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for one-shot error */
  if (s_next_error != k_rx_ok) {
    rx_err_t err = s_next_error;
    s_next_error = k_rx_ok;
    return err;
  }

  /* Check for command-specific error */
  if (s_command_errors[command] != k_rx_ok) {
    return s_command_errors[command];
  }

  /* Store value */
  s_register_values[command] = data;

  return k_rx_ok;
}

rx_err_t rx_bus_smbus_read_word_data(rx_bus_manager_t* manager,
                                     const char*       bus_name,
                                     uint8_t           command,
                                     uint16_t*         data)
{
  if (manager == NULL || bus_name == NULL || data == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for one-shot error */
  if (s_next_error != k_rx_ok) {
    rx_err_t err = s_next_error;
    s_next_error = k_rx_ok;
    return err;
  }

  /* Check for command-specific error */
  if (s_command_errors[command] != k_rx_ok) {
    return s_command_errors[command];
  }

  s_read_count++;
  s_last_command = command;
  *data          = s_register_values[command];

  return k_rx_ok;
}

rx_err_t rx_bus_smbus_read_block_data(rx_bus_manager_t* manager,
                                      const char*       bus_name,
                                      uint8_t           command,
                                      uint8_t*          data,
                                      uint8_t*          length,
                                      uint8_t           max_length)
{
  if (manager == NULL || bus_name == NULL || data == NULL || length == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!s_initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for one-shot error */
  if (s_next_error != k_rx_ok) {
    rx_err_t err = s_next_error;
    s_next_error = k_rx_ok;
    return err;
  }

  /* Check for command-specific error */
  if (s_command_errors[command] != k_rx_ok) {
    return s_command_errors[command];
  }

  s_read_count++;
  s_last_command = command;

  /* Return empty block */
  *length = 0;
  (void)max_length;

  return k_rx_ok;
}
