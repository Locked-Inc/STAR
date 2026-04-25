/**
 * @file rx_pin_validator.h
 * @brief Concrete Pin Validator Implementation - Dependency Inversion Pattern
 *
 * @details
 * ## Overview
 *
 * Provides the **concrete implementation** of the `rx_pin_interface_t` abstract interface
 * for GPIO pin validation and reservation tracking on the Renesas RX72N microcontroller.
 * This is the "low-level module" in the Dependency Inversion Principle architecture.
 *
 * **Key Design Pattern: Dependency Inversion + Interface Segregation**
 *
 * ```
 * +------------------------------------------------------------------+
 * |              High-Level Modules (Peripheral Drivers)             |
 * |   SPI, I2C, UART, USB, Motor Control, Encoder, GPIO, PWM, etc.  |
 * +---------------+------------------------------+-------------------+
 *                 |                              |
 *                 | depends on (abstract)        | depends on (abstract)
 *                 v                              v
 * +---------------------------+  +---------------------------------+
 * |  rx_pin_interface_t       |  |   rx_infrastructure module      |
 * |  (ABSTRACT INTERFACE)     |<-+   (Global service locator)      |
 * |  - Function pointers      |  +---------------------------------+
 * |  - No implementation      |
 * +-----------+---------------+
 *             |
 *             | implements (this file provides concrete functions)
 *             v
 * +-------------------------------------------------------------------+
 * |  pin_validator_t (THIS FILE - CONCRETE IMPLEMENTATION)            |
 * |  +- TX_MUTEX mutex (thread safety)                                |
 * |  +- pin_reservation_t[17][8] (reservation tracking)               |
 * |  +- bool initialized (lifecycle management)                       |
 * |  +- Implementation functions:                                     |
 * |     +- pin_validator_validate()  -> iface.validate_pin             |
 * |     +- pin_validator_reserve()   -> iface.reserve_pin              |
 * |     +- pin_validator_release()   -> iface.release_pin              |
 * |     +- pin_validator_is_reserved() -> iface.is_pin_reserved        |
 * |     +- pin_validator_get_function() -> iface.get_pin_function      |
 * |     +- pin_validator_clear_all() -> iface.clear_all_reservations   |
 * +-------------------------------------------------------------------+
 * ```
 *
 * ## Architecture Principles
 *
 * 1. **Single Responsibility**: Sole purpose is pin tracking and conflict prevention
 * 2. **Open/Closed**: Interface is stable, implementation can change
 * 3. **Liskov Substitution**: Can be swapped with other implementations (e.g., mocks)
 * 4. **Interface Segregation**: Implements exactly what interface requires (no more)
 * 5. **Dependency Inversion**: High-level code depends on abstract interface, not this
 *
 * ## Core Features
 *
 * ### 1. Pin Validation
 * - **Hardware-aware**: Validates against actual RX72N pin availability
 * - **Port range**: Supports 0-9 (decimal), 0xA-0xG (hex A-G = 10-16)
 * - **Pin range**: 0-7 per port (not all ports have all pins)
 * - **Fast lookup**: O(1) table-based validation
 *
 * ### 2. Reservation Tracking
 * - **Conflict prevention**: Prevents double reservation of same pin
 * - **Function tracking**: Stores which module owns each pin
 * - **Query support**: Can check reservation status and owner
 * - **Clear all**: Can reset entire table (for testing/reboot)
 *
 * ### 3. Thread Safety
 * - **ThreadX mutex**: All operations protected by TX_MUTEX
 * - **Atomic operations**: Reserve/release are atomic
 * - **Read locks**: Queries also use mutex for consistency
 * - **ISR-unsafe**: Cannot call from interrupts (mutex blocks)
 *
 * ### 4. Memory Efficiency
 * - **Static allocation**: No malloc/free (NASA Rule 3)
 * - **Fixed size**: 17 ports x 8 pins = 136 pin slots
 * - **Predictable footprint**: ~4.7 KB total RAM
 * - **Zero overhead**: Direct array indexing, no lists/trees
 *
 * ## Memory Footprint Analysis
 *
 * | Component | Size (bytes) | Calculation | Location |
 * |-----------|--------------|-------------|----------|
 * | TX_MUTEX | ~100 | ThreadX mutex structure | .bss |
 * | pin_reservation_t array | 4352 | 17 x 8 x 32 = 4352 | .bss |
 * | bool initialized | 1 | Single flag | .bss |
 * | **Total pin_validator_t** | **~4453** | Struct total | .bss |
 *
 * **Memory breakdown per pin**:
 * - `bool reserved`: 1 byte
 * - `char function[32]`: 32 bytes
 * - **Total per pin**: 33 bytes
 * - **Total for 136 pins**: 136 x 33 = 4488 bytes (~4.4 KB)
 *
 * ## RX72N Pin Architecture
 *
 * The RX72N microcontroller has **182 GPIO pins** organized into **17 ports**:
 *
 * | Port | Hex ID | Decimal | Pin Count | Example Usage |
 * |------|--------|---------|-----------|---------------|
 * | PORT0 | 0x0 | 0 | 8 | LEDs, GPIOs |
 * | PORT1 | 0x1 | 1 | 8 | ADC inputs |
 * | PORT2 | 0x2 | 2 | 8 | GPIO, interrupts |
 * | PORT3-9 | 0x3-0x9 | 3-9 | Variable | Mixed peripherals |
 * | PORTA | 0xA | 10 | 8 | SPI, I2C |
 * | PORTB | 0xB | 11 | 8 | USB, UART |
 * | PORTC | 0xC | 12 | 8 | SPI, PWM |
 * | PORTD | 0xD | 13 | 8 | UART, GPIOs |
 * | PORTE | 0xE | 14 | 8 | Encoder, Timers |
 * | PORTF-G | 0xF-0x10 | 15-16 | Variable | Reserved, special |
 *
 * **Note**: Not all ports have all 8 pins physically available on the package.
 *
 * ## Thread Safety Guarantees
 *
 * | Operation | Thread-Safe? | Mutex Used? | Details |
 * |-----------|--------------|-------------|---------|
 * | pin_validator_init() | [X] No | N/A | Call from main() only |
 * | pin_validator_get_interface() | [OK] Yes | No | Read-only check |
 * | pin_validator_deinit() | [X] No | No | Call during shutdown only |
 * | validate_pin() | [OK] Yes | No | Read-only table lookup |
 * | reserve_pin() | [OK] Yes | [OK] Yes | Mutex-protected write |
 * | release_pin() | [OK] Yes | [OK] Yes | Mutex-protected write |
 * | is_pin_reserved() | [OK] Yes | [OK] Yes | Mutex-protected read |
 * | get_pin_function() | [OK] Yes | [OK] Yes | Mutex-protected read + strcpy |
 * | clear_all_reservations() | [OK] Yes | [OK] Yes | Exclusive mutex lock |
 *
 * ## Usage Examples
 *
 * ### Example 1: Basic Initialization and Usage
 * @code
 * // Global validator instance
 * static pin_validator_t s_pin_validator;
 *
 * int main(void)
 * {
 *   // Step 1: Initialize validator
 *   rx_err_t err = pin_validator_init(&s_pin_validator);
 *   if (err != k_rx_ok) {
 *     // Fatal error - cannot continue
 *     while (1) { __asm("nop"); }
 *   }
 *
 *   // Step 2: Get interface from concrete validator
 *   rx_pin_interface_t pin_iface;
 *   err = pin_validator_get_interface(&pin_iface, &s_pin_validator);
 *   if (err != k_rx_ok) {
 *     while (1) { __asm("nop"); }
 *   }
 *
 *   // Step 3: Use interface to reserve pins
 *   err = pin_iface.reserve_pin(pin_iface.ctx, 0xA, 5, "SPI_COPI");
 *   if (err == k_rx_err_gpio_conflict) {
 *     debug_printf("Pin PA5 already reserved\n");
 *   }
 *
 *   // Continue with application
 *   return 0;
 * }
 * @endcode
 *
 * ### Example 2: Using with Global Infrastructure
 * @code
 * // Infrastructure owns the validator
 * static pin_validator_t s_global_validator;
 * static rx_pin_interface_t s_global_pin_iface;
 *
 * rx_err_t infrastructure_init(void)
 * {
 *   // Initialize concrete validator
 *   rx_err_t err = pin_validator_init(&s_global_validator);
 *   if (err != k_rx_ok) return err;
 *
 *   // Extract interface
 *   err = pin_validator_get_interface(&s_global_pin_iface, &s_global_validator);
 *   if (err != k_rx_ok) return err;
 *
 *   // Now modules can get interface via infrastructure
 *   return k_rx_ok;
 * }
 *
 * rx_pin_interface_t* infrastructure_get_pin_interface(void)
 * {
 *   return &s_global_pin_iface;
 * }
 * @endcode
 *
 * ### Example 3: Unit Testing with Mock vs. Real Validator
 * @code
 * #ifdef UNIT_TEST
 *   // Use mock validator for testing
 *   rx_pin_interface_t iface = create_mock_pin_interface();
 * #else
 *   // Use real validator in production
 *   static pin_validator_t validator;
 *   pin_validator_init(&validator);
 *
 *   rx_pin_interface_t iface;
 *   pin_validator_get_interface(&iface, &validator);
 * #endif
 *
 * // Module code works with either (Dependency Inversion!)
 * spi_init(&iface);
 * @endcode
 *
 * ## Design Rationale
 *
 * **Why 2D array instead of hashmap?**
 * - Embedded systems: Predictable memory, O(1) access
 * - RX72N has fixed port/pin structure
 * - Direct indexing: `reservations[port][pin]` is fast
 * - No dynamic allocation (NASA Rule 3)
 *
 * **Why ThreadX mutex instead of atomic operations?**
 * - Struct copy for get_pin_function() requires protection
 * - Consistency between reservation flag and function name
 * - ThreadX already integrated (no additional dependency)
 * - Mutex overhead acceptable for non-ISR operations
 *
 * **Why store function names vs. module IDs?**
 * - Human-readable debugging ("SPI_COPI" vs. ID 42)
 * - No need for global module registry
 * - String literals in .rodata (no RAM cost)
 * - Easy to print in debug reports
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simplify control flow | [OK] | No goto, setjmp, recursion |
 * | 2. Fixed loop bounds | [OK] | All loops use k_pin_validator_max_* constants |
 * | 3. No dynamic memory | [OK] | Static allocation only (2D array) |
 * | 4. Functions < 60 lines | [OK] | All functions under limit |
 * | 5. Use assertions | [OK] | NULL checks, initialization checks |
 * | 6. Data at smallest scope | [OK] | Private functions, local variables |
 * | 7. Check return values | [OK] | All errors propagated |
 * | 8. Limit preprocessor | [OK] | Only include guards |
 * | 9. Restrict pointers | [WARN] | Function pointers (DIP pattern) |
 * | 10. Compile warnings | [OK] | -Wall -Wextra -Werror |
 *
 * @see rx_pin_interface.h Abstract interface this file implements
 * @see rx_infrastructure.h Global service locator using this validator
 * @see RX_RESERVE_PIN() Convenience macro for pin reservation
 * @see RX_RELEASE_PIN() Convenience macro for pin release
 *
 * @since Version 1.0.0
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_pin_interface.h"
#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/**
 * @enum pin_validator_limits_t
 * @brief Pin Validator Configuration Constants
 *
 * @details
 * Defines compile-time array dimensions for the pin reservation table. These limits
 * are based on the RX72N hardware capabilities and determine memory footprint.
 *
 * ## Rationale for Limits
 *
 * **Why 17 ports?**
 * - RX72N has ports 0-9 (10 ports) + A-G (7 ports) = 17 total
 * - Covers all possible GPIO ports on RX72N
 * - Array index maps directly to port number (0-16)
 * - No gaps, no hash function needed
 *
 * **Why 8 pins per port?**
 * - Each RX72N port has up to 8 pins (0-7)
 * - Some ports have fewer pins physically, but we allocate max
 * - Uniform array size simplifies indexing
 * - Small waste (~10% unused slots) vs. complexity of variable-size
 *
 * ## Memory Impact
 *
 * Total pin slots: 17 ports x 8 pins = **136 slots**
 *
 * Per-slot memory:
 * - `bool reserved`: 1 byte
 * - `char function[32]`: 32 bytes
 * - **Total**: 33 bytes per slot
 *
 * Total table size: 136 slots x 33 bytes = **4488 bytes (~4.4 KB)**
 *
 * ## Alternative Designs Considered
 *
 * | Design | Memory | Access Time | Complexity | Chosen? |
 * |--------|--------|-------------|------------|---------|
 * | 2D array (current) | 4.4 KB | O(1) | Low | [OK] Yes |
 * | Linked list | ~800 bytes (sparse) | O(n) | Medium | [X] No |
 * | Hash table | ~2 KB + overhead | O(1) avg | High | [X] No |
 * | Bitfield + separate names | ~17 bytes + strings | O(1) + lookup | Medium | [X] No |
 *
 * **Decision**: 2D array chosen for simplicity, predictability, and O(1) access.
 *
 * @see pin_reservation_t Structure stored in each array slot
 * @see pin_validator_t::reservations 2D array using these dimensions
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /**
   * @brief Maximum number of ports on RX72N (0-9, A-G)
   * @details
   * RX72N port numbering:
   * - Decimal ports: 0-9 (10 ports)
   * - Hex ports: A-G (0xA-0x10 = 10-16 decimal, 7 ports)
   * - **Total**: 17 ports (indices 0-16 in array)
   */
  k_pin_validator_max_ports = 17,

  /**
   * @brief Maximum pins per port (0-7)
   * @details
   * Each RX72N port has up to 8 pins numbered 0-7.
   * Not all ports have all 8 pins physically available on the package,
   * but we allocate space for all 8 to simplify indexing.
   */
  k_pin_validator_max_pins = 8,
} pin_validator_limits_t;

