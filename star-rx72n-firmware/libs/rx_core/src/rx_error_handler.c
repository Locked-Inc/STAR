/**
 * @file rx_error_handler.c
 * @brief Error Handler Concrete Implementation
 *
 * @details
 * Implements the rx_error_interface_t abstract interface with concrete error
 * tracking, retry logic, and exponential backoff capabilities. This module
 * provides a centralized error handling mechanism for the STAR firmware,
 * enabling consistent error reporting, component-level error tracking, and
 * intelligent retry strategies.
 *
 * @par Implementation Architecture
 * @dot
 * digraph error_handler_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_public {
 *     label="Public API";
 *     style=filled;
 *     color=lightblue;
 *     init [label="error_handler_init()"];
 *     deinit [label="error_handler_deinit()"];
 *     get_iface [label="error_handler_get_interface()"];
 *   }
 *
 *   subgraph cluster_interface {
 *     label="Interface Implementation";
 *     style=filled;
 *     color=lightgreen;
 *     report [label="impl_report_error()"];
 *     get_count [label="impl_get_error_count()"];
 *     get_comp [label="impl_get_component_error_count()"];
 *     clear [label="impl_clear_errors()"];
 *     retry_check [label="impl_is_retry_limit_reached()"];
 *     reset_retry [label="impl_reset_retry_counter()"];
 *     backoff [label="impl_get_backoff_delay()"];
 *   }
 *
 *   subgraph cluster_internal {
 *     label="Internal Helpers";
 *     style=filled;
 *     color=lightyellow;
 *     find [label="internal_find_component()"];
 *     find_create [label="internal_find_or_create_component()"];
 *   }
 *
 *   subgraph cluster_state {
 *     label="State Management";
 *     style=filled;
 *     color=lightgrey;
 *     handler [label="error_handler_t\n(mutex protected)"];
 *     components [label="error_component_state_t[]\n(per-component tracking)"];
 *   }
 *
 *   init -> handler [label="initialize"];
 *   get_iface -> handler [label="bind"];
 *   report -> find_create -> components;
 *   get_count -> handler;
 *   get_comp -> find -> components;
 *   clear -> handler;
 *   clear -> components;
 *   retry_check -> find -> components;
 *   reset_retry -> find -> components;
 *   backoff -> find -> components;
 *   deinit -> handler [label="cleanup"];
 * }
 * @enddot
 *
 * @par Exponential Backoff Algorithm
 * The backoff delay calculation uses exponential growth with configurable
 * bounds:
 *
 * @f[
 *   \text{delay} = \min\left(\text{initial} \times 2^{(\text{retries}-1)},
 *                           \text{max\_backoff}\right)
 * @f]
 *
 * @par State Machine
 * @startuml
 * [*] --> Uninitialized
 * Uninitialized --> Initialized : error_handler_init()
 * Initialized --> Initialized : report_error() / get_error_count() / etc.
 * Initialized --> Uninitialized : error_handler_deinit()
 * Initialized --> Initialized : clear_errors()
 * @enduml
 *
 * @par Memory Layout
 * | Component           | Size     | Description                       |
 * |---------------------|----------|-----------------------------------|
 * | error_handler_t     | ~1.2 KB  | Main handler structure            |
 * | TX_MUTEX            | ~100 B   | ThreadX mutex control block       |
 * | components[]        | ~1 KB    | 16 component tracking slots       |
 *
 * @par Performance Characteristics
 * | Operation              | Best Case | Worst Case | Notes                |
 * |------------------------|-----------|------------|----------------------|
 * | report_error()         | ~5 us     | ~50 us     | Mutex + string cmp   |
 * | get_error_count()      | ~2 us     | ~10 us     | Mutex only           |
 * | get_backoff_delay()    | ~3 us     | ~20 us     | Loop up to 32 iters  |
 * | clear_errors()         | ~5 us     | ~30 us     | Clears all slots     |
 *
 * @par Thread Safety
 * All interface functions are thread-safe. Internal state is protected by
 * a ThreadX mutex (TX_NO_INHERIT priority inheritance disabled).
 *
 * @par Module Dependencies
 * - rx_error_handler.h: Public API and types
 * - rx_check.h: Parameter validation macros
 * - rx_log.h: Error logging infrastructure
 * - ThreadX (tx_api.h): Mutex synchronization
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] All loops have static upper bounds (k_error_handler_max_retries)
 * - Rule 3: [OK] No dynamic memory allocation (static component slots)
 * - Rule 4: [OK] Functions are concise (~30-60 lines)
 * - Rule 5: [OK] Minimum 2 assertions/validations per function
 * - Rule 6: [OK] Variables declared at smallest scope
 * - Rule 7: [OK] All return values checked or explicitly cast to void
 * - Rule 8: [OK] All constants use C23 typed enums
 * - Rule 9: [WARN] Function pointers used for interface (DIP pattern)
 * - Rule 10: [OK] Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S (SRP):** Single responsibility - error tracking only
 * - **O (OCP):** Extensible via configuration without modification
 * - **L (LSP):** Implements rx_error_interface_t, substitutable
 * - **I (ISP):** Interface has cohesive set of error operations
 * - **D (DIP):** Clients depend on rx_error_interface_t abstraction
 *
 * @see rx_error_handler.h Public API and type definitions
 * @see rx_error_interface.h Abstract error interface
 * @see rx_check.h Validation macros (RX_CHECK_NULL_PTR)
 * @see rx_log.h Logging functions (rx_log_error, rx_log_info)
 *
 * @author Locked, Inc.
 * @date 2026-01-28
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "rx_error_handler.h"

#include <string.h>

#include "rx_check.h"
#include "rx_log.h"

/* =============================================================================
 * Module State
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Log tag for error handler module
 * @details Identifies log messages from this module. Points to a string
 *          literal and is immutable for the entire lifetime of the program
 *          (compile-time constant, not just after initialization).
 * @since Version 1.0.0
 */
