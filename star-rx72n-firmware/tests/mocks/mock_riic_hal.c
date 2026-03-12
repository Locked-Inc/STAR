/**
 * @file mock_riic_hal.c
 * @brief Mock RIIC (I2C) HAL Function Implementation
 *
 * Provides mock implementations of RIIC HAL functions for unit testing.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "mock_riic_hal.h"

#include <string.h>

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Valid I2C frequencies */
typedef enum : uint32_t {
  k_mock_riic_freq_100khz = 100000,  /**< Standard mode */
  k_mock_riic_freq_400khz = 400000,  /**< Fast mode */
  k_mock_riic_freq_1mhz   = 1000000, /**< Fast mode plus */
} mock_riic_frequency_t;

/**
 * @enum mock_riic_length_t
 * @brief Named constants for clearing receive-length fields in mock state
 *
 * @details
 * Used instead of the magic literal 0 when resetting ch->rx_length after
 * peripheral-read data has been consumed, making the intent explicit.
 *
 * @see riic_peripheral_read() Consumes rx_buffer and resets rx_length
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_mock_riic_length_cleared = 0U, /**< Sentinel: receive buffer fully consumed */
} mock_riic_length_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

mock_riic_state_t g_mock_riic;

/* =============================================================================
 * Private Helper Functions
 * =============================================================================
 */

/**
 * @brief Record a call in the history
 */
static void internal_record_call(mock_riic_call_type_t type,
                                 uint8_t               channel,
                                 uint8_t               device_addr,
                                 uint16_t              write_length,
                                 uint16_t              read_length)
{
  if (g_mock_riic.call_count < k_mock_riic_call_history_size) {
    mock_riic_call_t* call = &g_mock_riic.call_history[g_mock_riic.call_count];
    call->type             = type;
    call->channel          = channel;
    call->device_addr      = device_addr;
    call->write_length     = write_length;
    call->read_length      = read_length;
    /* Clear tx_snapshot so stale bytes from a previous clear+reuse cycle are not visible */
    call->tx_snapshot[k_mock_riic_snapshot_reg_idx] = k_mock_riic_snapshot_empty;
    call->tx_snapshot[k_mock_riic_snapshot_val_idx] = k_mock_riic_snapshot_empty;
    g_mock_riic.call_count++;
  }
}

/**
 * @brief Check and consume error injection
 */
static rx_err_t internal_check_error(void)
{
  if (g_mock_riic.error_set) {
    rx_err_t err          = g_mock_riic.next_error;
    g_mock_riic.error_set = false;
    return err;
  }
  return k_rx_ok;
}

/**
 * @brief Check for simulated error conditions
 */
static rx_err_t internal_check_simulated_errors(void)
{
  if (g_mock_riic.simulate_busy) {
    return k_rx_err_timeout; /* Bus busy appears as timeout */
  }
  if (g_mock_riic.simulate_timeout) {
    return k_rx_err_timeout;
  }
  if (g_mock_riic.simulate_nack) {
    return k_rx_err_nack;
  }
  return k_rx_ok;
}

/**
 * @brief Populate tx_snapshot in a call history entry from a write buffer
 *
 * @details
 * Captures up to two bytes from @p data into @p entry->tx_snapshot using the
 * named index constants k_mock_riic_snapshot_reg_idx and
 * k_mock_riic_snapshot_val_idx.  Each slot that falls within @p length receives
 * the corresponding raw byte; any slot whose index is >= @p length is filled with
 * k_mock_riic_snapshot_empty (0x100), which lies outside the uint8_t domain and
 * is therefore unambiguous even when a legitimate TX byte of 0x00 is captured.
 *
 * @param[in,out] entry  Call history entry whose tx_snapshot is written.
 *                       Must not be NULL.  Ownership remains with the caller.
 * @param[in]     data   Source write buffer.  May be NULL only when length == 0.
 * @param[in]     length Number of valid bytes in @p data (0 or more).
 *
 * @pre  entry != NULL
 * @pre  data != NULL || length == 0
 * @post entry->tx_snapshot[k_mock_riic_snapshot_reg_idx] == data[0] if length > 0,
 *       else k_mock_riic_snapshot_empty
 * @post entry->tx_snapshot[k_mock_riic_snapshot_val_idx] == data[1] if length > 1,
 *       else k_mock_riic_snapshot_empty
 *
 * @note Uses k_mock_riic_snapshot_reg_idx, k_mock_riic_snapshot_val_idx, and
 *       k_mock_riic_snapshot_empty for all array subscripts and sentinel writes
 *       so that no unnamed integer literals appear in the snapshot logic.
 *
 * @return void
 * @retval N/A This function has no return value.
 *
 * @code
 * // Length 0: both slots receive the empty sentinel (0x100)
 * mock_riic_call_t e0 = {0};
 * internal_record_tx_snapshot(&e0, nullptr, 0);
 * // e0.tx_snapshot[k_mock_riic_snapshot_reg_idx] == k_mock_riic_snapshot_empty
 * // e0.tx_snapshot[k_mock_riic_snapshot_val_idx] == k_mock_riic_snapshot_empty
 *
 * // Length 1: slot 0 has the byte; slot 1 receives the sentinel
 * uint8_t buf1[] = {0x07U};
 * mock_riic_call_t e1 = {0};
 * internal_record_tx_snapshot(&e1, buf1, 1);
 * // e1.tx_snapshot[k_mock_riic_snapshot_reg_idx] == 0x07U
 * // e1.tx_snapshot[k_mock_riic_snapshot_val_idx] == k_mock_riic_snapshot_empty
 *
 * // Length 2: both slots have real bytes
 * uint8_t buf2[] = {0x07U, 0x01U};
 * mock_riic_call_t e2 = {0};
 * internal_record_tx_snapshot(&e2, buf2, 2);
 * // e2.tx_snapshot[k_mock_riic_snapshot_reg_idx] == 0x07U
 * // e2.tx_snapshot[k_mock_riic_snapshot_val_idx] == 0x01U
 * @endcode
 */
