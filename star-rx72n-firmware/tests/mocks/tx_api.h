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
 * NOTE: We intentionally use SCREAMING_SNAKECASE macros here to mirror the
 * ThreadX public API. This explicitly deviates from the project style guide
 * so the mock header matches real ThreadX usage and signatures.
 * =============================================================================
 */

/** @brief ThreadX return codes (must match real ThreadX API values) */
typedef UINT tx_status;
#define TX_SUCCESS             ((UINT)0x00)
#define TX_DELETED             ((UINT)0x01)
#define TX_POOL_ERROR          ((UINT)0x02)
#define TX_PTR_ERROR           ((UINT)0x03)
#define TX_WAIT_ERROR          ((UINT)0x04)
#define TX_SIZE_ERROR          ((UINT)0x05)
#define TX_GROUP_ERROR         ((UINT)0x06)
#define TX_NO_EVENTS           ((UINT)0x07)
#define TX_OPTION_ERROR        ((UINT)0x08)
#define TX_QUEUE_ERROR         ((UINT)0x09)
#define TX_QUEUE_EMPTY         ((UINT)0x0A)
#define TX_QUEUE_FULL          ((UINT)0x0B)
#define TX_SEMAPHORE_ERROR     ((UINT)0x0C)
#define TX_NO_INSTANCE         ((UINT)0x0D)
#define TX_THREAD_ERROR        ((UINT)0x0E)
#define TX_PRIORITY_ERROR      ((UINT)0x0F)
#define TX_NO_MEMORY           ((UINT)0x10)
#define TX_START_ERROR         ((UINT)0x10)
#define TX_DELETE_ERROR        ((UINT)0x11)
#define TX_RESUME_ERROR        ((UINT)0x12)
#define TX_CALLER_ERROR        ((UINT)0x13)
#define TX_SUSPEND_ERROR       ((UINT)0x14)
#define TX_TIMER_ERROR         ((UINT)0x15)
#define TX_TICK_ERROR          ((UINT)0x16)
#define TX_ACTIVATE_ERROR      ((UINT)0x17)
#define TX_THRESH_ERROR        ((UINT)0x18)
#define TX_SUSPEND_LIFTED      ((UINT)0x19)
#define TX_WAIT_ABORTED        ((UINT)0x1A)
#define TX_WAIT_ABORT_ERROR    ((UINT)0x1B)
#define TX_MUTEX_ERROR         ((UINT)0x1C)
#define TX_NOT_AVAILABLE       ((UINT)0x1D)
#define TX_NOT_OWNED           ((UINT)0x1E)
#define TX_INHERIT_ERROR       ((UINT)0x1F)
#define TX_NOT_DONE            ((UINT)0x20)
#define TX_CEILING_EXCEEDED    ((UINT)0x21)
#define TX_INVALID_CEILING     ((UINT)0x22)
#define TX_FEATURE_NOT_ENABLED ((UINT)0xFF)

/** @brief ThreadX wait options */
typedef ULONG tx_wait_option;
#define TX_NO_WAIT      ((ULONG)0)
#define TX_WAIT_FOREVER ((ULONG)0xFFFFFFFFUL)

/** @brief ThreadX inheritance options */
typedef UINT tx_inherit_option;
#define TX_NO_INHERIT ((UINT)0)
#define TX_INHERIT    ((UINT)1)

/** @brief ThreadX thread constants */
#define TX_NO_TIME_SLICE ((ULONG)0)
#define TX_AUTO_START    ((UINT)1)
#define TX_DONT_START    ((UINT)0)
#define TX_AUTO_ACTIVATE ((UINT)1)
#define TX_NO_ACTIVATE   ((UINT)0)

/** @brief ThreadX event flags constants */
#define TX_OR        ((UINT)0)
#define TX_OR_CLEAR  ((UINT)1)
#define TX_AND       ((UINT)2)
#define TX_AND_CLEAR ((UINT)3)

/** @brief ThreadX magic ID constants for object validation */
typedef enum {
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
  mutex_ptr->tx_mutex_id = 0;
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

  thread_ptr->tx_thread_id = 0;
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
static inline tx_status tx_thread_sleep(tx_wait_option timer_ticks)
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

  group_ptr->tx_event_flags_id = 0;
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
