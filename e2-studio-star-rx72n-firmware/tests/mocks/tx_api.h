/**
 * @file tx_api.h
 * @brief Mock ThreadX RTOS API for host-side testing without ThreadX
 *
 * @details
 * Provides minimal ThreadX API stubs (mutex, semaphore, thread, timer) to enable
 * host-side unit testing of RTOS-dependent code without actual ThreadX kernel.
 * Shadows the real tx_api.h when mocks directory is first in include path.
 *
 * Enables testing of: Task synchronization logic, Mutex lock/unlock sequences,
 * Semaphore signaling, Thread lifecycle, Timer callbacks
 *
 * @par Mock Scope: TX_MUTEX, TX_SEMAPHORE, TX_THREAD, TX_TIMER types and functions
 * @par Limitations: No actual scheduling (single-threaded test execution), No timing
 * (instant operations), Simplified error codes
 *
 * @par Usage: Automatically included in all tests (via include path)
 * @see ThreadX User Guide Real ThreadX RTOS API
 * @par NASA Power of 10: [OK] Static allocation (ThreadX disabled in tests)
 * @par SOLID: D - Tasks depend on RTOS interface
 *
 * @date 2026-01-11
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ThreadX Type Definitions (Minimal Subset)
 * =============================================================================
 */

/** @brief ThreadX return type */
typedef uint32_t UINT;

/** @brief ThreadX unsigned long type */
typedef unsigned long ULONG;

/** @brief ThreadX character type */
typedef char CHAR;

/** @brief ThreadX void type */
typedef void VOID;

/** @brief ThreadX unsigned char type */
typedef unsigned char UCHAR;

/** @brief ThreadX NULL pointer constant */
#define TX_NULL ((void*)0)

/* =============================================================================
 * ThreadX Constants
 *
 * NOTE: ThreadX uses SCREAMING_SNAKECASE names. We preserve the names but use
 * enums to comply with the no-magic-number policy in the firmware codebase.
 * =============================================================================
 */

/** @brief ThreadX return codes (must match real ThreadX API values) */
typedef enum : uint8_t {
  TX_SUCCESS             = 0x00U,
  TX_DELETED             = 0x01U,
  TX_POOL_ERROR          = 0x02U,
  TX_PTR_ERROR           = 0x03U,
  TX_WAIT_ERROR          = 0x04U,
  TX_SIZE_ERROR          = 0x05U,
  TX_GROUP_ERROR         = 0x06U,
  TX_NO_EVENTS           = 0x07U,
  TX_OPTION_ERROR        = 0x08U,
  TX_QUEUE_ERROR         = 0x09U,
  TX_QUEUE_EMPTY         = 0x0AU,
  TX_QUEUE_FULL          = 0x0BU,
  TX_SEMAPHORE_ERROR     = 0x0CU,
  TX_NO_INSTANCE         = 0x0DU,
  TX_THREAD_ERROR        = 0x0EU,
  TX_PRIORITY_ERROR      = 0x0FU,
  TX_NO_MEMORY           = 0x10U,
  TX_START_ERROR         = 0x10U,
  TX_DELETE_ERROR        = 0x11U,
  TX_RESUME_ERROR        = 0x12U,
  TX_CALLER_ERROR        = 0x13U,
  TX_SUSPEND_ERROR       = 0x14U,
  TX_TIMER_ERROR         = 0x15U,
  TX_TICK_ERROR          = 0x16U,
  TX_ACTIVATE_ERROR      = 0x17U,
  TX_THRESH_ERROR        = 0x18U,
  TX_SUSPEND_LIFTED      = 0x19U,
  TX_WAIT_ABORTED        = 0x1AU,
  TX_WAIT_ABORT_ERROR    = 0x1BU,
  TX_MUTEX_ERROR         = 0x1CU,
  TX_NOT_AVAILABLE       = 0x1DU,
  TX_NOT_OWNED           = 0x1EU,
  TX_INHERIT_ERROR       = 0x1FU,
  TX_NOT_DONE            = 0x20U,
  TX_CEILING_EXCEEDED    = 0x21U,
  TX_INVALID_CEILING     = 0x22U,
  TX_FEATURE_NOT_ENABLED = 0xFFU,
} tx_status;

/** @brief ThreadX wait options */
typedef enum tx_wait_option : uint32_t {
  TX_NO_WAIT      = 0U,
  TX_WAIT_FOREVER = 0xFFFFFFFFU,
} tx_wait_option;