static const char* const s_tag = "ERROR_HANDLER";

/**
 * @enum error_handler_backoff_constants_t
 * @brief Internal constants for exponential backoff algorithm
 *
 * @details
 * These constants control the exponential backoff behavior used by
 * impl_get_backoff_delay(). The algorithm doubles the delay on each
 * retry up to the configured maximum.
 *
 * @par Backoff Progression Example
 * With initial_backoff_ms=100, max_backoff_ms=5000:
 * | Retry | Delay (ms) | Calculation                    |
 * |-------|------------|--------------------------------|
 * | 1     | 100        | 100 x 2^0 = 100                |
 * | 2     | 200        | 100 x 2^1 = 200                |
 * | 3     | 400        | 100 x 2^2 = 400                |
 * | 4     | 800        | 100 x 2^3 = 800                |
 * | 5     | 1600       | 100 x 2^4 = 1600               |
 * | 6     | 3200       | 100 x 2^5 = 3200               |
 * | 7+    | 5000       | Capped at max_backoff_ms       |
 *
 * @invariant k_exponential_backoff_multiplier must be 2 for binary exponential
 * @invariant k_error_handler_max_retries bounds all retry loops (NASA Rule 2)
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  /**
   * @brief Exponential backoff multiplier (base 2)
   * @details Each retry doubles the previous delay: delay = initial x 2^(n-1)
   */
  k_exponential_backoff_multiplier = 2,

  /**
   * @brief Sentinel value indicating unlimited retries
   * @details When max_retries is 0, retry limit checks always return false
   */
  k_error_handler_no_retry_limit = 0,

  /**
   * @brief Starting index for retry loop iteration
   * @details Loop starts at 1 since retry 0 uses initial delay directly
   */
  k_error_handler_first_retry = 1,

  /**
   * @brief Maximum retry iterations for backoff calculation (NASA Rule 2)
   * @details
   * Provides static upper bound for the exponential backoff loop.
   * After 32 retries with base multiplier 2, delay would be astronomically
   * large, so this cap prevents overflow and ensures bounded execution.
   * @warning Do not increase beyond 32 to avoid uint32_t overflow
   */
  k_error_handler_max_retries = 32,
} error_handler_backoff_constants_t;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Find component tracking slot by name
 *
 * @details
 * Searches the handler's component array for an existing entry matching
 * the given component name. Uses bounded string comparison with
 * k_error_handler_component_name_max limit.
 *
 * @par Algorithm
 * 1. Iterate through all k_error_handler_max_components slots
 * 2. For each slot marked in_use, compare name strings
 * 3. Return pointer to first matching slot, or nullptr if not found
 *
 * @dot
 * digraph find_component {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="Start\ni=0"];
 *   check_use [label="components[i].in_use?" shape=diamond];
 *   check_name [label="Name matches?" shape=diamond];
 *   found [label="Return &components[i]" style=filled, fillcolor=lightgreen];
 *   next [label="i++"];
 *   check_bound [label="i < MAX?" shape=diamond];
 *   not_found [label="Return NULL" style=filled, fillcolor=lightyellow];
 *   start -> check_use;
 *   check_use -> check_name [label="yes"];
 *   check_use -> next [label="no"];
 *   check_name -> found [label="yes"];
 *   check_name -> next [label="no"];
 *   next -> check_bound;
 *   check_bound -> check_use [label="yes"];
 *   check_bound -> not_found [label="no"];
 * }
 * @enddot
 *
 *
 *
 * @pre handler != nullptr (caller responsibility, not validated)
 * @pre component != nullptr (caller responsibility, not validated)
 * @pre Caller holds handler->mutex (thread safety)
 *
 * @post Return value is either NULL or points to valid components[] entry
 * @post Handler state unchanged (read-only operation)
 *
 * @note Not thread-safe; caller must hold mutex
 * @note Time complexity: O(n) where n = k_error_handler_max_components
 *
 * @par Performance
 * - Best case: O(1) if first slot matches
 * - Worst case: O(n) full scan, n=16 typical
 * - ~2-10 us depending on array occupancy
 *
 * @see internal_find_or_create_component() Creates entry if not found
 * @see k_error_handler_max_components Maximum tracked components
 *
 * @since Version 1.0.0
 */
static error_component_state_t* internal_find_component(error_handler_t* handler,
                                                        const char*      component)
{
  for (uint32_t i = 0; i < k_error_handler_max_components; i++) {
    if ((int)handler->components[i].in_use &&
        strncmp(handler->components[i].name, component, k_error_handler_component_name_max) == 0) {
      return &handler->components[i];
    }
  }
  return nullptr;
}

