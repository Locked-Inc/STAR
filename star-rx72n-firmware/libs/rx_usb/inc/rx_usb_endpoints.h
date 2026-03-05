/**
 * @file rx_usb_endpoints.h
 * @brief USB CDC Endpoint Address Definitions for RX72N Composite Device
 *
 * @details
 * # Overview
 *
 * This header defines USB endpoint addresses for the STAR motor controller's
 * USB CDC composite device implementation. The RX72N exposes three virtual
 * serial ports over USB, each requiring three endpoints (bulk IN, bulk OUT,
 * and interrupt IN).
 *
 * ## USB CDC Composite Architecture
 *
 * @dot
 * digraph usb_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_host {
 *     label="Host PC";
 *     color=blue;
 *     com0 [label="/dev/ttyACM0\nProtocol Port"];
 *     com1 [label="/dev/ttyACM1\nDecoded Port"];
 *     com2 [label="/dev/ttyACM2\nLog Port"];
 *   }
 *
 *   subgraph cluster_usb {
 *     label="USB Bus";
 *     color=green;
 *     ep0 [label="EP0\nControl"];
 *     bulk0 [label="EP1/EP2\nBulk IN/OUT"];
 *     int0 [label="EP3\nInterrupt IN"];
 *     bulk1 [label="EP4/EP5\nBulk IN/OUT"];
 *     int1 [label="EP6\nInterrupt IN"];
 *     bulk2 [label="EP7/EP8\nBulk IN/OUT"];
 *     int2 [label="EP9\nInterrupt IN"];
 *   }
 *
 *   subgraph cluster_rx72n {
 *     label="RX72N MCU";
 *     color=orange;
 *     port0 [label="CDC Port 0\nProtocol (nanopb)"];
 *     port1 [label="CDC Port 1\nDecoded ASCII"];
 *     port2 [label="CDC Port 2\nLog Output"];
 *   }
 *
 *   com0 -> bulk0 -> port0;
 *   com0 -> int0 -> port0;
 *   com1 -> bulk1 -> port1;
 *   com1 -> int1 -> port1;
 *   com2 -> bulk2 -> port2;
 *   com2 -> int2 -> port2;
 * }
 * @enddot
 *
 * ## Endpoint Addressing
 *
 * USB endpoint addresses use a specific format defined by the USB specification:
 *
 * ```
 * Address byte format: 0bDNNN_NNNN
 *   D (bit 7): Direction (0=OUT from host, 1=IN to host)
 *   N (bits 0-6): Endpoint number (1-15, 0 reserved for control)
 * ```
 *
 * | Address | Direction | Endpoint | Purpose |
 * |---------|-----------|----------|---------|
 * | 0x81 | IN | EP1 | Port 0 bulk data to host |
 * | 0x02 | OUT | EP2 | Port 0 bulk data from host |
 * | 0x83 | IN | EP3 | Port 0 interrupt notifications |
 * | 0x84 | IN | EP4 | Port 1 bulk data to host |
 * | 0x05 | OUT | EP5 | Port 1 bulk data from host |
 * | 0x86 | IN | EP6 | Port 1 interrupt notifications |
 * | 0x87 | IN | EP7 | Port 2 bulk data to host |
 * | 0x08 | OUT | EP8 | Port 2 bulk data from host |
 * | 0x89 | IN | EP9 | Port 2 interrupt notifications |
 *
 * ## Port Assignments
 *
 * | Port | Linux Device | Windows Device | Purpose | Baud Rate |
 * |------|--------------|----------------|---------|-----------|
 * | 0 | /dev/ttyACM0 | COMx | Protocol (nanopb) | Virtual (any) |
 * | 1 | /dev/ttyACM1 | COMy | Decoded ASCII | Virtual (any) |
 * | 2 | /dev/ttyACM2 | COMz | Log output | Virtual (any) |
 *
 * ## RX72N USB Hardware
 *
 * The RX72N USB 2.0 peripheral supports:
 * - Full-speed (12 Mbps) operation
 * - 10 configurable endpoints (EP0-EP9)
 * - Up to 8 KB shared FIFO memory
 * - DMA transfer support
 *
 * | Resource | Available | Used | Notes |
 * |----------|-----------|------|-------|
 * | Endpoints | 10 | 10 | EP0 + 9 CDC endpoints |
 * | FIFO | 8 KB | ~3 KB | 256B per bulk, 64B per interrupt |
 * | Max packet | 512B | 64B | Full-speed limit |
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | Rule 1: Control flow | [PASS] | Constants only, no code |
 * | Rule 2: Loop bounds | [PASS] | No loops |
 * | Rule 3: No heap | [PASS] | Compile-time constants |
 * | Rule 4: Function length | [PASS] | No functions |
 * | Rule 5: Assertions | [PASS] | N/A (constants) |
 * | Rule 6: Data scope | [PASS] | Header-only typed enum |
 * | Rule 7: Return checks | [PASS] | N/A (constants) |
 * | Rule 8: Preprocessor | [PASS] | C23 typed enum only |
 * | Rule 9: Pointers | [PASS] | No pointers |
 * | Rule 10: Warnings | [PASS] | Clean compilation |
 *
 * ## Module Dependencies
 *
 * @dot
 * digraph dependencies {
 *   rankdir=LR;
 *   node [shape=box];
 *
 *   endpoints [label="rx_usb_endpoints.h", fillcolor=lightgreen, style=filled];
 *   stdint [label="<stdint.h>"];
 *   usb_cdc [label="rx_usb_cdc.c"];
 *   usb_hw [label="rx_usb_hw.c"];
 *   usb_desc [label="rx_usb_descriptors.c"];
 *
 *   endpoints -> stdint [label="uint8_t"];
 *   usb_cdc -> endpoints [label="includes"];
 *   usb_hw -> endpoints [label="includes"];
 *   usb_desc -> endpoints [label="includes"];
 * }
 * @enddot
 *
 * @see rx_usb.h Main USB module API
 * @see rx_usb_cdc.c CDC class implementation
 * @see rx_usb_hw.c Hardware abstraction layer
 * @see rx72n_usb_regs.h RX72N USB register definitions
 *
 * @par Related Documentation:
 * - USB 2.0 Specification, Chapter 9 (USB Device Framework)
 * - USB CDC Specification 1.2 (Communications Device Class)
 * - RX72N User's Manual: Hardware, Chapter 40 (USB 2.0)
 *
 * @author Locked, Inc.
 * @date 2026-01-27
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 *
 * @internal
 * This header is for internal use within the rx_usb module.
 * External code should use rx_usb.h for USB operations.
 * @endinternal
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum usb_endpoint_addresses_t
 * @brief USB CDC endpoint addresses for three-port composite device
 *
 * @details
 * Defines the USB endpoint addresses for all nine endpoints used by the
 * STAR motor controller's USB CDC composite device. Each of the three
 * virtual serial ports requires three endpoints:
 *
 * - **Bulk IN:** Device-to-host data transfer (0x8N format)
 * - **Bulk OUT:** Host-to-device data transfer (0x0N format)
 * - **Interrupt IN:** Asynchronous notifications (0x8N format)
 *
 * ## Endpoint Address Format
 *
 * ```
 * Bit 7: Direction (1=IN, 0=OUT)
 * Bits 6-4: Reserved (must be 0)
 * Bits 3-0: Endpoint number (1-15)
 *
 * Examples:
 *   0x81 = 1000_0001 = IN,  endpoint 1
 *   0x02 = 0000_0010 = OUT, endpoint 2
 *   0x83 = 1000_0011 = IN,  endpoint 3
 * ```
 *
 * ## Endpoint Configuration Table
 *
 * | Constant | Address | Dir | EP# | Type | Max Packet | Port | Function |
 * |----------|---------|-----|-----|------|------------|------|----------|
 * | k_port0_ep_bulk_in | 0x81 | IN | 1 | Bulk | 64 | 0 | TX data |
 * | k_port0_ep_bulk_out | 0x02 | OUT | 2 | Bulk | 64 | 0 | RX data |
 * | k_port0_ep_int_in | 0x83 | IN | 3 | Intr | 8 | 0 | Notify |
 * | k_port1_ep_bulk_in | 0x84 | IN | 4 | Bulk | 64 | 1 | TX data |
 * | k_port1_ep_bulk_out | 0x05 | OUT | 5 | Bulk | 64 | 1 | RX data |
 * | k_port1_ep_int_in | 0x86 | IN | 6 | Intr | 8 | 1 | Notify |
 * | k_port2_ep_bulk_in | 0x87 | IN | 7 | Bulk | 64 | 2 | TX data |
 * | k_port2_ep_bulk_out | 0x08 | OUT | 8 | Bulk | 64 | 2 | RX data |
 * | k_port2_ep_int_in | 0x89 | IN | 9 | Intr | 8 | 2 | Notify |
 *
 * ## Memory Footprint
 *
 * As a C23 typed enum with uint8_t underlying type, this enum uses:
 * - **1 byte per value** when stored
 * - **0 bytes static allocation** (compile-time constants)
 *
 * @par Usage Example - Endpoint Initialization:
 * @code{.c}
 * void usb_configure_endpoints(void)
 * {
 *     // Configure Port 0 endpoints
 *     usb_endpoint_configure(k_port0_ep_bulk_in, USB_EP_BULK, 64);
 *     usb_endpoint_configure(k_port0_ep_bulk_out, USB_EP_BULK, 64);
 *     usb_endpoint_configure(k_port0_ep_int_in, USB_EP_INTERRUPT, 8);
 *
 *     // Configure Port 1 endpoints
 *     usb_endpoint_configure(k_port1_ep_bulk_in, USB_EP_BULK, 64);
 *     usb_endpoint_configure(k_port1_ep_bulk_out, USB_EP_BULK, 64);
 *     usb_endpoint_configure(k_port1_ep_int_in, USB_EP_INTERRUPT, 8);
 *
 *     // Configure Port 2 endpoints
 *     usb_endpoint_configure(k_port2_ep_bulk_in, USB_EP_BULK, 64);
 *     usb_endpoint_configure(k_port2_ep_bulk_out, USB_EP_BULK, 64);
 *     usb_endpoint_configure(k_port2_ep_int_in, USB_EP_INTERRUPT, 8);
 * }
 * @endcode
 *
 * @par Usage Example - Data Transfer:
 * @code{.c}
 * rx_err_t usb_send_to_port(uint8_t port, const uint8_t* data, uint32_t len)
 * {
 *     usb_endpoint_addresses_t ep;
 *
 *     switch (port) {
 *         case 0: ep = k_port0_ep_bulk_in; break;
 *         case 1: ep = k_port1_ep_bulk_in; break;
 *         case 2: ep = k_port2_ep_bulk_in; break;
 *         default: return k_rx_err_invalid_arg;
 *     }
 *
 *     return usb_endpoint_write(ep, data, len);
 * }
 * @endcode
 *
 * @par Usage Example - Endpoint Number Extraction:
 * @code{.c}
 * // Extract endpoint number from address
 * uint8_t get_endpoint_number(usb_endpoint_addresses_t addr)
 * {
 *     return addr & 0x0F;  // Mask out direction bit
 * }
 *
 * // Check if endpoint is IN direction
 * bool is_endpoint_in(usb_endpoint_addresses_t addr)
 * {
 *     return (addr & 0x80) != 0;  // Check bit 7
 * }
 *
 * // Example usage
 * uint8_t ep_num = get_endpoint_number(k_port0_ep_bulk_in);  // Returns 1
 * bool is_in = is_endpoint_in(k_port0_ep_bulk_in);           // Returns true
 * @endcode
 *
 * @note Endpoint 0 (0x00/0x80) is reserved for USB control transfers.
 * @note USB Full-Speed limits bulk endpoints to 64 bytes max packet size.
 * @note Interrupt endpoints configured with 8-byte max packet (CDC ACM standard).
 *
 * @warning Do not modify endpoint addresses without updating USB descriptors.
 * @warning Changing addresses requires corresponding changes in rx_usb_descriptors.c.
 *
 * @see rx_usb_cdc.c Uses these addresses for CDC data transfer
 * @see rx_usb_descriptors.c Defines matching endpoint descriptors
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /* =========================================================================
   * Port 0 (Protocol) Endpoints
   *
   * Primary communication port for Protocol Buffer (nanopb) messages.
   * Used for high-level command/response communication with host.
   * ========================================================================= */

  /**
   * @brief Port 0 bulk IN endpoint for data transmission to host
   *
   * @details
   * Bulk transfer endpoint for sending protocol buffer encoded messages
   * from the RX72N to the host PC. Used for telemetry, responses, and
   * asynchronous notifications.
   *
   * @par Value: 0x81
   * @par Encoding: Direction=IN (bit 7=1), Endpoint=1 (bits 3-0)
   * @par Transfer type: Bulk
   * @par Max packet size: 64 bytes (Full-Speed USB limit)
   * @par FIFO allocation: 256 bytes (double-buffered)
   *
   * @par Typical data:
   * - Motor telemetry (position, velocity, current)
   * - Command acknowledgments
   * - Error notifications
   *
   * @see k_port0_ep_bulk_out Corresponding OUT endpoint
   */
  k_port0_ep_bulk_in = 0x81,

  /**
   * @brief Port 0 bulk OUT endpoint for data reception from host
   *
   * @details
   * Bulk transfer endpoint for receiving protocol buffer encoded commands
   * from the host PC to the RX72N. Primary channel for motor commands
   * and configuration updates.
   *
   * @par Value: 0x02
   * @par Encoding: Direction=OUT (bit 7=0), Endpoint=2 (bits 3-0)
   * @par Transfer type: Bulk
   * @par Max packet size: 64 bytes (Full-Speed USB limit)
   * @par FIFO allocation: 256 bytes (double-buffered)
   *
   * @par Typical data:
   * - Motor velocity commands
   * - PID gain updates
   * - Configuration requests
   *
   * @see k_port0_ep_bulk_in Corresponding IN endpoint
   */
  k_port0_ep_bulk_out = 0x02,

  /**
   * @brief Port 0 interrupt IN endpoint for CDC notifications
   *
   * @details
   * Interrupt transfer endpoint for CDC ACM serial state notifications.
   * Used to signal line state changes (DTR, RTS) and serial errors
   * to the host operating system.
   *
   * @par Value: 0x83
   * @par Encoding: Direction=IN (bit 7=1), Endpoint=3 (bits 3-0)
   * @par Transfer type: Interrupt
   * @par Max packet size: 8 bytes (CDC ACM notification size)
   * @par Polling interval: 10 ms
   *
   * @par Notifications sent:
   * - SERIAL_STATE: Line state changes
   * - NETWORK_CONNECTION: Connection status
   *
   * @see USB CDC PSTN Subclass Specification for notification format
   */
  k_port0_ep_int_in = 0x83,

  /* =========================================================================
   * Port 1 (Decoded) Endpoints
   *
   * Human-readable ASCII output of decoded protocol messages.
   * Useful for debugging and monitoring communication.
   * ========================================================================= */

  /**
   * @brief Port 1 bulk IN endpoint for decoded ASCII output
   *
   * @details
   * Bulk transfer endpoint for sending human-readable decoded versions
   * of protocol buffer messages. Provides real-time visibility into
   * communication for debugging purposes.
   *
   * @par Value: 0x84
   * @par Encoding: Direction=IN (bit 7=1), Endpoint=4 (bits 3-0)
   * @par Transfer type: Bulk
   * @par Max packet size: 64 bytes
   * @par FIFO allocation: 256 bytes (double-buffered)
   *
   * @par Output format:
   * - "[RX] MotorCommand: velocity=1.5 m/s, motor_id=0"
   * - "[TX] Telemetry: pos=125.3 rad, vel=1.48 m/s"
   */
  k_port1_ep_bulk_in = 0x84,

  /**
   * @brief Port 1 bulk OUT endpoint for decoded port input
   *
   * @details
   * Bulk transfer endpoint for receiving data on the decoded port.
   * Generally unused as decoded port is output-only, but provided
   * for completeness and potential future debugging commands.
   *
   * @par Value: 0x05
   * @par Encoding: Direction=OUT (bit 7=0), Endpoint=5 (bits 3-0)
   * @par Transfer type: Bulk
   * @par Max packet size: 64 bytes
   * @par FIFO allocation: 256 bytes (double-buffered)
   */
  k_port1_ep_bulk_out = 0x05,

  /**
   * @brief Port 1 interrupt IN endpoint for CDC notifications
   *
   * @details
   * Interrupt transfer endpoint for CDC ACM serial state notifications
   * on the decoded ASCII port.
   *
   * @par Value: 0x86
   * @par Encoding: Direction=IN (bit 7=1), Endpoint=6 (bits 3-0)
   * @par Transfer type: Interrupt
   * @par Max packet size: 8 bytes
   * @par Polling interval: 10 ms
   */
  k_port1_ep_int_in = 0x86,

  /* =========================================================================
   * Port 2 (Log) Endpoints
   *
   * System logging output for firmware diagnostics.
   * Receives rx_log_*() output with timestamps and severity levels.
   * ========================================================================= */

  /**
   * @brief Port 2 bulk IN endpoint for log output
   *
   * @details
   * Bulk transfer endpoint for streaming firmware log messages to the
   * host. Receives output from rx_log_error(), rx_log_warn(),
   * rx_log_info(), and rx_log_debug() calls.
   *
   * @par Value: 0x87
   * @par Encoding: Direction=IN (bit 7=1), Endpoint=7 (bits 3-0)
   * @par Transfer type: Bulk
   * @par Max packet size: 64 bytes
   * @par FIFO allocation: 256 bytes (double-buffered)
   *
   * @par Output format:
   * - "[00123.456] [E] MOTOR: Overcurrent fault detected"
   * - "[00123.457] [I] USB: Host connected"
   */
  k_port2_ep_bulk_in = 0x87,

  /**
   * @brief Port 2 bulk OUT endpoint for log port input
   *
   * @details
   * Bulk transfer endpoint for receiving data on the log port.
   * Can be used for runtime log level configuration or debug commands.
   *
   * @par Value: 0x08
   * @par Encoding: Direction=OUT (bit 7=0), Endpoint=8 (bits 3-0)
   * @par Transfer type: Bulk
   * @par Max packet size: 64 bytes
   * @par FIFO allocation: 256 bytes (double-buffered)
   */
  k_port2_ep_bulk_out = 0x08,

  /**
   * @brief Port 2 interrupt IN endpoint for CDC notifications
   *
   * @details
   * Interrupt transfer endpoint for CDC ACM serial state notifications
   * on the log output port.
   *
   * @par Value: 0x89
   * @par Encoding: Direction=IN (bit 7=1), Endpoint=9 (bits 3-0)
   * @par Transfer type: Interrupt
   * @par Max packet size: 8 bytes
   * @par Polling interval: 10 ms
   */
  k_port2_ep_int_in = 0x89,
} usb_endpoint_addresses_t;

#ifdef __cplusplus
}
#endif
