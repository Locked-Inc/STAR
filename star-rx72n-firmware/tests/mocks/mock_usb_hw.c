/* tests/mocks/mock_usb_hw.c */

/**
 * @file mock_usb_hw.c
 * @brief Mock USB Hardware Layer Implementation
 *
 * Implements the mock USB hardware layer for host-side testing.
 * This file provides mock implementations of rx_usb_hw_* functions
 * that can be linked instead of the real hardware implementations.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_usb_hw.h"

#include <string.h>

/* =============================================================================
 * Global Mock Instance
 * =============================================================================
 */

mock_usb_hw_t g_mock_usb_hw;

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

static mock_usb_hw_t* internal_get_mock(mock_usb_hw_t* mock)
{
  return (mock != NULL) ? mock : &g_mock_usb_hw;
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

rx_err_t mock_usb_hw_init(mock_usb_hw_t* mock)
{
  mock_usb_hw_t* m = internal_get_mock(mock);

  memset(m, 0, sizeof(mock_usb_hw_t));

  /* Set default return values (success) */
  m->next_init_return   = k_rx_ok;
  m->next_attach_return = k_rx_ok;
  m->fifo_ready         = true;
  m->bus_state          = k_usb_state_detached;

  return k_rx_ok;
}

rx_err_t mock_usb_hw_deinit(mock_usb_hw_t* mock)
{
  mock_usb_hw_t* m = internal_get_mock(mock);
  memset(m, 0, sizeof(mock_usb_hw_t));
  return k_rx_ok;
}

rx_err_t mock_usb_hw_clear(mock_usb_hw_t* mock)
{
  mock_usb_hw_t* m = internal_get_mock(mock);

  /* Preserve configuration but clear state */
  rx_err_t saved_init_ret   = m->next_init_return;
  rx_err_t saved_attach_ret = m->next_attach_return;
  bool     saved_fifo_ready = m->fifo_ready;

  memset(m, 0, sizeof(mock_usb_hw_t));

  m->next_init_return   = saved_init_ret;
  m->next_attach_return = saved_attach_ret;
  m->fifo_ready         = saved_fifo_ready;

  return k_rx_ok;
}

/* =============================================================================
 * Error Injection Functions
 * =============================================================================
 */

void mock_usb_hw_set_init_return(mock_usb_hw_t* mock, rx_err_t ret)
{
  mock_usb_hw_t* m   = internal_get_mock(mock);
  m->next_init_return = ret;
}

void mock_usb_hw_set_attach_return(mock_usb_hw_t* mock, rx_err_t ret)
{
  mock_usb_hw_t* m     = internal_get_mock(mock);
  m->next_attach_return = ret;
}

void mock_usb_hw_set_fifo_ready(mock_usb_hw_t* mock, bool ready)
{
  mock_usb_hw_t* m = internal_get_mock(mock);
  m->fifo_ready    = ready;
}

void mock_usb_hw_set_bus_state(mock_usb_hw_t* mock, rx_usb_state_t state)
{
  mock_usb_hw_t* m = internal_get_mock(mock);
  m->bus_state     = state;
}

/* =============================================================================
 * Data Injection/Extraction Functions
 * =============================================================================
 */

rx_err_t mock_usb_hw_inject_rx_data(mock_usb_hw_t* mock,
                                    uint8_t        pipe,
                                    const uint8_t* data,
                                    uint32_t       len)
{
  mock_usb_hw_t* m = internal_get_mock(mock);

  if (pipe >= MOCK_USB_HW_MAX_PIPES) {
    return k_rx_err_invalid_arg;
  }

  if (data == NULL || len == 0) {
    return k_rx_err_invalid_arg;
  }

  mock_pipe_state_t* p = &m->pipes[pipe];

  /* Check available space */
  uint32_t space = MOCK_USB_HW_FIFO_SIZE - p->rx_len;
  if (len > space) {
    len = space;
  }

  /* Copy data */
  memcpy(p->rx_data + p->rx_len, data, len);
  p->rx_len += len;

  return k_rx_ok;
}

rx_err_t mock_usb_hw_get_tx_data(mock_usb_hw_t* mock,
                                 uint8_t        pipe,
                                 uint8_t*       data,
                                 uint32_t       max_len,
                                 uint32_t*      actual_len)
{
  mock_usb_hw_t* m = internal_get_mock(mock);

  if (pipe >= MOCK_USB_HW_MAX_PIPES) {
    return k_rx_err_invalid_arg;
  }

  if (data == NULL || actual_len == NULL) {
    return k_rx_err_invalid_arg;
  }

  mock_pipe_state_t* p = &m->pipes[pipe];

  /* Copy available data */
  uint32_t to_read = (p->tx_len < max_len) ? p->tx_len : max_len;
  memcpy(data, p->tx_data, to_read);
  *actual_len = to_read;

  /* Remove read data by shifting */
  if (to_read < p->tx_len) {
    memmove(p->tx_data, p->tx_data + to_read, p->tx_len - to_read);
  }
  p->tx_len -= to_read;

  return k_rx_ok;
}

void mock_usb_hw_clear_pipe(mock_usb_hw_t* mock, uint8_t pipe)
{
  mock_usb_hw_t* m = internal_get_mock(mock);

  if (pipe < MOCK_USB_HW_MAX_PIPES) {
    memset(&m->pipes[pipe], 0, sizeof(mock_pipe_state_t));
  }
}

/* =============================================================================
 * Call Tracking Functions
 * =============================================================================
 */

void mock_usb_hw_record_call(mock_usb_hw_t* mock,
                             const char*    func,
                             uint32_t       arg1,
                             uint32_t       arg2,
                             uint32_t       arg3,
                             rx_err_t       ret)
{
  mock_usb_hw_t* m = internal_get_mock(mock);

  mock_usb_hw_call_t* call = &m->call_history[m->call_write_index];

  strncpy(call->function, func, MOCK_USB_HW_FUNC_NAME_MAX - 1);
  call->function[MOCK_USB_HW_FUNC_NAME_MAX - 1] = '\0';
  call->arg1                                    = arg1;
  call->arg2                                    = arg2;
  call->arg3                                    = arg3;
  call->return_value                            = ret;

  m->call_write_index = (m->call_write_index + 1) % MOCK_USB_HW_CALL_HISTORY_SIZE;
  m->call_count++;
}

bool mock_usb_hw_was_called(mock_usb_hw_t* mock, const char* func)
{
  return mock_usb_hw_get_call_count(mock, func) > 0;
}

uint32_t mock_usb_hw_get_call_count(mock_usb_hw_t* mock, const char* func)
{
  mock_usb_hw_t* m     = internal_get_mock(mock);
  uint32_t       count = 0;

  uint32_t history_size =
    (m->call_count < MOCK_USB_HW_CALL_HISTORY_SIZE) ? m->call_count : MOCK_USB_HW_CALL_HISTORY_SIZE;

  for (uint32_t i = 0; i < history_size; i++) {
    if (strcmp(m->call_history[i].function, func) == 0) {
      count++;
    }
  }

  return count;
}

rx_err_t mock_usb_hw_get_last_call(mock_usb_hw_t*      mock,
                                   const char*         func,
                                   mock_usb_hw_call_t* out_call)
{
  mock_usb_hw_t* m = internal_get_mock(mock);

  if (out_call == NULL) {
    return k_rx_err_invalid_arg;
  }

  uint32_t history_size =
    (m->call_count < MOCK_USB_HW_CALL_HISTORY_SIZE) ? m->call_count : MOCK_USB_HW_CALL_HISTORY_SIZE;

  /* Search backwards for most recent call */
  for (int32_t i = history_size - 1; i >= 0; i--) {
    uint32_t idx = (m->call_write_index - 1 - i + MOCK_USB_HW_CALL_HISTORY_SIZE) %
                   MOCK_USB_HW_CALL_HISTORY_SIZE;

    if (strcmp(m->call_history[idx].function, func) == 0) {
      *out_call = m->call_history[idx];
      return k_rx_ok;
    }
  }

  return k_rx_err_not_found;
}

void mock_usb_hw_clear_calls(mock_usb_hw_t* mock)
{
  mock_usb_hw_t* m = internal_get_mock(mock);

  memset(m->call_history, 0, sizeof(m->call_history));
  m->call_count       = 0;
  m->call_write_index = 0;
}

/* =============================================================================
 * Mock Implementations of rx_usb_hw_* Functions
 *
 * These are the actual mock implementations that replace the real hardware
 * functions when linking for tests. They track calls and use mock state.
 * =============================================================================
 */

rx_err_t rx_usb_hw_init(void)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  rx_err_t ret = m->next_init_return;
  mock_usb_hw_record_call(m, "rx_usb_hw_init", 0, 0, 0, ret);
  m->init_calls++;

  if (ret == k_rx_ok) {
    m->initialized = true;
  }

  /* Reset to default for next call */
  m->next_init_return = k_rx_ok;

  return ret;
}

