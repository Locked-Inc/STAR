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
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
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

/**
 * @typedef UCHAR
 * @brief ThreadX unsigned 8-bit character type
 *
 * @details
 * Portable alias for `unsigned char` used throughout ThreadX APIs for raw
 * byte buffers and character data.  Range: 0-255.  Prefer this alias over
 * plain `unsigned char` wherever ThreadX types are expected, to make the
 * RTOS dependency explicit and improve searchability across the codebase.
 *
 * @note Alignment is 1 byte; portability is guaranteed across all compilers
 *       used in the STAR project (GNURX, GCC, Clang).
 *
 * @since Version 1.0.0
 */
typedef unsigned char UCHAR;

/**
 * @def TX_NULL
 * @brief ThreadX NULL pointer constant
 *
 * @details
 * Intentional deviation from the no-constant-macro rule (CLAUDE.md Sec.Constants):
 * TX_NULL must be a macro because it is used as a pointer initializer and as a
 * sentinel in struct-initializer contexts where a typed enum or static const
 * cannot legally appear in C (e.g., `.tx_thread_name = TX_NULL`).  This mirrors
 * the real ThreadX API definition and must remain a macro so that all existing
 * call sites continue to compile without modification.
 *
 * @note Intentional deviation - approved for pointer-initializer compatibility.
 * @since Version 1.0.0
 */
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
 * @struct TX_THREAD
 * @brief Mock ThreadX thread control block
 *
 * @details
 * Contains the minimal set of fields used by STAR firmware, including the
 * stack inspection fields required by rx_stack_monitor_get_free_bytes().
 * In real ThreadX the control block is much larger; this mock exposes only
 * the subset accessed by the STAR stack monitor and task-creation code.
 *
 * @invariant tx_thread_stack_start is non-NULL when the thread is active
 * @invariant tx_thread_stack_size > 0 when the thread is active
 * @invariant tx_thread_id equals k_tx_thread_magic for a valid live thread
 *
 * @code
 * uint8_t stack_buf[256];
 * TX_THREAD t = {
 *     .tx_thread_name        = "example",
 *     .tx_thread_id          = k_tx_thread_magic,
 *     .tx_thread_stack_start = stack_buf,
 *     .tx_thread_stack_size  = sizeof(stack_buf),
 * };
 * uint32_t free_bytes;
 * rx_stack_monitor_get_free_bytes(&t, &free_bytes);
 * @endcode
 *
 * @see rx_stack_monitor_get_free_bytes() Uses tx_thread_stack_start and
 *      tx_thread_stack_size to compute the stack high-water mark
 *
 * @since Version 1.0.0
 */
