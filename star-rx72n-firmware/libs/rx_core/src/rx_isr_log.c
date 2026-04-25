/**
 * @file rx_isr_log.c
 * @brief ISR-safe deferred log queue: SPSC ring + drain emitter
 *
 * @details
 * See rx_isr_log.h for the high-level rationale. This file implements the
 * ring buffer and the format-string lookup table used by the drain hook.
 *
 * The ring is sized to k_rx_isr_log_capacity (32) records and uses the
 * power-of-two trick (capacity_mask) so head/tail advancement is a cheap
 * AND rather than a modulo. head and tail are 32-bit and wrap freely; the
 * occupancy is computed as (head - tail) and the slot index is
 * (head & mask).
 *
 * @author Locked, Inc.
 * @date 2026-04-25
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_isr_log.h"

#include <stdbool.h>
#include <stdint.h>

#include "rx_log.h"

/* =============================================================================
 * Static state
 * ============================================================================= */

/**
 * @brief Ring buffer storage
 *
 * @details
 * Records are written by the producer (ISR) at index (s_head & mask) and
 * read by the consumer (drain) at index (s_tail & mask). Volatile so the
 * compiler does not hoist either side's read across the matching producer
 * or consumer write.
 */
static volatile rx_isr_log_record_t s_ring[k_rx_isr_log_capacity];

/** @brief Producer write cursor (monotonically increases, wraps at uint32) */
static volatile uint32_t s_head;

/** @brief Consumer read cursor (monotonically increases, wraps at uint32) */
static volatile uint32_t s_tail;

/** @brief Diagnostic counters (writes to dropped_count happen from ISR) */
static volatile uint32_t s_total_pushed;
static volatile uint32_t s_total_drained;
static volatile uint32_t s_total_dropped;
static volatile uint32_t s_high_water;

/** @brief drop count observed at the previous drain (lets us emit a delta summary) */
static uint32_t s_drop_count_last_seen;

/* =============================================================================
 * Format-string table
 * =============================================================================
 * One row per rx_isr_log_event_id_t value (skipping k_isr_log_event_none).
 * Drain looks up by event_id and emits a rx_log_* call with the right level
 * and tag. has_value controls whether the auxiliary value field is logged.
 */

typedef enum : uint8_t {
  k_isr_log_level_debug = 0,
  k_isr_log_level_info  = 1,
  k_isr_log_level_warn  = 2,
  k_isr_log_level_error = 3,
} rx_isr_log_level_t;

typedef struct {
  const char*        tag;
  const char*        message;
  rx_isr_log_level_t level;
  bool               has_value;
} rx_isr_log_event_meta_t;

/* Event metadata table. Indexed by event_id. The k_isr_log_event_none entry
 * is unused (drained records are validated against the count sentinel). */