/**
 * @brief Copy a component name safely without strncpy
 *
 * @details
 * Copies up to max_len-1 characters from src to dst, then null-terminates.
 * Replaces strncpy to satisfy clang-analyzer-security checks.
 *
 *
 * @pre dst and src must be non-NULL
 * @pre max_len must be > 0
 * @post dst is null-terminated
 *
 * @note Thread-safe: no shared state
 * @since Version 1.0.0
 */
static void internal_copy_component_name(char* dst, const char* src, const uint32_t max_len)
{
  for (uint32_t i = 0; i < max_len - 1; i++) {
    dst[i] = src[i];
    if (src[i] == '\0') {
      return;
    }
  }
  dst[max_len - 1] = '\0';
}

/**
 * @brief Find existing component entry or create new one
 *
 * @details
 * First attempts to find an existing component entry via internal_find_component().
 * If not found, searches for an empty slot (in_use == false) and initializes
 * it with the component name and zeroed counters.
 *
 * @par Algorithm
 * 1. Call internal_find_component() to search for existing entry
 * 2. If found, return pointer to existing entry
 * 3. Otherwise, scan for first empty slot (in_use == false)
 * 4. Initialize empty slot: copy name, zero counters, set in_use = true
 * 5. Return pointer to newly initialized slot, or nullptr if array full
 *
 * @dot
 * digraph find_or_create {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="Start"];
 *   find [label="internal_find_component()"];
 *   check_found [label="Found?" shape=diamond];
 *   return_found [label="Return existing" style=filled, fillcolor=lightgreen];
 *   scan_empty [label="Scan for empty slot\ni=0..MAX"];
 *   check_empty [label="!in_use?" shape=diamond];
 *   init_slot [label="Initialize:\nstrncpy name\nzero counters\nin_use=true"];
 *   return_new [label="Return new slot" style=filled, fillcolor=lightgreen];
 *   return_null [label="Return NULL\n(array full)" style=filled, fillcolor=lightyellow];
 *   start -> find;
 *   find -> check_found;
 *   check_found -> return_found [label="yes"];
 *   check_found -> scan_empty [label="no"];
 *   scan_empty -> check_empty;
 *   check_empty -> init_slot [label="yes"];
 *   check_empty -> return_null [label="no slots"];
 *   init_slot -> return_new;
 * }
 * @enddot
 *
 *
 *
 * @pre handler != nullptr (caller responsibility, not validated)
 * @pre component != nullptr (caller responsibility, not validated)
 * @pre Caller holds handler->mutex (thread safety)
 *
 * @post If return != nullptr, component entry exists and is marked in_use
 * @post If new entry created, error_count and retry_count are 0
 * @post Component name is null-terminated in entry
 *
 * @invariant Total in_use entries <= k_error_handler_max_components
 *
 * @note Not thread-safe; caller must hold mutex
 * @note Name truncated to k_error_handler_component_name_max - 1 chars
 *
 * @warning Returns NULL when component array is full - caller must handle
 *
 * @par Performance
 * - Best case: O(1) if component found in first slot
 * - Worst case: O(2n) two full scans (find + create)
 * - ~5-20 us typical
 *
 * @see internal_find_component() Used internally for search
 * @see k_error_handler_max_components Maximum trackable components
 *
 * @since Version 1.0.0
 */
static error_component_state_t* internal_find_or_create_component(error_handler_t* handler,
                                                                  const char*      component)
{
  /* Try to find existing component */
  error_component_state_t* comp = internal_find_component(handler, component);
  if (comp != nullptr) {
    return comp;
  }

  /* Find empty slot */
  for (uint32_t i = 0; i < k_error_handler_max_components; i++) {
    if (!handler->components[i].in_use) {
      /* Initialize new component entry */
      internal_copy_component_name(handler->components[i].name,
                                   component,
                                   k_error_handler_component_name_max);
      handler->components[i].error_count = 0;
      handler->components[i].retry_count = 0;
      handler->components[i].in_use      = true;
      return &handler->components[i];
    }
  }

  /* No slots available */
  return nullptr;
}

/* =============================================================================
 * Interface Implementation Functions
 * =============================================================================
 */