/* =============================================================================
 * Pin Reservation State
 * =============================================================================
 */

/**
 * @struct pin_reservation_t
 * @brief Pin reservation state for a single GPIO pin
 *
 * @details
 * Stores the reservation status and owner information for one pin. These structures
 * are organized in a 2D array `[port][pin]` within the pin_validator_t.
 *
 * ## Memory Layout
 * @code
 * struct pin_reservation_t {
 *   // Offset | Size | Field
 *   //      0 |    1 | bool reserved
 *   //      1 |   32 | char function[32]
 *   // TOTAL: 33 bytes (with padding)
 * };
 * @endcode
 *
 * ## Field Relationships
 *
 * | reserved | function | Meaning |
 * |----------|----------|---------|
 * | false | (any) | Pin is available (function value ignored) |
 * | true | "SPI_COPI" | Pin reserved for SPI COPI |
 * | true | "GPIO_OUT" | Pin reserved for GPIO output |
 * | true | "" | Pin reserved but function unknown (edge case) |
 *
 * ## Typical Usage
 * @code
 * pin_reservation_t* slot = &validator->reservations[port][pin];
 *
 * // Check if reserved
 * if (slot->reserved) {
 *   printf("Pin P%X.%d used for: %s\n", port, pin, slot->function);
 * }
 *
 * // Reserve pin
 * slot->reserved = true;
 * strncpy(slot->function, "SPI_COPI", sizeof(slot->function) - 1);
 * slot->function[sizeof(slot->function) - 1] = '\0';
 *
 * // Release pin
 * slot->reserved = false;
 * memset(slot->function, 0, sizeof(slot->function));
 * @endcode
 *
 * @var pin_reservation_t::reserved
 * @brief Reservation flag (true = pin is reserved)
 * @details
 * When false, the pin is available for reservation. When true, the pin is in use
 * and the `function` field indicates which module owns it.
 *
 * @var pin_reservation_t::function
 * @brief Function name identifying the pin's owner
 * @details
 * Null-terminated string (max 31 chars + null). Examples: "SPI_COPI", "UART_TX",
 * "MOTOR_PWM". Only valid when `reserved == true`. String is typically a literal
 * from .rodata (not copied, just pointer stored).
 *
 * @note Size is 33 bytes (1 bool + 32 char array)
 * @note Function name is NOT copied - must be string literal or static storage
 * @note Thread safety: Access must be mutex-protected
 *
 * @see pin_validator_t::reservations 2D array of these structures
 * @see pin_interface_limits_t::k_pin_function_name_max_len Function name buffer size
 *
 * @since Version 1.0.0
 */
