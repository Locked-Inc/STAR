/**
 * @file tx_api.h
 * @brief Mock ThreadX API for Host-Side Testing
 *
 * Provides minimal ThreadX stubs to allow testing of code that
 * uses ThreadX primitives (mutex, semaphore, etc.) on the host.
 *
 * This file shadows the real tx_api.h when the mocks directory
 * is first in the include path.
 *
 * @date 2026-01-11
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef TX_API_H
#define TX_API_H

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

/* =============================================================================
 * ThreadX Constants
 *
 * NOTE: ThreadX uses SCREAMING_SNAKECASE names. We preserve the names but use
 * enums to comply with the no-magic-number policy in the firmware codebase.
 * =============================================================================
 */

/** @brief ThreadX return codes (must match real ThreadX API values) */
typedef enum {
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
typedef ULONG tx_wait_option;
static const tx_wait_option TX_NO_WAIT      = 0U;
static const tx_wait_option TX_WAIT_FOREVER = 0xFFFFFFFFUL;

/** @brief ThreadX inheritance options */
typedef enum {
  TX_NO_INHERIT = 0U,
  TX_INHERIT    = 1U,
} tx_inherit_option;

/** @brief ThreadX thread constants */
typedef enum {
  TX_NO_TIME_SLICE = 0U,
  TX_AUTO_START    = 1U,
  TX_DONT_START    = 0U,
  TX_AUTO_ACTIVATE = 1U,
  TX_NO_ACTIVATE   = 0U,
} tx_thread_constants_t;

/** @brief ThreadX event flags constants */
typedef enum {
  TX_OR        = 0U,
  TX_OR_CLEAR  = 1U,
  TX_AND       = 2U,
  TX_AND_CLEAR = 3U,
} tx_event_flags_option_t;

/** @brief ThreadX magic ID constants for object validation */
typedef enum {
  k_tx_invalid_id        = 0U,         /**< Invalid/uninitialized object ID */
  k_tx_mutex_magic       = 0x4D555458, /**< "MUTX" ASCII magic ID */
  k_tx_thread_magic      = 0x54485244, /**< "THRD" ASCII magic ID */
  k_tx_event_flags_magic = 0x4556544E, /**< "EVTN" ASCII magic ID */
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
 * @brief Mock ThreadX thread structure
 */
typedef struct TX_THREAD_STRUCT {
  CHAR* tx_thread_name; /**< Thread name */
  UINT  tx_thread_id;   /**< Thread ID */
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
  if (mutex_ptr == NULL) {
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
  if (mutex_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }
  if (mutex_ptr->tx_mutex_id != k_tx_mutex_magic) {
    return TX_DELETED;
  }
  mutex_ptr->tx_mutex_id = k_tx_invalid_id;
  mutex_ptr->locked      = false;
  if (mutex_ptr->tx_mutex_id == k_tx_mutex_magic || mutex_ptr->locked) {
    return TX_PTR_ERROR;
  }
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
  if (mutex_ptr == NULL) {
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
  if (mutex_ptr == NULL) {
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
static inline tx_status tx_thread_create(TX_THREAD* thread_ptr,
                                         CHAR*      name_ptr,
                                         VOID (*entry_function)(ULONG),
                                         ULONG entry_input,
                                         VOID* stack_start,
                                         ULONG stack_size,
                                         UINT  priority,
                                         UINT  preempt_threshold,
                                         ULONG time_slice,
                                         UINT  auto_start)
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
  if (thread_ptr == NULL) {
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

/**
 * @brief Delete a thread
 *
 * @param[in] thread_ptr Thread control block pointer
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_thread_delete(TX_THREAD* thread_ptr)
{
  if (thread_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }

  if (thread_ptr->tx_thread_id != k_tx_thread_magic) {
    return TX_DELETED;
  }

  thread_ptr->tx_thread_id = k_tx_invalid_id;
  if (thread_ptr->tx_thread_id == k_tx_thread_magic) {
    return TX_PTR_ERROR;
  }
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

  if (thread_ptr == NULL) {
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

  if (thread_ptr == NULL) {
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
  if (group_ptr == NULL) {
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
 * @brief Delete an event flags group
 *
 * @param[in] group_ptr Event flags group control block
 *
 * @return TX_SUCCESS on success
 */
static inline tx_status tx_event_flags_delete(TX_EVENT_FLAGS_GROUP* group_ptr)
{
  if (group_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }

  if (group_ptr->tx_event_flags_id != k_tx_event_flags_magic) {
    return TX_DELETED;
  }

  group_ptr->tx_event_flags_id = k_tx_invalid_id;
  group_ptr->tx_event_flags    = 0;

  if (group_ptr->tx_event_flags_id == k_tx_event_flags_magic) {
    return TX_PTR_ERROR;
  }

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
  if (group_ptr == NULL) {
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

  if (group_ptr == NULL || actual_flags_ptr == NULL) {
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

#ifdef __cplusplus
}
#endif

#endif /* TX_API_H */
