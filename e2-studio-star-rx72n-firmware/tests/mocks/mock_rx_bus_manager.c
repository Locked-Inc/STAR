/* tests/mocks/mock_rx_bus_manager.c */

/**
 * @file mock_rx_bus_manager.c
 * @brief Mock Bus Manager Implementation for Host-Side Testing
 *
 * @details
 * Provides simplified bus manager functionality for testing bus abstraction
 * layers (rx_bus_onewire, rx_bus_i2c, etc.) without full ThreadX integration.
 * Replaces ThreadX mutex operations with simple boolean flag toggling and
 * omits interrupt-level protections that are not applicable on host.
 *
 * All linked-list traversals are bounded by k_max_buses to comply with
 * NASA Power of 10 Rule 2 (fixed loop upper-bounds). Each public function
 * validates at least 2 preconditions per NASA Rule 5.
 *
 * @author STAR Team
 * @version 1.0.0
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 *
 * @par NASA Power of 10 Compliance:
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | **Rule 1** | [PASS] | No goto, setjmp, recursion |
 * | **Rule 2** | [PASS] | All loops bounded by k_max_buses |
 * | **Rule 3** | [PASS] | No dynamic allocation |
 * | **Rule 4** | [PASS] | Functions <= 60 lines |
 * | **Rule 5** | [PASS] | Min 2 pre/postconditions per function |
 * | **Rule 7** | [PASS] | All return values checked |
 * | **Rule 8** | [PASS] | No preprocessor constants |
 * | **Rule 10** | [PASS] | Compiled with -Wall -Wextra -Werror |
 *
 * @par SOLID Principles:
 * - **S**: Mock bus manager only -- single responsibility
 * - **L**: Substitutes real rx_bus_manager in test builds
 * - **D**: Injected error_iface and pin_iface for dependency inversion
 *
 * @see rx_bus_manager.h Public API declarations
 * @see rx_bus_manager.c Production implementation with ThreadX
 * @see rx_bus_types.h Type definitions (rx_bus_manager_t, rx_bus_config_t)
 */

#include <assert.h>
#include <string.h>

#include "rx_bus_manager.h"

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize mock bus manager with injected dependencies
 *
 * @details
 * Sets up the bus manager structure with the provided tag, error interface,
 * and pin interface. Clears all state via memset and initializes the mock
 * mutex by setting the magic ID. No ThreadX calls are made in the mock.
 *
 * @param[in,out] manager  Bus manager to initialize. Must be non-NULL.
 * @param[in]     tag      Logging tag string. Must be non-NULL.
 * @param[in]     error_iface Error reporting interface (may be NULL for tests
 *                            that do not exercise error paths)
 * @param[in]     pin_iface   Pin validation interface (may be NULL for tests
 *                            that do not exercise pin reservation)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok            Manager initialized successfully
 * @retval k_rx_err_null_ptr  manager or tag is nullptr
 *
 * @pre manager != nullptr
 * @pre tag != nullptr
 * @post manager->bus_count == 0
 * @post manager->buses == nullptr
 * @post manager->mutex.tx_mutex_id == k_tx_mutex_magic
 *
 * @note Not thread-safe -- call from single-threaded test setup only
 *
 * @see rx_bus_manager_deinit() Tear down manager
 * @see rx_bus_manager_add_bus() Register buses after init
 *
 * @since STAR v1.0.0
 */
