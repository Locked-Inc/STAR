/**
 * @file mock_hardware.c
 * @brief Mock Hardware State Implementation
 *
 * Implements mock hardware state for USB0 registers, ICU, SYSTEM,
 * logging, and ThreadX API.
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <stdio.h>
#include <string.h>

#include "mock_rx_log.h"
#include "mock_tx_api.h"
#include "mock_usb0_regs.h"

/* =============================================================================
 * Global Mock Register Instances
 * =============================================================================
 */

mock_usb_regs_t    g_mock_usb0;
mock_icu_regs_t    g_mock_icu;
mock_system_regs_t g_mock_system;
mock_log_state_t   g_mock_log;
mock_tx_state_t    g_mock_tx;

/* =============================================================================
 * USB Register Helper Constants
 * =============================================================================
 */

/** @brief INTSTS0 field positions and masks */
typedef enum : uint16_t {
  k_intsts0_ctsq_mask  = 0x0007,
  k_intsts0_dvsq_mask  = 0x0070,
  k_intsts0_dvsq_shift = 4,
} intsts0_fields_t;

/** @brief CFIFOCTR field definitions */
typedef enum : uint16_t {
  k_cfifoctr_dtln_mask = 0x0FFF,
  k_cfifoctr_frdy_bit  = 13,
} cfifoctr_fields_t;

/** @brief Pipe control register bit positions */
typedef enum : uint8_t {
  k_pipectr_pbusy_bit = 5,
} pipectr_fields_t;

/* =============================================================================
 * USB0 Register Mock Implementation
 * =============================================================================
 */

void mock_regs_init(void)
{
  mock_regs_clear();

  /* Set reasonable defaults for USB testing */
  g_mock_usb0.cfifoctr = (1U << k_cfifoctr_frdy_bit); /* FIFO ready by default */
  g_mock_usb0.dcpmaxp  = k_usb_cdc_max_packet_fs;     /* 64 bytes for FS */
}

void mock_regs_clear(void)
{
  memset(&g_mock_usb0, 0, sizeof(g_mock_usb0));
  memset(&g_mock_icu, 0, sizeof(g_mock_icu));
  memset(&g_mock_system, 0, sizeof(g_mock_system));
}

void mock_usb0_set_intsts0(uint16_t value)
{
  g_mock_usb0.intsts0 = value;
}

void mock_usb0_set_dvsq(uint16_t dvsq)
{
  /* Clear existing DVSQ bits and set new value */
  g_mock_usb0.intsts0 = (g_mock_usb0.intsts0 & ~k_intsts0_dvsq_mask) |
                        ((dvsq << k_intsts0_dvsq_shift) & k_intsts0_dvsq_mask);
}

void mock_usb0_set_ctsq(uint16_t ctsq)
{
  /* Clear existing CTSQ bits and set new value */
  g_mock_usb0.intsts0 = (g_mock_usb0.intsts0 & ~k_intsts0_ctsq_mask) | (ctsq & k_intsts0_ctsq_mask);
}

void mock_usb0_set_fifo_ready(uint8_t ready)
{
  if (ready) {
    g_mock_usb0.cfifoctr |= (1U << k_cfifoctr_frdy_bit);
  } else {
    g_mock_usb0.cfifoctr &= ~(1U << k_cfifoctr_frdy_bit);
  }
}

void mock_usb0_set_fifo_dtln(uint16_t len)
{
  g_mock_usb0.cfifoctr =
    (g_mock_usb0.cfifoctr & ~k_cfifoctr_dtln_mask) | (len & k_cfifoctr_dtln_mask);
}

void mock_usb0_set_pipe1_busy(uint8_t busy)
{
  if (busy) {
    g_mock_usb0.pipe1ctr |= (1U << k_pipectr_pbusy_bit);
  } else {
    g_mock_usb0.pipe1ctr &= ~(1U << k_pipectr_pbusy_bit);
  }
}

void mock_usb0_set_line_status(uint16_t lnst)
{
  g_mock_usb0.syssts0 =
    (g_mock_usb0.syssts0 & ~k_usb_syssts0_lnst_mask) | (lnst & k_usb_syssts0_lnst_mask);
}

/* =============================================================================
 * Logging Mock Implementation
 * =============================================================================
 */

void mock_log_init(void)
{
  memset(&g_mock_log, 0, sizeof(g_mock_log));
  g_mock_log.print_enabled = false;
}

void mock_log_clear(void)
{
  memset(&g_mock_log.entries, 0, sizeof(g_mock_log.entries));
  g_mock_log.write_index = 0;
  g_mock_log.count       = 0;
  g_mock_log.error_count = 0;
  g_mock_log.warn_count  = 0;
  g_mock_log.info_count  = 0;
  g_mock_log.debug_count = 0;
}

void mock_log_set_print(bool enable)
{
  g_mock_log.print_enabled = enable;
}

/**
 * @brief Internal function to record a log entry
 */