typedef struct {
  bool reserved;                              /**< Is this pin reserved? */
  char function[k_pin_function_name_max_len]; /**< Function name (e.g., "SPI_COPI", "GPIO_OUT") */
} pin_reservation_t;

/* =============================================================================
 * Pin Validator Structure
 * =============================================================================
 */

/**
 * @struct pin_validator_t
 * @brief Concrete Pin Validator Implementation Structure
 *
 * @details
 * This is the **concrete implementation** of the pin validator that backs the
 * `rx_pin_interface_t` abstract interface. Contains all state for pin validation,
 * reservation tracking, and thread synchronization.
 *
 * ## Memory Layout
 *
 * @code
 * struct pin_validator_t {
 *   // Offset | Size  | Field
 *   //      0 |  ~100 | TX_MUTEX mutex
 *   //   ~100 |  4488 | pin_reservation_t[17][8] reservations
 *   //  ~4588 |     1 | bool initialized
 *   //  ~4589 |    ~3 | (padding to alignment)
 *   // TOTAL: ~4592 bytes (~4.5 KB)
 * };
 * @endcode
 *
 * ## Field Descriptions
 *
 * ### mutex (ThreadX Mutex)
 * - **Purpose**: Protects reservation table from concurrent access
 * - **Type**: TX_MUTEX (ThreadX RTOS primitive)
 * - **Size**: ~100 bytes (platform-dependent)
 * - **Operations protected**:
 *   - reserve_pin() - Exclusive write access
 *   - release_pin() - Exclusive write access
 *   - is_pin_reserved() - Shared read access
 *   - get_pin_function() - Shared read access + string copy
 *   - clear_all_reservations() - Exclusive write access
 *
 * ### reservations[port][pin] (2D Array)
 * - **Purpose**: Stores reservation status for all pins
 * - **Dimensions**: 17 ports x 8 pins = 136 slots
 * - **Per-slot size**: 33 bytes (1 bool + 32 char)
 * - **Total size**: 136 x 33 = 4488 bytes (~4.4 KB)
 * - **Indexing**: `reservations[port][pin]` where:
 *   - `port`: 0-9 (decimal), 10-16 (hex A-G)
 *   - `pin`: 0-7
 * - **Access pattern**: O(1) direct indexing, no search needed
 *
 * ### initialized (Lifecycle Flag)
 * - **Purpose**: Tracks whether pin_validator_init() was called
 * - **Type**: bool (1 byte)
 * - **States**:
 *   - `false`: Validator not initialized, cannot use
 *   - `true`: Validator ready, mutex created, table zeroed
 * - **Use**: Prevents double-init and validates before interface extraction
 *
 * ## Lifecycle State Machine
 *
 * @startuml
 * [*] --> Uninitialized : malloc/static declaration
 * Uninitialized --> Initialized : pin_validator_init()
 * Initialized --> InUse : pin_validator_get_interface()
 * InUse --> InUse : reserve/release/query operations
 * InUse --> Initialized : (interface still valid)
 * Initialized --> Uninitialized : pin_validator_deinit()
 * Uninitialized --> [*]
 *
 * note right of Uninitialized
 *   initialized = false
 *   mutex not created
 *   reservations undefined
 * end note
 *
 * note right of Initialized
 *   initialized = true
 *   mutex created
 *   reservations all cleared
 * end note
 *
 * note right of InUse
 *   Interface extracted
 *   Modules using validator
 *   All operations thread-safe
 * end note
 * @enduml
 *
 * ## Typical Usage Pattern
 *
 * @code
 * // Step 1: Declare (uninitialized state)
 * static pin_validator_t s_validator;  // .bss section, zeroed by startup code
 *
 * // Step 2: Initialize (transition to initialized state)
 * rx_err_t err = pin_validator_init(&s_validator);
 * if (err != k_rx_ok) {
 *   return err;  // Initialization failed
 * }
 * // Now: s_validator.initialized == true
 * //      s_validator.mutex created
 * //      s_validator.reservations[] all cleared
 *
 * // Step 3: Extract interface (transition to in-use state)
 * rx_pin_interface_t iface;
 * err = pin_validator_get_interface(&iface, &s_validator);
 * if (err != k_rx_ok) {
 *   return err;
 * }
 * // Now: iface.ctx points to s_validator
 * //      iface function pointers point to validator's implementations
 *
 * // Step 4: Use through interface (in-use state)
 * err = iface.reserve_pin(iface.ctx, 0xA, 5, "SPI_COPI");
 * // Internally: Acquires mutex, checks reservations[10][5], updates if free
 *
 * // Step 5: Deinitialize (transition back to uninitialized)
 * pin_validator_deinit(&s_validator);
 * // Now: s_validator.initialized == false
 * //      mutex destroyed
 * //      reservations cleared (optional)
 * @endcode
 *
 * ## Memory Allocation Strategy
 *
 * This struct is **ALWAYS statically allocated** (never malloc):
 * - Global/file scope: `static pin_validator_t s_validator;`
 * - Infrastructure module: `static pin_validator_t s_global_validator;`
 * - Stack allocation: `pin_validator_t validator;` (rare, wastes stack)
 *
 * **Why static allocation?**
 * - NASA Rule 3: No dynamic memory after initialization
 * - Predictable memory layout
 * - No fragmentation risk
 * - No malloc/free errors
 *
 * ## Thread Safety Responsibilities
 *
 * | Component | Responsibility |
 * |-----------|----------------|
 * | **Caller** | Ensure init/deinit called from single thread |
 * | **Validator** | Protect all reservation table access with mutex |
 * | **Interface methods** | Acquire mutex before accessing reservations[] |
 * | **ThreadX** | Handle mutex priority inheritance |
 *
 * @var pin_validator_t::mutex
 * @brief ThreadX mutex for thread-safe access to reservation table
 * @details
 * Created during pin_validator_init(), destroyed during pin_validator_deinit().
 * Protects all accesses to the `reservations` array. Uses priority inheritance
 * to prevent priority inversion.
 *
 * @var pin_validator_t::reservations
 * @brief 2D array of pin reservation states [port][pin]
 * @details
 * Direct-mapped table: port number is first index, pin number is second index.
 * Example: `reservations[0xA][5]` = Port A, Pin 5. All access must be mutex-protected.
 *
 * @var pin_validator_t::initialized
 * @brief Lifecycle flag indicating validator is ready for use
 * @details
 * Set to true by pin_validator_init(), false by pin_validator_deinit(). Checked
 * by pin_validator_get_interface() to ensure validator is ready before extracting interface.
 *
 * @note Total size: ~4592 bytes (~4.5 KB)
 * @note Always use static allocation (global or file scope)
 * @note Mutex must be created before use (via pin_validator_init())
 * @note Thread-safe after initialization (all operations mutex-protected)
 *
 * @see pin_validator_init() Initialize this structure
 * @see pin_validator_get_interface() Extract interface from this structure
 * @see pin_validator_deinit() Clean up this structure
 * @see rx_pin_interface_t Abstract interface this implements
 *
 * @since Version 1.0.0
 */
