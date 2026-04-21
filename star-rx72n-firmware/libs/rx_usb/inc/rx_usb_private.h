/**
 * @file rx_usb_private.h
 * @brief Private types and declarations for USB driver internal testing
 *
 * @details
 * # Overview
 *
 * This header exposes internal types and functions from rx_usb.c for unit testing
 * purposes. In production builds (UNIT_TEST not defined), this header provides
 * only the include guard and standard headers, maintaining encapsulation.
 *
 * ## Design Philosophy
 *
 * The USB driver uses a "private header" pattern to enable thorough unit testing
 * while preserving information hiding in production:
 *
 * @dot
 * digraph encapsulation {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_production {
 *     label="Production Build";
 *     color=green;
 *     rx_usb_h_prod [label="rx_usb.h\nPublic API"];
 *     rx_usb_c_prod [label="rx_usb.c\nImplementation"];
 *     app_prod [label="Application"];
 *
 *     app_prod -> rx_usb_h_prod [label="includes"];
 *     rx_usb_c_prod -> rx_usb_h_prod [label="implements"];
 *   }
 *
 *   subgraph cluster_test {
 *     label="Test Build (UNIT_TEST)";
 *     color=blue;
 *     rx_usb_h_test [label="rx_usb.h\nPublic API"];
 *     rx_usb_private_test [label="rx_usb_private.h\nInternal Types"];
 *     rx_usb_c_test [label="rx_usb.c\nImplementation"];
 *     test_file [label="test_rx_usb.c\nUnit Tests"];
 *
 *     test_file -> rx_usb_h_test [label="includes"];
 *     test_file -> rx_usb_private_test [label="includes"];
 *     rx_usb_c_test -> rx_usb_h_test [label="implements"];
 *     rx_usb_c_test -> rx_usb_private_test [label="exports"];
 *   }
 * }
 * @enddot
 *
 * ## Exposed Components
 *
 * | Component | Type | Purpose |
 * |-----------|------|---------|
 * | ring_buffer_t | struct | Internal FIFO buffer for USB data |
 * | priv_ring_buffer_init() | function | Initialize ring buffer |
 * | priv_ring_buffer_available() | function | Query readable bytes |
 * | priv_ring_buffer_free() | function | Query writable space |
 * | priv_ring_buffer_write() | function | Write to buffer |
 * | priv_ring_buffer_read() | function | Read from buffer |
 * | rx_usb_set_state() | function | Set global USB state |
 * | rx_usb_priv_set_port_state() | function | Set port state |
 * | rx_usb_rx_push() | function | Simulate incoming data |
 *
 * ## Conditional Compilation
 *
 * ```c
 * #ifdef UNIT_TEST
 *   // Internal types and functions exposed
 *   typedef struct { ... } ring_buffer_t;
 *   extern void priv_ring_buffer_init(...);
 * #endif
 * ```
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | Rule 1: Control flow | [PASS] | Declarations only |
 * | Rule 2: Loop bounds | [PASS] | N/A (declarations) |
 * | Rule 3: No heap | [PASS] | Ring buffer uses caller-provided storage |
 * | Rule 4: Function length | [PASS] | Implementations in .c file |
 * | Rule 5: Assertions | [PASS] | Implementations validate parameters |
 * | Rule 6: Data scope | [PASS] | Test-only visibility via UNIT_TEST |
 * | Rule 7: Return checks | [PASS] | Implementations check all returns |
 * | Rule 8: Preprocessor | [PASS] | UNIT_TEST for conditional compilation |
 * | Rule 9: Pointers | [PASS] | Single-level pointers only |
 * | Rule 10: Warnings | [PASS] | Clean compilation |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **S** | Private header has single purpose: test visibility |
 * | **O** | Test functions don't modify production behavior |
 * | **L** | Ring buffer implements standard FIFO semantics |
 * | **I** | Minimal interface: only what tests need |
 * | **D** | Tests depend on abstraction (ring_buffer_t), not raw arrays |
 *
 * ## Module Dependencies
 *
 * | Dependency | Purpose |
 * |------------|---------|
 * | stdint.h | Fixed-width integer types |
 * | rx_usb.h | USB types (rx_usb_state_t, rx_usb_port_id_t) |
 *
 * @see rx_usb.h Public USB driver API
 * @see rx_usb.c Implementation with internal definitions
 * @see test_rx_usb.c Unit tests that use this header
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 *
 * @internal
 * This header is for internal use within the rx_usb module and unit tests.
 * External code should NEVER include this header.
 * @endinternal
 */