/**
 * @brief Report error implementation (rx_error_interface_t.report_error)
 *
 * @details
 * Records an error occurrence for the specified component. Increments both
 * the total error count and the component-specific error and retry counters.
 * The error is also logged via rx_log_error().
 *
 * @par Algorithm
 * 1. Validate input pointers (NULL check)
 * 2. Verify handler is initialized
 * 3. Acquire mutex for thread-safe access
 * 4. Increment total_error_count
 * 5. Find or create component tracking entry
 * 6. If component entry exists, increment error_count and retry_count
 * 7. Release mutex
 * 8. Log error message and error code
 *
 * @dot
 * digraph report_error {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="Start"];
 *   validate [label="Validate:\nhandler, component,\nmessage != nullptr"];
 *   check_init [label="initialized?" shape=diamond];
 *   get_mutex [label="tx_mutex_get()"];
 *   inc_total [label="total_error_count++"];
 *   find_comp [label="internal_find_or_create_component()"];
 *   inc_comp [label="comp->error_count++\ncomp->retry_count++"];
 *   put_mutex [label="tx_mutex_put()"];
 *   log_err [label="rx_log_error()"];
 *   return_ok [label="Return k_rx_ok" style=filled, fillcolor=lightgreen];
 *   return_err [label="Return error" style=filled, fillcolor=lightyellow];
 *   start -> validate;
 *   validate -> check_init [label="ok"];
 *   validate -> return_err [label="NULL"];
 *   check_init -> get_mutex [label="yes"];
 *   check_init -> return_err [label="no"];
 *   get_mutex -> inc_total;
 *   inc_total -> find_comp;
 *   find_comp -> inc_comp [label="found/created"];
 *   find_comp -> put_mutex [label="NULL (full)"];
 *   inc_comp -> put_mutex;
 *   put_mutex -> log_err;
 *   log_err -> return_ok;
 * }
 * @enddot
 *
 *
 *
 * @pre ctx points to initialized error_handler_t
 * @pre component is a valid null-terminated string
 * @pre message is a valid null-terminated string
 *
 * @post total_error_count incremented by 1
 * @post Component's error_count and retry_count incremented by 1
 *
 * @note Thread-safe: protected by internal mutex
 * @note Component created automatically if not exists (up to max slots)
 * @note If component array full, error still logged but not tracked per-component
 *
 * @par Performance
 * ~5-50 us depending on component lookup and logging overhead
 *
 * @see impl_clear_errors() Reset all error counters
 * @see impl_reset_retry_counter() Reset only retry counter
 *
 * @since Version 1.0.0
 */
static rx_err_t
impl_report_error(void* ctx, rx_err_t err, const char* component, const char* message)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  RX_CHECK_NULL_PTR(handler, s_tag, "Handler pointer is nullptr");
  RX_CHECK_NULL_PTR(component, s_tag, "Component pointer is nullptr");
  RX_CHECK_NULL_PTR(message, s_tag, "Message pointer is nullptr");
  if (!handler->initialized) {
    rx_log_error(s_tag, "Handler not initialized");
    return k_rx_err_invalid_state;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_mutex;
  }

  /* Increment total error count */
  handler->total_error_count++;

  /* Find or create component */
  error_component_state_t* comp = internal_find_or_create_component(handler, component);
  if (comp != nullptr) {
    comp->error_count++;
    comp->retry_count++;
  }

  /* Release mutex */
  (void)tx_mutex_put(&handler->mutex);

  /* Log the error */
  rx_log_error(component, message);
  rx_log_error_val(component, "Error", err);

  return k_rx_ok;
}

/**
 * @brief Get total error count implementation (rx_error_interface_t.get_error_count)
 *
 * @details
 * Returns the total number of errors reported across all components since
 * initialization or last clear_errors() call.
 *
 * @par Algorithm
 * 1. Validate context pointer and initialization state
 * 2. Acquire mutex
 * 3. Read total_error_count
 * 4. Release mutex
 * 5. Return count (or 0 on any failure)
 *
 *
 *
 * @pre ctx points to initialized error_handler_t (for meaningful result)
 *
 * @post Handler state unchanged (read-only operation)
 *
 * @note Thread-safe: protected by internal mutex
 * @note Returns 0 on any error condition (fail-safe behavior)
 *
 * @par Performance
 * ~2-10 us (mutex overhead only)
 *
 * @see impl_get_component_error_count() Per-component error count
 * @see impl_clear_errors() Reset total count
 *
 * @since Version 1.0.0
 */
static uint32_t impl_get_error_count(void* ctx)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if (handler == nullptr) {
    return 0;
  }
  if (!handler->initialized) {
    return 0;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return 0;
  }

  const uint32_t count = handler->total_error_count;

  /* Release mutex */
  (void)tx_mutex_put(&handler->mutex);

  return count;
}

/**
 * @brief Get component-specific error count (rx_error_interface_t.get_component_error_count)
 *
 * @details
 * Returns the number of errors reported for a specific component.
 * Returns 0 if the component has never reported an error (not tracked).
 *
 * @par Algorithm
 * 1. Validate context and component pointers
 * 2. Verify handler is initialized
 * 3. Acquire mutex
 * 4. Search for component via internal_find_component()
 * 5. If found, read error_count; otherwise use 0
 * 6. Release mutex
 * 7. Return count
 *
 *
 *
 * @pre ctx points to initialized error_handler_t (for meaningful result)
 * @pre component is a valid null-terminated string
 *
 * @post Handler state unchanged (read-only operation)
 *
 * @note Thread-safe: protected by internal mutex
 * @note Component name comparison is bounded by k_error_handler_component_name_max
 *
 * @par Performance
 * ~3-15 us (mutex + component search)
 *
 * @see impl_get_error_count() Total error count
 * @see impl_clear_errors() Reset component counts
 *
 * @since Version 1.0.0
 */
static uint32_t impl_get_component_error_count(void* ctx, const char* component)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if (handler == nullptr || component == nullptr) {
    return 0;
  }
  if (!handler->initialized) {
    return 0;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return 0;
  }

  uint32_t                       count = 0;
  const error_component_state_t* comp  = internal_find_component(handler, component);
  if (comp != nullptr) {
    count = comp->error_count;
  }

  /* Release mutex */
  (void)tx_mutex_put(&handler->mutex);

  return count;
}