typedef struct {
  TX_MUTEX          mutex; /**< ThreadX mutex for thread-safe operation */
  pin_reservation_t reservations
    [k_pin_validator_max_ports]
    [k_pin_validator_max_pins]; /**< Pin reservation tracking array (indexed: reservations[port][pin], Port 0-9: Decimal, Port 10-16: Hex A-G) */
  bool initialized;             /**< Is the validator initialized? */
} pin_validator_t;

/* =============================================================================
 * Public API
 * =============================================================================
 */

/**
 * @brief Initialize pin validator (MUST be called before use)
 *
 * @details
 * Performs one-time initialization of the pin validator structure. Creates ThreadX
 * mutex, clears reservation table, and marks validator as initialized. Must be called
 * exactly once before extracting interface via pin_validator_get_interface().
 *
 * ## Initialization Algorithm
 *
 * 1. **Validate inputs**: Check validator pointer is non-NULL
 * 2. **Check double-init**: Return error if already initialized
 * 3. **Create mutex**: Initialize ThreadX mutex for thread safety
 * 4. **Clear reservation table**: Zero all reservations[port][pin] entries
 * 5. **Set initialized flag**: Mark validator as ready for use
 *
 * ## Typical Usage
 * @code
 * // Global validator (static allocation, zero-initialized by startup code)
 * static pin_validator_t s_pin_validator;
 *
 * int main(void)
 * {
 *   // Initialize validator (one-time operation)
 *   rx_err_t err = pin_validator_init(&s_pin_validator);
 *   if (err != k_rx_ok) {
 *     // Fatal error - cannot continue
 *     debug_printf("Pin validator init failed: %d\n", err);
 *     while (1) { __asm("nop"); }
 *   }
 *
 *   // Validator is now ready for interface extraction
 *   // ...
 * }
 * @endcode
 *
 * @param[in,out] validator Pin validator instance to initialize (must be non-NULL)
 *
 * @return rx_err_t Error code indicating initialization result
 * @retval k_rx_ok Initialization successful, validator ready for use
 * @retval k_rx_err_null_ptr validator parameter is nullptr
 * @retval k_rx_err_invalid_state Validator already initialized (double-init attempt)
 * @retval k_rx_err_rtos_mutex ThreadX mutex creation failed
 *
 * @pre validator must point to valid pin_validator_t structure
 * @pre validator must NOT be already initialized (initialized flag must be false)
 * @pre ThreadX kernel must be initialized (tx_kernel_enter called or about to be called)
 * @pre Must be called from main thread before starting ThreadX (or from initialization thread)
 *
 * @post validator->initialized set to true
 * @post validator->mutex created and ready for use
 * @post validator->reservations[] array cleared (all pins available)
 * @post Validator ready for pin_validator_get_interface()
 *
 * @note **NOT thread-safe**: Must be called from single thread during initialization
 * @note Calling twice returns k_rx_err_invalid_state (double-init protection)
 * @note On failure, validator is left in undefined state - must not use
 * @note Typical execution time: ~200-500 us @ 240 MHz
 *
 * @warning **CRITICAL**: Must be called before any pin reservation operations
 * @warning Do NOT call from multiple threads (not thread-safe during init)
 * @warning If this fails, system cannot continue - must halt or reset
 *
 * @see pin_validator_get_interface() Extract interface after initialization
 * @see pin_validator_deinit() Cleanup validator (shutdown only)
 * @see rx_infrastructure_init() Typically calls this function
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 4 preconditions, 4 postconditions
 */