#pragma once

#include <stdint.h>

#include "rx_usb.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef UNIT_TEST

/* =============================================================================
 * Internal Types (exposed for testing)
 * =============================================================================
 */

/**
 * @struct ring_buffer_t
 * @brief Circular FIFO buffer for USB data transfer
 *
 * @details
 * Implements a lock-free single-producer single-consumer (SPSC) ring buffer
 * used internally by the USB driver for buffering TX and RX data. This
 * structure is exposed only for unit testing; production code uses the
 * USB driver's public API.
 *
 * ## Ring Buffer Operation
 *
 * @dot
 * digraph ring_buffer {
 *   rankdir=LR;
 *   node [shape=record];
 *
 *   buffer [label="<0>0|<1>1|<2>2|<3>3|<4>4|<5>5|<6>6|<7>7"];
 *
 *   tail [label="tail\n(read)", shape=plaintext];
 *   head [label="head\n(write)", shape=plaintext];
 *   data [label="data in buffer", shape=plaintext, fontcolor=blue];
 *
 *   tail -> buffer:2 [style=dashed];
 *   head -> buffer:5 [style=dashed];
 *   data -> buffer:2 [style=dotted, color=blue];
 *   data -> buffer:3 [style=dotted, color=blue];
 *   data -> buffer:4 [style=dotted, color=blue];
 * }
 * @enddot
 *
 * ```
 * Initial state:    head=0, tail=0, count=0  (empty)
 * After write(3):   head=3, tail=0, count=3  (3 bytes available)
 * After read(2):    head=3, tail=2, count=1  (1 byte available)
 * Wrap-around:      head=1, tail=6, count=3  (wraps at size)
 * ```
 *
 * ## Memory Layout
 *
 * | Offset | Field | Size | Alignment | Description |
 * |--------|-------|------|-----------|-------------|
 * | 0x00 | data | 4 | 4 | Pointer to backing array |
 * | 0x04 | size | 4 | 4 | Total buffer capacity |
 * | 0x08 | head | 4 | 4 | Next write position |
 * | 0x0C | tail | 4 | 4 | Next read position |
 * | 0x10 | count | 4 | 4 | Current byte count |
 *
 * **Total size:** 20 bytes (no padding)
 *
 * ## Field Relationships
 *
 * | Constraint | Invariant |
 * |------------|-----------|
 * | Valid head | 0 <= head < size |
 * | Valid tail | 0 <= tail < size |
 * | Count bounds | 0 <= count <= size |
 * | Consistency | count == (head - tail) mod size |
 * | Data validity | data != nullptr when initialized |
 *
 * @par Usage Example - Basic Operations:
 * @code{.c}
 * // Allocate backing storage
 * uint8_t storage[256];
 * ring_buffer_t buf;
 *
 * // Initialize
 * priv_ring_buffer_init(&buf, storage, sizeof(storage));
 *
 * // Write data
 * const uint8_t tx_data[] = "Hello";
 * uint32_t written = priv_ring_buffer_write(&buf, tx_data, 5);
 * // written == 5, buf.count == 5
 *
 * // Check available
 * uint32_t available = priv_ring_buffer_available(&buf);
 * // available == 5
 *
 * // Read data
 * uint8_t rx_data[10];
 * uint32_t read = priv_ring_buffer_read(&buf, rx_data, 10);
 * // read == 5, rx_data == "Hello", buf.count == 0
 * @endcode
 *
 * @par Usage Example - Wrap-around Handling:
 * @code{.c}
 * uint8_t storage[8];
 * ring_buffer_t buf;
 * priv_ring_buffer_init(&buf, storage, 8);
 *
 * // Fill buffer to position 6
 * uint8_t data1[6] = {1, 2, 3, 4, 5, 6};
 * priv_ring_buffer_write(&buf, data1, 6);  // head=6, tail=0, count=6
 *
 * // Read 4 bytes
 * uint8_t out[4];
 * priv_ring_buffer_read(&buf, out, 4);  // head=6, tail=4, count=2
 *
 * // Write 4 more bytes (wraps around)
 * uint8_t data2[4] = {7, 8, 9, 10};
 * priv_ring_buffer_write(&buf, data2, 4);  // head=2, tail=4, count=6
 *
 * // Read wraps correctly
 * uint8_t out2[6];
 * priv_ring_buffer_read(&buf, out2, 6);  // Returns {5, 6, 7, 8, 9, 10}
 * @endcode
 *
 * @invariant data != nullptr after initialization
 * @invariant head < size
 * @invariant tail < size
 * @invariant count <= size
 *
 * @note Thread-safe for single-producer single-consumer use.
 * @note NOT thread-safe for multiple readers or multiple writers.
 *
 * @warning Do not access fields directly in production code.
 * @warning Caller must ensure backing storage outlives buffer.
 *
 * @see priv_ring_buffer_init() Initialize the buffer
 * @see priv_ring_buffer_write() Write data to buffer
 * @see priv_ring_buffer_read() Read data from buffer
 *
 * @since Version 1.0.0
 */
