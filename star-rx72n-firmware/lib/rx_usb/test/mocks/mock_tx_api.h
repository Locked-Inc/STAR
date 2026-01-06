/**
 * @file mock_tx_api.h
 * @brief Mock ThreadX API for Host-Side Testing
 *
 * Provides mock implementations of ThreadX API functions used by USB driver.
 * This allows testing on host without actual RTOS.
 *
 * @date 2026-01-06
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_TX_API_H
#define MOCK_TX_API_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ThreadX Type Definitions
 * =============================================================================
 */

/** @brief ThreadX unsigned integer type */
typedef uint32_t UINT;

/** @brief ThreadX unsigned long type */
typedef uint32_t ULONG;

/** @brief ThreadX character type */
typedef char CHAR;

/** @brief ThreadX void pointer type */
typedef void VOID;

/* =============================================================================
 * ThreadX Return Codes
 * =============================================================================
 */

typedef enum {
  TX_SUCCESS                = 0x00, /**< Success */
  TX_DELETED                = 0x01, /**< Object was deleted */
  TX_NO_MEMORY              = 0x10, /**< No memory available */
  TX_POOL_ERROR             = 0x02, /**< Invalid pool */
  TX_PTR_ERROR              = 0x03, /**< Invalid pointer */
  TX_WAIT_ERROR             = 0x04, /**< Invalid wait option */
  TX_SIZE_ERROR             = 0x05, /**< Invalid size */
  TX_GROUP_ERROR            = 0x06, /**< Invalid group */
  TX_NO_EVENTS              = 0x07, /**< No events available */
  TX_OPTION_ERROR           = 0x08, /**< Invalid option */
  TX_QUEUE_ERROR            = 0x09, /**< Invalid queue */
  TX_QUEUE_EMPTY            = 0x0A, /**< Queue is empty */
  TX_QUEUE_FULL             = 0x0B, /**< Queue is full */
  TX_SEMAPHORE_ERROR        = 0x0C, /**< Invalid semaphore */
  TX_NO_INSTANCE            = 0x0D, /**< No instance available */
  TX_THREAD_ERROR           = 0x0E, /**< Invalid thread */
  TX_PRIORITY_ERROR         = 0x0F, /**< Invalid priority */
  TX_START_ERROR            = 0x10, /**< Start error */
  TX_DELETE_ERROR           = 0x11, /**< Delete error */
  TX_RESUME_ERROR           = 0x12, /**< Resume error */
  TX_CALLER_ERROR           = 0x13, /**< Caller error */
  TX_SUSPEND_ERROR          = 0x14, /**< Suspend error */
  TX_TIMER_ERROR            = 0x15, /**< Invalid timer */
  TX_TICK_ERROR             = 0x16, /**< Invalid tick value */
  TX_ACTIVATE_ERROR         = 0x17, /**< Activate error */
  TX_THRESH_ERROR           = 0x18, /**< Threshold error */
  TX_SUSPEND_LIFTED         = 0x19, /**< Suspension lifted */
  TX_WAIT_ABORTED           = 0x1A, /**< Wait aborted */
  TX_WAIT_ABORT_ERROR       = 0x1B, /**< Wait abort error */
  TX_MUTEX_ERROR            = 0x1C, /**< Invalid mutex */
  TX_NOT_AVAILABLE          = 0x1D, /**< Not available */
  TX_NOT_OWNED              = 0x1E, /**< Not owned */
  TX_INHERIT_ERROR          = 0x1F, /**< Inherit error */
  TX_NOT_DONE               = 0x20, /**< Not done */
  TX_CEILING_EXCEEDED       = 0x21, /**< Ceiling exceeded */
  TX_INVALID_CEILING        = 0x22, /**< Invalid ceiling */
  TX_FEATURE_NOT_ENABLED    = 0xFF, /**< Feature not enabled */
} tx_return_codes_t;

/* =============================================================================
 * ThreadX Wait Options
 * =============================================================================
 */

typedef enum {
  TX_NO_WAIT      = 0,          /**< Don't wait */
  TX_WAIT_FOREVER = 0xFFFFFFFF, /**< Wait forever */
} tx_wait_options_t;

/* =============================================================================
 * ThreadX Thread Structure (simplified for testing)
 * =============================================================================
 */

/**
 * @brief Mock ThreadX thread control block
 */
typedef struct tx_thread_struct {
  char*    tx_thread_name;     /**< Thread name */
  UINT     tx_thread_state;    /**< Thread state */
  UINT     tx_thread_priority; /**< Thread priority */
  void*    tx_thread_stack_ptr;/**< Stack pointer */
  ULONG    tx_thread_stack_size; /**< Stack size */
} TX_THREAD;

/* =============================================================================
 * ThreadX Semaphore Structure (simplified for testing)
 * =============================================================================
 */

/**
 * @brief Mock ThreadX semaphore control block
 */
typedef struct tx_semaphore_struct {
  char*    tx_semaphore_name;  /**< Semaphore name */
  ULONG    tx_semaphore_count; /**< Semaphore count */
} TX_SEMAPHORE;

/* =============================================================================
 * ThreadX Mutex Structure (simplified for testing)
 * =============================================================================
 */

/**
 * @brief Mock ThreadX mutex control block
 */
typedef struct tx_mutex_struct {
  char*    tx_mutex_name;      /**< Mutex name */
  UINT     tx_mutex_ownership; /**< Ownership count */
} TX_MUTEX;

/* =============================================================================
 * Mock State Tracking
 * =============================================================================
 */

/** @brief Mock state constants */
typedef enum {
  k_mock_tx_max_sleep_calls = 64, /**< Maximum sleep calls to track */
} mock_tx_constants_t;

/**
 * @brief Mock ThreadX state
 */
typedef struct {
  uint32_t sleep_calls;                               /**< Number of tx_thread_sleep calls */
  ULONG    sleep_ticks[k_mock_tx_max_sleep_calls];    /**< Sleep durations */
  uint32_t current_time;                              /**< Simulated current time (ticks) */
  bool     interrupts_enabled;                        /**< Interrupt state */
} mock_tx_state_t;

extern mock_tx_state_t g_mock_tx;

/* =============================================================================
 * Mock ThreadX Functions
 * =============================================================================
 */

/**
 * @brief Sleep for specified number of ticks
 * @param timer_ticks Number of ticks to sleep
 * @return TX_SUCCESS
 */
UINT tx_thread_sleep(ULONG timer_ticks);

/**
 * @brief Get current time in ticks
 * @return Current time in ticks
 */
ULONG tx_time_get(void);

/**
 * @brief Disable interrupts
 * @return Previous interrupt state
 */
UINT tx_interrupt_control(UINT new_posture);

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

/**
 * @brief Initialize mock ThreadX state
 */
void mock_tx_init(void);

/**
 * @brief Clear mock ThreadX state
 */
void mock_tx_clear(void);

/**
 * @brief Get number of sleep calls
 * @return Number of tx_thread_sleep calls
 */
uint32_t mock_tx_get_sleep_count(void);

/**
 * @brief Get total sleep ticks
 * @return Sum of all sleep durations
 */
ULONG mock_tx_get_total_sleep_ticks(void);

/**
 * @brief Set simulated current time
 * @param ticks Current time in ticks
 */
void mock_tx_set_time(uint32_t ticks);

/**
 * @brief Advance simulated time
 * @param ticks Ticks to advance
 */
void mock_tx_advance_time(uint32_t ticks);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_TX_API_H */
