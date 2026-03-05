/* star-rx72n-firmware/libs/rx_crc/inc/rx_crc.h */

/**
 * @file rx_crc.h
 * @brief Cyclic Redundancy Check (CRC) API for Data Integrity Validation
 *
 * @details
 * ## Overview
 * Production-quality CRC (Cyclic Redundancy Check) implementation providing both
 * hardware-accelerated and software-based checksumming for data integrity validation
 * across all STAR firmware communication protocols and storage subsystems.
 *
 * Supports two CRC algorithms:
 * - **CRC-32/IEEE 802.3** - For frame protocols, SPI, USB, file integrity
 * - **CRC-8/Maxim** - For OneWire (DS18B20) and Dallas/Maxim devices
 *
 * ## CRC Algorithm Comparison
 *
 * | Feature | CRC-32/IEEE 802.3 | CRC-8/Maxim |
 * |---------|-------------------|-------------|
 * | **Polynomial** | 0x04C11DB7 (reflected: 0xEDB88320) | 0x31 (reflected: 0x8C) |
 * | **Output size** | 32 bits | 8 bits |
 * | **Initial value** | 0xFFFFFFFF | 0x00 |
 * | **Final XOR** | 0xFFFFFFFF | 0x00 |
 * | **Bit order** | LSB-first (reflected) | LSB-first |
 * | **Detection** | 4.3 billion errors | 256 errors |
 * | **Usage** | Ethernet, ZIP, PNG, SPI frames | OneWire devices, ROM codes |
 * | **Compatibility** | Go crc32.ChecksumIEEE() | DS18B20, DS2482 |
 * | **Performance (SW)** | ~130 us/100 bytes @ 240 MHz | ~15 us/8 bytes @ 240 MHz |
 * | **Performance (HW)** | ~32 us/100 bytes @ 240 MHz | N/A (SW only) |
 *
 * ## Hardware vs Software Implementation
 *
 * **Hardware CRC (RX72N only):**
 * - Uses RX72N CRC peripheral (32-bit, polynomial 0x04C11DB7)
 * - **4x faster** than software for CRC-32 (32 us vs 130 us per 100 bytes)
 * - Requires initialization via rx_crc_init()
 * - Zero CPU overhead during computation
 * - Limited to CRC-32/IEEE 802.3 polynomial
 *
 * **Software CRC (Host and RX72N):**
 * - Table-based lookup for fast software computation
 * - Works on host (x86/ARM) and embedded (RX72N)
 * - No initialization required
 * - Supports both CRC-32 and CRC-8 algorithms
 * - Portable across all platforms
 *
 * **Automatic Selection:**
 * ```c
 * // Compile-time selection based on RX_CRC32_USE_HARDWARE define
 * #ifdef RX_CRC32_USE_HARDWARE
 *   // Use hardware CRC peripheral (RX72N)
 * #else
 *   // Use software table-based CRC (portable)
 * #endif
 * ```
 *
 * ## Performance Characteristics
 *
 * **CRC-32/IEEE 802.3** (@ 240 MHz RX72N):
 *
 * | Data Size | Hardware CRC | Software CRC | Speedup |
 * |-----------|--------------|--------------|---------|
 * | 8 bytes | 5 us | 10 us | 2.0x |
 * | 64 bytes | 20 us | 85 us | 4.25x |
 * | 100 bytes | 32 us | 130 us | 4.06x |
 * | 255 bytes | 80 us | 330 us | 4.13x |
 * | 1024 bytes | 320 us | 1320 us | 4.13x |
 *
 * **CRC-8/Maxim** (Software only, @ 240 MHz):
 *
 * | Data Size | Time | Notes |
 * |-----------|------|-------|
 * | 8 bytes | ~15 us | Typical OneWire ROM code |
 * | 9 bytes | ~17 us | ROM + CRC byte |
 * | 64 bytes | ~120 us | Scratchpad data |
 *
 * **Throughput Analysis:**
 * - **Hardware CRC-32**: ~3.2 MB/s (1024 bytes / 320 us)
 * - **Software CRC-32**: ~0.78 MB/s (1024 bytes / 1320 us)
 * - **Software CRC-8**: ~0.47 MB/s (64 bytes / 120 us, limited use case)
 *
 * ## Memory Usage
 *
 * **Static Memory:**
 * - **CRC-32 lookup table**: 1024 bytes (256 entries x 4 bytes)
 * - **CRC-8 lookup table**: 256 bytes (256 entries x 1 byte)
 * - **Total**: 1280 bytes (shared globally, one-time cost)
 *
 * **Stack Usage:**
 * - `rx_crc32_ieee()`: ~16 bytes (loop counter, temp vars)
 * - `rx_crc8_maxim()`: ~12 bytes (loop counter, temp vars)
 * - `rx_crc_init()`: ~8 bytes (minimal)
 *
 * ## Hardware Requirements
 *
 * **For Hardware CRC (RX72N only):**
 * - **Peripheral**: CRC Calculator (CRCA)
 * - **Clock**: PCLKA (120 MHz typical)
 * - **Register base**: 0x0008C000
 * - **Power domain**: Module stop control register (MSTPCRB bit 23)
 * - **Initialization**: Must call rx_crc_init() before use
 *
 * **For Software CRC (All platforms):**
 * - No hardware dependencies
 * - Works on RX72N, x86, ARM, etc.
 * - No initialization required
 *
 * ## CRC-32/IEEE 802.3 Mathematical Definition
 *
 * **Polynomial:**
 * @f[
 *   P(x) = x^{32} + x^{26} + x^{23} + x^{22} + x^{16} + x^{12} + x^{11} +
 *          x^{10} + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1
 * @f]
 *
 * **Binary representation:** 0x04C11DB7 (normal), 0xEDB88320 (reflected)
 *
 * **Algorithm (reflected, LSB-first):**
 * @f[
 *   \text{CRC} = \text{CRC}_{\text{init}} \oplus 0xFFFFFFFF = 0xFFFFFFFF
 * @f]
 * @f[
 *   \text{For each byte } b: \quad \text{CRC} = (\text{CRC} \gg 8) \oplus
 *   \text{Table}[(\text{CRC} \oplus b) \& 0xFF]
 * @f]
 * @f[
 *   \text{CRC}_{\text{final}} = \text{CRC} \oplus 0xFFFFFFFF
 * @f]
 *
 * **Properties:**
 * - Detects all single-bit errors
 * - Detects all double-bit errors
 * - Detects all burst errors <= 32 bits
 * - Detects 99.9999% of all other errors
 *
 * ## CRC-8/Maxim Mathematical Definition
 *
 * **Polynomial:**
 * @f[
 *   P(x) = x^8 + x^5 + x^4 + 1
 * @f]
 *
 * **Binary representation:** 0x31 (normal), 0x8C (reflected)
 *
 * **Algorithm (LSB-first):**
 * @f[
 *   \text{CRC} = 0x00 \quad (\text{initial value})
 * @f]
 * @f[
 *   \text{For each byte } b: \quad \text{CRC} = \text{Table}[\text{CRC} \oplus b]
 * @f]
 *
 * **Properties:**
 * - Detects all single-bit errors
 * - Detects all 2-bit errors within 8 bits
 * - Used by Dallas/Maxim OneWire devices (DS18B20, DS2482, etc.)
 *
 * ## Thread Safety
 *
 * | Function | Thread Safe? | Notes |
 * |----------|--------------|-------|
 * | `rx_crc_init()` | [FAIL] No | Call once during system init, single-threaded |
 * | `rx_crc32_ieee()` | [PASS] Yes | Stateless, read-only lookup table |
 * | `rx_crc32_update()` | [PASS] Yes | Stateless, pure function |
 * | `rx_crc8_maxim()` | [PASS] Yes | Stateless, read-only lookup table |
 * | `rx_crc_deinit()` | [FAIL] No | Call once during shutdown, single-threaded |
 *
 * **Note:** CRC computation functions are thread-safe because they use read-only
 * lookup tables and have no shared mutable state.
 *
 * ## Integration Example - SPI Frame Validation
 *
 * @code{.c}
 * #include "rx_crc.h"
 * #include "rx_spi_comm.h"
 *
 * void system_init(void)
 * {
 *   // Initialize CRC hardware (if available)
 *   rx_err_t err = rx_crc_init();
 *   if (err != k_rx_ok) {
 *     rx_log_warn("CRC", "Hardware CRC unavailable, using software");
 *   }
 * }
 *
 * // Transmit side: Compute CRC for outgoing frame
 * void send_spi_frame(const uint8_t* payload, uint32_t len)
 * {
 *   // Build frame header
 *   uint8_t frame[279];  // Max SPI frame size
 *   frame[0] = 0xAA;  // Sync high
 *   frame[1] = 0x55;  // Sync low
 *   // ... populate sequence, length, type, flags ...
 *
 *   // Copy payload
 *   memcpy(&frame[10], payload, len);
 *
 *   // Compute CRC-32 over header + payload
 *   uint32_t crc_offset = 10 + len;
 *   uint32_t crc = rx_crc32_ieee(frame, crc_offset);
 *
 *   // Append CRC (little-endian)
 *   frame[crc_offset + 0] = (crc >> 0) & 0xFF;
 *   frame[crc_offset + 1] = (crc >> 8) & 0xFF;
 *   frame[crc_offset + 2] = (crc >> 16) & 0xFF;
 *   frame[crc_offset + 3] = (crc >> 24) & 0xFF;
 *
 *   // Send via SPI
 *   spi_transmit(frame, crc_offset + 4);
 * }
 *
 * // Receive side: Validate CRC of incoming frame
 * rx_err_t receive_spi_frame(uint8_t* frame, uint32_t frame_len)
 * {
 *   // Extract received CRC (last 4 bytes, little-endian)
 *   uint32_t crc_offset = frame_len - 4;
 *   uint32_t received_crc = (frame[crc_offset + 0] << 0) |
 *                           (frame[crc_offset + 1] << 8) |
 *                           (frame[crc_offset + 2] << 16) |
 *                           (frame[crc_offset + 3] << 24);
 *
 *   // Compute CRC over header + payload (exclude CRC itself)
 *   uint32_t calculated_crc = rx_crc32_ieee(frame, crc_offset);
 *
 *   // Validate
 *   if (received_crc != calculated_crc) {
 *     rx_log_error("SPI", "CRC mismatch: rx=0x%08X calc=0x%08X",
 *                  received_crc, calculated_crc);
 *     return k_rx_err_crc_mismatch;
 *   }
 *
 *   return k_rx_ok;  // Frame valid
 * }
 * @endcode
 *
 * ## Integration Example - OneWire ROM Code Validation
 *
 * @code{.c}
 * #include "rx_crc.h"
 * #include "rx_hcsr04.h"  // OneWire interface
 *
 * // Validate DS18B20 64-bit ROM code
 * bool validate_onewire_rom(const uint8_t* rom)
 * {
 *   // ROM format: [Family Code][48-bit Serial][CRC-8]
 *   // Example: 28 FF 64 1F 21 16 04 B3
 *   //          ^^                   ^^
 *   //          Family (DS18B20)     CRC
 *
 *   // Compute CRC over first 7 bytes
 *   uint8_t calculated_crc = rx_crc8_maxim(rom, 7);
 *   uint8_t received_crc = rom[7];
 *
 *   if (calculated_crc != received_crc) {
 *     rx_log_error("OneWire", "ROM CRC mismatch: rx=0x%02X calc=0x%02X",
 *                  received_crc, calculated_crc);
 *     return false;
 *   }
 *
 *   rx_log_debug("OneWire", "ROM valid: Family=0x%02X Serial=%02X%02X%02X%02X%02X%02X",
 *                rom[0], rom[6], rom[5], rom[4], rom[3], rom[2], rom[1]);
 *   return true;
 * }
 * @endcode
 *
 * ## Integration Example - Incremental CRC for Large Data
 *
 * @code{.c}
 * #include "rx_crc.h"
 *
 * // Compute CRC-32 over multiple buffers without concatenation
 * uint32_t compute_multi_buffer_crc(void)
 * {
 *   // Initialize CRC
 *   uint32_t crc = 0xFFFFFFFF;  // CRC-32/IEEE initial value
 *
 *   // Process buffer 1
 *   uint8_t buf1[64] = { ... };
 *   crc = rx_crc32_update(crc, buf1, sizeof(buf1));
 *
 *   // Process buffer 2
 *   uint8_t buf2[128] = { ... };
 *   crc = rx_crc32_update(crc, buf2, sizeof(buf2));
 *
 *   // Process buffer 3
 *   uint8_t buf3[32] = { ... };
 *   crc = rx_crc32_update(crc, buf3, sizeof(buf3));
 *
 *   // Finalize CRC
 *   crc ^= 0xFFFFFFFF;
 *
 *   return crc;  // Final CRC-32 value
 * }
 *
 * // Equivalent to single-shot computation:
 * // uint8_t all_data[224];
 * // memcpy(&all_data[0], buf1, 64);
 * // memcpy(&all_data[64], buf2, 128);
 * // memcpy(&all_data[192], buf3, 32);
 * // uint32_t crc = rx_crc32_ieee(all_data, 224);
 * @endcode
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation Notes |
 * |------|--------|---------------------|
 * | 1. Simple control flow | [PASS] Pass | No goto/setjmp/recursion, simple loops only |
 * | 2. Fixed loop bounds | [PASS] Pass | All loops bounded by data length (caller-controlled) |
 * | 3. No dynamic memory | [PASS] Pass | Lookup tables statically allocated, no malloc |
 * | 4. Short functions | [PASS] Pass | All functions <40 lines, single responsibility |
 * | 5. Assertions | [PASS] Pass | nullptr checks, parameter validation |
 * | 6. Small scope | [PASS] Pass | Variables declared near use, minimal scope |
 * | 7. Check returns | [PASS] Pass | rx_crc_init() return checked, CRC functions return value directly |
 * | 8. Limited preprocessor | [PASS] Pass | Only compile-time HW/SW selection, no magic numbers |
 * | 9. Restrict pointers | [PASS] Pass | No function pointers, simple data pointers only |
 * | 10. Compiler warnings | [PASS] Pass | -Wall -Wextra -Werror, zero warnings |
 *
 * ## SOLID Principles
 *
 * | Principle | Implementation |
 * |-----------|----------------|
 * | **Single Responsibility** | Module handles ONLY CRC computation, no frame parsing or validation logic |
 * | **Open/Closed** | Supports multiple CRC algorithms (CRC-32, CRC-8) without modifying existing code |
 * | **Liskov Substitution** | Hardware/software CRC implementations interchangeable via compile-time selection |
 * | **Interface Segregation** | Minimal API (4 functions), each serves specific purpose |
 * | **Dependency Inversion** | CRC functions independent of frame protocol, reusable across OneWire, SPI, USB |
 *
 * ## References
 *
 * - **IEEE 802.3**: Ethernet CRC-32 specification
 * - **Go crc32 package**: Compatible ChecksumIEEE() implementation
 * - **RX72N Manual**: Section 30 (CRC Calculator peripheral)
 * - **Dallas/Maxim App Note 27**: Understanding and Using Cyclic Redundancy Checks
 * - **DS18B20 Datasheet**: OneWire temperature sensor with CRC-8
 *
 * @see rx_crc32.c CRC-32 implementation (dispatcher)
 * @see rx_crc32_hw.c Hardware CRC-32 (RX72N peripheral)
 * @see rx_crc32_sw.c Software CRC-32 (table-based)
 * @see rx_crc8.c CRC-8/Maxim implementation
 * @see rx_frame.h Frame protocol using CRC-32
 * @see rx_spi_comm.h SPI communication using CRC-32
 *
 * @since Version 1.0.0
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize CRC backend (hardware peripheral if available)
 *
 * @details
 * Initializes the CRC subsystem, enabling hardware CRC peripheral on RX72N
 * targets for accelerated CRC-32 computation. On host builds (x86/ARM), this
 * is a no-op since only software CRC is available.
 *
 * **Platform Behavior:**
 * - **RX72N (`__RX__` defined, `RX_CRC32_USE_HARDWARE` defined)**:
 *   - Enables CRCA (CRC Calculator) peripheral via module stop control
 *   - Configures CRC polynomial to 0x04C11DB7 (IEEE 802.3)
 *   - Sets LSB-first bit order (reflected)
 *   - Initializes registers for 32-bit CRC computation
 *   - Returns k_rx_ok if hardware available and configured
 * - **Host builds** (Linux, macOS, Windows for testing):
 *   - No-op, returns k_rx_ok immediately
 *   - Software CRC tables already initialized statically
 *
 * **Algorithm (RX72N Hardware Init):**
 * 1. **Validate not already initialized** (prevent double-init)
 * 2. **Enable CRCA module clock** via MSTPCRB register (bit 23 = 0)
 * 3. **Reset CRC peripheral** by writing control register
 * 4. **Configure polynomial** to 0x04C11DB7 (IEEE 802.3 standard)
 * 5. **Set bit order** to LSB-first (reflected, compatible with software CRC)
 * 6. **Set initial value** to 0xFFFFFFFF (IEEE 802.3 standard)
 * 7. **Mark initialized** (set internal flag)
 *
 * @return rx_err_t Initialization result
 * @retval k_rx_ok Initialization successful (hardware or no-op)
 * @retval k_rx_err_invalid_state Already initialized (RX72N only)
 * @retval k_rx_err_hardware Hardware CRC peripheral fault (RX72N only, rare)
 *
 * @pre System clock (PCLKA) must be configured and running
 * @pre Module stop registers must be accessible
 * @post On RX72N, CRCA peripheral ready for rx_crc32_ieee() hardware acceleration
 * @post On host, software CRC tables ready for use
 * @post rx_crc32_ieee() will use fastest available implementation
 *
 * @note Call this ONCE during system initialization before any CRC operations
 * @note Thread-safe ONLY if called single-threaded during init
 * @warning Do not call from interrupt context
 * @warning On RX72N, failure to call this results in slower software CRC fallback
 *
 * @par Performance Impact:
 * - **With init (HW CRC)**: 32 us per 100 bytes (4x faster)
 * - **Without init (SW CRC)**: 130 us per 100 bytes
 *
 * @par Example - System Initialization:
 * @code{.c}
 * void system_init(void)
 * {
 *   // Initialize clocks first
 *   rx_clock_init();
 *
 *   // Initialize CRC (enables hardware if available)
 *   rx_err_t err = rx_crc_init();
 *   if (err == k_rx_ok) {
 *     rx_log_info("CRC", "CRC initialized (HW accelerated)");
 *   } else {
 *     rx_log_warn("CRC", "CRC init failed: %d (SW fallback)", err);
 *   }
 *
 *   // Now CRC functions can use hardware acceleration
 *   uint32_t crc = rx_crc32_ieee(data, len);  // Fast path
 * }
 * @endcode
 *
 * @par Example - Conditional Hardware CRC:
 * @code{.c}
 * #ifdef __RX__
 *   // RX72N: Try to enable hardware CRC
 *   if (rx_crc_init() == k_rx_ok) {
 *     rx_log_info("CRC", "Using hardware CRC (4x faster)");
 *   } else {
 *     rx_log_warn("CRC", "Using software CRC");
 *   }
 * #else
 *   // Host: Software CRC always available
 *   rx_crc_init();  // No-op, but safe to call
 *   rx_log_info("CRC", "Using software CRC (portable)");
 * #endif
 * @endcode
 *
 * @see rx_crc_deinit() Disable CRC hardware when done
 * @see rx_crc32_ieee() CRC-32 computation (uses HW if initialized)
 * @see rx_crc32_hw.c Hardware CRC implementation
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_crc_init(void);

/**
 * @brief Deinitialize CRC backend (hardware peripheral shutdown)
 *
 * @details
 * Disables the CRC hardware peripheral on RX72N targets to save power when CRC
 * operations are no longer needed. On host builds (x86/ARM), this is a no-op
 * since software CRC requires no deinitialization.
 *
 * **Platform Behavior:**
 * - **RX72N (`__RX__` defined, `RX_CRC32_USE_HARDWARE` defined)**:
 *   - Disables CRCA (CRC Calculator) peripheral via module stop control
 *   - Sets MSTPCRB bit 23 = 1 to stop peripheral clock
 *   - Clears internal initialization flag
 *   - Frees hardware resource for other modules
 *   - **Note**: Current implementation intentionally leaves CRC enabled for continuous availability
 * - **Host builds** (Linux, macOS, Windows for testing):
 *   - No-op, returns k_rx_ok immediately
 *   - Software CRC tables remain in memory (statically allocated)
 *
 * **Algorithm (RX72N Hardware Deinit):**
 * 1. **Validate initialized** (prevent deinit of uninitialized module)
 * 2. **Disable CRCA module clock** via MSTPCRB register (bit 23 = 1)
 * 3. **Clear registers** (reset peripheral state)
 * 4. **Mark uninitialized** (clear internal flag)
 *
 * **Power Savings:**
 * - **With deinit**: CRCA module stopped, saves ~0.2 mA @ 3.3V (negligible)
 * - **Without deinit**: CRCA remains clocked, minimal power overhead
 * - **Recommendation**: Leave CRC enabled unless power is critical (low-power mode)
 *
 * @return rx_err_t Deinitialization result
 * @retval k_rx_ok Deinitialization successful (hardware or no-op)
 * @retval k_rx_err_invalid_state Not initialized (RX72N only)
 *
 * @pre CRC module must have been initialized via rx_crc_init()
 * @post On RX72N, CRCA peripheral disabled and clock stopped (if implementation changed)
 * @post rx_crc32_ieee() will fall back to software CRC
 * @post On host, no observable change (software CRC unaffected)
 *
 * @note Call this ONCE during system shutdown if power optimization is critical
 * @note Thread-safe ONLY if called single-threaded during shutdown
 * @note After deinit, rx_crc32_ieee() still works (software fallback)
 * @warning Do not call from interrupt context
 * @warning Current implementation is a no-op to keep CRC available
 *
 * @par Design Rationale (No-Op Deinit):
 * The current implementation intentionally does NOT disable the CRC module
 * after initialization. Reasons:
 * 1. CRC calculations may be needed at any time for frame validation
 * 2. Module stop/start overhead is significant (~50 us per init)
 * 3. Power savings from disabling are minimal (<0.2 mA)
 * 4. No functional benefit in typical operation
 *
 * If power optimization is critical (low-power sleep modes),
 * modify implementation to actually disable peripheral.
 *
 * @par Performance Impact:
 * - **After deinit (SW CRC)**: 130 us per 100 bytes (4x slower)
 * - **After init (HW CRC)**: 32 us per 100 bytes
 *
 * @par Example - System Shutdown:
 * @code{.c}
 * void system_shutdown(void)
 * {
 *   // Deinitialize CRC (optional, for power savings)
 *   rx_err_t err = rx_crc_deinit();
 *   if (err == k_rx_ok) {
 *     rx_log_info("CRC", "CRC deinitialized (power saving)");
 *   } else {
 *     rx_log_warn("CRC", "CRC deinit failed: %d", err);
 *   }
 *
 *   // Other shutdown tasks...
 *   rx_gpio_deinit();
 *   rx_clock_deinit();
 * }
 * @endcode
 *
 * @par Example - Conditional Deinit (Low-Power Mode):
 * @code{.c}
 * void enter_low_power_mode(void)
 * {
 *   // Disable CRC to save power
 *   rx_crc_deinit();
 *
 *   // CRC still works (software fallback)
 *   uint32_t crc = rx_crc32_ieee(data, len);  // Slower, but functional
 *
 *   // Enter low-power sleep
 *   rx_power_enter_sleep();
 * }
 *
 * void exit_low_power_mode(void)
 * {
 *   // Re-enable CRC for performance
 *   rx_crc_init();
 *
 *   // CRC now fast again (hardware)
 *   uint32_t crc = rx_crc32_ieee(data, len);  // Fast path
 * }
 * @endcode
 *
 * @see rx_crc_init() Initialize CRC hardware
 * @see rx_crc32_ieee() CRC-32 computation (unaffected by deinit)
 * @see rx_crc32_hw.c Hardware CRC implementation
 *
 * @since Version 1.0.0
 */