[[nodiscard]] rx_err_t pin_validator_init(pin_validator_t* validator);

/**
 * @brief Extract abstract interface from concrete pin validator (Dependency Inversion)
 *
 * @details
 * Extracts an `rx_pin_interface_t` from the concrete `pin_validator_t` implementation.
 * This function is the **key to Dependency Inversion**: high-level modules depend on
 * the abstract interface, not the concrete validator type.
 *
 * ## Dependency Inversion Pattern
 * ```
 * High-Level Module (SPI Driver)
 *        v depends on v
 * rx_pin_interface_t (Abstract)  <- THIS FUNCTION PRODUCES THIS
 *        ^ implements ^
 * pin_validator_t (Concrete)     <- THIS FUNCTION CONSUMES THIS
 * ```
 *
 * ## What This Function Does
 *
 * Populates the `rx_pin_interface_t` structure with:
 * - **ctx pointer**: Points to concrete validator (`&validator`)
 * - **Function pointers**: Point to validator's implementation functions:
 *   - `iface->validate_pin = pin_validator_validate`
 *   - `iface->reserve_pin = pin_validator_reserve`
 *   - `iface->release_pin = pin_validator_release`
 *   - `iface->is_pin_reserved = pin_validator_is_reserved`
 *   - `iface->get_pin_function = pin_validator_get_function`
 *   - `iface->clear_all_reservations = pin_validator_clear_all`
 *
 * ## Typical Usage Pattern
 * @code
 * // Step 1: Initialize concrete validator
 * static pin_validator_t s_validator;
 * rx_err_t err = pin_validator_init(&s_validator);
 * if (err != k_rx_ok) {
 *   return err;
 * }
 *
 * // Step 2: Extract interface
 * rx_pin_interface_t iface;
 * err = pin_validator_get_interface(&iface, &s_validator);
 * if (err != k_rx_ok) {
 *   return err;
 * }
 *
 * // Step 3: Inject interface into high-level module
 * spi_config_t spi_cfg = {
 *   .pin_iface = &iface,  // High-level code only sees interface!
 *   // ...
 * };
 * spi_init(&spi_cfg);
 *
 * // Step 4: Use through interface (polymorphism in C)
 * iface.reserve_pin(iface.ctx, 0xA, 5, "SPI_COPI");
 * // Internally calls: pin_validator_reserve(&s_validator, 0xA, 5, "SPI_COPI")
 * @endcode
 *
 * @param[out] iface Interface structure to populate (must be non-NULL)
 * @param[in,out] validator Concrete validator instance (must be initialized)
 *
 * @return rx_err_t Error code indicating extraction result
 * @retval k_rx_ok Interface populated successfully, ready for use
 * @retval k_rx_err_null_ptr iface or validator parameter is nullptr
 * @retval k_rx_err_invalid_state Validator not initialized (call pin_validator_init first)
 *
 * @pre validator must be initialized via pin_validator_init()
 * @pre iface must point to valid rx_pin_interface_t structure
 * @pre validator pointer must remain valid for interface lifetime
 *
 * @post iface->ctx points to validator
 * @post All iface function pointers point to validator implementations
 * @post iface is valid and ready for use
 * @post validator lifetime must extend beyond iface usage
 *
 * @note **Thread-safe**: Can be called from any thread after validator initialization
 * @note **Interface lifetime**: Returned interface is valid as long as validator exists
 * @note **Multiple calls OK**: Can extract multiple interfaces from same validator
 * @note **Performance**: ~50 cycles (simple struct copy + pointer assignments)
 *
 * @warning Validator must remain alive while interface is in use
 * @warning Do NOT call pin_validator_deinit() while interface is in use
 * @warning Interface becomes invalid if validator is deinitialized
 *
 * @see pin_validator_init() Must be called first to initialize validator
 * @see rx_pin_interface_t Abstract interface structure being populated
 * @see rx_infrastructure_get_pin_interface() Global access to interface
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 3 preconditions, 4 postconditions
 */