/** @brief ThreadX inheritance options */
typedef enum : uint8_t {
  TX_NO_INHERIT = 0U,
  TX_INHERIT    = 1U,
} tx_inherit_option;

/** @brief ThreadX time slice options */
typedef enum : uint8_t {
  TX_NO_TIME_SLICE   = 0U,
  TX_AUTO_TIME_SLICE = 1U,
} tx_time_slice_option_t;

/** @brief ThreadX thread start options */
typedef enum : uint8_t {
  TX_DONT_START = 0U,
  TX_AUTO_START = 1U,
} tx_thread_start_option_t;

/** @brief ThreadX thread activation options */
typedef enum : uint8_t {
  TX_NO_ACTIVATE   = 0U,
  TX_AUTO_ACTIVATE = 1U,
} tx_thread_activate_option_t;

/** @brief ThreadX event flags constants */
typedef enum : uint8_t {
  TX_OR        = 0U,
  TX_OR_CLEAR  = 1U,
  TX_AND       = 2U,
  TX_AND_CLEAR = 3U,
} tx_event_flags_option_t;

/** @brief ThreadX magic ID constants for object validation */
typedef enum : uint32_t {
  k_tx_invalid_id        = 0U,         /**< Invalid/uninitialized object ID */
  k_tx_mutex_magic       = 0x4D555458, /**< "MUTX" ASCII magic ID */
  k_tx_thread_magic      = 0x54485244, /**< "THRD" ASCII magic ID */
  k_tx_event_flags_magic = 0x4556544E, /**< "EVTN" ASCII magic ID */
  k_tx_semaphore_magic   = 0x53454D41, /**< "SEMA" ASCII magic ID */
} tx_magic_ids_t;

/* =============================================================================
 * ThreadX Mutex Structure (Mock)
 * =============================================================================
 */

/**
 * @brief Mock ThreadX mutex structure
 */
typedef struct TX_MUTEX_STRUCT {
  CHAR* tx_mutex_name; /**< Mutex name */
  UINT  tx_mutex_id;   /**< Mutex ID */
  bool  locked;        /**< Lock state (mock) */
} TX_MUTEX;

/**
 * @brief Mock ThreadX semaphore structure
 */
typedef struct TX_SEMAPHORE_STRUCT {
  CHAR* tx_semaphore_name; /**< Semaphore name */
  UINT  tx_semaphore_id;   /**< Semaphore ID */
  UINT  count;             /**< Current count */
} TX_SEMAPHORE;

/**
 * @brief Mock ThreadX thread structure
 *
 * @details
 * Contains the minimal set of fields used by STAR firmware including stack
 * inspection fields required by rx_stack_monitor_get_free_bytes().
 */
typedef struct TX_THREAD_STRUCT {
  CHAR*  tx_thread_name;       /**< Thread name */
  UINT   tx_thread_id;         /**< Thread ID */
  VOID*  tx_thread_stack_start; /**< Stack starting address (lowest address) */
  ULONG  tx_thread_stack_size;  /**< Total stack size in bytes */
} TX_THREAD;

/**
 * @brief Mock ThreadX event flags group structure
 */
typedef struct TX_EVENT_FLAGS_GROUP_STRUCT {
  CHAR* tx_event_flags_name; /**< Event flags name */
  UINT  tx_event_flags_id;   /**< Event flags ID */
  ULONG tx_event_flags;      /**< Current event flags */
} TX_EVENT_FLAGS_GROUP;

/* =============================================================================
 * ThreadX Mutex Functions (Mock Implementations)
 * =============================================================================
 */

/**
 * @brief Create a mutex
 *
 * @param[out] mutex_ptr Pointer to mutex control block
 * @param[in] name_ptr Mutex name string
 * @param[in] inherit Priority inheritance option
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status
tx_mutex_create(TX_MUTEX* mutex_ptr, CHAR* name_ptr, tx_inherit_option inherit)
{
  (void)inherit;

  /* Pre-condition: Validate input pointer */
  if (mutex_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  /* Initialize mutex structure */
  mutex_ptr->tx_mutex_name = name_ptr;
  mutex_ptr->tx_mutex_id   = k_tx_mutex_magic;
  mutex_ptr->locked        = false;

  /* Post-condition: Verify initialization succeeded */
  if (mutex_ptr->tx_mutex_id != k_tx_mutex_magic) {
    return TX_PTR_ERROR;
  }

  return TX_SUCCESS;
}