typedef struct {
  /**
   * @brief Pointer to caller-provided backing storage array
   *
   * @details
   * Points to the external byte array used for storing buffered data.
   * Memory is provided by the caller and must remain valid for the
   * lifetime of the ring buffer.
   *
   * @note Set by priv_ring_buffer_init().
   * @note Must not be nullptr for a valid buffer.
   */
  uint8_t* data;

  /**
   * @brief Total capacity of the buffer in bytes
   *
   * @details
   * Size of the backing storage array pointed to by data.
   * Determines maximum bytes that can be buffered.
   *
   * @par Valid range: 1 to UINT32_MAX
   * @note Set by priv_ring_buffer_init() from caller-provided size.
   */
  uint32_t size;

  /**
   * @brief Write index (next position for write)
   *
   * @details
   * Index into data[] where the next byte will be written.
   * Wraps to 0 when it reaches size.
   *
   * @par Valid range: 0 to (size - 1)
   * @invariant head < size
   */
  uint32_t head;

  /**
   * @brief Read index (next position for read)
   *
   * @details
   * Index into data[] where the next byte will be read from.
   * Wraps to 0 when it reaches size.
   *
   * @par Valid range: 0 to (size - 1)
   * @invariant tail < size
   */
  uint32_t tail;

  /**
   * @brief Number of bytes currently stored in buffer
   *
   * @details
   * Count of valid bytes between tail and head.
   * Incremented by write, decremented by read.
   *
   * @par Valid range: 0 to size
   * @note count == 0 means buffer empty
   * @note count == size means buffer full
   */
  uint32_t count;
} ring_buffer_t;

/* =============================================================================
 * Internal Function Declarations (exposed for testing)
 * =============================================================================
 */