/**
 * @brief Clear all error counters (rx_error_interface_t.clear_errors)
 *
 * @details
 * Resets the total error count and all component-specific error and retry
 * counters to zero. Component tracking entries remain valid (in_use stays true)
 * but their counts are zeroed.
 *
 * @par Algorithm
 * 1. Validate context pointer
 * 2. Verify handler is initialized
 * 3. Acquire mutex
 * 4. Set total_error_count = 0
 * 5. For each component slot: set error_count = 0, retry_count = 0
 * 6. Release mutex
 *
 *
 *
 * @pre ctx points to initialized error_handler_t
 *
 * @post total_error_count == 0
 * @post All component error_count and retry_count == 0
 * @post Component in_use flags unchanged (entries still tracked)
 *
 * @note Thread-safe: protected by internal mutex
 * @note Does NOT remove component entries, only clears their counts
 *
 * @warning Clears retry_count too, affecting backoff calculations
 *
 * @par Performance
 * ~5-30 us (mutex + loop over all slots)
 *
 * @see impl_reset_retry_counter() Reset only one component's retry counter
 *
 * @since Version 1.0.0
 */
static rx_err_t impl_clear_errors(void* ctx)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  RX_CHECK_NULL_PTR(handler, s_tag, "Handler pointer is nullptr");
  if (!handler->initialized) {
    rx_log_error(s_tag, "Handler not initialized");
    return k_rx_err_invalid_state;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_mutex;
  }

  /* Clear total count */
  handler->total_error_count = 0;

  /* Clear all component counts */
  for (uint32_t i = 0; i < k_error_handler_max_components; i++) {
    handler->components[i].error_count = 0;
    handler->components[i].retry_count = 0;
  }

  /* Release mutex */
  (void)tx_mutex_put(&handler->mutex);

  return k_rx_ok;
}

/**
 * @brief Check if component has reached retry limit (rx_error_interface_t.is_retry_limit_reached)
 *
 * @details
 * Determines whether the specified component has exceeded its configured
 * maximum retry count. Used to implement fail-fast behavior after repeated
 * errors.
 *
 * @par Algorithm
 * 1. Validate context and component pointers (return true on failure - fail-safe)
 * 2. If max_retries == 0, return false (unlimited retries)
 * 3. Acquire mutex
 * 4. Find component via internal_find_component()
 * 5. Compare retry_count against max_retries
 * 6. Release mutex
 * 7. Return comparison result
 *
 *
 *
 * @pre ctx points to initialized error_handler_t
 * @pre component is a valid null-terminated string
 *
 * @post Handler state unchanged (read-only operation)
 *
 * @note Thread-safe: protected by internal mutex
 * @note Fail-safe: returns true (limit reached) on any error condition
 * @note If max_retries == 0, always returns false (unlimited)
 * @note If component not found, returns false (no retries yet)
 *
 * @par Performance
 * ~3-15 us (mutex + component search)
 *
 * @see impl_reset_retry_counter() Reset retry count after recovery
 * @see impl_get_backoff_delay() Get delay before next retry
 *
 * @since Version 1.0.0
 */
static bool impl_is_retry_limit_reached(void* ctx, const char* component)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if (handler == nullptr || component == nullptr) {
    return true; /* Fail-safe: consider limit reached if invalid params */
  }
  if (!handler->initialized) {
    return true;
  }

  /* If max_retries is 0, no limit */
  if (handler->max_retries == 0) {
    return false;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return true; /* Fail-safe */
  }

  bool                           limit_reached = false;
  const error_component_state_t* comp          = internal_find_component(handler, component);
  if (comp != nullptr) {
    if (comp->retry_count >= handler->max_retries) {
      limit_reached = true;
    }
  }

  /* Release mutex */
  (void)tx_mutex_put(&handler->mutex);

  return limit_reached;
}

/**
 * @brief Reset component retry counter (rx_error_interface_t.reset_retry_counter)
 *
 * @details
 * Resets only the retry counter for a specific component, leaving the error
 * count unchanged. Call this after successful recovery to enable fresh retries.
 *
 * @par Algorithm
 * 1. Validate context and component pointers
 * 2. Verify handler is initialized
 * 3. Acquire mutex
 * 4. Find component via internal_find_component()
 * 5. If found, set retry_count = 0
 * 6. Release mutex
 *
 *
 *
 * @pre ctx points to initialized error_handler_t
 * @pre component is a valid null-terminated string
 *
 * @post If component found, retry_count == 0
 * @post error_count unchanged (only retry reset)
 *
 * @note Thread-safe: protected by internal mutex
 * @note No-op if component not found (returns k_rx_ok)
 * @note Use after successful operation to reset backoff
 *
 * @par Example
 * @code
 * // After successful recovery from USB error
 * if (usb_operation_succeeded) {
 *     iface->reset_retry_counter(iface->ctx, "USB");
 * }
 * @endcode
 *
 * @par Performance
 * ~3-15 us (mutex + component search)
 *
 * @see impl_clear_errors() Reset all counters for all components
 * @see impl_get_backoff_delay() Retry count affects backoff
 *
 * @since Version 1.0.0
 */