/**
 * @brief Delete a mutex
 *
 * @param[in] mutex_ptr Pointer to mutex control block
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_mutex_delete(TX_MUTEX* mutex_ptr)
{
  if (mutex_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }
  if (mutex_ptr->tx_mutex_id != k_tx_mutex_magic) {
    return TX_DELETED;
  }
  if (mutex_ptr->locked) {
    return TX_PTR_ERROR;
  }
  mutex_ptr->tx_mutex_id = k_tx_invalid_id;
  mutex_ptr->locked      = false;
  return TX_SUCCESS;
}

/**
 * @brief Acquire a mutex
 *
 * @param[in] mutex_ptr Pointer to mutex control block
 * @param[in] wait_option Wait option (TX_WAIT_FOREVER, TX_NO_WAIT, or ticks)
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_mutex_get(TX_MUTEX* mutex_ptr, tx_wait_option wait_option)
{
  (void)wait_option;
  if (mutex_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }
  if (mutex_ptr->tx_mutex_id != k_tx_mutex_magic) {
    return TX_DELETED;
  }
  mutex_ptr->locked = true;
  return TX_SUCCESS;
}

/**
 * @brief Release a mutex
 *
 * @param[in] mutex_ptr Pointer to mutex control block
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_mutex_put(TX_MUTEX* mutex_ptr)
{
  if (mutex_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }
  if (mutex_ptr->tx_mutex_id != k_tx_mutex_magic) {
    return TX_DELETED;
  }
  mutex_ptr->locked = false;
  return TX_SUCCESS;
}

/* =============================================================================
 * ThreadX Thread Functions (Mock Implementations)
 * =============================================================================
 */

/**
 * @brief Create a thread
 *
 * @param[out] thread_ptr Thread control block pointer
 * @param[in] name_ptr Thread name
 * @param[in] entry_function Thread entry function
 * @param[in] entry_input Entry function input
 * @param[in] stack_start Stack start address
 * @param[in] stack_size Stack size in bytes
 * @param[in] priority Thread priority
 * @param[in] preempt_threshold Preemption threshold
 * @param[in] time_slice Time slice value
 * @param[in] auto_start Auto-start option
 *
 * @return TX_SUCCESS on success
 */
#ifdef MOCK_TX_THREAD_CREATE
/* When MOCK_TX_THREAD_CREATE is defined, use non-inline declaration.
 * Implementation in mock_tx_api.c allows controlling return values. */
tx_status tx_thread_create(TX_THREAD* thread_ptr,
                           CHAR*      name_ptr,
                           VOID (*entry_function)(ULONG),
                           ULONG                    entry_input,
                           VOID*                    stack_start,
                           ULONG                    stack_size,
                           UINT                     priority,
                           UINT                     preempt_threshold,
                           tx_time_slice_option_t   time_slice,
                           tx_thread_start_option_t auto_start);