static void internal_log_record(mock_log_level_t level, const char* tag, const char* msg)
{
  mock_log_entry_t* entry = &g_mock_log.entries[g_mock_log.write_index];

  entry->level = level;

  if (tag != NULL) {
    strncpy(entry->tag, tag, k_mock_log_max_tag_len - 1);
    entry->tag[k_mock_log_max_tag_len - 1] = '\0';
  } else {
    entry->tag[0] = '\0';
  }

  if (msg != NULL) {
    strncpy(entry->msg, msg, k_mock_log_max_msg_len - 1);
    entry->msg[k_mock_log_max_msg_len - 1] = '\0';
  } else {
    entry->msg[0] = '\0';
  }

  g_mock_log.write_index = (g_mock_log.write_index + 1) % k_mock_log_history_size;
  g_mock_log.count++;

  if (g_mock_log.print_enabled) {
    const char* level_str = "???";
    switch (level) {
      case k_mock_log_level_error:
        level_str = "ERROR";
        break;
      case k_mock_log_level_warn:
        level_str = "WARN";
        break;
      case k_mock_log_level_info:
        level_str = "INFO";
        break;
      case k_mock_log_level_debug:
        level_str = "DEBUG";
        break;
    }
    printf("[%s] %s: %s\n", level_str, tag ? tag : "???", msg ? msg : "");
  }
}

void rx_log_error(const char* tag, const char* msg)
{
  internal_log_record(k_mock_log_level_error, tag, msg);
  g_mock_log.error_count++;
}

void rx_log_warn(const char* tag, const char* msg)
{
  internal_log_record(k_mock_log_level_warn, tag, msg);
  g_mock_log.warn_count++;
}

void rx_log_info(const char* tag, const char* msg)
{
  internal_log_record(k_mock_log_level_info, tag, msg);
  g_mock_log.info_count++;
}

void rx_log_debug(const char* tag, const char* msg)
{
  internal_log_record(k_mock_log_level_debug, tag, msg);
  g_mock_log.debug_count++;
}

uint32_t mock_log_get_error_count(void)
{
  return g_mock_log.error_count;
}

uint32_t mock_log_get_warn_count(void)
{
  return g_mock_log.warn_count;
}

bool mock_log_contains(mock_log_level_t level, const char* tag, const char* msg)
{
  uint32_t search_count =
    (g_mock_log.count < k_mock_log_history_size) ? g_mock_log.count : k_mock_log_history_size;

  for (uint32_t i = 0; i < search_count; i++) {
    mock_log_entry_t* entry = &g_mock_log.entries[i];

    if (entry->level != level) {
      continue;
    }

    if (tag != NULL && strstr(entry->tag, tag) == NULL) {
      continue;
    }

    if (msg != NULL && strstr(entry->msg, msg) == NULL) {
      continue;
    }

    return true;
  }

  return false;
}

bool mock_log_get_last(mock_log_level_t level, mock_log_entry_t* out_entry)
{
  uint32_t search_count =
    (g_mock_log.count < k_mock_log_history_size) ? g_mock_log.count : k_mock_log_history_size;

  /* Search backwards for most recent entry at this level */
  for (int32_t i = search_count - 1; i >= 0; i--) {
    uint32_t idx = (g_mock_log.write_index - 1 - (uint32_t)i + k_mock_log_history_size) %
                   k_mock_log_history_size;

    if (g_mock_log.entries[idx].level == level) {
      if (out_entry != NULL) {
        *out_entry = g_mock_log.entries[idx];
      }
      return true;
    }
  }

  return false;
}

/* =============================================================================
 * ThreadX Mock Implementation
 * =============================================================================
 */

void mock_tx_init(void)
{
  memset(&g_mock_tx, 0, sizeof(g_mock_tx));
  g_mock_tx.interrupts_enabled = true;
}

void mock_tx_clear(void)
{
  g_mock_tx.sleep_calls = 0;
  memset(g_mock_tx.sleep_ticks, 0, sizeof(g_mock_tx.sleep_ticks));
}

UINT tx_thread_sleep(ULONG timer_ticks)
{
  if (g_mock_tx.sleep_calls < k_mock_tx_max_sleep_calls) {
    g_mock_tx.sleep_ticks[g_mock_tx.sleep_calls] = timer_ticks;
  }
  g_mock_tx.sleep_calls++;
  g_mock_tx.current_time += timer_ticks;

  return TX_SUCCESS;
}

ULONG tx_time_get(void)
{
  return g_mock_tx.current_time;
}

UINT tx_interrupt_control(UINT new_posture)
{
  UINT old_posture             = g_mock_tx.interrupts_enabled ? 1 : 0;
  g_mock_tx.interrupts_enabled = (new_posture != 0);
  return old_posture;
}

uint32_t mock_tx_get_sleep_count(void)
{
  return g_mock_tx.sleep_calls;
}

ULONG mock_tx_get_total_sleep_ticks(void)
{
  ULONG    total = 0;
  uint32_t count = g_mock_tx.sleep_calls;
  if (count > k_mock_tx_max_sleep_calls) {
    count = k_mock_tx_max_sleep_calls;
  }

  for (uint32_t i = 0; i < count; i++) {
    total += g_mock_tx.sleep_ticks[i];
  }

  return total;
}

void mock_tx_set_time(uint32_t ticks)
{
  g_mock_tx.current_time = ticks;
}

void mock_tx_advance_time(uint32_t ticks)
{
  g_mock_tx.current_time += ticks;
}