/**
 * @brief Initialize a ring buffer with caller-provided backing storage
 *
 * @details
 * Initializes a ring_buffer_t structure with the provided backing storage.
 * Sets head, tail, and count to zero, establishing an empty buffer ready
 * for use.
 *
 * ## Algorithm Steps
 *
 * 1. Validate parameters (buf, data, size)
 * 2. Store data pointer and size
 * 3. Reset head, tail, count to 0
 * 4. Buffer ready for write operations
 *
 * @param[in,out] buf Pointer to ring buffer structure to initialize.
 *                    Must not be nullptr.
 *                    After return, buf is ready for read/write operations.
 * @param[in] data Pointer to backing data array provided by caller.
 *                 Must not be nullptr.
 *                 Caller retains ownership; must remain valid for buffer lifetime.
 * @param[in] size Size of backing data array in bytes.
 *                 Must be > 0.
 *                 Determines maximum bytes that can be buffered.
 *
 * @return void (initialization always succeeds if preconditions met)
 *
 * @pre buf != nullptr
 * @pre data != nullptr
 * @pre size > 0
 *
 * @post buf->data == data
 * @post buf->size == size
 * @post buf->head == 0
 * @post buf->tail == 0
 * @post buf->count == 0
 *
 * @note If preconditions not met, function returns without modifying buf.
 *
 * @par Thread Safety:
 * Not thread-safe. Caller must ensure no concurrent access during init.
 *
 * @par Example:
 * @code{.c}
 * uint8_t storage[256];
 * ring_buffer_t buf;
 * priv_ring_buffer_init(&buf, storage, sizeof(storage));
 * // Buffer now ready: 256 bytes capacity, 0 bytes used
 * @endcode
 *
 * @see ring_buffer_t Buffer structure definition
 * @see priv_ring_buffer_write() Write to initialized buffer
 *
 * @since Version 1.0.0
 * @internal Test-only function exposed for unit testing
 */
extern void priv_ring_buffer_init(ring_buffer_t* buf, uint8_t* data, uint32_t size);

/**
 * @brief Query number of bytes available to read from ring buffer
 *
 * @details
 * Returns the count of valid bytes currently stored in the buffer.
 * This is the maximum number of bytes that can be read without blocking.
 *
 * @param[in] buf Pointer to ring buffer structure to query.
 *                Must be initialized via priv_ring_buffer_init().
 *                NULL is handled safely (returns 0).
 *
 * @return uint32_t Number of bytes available to read
 * @retval 0 Buffer is empty, or buf is nullptr
 * @retval >0 Number of readable bytes (up to buf->size)
 *
 * @pre buf should be initialized (NULL returns 0)
 *
 * @post Buffer state unchanged (read-only operation)
 *
 * @note O(1) operation, does not modify buffer.
 *
 * @par Thread Safety:
 * Safe for single-producer single-consumer model. Returns consistent
 * snapshot even during concurrent write operations.
 *
 * @par Example:
 * @code{.c}
 * uint32_t available = priv_ring_buffer_available(&buf);
 * if (available >= sizeof(expected_packet)) {
 *     priv_ring_buffer_read(&buf, packet, sizeof(expected_packet));
 * }
 * @endcode
 *
 * @see priv_ring_buffer_free() Query writable space
 * @see priv_ring_buffer_read() Read available data
 *
 * @since Version 1.0.0
 * @internal Test-only function exposed for unit testing
 */
extern uint32_t priv_ring_buffer_available(const ring_buffer_t* buf);

/**
 * @brief Query number of free bytes available for writing
 *
 * @details
 * Returns the count of free bytes in the buffer. This is the maximum
 * number of bytes that can be written without overwriting existing data.
 *
 * @param[in] buf Pointer to ring buffer structure to query.
 *                Must be initialized via priv_ring_buffer_init().
 *                NULL is handled safely (returns 0).
 *
 * @return uint32_t Number of free bytes for writing
 * @retval 0 Buffer is full, or buf is nullptr
 * @retval >0 Number of writable bytes (up to buf->size)
 *
 * @pre buf should be initialized (NULL returns 0)
 *
 * @post Buffer state unchanged (read-only operation)
 *
 * @note O(1) operation: returns (size - count)
 *
 * @par Thread Safety:
 * Safe for single-producer single-consumer model.
 *
 * @par Example:
 * @code{.c}
 * uint32_t free_space = priv_ring_buffer_free(&buf);
 * if (free_space >= data_len) {
 *     priv_ring_buffer_write(&buf, data, data_len);
 * } else {
 *     // Handle buffer full condition
 * }
 * @endcode
 *
 * @see priv_ring_buffer_available() Query readable bytes
 * @see priv_ring_buffer_write() Write data to buffer
 *
 * @since Version 1.0.0
 * @internal Test-only function exposed for unit testing
 */
extern uint32_t priv_ring_buffer_free(const ring_buffer_t* buf);