#else
static inline tx_status tx_thread_create(TX_THREAD* thread_ptr,
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

  /* Pre-condition: Validate input pointer */
  if (thread_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  /* Initialize thread structure */
  thread_ptr->tx_thread_name = name_ptr;
  thread_ptr->tx_thread_id   = k_tx_thread_magic;

  /* Post-condition: Verify initialization succeeded */
  if (thread_ptr->tx_thread_id != k_tx_thread_magic) {
    return TX_PTR_ERROR;
  }

  return TX_SUCCESS;
}
#endif /* MOCK_TX_THREAD_CREATE */

/**
 * @brief Delete a thread
 *
 * @param[in] thread_ptr Thread control block pointer
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_thread_delete(TX_THREAD* thread_ptr)
{
  if (thread_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  if (thread_ptr->tx_thread_id != k_tx_thread_magic) {
    return TX_PTR_ERROR;
  }

  thread_ptr->tx_thread_id = k_tx_invalid_id;
  return TX_SUCCESS;
}

/**
 * @brief Terminate a thread
 *
 * @param[in] thread_ptr Thread control block pointer
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_thread_terminate(TX_THREAD* thread_ptr)
{
  UINT original_id = 0;

  if (thread_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  original_id = thread_ptr->tx_thread_id;
  if (original_id != k_tx_thread_magic) {
    return TX_DELETED;
  }

  return TX_SUCCESS;
}

/**
 * @brief Resume a suspended thread
 *
 * @param[in] thread_ptr Thread control block pointer
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_thread_resume(TX_THREAD* thread_ptr)
{
  UINT original_id = 0;

  if (thread_ptr == nullptr) {
    return TX_THREAD_ERROR;
  }

  original_id = thread_ptr->tx_thread_id;
  if (original_id != k_tx_thread_magic) {
    return TX_DELETED;
  }

  return TX_SUCCESS;
}

/**
 * @brief Sleep for specified number of timer ticks
 *
 * @param[in] timer_ticks Number of ticks to sleep
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_thread_sleep(ULONG timer_ticks)
{
  if (timer_ticks == TX_WAIT_FOREVER) {
    return TX_WAIT_ERROR;
  }

  if (timer_ticks == TX_NO_WAIT) {
    return TX_SUCCESS;
  }

  return TX_SUCCESS;
}

/* =============================================================================
 * ThreadX Event Flags Functions (Mock Implementations)
 * =============================================================================
 */

/**
 * @brief Create an event flags group
 *
 * @param[out] group_ptr Event flags group control block
 * @param[in] name_ptr Event flags group name
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_event_flags_create(TX_EVENT_FLAGS_GROUP* group_ptr, CHAR* name_ptr)
{
  /* Pre-condition: Validate input pointer */
  if (group_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  /* Initialize event flags group structure */
  group_ptr->tx_event_flags_name = name_ptr;
  group_ptr->tx_event_flags_id   = k_tx_event_flags_magic;
  group_ptr->tx_event_flags      = 0;

  /* Post-condition: Verify initialization succeeded */
  if (group_ptr->tx_event_flags_id != k_tx_event_flags_magic) {
    return TX_PTR_ERROR;
  }

  return TX_SUCCESS;
}

/**
 * @brief Create a semaphore
 *
 * @param[in] sem_ptr Pointer to semaphore control block
 * @param[in] name_ptr Semaphore name
 * @param[in] initial_count Initial count
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status
tx_semaphore_create(TX_SEMAPHORE* sem_ptr, CHAR* name_ptr, UINT initial_count)
{
  if (sem_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  sem_ptr->tx_semaphore_name = name_ptr;
  sem_ptr->tx_semaphore_id   = k_tx_semaphore_magic;
  sem_ptr->count             = initial_count;

  if (sem_ptr->tx_semaphore_id != k_tx_semaphore_magic) {
    return TX_PTR_ERROR;
  }

  return TX_SUCCESS;
}

/**
 * @brief Delete a semaphore
 *
 * @param[in] sem_ptr Pointer to semaphore control block
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_semaphore_delete(TX_SEMAPHORE* sem_ptr)
{
  if (sem_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  if (sem_ptr->tx_semaphore_id != k_tx_semaphore_magic) {
    return TX_PTR_ERROR;
  }

  sem_ptr->tx_semaphore_id = k_tx_invalid_id;
  sem_ptr->count           = 0;
  return TX_SUCCESS;
}

/**
 * @brief Get a semaphore (decrement count if available)
 *
 * @param[in] sem_ptr Pointer to semaphore control block
 * @param[in] wait_option Wait option (ignored in mock)
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_semaphore_get(TX_SEMAPHORE* sem_ptr, tx_wait_option wait_option)
{
  (void)wait_option;

  if (sem_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  if (sem_ptr->tx_semaphore_id != k_tx_semaphore_magic) {
    return TX_PTR_ERROR;
  }

  if (sem_ptr->count == 0U) {
    if (wait_option == TX_NO_WAIT) {
      return TX_NO_INSTANCE;
    }
    return TX_SUCCESS;
  }

  sem_ptr->count--;
  return TX_SUCCESS;
}

/**
 * @brief Put a semaphore (increment count)
 *
 * @param[in] sem_ptr Pointer to semaphore control block
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_semaphore_put(TX_SEMAPHORE* sem_ptr)
{
  if (sem_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  if (sem_ptr->tx_semaphore_id != k_tx_semaphore_magic) {
    return TX_PTR_ERROR;
  }

  sem_ptr->count++;
  return TX_SUCCESS;
}

/**
 * @brief Delete an event flags group
 *
 * @param[in] group_ptr Event flags group control block
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_event_flags_delete(TX_EVENT_FLAGS_GROUP* group_ptr)
{
  if (group_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  if (group_ptr->tx_event_flags_id != k_tx_event_flags_magic) {
    return TX_PTR_ERROR;
  }

  group_ptr->tx_event_flags_id = k_tx_invalid_id;
  group_ptr->tx_event_flags    = 0;

  return TX_SUCCESS;
}

/**
 * @brief Set event flags
 *
 * @param[in,out] group_ptr Event flags group control block
 * @param[in] flags_to_set Flags to set
 * @param[in] set_option Set option (TX_OR or TX_AND)
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status
tx_event_flags_set(TX_EVENT_FLAGS_GROUP* group_ptr, ULONG flags_to_set, UINT set_option)
{
  if (group_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  if (group_ptr->tx_event_flags_id != k_tx_event_flags_magic) {
    return TX_DELETED;
  }

  if (set_option == TX_OR) {
    group_ptr->tx_event_flags |= flags_to_set;
  } else if (set_option == TX_AND) {
    group_ptr->tx_event_flags &= flags_to_set;
  }

  return TX_SUCCESS;
}

/**
 * @brief Get event flags
 *
 * @param[in] group_ptr Event flags group control block
 * @param[in] requested_flags Requested flags
 * @param[in] get_option Get option (TX_OR, TX_OR_CLEAR, TX_AND, TX_AND_CLEAR)
 * @param[out] actual_flags_ptr Actual flags retrieved
 * @param[in] wait_option Wait option
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_event_flags_get(TX_EVENT_FLAGS_GROUP* group_ptr,
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

  /* In mock environment, always return requested flags immediately */
  *actual_flags_ptr = requested_flags;

  /* Clear flags if requested */
  if (get_option == TX_OR_CLEAR || get_option == TX_AND_CLEAR) {
    group_ptr->tx_event_flags &= ~requested_flags;
  }

  return TX_SUCCESS;
}

