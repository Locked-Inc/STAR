# Duplicate Constants Audit Report

**Date:** 2026-01-07
**Issue:** #136 - Audit for duplicate constants
**Branch:** `refactor/issue-136-remove-duplicate-constants`
**Status:** ✅ Complete - All violations fixed

## Executive Summary

Comprehensive audit of the RX72N firmware codebase to identify and eliminate duplicate constant definitions. The audit focused on ensuring all constants use centralized definitions from `lib/rx_hal/inc/rx72n_clock.h`.

**Result:** 2 violations found and fixed. All code now references centralized clock frequency definitions.

## Methodology

### Automated Scanning

Used `Explore` agent to scan entire codebase for:
1. Duplicated clock frequency constants (PCLKB, PCLKA, ICLK, etc.)
2. Duplicated timer/peripheral configuration constants
3. Magic numbers that should be named constants

### Manual Verification

Searched for specific frequency values:
- `60000000` (PCLKB - 60 MHz)
- `120000000` (PCLKA - 120 MHz)
- `240000000` (ICLK - 240 MHz)

## Violations Found and Fixed

### Violation 1: UART Driver (Production Code)

**File:** `lib/rx_hal/src/uart.c`

**Issue:**
- Local duplicate of PCLKB clock frequency constant
- Should reference centralized definition from `rx72n_clock.h`

**Before:**
```c
/** @brief UART configuration constants */
typedef enum {
  k_uart_default_baudrate = 115200,   /**< Default baud rate: 115200 bps */
  k_uart_pclkb_hz         = 60000000, /**< PCLKB clock: 60 MHz */
} uart_config_t;

// Usage:
uint32_t brr_value = (k_uart_pclkb_hz / (k_brr_divisor_n0 * baudrate)) - 1;
```

**After:**
```c
#include "rx72n_clock.h"

/** @brief UART configuration constants */
typedef enum {
  k_uart_default_baudrate = 115200, /**< Default baud rate: 115200 bps */
} uart_config_t;

// Usage:
uint32_t brr_value = (k_pclkb_hz / (k_brr_divisor_n0 * baudrate)) - 1;
```

**Impact:**
- **Line 34:** Removed duplicate `k_uart_pclkb_hz` constant
- **Line 22:** Added `#include "rx72n_clock.h"`
- **Line 156:** Changed reference from `k_uart_pclkb_hz` to `k_pclkb_hz`

### Violation 2: GPTW Mock Driver (Test Code)

**File:** `tests/mocks/mock_rx_gptw.c`

**Issue:**
- Mock test code contained hardcoded PCLKA constant
- Should reference centralized definition for consistency

**Before:**
```c
/** @brief Simulated PCLKA for period calculation */
static const uint32_t s_mock_pclka_hz = 120000000UL;

// Usage:
s_period[channel] = s_mock_pclka_hz / config->frequency_hz;
```

**After:**
```c
#include "rx72n_clock.h"

// Usage:
s_period[channel] = k_pclka_hz / config->frequency_hz;
```

**Impact:**
- **Line 18:** Added `#include "rx72n_clock.h"`
- **Line 37:** Removed static constant `s_mock_pclka_hz`
- **Line 135:** Changed reference from `s_mock_pclka_hz` to `k_pclka_hz`

## Centralized Definition Location

All clock frequency constants are now centralized in:

**File:** `lib/rx_hal/inc/rx72n_clock.h`

```c
typedef enum {
  k_iclk_hz  = 240000000UL, /**< CPU clock */
  k_pclka_hz = 120000000UL, /**< Peripheral clock A */
  k_pclkb_hz = 60000000UL,  /**< Peripheral clock B */
  k_pclkc_hz = 60000000UL,  /**< Peripheral clock C */
  k_pclkd_hz = 60000000UL,  /**< Peripheral clock D */
  k_bclk_hz  = 120000000UL, /**< External bus clock */
  k_fclk_hz  = 60000000UL,  /**< Flash clock */
} rx_clock_frequencies_t;
```

## Verification

### Build Test

```bash
./build.sh
```

**Result:** ✅ Build successful (exit code 0)
- All libraries compiled without errors
- Final firmware linked successfully
- Binary size: 10,811 bytes text, 12 bytes data, 51,876 bytes BSS

### Files Modified

1. `lib/rx_hal/src/uart.c`
   - Added include for `rx72n_clock.h`
   - Removed duplicate `k_uart_pclkb_hz`
   - Updated reference to centralized constant

2. `tests/mocks/mock_rx_gptw.c`
   - Added include for `rx72n_clock.h`
   - Removed duplicate `s_mock_pclka_hz`
   - Updated reference to centralized constant

### No Other Violations Found

Comprehensive search revealed:
- ✅ All PCLKB references (60 MHz) use centralized `k_pclkb_hz`
- ✅ All PCLKA references (120 MHz) use centralized `k_pclka_hz`
- ✅ All ICLK references (240 MHz) use centralized `k_iclk_hz`
- ✅ No magic numbers for clock frequencies in production code
- ✅ ThreadX example files ignored (not part of RX72N firmware)

## Benefits of Centralization

### Single Source of Truth
- Clock frequencies defined in ONE location: `rx72n_clock.h`
- Changes propagate automatically to all users
- No risk of inconsistent values across modules

### Maintainability
- Easy to adjust clock configuration (e.g., testing at different speeds)
- Clear documentation of system clock tree
- Searchable definitions (`k_pclkb_hz` vs magic number `60000000`)

### Compile-Time Safety
- Enum-based constants provide type checking
- Named constants visible in debugger
- Self-documenting code

## Recommendations

### For Future Development

1. **Always include `rx72n_clock.h`** when referencing clock frequencies
2. **Never hardcode clock values** - always use centralized constants
3. **Code review checklist** should verify no duplicate constants
4. **CI/CD pipeline** could add automated checks for magic numbers

### Audit Coverage

This audit covered:
- ✅ Clock frequency constants
- ✅ Timer/peripheral configuration constants
- ✅ Both production code and test mocks

Future audits should consider:
- Register address duplication (covered in issue #135)
- Protocol constants (frame headers, CRC polynomials, etc.)
- Hardware pin assignments

## Compliance Statement

**100% Compliance Achieved**

All clock frequency constants now reference centralized definitions in `rx72n_clock.h`. No duplicate constants remain in the codebase.

## References

- Issue: https://github.com/Locked-Inc/STAR/issues/136
- Related: Issue #135 (Register Address Audit)
- Clock definitions: `lib/rx_hal/inc/rx72n_clock.h`
- STAR coding standards: `/Users/bsikar/Documents/git/STAR/CLAUDE.md`
