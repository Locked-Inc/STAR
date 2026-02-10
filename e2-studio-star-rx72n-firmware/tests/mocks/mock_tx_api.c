/* tests/mocks/mock_tx_api.c */

/**
 * @file mock_tx_api.c
 * @brief Mock ThreadX API Implementation for Unit Testing
 *
 * @details
 * Provides mock implementation of ThreadX thread functions for unit testing.
 * Allows controlling return values and tracking call counts.
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "tx_api.h"

#include <string.h>

/* External function to reset task mock states */
extern void mock_tasks_reset(void);

/* =============================================================================
 * Static State
 * =============================================================================
 */

static tx_status s_thread_create_return = TX_SUCCESS;
static uint32_t  s_thread_create_count  = 0;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_tx_reset(void)
{
  s_thread_create_return = TX_SUCCESS;
  s_thread_create_count  = 0;

  /* Also reset task creation states */
  mock_tasks_reset();
}

void mock_tx_set_thread_create_return(tx_status status)
{
  s_thread_create_return = status;
}

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