/**
 * @brief Write data to ring buffer with wrap-around handling
 *
 * @details
 * Copies data from the source array into the ring buffer, handling
 * wrap-around at the buffer boundary. If insufficient space exists,
 * writes as many bytes as possible and returns the actual count.
 *
 * ## Algorithm Steps
 *
 * 1. Validate parameters (buf, data)
 * 2. Calculate available space: min(len, size - count)
 * 3. Calculate bytes before wrap: min(to_write, size - head)
 * 4. Copy first chunk (head to end of buffer)
 * 5. Copy second chunk if needed (start of buffer)
 * 6. Update head index with modulo wrap
 * 7. Update count
 * 8. Return bytes written
 *
 * @param[in,out] buf Pointer to ring buffer structure.
 *                    Must be initialized.
 *                    Modified: head and count updated.
 * @param[in] data Pointer to source data array to copy from.
 *                 Must not be nullptr.
 *                 Caller retains ownership.
 * @param[in] len Number of bytes to write.
 *                May write fewer if buffer becomes full.
 *
 * @return uint32_t Number of bytes actually written
 * @retval 0 No bytes written (buf or data NULL, or buffer full)
 * @retval <len Buffer became full, partial write
 * @retval len All requested bytes written successfully
 *
 * @pre buf initialized via priv_ring_buffer_init()
 * @pre data != nullptr
 *
 * @post buf->count increased by return value
 * @post buf->head advanced by return value (with wrap)
 * @post buf->tail unchanged
 *
 * @note Non-blocking: returns immediately with bytes written.
 * @note Partial writes: may write fewer bytes than requested.
 *
 * @par Thread Safety:
 * Safe for single writer. Do not call from multiple threads concurrently.
 *
 * @par Example:
 * @code{.c}
 * const uint8_t message[] = "Hello, USB!";
 * uint32_t written = priv_ring_buffer_write(&buf, message, sizeof(message));
 * if (written < sizeof(message)) {
 *     // Handle partial write - buffer was full
 *     rx_log_warn("USB", "Buffer full, %u bytes dropped", sizeof(message) - written);
 * }
 * @endcode
 *
 * @see priv_ring_buffer_free() Check space before writing
 * @see priv_ring_buffer_read() Read data back out
 *
 * @since Version 1.0.0
 * @internal Test-only function exposed for unit testing
 */
extern uint32_t priv_ring_buffer_write(ring_buffer_t* buf, const uint8_t* data, uint32_t len);

/**
 * @brief Read data from ring buffer with wrap-around handling
 *
 * @details
 * Copies data from the ring buffer into the destination array, handling
 * wrap-around at the buffer boundary. If insufficient data exists,
 * reads as many bytes as available and returns the actual count.
 *
 * ## Algorithm Steps
 *
 * 1. Validate parameters (buf, data)
 * 2. Calculate available data: min(max_len, count)
 * 3. Calculate bytes before wrap: min(to_read, size - tail)
 * 4. Copy first chunk (tail to end of buffer)
 * 5. Copy second chunk if needed (start of buffer)
 * 6. Update tail index with modulo wrap
 * 7. Update count
 * 8. Return bytes read
 *
 * @param[in,out] buf Pointer to ring buffer structure.
 *                    Must be initialized.
 *                    Modified: tail and count updated.
 * @param[out] data Pointer to destination buffer for read data.
 *                  Must not be nullptr.
 *                  Must have space for at least max_len bytes.
 * @param[in] max_len Maximum number of bytes to read.
 *                    May read fewer if buffer has less data.
 *
 * @return uint32_t Number of bytes actually read
 * @retval 0 No bytes read (buf or data NULL, or buffer empty)
 * @retval <max_len Buffer had less data, partial read
 * @retval max_len All requested bytes read successfully
 *
 * @pre buf initialized via priv_ring_buffer_init()
 * @pre data != nullptr
 * @pre data has capacity for max_len bytes
 *
 * @post buf->count decreased by return value
 * @post buf->tail advanced by return value (with wrap)
 * @post buf->head unchanged
 *
 * @note Non-blocking: returns immediately with bytes read.
 * @note Data is removed from buffer (consuming read).
 *
 * @par Thread Safety:
 * Safe for single reader. Do not call from multiple threads concurrently.
 *
 * @par Example:
 * @code{.c}
 * uint8_t packet[64];
 * uint32_t read = priv_ring_buffer_read(&buf, packet, sizeof(packet));
 * if (read > 0) {
 *     process_packet(packet, read);
 * }
 * @endcode
 *
 * @see priv_ring_buffer_available() Check data before reading
 * @see priv_ring_buffer_write() Write data to buffer
 *
 * @since Version 1.0.0
 * @internal Test-only function exposed for unit testing
 */