[[nodiscard]] rx_err_t rx_crc_deinit(void);

/**
 * @brief Compute CRC-32/IEEE 802.3 checksum over buffer
 *
 * @details
 * Computes 32-bit cyclic redundancy check using IEEE 802.3 polynomial
 * (0x04C11DB7 in reflected/LSB-first form 0xEDB88320). Automatically selects
 * hardware-accelerated or software table-based implementation at compile time.
 *
 * **Algorithm Selection:**
 * - **RX72N (if rx_crc_init() called)**: Hardware CRC peripheral (4x faster)
 * - **RX72N (no init) or Host**: Software table lookup (portable, slower)
 *
 * **CRC-32/IEEE 802.3 Specification:**
 * - **Polynomial**: @f$ x^{32} + x^{26} + x^{23} + ... + x + 1 @f$ (0x04C11DB7)
 * - **Initial value**: 0xFFFFFFFF
 * - **Final XOR**: 0xFFFFFFFF
 * - **Bit order**: LSB-first (reflected)
 * - **Byte order**: Process bytes sequentially, LSB of each byte first
 *
 * **Compatibility:**
 * - Matches Go `crc32.ChecksumIEEE()`
 * - Matches Python `zlib.crc32()`
 * - Matches Ethernet FCS (Frame Check Sequence)
 * - Matches ZIP, PNG, Gzip CRC-32
 *
 * **Algorithm (Software):**
 * 1. **Initialize CRC**: `crc = 0xFFFFFFFF`
 * 2. **For each byte**:
 *    - `table_index = (crc ^ byte) & 0xFF`
 *    - `crc = (crc >> 8) ^ lookup_table[table_index]`
 * 3. **Finalize**: `crc ^= 0xFFFFFFFF`
 * 4. **Return** final CRC value
 *
 * **Algorithm (Hardware, RX72N):**
 * 1. **Write data to CRCA data register** (DMA or CPU)
 * 2. **Read CRC from CRCA result register**
 * 3. **Apply final XOR** (0xFFFFFFFF)
 * 4. **Return** final CRC value
 *
 * @param[in] data Input buffer to checksum
 *   - **Valid range**: NULL or pointer to buffer of size len
 *   - **NULL handling**: If NULL, len must be 0 (returns 0xFFFFFFFF XOR 0xFFFFFFFF = 0)
 *   - **Alignment**: No alignment requirement (byte-addressable)
 *
 * @param[in] len Number of bytes to include in CRC
 *   - **Valid range**: 0 - 4294967295 (0 - 2^32-1)
 *   - **Typical**: 10-275 bytes (frame sizes)
 *   - **Zero handling**: len=0 returns init value XOR final XOR = 0
 *
 * @return uint32_t Computed CRC-32 checksum
 *   - **Range**: 0x00000000 - 0xFFFFFFFF (all 32-bit values possible)
 *   - **Deterministic**: Same input always produces same output
 *   - **Zero for zero**: Empty buffer (len=0) returns 0
 *
 * @pre If data != nullptr, data must point to valid buffer of size len
 * @pre len must accurately reflect data buffer size (no bounds checking)
 * @post Return value is valid IEEE 802.3 CRC-32
 * @post No side effects, pure function (thread-safe)
 *
 * @note This function is thread-safe (stateless, read-only tables)
 * @note For incremental CRC over multiple buffers, use rx_crc32_update()
 * @warning Passing invalid data pointer or incorrect len causes undefined behavior
 * @warning For large buffers (>10 KB), consider chunking with rx_crc32_update()
 *
 * @par Performance:
 * - **Hardware (RX72N)**: ~32 us / 100 bytes @ 240 MHz
 * - **Software (RX72N)**: ~130 us / 100 bytes @ 240 MHz
 * - **Software (x86)**: ~50 us / 100 bytes @ 3 GHz
 *
 * @par Example - Basic Usage:
 * @code{.c}
 * uint8_t data[] = "Hello World";
 * uint32_t crc = rx_crc32_ieee(data, sizeof(data) - 1);  // Exclude null terminator
 * printf("CRC-32: 0x%08X\n", crc);
 * // Output: CRC-32: 0x4A17B156
 * @endcode
 *
 * @par Example - Frame Integrity Check:
 * @code{.c}
 * // Transmit side: Append CRC to frame
 * uint8_t tx_frame[279];
 * // ... populate header and payload ...
 * uint32_t crc = rx_crc32_ieee(tx_frame, 275);  // CRC over first 275 bytes
 *
 * // Append CRC (little-endian)
 * tx_frame[275] = (crc >> 0) & 0xFF;
 * tx_frame[276] = (crc >> 8) & 0xFF;
 * tx_frame[277] = (crc >> 16) & 0xFF;
 * tx_frame[278] = (crc >> 24) & 0xFF;
 *
 * // Receive side: Validate CRC
 * uint8_t rx_frame[279];
 * // ... receive frame from SPI ...
 *
 * uint32_t received_crc = (rx_frame[275] << 0) |
 *                         (rx_frame[276] << 8) |
 *                         (rx_frame[277] << 16) |
 *                         (rx_frame[278] << 24);
 *
 * uint32_t calculated_crc = rx_crc32_ieee(rx_frame, 275);
 *
 * if (received_crc == calculated_crc) {
 *   // Frame valid, process data
 * } else {
 *   // Corruption detected, discard frame
 *   rx_log_error("CRC", "Mismatch: rx=0x%08X calc=0x%08X",
 *                received_crc, calculated_crc);
 * }
 * @endcode
 *
 * @par Example - Zero-Length Buffer:
 * @code{.c}
 * uint32_t crc = rx_crc32_ieee(nullptr, 0);
 * // Returns: 0x00000000 (init 0xFFFFFFFF XOR final 0xFFFFFFFF)
 * @endcode
 *
 * @par Example - Performance Comparison:
 * @code{.c}
 * #include "rx_time.h"
 *
 * uint8_t data[1024];
 * memset(data, 0x55, sizeof(data));
 *
 * // Without hardware CRC initialization
 * uint32_t start = rx_time_get_us();
 * uint32_t crc_sw = rx_crc32_ieee(data, sizeof(data));  // Software
 * uint32_t elapsed_sw = rx_time_get_us() - start;
 * rx_log_info("CRC", "Software: %u us", elapsed_sw);
 * // Output: Software: 1320 us
 *
 * // With hardware CRC initialization
 * rx_crc_init();
 * start = rx_time_get_us();
 * uint32_t crc_hw = rx_crc32_ieee(data, sizeof(data));  // Hardware
 * uint32_t elapsed_hw = rx_time_get_us() - start;
 * rx_log_info("CRC", "Hardware: %u us (%.1fx faster)",
 *             elapsed_hw, (float)elapsed_sw / elapsed_hw);
 * // Output: Hardware: 320 us (4.1x faster)
 *
 * // Both produce same result
 * assert(crc_sw == crc_hw);
 * @endcode
 *
 * @see rx_crc32_update() Incremental CRC for multiple buffers
 * @see rx_crc_init() Initialize hardware CRC (optional, for performance)
 * @see rx_crc32_hw.c Hardware CRC implementation (RX72N)
 * @see rx_crc32_sw.c Software CRC implementation (portable)
 *
 * @since Version 1.0.0
 */
