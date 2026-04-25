/**
 * @file rx_isr_log.h
 * @brief ISR-safe deferred logging facility
 *
 * @details
 * Single-producer-single-consumer (SPSC) ring buffer that lets ISR handlers
 * record a log "event" without ever calling into rx_log_* directly. The
 * rx_log_* macros funnel through rx_log_uart.c which acquires a ThreadX
 * mutex with TX_WAIT_FOREVER -- legal from a task, illegal from interrupt
 * context (deadlock or undefined behavior under ThreadX).
 *
 * ## Producer side (ISR)
 *   Push a record with rx_isr_log_push(). The call is lock-free: it reads
 *   tail, computes (head + 1) % capacity, and either advances head with the
 *   record stored or bumps a drop counter. No ThreadX call, no I/O, no
 *   busy-wait. Safe to invoke from any interrupt-priority context.
 *
 * ## Consumer side (task)
 *   A low-priority task (currently comm_task) calls rx_isr_log_drain() once
 *   per poll tick. The drain reads each pending record, looks up its format
 *   string from a static table, and emits via the regular rx_log_* macros so
 *   the message ends up wrapped in the same UART log frame as task-context
 *   logs. After all records are drained, if drop_count advanced since the
 *   last drain it emits a single "ISR log overflow: dropped N records"
 *   summary so observers know events were lost.
 *
 * ## Ring sizing
 *   k_rx_isr_log_capacity = 32 records, 8 bytes each = 256 bytes total. At
 *   the comm_task drain rate of 100 Hz this absorbs an event burst of
 *   ~3200/s before any drops occur, far above the steady-state USB ISR
 *   event rate of ~10-20 events/s during enumeration and ~0/s after.
 *
 * ## Event model
 *   Each event is identified by an rx_isr_log_event_id_t enum value which
 *   indexes a static const table of {tag, format, level} tuples. A 32-bit
 *   `value` field carries one optional auxiliary integer (port id, error
 *   code, register snapshot, etc.). This keeps records fixed-size and
 *   trivially copyable, avoiding any sprintf-style formatting in the ISR.
 *
 * @author Locked, Inc.
 * @date 2026-04-25
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum rx_isr_log_constants_t
 * @brief Sizing and queue limits for the ISR log ring
 */
typedef enum : uint16_t {
  k_rx_isr_log_capacity =
    32U, /**< Number of records the ring can hold before dropping (power of two) */
  k_rx_isr_log_capacity_mask =
    31U, /**< Bitwise mask for index modulo (capacity - 1). MUST track capacity. */
} rx_isr_log_constants_t;

/**
 * @enum rx_isr_log_event_id_t
 * @brief Identifier for every distinct event the ISR layer can log
 *
 * @details
 * The drain hook indexes a static format-string table by this enum so the
 * ISR never needs to reach for a literal string at runtime. Add a new entry
 * here, add a matching row in s_isr_log_event_table[] in rx_isr_log.c, and
 * the new event is wired up. Keep entries grouped by source module.
 */
typedef enum : uint8_t {
  k_isr_log_event_none = 0, /**< Reserved sentinel; never enqueued */

  /* USB ISR events (rx_usb_isr.c) */
  k_isr_log_event_usb_vbus_se0,
  k_isr_log_event_usb_vbus_fs_j,
  k_isr_log_event_usb_dvst_powered,
  k_isr_log_event_usb_dvst_default,
  k_isr_log_event_usb_dvst_address,
  k_isr_log_event_usb_dvst_configured,
  k_isr_log_event_usb_dvst_suspended,
  k_isr_log_event_usb_dvst_unknown,
  k_isr_log_event_usb_resume,
  k_isr_log_event_usb_ctrt_seq_err,

  /* USB CDC ISR-context events (rx_usb_cdc.c) */
  k_isr_log_event_cdc_descriptor_short_write,
  k_isr_log_event_cdc_pipe_rolled_back,
  k_isr_log_event_cdc_port0_bulk_in_failed,
  k_isr_log_event_cdc_port0_bulk_out_failed,
  k_isr_log_event_cdc_port0_int_in_failed,
  k_isr_log_event_cdc_port1_bulk_in_failed,
  k_isr_log_event_cdc_port1_bulk_out_failed,
  k_isr_log_event_cdc_port1_int_in_failed,
  k_isr_log_event_cdc_port2_bulk_in_failed,
  k_isr_log_event_cdc_port2_bulk_out_failed,
  k_isr_log_event_cdc_port2_int_in_failed,
  k_isr_log_event_cdc_composite_configured,
  k_isr_log_event_cdc_line_coding_short_write,

  k_isr_log_event_count, /**< Sentinel: total number of defined events */
} rx_isr_log_event_id_t;