extern uint32_t priv_ring_buffer_read(ring_buffer_t* buf, uint8_t* data, uint32_t max_len);

/**
 * @brief Set the global USB device state (test helper)
 *
 * @details
 * Forces the USB driver's global state to a specific value for testing
 * purposes. This allows unit tests to simulate various USB states without
 * going through the full USB enumeration process.
 *
 * ## State Machine
 *
 * @startuml
 * [*] --> DISCONNECTED
 * DISCONNECTED --> CONNECTED : Cable attached
 * CONNECTED --> CONFIGURED : Enumeration complete
 * CONFIGURED --> CONNECTED : Deconfigured
 * CONNECTED --> DISCONNECTED : Cable removed
 * CONFIGURED --> DISCONNECTED : Cable removed
 * CONFIGURED --> ERROR : Fault detected
 * ERROR --> DISCONNECTED : Reset
 * @enduml
 *
 * @param[in] state New USB device state to set.
 *                  Must be a valid rx_usb_state_t value.
 *                  Invalid values are ignored (no state change).
 *
 * @return void
 *
 * @pre USB driver must be initialized
 *
 * @post Global USB state set to 'state' if valid
 * @post No change if state is invalid
 *
 * @note **Test only:** Do not use in production code.
 * @note State validation prevents invalid states from being set.
 *
 * @par Thread Safety:
 * Not thread-safe. Use only in single-threaded test context.
 *
 * @par Example:
 * @code{.c}
 * // Test USB read behavior when configured
 * rx_usb_set_state(k_rx_usb_state_configured);
 * rx_usb_rx_push(k_rx_usb_port_protocol, test_data, sizeof(test_data));
 *
 * uint8_t buf[64];
 * uint32_t read = rx_usb_read(k_rx_usb_port_protocol, buf, sizeof(buf));
 * TEST_ASSERT_EQUAL(sizeof(test_data), read);
 *
 * // Test behavior when disconnected
 * rx_usb_set_state(k_rx_usb_state_disconnected);
 * read = rx_usb_read(k_rx_usb_port_protocol, buf, sizeof(buf));
 * TEST_ASSERT_EQUAL(0, read);  // Should fail when disconnected
 * @endcode
 *
 * @see rx_usb_get_state() Query current state
 * @see rx_usb_state_t State enumeration
 *
 * @since Version 1.0.0
 * @internal Test-only function exposed for unit testing
 */
extern void rx_usb_set_state(rx_usb_state_t state);

/**
 * @brief Set a specific USB port's state (test helper)
 *
 * @details
 * Forces a specific USB CDC port's state to a value for testing purposes.
 * Allows testing port-specific behavior independent of global USB state.
 *
 * @param[in] port USB port identifier to modify.
 *                 Must be a valid rx_usb_port_id_t value.
 *                 Invalid ports are ignored (no state change).
 * @param[in] state New state for the specified port.
 *                  Must be a valid rx_usb_state_t value.
 *                  Invalid states are ignored (no state change).
 *
 * @return void
 *
 * @pre USB driver must be initialized
 *
 * @post Port state set to 'state' if both port and state valid
 * @post No change if either port or state is invalid
 *
 * @note **Test only:** Do not use in production code.
 *
 * @par Thread Safety:
 * Not thread-safe. Use only in single-threaded test context.
 *
 * @par Example:
 * @code{.c}
 * // Test multi-port scenarios
 * rx_usb_priv_set_port_state(k_rx_usb_port_protocol, k_rx_usb_state_configured);
 * rx_usb_priv_set_port_state(k_rx_usb_port_log, k_rx_usb_state_connected);
 *
 * // Protocol port should work
 * TEST_ASSERT_EQUAL(k_rx_ok, rx_usb_write(k_rx_usb_port_protocol, data, len));
 *
 * // Log port should fail (not configured)
 * TEST_ASSERT_EQUAL(k_rx_err_invalid_state,
 *                   rx_usb_write(k_rx_usb_port_log, data, len));
 * @endcode
 *
 * @see rx_usb_set_state() Set global state
 * @see rx_usb_port_id_t Port enumeration
 *
 * @since Version 1.0.0
 * @internal Test-only function exposed for unit testing
 */
