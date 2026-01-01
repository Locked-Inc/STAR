/* lib/rx_usb/src/rx_usb.c */

/**
 * @file rx_usb.c
 * @brief USB CDC-ACM Driver Implementation for RX72N
 * @details
 * This file implements the public API for the USB CDC driver.
 * It manages USB state, ring buffers for TX/RX, and coordinates
 * with the hardware layer and CDC class implementation.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_usb.h"

#include <string.h>

#ifndef UNIT_TEST
#include "rx72n_regs.h"
#include "rx_log.h"
#else
/* Mock includes for unit testing */
#include "mock_usb0_regs.h"
#define rx_log_info(tag, msg)  ((void)0)
#define rx_log_warn(tag, msg)  ((void)0)
#define rx_log_error(tag, msg) ((void)0)
#define rx_log_debug(tag, msg) ((void)0)
#endif

/* =============================================================================
 * Testability Macros
 *
 * STATIC_TESTABLE allows static functions to be exposed for unit testing.
 * When UNIT_TEST is defined, these functions become externally visible.
 * =============================================================================
 */

#ifdef UNIT_TEST
#define STATIC_TESTABLE
#else
#define STATIC_TESTABLE static
#endif

/* =============================================================================
 * Private Definitions
 * =============================================================================
 */

static const char* s_tag = "USB";

/* Ring buffer structure */
typedef struct {
  uint8_t  data[k_usb_rx_buffer_size];
  uint32_t head;
  uint32_t tail;
  uint32_t count;
} ring_buffer_t;

/* USB driver state */
typedef struct {
  bool                 initialized;
  rx_usb_state_t       state;
  rx_usb_callback_t    callback;
  void*                callback_context;
  ring_buffer_t        rx_buffer;
  ring_buffer_t        tx_buffer;
  rx_usb_stats_t       stats;
  rx_usb_line_coding_t line_coding;
} usb_driver_t;

/* =============================================================================
 * Private Variables
 * =============================================================================
 */

static usb_driver_t s_usb = {0};

/* =============================================================================
 * Forward Declarations (internal functions from other modules)
 * =============================================================================
 */

/* From rx_usb_hw.c */
extern rx_err_t rx_usb_hw_init(void);
extern rx_err_t rx_usb_hw_deinit(void);
extern rx_err_t rx_usb_hw_attach(void);
extern rx_err_t rx_usb_hw_detach(void);

/* From rx_usb_cdc.c */
extern rx_err_t rx_usb_cdc_init(void);
extern void     rx_usb_cdc_handle_bulk_in(void);

/* =============================================================================
 * Private Functions - Ring Buffer Operations
 *
 * These functions use STATIC_TESTABLE to allow unit testing.
 * In production builds, they are static. In test builds, they are exposed.
 * =============================================================================
 */

STATIC_TESTABLE void internal_ring_buffer_init(ring_buffer_t* buf)
{
  buf->head  = 0;
  buf->tail  = 0;
  buf->count = 0;
}

STATIC_TESTABLE uint32_t internal_ring_buffer_available(const ring_buffer_t* buf)
{
  return buf->count;
}

STATIC_TESTABLE uint32_t internal_ring_buffer_free(const ring_buffer_t* buf)
{
  return k_usb_rx_buffer_size - buf->count;
}

STATIC_TESTABLE uint32_t internal_ring_buffer_write(ring_buffer_t* buf,
                                                    const uint8_t* data,
                                                    uint32_t       len)
{
  uint32_t written = 0;

  while (written < len && buf->count < k_usb_rx_buffer_size) {
    buf->data[buf->head] = data[written];
    buf->head            = (buf->head + 1) % k_usb_rx_buffer_size;
    buf->count++;
    written++;
  }

  return written;
}

STATIC_TESTABLE uint32_t internal_ring_buffer_read(ring_buffer_t* buf,
                                                   uint8_t*       data,
                                                   uint32_t       max_len)
{
  uint32_t read_count = 0;

  while (read_count < max_len && buf->count > 0) {
    data[read_count] = buf->data[buf->tail];
    buf->tail        = (buf->tail + 1) % k_usb_rx_buffer_size;
    buf->count--;
    read_count++;
  }

  return read_count;
}