typedef struct TX_THREAD_STRUCT {
  CHAR*
    tx_thread_name; /**< Thread name string (NULL-terminated, max 64 chars; TX_NULL for unnamed threads) */
  UINT
    tx_thread_id; /**< Thread magic ID (k_tx_thread_magic = 0x54485244 when valid; k_tx_invalid_id = 0 when deleted) */
  VOID*
    tx_thread_stack_start; /**< Lowest stack address (byte-aligned; must be non-NULL when thread is active) */
  ULONG
  tx_thread_stack_size; /**< Total stack allocation in bytes (must be > 0 when thread is active) */
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
static inline tx_status tx_mutex_create(TX_MUTEX* mutex_ptr, CHAR* name_ptr, UINT inherit)
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
  (void)priority;
  (void)preempt_threshold;
  (void)time_slice;
  (void)auto_start;

  /* Pre-condition: Validate input pointer */
  if (thread_ptr == nullptr) {
    return TX_NOT_AVAILABLE;
  }

  /* Initialize thread structure */
  thread_ptr->tx_thread_name        = name_ptr;
  thread_ptr->tx_thread_id          = k_tx_thread_magic;
  thread_ptr->tx_thread_stack_start = stack_start;
  thread_ptr->tx_thread_stack_size  = stack_size;

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
#ifdef MOCK_TX_THREAD_DELETE
/* When MOCK_TX_THREAD_DELETE is defined, use non-inline declaration.
 * Implementation in mock_tx_api.c allows controlling the return value. */
tx_status tx_thread_delete(TX_THREAD* thread_ptr);
#else
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
#endif /* MOCK_TX_THREAD_DELETE */

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
#ifdef MOCK_TX_THREAD_SLEEP
/* When MOCK_TX_THREAD_SLEEP is defined, use non-inline declaration.
 * Implementation in mock_tx_api.c allows registering a callback. */
tx_status tx_thread_sleep(ULONG timer_ticks);
#else
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
#endif /* MOCK_TX_THREAD_SLEEP */

/**
 * @brief Suspend a thread (mock declaration -- implemented in mock_coverage_stubs.c).
 *
 * @details
 * In real ThreadX this moves the thread to the suspended state. The mock
 * implementation is a no-op that returns TX_SUCCESS so that firmware code
 * calling tx_thread_suspend() compiles and links on the host without a
 * kernel dependency.
 *
 * @param[in] thread_ptr Thread control block pointer.
 *
 * @return TX_SUCCESS always.
 *
 * @pre  thread_ptr points to a valid TX_THREAD (or may be nullptr in mocks).
 * @pre  Called from a single-threaded test context (no real scheduling).
 * @post Thread state is unchanged in the host environment.
 * @post Returns synchronously with no scheduling side-effects.
 *
 * @note Thread safety: N/A in single-threaded host test environment.
 * @since Version 1.0.0
 */
UINT tx_thread_suspend(TX_THREAD* thread_ptr);

/**
 * @brief Identify the currently running thread (mock declaration -- implemented
 *        in mock_coverage_stubs.c).
 *
 * @details
 * In real ThreadX this returns a pointer to the currently executing TX_THREAD
 * control block, or nullptr if called from initialization or a timer ISR. The
 * mock always returns nullptr to signal "not inside a thread" so that firmware
 * code that guards critical sections with tx_thread_identify() != nullptr takes
 * the non-thread path on the host.
 *
 * @return nullptr always.
 *
 * @pre  Called from a single-threaded test context (no real scheduling).
 * @pre  No ThreadX scheduler is running on the host.
 * @post Return value is nullptr.
 * @post No state is modified.
 *
 * @note Thread safety: N/A in single-threaded host test environment.
 * @since Version 1.0.0
 */
TX_THREAD* tx_thread_identify(void);

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
#ifdef MOCK_TX_EVENT_FLAGS_CREATE
/* When MOCK_TX_EVENT_FLAGS_CREATE is defined, use non-inline declaration.
 * Implementation in mock_tx_api.c allows controlling return values. */
tx_status tx_event_flags_create(TX_EVENT_FLAGS_GROUP* group_ptr, CHAR* name_ptr);
#else
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
#endif /* MOCK_TX_EVENT_FLAGS_CREATE */

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
#ifdef MOCK_TX_EVENT_FLAGS_GET
/* When MOCK_TX_EVENT_FLAGS_GET is defined, use non-inline declaration.
 * Implementation in mock_tx_api.c allows controlling return values per call. */
tx_status tx_event_flags_get(TX_EVENT_FLAGS_GROUP* group_ptr,
                             ULONG                 requested_flags,
                             UINT                  get_option,
                             ULONG*                actual_flags_ptr,
                             tx_wait_option        wait_option);
#else
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
#endif /* MOCK_TX_EVENT_FLAGS_GET */

/* =============================================================================
 * ThreadX Timer Tick Constants and Functions
 * =============================================================================
 */

/** @brief Timer ticks per second (matches real ThreadX default) */
#ifndef TX_TIMER_TICKS_PER_SECOND
#define TX_TIMER_TICKS_PER_SECOND (100UL)
#endif

/* =============================================================================
 * ThreadX Interrupt Control Constants and Functions
 * =============================================================================
 */

/**
 * @def TX_INT_DISABLE
 * @brief Argument to tx_interrupt_control() that disables interrupts.
 *
 * @details
 * In real ThreadX this is a platform-specific mask value (typically 0xC0 on
 * ARM Cortex-M).  The mock uses 0x1 as a sentinel so that test code can
 * distinguish between "disable" and "restore to prior state" call patterns.
 *
 * @note Intentional macro: must match the ThreadX API call convention where
 *       TX_INT_DISABLE is passed directly as a UINT argument. Cannot be an
 *       enum constant because the argument and return types carry interrupt
 *       state as an opaque UINT.
 *
 * @since Version 1.0.0
 */
#ifndef TX_INT_DISABLE
#define TX_INT_DISABLE 0x1U
#endif

/**
 * @enum tx_interrupt_prior_state_t
 * @brief Named constants for the prior interrupt-enable state returned by
 *        tx_interrupt_control() in the mock.
 *
 * @details
 * The real ThreadX API returns an opaque UINT platform bitmask. In the mock
 * we return a named constant instead of the magic literal 0U so that the
 * return value is self-documenting and auditable.
 *
 * @since Version 1.0.0
 */
typedef enum : UINT {
  k_tx_int_was_enabled = 0U, /**< Prior state: interrupts were enabled. */
} tx_interrupt_prior_state_t;

/**
 * @brief Save the current interrupt-enable state and optionally disable
 *        interrupts (mock implementation).
 *
 * @details
 * In real ThreadX this performs a platform-specific atomic
 * read-modify-write on the CPU interrupt-mask register.  The mock
 * simply returns a fixed "previously enabled" sentinel so that firmware
 * code that saves and later restores the state compiles and links on the
 * host without any kernel dependency.
 *
 * Typical usage in firmware:
 * @code
 * UINT saved = tx_interrupt_control(TX_INT_DISABLE);
 * // ... critical section ...
 * (void)tx_interrupt_control(saved);
 * @endcode
 *
 * @param[in] new_posture Desired interrupt state; TX_INT_DISABLE to disable,
 *            or a previously returned value to restore.
 *
 * @return Previous interrupt-enable state (mock always returns 0 = "was
 *         enabled before disable").
 *
 * @pre  Called from a single-threaded test context (no real scheduling).
 * @pre  No concurrent access to this mock (single-threaded host environment).
 * @post Interrupt state is unchanged in the host environment (mock is a no-op).
 * @post Mock performs no heap or I/O side-effects.
 * @post Returns synchronously; no scheduling occurred.
 *
 * @note Thread safety: N/A in single-threaded host test environment.
 *
 * @since Version 1.0.0
 */
static inline UINT tx_interrupt_control(UINT new_posture)
{
  (void)new_posture;
  /* Mock: always report "interrupts were enabled" as the prior state. */
  return (UINT)k_tx_int_was_enabled;
}

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
 * Mock of the real tx_thread_stack_error_notify() API.  In production
 * ThreadX this stores the handler pointer in a global; the RTOS invokes
 * it at every context switch when a corrupted stack sentinel is detected.
 *
 * This mock ignores the handler pointer and unconditionally returns
 * TX_SUCCESS whenever TX_ENABLE_STACK_CHECKING is defined.  If the symbol
 * is undefined the real ThreadX would return TX_FEATURE_NOT_ENABLED; to
 * simulate that behaviour compile with TX_ENABLE_STACK_CHECKING undefined.
 *
 * @param[in] stack_error_handler Callback invoked by ThreadX on stack
 *            overflow.  Pass TX_NULL to deregister the current handler.
 *            Must accept a single TX_THREAD* argument.
 *
 * @return tx_status Registration result
 * @retval TX_SUCCESS Handler accepted (TX_ENABLE_STACK_CHECKING is defined);
 *         this mock always returns TX_SUCCESS
 * @retval TX_FEATURE_NOT_ENABLED TX_ENABLE_STACK_CHECKING is not defined
 *         (real ThreadX compile-time behaviour; not returned by this mock)
 *
 * @pre ThreadX is initialised (tx_kernel_enter() has been called) or mock
 *      environment is active (host unit-test build)
 * @pre stack_error_handler is a valid function pointer or TX_NULL
 * @post The handler is stored for later invocation (in production ThreadX)
 * @post Returns TX_SUCCESS, indicating the registration was accepted
 *
 * @note This is a mock implementation -- it does not store the handler and
 *       will not call it during tests.  Invoke the handler directly from
 *       test code if overflow behaviour needs to be exercised.
 *
 * @code
 * // Registration (done indirectly via rx_stack_monitor_init()):
 * tx_status status = tx_thread_stack_error_notify(my_overflow_handler);
 * // Deregistration (reuse same variable):
 * status = tx_thread_stack_error_notify(TX_NULL);
 * @endcode
 *
 * @see rx_stack_monitor_init() STAR wrapper that calls this function
 *
 * @since Version 1.0.0
 */
static inline tx_status
tx_thread_stack_error_notify(VOID (*stack_error_handler)(TX_THREAD* thread_ptr))
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
 * @brief Set return value for tx_event_flags_create()
 *
 * @param[in] status Status to return from tx_event_flags_create()
 */
void mock_tx_set_event_flags_create_return(tx_status status);

#ifdef MOCK_TX_THREAD_DELETE
/**
 * @brief Set return value for tx_thread_delete()
 *
 * @details
 * Overrides the next tx_thread_delete() call to return the given status.
 * After one call the override is cleared (one-shot). Set TX_SUCCESS to
 * restore normal behavior.
 *
 * @param[in] status Status to return from tx_thread_delete()
 */
void mock_tx_set_thread_delete_return(tx_status status);
#endif /* MOCK_TX_THREAD_DELETE */

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

#ifdef MOCK_TX_EVENT_FLAGS_GET
/**
 * @brief Set return value for tx_event_flags_get() on the Nth call
 *
 * @details
 * The first N-1 calls return TX_NO_EVENTS (flag not set), the Nth and
 * subsequent calls return TX_SUCCESS. Set N=0 to always return TX_SUCCESS.
 * Use this to control whether a specific event is "ready" on the Nth poll.
 *
 * @param[in] success_after_n_calls Number of calls that return TX_NO_EVENTS
 *   before switching to TX_SUCCESS. 0 = always TX_SUCCESS.
 */
void mock_tx_set_event_flags_get_no_events_count(uint32_t success_after_n_calls);

/**
 * @brief Reset the tx_event_flags_get() call counter to zero
 *
 * @details
 * Resets the internal call counter used by
 * mock_tx_set_event_flags_get_no_events_count() so that the "first N calls
 * return TX_NO_EVENTS" window starts over from the next call. Use from a
 * sleep callback or obstacle callback to re-arm the no-events window between
 * the outer and inner task-loop calls.
 *
 * @note Only available when MOCK_TX_EVENT_FLAGS_GET is defined.
 *
 * @since Version 1.0.0
 */
void mock_tx_event_flags_get_reset_count(void);

/**
 * @brief Register a callback invoked each time tx_event_flags_get() returns TX_SUCCESS
 *
 * @details
 * The registered function is called immediately after tx_event_flags_get()
 * determines it will return TX_SUCCESS but before returning to the caller.
 * Use this to re-arm the no-events counter so that the NEXT call to
 * tx_event_flags_get() starts a fresh no-events window.
 *
 * Example: to let the outer "wait for start" fire (TX_SUCCESS), then make
 * the first inner "check stop" return TX_NO_EVENTS:
 * @code
 * static void on_start_event(void) {
 *   mock_tx_event_flags_get_reset_count();
 *   mock_tx_set_event_flags_get_no_events_count(1);
 *   mock_tx_event_flags_get_set_on_success_cb(nullptr); // fire once only
 * }
 * mock_tx_event_flags_get_set_on_success_cb(on_start_event);
 * @endcode
 *
 * @param[in] cb Callback function pointer, or nullptr to clear.
 *
 * @note Only available when MOCK_TX_EVENT_FLAGS_GET is defined.
 *
 * @since Version 1.0.0
 */
void mock_tx_event_flags_get_set_on_success_cb(void (*cb)(void));

/**
 * @brief Set a bitmask applied to the flags returned by tx_event_flags_get()
 *
 * @details
 * The returned actual_flags are ANDed with mask before being written to the
 * caller. Use to suppress specific flag bits on a call (e.g., exclude the
 * shutdown bit so the worker loop continues rather than exits).
 * Reset mask to ~0 to restore normal (all-flags) behavior.
 *
 * @param[in] mask Bitmask to AND with returned flags (default ~0 = all bits set)
 */
void mock_tx_event_flags_get_set_mask(ULONG mask);
#endif /* MOCK_TX_EVENT_FLAGS_GET */

#ifdef MOCK_TX_THREAD_SLEEP
/**
 * @brief Register a callback invoked each time tx_thread_sleep() is called
 *
 * @details
 * The registered function is called immediately when tx_thread_sleep() is
 * invoked (before returning TX_SUCCESS). Use this in task-entry tests to
 * modify state (e.g., stop the outer task loop) at a predictable point.
 *
 * @param[in] cb Callback function pointer, or nullptr to clear.
 */
void mock_tx_set_sleep_callback(void (*cb)(void));
#endif /* MOCK_TX_THREAD_SLEEP */

#ifdef __cplusplus
}
#endif
