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
 * STAR Project - Texas A&M University
 * January 2026
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
 * =============================================================================
 */

/** @brief ThreadX success status */
#define TX_SUCCESS ((UINT)0)

/** @brief ThreadX error status */
#define TX_NOT_AVAILABLE ((UINT)1)
#define TX_NO_MEMORY     ((UINT)2)
#define TX_DELETED       ((UINT)3)
#define TX_DELETE_ERROR  ((UINT)4)
#define TX_THREAD_ERROR  ((UINT)5)
#define TX_NO_EVENTS     ((UINT)6)
#define TX_WAIT_FOREVER  ((ULONG)0xFFFFFFFF)
#define TX_NO_WAIT       ((ULONG)0)
#define TX_NO_INHERIT    ((UINT)0)

/** @brief ThreadX thread constants */
#define TX_NO_TIME_SLICE ((UINT)0)
#define TX_AUTO_START    ((UINT)1)
#define TX_DONT_START    ((UINT)0)

/** @brief ThreadX event flags constants */
#define TX_OR           ((UINT)0)
#define TX_OR_CLEAR     ((UINT)1)
#define TX_AND          ((UINT)2)
#define TX_AND_CLEAR    ((UINT)3)

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
  CHAR*  tx_event_flags_name; /**< Event flags name */
  UINT   tx_event_flags_id;   /**< Event flags ID */
  ULONG  tx_event_flags;      /**< Current event flags */
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
static inline UINT tx_mutex_create(TX_MUTEX* mutex_ptr, CHAR* name_ptr, UINT inherit)
{
  (void)inherit;
  if (mutex_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }
  mutex_ptr->tx_mutex_name = name_ptr;
  mutex_ptr->tx_mutex_id   = 0x4D555458; /* "MUTX" */
  mutex_ptr->locked        = false;
  return TX_SUCCESS;
}

/**
 * @brief Delete a mutex
 *
 * @param[in] mutex_ptr Pointer to mutex control block
 *
 * @return TX_SUCCESS on success
 */
static inline UINT tx_mutex_delete(TX_MUTEX* mutex_ptr)
{
  if (mutex_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }
  mutex_ptr->tx_mutex_id = 0;
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
static inline UINT tx_mutex_get(TX_MUTEX* mutex_ptr, ULONG wait_option)
{
  (void)wait_option;
  if (mutex_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }
  if (mutex_ptr->tx_mutex_id != 0x4D555458) {
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
static inline UINT tx_mutex_put(TX_MUTEX* mutex_ptr)
{
  if (mutex_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }
  if (mutex_ptr->tx_mutex_id != 0x4D555458) {
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
static inline UINT tx_thread_create(TX_THREAD* thread_ptr,
                                    CHAR*      name_ptr,
                                    VOID (*entry_function)(ULONG),
                                    ULONG entry_input,
                                    VOID* stack_start,
                                    ULONG stack_size,
                                    UINT  priority,
                                    UINT  preempt_threshold,
                                    UINT  time_slice,
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

  if (thread_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }

  thread_ptr->tx_thread_name = name_ptr;
  thread_ptr->tx_thread_id   = 0x54485244; /* "THRD" */

  return TX_SUCCESS;
}

/**
 * @brief Delete a thread
 *
 * @param[in] thread_ptr Thread control block pointer
 *
 * @return TX_SUCCESS on success
 */
static inline UINT tx_thread_delete(TX_THREAD* thread_ptr)
{
  if (thread_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }

  thread_ptr->tx_thread_id = 0;
  return TX_SUCCESS;
}

/**
 * @brief Terminate a thread
 *
 * @param[in] thread_ptr Thread control block pointer
 *
 * @return TX_SUCCESS on success
 */
static inline UINT tx_thread_terminate(TX_THREAD* thread_ptr)
{
  if (thread_ptr == NULL) {
    return TX_NOT_AVAILABLE;
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
static inline UINT tx_thread_resume(TX_THREAD* thread_ptr)
{
  if (thread_ptr == NULL) {
    return TX_THREAD_ERROR;
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
static inline UINT tx_thread_sleep(ULONG timer_ticks)
{
  (void)timer_ticks; /* No actual sleep in mock environment */
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
static inline UINT tx_event_flags_create(TX_EVENT_FLAGS_GROUP* group_ptr, CHAR* name_ptr)
{
  if (group_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }

  group_ptr->tx_event_flags_name = name_ptr;
  group_ptr->tx_event_flags_id   = 0x4556544E; /* "EVTN" */
  group_ptr->tx_event_flags      = 0;

  return TX_SUCCESS;
}

/**
 * @brief Delete an event flags group
 *
 * @param[in] group_ptr Event flags group control block
 *
 * @return TX_SUCCESS on success
 */
static inline UINT tx_event_flags_delete(TX_EVENT_FLAGS_GROUP* group_ptr)
{
  if (group_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }

  group_ptr->tx_event_flags_id = 0;
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
static inline UINT
tx_event_flags_set(TX_EVENT_FLAGS_GROUP* group_ptr, ULONG flags_to_set, UINT set_option)
{
  if (group_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }

  if (group_ptr->tx_event_flags_id != 0x4556544E) {
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
static inline UINT tx_event_flags_get(TX_EVENT_FLAGS_GROUP* group_ptr,
                                      ULONG                 requested_flags,
                                      UINT                  get_option,
                                      ULONG*                actual_flags_ptr,
                                      ULONG                 wait_option)
{
  (void)wait_option;

  if (group_ptr == NULL || actual_flags_ptr == NULL) {
    return TX_NOT_AVAILABLE;
  }

  if (group_ptr->tx_event_flags_id != 0x4556544E) {
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