/* =============================================================================
 * Private Functions - Transmission Trigger
 * =============================================================================
 */

/**
 * @brief Trigger USB transmission if pipe is not busy
 *
 * This function checks if Pipe 1 (Bulk IN) is idle and if there is data
 * in the TX buffer. If both conditions are met, it initiates transmission
 * by calling rx_usb_cdc_handle_bulk_in().
 *
 * @note Called internally after writing to the TX buffer.
 */
static void internal_trigger_tx_if_idle(void)
{
  /* Only trigger if data is available */
  if (internal_ring_buffer_available(&s_usb.tx_buffer) == 0) {
    return;
  }

  /* Check if Pipe 1 is not busy (PBUSY bit = 0 means idle) */
  if ((USB0.PIPE1CTR & k_usb_pipectr_pbusy) == 0) {
    /* Pipe is idle, trigger transmission */
    rx_usb_cdc_handle_bulk_in();
  }
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_usb_init(const rx_usb_config_t* config)
{
  if (s_usb.initialized) {
    rx_log_warn(s_tag, "USB already initialized");
    return k_rx_err_invalid_state;
  }

  rx_log_info(s_tag, "Initializing USB CDC driver");

  /* Clear driver state */
  memset(&s_usb, 0, sizeof(s_usb));

  /* Store callback if provided */
  if (config != NULL) {
    s_usb.callback         = config->callback;
    s_usb.callback_context = config->ctx;
  }

  /* Initialize ring buffers */
  internal_ring_buffer_init(&s_usb.rx_buffer);
  internal_ring_buffer_init(&s_usb.tx_buffer);

  /* Set default line coding (115200 8N1) */
  s_usb.line_coding.baud_rate = 115200;
  s_usb.line_coding.stop_bits = 0; /* 1 stop bit */
  s_usb.line_coding.parity    = 0; /* No parity */
  s_usb.line_coding.data_bits = 8;

  /* Initialize hardware */
  rx_err_t err = rx_usb_hw_init();
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to initialize USB hardware");
    return err;
  }

  /* Initialize CDC class */
  err = rx_usb_cdc_init();
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to initialize USB CDC");
    rx_usb_hw_deinit();
    return err;
  }

  /* Attach to USB bus (enable pull-up) */
  err = rx_usb_hw_attach();
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to attach to USB bus");
    rx_usb_hw_deinit();
    return err;
  }

  s_usb.state       = k_usb_state_attached;
  s_usb.initialized = true;

  rx_log_info(s_tag, "USB CDC driver initialized");

  return k_rx_ok;
}

rx_err_t rx_usb_deinit(void)
{
  if (!s_usb.initialized) {
    return k_rx_err_invalid_state;
  }

  rx_log_info(s_tag, "Deinitializing USB CDC driver");

  /* Detach from USB bus */
  rx_usb_hw_detach();

  /* Deinitialize hardware */
  rx_usb_hw_deinit();

  /* Clear state */
  s_usb.initialized = false;
  s_usb.state       = k_usb_state_detached;

  return k_rx_ok;
}

bool rx_usb_is_configured(void)
{
  return s_usb.initialized && (s_usb.state == k_usb_state_configured);
}

rx_usb_state_t rx_usb_get_state(void)
{
  return s_usb.state;
}

rx_err_t rx_usb_write(const uint8_t* data, uint32_t len)
{
  if (data == NULL) {
    return k_rx_err_null_pointer;
  }

  if (!s_usb.initialized) {
    return k_rx_err_invalid_state;
  }

  if (s_usb.state != k_usb_state_configured) {
    return k_rx_err_invalid_state;
  }

  /* Write to TX ring buffer */
  uint32_t written = internal_ring_buffer_write(&s_usb.tx_buffer, data, len);

  s_usb.stats.bytes_tx += written;

  if (written < len) {
    s_usb.stats.tx_underruns++;
    return k_rx_err_busy;
  }

  /* Trigger USB transmission if not already in progress */
  internal_trigger_tx_if_idle();

  return k_rx_ok;
}