extern void rx_usb_priv_set_port_state(rx_usb_port_id_t port, rx_usb_state_t state);

/**
 * @brief Inject data into USB port RX buffer (test helper)
 *
 * @details
 * Pushes data directly into a USB port's receive buffer, simulating
 * incoming USB data from the host. Used by unit tests to inject test
 * data without actual USB hardware.
 *
 * ## Test Simulation Flow
 *
 * @msc
 * msc {
 *   width=500;
 *   Test, RxPush, RxBuffer, UsbRead;
 *
 *   Test => RxPush [label="rx_usb_rx_push(port, data, len)"];
 *   RxPush => RxBuffer [label="write to port RX buffer"];
 *   RxBuffer >> RxPush [label="bytes written"];
 *   RxPush >> Test [label="return count"];
 *
 *   --- [label="Application reads"];
 *
 *   Test => UsbRead [label="rx_usb_read(port, buf, max)"];
 *   UsbRead => RxBuffer [label="read from buffer"];
 *   RxBuffer >> UsbRead [label="return data"];
 *   UsbRead >> Test [label="return count"];
 * }
 * @endmsc
 *
 * @param[in] port USB port identifier to push data to.
 *                 Must be a valid rx_usb_port_id_t value.
 * @param[in] data Pointer to data to inject into RX buffer.
 *                 Must not be nullptr.
 * @param[in] len Number of bytes to push.
 *                May push fewer if buffer becomes full.
 *
 * @return uint32_t Number of bytes actually pushed
 * @retval 0 No bytes pushed (invalid port, NULL data, or buffer full)
 * @retval <len Buffer became full, partial push
 * @retval len All requested bytes pushed successfully
 *
 * @pre USB driver must be initialized
 * @pre Port must be valid
 * @pre data != nullptr
 *
 * @post Port's RX buffer contains pushed data
 * @post Data available via rx_usb_read()
 *
 * @note **Test only:** In production, RX buffer is filled by USB ISR.
 *
 * @par Thread Safety:
 * Not thread-safe. Use only in single-threaded test context.
 *
 * @par Example:
 * @code{.c}
 * // Simulate receiving a command packet
 * const uint8_t cmd_packet[] = {0x01, 0x02, 0x03, 0x04};
 * rx_usb_set_state(k_rx_usb_state_configured);
 *
 * uint32_t pushed = rx_usb_rx_push(k_rx_usb_port_protocol,
 *                                   cmd_packet, sizeof(cmd_packet));
 * TEST_ASSERT_EQUAL(sizeof(cmd_packet), pushed);
 *
 * // Verify application can read the data
 * uint8_t recv_buf[64];
 * uint32_t received = rx_usb_read(k_rx_usb_port_protocol,
 *                                  recv_buf, sizeof(recv_buf));
 * TEST_ASSERT_EQUAL(sizeof(cmd_packet), received);
 * TEST_ASSERT_EQUAL_UINT8_ARRAY(cmd_packet, recv_buf, sizeof(cmd_packet));
 * @endcode
 *
 * @see rx_usb_read() Read data pushed by this function
 * @see rx_usb_set_state() Set state before pushing data
 *
 * @since Version 1.0.0
 * @internal Test-only function exposed for unit testing
 */
extern uint32_t rx_usb_rx_push(rx_usb_port_id_t port, const uint8_t* data, uint32_t len);

#endif /* UNIT_TEST */

#ifdef __cplusplus
}
#endif