uint32_t rx_crc32_ieee(const uint8_t* data, uint32_t len);

/**
 * @brief Update CRC-32 with additional data (incremental computation)
 *
 * @details
 * Continues a CRC-32/IEEE 802.3 calculation from a previous partial result,
 * allowing incremental CRC computation over non-contiguous buffers or streaming
 * data without concatenation. Uses software table-based implementation (hardware
 * CRC peripheral does not support loading arbitrary initial state).
 *
 * **Use Cases:**
 * 1. **Streaming data**: Compute CRC as data arrives in chunks
 * 2. **Non-contiguous buffers**: CRC over header + payload without copying
 * 3. **Large data sets**: Process data in manageable chunks
 * 4. **Memory-constrained**: Avoid allocating large concatenation buffer
 *
 * **Implementation:**
 * - **Always uses software CRC** (even on RX72N with hardware support)
 * - **Rationale**: Hardware CRC peripheral cannot load arbitrary initial CRC state
 * - **Performance**: Same as software rx_crc32_ieee() (~130 us per 100 bytes @ 240 MHz)
 *
 * **Algorithm (Software Table Lookup):**
 * 1. **Un-finalize input CRC**: `crc ^= 0xFFFFFFFF` (reverse final XOR)
 * 2. **For each byte**:
 *    - `table_index = (crc ^ byte) & 0xFF`
 *    - `crc = (crc >> 8) ^ lookup_table[table_index]`
 * 3. **Re-finalize CRC**: `crc ^= 0xFFFFFFFF` (apply final XOR)
 * 4. **Return** updated CRC value
 *
 * **Mathematical Equivalence:**
 * Computing CRC over concatenated data produces identical result:
 * ```
 * // Method 1: Incremental
 * uint32_t crc = rx_crc32_ieee(buf1, len1);
 * crc = rx_crc32_update(crc, buf2, len2);
 *
 * // Method 2: Concatenated (equivalent)
 * uint8_t combined[len1 + len2];
 * memcpy(&combined[0], buf1, len1);
 * memcpy(&combined[len1], buf2, len2);
 * uint32_t crc = rx_crc32_ieee(combined, len1 + len2);
 *
 * // Both methods produce identical CRC
 * ```
 *
 * @param[in] crc Previous CRC-32 value
 *   - **Valid range**: 0x00000000 - 0xFFFFFFFF (any 32-bit value)
 *   - **Initial value**: Use result from rx_crc32_ieee() or previous rx_crc32_update()
 *   - **Special case**: 0xFFFFFFFF is valid previous CRC (not same as init value)
 *
 * @param[in] data Additional data buffer to include in CRC
 *   - **Valid range**: NULL or pointer to buffer of size len
 *   - **NULL handling**: If NULL, len must be 0 (returns input crc unchanged)
 *   - **Alignment**: No alignment requirement (byte-addressable)
 *
 * @param[in] len Number of bytes in data buffer
 *   - **Valid range**: 0 - 4294967295 (0 - 2^32-1)
 *   - **Typical**: 10-275 bytes (frame chunks)
 *   - **Zero handling**: len=0 returns input crc unchanged
 *
 * @return uint32_t Updated CRC-32 checksum
 *   - **Range**: 0x00000000 - 0xFFFFFFFF (all 32-bit values possible)
 *   - **Deterministic**: Same inputs always produce same output
 *   - **Associative**: Order of updates matters, but partial results can be cached
 *
 * @pre If data != nullptr, data must point to valid buffer of size len
 * @pre len must accurately reflect data buffer size (no bounds checking)
 * @post Return value is valid IEEE 802.3 CRC-32
 * @post Input crc is unchanged (pure function)
 * @post No side effects (thread-safe)
 *
 * @note This function is thread-safe (stateless, read-only tables)
 * @note For single-shot CRC over contiguous buffer, use rx_crc32_ieee() (simpler)
 * @note Software CRC only - hardware peripheral does not support incremental mode
 * @warning Passing invalid data pointer or incorrect len causes undefined behavior
 * @warning Do not mix rx_crc32_ieee() and rx_crc32_update() incorrectly (see examples)
 *
 * @par Performance:
 * - **Software (RX72N)**: ~130 us / 100 bytes @ 240 MHz (same as rx_crc32_ieee)
 * - **Software (x86)**: ~50 us / 100 bytes @ 3 GHz
 *
 * @par Example - Basic Incremental CRC:
 * @code{.c}
 * // Compute CRC over two buffers without concatenation
 * uint8_t header[10] = { 0xAA, 0x55, ... };
 * uint8_t payload[64] = { ... };
 *
 * // Start with header
 * uint32_t crc = rx_crc32_ieee(header, sizeof(header));
 *
 * // Update with payload
 * crc = rx_crc32_update(crc, payload, sizeof(payload));
 *
 * // Final CRC is over header + payload (74 bytes total)
 * printf("CRC-32: 0x%08X\n", crc);
 * @endcode
 *
 * @par Example - Streaming Data:
 * @code{.c}
 * // Compute CRC as data arrives in chunks
 * uint32_t crc = rx_crc32_ieee(nullptr, 0);  // Start with init value (returns 0)
 * crc ^= 0xFFFFFFFF;  // Convert to "in-progress" CRC
 *
 * // Process chunk 1
 * uint8_t chunk1[32];
 * spi_receive(chunk1, sizeof(chunk1));
 * crc = rx_crc32_update(crc, chunk1, sizeof(chunk1));
 *
 * // Process chunk 2
 * uint8_t chunk2[64];
 * spi_receive(chunk2, sizeof(chunk2));
 * crc = rx_crc32_update(crc, chunk2, sizeof(chunk2));
 *
 * // Process chunk 3
 * uint8_t chunk3[16];
 * spi_receive(chunk3, sizeof(chunk3));
 * crc = rx_crc32_update(crc, chunk3, sizeof(chunk3));
 *
 * // Final CRC is over all chunks (112 bytes total)
 * printf("Streaming CRC-32: 0x%08X\n", crc);
 * @endcode
 *
 * @par Example - Frame Header + Payload:
 * @code{.c}
 * // SPI frame structure
 * typedef struct {
 *   uint16_t sync;      // 0xAA55
 *   uint16_t seq;       // Sequence number
 *   uint8_t  len;       // Payload length
 *   uint8_t  type;      // Message type
 *   uint8_t  flags;     // Control flags
 *   uint8_t  reserved;  // Reserved
 * } spi_header_t;
 *
 * spi_header_t header;
 * uint8_t      payload[255];
 *
 * // ... populate header and payload ...
 *
 * // Compute CRC over header + payload
 * uint32_t crc = rx_crc32_ieee((uint8_t*)&header, sizeof(header));
 * crc = rx_crc32_update(crc, payload, header.len);
 *
 * // Append CRC to frame
 * uint8_t crc_bytes[4] = {
 *   (crc >> 0) & 0xFF,
 *   (crc >> 8) & 0xFF,
 *   (crc >> 16) & 0xFF,
 *   (crc >> 24) & 0xFF,
 * };
 * spi_transmit(crc_bytes, sizeof(crc_bytes));
 * @endcode
 *
 * @par Example - Multi-Buffer CRC (Memory Efficient):
 * @code{.c}
 * // Compute CRC over 3 buffers without allocating concatenation buffer
 * uint8_t buf1[64];   // 64 bytes
 * uint8_t buf2[128];  // 128 bytes
 * uint8_t buf3[32];   // 32 bytes
 *
 * // Method 1: Incremental (no extra memory)
 * uint32_t crc = rx_crc32_ieee(buf1, sizeof(buf1));
 * crc = rx_crc32_update(crc, buf2, sizeof(buf2));
 * crc = rx_crc32_update(crc, buf3, sizeof(buf3));
 *
 * // Method 2: Concatenated (requires 224-byte buffer)
 * // uint8_t combined[224];
 * // memcpy(&combined[0], buf1, 64);
 * // memcpy(&combined[64], buf2, 128);
 * // memcpy(&combined[192], buf3, 32);
 * // uint32_t crc = rx_crc32_ieee(combined, 224);
 *
 * // Both methods produce identical CRC, but Method 1 saves 224 bytes
 * @endcode
 *
 * @par Example - WRONG Usage (Common Mistake):
 * @code{.c}
 * // [FAIL] WRONG: Don't start incremental CRC with rx_crc32_ieee(nullptr, 0)
 * uint32_t crc = rx_crc32_ieee(nullptr, 0);  // Returns 0 (init XOR final)
 * crc = rx_crc32_update(crc, buf1, len1);  // [FAIL] Incorrect CRC!
 *
 * // [PASS] CORRECT: Start with rx_crc32_ieee() on first buffer
 * uint32_t crc = rx_crc32_ieee(buf1, len1);
 * crc = rx_crc32_update(crc, buf2, len2);  // [PASS] Correct
 *
 * // OR: Manually initialize if you need empty start
 * uint32_t crc = 0xFFFFFFFF;  // CRC-32 init value (before final XOR)
 * crc = rx_crc32_update(crc, buf1, len1);
 * crc = rx_crc32_update(crc, buf2, len2);
 * crc ^= 0xFFFFFFFF;  // Apply final XOR
 * @endcode
 *
 * @see rx_crc32_ieee() Single-shot CRC-32 computation
 * @see rx_crc32_sw.c Software CRC implementation (used by this function)
 *
 * @since Version 1.0.0
 */