/**
 * @struct rx_isr_log_record_t
 * @brief One pending log emission queued by an ISR for the drain hook
 */
typedef struct {
  uint8_t  event_id; /**< rx_isr_log_event_id_t (kept as uint8_t for fixed packing) */
  uint8_t  reserved0;
  uint16_t reserved1;
  uint32_t value; /**< Auxiliary value (event-specific; 0 if unused) */
} rx_isr_log_record_t;

/**
 * @struct rx_isr_log_stats_t
 * @brief Diagnostic counters for the ISR log ring
 */
typedef struct {
  uint32_t total_pushed;  /**< Records accepted by the ring */
  uint32_t total_drained; /**< Records emitted via rx_log_* by the drain hook */
  uint32_t total_dropped; /**< Records discarded because the ring was full */
  uint32_t high_water;    /**< Maximum observed occupancy (records) */
} rx_isr_log_stats_t;

/**
 * @brief Push one log event from interrupt context
 *
 * @details
 * Lock-free SPSC enqueue. Reads tail, computes the next head; if the ring
 * is full, increments the drop counter and returns false. Otherwise writes
 * the record at the current head slot, then advances head. Safe to call
 * from any ISR or task; the only producer is the ISR by convention but
 * the implementation is correct for any single-thread producer because the
 * head/tail are written by exactly one side each.
 *
 * @param[in] event_id One of rx_isr_log_event_id_t. k_isr_log_event_none
 *                     and values >= k_isr_log_event_count are silently
 *                     dropped (counted as drops).
 * @param[in] value    Auxiliary 32-bit value (0 if the event has no value)
 *
 * @return true if the record was queued, false if dropped
 *
 * @note ISR-SAFE: no mutex, no I/O, no allocation, no busy-wait.
 */
bool rx_isr_log_push(rx_isr_log_event_id_t event_id, uint32_t value);

/**
 * @brief Drain all pending records and emit them via rx_log_*
 *
 * @details
 * Called from a task-context periodic hook (currently comm_task at 100 Hz).
 * Walks tail forward to head, emitting one rx_log_* call per record. Also
 * detects and reports newly dropped records via a single summary log.
 *
 * @return Number of records drained on this call
 *
 * @note MUST be called from task context only -- it invokes the regular
 *       rx_log_* macros which acquire a ThreadX mutex.
 */
uint32_t rx_isr_log_drain(void);

/**
 * @brief Snapshot diagnostic counters
 *
 * @param[out] stats Statistics destination (nullptr is silently ignored)
 */
void rx_isr_log_get_stats(rx_isr_log_stats_t* stats);

#ifdef UNIT_TEST
/**
 * @brief Reset all ring buffer and statistics state (unit tests only)
 */
void rx_isr_log_test_reset_state(void);

/**
 * @brief Read pending record count without draining (unit tests only)
 *
 * @return Number of records currently queued in the ring (0..k_rx_isr_log_capacity)
 */
uint32_t rx_isr_log_test_pending_count(void);
#endif

#ifdef __cplusplus
}
#endif
