/**
 * @file rx_usb_config.h
 * @brief Compile-time configuration for the RX72N 3-port CDC composite device.
 *
 * Each CDC port can be individually disabled at build time.  Default is
 * all three enabled.  Override by defining the flag to 0 on the
 * compiler command line, in a project header, or in CMake.
 *
 * @par Port roles (matching rx_usb_port_id_t)
 * | Port | Macro                         | Default | Purpose                          |
 * |------|-------------------------------|---------|----------------------------------|
 * | 0    | STAR_USB_ENABLE_PORT_PROTO    | 1       | nanopb binary protocol /ttyACM0  |
 * | 1    | STAR_USB_ENABLE_PORT_DECODED  | 1       | ASCII frame dumps /ttyACM1       |
 * | 2    | STAR_USB_ENABLE_PORT_LOG      | 1       | log output /ttyACM2              |
 *
 * @par Behavior when a port is disabled
 *   - Its bulk / interrupt pipes are **not configured** on the device.
 *   - Its BRDY / BEMP interrupt enables are left clear.
 *   - `rx_usb_write()` / `rx_usb_read()` / `rx_usb_puts()` / etc. on
 *     that port return `k_rx_err_invalid_arg` without touching
 *     hardware.
 *   - The `k_usb_event_configured` callback is NOT invoked for that
 *     port.
 *   - The composite config descriptor advertised to the host still
 *     lists all three CDC-ACM functions.  The host will see the
 *     interface but any bulk traffic to its endpoints is NAK'd by
 *     hardware (no pipe is bound to the EP number).  Keeping the
 *     descriptor stable means enumeration behaviour doesn't change
 *     across build variants -- useful when bisecting host-side issues.
 *
 * At least one port must be enabled (enforced by static_assert below).
 *
 * @par Typical build overrides
 * @code
 *   # Minimum: only the protobuf protocol port
 *   rx-elf-gcc ... -DSTAR_USB_ENABLE_PORT_DECODED=0 \
 *                  -DSTAR_USB_ENABLE_PORT_LOG=0     ...
 *
 *   # Headless log-only build (telemetry push only, no commands)
 *   rx-elf-gcc ... -DSTAR_USB_ENABLE_PORT_PROTO=0   \
 *                  -DSTAR_USB_ENABLE_PORT_DECODED=0 ...
 * @endcode
 */

#pragma once

#ifndef STAR_USB_ENABLE_PORT_PROTO
#define STAR_USB_ENABLE_PORT_PROTO   1
#endif

#ifndef STAR_USB_ENABLE_PORT_DECODED
#define STAR_USB_ENABLE_PORT_DECODED 1
#endif

#ifndef STAR_USB_ENABLE_PORT_LOG
#define STAR_USB_ENABLE_PORT_LOG     1
#endif

#if (STAR_USB_ENABLE_PORT_PROTO + STAR_USB_ENABLE_PORT_DECODED + STAR_USB_ENABLE_PORT_LOG) == 0
#error \
    "rx_usb_config.h: at least one of STAR_USB_ENABLE_PORT_{PROTO,DECODED,LOG} must be non-zero"
#endif

/**
 * @brief Number of CDC ports enabled at compile time (1..3).
 *
 * Useful for sizing diagnostic arrays or formatting banner strings.
 * Does NOT change rx_usb_port_id_t's enum range -- that stays 0..2 so
 * that port IDs remain stable across build variants.
 */
#define STAR_USB_ENABLED_PORT_COUNT \
  (STAR_USB_ENABLE_PORT_PROTO + STAR_USB_ENABLE_PORT_DECODED + STAR_USB_ENABLE_PORT_LOG)