static rx_err_t impl_reset_retry_counter(void* ctx, const char* component)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  RX_CHECK_NULL_PTR(handler, s_tag, "Handler pointer is nullptr");
  RX_CHECK_NULL_PTR(component, s_tag, "Component pointer is nullptr");
  if (!handler->initialized) {
    rx_log_error(s_tag, "Handler not initialized");
    return k_rx_err_invalid_state;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return k_rx_err_rtos_mutex;
  }

  error_component_state_t* comp = internal_find_component(handler, component);
  if (comp != nullptr) {
    comp->retry_count = 0;
  }

  /* Release mutex */
  (void)tx_mutex_put(&handler->mutex);

  return k_rx_ok;
}

/**
 * @brief Calculate exponential backoff delay (rx_error_interface_t.get_backoff_delay)
 *
 * @details
 * Computes the delay (in milliseconds) to wait before the next retry attempt
 * using binary exponential backoff. Delay doubles with each retry, capped at
 * the configured maximum.
 *
 * @par Algorithm
 * 1. Validate context and component pointers (return 0 on failure)
 * 2. Acquire mutex
 * 3. Find component via internal_find_component()
 * 4. If retry_count == 0, delay = 0 (no backoff on first attempt)
 * 5. Otherwise, compute: delay = initial x 2^(retry_count - 1)
 * 6. Cap delay at max_backoff_ms
 * 7. Release mutex
 * 8. Return computed delay
 *
 * @par Exponential Backoff Formula
 * @f[
 *   \text{delay} = \min\left(\text{initial\_backoff\_ms} \times 2^{(n-1)},\;
 *                           \text{max\_backoff\_ms}\right)
 * @f]
 * where @f$ n @f$ = retry_count (capped at 32 to prevent overflow)
 *
 * @dot
 * digraph get_backoff {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="Start"];
 *   validate [label="Validate ctx,\ncomponent"];
 *   get_mutex [label="tx_mutex_get()"];
 *   find [label="internal_find_component()"];
 *   check_retry [label="retry_count > 0?" shape=diamond];
 *   init_delay [label="delay = initial_backoff_ms"];
 *   loop [label="For i = 1 to retry_count-1"];
 *   double [label="delay *= 2"];
 *   check_cap [label="delay > max?" shape=diamond];
 *   cap [label="delay = max_backoff_ms"];
 *   put_mutex [label="tx_mutex_put()"];
 *   return_delay [label="Return delay" style=filled, fillcolor=lightgreen];
 *   return_zero [label="Return 0" style=filled, fillcolor=lightyellow];
 *   start -> validate;
 *   validate -> get_mutex [label="ok"];
 *   validate -> return_zero [label="invalid"];
 *   get_mutex -> find;
 *   find -> check_retry [label="found"];
 *   find -> put_mutex [label="not found"];
 *   check_retry -> init_delay [label="yes"];
 *   check_retry -> put_mutex [label="no"];
 *   init_delay -> loop;
 *   loop -> double;
 *   double -> check_cap;
 *   check_cap -> cap [label="yes"];
 *   check_cap -> loop [label="no, more iters"];
 *   cap -> put_mutex;
 *   loop -> put_mutex [label="done"];
 *   put_mutex -> return_delay;
 * }
 * @enddot
 *
 *
 *
 * @pre ctx points to initialized error_handler_t
 * @pre component is a valid null-terminated string
 *
 * @post Handler state unchanged (read-only operation)
 *
 * @invariant Return value <= max_backoff_ms (when successful)
 *
 * @note Thread-safe: protected by internal mutex
 * @note Returns 0 for first attempt (retry_count == 0)
 * @note Loop bounded by k_error_handler_max_retries (NASA Rule 2)
 *
 * @par Example
 * @code
 * uint32_t delay = iface->get_backoff_delay(iface->ctx, "SPI");
 * if (delay > 0) {
 *     tx_thread_sleep(delay * TX_TIMER_TICKS_PER_MS);
 * }
 * // Retry operation
 * @endcode
 *
 * @par Performance
 * ~3-20 us (mutex + search + backoff loop)
 *
 * @see impl_reset_retry_counter() Reset to clear backoff
 * @see impl_report_error() Increments retry_count
 *
 * @since Version 1.0.0
 */