static void
internal_record_tx_snapshot(mock_riic_call_t* entry, const uint8_t* data, uint16_t length)
{
  entry->tx_snapshot[k_mock_riic_snapshot_reg_idx] = (length > k_mock_riic_snapshot_reg_idx)
                                                       ? data[k_mock_riic_snapshot_reg_idx]
                                                       : k_mock_riic_snapshot_empty;
  entry->tx_snapshot[k_mock_riic_snapshot_val_idx] = (length > k_mock_riic_snapshot_val_idx)
                                                       ? data[k_mock_riic_snapshot_val_idx]
                                                       : k_mock_riic_snapshot_empty;
}

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

void mock_riic_init(void)
{
  memset(&g_mock_riic, 0, sizeof(g_mock_riic));
}

void mock_riic_reset(void)
{
  mock_riic_init();
}

/* =============================================================================
 * Test Setup Functions
 * =============================================================================
 */

void mock_riic_set_rx_data(uint8_t channel, const uint8_t* data, uint16_t length)
{
  if (channel >= k_mock_riic_max_channels || data == nullptr) {
    return;
  }

  uint16_t to_copy = (length < k_mock_riic_buffer_size) ? length : k_mock_riic_buffer_size;
  memcpy(g_mock_riic.channels[channel].rx_buffer, data, to_copy);
  g_mock_riic.channels[channel].rx_length = to_copy;
}

void mock_riic_simulate_nack(bool simulate)
{
  g_mock_riic.simulate_nack = simulate;
}

void mock_riic_simulate_timeout(bool simulate)
{
  g_mock_riic.simulate_timeout = simulate;
}

void mock_riic_simulate_busy(bool simulate)
{
  g_mock_riic.simulate_busy = simulate;
}

void mock_riic_set_next_error(rx_err_t err)
{
  g_mock_riic.next_error = err;
  g_mock_riic.error_set  = true;
}

void mock_riic_clear_error(void)
{
  g_mock_riic.error_set = false;
}

/* =============================================================================
 * State Inspection Functions
 * =============================================================================
 */

bool mock_riic_is_initialized(uint8_t channel)
{
  if (channel < k_mock_riic_max_channels) {
    return g_mock_riic.channels[channel].initialized;
  }
  return false;
}

uint32_t mock_riic_get_frequency(uint8_t channel)
{
  if (channel < k_mock_riic_max_channels) {
    return g_mock_riic.channels[channel].frequency_hz;
  }
  return 0;
}

uint16_t mock_riic_get_tx_data(uint8_t channel, uint8_t* data, uint16_t max_length)
{
  if (channel >= k_mock_riic_max_channels || data == nullptr) {
    return 0;
  }

  mock_riic_channel_state_t* ch      = &g_mock_riic.channels[channel];
  uint16_t                   to_copy = (max_length < ch->tx_length) ? max_length : ch->tx_length;
  memcpy(data, ch->tx_buffer, to_copy);
  return to_copy;
}

uint8_t mock_riic_get_last_device_addr(uint8_t channel)
{
  if (channel < k_mock_riic_max_channels) {
    return g_mock_riic.channels[channel].last_device_addr;
  }
  return 0;
}