rx_err_t rx_bus_manager_init(rx_bus_manager_t*     manager,
                             const char*           tag,
                             rx_error_interface_t* error_iface,
                             rx_pin_interface_t*   pin_iface)
{
  /* Pre-condition: manager must not be null */
  if (manager == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Pre-condition: tag must not be null */
  if (tag == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Clear manager state */
  (void)memset(manager, 0, sizeof(rx_bus_manager_t));

  /* Store configuration */
  manager->tag         = tag;
  manager->error_iface = error_iface;
  manager->pin_iface   = pin_iface;
  manager->buses       = nullptr;
  manager->bus_count   = 0;

  /* Create mutex (mock version - just sets ID) */
  manager->mutex.tx_mutex_id   = k_tx_mutex_magic;
  manager->mutex.tx_mutex_name = (char*)"BusMutex";
  manager->mutex.locked        = false;

  /* Post-condition: bus list is empty */
  assert(manager->bus_count == 0);
  assert(manager->buses == nullptr);

  return k_rx_ok;
}

/**
 * @brief De-initialize mock bus manager and clear all state
 *
 * @details
 * Resets the bus manager to an uninitialized state by clearing the bus list,
 * count, and invalidating the mock mutex ID. After this call the manager
 * must be re-initialized via rx_bus_manager_init() before further use.
 *
 * @param[in,out] manager Bus manager to de-initialize. Must be non-NULL and
 *                        previously initialized.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok           Manager de-initialized successfully
 * @retval k_rx_err_null_ptr manager is nullptr
 *
 * @pre manager != nullptr
 * @pre manager was previously initialized via rx_bus_manager_init()
 * @post manager->bus_count == 0
 * @post manager->buses == nullptr
 * @post manager->mutex.tx_mutex_id == k_tx_invalid_id
 *
 * @note Not thread-safe -- call from single-threaded test teardown only
 *
 * @see rx_bus_manager_init() Re-initialize after deinit
 *
 * @since STAR v1.0.0
 */
rx_err_t rx_bus_manager_deinit(rx_bus_manager_t* manager)
{
  /* Pre-condition: manager must not be null */
  if (manager == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Pre-condition: manager should have been initialized */
  assert(manager->mutex.tx_mutex_id == k_tx_mutex_magic);

  /* Clear all state */
  manager->buses             = nullptr;
  manager->bus_count         = 0;
  manager->mutex.tx_mutex_id = k_tx_invalid_id;

  /* Post-condition: bus list is empty */
  assert(manager->bus_count == 0);
  assert(manager->buses == nullptr);

  return k_rx_ok;
}

/**
 * @brief Add a bus configuration to the mock manager's registry
 *
 * @details
 * Inserts a bus configuration at the head of the linked list after checking
 * for duplicate names and capacity limits. The linked-list traversal for
 * duplicate detection is bounded by k_max_buses (NASA Rule 2).
 *
 * @param[in,out] manager    Bus manager. Must be non-NULL and initialized.
 * @param[in,out] bus_config Bus configuration to add. Must be non-NULL with
 *                           a valid (non-NULL, non-empty) name field.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok              Bus added successfully
 * @retval k_rx_err_null_ptr    manager or bus_config is nullptr
 * @retval k_rx_err_invalid_arg bus_config->name is nullptr or empty
 * @retval k_rx_err_exists      Bus with same name already registered
 * @retval k_rx_err_no_mem      Maximum buses reached (k_max_buses)
 *
 * @pre manager != nullptr
 * @pre bus_config != nullptr
 * @pre bus_config->name != nullptr and not empty
 * @post manager->bus_count incremented by 1 on success
 * @post bus_config is at the head of manager->buses on success
 *
 * @note Not thread-safe -- no mutex acquisition in mock
 *
 * @see rx_bus_manager_remove_bus() Remove a bus by name
 * @see rx_bus_manager_find_bus() Look up a bus by name
 *
 * @since STAR v1.0.0
 */
rx_err_t rx_bus_manager_add_bus(rx_bus_manager_t* manager, rx_bus_config_t* bus_config)
{
  /* Pre-condition: manager must not be null */
  if (manager == nullptr || bus_config == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Pre-condition: manager must be initialized */
  assert(manager->mutex.tx_mutex_id == k_tx_mutex_magic);

  /* Pre-condition: bus name must be valid */
  if (bus_config->name == nullptr || bus_config->name[0] == '\0') {
    return k_rx_err_invalid_arg;
  }

  /* Check for duplicate name (bounded traversal -- NASA Rule 2) */
  rx_bus_config_t* current   = manager->buses;
  uint8_t          loop_count = 0;
  while (current != nullptr && loop_count < k_max_buses) {
    if (strcmp(current->name, bus_config->name) == 0) {
      return k_rx_err_exists;
    }
    current = current->next;
    loop_count++;
  }

  /* Check max buses */
  if (manager->bus_count >= k_max_buses) {
    return k_rx_err_no_mem;
  }

  /* Add to linked list head */
  bus_config->next = manager->buses;
  manager->buses   = bus_config;
  manager->bus_count++;

  /* Post-condition: bus_count within bounds */
  assert(manager->bus_count <= k_max_buses);
  /* Post-condition: head of list is the newly added bus */
  assert(manager->buses == bus_config);

  return k_rx_ok;
}

/**
 * @brief Remove a bus from the mock manager's registry by name
 *
 * @details
 * Searches the linked list for a bus matching the given name and removes it.
 * The linked-list traversal is bounded by k_max_buses (NASA Rule 2).
 *
 * @param[in,out] manager Bus manager. Must be non-NULL and initialized.
 * @param[in]     name    Name of the bus to remove. Must be non-NULL.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok              Bus removed successfully
 * @retval k_rx_err_null_ptr    manager or name is nullptr
 * @retval k_rx_err_not_found   No bus with the given name
 *
 * @pre manager != nullptr
 * @pre name != nullptr
 * @post manager->bus_count decremented by 1 on success
 * @post Removed bus's next pointer set to nullptr
 *
 * @note Not thread-safe -- no mutex acquisition in mock
 *
 * @see rx_bus_manager_add_bus() Add a bus to the registry
 * @see rx_bus_manager_find_bus() Look up a bus by name
 *
 * @since STAR v1.0.0
 */
rx_err_t rx_bus_manager_remove_bus(rx_bus_manager_t* manager, const char* name)
{
  /* Pre-condition: manager must not be null */
  if (manager == nullptr || name == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Pre-condition: manager must be initialized */
  assert(manager->mutex.tx_mutex_id == k_tx_mutex_magic);

  /* Pre-condition: bus_count must be consistent */
  assert(manager->bus_count <= k_max_buses);

  rx_bus_config_t** pp         = &manager->buses;
  uint8_t           loop_count = 0;
  while (*pp != nullptr && loop_count < k_max_buses) {
    if (strcmp((*pp)->name, name) == 0) {
      rx_bus_config_t* to_remove = *pp;
      *pp                        = to_remove->next;
      to_remove->next            = nullptr;
      assert(manager->bus_count > 0);
      manager->bus_count--;

      /* Post-condition: removed node is detached */
      assert(to_remove->next == nullptr);
      /* Post-condition: bus_count still valid */
      assert(manager->bus_count <= k_max_buses);

      return k_rx_ok;
    }
    pp = &(*pp)->next;
    loop_count++;
  }

  return k_rx_err_not_found;
}

/**
 * @brief Find a bus configuration by name in the mock manager's registry
 *
 * @details
 * Searches the linked list for a bus matching the given name and returns
 * a pointer to it via the output parameter. The linked-list traversal is
 * bounded by k_max_buses (NASA Rule 2).
 *
 * @param[in]  manager    Bus manager. Must be non-NULL and initialized.
 * @param[in]  name       Name of the bus to find. Must be non-NULL.
 * @param[out] bus_config Pointer to receive found bus configuration.
 *                        Must be non-NULL. Set to found bus on success.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok            Bus found, *bus_config set
 * @retval k_rx_err_null_ptr  manager, name, or bus_config is nullptr
 * @retval k_rx_err_not_found No bus with the given name
 *
 * @pre manager != nullptr
 * @pre name != nullptr
 * @pre bus_config != nullptr
 * @post *bus_config points to matching rx_bus_config_t on success
 * @post *bus_config unchanged on failure
 *
 * @note Not thread-safe -- no mutex acquisition in mock
 *
 * @see rx_bus_manager_add_bus() Register a bus before lookup
 * @see rx_bus_manager_with_bus() Find and execute callback atomically
 *
 * @since STAR v1.0.0
 */
rx_err_t
rx_bus_manager_find_bus(rx_bus_manager_t* manager, const char* name, rx_bus_config_t** bus_config)
{
  /* Pre-condition: all pointers must be non-null */
  if (manager == nullptr || name == nullptr || bus_config == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Pre-condition: manager must be initialized */
  assert(manager->mutex.tx_mutex_id == k_tx_mutex_magic);

  /* Pre-condition: bus_count must be consistent */
  assert(manager->bus_count <= k_max_buses);

  rx_bus_config_t* current    = manager->buses;
  uint8_t          loop_count = 0;
  while (current != nullptr && loop_count < k_max_buses) {
    if (strcmp(current->name, name) == 0) {
      *bus_config = current;

      /* Post-condition: output is non-null */
      assert(*bus_config != nullptr);
      /* Post-condition: output name matches search name */
      assert(strcmp((*bus_config)->name, name) == 0);

      return k_rx_ok;
    }
    current = current->next;
    loop_count++;
  }

  return k_rx_err_not_found;
}

/**
 * @brief Find a bus by name and execute a callback while holding mock mutex
 *
 * @details
 * Locates the named bus via rx_bus_manager_find_bus(), acquires the mock
 * mutex (sets locked flag), invokes the callback with the bus and user
 * context, then releases the mock mutex. The callback return value is
 * propagated to the caller.
 *
 * @param[in,out] manager  Bus manager. Must be non-NULL and initialized.
 * @param[in]     name     Name of the bus to operate on. Must be non-NULL.
 * @param[in]     callback Function to invoke with the found bus. Must be non-NULL.
 * @param[in,out] user_ctx User-provided context passed to callback. May be NULL.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok            Callback executed successfully
 * @retval k_rx_err_null_ptr  manager, name, or callback is nullptr
 * @retval k_rx_err_not_found No bus with the given name
 * @retval (other)            Error returned by the callback
 *
 * @pre manager != nullptr
 * @pre name != nullptr
 * @pre callback != nullptr
 * @post Mock mutex released (manager->mutex.locked == false)
 * @post Callback has been invoked exactly once on success
 *
 * @note Not thread-safe -- mock mutex is a simple boolean flag
 * @warning Callback must not call rx_bus_manager functions (no re-entrancy)
 *
 * @see rx_bus_manager_find_bus() Used internally for bus lookup
 * @see rx_bus_manager_execute_command() Alternative command pattern
 *
 * @since STAR v1.0.0
 */
rx_err_t rx_bus_manager_with_bus(rx_bus_manager_t* manager,
                                 const char*       name,
                                 rx_bus_callback_t callback,
                                 void*             user_ctx)
{
  /* Pre-condition: required pointers must not be null */
  if (manager == nullptr || name == nullptr || callback == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Pre-condition: manager should be initialized */
  assert(manager->mutex.tx_mutex_id == k_tx_mutex_magic);

  /* Find the bus */
  rx_bus_config_t* bus = nullptr;
  rx_err_t         err = rx_bus_manager_find_bus(manager, name, &bus);
  if (err != k_rx_ok) {
    return err;
  }

  /* Mock mutex acquire */
  manager->mutex.locked = true;

  /* Execute callback */
  err = callback(bus, user_ctx);

  /* Mock mutex release */
  manager->mutex.locked = false;

  /* Post-condition: mutex is released */
  assert(manager->mutex.locked == false);
  /* Post-condition: bus pointer was valid */
  assert(bus != nullptr);

  return err;
}

/**
 * @brief Find a bus by name and execute a command on it
 *
 * @details
 * Locates the named bus via rx_bus_manager_find_bus() and invokes the
 * command's execute function pointer. The command result is stored in
 * command->result and also returned to the caller.
 *
 * @param[in,out] manager Bus manager. Must be non-NULL and initialized.
 * @param[in]     name    Name of the bus to operate on. Must be non-NULL.
 * @param[in,out] command Command to execute. Must be non-NULL with a
 *                        non-NULL execute function pointer.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok            Command executed successfully
 * @retval k_rx_err_null_ptr  manager, name, command, or command->execute is nullptr
 * @retval k_rx_err_not_found No bus with the given name
 * @retval (other)            Error returned by command->execute
 *
 * @pre manager != nullptr
 * @pre name != nullptr
 * @pre command != nullptr
 * @pre command->execute != nullptr
 * @post command->result contains the execute function's return value
 * @post Bus state may be modified by the command
 *
 * @note Not thread-safe -- no mutex acquisition in mock
 *
 * @see rx_bus_manager_with_bus() Alternative callback pattern
 * @see rx_bus_command_t Command structure definition
 *
 * @since STAR v1.0.0
 */
rx_err_t rx_bus_manager_execute_command(rx_bus_manager_t* manager,
                                        const char*       name,
                                        rx_bus_command_t* command)
{
  /* Pre-condition: required pointers must not be null */
  if (manager == nullptr || name == nullptr || command == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Pre-condition: manager must be initialized */
  assert(manager->mutex.tx_mutex_id == k_tx_mutex_magic);

  /* Pre-condition: command must have an execute function */
  if (command->execute == nullptr) {
    return k_rx_err_null_ptr;
  }

  /* Find the bus */
  rx_bus_config_t* bus = nullptr;
  rx_err_t         err = rx_bus_manager_find_bus(manager, name, &bus);
  if (err != k_rx_ok) {
    return err;
  }

  /* Execute command */
  command->result = command->execute(bus, command->data);

  /* Post-condition: execute function was called (result now set by execute()) */
  /* Post-condition: bus was valid for command execution */
  assert(bus != nullptr);

  return command->result;
}
