/**
 * @file mock_tx_api.c
 * @brief Mock ThreadX API Implementation for Unit Testing
 *
 * @details
 * Provides mock implementation of ThreadX thread functions for unit testing.
 * Allows controlling return values and tracking call counts.
 *
 * @author Locked, Inc.
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include "tx_api.h"

/* External function to reset task mock states */
extern void mock_tasks_reset(void);

/* =============================================================================
 * Static State
 * =============================================================================
 */

static tx_status s_thread_create_return      = TX_SUCCESS;
static uint32_t  s_thread_create_count       = 0;
static tx_status s_event_flags_create_return = TX_SUCCESS;

#ifdef MOCK_TX_THREAD_DELETE
static tx_status s_thread_delete_return = TX_SUCCESS;
#endif /* MOCK_TX_THREAD_DELETE */

#ifdef MOCK_TX_EVENT_FLAGS_GET
static uint32_t s_event_flags_get_call_count    = 0;
static uint32_t s_event_flags_get_no_evt_count  = 0;
static void (*s_get_success_callback)(void)     = nullptr;
static ULONG s_event_flags_get_mask             = ~(ULONG)0; /* All bits set by default */
#endif /* MOCK_TX_EVENT_FLAGS_GET */

#ifdef MOCK_TX_THREAD_SLEEP
static void (*s_sleep_callback)(void) = nullptr;
#endif /* MOCK_TX_THREAD_SLEEP */

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_tx_reset(void)
{
  s_thread_create_return      = TX_SUCCESS;
  s_thread_create_count       = 0;
  s_event_flags_create_return = TX_SUCCESS;

#ifdef MOCK_TX_EVENT_FLAGS_GET
  s_event_flags_get_call_count   = 0;
  s_event_flags_get_no_evt_count = 0;
  s_get_success_callback         = nullptr;
  s_event_flags_get_mask         = ~(ULONG)0;
#endif /* MOCK_TX_EVENT_FLAGS_GET */

#ifdef MOCK_TX_THREAD_SLEEP
  s_sleep_callback = nullptr;
#endif /* MOCK_TX_THREAD_SLEEP */

  /* Also reset task creation states */
  mock_tasks_reset();

#ifdef MOCK_TX_THREAD_DELETE
  s_thread_delete_return = TX_SUCCESS;
#endif /* MOCK_TX_THREAD_DELETE */
}

void mock_tx_set_thread_create_return(tx_status status)
{
  s_thread_create_return = status;
}

void mock_tx_set_event_flags_create_return(tx_status status)
{
  s_event_flags_create_return = status;
}

#ifdef MOCK_TX_THREAD_DELETE
void mock_tx_set_thread_delete_return(tx_status status)
{
  s_thread_delete_return = status;
}
#endif /* MOCK_TX_THREAD_DELETE */

bool mock_tx_was_thread_create_called(void)
{
  return s_thread_create_count > 0;
}

uint32_t mock_tx_get_thread_create_count(void)
{
  return s_thread_create_count;
}

/* =============================================================================
 * Overridden ThreadX Functions
 *
 * NOTE: These override the static inline versions when this .c file is linked.
 * The linker will use these implementations instead of the inline ones.
 * =============================================================================
 */

#ifdef MOCK_TX_THREAD_CREATE