/* =============================================================================
 * Call History Functions
 * =============================================================================
 */

const mock_riic_call_t* mock_riic_get_call(uint16_t index)
{
  if (index < g_mock_riic.call_count) {
    return &g_mock_riic.call_history[index];
  }
  return nullptr;
}

uint16_t mock_riic_get_call_count(void)
{
  return g_mock_riic.call_count;
}

void mock_riic_clear_history(void)
{
  g_mock_riic.call_count = 0;
}

/* =============================================================================
 * Mock RIIC HAL Functions
 * =============================================================================
 */

rx_err_t riic_init(riic_channel_t channel, uint32_t frequency_hz)
{
  internal_record_call(k_mock_riic_call_init, channel.value, 0, 0, 0);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Validate channel */
  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Validate frequency */
  if (frequency_hz != k_mock_riic_freq_100khz && frequency_hz != k_mock_riic_freq_400khz &&
      frequency_hz != k_mock_riic_freq_1mhz) {
    return k_rx_err_invalid_arg;
  }

  /* Initialize channel */
  g_mock_riic.channels[channel.value].initialized  = true;
  g_mock_riic.channels[channel.value].frequency_hz = frequency_hz;

  return k_rx_ok;
}

rx_err_t riic_write(riic_channel_t    channel,
                    i2c_device_addr_t device_addr,
                    const uint8_t*    data,
                    const uint16_t    length)
{
  /* Capture count before recording so snapshot only targets the freshly appended entry */
  const uint16_t count_before = g_mock_riic.call_count;
  internal_record_call(k_mock_riic_call_write, channel.value, device_addr.value, length, 0);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Null pointer check */
  if (data == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate channel */
  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Check initialization */
  if (!g_mock_riic.channels[channel.value].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for simulated errors */
  err = internal_check_simulated_errors();
  if (err != k_rx_ok) {
    return err;
  }

  /* Store transmitted data */
  mock_riic_channel_state_t* ch = &g_mock_riic.channels[channel.value];
  uint16_t to_copy = (length < k_mock_riic_buffer_size) ? length : k_mock_riic_buffer_size;
  memcpy(ch->tx_buffer, data, to_copy);
  ch->tx_length        = to_copy;
  ch->last_device_addr = device_addr.value;

  /* Snapshot first 2 TX bytes into the freshly appended call history entry.
   * Only write when internal_record_call() actually appended a new record
   * (call_count incremented), so we never touch existing or out-of-bounds entries. */
  if (g_mock_riic.call_count > count_before) {
    internal_record_tx_snapshot(&g_mock_riic.call_history[count_before], data, length);
  }

  return k_rx_ok;
}

rx_err_t riic_read(riic_channel_t    channel,
                   i2c_device_addr_t device_addr,
                   uint8_t*          data,
                   const uint16_t    length)
{
  internal_record_call(k_mock_riic_call_read, channel.value, device_addr.value, 0, length);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Null pointer check */
  if (data == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate channel */
  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Check initialization */
  if (!g_mock_riic.channels[channel.value].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for simulated errors */
  err = internal_check_simulated_errors();
  if (err != k_rx_ok) {
    return err;
  }

  /* Copy pre-loaded RX data */
  mock_riic_channel_state_t* ch      = &g_mock_riic.channels[channel.value];
  uint16_t                   to_copy = (length < ch->rx_length) ? length : ch->rx_length;
  memcpy(data, ch->rx_buffer, to_copy);
  ch->last_device_addr = device_addr.value;

  return k_rx_ok;
}

rx_err_t riic_write_read(riic_channel_t    channel,
                         i2c_device_addr_t device_addr,
                         const uint8_t*    write_data,
                         uint16_t          write_length,
                         uint8_t*          read_data,
                         uint16_t          read_length)
{
  /* Capture count before recording so snapshot only targets the freshly appended entry */
  const uint16_t count_before = g_mock_riic.call_count;
  internal_record_call(k_mock_riic_call_write_read,
                       channel.value,
                       device_addr.value,
                       write_length,
                       read_length);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  /* Null pointer checks */
  if (write_data == nullptr) {
    return k_rx_err_null_ptr;
  }
  if (read_data == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Validate channel */
  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  /* Check initialization */
  if (!g_mock_riic.channels[channel.value].initialized) {
    return k_rx_err_invalid_state;
  }

  /* Check for simulated errors */
  err = internal_check_simulated_errors();
  if (err != k_rx_ok) {
    return err;
  }

  /* Store transmitted data */
  mock_riic_channel_state_t* ch = &g_mock_riic.channels[channel.value];
  uint16_t                   to_write =
    (write_length < k_mock_riic_buffer_size) ? write_length : k_mock_riic_buffer_size;
  memcpy(ch->tx_buffer, write_data, to_write);
  ch->tx_length        = to_write;
  ch->last_device_addr = device_addr.value;

  /* Copy pre-loaded RX data */
  uint16_t to_read = (read_length < ch->rx_length) ? read_length : ch->rx_length;
  memcpy(read_data, ch->rx_buffer, to_read);

  /* Snapshot first 2 TX bytes into the freshly appended call history entry */
  if (g_mock_riic.call_count > count_before) {
    internal_record_tx_snapshot(&g_mock_riic.call_history[count_before], write_data, write_length);
  }

  return k_rx_ok;
}

/* =============================================================================
 * Peripheral Mode Mock Implementations
 * =============================================================================
 */

/**
 * @brief Mock implementation of riic_init_peripheral()
 *
 * @details
 * Marks the specified channel as initialized in mock state and records the
 * call in global call history. Error injection (internal_check_error()) is
 * applied before validating the channel index so test fixtures can simulate
 * hardware init failures. A channel.value out of range returns
 * k_rx_err_invalid_arg without modifying any channel state.
 *
 * @param[in] channel     RIIC channel wrapper (channel.value range: 0 to
 *                        k_mock_riic_max_channels - 1)
 * @param[in] device_addr 7-bit I2C peripheral address the mock should respond to.
 *                        Recorded in call history; not range-validated by mock.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok                 Channel marked initialized
 * @retval k_rx_err_invalid_arg    channel.value >= k_mock_riic_max_channels
 * @retval Other                   Injected error from mock_riic_set_next_error()
 *
 * @pre  channel.value < k_mock_riic_max_channels
 * @pre  mock state must be initialized via mock_riic_init() before use
 * @post On k_rx_ok: g_mock_riic.channels[channel.value].initialized == true
 * @post call recorded in g_mock_riic.call_history (if history not full)
 *
 * @note Not thread-safe; must be called from a single test thread
 * @see  mock_riic_init()        Reset mock state before each test
 * @see  riic_init_peripheral()  Real HAL function this mock replaces
 *
 * @since Version 1.0.0
 */
rx_err_t riic_init_peripheral(const riic_channel_t channel, const i2c_device_addr_t device_addr)
{
  internal_record_call(k_mock_riic_call_init, channel.value, device_addr.value, 0, 0);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }

  g_mock_riic.channels[channel.value].initialized = true;
  return k_rx_ok;
}

/**
 * @brief Mock implementation of riic_peripheral_read()
 *
 * @details
 * Returns pre-loaded RX data set by mock_riic_set_rx_data() as if the I2C
 * controller (RPi5) had written bytes to this peripheral. After the data is
 * consumed, ch->rx_length is reset to k_mock_riic_length_cleared so
 * subsequent calls return bytes_read=0 (no new data). Error injection and
 * simulated error conditions are applied before any data copy.
 *
 * @param[in]  channel     RIIC channel wrapper (channel.value range: 0 to
 *                         k_mock_riic_max_channels - 1)
 * @param[out] data        Buffer to receive copied bytes (must not be nullptr)
 * @param[in]  max_length  Maximum bytes to copy (must be >= 1)
 * @param[out] bytes_read  Actual bytes copied (may be 0 if no pre-loaded data)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok               Data (possibly 0 bytes) placed in @p data
 * @retval k_rx_err_null_ptr     data or bytes_read is nullptr
 * @retval k_rx_err_invalid_arg  channel.value >= k_mock_riic_max_channels
 *                               or max_length == 0
 * @retval k_rx_err_invalid_state Channel not initialized via
 *                                riic_init_peripheral() mock
 * @retval Other                 Injected error from mock_riic_set_next_error()
 *                               or simulated error (timeout, NACK, busy)
 *
 * @pre  channel.value < k_mock_riic_max_channels
 * @pre  channel must be initialized via riic_init_peripheral() mock first
 * @post On k_rx_ok: *bytes_read contains bytes copied (0 to max_length)
 * @post On k_rx_ok: g_mock_riic.channels[channel.value].rx_length reset to
 *       k_mock_riic_length_cleared (consumed)
 *
 * @note Not thread-safe; must be called from a single test thread
 * @note Call mock_riic_set_rx_data() before this function to pre-load data
 * @see  mock_riic_set_rx_data()   Load data for the mock to return
 * @see  riic_peripheral_read()    Real HAL function this mock replaces
 *
 * @since Version 1.0.0
 */
rx_err_t riic_peripheral_read(const riic_channel_t channel,
                              uint8_t*             data,
                              const uint16_t       max_length,
                              uint16_t*            bytes_read)
{
  internal_record_call(k_mock_riic_call_read, channel.value, 0, 0, max_length);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (data == nullptr || bytes_read == nullptr) {
    return k_rx_err_null_ptr;
  }
  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }
  if (max_length == 0 || max_length > k_riic_peripheral_transfer_limit) {
    return k_rx_err_invalid_arg;
  }
  if (!g_mock_riic.channels[channel.value].initialized) {
    return k_rx_err_invalid_state;
  }

  err = internal_check_simulated_errors();
  if (err != k_rx_ok) {
    return err;
  }

  mock_riic_channel_state_t* ch      = &g_mock_riic.channels[channel.value];
  uint16_t                   to_read = (max_length < ch->rx_length) ? max_length : ch->rx_length;
  if (to_read > 0) {
    memcpy(data, ch->rx_buffer, to_read);
    if (to_read == ch->rx_length) {
      ch->rx_length = k_mock_riic_length_cleared;
    } else {
      (void)memmove(ch->rx_buffer, ch->rx_buffer + to_read, ch->rx_length - to_read);
      ch->rx_length = (uint16_t)(ch->rx_length - to_read);
    }
  }
  *bytes_read = to_read;
  return k_rx_ok;
}

/**
 * @brief Mock implementation of riic_peripheral_write()
 *
 * @details
 * Records data bytes that the peripheral would transmit to the I2C controller.
 * Copies up to k_mock_riic_buffer_size bytes from @p data into the channel's
 * tx_buffer and updates tx_length. Also records the call in the global call
 * history via internal_record_call() so tests can inspect the call count and
 * arguments. Error injection (internal_check_error()) and simulated error
 * conditions (internal_check_simulated_errors()) are applied before copying.
 *
 * @param[in] channel RIIC channel wrapper (channel.value range: 0-2)
 * @param[in] data    Buffer containing bytes to write (must not be nullptr)
 * @param[in] length  Number of bytes in @p data (must be >= 1)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok         Data stored in tx_buffer; tx_length updated
 * @retval k_rx_err_null_ptr  @p data is nullptr
 * @retval k_rx_err_invalid_arg  channel.value >= k_mock_riic_max_channels or length == 0
 * @retval k_rx_err_invalid_state  Channel not initialized via riic_init_peripheral() mock
 * @retval Other  Injected error from mock_riic_set_next_error()
 *
 * @pre channel.value < k_mock_riic_max_channels
 * @pre data != nullptr with at least length valid bytes
 * @pre length >= 1
 * @post On k_rx_ok: g_mock_riic.channels[channel.value].tx_buffer[0..to_write-1]
 *       contain a copy of data[0..to_write-1]
 * @post On k_rx_ok: g_mock_riic.channels[channel.value].tx_length == to_write
 *
 * @note Not thread-safe; must be called from a single test thread
 * @note Truncates silently to k_mock_riic_buffer_size bytes if length exceeds it
 *
 * @see riic_init_peripheral()   Initialize channel before calling this mock
 * @see mock_riic_get_tx_data()  Retrieve stored TX bytes from tests
 *
 * @since Version 1.0.0
 */
rx_err_t
riic_peripheral_write(const riic_channel_t channel, const uint8_t* data, const uint16_t length)
{
  internal_record_call(k_mock_riic_call_write, channel.value, 0, length, 0);

  rx_err_t err = internal_check_error();
  if (err != k_rx_ok) {
    return err;
  }

  if (data == nullptr) {
    return k_rx_err_null_ptr;
  }
  if (channel.value >= k_mock_riic_max_channels) {
    return k_rx_err_invalid_arg;
  }
  if (length == 0 || length > k_riic_peripheral_transfer_limit) {
    return k_rx_err_invalid_arg;
  }
  if (!g_mock_riic.channels[channel.value].initialized) {
    return k_rx_err_invalid_state;
  }

  err = internal_check_simulated_errors();
  if (err != k_rx_ok) {
    return err;
  }

  mock_riic_channel_state_t* ch = &g_mock_riic.channels[channel.value];
  uint16_t to_write = (length < k_mock_riic_buffer_size) ? length : k_mock_riic_buffer_size;
  memcpy(ch->tx_buffer, data, to_write);
  ch->tx_length = to_write;
  return k_rx_ok;
}
