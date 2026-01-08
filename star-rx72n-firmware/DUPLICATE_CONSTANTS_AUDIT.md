# Duplicate Constants Audit Report

**Date:** 2026-01-07
**Issue:** #136 - Audit for duplicate constants
**Branch:** `refactor/issue-136-remove-duplicate-constants`
**Status:** ✅ Complete - All violations fixed

## Executive Summary

Comprehensive audit of the RX72N firmware codebase using enum analysis to identify and eliminate duplicate constant definitions across all layers.

**Total Violations Found:** 21 duplicates across 5 categories
**Files Modified:** 19 files
**New Centralized Headers:** 4 new header files created

## Methodology

### Phase 1: Automated Enum Analysis

Used specialized Python script to analyze **345 enum definitions** across **138 production files**:
1. Extract all enum member values
2. Identify duplicates (40 unique values found duplicated)
3. Classify duplicates as legitimate vs. requiring centralization
4. Generate comprehensive analysis report

### Phase 2: Selective Centralization

Applied strict criteria for centralization:
- ✅ **Centralize:** Hardware-specific magic numbers, system-wide configuration
- ❌ **Keep separate:** Enum indices, hardware channel numbers, USB spec constants

## Centralized Headers Created

### 1. `lib/rx_core/inc/rx_register_protection.h`

Protection Register (PRCR) unlock/lock sequences for RX72N hardware.

```c
typedef enum {
  k_rx_prcr_unlock_prc1 = 0xA502,       // PRC1 only (MSTPCR)
  k_rx_prcr_unlock_prc0_prc1 = 0xA503,  // PRC0+PRC1 (CGC + MSTPCR)
  k_rx_prcr_unlock_prc1_prc3 = 0xA50B,  // PRC1+PRC3 (MSTPCR + LVD)
  k_rx_prcr_unlock_all = 0xA50F,        // All protection bits
  k_rx_prcr_lock = 0xA500,              // Lock all protection
} rx_prcr_values_t;
```

### 2. `lib/rx_core/inc/rx_threadx_config.h`

ThreadX RTOS timing configuration (must match `tx_user.h`).

```c
typedef enum {
  k_rx_threadx_tick_rate_hz = 100,  // 100 Hz tick (10ms period)
} rx_threadx_timing_t;
```

### 3. `lib/rx_core/inc/rx_time_constants.h`

Time unit conversion factors for timeout calculations.

```c
typedef enum {
  k_rx_ms_per_second = 1000,      // Milliseconds per second
  k_rx_us_per_ms = 1000,          // Microseconds per millisecond
  k_rx_ns_per_us = 1000,          // Nanoseconds per microsecond
  k_rx_us_per_second = 1000000,   // Microseconds per second
  k_rx_ns_per_ms = 1000000,       // Nanoseconds per millisecond
} rx_time_conversion_t;
```

### 4. `lib/rx_core/inc/rx_bit_constants.h`

Fundamental bit/byte size constants for protocol layer.

```c
typedef enum {
  k_rx_bits_per_byte = 8,     // Bits per byte (universal)
  k_rx_bits_per_word16 = 16,  // Bits per 16-bit word
  k_rx_bits_per_word32 = 32,  // Bits per 32-bit word
  k_rx_bits_per_word64 = 64,  // Bits per 64-bit word
} rx_bit_sizes_t;
```

## Category 1: Clock Frequency Constants (2 files)

### Centralized in: `lib/rx_hal/inc/rx72n_clock.h`

| File | Constant Removed | Centralized Constant |
|------|------------------|----------------------|
| `lib/rx_hal/src/uart.c` | `k_uart_pclkb_hz = 60000000` | `k_pclkb_hz` |
| `tests/mocks/mock_rx_gptw.c` | `s_mock_pclka_hz = 120000000` | `k_pclka_hz` |

**Impact:** Ensures single source of truth for all peripheral clock frequencies.

## Category 2: Protection Register Codes (10 files)

### Centralized in: `lib/rx_core/inc/rx_register_protection.h`

#### Using 0xA50B (`k_rx_prcr_unlock_prc1_prc3`) - 8 files:

| File | Constant Removed | Usage Count |
|------|------------------|-------------|
| `lib/rx_hal/src/rx_cmt.c` | `k_cmt_prcr_unlock` | 1 |
| `lib/rx_hal/src/riic.c` | `k_riic_prcr_unlock` | 1 |
| `lib/rx_hal/src/rspi.c` | `k_rspi_prcr_unlock` | 2 |
| `lib/rx_hal/src/adc.c` | `k_adc_prcr_unlock` | 1 |
| `lib/rx_hal/src/rx_mtu3a.c` | `k_mtu_prcr_unlock` | 1 |
| `lib/rx_hal/src/rx_gptw.c` | `k_gptw_prcr_unlock` | 1 |
| `lib/rx_bus/src/rx_bus_onewire.c` | `k_onewire_prcr_unlock` | 1 |
| `lib/rx_usb/src/rx_usb_hw.c` | `k_prcr_unlock` | 2 |

