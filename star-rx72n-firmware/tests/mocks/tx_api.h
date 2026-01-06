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
#define TX_WAIT_FOREVER  ((ULONG)0xFFFFFFFF)
#define TX_NO_WAIT       ((ULONG)0)
#define TX_NO_INHERIT    ((UINT)0)

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

#ifdef __cplusplus
}
#endif

#endif /* TX_API_H */