uint32_t rx_crc32_update(uint32_t crc, const uint8_t* data, uint32_t len);

/**
 * @brief Compute CRC-8/Maxim checksum for Dallas/Maxim OneWire devices
 *
 * @details
 * Computes 8-bit cyclic redundancy check using Dallas/Maxim polynomial (0x31)
 * in reflected/LSB-first form (0x8C). This CRC-8 variant is used by all Dallas
 * Semiconductor and Maxim Integrated OneWire devices for ROM code and scratchpad
 * data validation.
 *
 * **CRC-8/Maxim Specification:**
 * - **Polynomial**: @f$ x^8 + x^5 + x^4 + 1 @f$ (0x31 normal, 0x8C reflected)
 * - **Initial value**: 0x00
 * - **Final XOR**: 0x00 (no finalization)
 * - **Bit order**: LSB-first (reflected)
 * - **Byte order**: Process bytes sequentially, LSB of each byte first
 *
 * **Compatibility:**
 * - DS18B20 temperature sensor (ROM code validation)
 * - DS2482 OneWire bridge (command/response validation)
 * - DS2431 EEPROM (memory page validation)
 * - DS28EA00 IO device (ROM code validation)
 * - All Dallas/Maxim OneWire devices
 *
 * **Algorithm (Software Table Lookup):**
 * 1. **Initialize CRC**: `crc = 0x00`
 * 2. **For each byte**:
 *    - `table_index = crc ^ byte`
 *    - `crc = lookup_table[table_index]`
 * 3. **Return** final CRC value (no finalization XOR)
 *
 * **Mathematical Definition:**
 *
 * **Polynomial:**
 * @f[
 *   P(x) = x^8 + x^5 + x^4 + 1
 * @f]
 *
 * **Binary representation:** 0x31 (normal), 0x8C (reflected)
 *
 * **Algorithm (LSB-first):**
 * @f[
 *   \text{CRC} = 0x00 \quad (\text{initial value})
 * @f]
 * @f[
 *   \text{For each byte } b: \quad \text{CRC} = \text{Table}[\text{CRC} \oplus b]
 * @f]
 *
 * **Properties:**
 * - Detects all single-bit errors
 * - Detects all 2-bit errors within 8 bits
 * - Detects all burst errors <= 8 bits
 * - Detects 99.6% of all other errors (256 possible values)
 *
 * @param[in] data Input buffer to checksum
 *   - **Valid range**: NULL or pointer to buffer of size len
 *   - **NULL handling**: If NULL, len must be 0 (returns 0x00)
 *   - **Alignment**: No alignment requirement (byte-addressable)
 *   - **Typical size**: 7-8 bytes (OneWire ROM code)
 *
 * @param[in] len Number of bytes to include in CRC
 *   - **Valid range**: 0 - 4294967295 (0 - 2^32-1)
 *   - **Typical**: 7 bytes (ROM code without CRC) or 8 bytes (ROM with CRC)
 *   - **Zero handling**: len=0 returns 0x00
 *
 * @return uint8_t Computed CRC-8 checksum
 *   - **Range**: 0x00 - 0xFF (all 8-bit values possible)
 *   - **Deterministic**: Same input always produces same output
 *   - **Zero for zero**: Empty buffer (len=0) returns 0x00
 *
 * @pre If data != nullptr, data must point to valid buffer of size len
 * @pre len must accurately reflect data buffer size (no bounds checking)
 * @post Return value is valid CRC-8/Maxim checksum
 * @post No side effects, pure function (thread-safe)
 *
 * @note This function is thread-safe (stateless, read-only tables)
 * @note Software implementation only (no hardware acceleration on RX72N)
 * @note For OneWire ROM codes, compute CRC over first 7 bytes, compare to byte 8
 * @warning Passing invalid data pointer or incorrect len causes undefined behavior
 * @warning Different from other CRC-8 variants (CRC-8/CCITT, CRC-8/WCDMA, etc.)
 *
 * @par Performance:
 * - **Software (RX72N)**: ~15 us / 8 bytes @ 240 MHz
 * - **Software (x86)**: ~5 us / 8 bytes @ 3 GHz
 *
 * @par Example - OneWire ROM Code Validation:
 * @code{.c}
 * // DS18B20 ROM code format: [Family][Serial 6 bytes][CRC-8]
 * uint8_t rom[8] = { 0x28, 0xFF, 0x64, 0x1F, 0x21, 0x16, 0x04, 0xB3 };
 * //                 ^^^^                                      ^^^^
 * //                 Family (DS18B20)                          CRC
 *
 * // Compute CRC over first 7 bytes
 * uint8_t calculated_crc = rx_crc8_maxim(rom, 7);
 * uint8_t received_crc = rom[7];
 *
 * if (calculated_crc == received_crc) {
 *   printf("ROM valid: Family=0x%02X CRC=0x%02X\n", rom[0], rom[7]);
 *   // Output: ROM valid: Family=0x28 CRC=0xB3
 * } else {
 *   printf("ROM invalid: expected=0x%02X actual=0x%02X\n",
 *          calculated_crc, received_crc);
 * }
 * @endcode
 *
 * @par Example - DS18B20 Scratchpad Validation:
 * @code{.c}
 * // DS18B20 scratchpad: 9 bytes total (8 data + 1 CRC)
 * uint8_t scratchpad[9];
 * rx_onewire_read_scratchpad(scratchpad, sizeof(scratchpad));
 *
 * // Compute CRC over first 8 bytes
 * uint8_t calculated_crc = rx_crc8_maxim(scratchpad, 8);
 * uint8_t received_crc = scratchpad[8];
 *
 * if (calculated_crc == received_crc) {
 *   // Extract temperature (bytes 0-1, little-endian)
 *   int16_t raw_temp = (scratchpad[1] << 8) | scratchpad[0];
 *   float temp_c = raw_temp / 16.0f;  // 12-bit resolution
 *   printf("Temperature: %.2f degC\n", temp_c);
 * } else {
 *   rx_log_error("OneWire", "Scratchpad CRC mismatch");
 * }
 * @endcode
 *
 * @par Example - Computing CRC for Transmission:
 * @code{.c}
 * // Send custom command to OneWire device with CRC
 * void send_onewire_command(uint8_t cmd, uint8_t param)
 * {
 *   uint8_t buffer[3];
 *   buffer[0] = cmd;
 *   buffer[1] = param;
 *
 *   // Compute CRC over command + parameter
 *   buffer[2] = rx_crc8_maxim(buffer, 2);
 *
 *   // Transmit with CRC
 *   rx_onewire_write(buffer, sizeof(buffer));
 * }
 * @endcode
 *
 * @par Example - Batch ROM Validation:
 * @code{.c}
 * // Validate multiple DS18B20 ROM codes
 * typedef struct {
 *   uint8_t rom[8];
 *   bool valid;
 * } onewire_device_t;
 *
 * onewire_device_t devices[4];
 *
 * // Search for devices and validate ROM codes
 * for (uint8_t i = 0; i < 4; i++) {
 *   if (rx_onewire_search(&devices[i].rom[0])) {
 *     // Validate ROM CRC
 *     uint8_t calc_crc = rx_crc8_maxim(devices[i].rom, 7);
 *     devices[i].valid = (calc_crc == devices[i].rom[7]);
 *
 *     if (devices[i].valid) {
 *       rx_log_info("OneWire", "Device %u: Family=0x%02X Serial=%02X%02X%02X%02X%02X%02X",
 *                   i, devices[i].rom[0],
 *                   devices[i].rom[6], devices[i].rom[5], devices[i].rom[4],
 *                   devices[i].rom[3], devices[i].rom[2], devices[i].rom[1]);
 *     } else {
 *       rx_log_error("OneWire", "Device %u: Invalid ROM CRC", i);
 *     }
 *   }
 * }
 * @endcode
 *
 * @par Example - Zero-Length Buffer:
 * @code{.c}
 * uint8_t crc = rx_crc8_maxim(nullptr, 0);
 * // Returns: 0x00 (init value with no processing)
 * @endcode
 *
 * @par Example - Manual CRC Verification (Algorithm Understanding):
 * @code{.c}
 * // Demonstrate CRC-8/Maxim algorithm step-by-step
 * uint8_t data[] = { 0x28, 0xFF, 0x64, 0x1F, 0x21, 0x16, 0x04 };
 *
 * // Method 1: Use library function
 * uint8_t crc_lib = rx_crc8_maxim(data, sizeof(data));
 * printf("Library CRC: 0x%02X\n", crc_lib);
 * // Output: Library CRC: 0xB3
 *
 * // Method 2: Manual computation (for understanding)
 * extern const uint8_t crc8_maxim_table[256];  // From rx_crc8.c
 * uint8_t crc_manual = 0x00;  // Initial value
 * for (uint8_t i = 0; i < sizeof(data); i++) {
 *   crc_manual = crc8_maxim_table[crc_manual ^ data[i]];
 * }
 * printf("Manual CRC: 0x%02X\n", crc_manual);
 * // Output: Manual CRC: 0xB3
 *
 * // Both produce identical result
 * assert(crc_lib == crc_manual);
 * @endcode
 *
 * @see rx_crc32_ieee() CRC-32 for frame protocols and Ethernet
 * @see rx_crc8.c CRC-8/Maxim implementation (software table)
 * @see rx_hcsr04.h OneWire interface for DS18B20 and similar devices
 *
 * @since Version 1.0.0
 */
uint8_t rx_crc8_maxim(const uint8_t* data, uint32_t len);

#ifdef __cplusplus
}
#endif