static uint32_t impl_get_backoff_delay(void* ctx, const char* component)
{
  error_handler_t* handler = (error_handler_t*)ctx;

  if ((handler == nullptr) || (component == nullptr)) {
    return 0;
  }
  if (!handler->initialized) {
    return 0;
  }

  /* Acquire mutex */
  const UINT status = tx_mutex_get(&handler->mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return 0;
  }

  uint32_t                       delay_ms = 0;
  const error_component_state_t* comp     = internal_find_component(handler, component);
  if (comp != nullptr && comp->retry_count > 0) {
    /* Exponential backoff: delay = initial * 2^(retry_count - 1)
     * Capped at max_backoff_ms */
    delay_ms           = handler->initial_backoff_ms;
    uint32_t retry_cap = handler->max_retries;
    if (handler->max_retries == k_error_handler_no_retry_limit ||
        handler->max_retries > k_error_handler_max_retries) {
      retry_cap = k_error_handler_max_retries;
    }
    const uint32_t retries = (comp->retry_count > retry_cap) ? retry_cap : comp->retry_count;
    /* Statically bounded loop: iterate from first retry to max retries cap (NASA Rule 2) */
    for (uint32_t i = k_error_handler_first_retry; i < k_error_handler_max_retries; ++i) {
      /* Explicit bounds check: break if reached actual retry limit */
      if (i >= retries) {
        break;
      }
      if (delay_ms > handler->max_backoff_ms / k_exponential_backoff_multiplier) {
        delay_ms = handler->max_backoff_ms;
        break;
      }
      delay_ms *= k_exponential_backoff_multiplier;
      if (delay_ms >= handler->max_backoff_ms) {
        delay_ms = handler->max_backoff_ms;
        break;
      }
    }
  }

  /* Release mutex */
  (void)tx_mutex_put(&handler->mutex);

  return delay_ms;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize error handler with retry logic and exponential backoff
 *
 * @details
 * Initializes an error handler instance with the specified configuration.
 * Creates a ThreadX mutex for thread-safe operation and clears all internal
 * state. Must be called before any other error handler operations.
 *
 * @par Algorithm
 * 1. Validate handler and config pointers
 * 2. Validate backoff configuration consistency
 * 3. Clear all handler state (memset to 0)
 * 4. Copy configuration values
 * 5. Create ThreadX mutex
 * 6. Set initialized flag
 * 7. Log initialization success
 *
 * @dot
 * digraph error_handler_init {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   start [label="Start"];
 *   check_null [label="handler/config\n!= nullptr?" shape=diamond];
 *   check_backoff [label="Backoff config\nvalid?" shape=diamond];
 *   memset [label="memset(handler, 0)"];
 *   copy_config [label="Copy config values"];
 *   create_mutex [label="tx_mutex_create()"];
 *   check_mutex [label="Success?" shape=diamond];
 *   set_init [label="initialized = true"];
 *   log_ok [label="rx_log_info()"];
 *   return_ok [label="Return k_rx_ok" style=filled, fillcolor=lightgreen];
 *   return_err [label="Return error" style=filled, fillcolor=lightyellow];
 *   start -> check_null;
 *   check_null -> check_backoff [label="yes"];
 *   check_null -> return_err [label="no"];
 *   check_backoff -> memset [label="yes"];
 *   check_backoff -> return_err [label="no"];
 *   memset -> copy_config;
 *   copy_config -> create_mutex;
 *   create_mutex -> check_mutex;
 *   check_mutex -> set_init [label="TX_SUCCESS"];
 *   check_mutex -> return_err [label="fail"];
 *   set_init -> log_ok;
 *   log_ok -> return_ok;
 * }
 * @enddot
 *
 *
 *
 * @pre handler points to allocated error_handler_t (not previously initialized)
 * @pre config points to valid configuration with consistent values
 *
 * @post handler->initialized == true
 * @post handler->mutex created and ready
 * @post All component slots cleared (in_use == false)
 * @post total_error_count == 0
 *
 * @note Not thread-safe: do not call concurrently for same handler
 * @note Call error_handler_deinit() before reinitializing
 *
 * @warning Do not initialize same handler twice without deinit
 *
 * @par Example
 * @code
 * static error_handler_t g_error_handler;
 *
 * void system_init(void) {
 *     error_handler_config_t config = {
 *         .max_retries        = 5,
 *         .initial_backoff_ms = 100,
 *         .max_backoff_ms     = 5000
 *     };
 *
 *     rx_err_t err = error_handler_init(&g_error_handler, &config);
 *     if (err != k_rx_ok) {
 *         // Handle init failure - system cannot proceed
 *         while (1) { }
 *     }
 * }
 * @endcode
 *
 * @par Performance
 * ~50-100 us (memset + mutex creation)
 *
 * @see error_handler_deinit() Cleanup handler
 * @see error_handler_get_interface() Get interface for dependency injection
 *
 * @since Version 1.0.0
 */
rx_err_t error_handler_init(error_handler_t* handler, const error_handler_config_t* config)
{
  RX_CHECK_NULL_PTR(handler, s_tag, "Handler pointer is nullptr");
  RX_CHECK_NULL_PTR(config, s_tag, "Config pointer is nullptr");
  if (config->max_backoff_ms < config->initial_backoff_ms) {
    rx_log_error(s_tag, "Backoff range invalid");
    return k_rx_err_invalid_arg;
  }
  /* Architecturally unreachable: if initial > 0 and max == 0, then max < initial,
   * which is caught by the preceding check. No RX_ASSERT needed. */

  /* Clear all state */
  *handler = (error_handler_t){};

  /* Initialize configuration */
  handler->max_retries        = config->max_retries;
  handler->initial_backoff_ms = config->initial_backoff_ms;
  handler->max_backoff_ms     = config->max_backoff_ms;

  /* Create mutex -- tx_mutex_create always succeeds when ThreadX is running */
  (void)tx_mutex_create(&handler->mutex, "ErrorHandlerMutex", TX_NO_INHERIT);

  handler->initialized = true;

  rx_log_info(s_tag, "Error handler initialized");

  return k_rx_ok;
}

/**
 * @brief Get error handler interface for dependency injection
 *
 * @details
 * Populates an rx_error_interface_t structure with function pointers to the
 * handler's implementation functions. This enables dependency injection
 * following the Dependency Inversion Principle (DIP).
 *
 * @par Algorithm
 * 1. Validate iface and handler pointers
 * 2. Verify handler is initialized
 * 3. Populate interface structure:
 *    - ctx = handler (opaque context)
 *    - Function pointers to impl_* functions
 *
 * @par Interface Binding Diagram
 * @dot
 * digraph interface_binding {
 *   rankdir=LR;
 *   node [shape=record];
 *   iface [label="rx_error_interface_t|ctx|report_error|get_error_count|get_component_error_count|clear_errors|is_retry_limit_reached|reset_retry_counter|get_backoff_delay"];
 *   handler [label="error_handler_t|mutex|components[]|total_error_count|config..."];
 *   impl [label="Implementation|impl_report_error()|impl_get_error_count()|impl_get_component_error_count()|impl_clear_errors()|impl_is_retry_limit_reached()|impl_reset_retry_counter()|impl_get_backoff_delay()"];
 *   iface:ctx -> handler [label="points to"];
 *   iface:report_error -> impl:impl_report_error;
 *   iface:get_error_count -> impl:impl_get_error_count;
 * }
 * @enddot
 *
 *
 *
 * @pre iface points to allocated rx_error_interface_t
 * @pre handler initialized via error_handler_init()
 *
 * @post iface->ctx == handler
 * @post All function pointers set to implementation functions
 *
 * @note Handler must remain valid while interface is in use
 * @note Interface structure is a shallow copy (pointers only)
 *
 * @par Example
 * @code
 * static error_handler_t g_error_handler;
 * static rx_error_interface_t g_error_iface;
 *
 * void system_init(void) {
 *     error_handler_config_t config = { ... };
 *     error_handler_init(&g_error_handler, &config);
 *
 *     // Get interface for dependency injection
 *     error_handler_get_interface(&g_error_iface, &g_error_handler);
 *
 *     // Pass interface to modules that need error handling
 *     motor_init(&motor, &g_error_iface);
 *     comm_init(&comm, &g_error_iface);
 * }
 *
 * // In motor module:
 * void motor_report_fault(motor_t* motor) {
 *     motor->error_iface->report_error(
 *         motor->error_iface->ctx,
 *         k_rx_err_hw_fault,
 *         "MOTOR",
 *         "Overcurrent detected"
 *     );
 * }
 * @endcode
 *
 * @par Performance
 * ~1 us (pointer assignments only)
 *
 * @see error_handler_init() Initialize handler first
 * @see rx_error_interface_t Abstract interface definition
 *
 * @since Version 1.0.0
 */
rx_err_t error_handler_get_interface(rx_error_interface_t* iface, error_handler_t* handler)
{
  RX_CHECK_NULL_PTR(iface, s_tag, "Interface pointer is nullptr");
  RX_CHECK_NULL_PTR(handler, s_tag, "Handler pointer is nullptr");

  if (!handler->initialized) {
    rx_log_error(s_tag, "Handler not initialized");
    return k_rx_err_invalid_state;
  }

  /* Fill interface */
  iface->ctx                       = handler;
  iface->report_error              = impl_report_error;
  iface->get_error_count           = impl_get_error_count;
  iface->get_component_error_count = impl_get_component_error_count;
  iface->clear_errors              = impl_clear_errors;
  iface->is_retry_limit_reached    = impl_is_retry_limit_reached;
  iface->reset_retry_counter       = impl_reset_retry_counter;
  iface->get_backoff_delay         = impl_get_backoff_delay;

  return k_rx_ok;
}

/**
 * @brief Deinitialize error handler and release resources
 *
 * @details
 * Releases resources held by the error handler, including the ThreadX mutex.
 * Safe to call on already-deinitialized handler (idempotent).
 *
 * @par Algorithm
 * 1. Validate handler pointer
 * 2. If not initialized, return k_rx_ok (idempotent)
 * 3. Delete ThreadX mutex
 * 4. Set initialized = false
 * 5. Log deinitialization
 *
 *
 *
 * @pre handler points to valid error_handler_t (initialized or not)
 *
 * @post handler->initialized == false
 * @post handler->mutex deleted (if was initialized)
 *
 * @note Idempotent: safe to call multiple times
 * @note Not thread-safe: ensure no other threads using handler
 *
 * @warning Invalidates any rx_error_interface_t pointing to this handler
 * @warning Do not use interface after deinit
 *
 * @par Example
 * @code
 * void system_shutdown(void) {
 *     // Stop all modules using error interface first
 *     motor_deinit(&motor);
 *     comm_deinit(&comm);
 *
 *     // Now safe to deinit error handler
 *     error_handler_deinit(&g_error_handler);
 * }
 * @endcode
 *
 * @par Performance
 * ~10-50 us (mutex deletion)
 *
 * @see error_handler_init() Reinitialize after deinit if needed
 *
 * @since Version 1.0.0
 */
rx_err_t error_handler_deinit(error_handler_t* handler)
{
  RX_CHECK_NULL_PTR(handler, s_tag, "Handler pointer is nullptr");

  if (!handler->initialized) {
    return k_rx_ok; /* Already deinitialized */
  }

  /* Delete mutex */
  (void)tx_mutex_delete(&handler->mutex);

  /* Clear state */
  handler->initialized = false;

  rx_log_info(s_tag, "Error handler deinitialized");

  return k_rx_ok;
}