static const rx_isr_log_event_meta_t s_isr_log_event_table[k_isr_log_event_count] = {
  [k_isr_log_event_none] = {.tag       = "ISR_LOG",
                            .message   = "(reserved)",
                            .level     = k_isr_log_level_debug,
                            .has_value = false},

  /* USB ISR events */
  [k_isr_log_event_usb_vbus_se0]        = {.tag       = "USB_ISR",
                                           .message   = "VBUS: SE0 detected",
                                           .level     = k_isr_log_level_debug,
                                           .has_value = false},
  [k_isr_log_event_usb_vbus_fs_j]       = {.tag       = "USB_ISR",
                                           .message   = "VBUS: FS J-state (connected)",
                                           .level     = k_isr_log_level_debug,
                                           .has_value = false},
  [k_isr_log_event_usb_dvst_powered]    = {.tag       = "USB_ISR",
                                           .message   = "DVST: Powered state",
                                           .level     = k_isr_log_level_debug,
                                           .has_value = false},
  [k_isr_log_event_usb_dvst_default]    = {.tag       = "USB_ISR",
                                           .message   = "DVST: Default state (bus reset complete)",
                                           .level     = k_isr_log_level_debug,
                                           .has_value = false},
  [k_isr_log_event_usb_dvst_address]    = {.tag       = "USB_ISR",
                                           .message   = "DVST: Address state",
                                           .level     = k_isr_log_level_debug,
                                           .has_value = false},
  [k_isr_log_event_usb_dvst_configured] = {.tag       = "USB_ISR",
                                           .message   = "DVST: Configured state",
                                           .level     = k_isr_log_level_debug,
                                           .has_value = false},
  [k_isr_log_event_usb_dvst_suspended]  = {.tag       = "USB_ISR",
                                           .message   = "DVST: Suspended state",
                                           .level     = k_isr_log_level_debug,
                                           .has_value = false},
  [k_isr_log_event_usb_dvst_unknown]    = {.tag       = "USB_ISR",
                                           .message   = "DVST: Unknown state",
                                           .level     = k_isr_log_level_warn,
                                           .has_value = false},
  [k_isr_log_event_usb_resume]          = {.tag       = "USB_ISR",
                                           .message   = "Resume detected",
                                           .level     = k_isr_log_level_debug,
                                           .has_value = false},
  [k_isr_log_event_usb_ctrt_seq_err]    = {.tag       = "USB_ISR",
                                           .message   = "CTRT: sequence error, stalled",
                                           .level     = k_isr_log_level_warn,
                                           .has_value = false},

  /* USB CDC ISR-context events */
  [k_isr_log_event_cdc_descriptor_short_write]  = {.tag       = "USB_CDC",
                                                   .message   = "Descriptor write incomplete",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
  [k_isr_log_event_cdc_pipe_rolled_back]        = {.tag       = "USB_CDC",
                                                   .message   = "Rolled back pipes on config failure",
                                                   .level     = k_isr_log_level_debug,
                                                   .has_value = false},
  [k_isr_log_event_cdc_port0_bulk_in_failed]    = {.tag = "USB_CDC",
                                                   .message =
                                                     "Failed to configure Port 0 Bulk IN pipe",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
  [k_isr_log_event_cdc_port0_bulk_out_failed]   = {.tag = "USB_CDC",
                                                   .message =
                                                     "Failed to configure Port 0 Bulk OUT pipe",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
  [k_isr_log_event_cdc_port0_int_in_failed]     = {.tag = "USB_CDC",
                                                   .message =
                                                     "Failed to configure Port 0 Interrupt IN pipe",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
  [k_isr_log_event_cdc_port1_bulk_in_failed]    = {.tag = "USB_CDC",
                                                   .message =
                                                     "Failed to configure Port 1 Bulk IN pipe",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
  [k_isr_log_event_cdc_port1_bulk_out_failed]   = {.tag = "USB_CDC",
                                                   .message =
                                                     "Failed to configure Port 1 Bulk OUT pipe",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
  [k_isr_log_event_cdc_port1_int_in_failed]     = {.tag = "USB_CDC",
                                                   .message =
                                                     "Failed to configure Port 1 Interrupt IN pipe",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
  [k_isr_log_event_cdc_port2_bulk_in_failed]    = {.tag = "USB_CDC",
                                                   .message =
                                                     "Failed to configure Port 2 Bulk IN pipe",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
  [k_isr_log_event_cdc_port2_bulk_out_failed]   = {.tag = "USB_CDC",
                                                   .message =
                                                     "Failed to configure Port 2 Bulk OUT pipe",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
  [k_isr_log_event_cdc_port2_int_in_failed]     = {.tag = "USB_CDC",
                                                   .message =
                                                     "Failed to configure Port 2 Interrupt IN pipe",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
  [k_isr_log_event_cdc_composite_configured]    = {.tag       = "USB_CDC",
                                                   .message   = "Composite device configured",
                                                   .level     = k_isr_log_level_info,
                                                   .has_value = false},
  [k_isr_log_event_cdc_line_coding_short_write] = {.tag       = "USB_CDC",
                                                   .message   = "Line coding write incomplete",
                                                   .level     = k_isr_log_level_error,
                                                   .has_value = false},
};

/* =============================================================================
 * Producer (ISR-safe)
 * ============================================================================= */

bool rx_isr_log_push(rx_isr_log_event_id_t event_id, uint32_t value)
{
  /* Reject sentinel and out-of-range ids -- count as drops so a corrupted
   * call site shows up in stats rather than silently mis-emitting later. */
  if ((event_id == k_isr_log_event_none) ||
      ((uint32_t)event_id >= (uint32_t)k_isr_log_event_count)) {
    s_total_dropped++;
    return false;
  }

  /* SPSC bounded ring: the consumer (drain) only advances s_tail, the
   * producer (this function) only advances s_head. Read tail snapshot
   * once -- it can only move forward between this read and our store, so
   * the worst case is we observe one slot fewer free space than reality
   * (we drop a record when there is in fact room). That is a benign loss
   * compared to corrupting the ring, and the drain hook will catch up
   * within one tick. */
  const uint32_t head = s_head;
  const uint32_t tail = s_tail;
  const uint32_t used = head - tail;

  if (used >= (uint32_t)k_rx_isr_log_capacity) {
    s_total_dropped++;
    return false;
  }

  const uint32_t slot   = head & (uint32_t)k_rx_isr_log_capacity_mask;
  s_ring[slot].event_id = (uint8_t)event_id;
  s_ring[slot].value    = value;
  /* reserved fields zeroed for layout determinism */
  s_ring[slot].reserved0 = 0U;
  s_ring[slot].reserved1 = 0U;

  /* Publish the record by advancing s_head. The volatile write acts as a
   * compiler barrier so the slot writes above are not reordered past it.
   * On RX (in-order, single core) this is sufficient -- no DMB needed. */
  s_head = head + 1U;

  s_total_pushed++;
  const uint32_t new_used = (head + 1U) - tail;
  if (new_used > s_high_water) {
    s_high_water = new_used;
  }
  return true;
}

/* =============================================================================
 * Consumer (task-context drain)
 * ============================================================================= */

/**
 * @brief Emit one drained record via the appropriate rx_log_* macro
 */
static void internal_emit_record(const rx_isr_log_record_t* rec)
{
  /* Validate the event id -- a torn record (producer interrupted us
   * mid-read) would be caught here as a 0 / out-of-range id and we drop
   * it as a soft error rather than dereferencing the table OOB. */
  if (rec->event_id >= k_isr_log_event_count) {
    return;
  }
  if (rec->event_id == k_isr_log_event_none) {
    return;
  }

  const rx_isr_log_event_meta_t* meta = &s_isr_log_event_table[rec->event_id];

  if (meta->has_value) {
    switch (meta->level) {
      case k_isr_log_level_error:
        rx_log_error_val(meta->tag, meta->message, rec->value);
        break;
      case k_isr_log_level_warn:
        rx_log_warn_val(meta->tag, meta->message, rec->value);
        break;
      case k_isr_log_level_info:
        rx_log_info_val(meta->tag, meta->message, rec->value);
        break;
      case k_isr_log_level_debug:
      default:
        rx_log_debug_val(meta->tag, meta->message, rec->value);
        break;
    }
  } else {
    switch (meta->level) {
      case k_isr_log_level_error:
        rx_log_error(meta->tag, meta->message);
        break;
      case k_isr_log_level_warn:
        rx_log_warn(meta->tag, meta->message);
        break;
      case k_isr_log_level_info:
        rx_log_info(meta->tag, meta->message);
        break;
      case k_isr_log_level_debug:
      default:
        rx_log_debug(meta->tag, meta->message);
        break;
    }
  }
}

uint32_t rx_isr_log_drain(void)
{
  uint32_t drained = 0U;

  /* Snapshot head once -- new pushes after this point will be picked up by
   * the next drain call. Bounding the loop by the snapshot keeps execution
   * time per drain bounded (NASA Rule 2) even if the ISR is spamming the
   * ring concurrently. */
  const uint32_t head_snapshot = s_head;

  while (s_tail != head_snapshot) {
    const uint32_t            slot = s_tail & (uint32_t)k_rx_isr_log_capacity_mask;
    const rx_isr_log_record_t rec  = {
       .event_id  = s_ring[slot].event_id,
       .reserved0 = s_ring[slot].reserved0,
       .reserved1 = s_ring[slot].reserved1,
       .value     = s_ring[slot].value,
    };

    internal_emit_record(&rec);

    /* Advance tail AFTER reading the record so the producer cannot reuse
     * the slot before we are done with it. */
    s_tail = s_tail + 1U;
    drained++;
    s_total_drained++;
  }

  /* Emit a single summary log if any drops occurred since the previous
   * drain. We deliberately use _val so the count surfaces in the message. */
  const uint32_t cur_drops = s_total_dropped;
  if (cur_drops > s_drop_count_last_seen) {
    const uint32_t delta = cur_drops - s_drop_count_last_seen;
    rx_log_warn_val("ISR_LOG", "ISR log overflow: dropped records", delta);
    s_drop_count_last_seen = cur_drops;
  }

  return drained;
}

void rx_isr_log_get_stats(rx_isr_log_stats_t* stats)
{
  if (stats == NULL) {
    return;
  }
  stats->total_pushed  = s_total_pushed;
  stats->total_drained = s_total_drained;
  stats->total_dropped = s_total_dropped;
  stats->high_water    = s_high_water;
}

#ifdef UNIT_TEST
void rx_isr_log_test_reset_state(void)
{
  s_head                 = 0U;
  s_tail                 = 0U;
  s_total_pushed         = 0U;
  s_total_drained        = 0U;
  s_total_dropped        = 0U;
  s_high_water           = 0U;
  s_drop_count_last_seen = 0U;
  for (uint32_t i = 0U; i < (uint32_t)k_rx_isr_log_capacity; i++) {
    s_ring[i].event_id  = 0U;
    s_ring[i].reserved0 = 0U;
    s_ring[i].reserved1 = 0U;
    s_ring[i].value     = 0U;
  }
}

uint32_t rx_isr_log_test_pending_count(void)
{
  return s_head - s_tail;
}
#endif