rx_err_t rx_usb_hw_deinit(void)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  rx_err_t ret = k_rx_ok;
  mock_usb_hw_record_call(m, "rx_usb_hw_deinit", 0, 0, 0, ret);
  m->deinit_calls++;

  m->initialized = false;
  m->attached    = false;

  return ret;
}

rx_err_t rx_usb_hw_attach(void)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  rx_err_t ret = m->next_attach_return;
  mock_usb_hw_record_call(m, "rx_usb_hw_attach", 0, 0, 0, ret);
  m->attach_calls++;

  if (ret == k_rx_ok) {
    m->attached = true;
  }

  /* Reset to default for next call */
  m->next_attach_return = k_rx_ok;

  return ret;
}

rx_err_t rx_usb_hw_detach(void)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  rx_err_t ret = k_rx_ok;
  mock_usb_hw_record_call(m, "rx_usb_hw_detach", 0, 0, 0, ret);
  m->detach_calls++;

  m->attached = false;

  return ret;
}

uint32_t rx_usb_hw_fifo_read(uint8_t pipe, uint8_t* data, uint32_t max_len)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  m->fifo_read_calls++;

  if (!m->fifo_ready || pipe >= MOCK_USB_HW_MAX_PIPES) {
    mock_usb_hw_record_call(m, "rx_usb_hw_fifo_read", pipe, max_len, 0, k_rx_err_timeout);
    return 0;
  }

  mock_pipe_state_t* p = &m->pipes[pipe];

  /* Read from mock RX FIFO */
  uint32_t available = p->rx_len - p->rx_pos;
  uint32_t to_read   = (available < max_len) ? available : max_len;

  if (to_read > 0 && data != NULL) {
    memcpy(data, p->rx_data + p->rx_pos, to_read);
    p->rx_pos += to_read;
  }

  mock_usb_hw_record_call(m, "rx_usb_hw_fifo_read", pipe, max_len, to_read, k_rx_ok);

  return to_read;
}