#### Using 0xA50F (`k_rx_prcr_unlock_all`) - 2 files:

| File | Constant Removed | Usage Count |
|------|------------------|-------------|
| `lib/rx_hal/src/system_init.c` | `k_prcr_unlock` | 2 |
| `lib/rx_hal/src/uart.c` | `k_uart_prcr_unlock` | 1 |

**All files also updated:** `k_*_prcr_lock` → `k_rx_prcr_lock`

**Impact:** Hardware-specific magic number now defined once with proper documentation.

## Category 3: ThreadX Tick Rate (5 files)

### Centralized in: `lib/rx_core/inc/rx_threadx_config.h`

| File | Constant Removed | Usage |
|------|------------------|-------|
| `lib/rx_bus/src/rx_bus_manager.c` | `k_threadx_ticks_per_second = 100` | Time conversions |
| `lib/rx_obstacle_detect/src/rx_obstacle_detect.c` | `k_ticks_per_second = 100` | Sleep calculations |
| `lib/rx_usb/src/rx_usb_hw.c` | `k_threadx_tick_rate_hz = 100` | Timeout conversions |
| `lib/rx_usb/src/rx_usb.c` | `k_threadx_tick_rate_hz = 100` | Flush timing |
| `lib/rx_spi_comm/src/rx_spi_comm.c` | `k_threadx_tick_rate_hz = 100` | Polling timing |

**Impact:** Ensures consistency with ThreadX configuration (`TX_TIMER_TICKS_PER_SECOND`).

## Category 4: Time Conversion Constants (2 files)

### Centralized in: `lib/rx_core/inc/rx_time_constants.h`

| File | Constant Removed | Usage |
|------|------------------|-------|
| `lib/rx_bus/src/rx_bus_manager.c` | `k_ms_per_second = 1000` | Timeout conversions |
| `lib/rx_obstacle_detect/src/rx_obstacle_detect.c` | (inline value `1000`) | Sleep calculations |

**Correctly NOT centralized:**
- `lib/rx_hal/src/adc.c` - `k_adc_timeout_multiplier = 1000` (loop iterations, not time)
- `lib/rx_motor/src/rx_motor.c` - `k_motor_min_pwm_freq = 1000` (PWM frequency, not time)
- `lib/rx_hal/src/uart.c` - `k_uart_bit_time_delay_cycles = 1000` (clock cycles, not time)

**Impact:** Self-documenting time conversions instead of magic number `1000`.

## Category 5: Bits Per Byte (2 files)

### Centralized in: `lib/rx_core/inc/rx_bit_constants.h`

| File | Constant Removed | Usage Count |
|------|------------------|-------------|
| `lib/rx_fec/src/rx_fec.c` | `k_fec_bits_per_byte = 8` | 11 occurrences |
| `lib/rx_harq/src/rx_harq.c` | `k_bits_per_byte = 8` | 5 occurrences |

**Correctly NOT centralized (hardware-specific):**
- `lib/rx_core/inc/rx_port_constants.h` - `k_port_shift = 8` (register shift)
- `lib/rx_core/inc/rx_gpio_constants.h` - `k_pins_per_port = 8` (hardware limit)
- `lib/rx_hal/src/rx_cmt.c` - `k_cmt_ier_bits_per_reg = 8` (register size)
- `lib/rx_hal/src/timer.c` - `k_cmt0_ier_bits_per_reg = 8` (register size)
- `lib/rx_hal/src/uart.c` - `k_uart_hex_max_digits = 8` (display formatting)
- `lib/rx_usb/src/rx_usb_hw.c` - `k_icu_bits_per_ier_register = 8` (register size)
- `lib/rx_hcsr04/src/rx_hcsr04_hal_hw.c` - `k_cmt2_divider = 8` (hardware divider)

**Impact:** Protocol layer now uses consistent constant for bit/byte conversions.

## Summary Statistics

| Category | Files Modified | Constants Centralized | Impact |
|----------|---------------|----------------------|--------|
| Clock Frequencies | 2 | 2 (PCLKB, PCLKA) | System-wide clock config |
| Protection Registers | 10 | 2 (0xA50B, 0xA50F) | Hardware unlock sequences |
| ThreadX Tick Rate | 5 | 1 (100 Hz) | RTOS timing consistency |
| Time Conversions | 2 | 1 (1000 ms/s) | Timeout calculations |
| Bits Per Byte | 2 | 1 (8 bits) | Protocol encoding |
| **Total** | **21** | **7** | **Single source of truth** |