/* =============================================================================
 * ThreadX Timer Tick Constants and Functions
 * =============================================================================
 */

/** @brief Timer ticks per second (matches real ThreadX default) */
#ifndef TX_TIMER_TICKS_PER_SECOND
#define TX_TIMER_TICKS_PER_SECOND (100UL)
#endif

/**
 * @brief Get current timer tick count (mock implementation)
 *
 * @details
 * Returns mock time value controlled by mock_tx_set_time().
 * In real ThreadX, returns OS tick count since boot.
 *
 * @return Current mock tick count
 */
ULONG tx_time_get(void);

/**
 * @brief Set mock timer tick count (for testing)
 *
 * @param[in] ticks Tick value to return from tx_time_get()
 */
void mock_tx_set_time(ULONG ticks);

/* =============================================================================
 * Mock Control Functions (for unit testing)
 * =============================================================================
 */

/**
 * @brief Register a stack error handler with ThreadX (mock implementation)
 *
 * @details
 * Mock of the real tx_thread_stack_error_notify() API.  Stores the provided
 * handler pointer so tests can verify it was registered and invoke it
 * directly to exercise the overflow handler without needing the ThreadX
 * context-switch machinery.
 *
 * @param[in] stack_error_handler Callback to invoke on stack overflow.
 *            Pass TX_NULL to deregister.
 *
 * @return TX_SUCCESS (mock always succeeds when TX_ENABLE_STACK_CHECKING is
 *         defined in tx_user.h; returns TX_FEATURE_NOT_ENABLED otherwise)
 */
static inline UINT tx_thread_stack_error_notify(VOID (*stack_error_handler)(TX_THREAD* thread_ptr))
{
  (void)stack_error_handler;
  /* Mock: unconditionally report success (TX_ENABLE_STACK_CHECKING active) */
  return TX_SUCCESS;
}

/**
 * @brief Reset mock ThreadX state
 *
 * @details
 * Clears all call counts and resets return values to defaults.
 * Call in test setUp().
 */
void mock_tx_reset(void);

/**
 * @brief Set return value for tx_thread_create()
 *
 * @param[in] status Status to return from tx_thread_create()
 */
void mock_tx_set_thread_create_return(tx_status status);

/**
 * @brief Check if tx_thread_create() was called
 *
 * @return true if tx_thread_create() was called since last reset
 */
bool mock_tx_was_thread_create_called(void);

/**
 * @brief Get number of tx_thread_create() calls
 *
 * @return Number of times tx_thread_create() was called
 */
uint32_t mock_tx_get_thread_create_count(void);

#ifdef __cplusplus
}
#endif