rx_err_t rx_usb_read(uint8_t* data, uint32_t max_len, uint32_t* actual_len)
{
  if (data == NULL || actual_len == NULL) {
    return k_rx_err_null_pointer;
  }

  *actual_len = internal_ring_buffer_read(&s_usb.rx_buffer, data, max_len);

  return k_rx_ok;
}

rx_err_t rx_usb_rx_available(uint32_t* available)
{
  if (available == NULL) {
    return k_rx_err_null_pointer;
  }

  *available = internal_ring_buffer_available(&s_usb.rx_buffer);

  return k_rx_ok;
}

rx_err_t rx_usb_tx_available(uint32_t* available)
{
  if (available == NULL) {
    return k_rx_err_null_pointer;
  }

  *available = internal_ring_buffer_free(&s_usb.tx_buffer);

  return k_rx_ok;
}

rx_err_t rx_usb_flush(uint32_t timeout_ms)
{
  /* TODO: Implement blocking flush with timeout */
  (void)timeout_ms;

  if (!s_usb.initialized) {
    return k_rx_err_invalid_state;
  }

  /* For now, just check if buffer is empty */
  if (internal_ring_buffer_available(&s_usb.tx_buffer) > 0) {
    return k_rx_err_timeout;
  }

  return k_rx_ok;
}

rx_err_t rx_usb_get_line_coding(rx_usb_line_coding_t* line_coding)
{
  if (line_coding == NULL) {
    return k_rx_err_null_pointer;
  }

  *line_coding = s_usb.line_coding;

  return k_rx_ok;
}

rx_err_t rx_usb_get_stats(rx_usb_stats_t* stats)
{
  if (stats == NULL) {
    return k_rx_err_null_pointer;
  }

  *stats = s_usb.stats;

  return k_rx_ok;
}

void rx_usb_reset_stats(void)
{
  memset(&s_usb.stats, 0, sizeof(s_usb.stats));
}

/* =============================================================================
 * Internal Functions (called by other USB modules)
 * =============================================================================
 */

/**
 * @brief Set USB device state (called by hardware/CDC modules)
 */
void rx_usb_set_state(rx_usb_state_t state)
{
  if (s_usb.state != state) {
    rx_log_debug(s_tag, "USB state change");
    s_usb.state = state;

    /* Notify via callback */
    if (s_usb.callback != NULL) {
      rx_usb_event_t event;
      switch (state) {
        case k_usb_state_attached:
          event = k_usb_event_attached;
          break;
        case k_usb_state_detached:
          event = k_usb_event_detached;
          break;
        case k_usb_state_configured:
          event = k_usb_event_configured;
          break;
        case k_usb_state_suspended:
          event = k_usb_event_suspended;
          break;
        default:
          return; /* No callback for intermediate states */
      }
      s_usb.callback(event, s_usb.callback_context);
    }
  }
}

/**
 * @brief Set line coding (called by CDC module on SET_LINE_CODING request)
 */
void rx_usb_set_line_coding(const rx_usb_line_coding_t* coding)
{
  if (coding != NULL) {
    s_usb.line_coding = *coding;
    rx_log_debug(s_tag, "Line coding updated");
  }
}

/**
 * @brief Add received data to RX buffer (called by CDC/ISR)
 */
uint32_t rx_usb_rx_push(const uint8_t* data, uint32_t len)
{
  uint32_t written = internal_ring_buffer_write(&s_usb.rx_buffer, data, len);

  s_usb.stats.bytes_rx += written;

  if (written < len) {
    s_usb.stats.rx_overruns++;
  }

  /* Notify via callback */
  if (s_usb.callback != NULL && written > 0) {
    s_usb.callback(k_usb_event_data_rx, s_usb.callback_context);
  }

  return written;
}

/**
 * @brief Get data from TX buffer for transmission (called by CDC/ISR)
 */
uint32_t rx_usb_tx_pop(uint8_t* data, uint32_t max_len)
{
  return internal_ring_buffer_read(&s_usb.tx_buffer, data, max_len);
}

/**
 * @brief Increment bus reset counter
 */
void rx_usb_count_bus_reset(void)
{
  s_usb.stats.bus_resets++;
}

/**
 * @brief Increment suspend counter
 */
void rx_usb_count_suspend(void)
{
  s_usb.stats.suspends++;
}