tx_status tx_thread_create(TX_THREAD* thread_ptr,
                           CHAR*      name_ptr,
                           VOID (*entry_function)(ULONG),
                           ULONG                    entry_input,
                           VOID*                    stack_start,
                           ULONG                    stack_size,
                           UINT                     priority,
                           UINT                     preempt_threshold,
                           tx_time_slice_option_t   time_slice,
                           tx_thread_start_option_t auto_start)
{
  (void)entry_function;
  (void)entry_input;
  (void)stack_start;
  (void)stack_size;
  (void)priority;
  (void)preempt_threshold;
  (void)time_slice;
  (void)auto_start;

  s_thread_create_count++;

  /* Return configured error if set */
  if (s_thread_create_return != TX_SUCCESS) {
    return s_thread_create_return;
  }

  /* Pre-condition: Validate input pointer */
  if (thread_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  /* Initialize thread structure */
  thread_ptr->tx_thread_name = name_ptr;
  thread_ptr->tx_thread_id   = k_tx_thread_magic;

  return TX_SUCCESS;
}

#endif /* MOCK_TX_THREAD_CREATE */

#ifdef MOCK_TX_EVENT_FLAGS_CREATE

tx_status tx_event_flags_create(TX_EVENT_FLAGS_GROUP* group_ptr, CHAR* name_ptr)
{
  /* Return configured error if set */
  if (s_event_flags_create_return != TX_SUCCESS) {
    return s_event_flags_create_return;
  }

  /* Pre-condition: Validate input pointer */
  if (group_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  /* Initialize event flags group structure */
  group_ptr->tx_event_flags_name = name_ptr;
  group_ptr->tx_event_flags_id   = k_tx_event_flags_magic;
  group_ptr->tx_event_flags      = 0;

  return TX_SUCCESS;
}

#endif /* MOCK_TX_EVENT_FLAGS_CREATE */

#ifdef MOCK_TX_EVENT_FLAGS_GET

void mock_tx_set_event_flags_get_no_events_count(uint32_t success_after_n_calls)
{
  s_event_flags_get_call_count   = 0;
  s_event_flags_get_no_evt_count = success_after_n_calls;
}

void mock_tx_event_flags_get_reset_count(void)
{
  s_event_flags_get_call_count = 0;
}

void mock_tx_event_flags_get_set_on_success_cb(void (*cb)(void))
{
  s_get_success_callback = cb;
}

void mock_tx_event_flags_get_set_mask(ULONG mask)
{
  s_event_flags_get_mask = mask;
}

tx_status tx_event_flags_get(TX_EVENT_FLAGS_GROUP* group_ptr,
                             ULONG                 requested_flags,
                             UINT                  get_option,
                             ULONG*                actual_flags_ptr,
                             tx_wait_option        wait_option)
{
  (void)wait_option;

  if (group_ptr == nullptr || actual_flags_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  if (group_ptr->tx_event_flags_id != k_tx_event_flags_magic) {
    return TX_DELETED;
  }

  s_event_flags_get_call_count++;

  /* Return TX_NO_EVENTS for the first N calls if configured */
  if (s_event_flags_get_no_evt_count > 0 &&
      s_event_flags_get_call_count <= s_event_flags_get_no_evt_count) {
    *actual_flags_ptr = 0;
    return TX_NO_EVENTS;
  }

  /* Return requested flags, filtered by mask */
  *actual_flags_ptr = requested_flags & s_event_flags_get_mask;

  /* Clear flags if requested */
  if (get_option == TX_OR_CLEAR || get_option == TX_AND_CLEAR) {
    group_ptr->tx_event_flags &= ~requested_flags;
  }

  /* Invoke success callback if registered */
  if (s_get_success_callback != nullptr) {
    s_get_success_callback();
  }

  return TX_SUCCESS;
}

#endif /* MOCK_TX_EVENT_FLAGS_GET */

#ifdef MOCK_TX_THREAD_SLEEP

void mock_tx_set_sleep_callback(void (*cb)(void))
{
  s_sleep_callback = cb;
}

tx_status tx_thread_sleep(ULONG timer_ticks)
{
  if (timer_ticks == TX_WAIT_FOREVER) {
    return TX_WAIT_ERROR;
  }

  /* Invoke registered callback before returning */
  if (s_sleep_callback != nullptr) {
    s_sleep_callback();
  }

  return TX_SUCCESS;
}

#endif /* MOCK_TX_THREAD_SLEEP */

#ifdef MOCK_TX_THREAD_DELETE

tx_status tx_thread_delete(TX_THREAD* thread_ptr)
{
  /* Return configured error if set (one-shot) */
  if (s_thread_delete_return != TX_SUCCESS) {
    const tx_status ret = s_thread_delete_return;
    s_thread_delete_return = TX_SUCCESS;
    return ret;
  }

  /* Pre-condition: Validate input pointer */
  if (thread_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  if (thread_ptr->tx_thread_id != k_tx_thread_magic) {
    return TX_PTR_ERROR;
  }

  thread_ptr->tx_thread_id = k_tx_invalid_id;
  return TX_SUCCESS;
}

#endif /* MOCK_TX_THREAD_DELETE */