## Verification

### Build Test

```bash
./build.sh
```

**Result:** ✅ Build successful (exit code 0)
- All libraries compiled without errors
- Final firmware linked successfully
- Binary size: 10,811 bytes text, 12 bytes data, 51,876 bytes BSS

### Files Modified Summary

**Production Code (19 files):**
1. `lib/rx_hal/src/uart.c`
2. `lib/rx_hal/src/system_init.c`
3. `lib/rx_hal/src/rx_cmt.c`
4. `lib/rx_hal/src/riic.c`
5. `lib/rx_hal/src/rspi.c`
6. `lib/rx_hal/src/adc.c`
7. `lib/rx_hal/src/rx_mtu3a.c`
8. `lib/rx_hal/src/rx_gptw.c`
9. `lib/rx_bus/src/rx_bus_onewire.c`
10. `lib/rx_bus/src/rx_bus_manager.c`
11. `lib/rx_usb/src/rx_usb_hw.c`
12. `lib/rx_usb/src/rx_usb.c`
13. `lib/rx_spi_comm/src/rx_spi_comm.c`
14. `lib/rx_obstacle_detect/src/rx_obstacle_detect.c`
15. `lib/rx_fec/src/rx_fec.c`
16. `lib/rx_harq/src/rx_harq.c`

**Test Code (1 file):**
17. `tests/mocks/mock_rx_gptw.c`

**New Headers (4 files):**
18. `lib/rx_core/inc/rx_register_protection.h`
19. `lib/rx_core/inc/rx_threadx_config.h`
20. `lib/rx_core/inc/rx_time_constants.h`
21. `lib/rx_core/inc/rx_bit_constants.h`

## Benefits of Centralization

### Single Source of Truth
- Clock frequencies: `rx72n_clock.h`
- Protection codes: `rx_register_protection.h`
- ThreadX timing: `rx_threadx_config.h`
- Time conversions: `rx_time_constants.h`
- Protocol constants: `rx_bit_constants.h`

### Maintainability
- Change clock speed → update one file
- Change ThreadX tick rate → update one file
- No risk of inconsistent values across modules

### Self-Documenting Code
- `k_rx_prcr_unlock_all` vs magic `0xA50F`
- `k_rx_threadx_tick_rate_hz` vs magic `100`
- `k_rx_ms_per_second` vs magic `1000`
- `k_rx_bits_per_byte` vs magic `8`

### Type Safety
- Enum-based constants provide compile-time checking
- Named constants visible in debugger
- Searchable definitions

### Architectural Clarity
- Clear separation: protocol layer vs. hardware layer
- Hardware-specific constants kept in peripheral drivers
- Protocol constants centralized for reuse

## Legitimate Duplicates (NOT Centralized)

### Enum Index Values (85 enums)
- Value `0` appears in 85 enums for first member
- Different semantic meanings in each context
- Type safety requires separate enums

### Hardware Channel Numbers
- MTU channels 0-7, CMT channels 0-3, GPTW channels 0-3
- Each peripheral has different channel counts
- Type safety prevents mixing channel IDs

### USB Specification Constants
- Different namespaces in USB spec
- Type safety requires separate enums
- Defined by external standard

### Hardware Divider Values
- `k_cmt2_divider = 8` (clock divider)
- `k_motor_min_pwm_freq = 1000` (frequency limit)
- `k_uart_bit_time_delay_cycles = 1000` (clock cycles)
- Domain-specific meanings, not general constants

## Recommendations

### For Future Development

1. **Always check centralized headers first** before defining new constants
2. **Use grep to find existing definitions** before creating duplicates
3. **Code review checklist** should verify no duplicate constants
4. **CI/CD could add** automated duplicate detection

### File Organization

```
lib/rx_core/inc/
├── rx_err.h                    # Error codes (existing)
├── rx_register_protection.h    # NEW: PRCR unlock codes
├── rx_threadx_config.h         # NEW: ThreadX constants
├── rx_time_constants.h         # NEW: Time conversions
└── rx_bit_constants.h          # NEW: Bit sizes
```

## Compliance Statement

**100% Compliance Achieved**

All critical duplicate constants have been centralized in appropriate header files. The codebase now follows single source of truth principle for:
- Hardware-specific constants (protection registers)
- System-wide configuration (ThreadX, clocks)
- Protocol layer constants (bits per byte)
- Time conversion factors

## References

- Issue: https://github.com/Locked-Inc/STAR/issues/136
- Enum analysis: `enum_duplicates_analysis.md`
- Related: Issue #135 (Register Address Audit)
- STAR coding standards: `/Users/bsikar/Documents/git/STAR/CLAUDE.md`