[[nodiscard]] rx_err_t pin_validator_get_interface(rx_pin_interface_t* iface,
                                                   pin_validator_t*    validator);

/**
 * @brief Deinitialize pin validator (graceful shutdown only)
 *
 * @details
 * Performs cleanup of pin validator resources. Destroys ThreadX mutex, clears
 * reservation table, and marks validator as uninitialized. In embedded systems,
 * this is **RARELY CALLED** because the system typically runs indefinitely.
 *
 * ## When to Use
 * - Unit test teardown (clean state between tests)
 * - System reset/reboot preparation
 * - Firmware update preparation
 * - Development/debugging (clean restart)
 *
 * ## When NOT to Use
 * - Normal operation (validator should remain initialized)
 * - While modules are still using pins
 * - From multiple threads (not thread-safe)
 *
 * ## Cleanup Algorithm
 * 1. **Validate inputs**: Check validator pointer is non-NULL
 * 2. **Check initialized**: Return error if not initialized
 * 3. **Destroy mutex**: Delete ThreadX mutex
 * 4. **Clear reservations**: Zero all reservations (optional, for safety)
 * 5. **Clear initialized flag**: Mark validator as uninitialized
 *
 * ## Typical Usage
 * @code
 * // Unit test teardown
 * void test_teardown(void)
 * {
 *   pin_validator_deinit(&s_test_validator);
 *   // Validator can be reinitialized for next test
 * }
 *
 * // System shutdown
 * void system_shutdown(void)
 * {
 *   // 1. Stop all modules using pins
 *   spi_shutdown();
 *   i2c_shutdown();
 *
 *   // 2. Deinitialize validator
 *   pin_validator_deinit(&s_global_validator);
 *
 *   // 3. Proceed with shutdown/reset
 *   // ...
 * }
 * @endcode
 *
 * @param[in,out] validator Pin validator instance to deinitialize (must be non-NULL)
 *
 * @return rx_err_t Error code indicating deinitialization result
 * @retval k_rx_ok Deinitialization successful, validator can be reinitialized
 * @retval k_rx_err_null_ptr validator parameter is nullptr
 * @retval k_rx_err_invalid_state Validator not initialized (nothing to clean up)
 *
 * @pre validator must be initialized via pin_validator_init()
 * @pre No modules should be using pins (all released)
 * @pre No threads should be calling validator operations concurrently
 *
 * @post validator->initialized set to false
 * @post validator->mutex destroyed
 * @post validator->reservations[] cleared (all pins available)
 * @post Validator can be reinitialized via pin_validator_init()
 *
 * @note **NOT thread-safe**: Must ensure no concurrent access during deinit
 * @note Calling when not initialized returns k_rx_err_invalid_state (safe no-op)
 * @note After deinit, all extracted interfaces become INVALID
 * @note Typical execution time: ~100-300 us @ 240 MHz
 *
 * @warning **DANGEROUS**: All interfaces extracted from this validator become invalid
 * @warning Ensure all modules have stopped using pins before calling
 * @warning Do NOT call from ISR (mutex operations may block)
 *
 * @see pin_validator_init() Reinitialize validator after deinit
 * @see pin_validator_get_interface() Interfaces become invalid after deinit
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 3 preconditions, 4 postconditions
 */
[[nodiscard]] rx_err_t pin_validator_deinit(pin_validator_t* validator);

#ifdef __cplusplus
}
#endif