uint32_t rx_usb_hw_fifo_write(uint8_t pipe, const uint8_t* data, uint32_t len)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  m->fifo_write_calls++;

  if (!m->fifo_ready || pipe >= MOCK_USB_HW_MAX_PIPES) {
    mock_usb_hw_record_call(m, "rx_usb_hw_fifo_write", pipe, len, 0, k_rx_err_timeout);
    return 0;
  }

  mock_pipe_state_t* p = &m->pipes[pipe];

  /* Write to mock TX FIFO */
  uint32_t space    = MOCK_USB_HW_FIFO_SIZE - p->tx_len;
  uint32_t to_write = (len < space) ? len : space;

  if (to_write > 0 && data != NULL) {
    memcpy(p->tx_data + p->tx_len, data, to_write);
    p->tx_len += to_write;
  }

  mock_usb_hw_record_call(m, "rx_usb_hw_fifo_write", pipe, len, to_write, k_rx_ok);

  return to_write;
}

rx_usb_state_t rx_usb_hw_get_bus_state(void)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  mock_usb_hw_record_call(m, "rx_usb_hw_get_bus_state", 0, 0, (uint32_t)m->bus_state, k_rx_ok);

  return m->bus_state;
}

void rx_usb_hw_set_address(uint8_t address)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  m->address = address;
  mock_usb_hw_record_call(m, "rx_usb_hw_set_address", address, 0, 0, k_rx_ok);
}

rx_err_t rx_usb_hw_configure_pipe(uint8_t  pipe,
                                  uint8_t  endpoint,
                                  bool     is_in,
                                  uint16_t type,
                                  uint16_t max_packet)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  if (pipe == 0 || pipe >= MOCK_USB_HW_MAX_PIPES) {
    mock_usb_hw_record_call(m, "rx_usb_hw_configure_pipe", pipe, endpoint, type, k_rx_err_invalid_arg);
    return k_rx_err_invalid_arg;
  }

  mock_pipe_state_t* p = &m->pipes[pipe];
  p->configured        = true;
  p->endpoint          = endpoint;
  p->is_in             = is_in;
  p->type              = type;
  p->max_packet        = max_packet;

  mock_usb_hw_record_call(m, "rx_usb_hw_configure_pipe", pipe, endpoint, type, k_rx_ok);

  return k_rx_ok;
}

/* =============================================================================
 * Mock Implementation of rx_usb_cdc_init
 *
 * This is called by rx_usb_init() and needs to be mocked for testing.
 * =============================================================================
 */

rx_err_t rx_usb_cdc_init(void)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  rx_err_t ret = k_rx_ok; /* CDC init always succeeds in mock */
  mock_usb_hw_record_call(m, "rx_usb_cdc_init", 0, 0, 0, ret);

  return ret;
}

void rx_usb_cdc_handle_bulk_in(void)
{
  mock_usb_hw_t* m = &g_mock_usb_hw;

  mock_usb_hw_record_call(m, "rx_usb_cdc_handle_bulk_in", 0, 0, 0, k_rx_ok);

  /* In real implementation, this pops from TX buffer and writes to FIFO.
   * For testing, we just record that it was called. */
}
