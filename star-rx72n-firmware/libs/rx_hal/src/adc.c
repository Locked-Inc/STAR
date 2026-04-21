/**
 * @file adc.c
 * @brief ADC Driver for RX72N S12ADFa - 12-bit Analog-to-Digital Conversion
 *
 * @details
 * ## Overview
 *
 * This file provides the hardware abstraction layer (HAL) for the S12ADFa
 * (12-bit Analog-to-Digital Converter) peripheral on the Renesas RX72N
 * microcontroller. The S12ADFa supports dual ADC units with 8 channels each,
 * configurable resolution (8/10/12-bit), and single-scan conversion mode.
 *
 * ## Hardware Architecture
 *
 * @dot
 * digraph adc_architecture {
 *   rankdir=LR;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_analog {
 *     label="Analog Inputs";
 *     style=dashed;
 *     an0 [label="AN0-AN7\n(S12AD0)"];
 *     an1 [label="AN16-AN23\n(S12AD1)"];
 *   }
 *
 *   subgraph cluster_adc {
 *     label="S12ADFa Peripheral";
 *     style=filled;
 *     fillcolor=lightblue;
 *     mux [label="Channel MUX"];
 *     sar [label="12-bit SAR\nConverter"];
 *     regs [label="ADDRn\nData Registers"];
 *   }
 *
 *   subgraph cluster_sw {
 *     label="Software (THIS FILE)";
 *     style=dashed;
 *     init [label="adc_init()"];
 *     read [label="adc_read()"];
 *     voltage [label="adc_read_voltage_mv()"];
 *   }
 *
 *   an0 -> mux;
 *   an1 -> mux;
 *   mux -> sar;
 *   sar -> regs;
 *   init -> regs [label="configure"];
 *   read -> regs [label="trigger & read"];
 *   voltage -> read [label="calls"];
 * }
 * @enddot
 *
 * ## S12ADFa Features Used
 *
 * | Feature | Status | Notes |
 * |---------|--------|-------|
 * | Dual ADC units (S12AD0, S12AD1) | [OK] | Both supported |
 * | 12/10/8-bit resolution | [OK] | Configurable per unit |
 * | Single-scan mode | [OK] | Software-triggered |
 * | Continuous scan mode | [X] | Not implemented |
 * | Group scan mode | [X] | Not implemented |
 * | A/D interrupts | [X] | Polling only |
 * | Self-diagnosis | [X] | Not implemented |
 * | Double trigger | [X] | Not implemented |
 *
 * ## Register Access Pattern
 *
 * @msc
 * Application, ADC_HAL, MSTPCRA, ADCSR, ADDRn;
 *
 * --- [label="Initialization"];
 * Application => ADC_HAL [label="adc_init(unit=0, ch=2, bits=12)"];
 * ADC_HAL => MSTPCRA [label="Clear bit 17 (enable S12AD0)"];
 * ADC_HAL => ADCSR [label="Configure resolution"];
 * ADC_HAL => Application [label="k_rx_ok"];
 *
 * --- [label="Conversion"];
 * Application => ADC_HAL [label="adc_read(unit=0, ch=2, &value)"];
 * ADC_HAL => ADCSR [label="Set ADST bit (start)"];
 * ADC_HAL box ADC_HAL [label="Poll ADST until clear"];
 * ADC_HAL => ADDRn [label="Read ADDR2"];
 * ADC_HAL => Application [label="k_rx_ok, value=raw"];
 * @endmsc
 *
 * ## Conversion Timing
 *
 * | Resolution | Conversion Time | Sample Rate (max) |
 * |------------|-----------------|-------------------|
 * | 12-bit | ~1.0 us | ~1 MHz |
 * | 10-bit | ~0.9 us | ~1.1 MHz |
 * | 8-bit | ~0.8 us | ~1.25 MHz |
 *
 * Note: Times are approximate at PCLKB = 60 MHz.
 *
 * ## Voltage Calculation
 *
 * The adc_read_voltage_mv() function converts raw ADC values to millivolts:
 *
 * @f[
 *   V_{mV} = \frac{V_{raw} \times V_{ref}}{2^{bits} - 1}
 * @f]
 *
 * Where:
 * - @f$ V_{raw} @f$ = Raw ADC value (0 to 2^bits-1)
 * - @f$ V_{ref} @f$ = Reference voltage (3300 mV)
 * - @f$ bits @f$ = Resolution (8, 10, or 12)
 *
 * ## Thread Safety
 *
 * | Function | Thread-Safe | Notes |
 * |----------|-------------|-------|
 * | adc_init() | No | Call during initialization only |
 * | adc_read() | No | Caller must synchronize |
 * | adc_read_voltage_mv() | No | Caller must synchronize |
 *
 * Each ADC unit can be accessed independently, but concurrent access to the
 * same unit requires external synchronization.
 *
 * ## Memory Usage
 *
 * | Section | Size | Contents |
 * |---------|------|----------|
 * | .text | ~800 bytes | Function code |
 * | .rodata | ~80 bytes | Log strings, tag |
 * | .bss | 6 bytes | s_adc_unit_initialized[2], s_adc_unit_resolution[2] |
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation |
 * |------|--------|----------------|
 * | 1. Simplify control flow | [OK] | No goto, setjmp, recursion |
 * | 2. Fixed loop bounds | [OK] | Timeout loop has fixed maximum |
 * | 3. No dynamic memory | [OK] | Static arrays only |
 * | 4. Functions < 60 lines | [OK] | Helper functions for decomposition |
 * | 5. Use assertions | [OK] | RX_CHECK_NULL_PTR, RX_CHECK_RANGE_TAG |
 * | 6. Data at smallest scope | [OK] | Local variables, static helpers |
 * | 7. Check return values | [OK] | RX_RETURN_ON_ERROR macro |
 * | 8. Limit preprocessor | [OK] | Only includes |
 * | 9. Restrict pointers | [OK] | Hardware register pointers only |
 * | 10. Compile warnings | [OK] | -Wall -Wextra -Werror clean |
 *
 * @see rx72n_adc_regs.h ADC register definitions
 * @see hardware.h ADC unit and channel types
 * @see rx_register_protection.h Module stop register access
 *
 * @author Locked, Inc.
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#ifdef __RX__

#include <string.h>

#include "hardware.h"
#include "rx72n_regs.h"
#include "rx_register_protection.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum adc_constants_t
 * @brief ADC hardware configuration limits
 *
 * @details
 * Defines the hardware limits and configuration constants for the S12ADFa
 * peripheral on the RX72N microcontroller. These values are derived from
 * the RX72N hardware manual Chapter 41 (S12ADFa).
 *
 * @see internal_get_adc_base() Uses k_adc_max_units for validation
 * @see s_adc_unit_initialized Array sized by k_adc_max_units
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_adc_max_units    = 2,   /**< Number of ADC units (S12AD0, S12AD1) */
  k_adc_max_channels = 8,   /**< Channels per unit (0-7 for S12AD0, 16-23 for S12AD1) */
  k_adc_timeout_ms   = 100, /**< Maximum wait time for conversion completion (ms) */
} adc_constants_t;

/**
 * @enum adc_bit_constants_t
 * @brief Bit manipulation constants for ADC register operations
 *
 * @details
 * Single-bit value used for shift operations when setting/clearing
 * individual bits in ADC control and channel selection registers.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_adc_bit_one = 1U, /**< Single-bit value for shift operations (1 << n) */
} adc_bit_constants_t;

/**
 * @enum adc_module_stop_bits_t
 * @brief ADC module stop bit positions in MSTPCRA register
 *
 * @details
 * Defines the bit positions in the MSTPCRA (Module Stop Control Register A)
 * that control the clock supply to the S12AD0 and S12AD1 modules. These bits
 * must be cleared (clock enabled) before accessing ADC registers.
 *
 * ## Module Stop Control
 *
 * | Bit | Module | 0 = Clock Enabled | 1 = Clock Stopped |
 * |-----|--------|-------------------|-------------------|
 * | 17 | S12AD0 | ADC unit 0 active | ADC unit 0 stopped |
 * | 16 | S12AD1 | ADC unit 1 active | ADC unit 1 stopped |
 *
 * @note Accessing ADC registers while module clock is stopped causes bus error
 * @note PRCR register must be unlocked before modifying MSTPCRA
 *
 * @see internal_configure_adc_unit() Uses these to enable module clock
 * @see rx_register_protection.h PRCR unlock/lock functions
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_adc_mstpra_s12ad1 = 16, /**< MSTPCRA bit 16: S12AD1 module stop control */
  k_adc_mstpra_s12ad0 = 17, /**< MSTPCRA bit 17: S12AD0 module stop control */
} adc_module_stop_bits_t;

/**
 * @enum adc_adcer_bits_t
 * @brief ADC Control Extended Register (ADCER) bit definitions
 *
 * @details
 * Defines the bit fields for the ADCER (A/D Control Extended Register) which
 * controls ADC resolution and data format. The ADPRC field (bits 1:0) sets
 * the A/D conversion resolution.
 *
 * ## ADCER Register Layout (bits 1:0 - ADPRC)
 *
 * | ADPRC[2:1] | Resolution | Max Value | Conversion Time |
 * |------------|------------|-----------|-----------------|
 * | 00 | 12-bit | 4095 | ~1.0 us |
 * | 01 | 10-bit | 1023 | ~0.9 us |
 * | 10 | 8-bit | 255 | ~0.8 us |
 * | 11 | Reserved | - | - |
 *
 * @see internal_configure_adc_unit() Uses these to set resolution
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_adc_adcer_adprc_mask =
    3U, /**< ADPRC field mask (2-bit, unshifted) per RX72N HW manual R01UH0824EJ0111 Section 56.2.13 */
  k_adc_adcer_adprc_shift = 1U, /**< ADPRC field bit position: bits 2:1 of ADCER */
  k_adc_adcer_adprc_12bit = 0U, /**< ADPRC = 00: 12-bit resolution (default) */
  k_adc_adcer_adprc_10bit = 1U, /**< ADPRC = 01: 10-bit resolution */
  k_adc_adcer_adprc_8bit  = 2U, /**< ADPRC = 10: 8-bit resolution */
} adc_adcer_bits_t;

/**
 * @enum adc_adcsr_bits_t
 * @brief ADC Control/Status Register (ADCSR) bit definitions
 *
 * @details
 * Defines bit values for the ADCSR (A/D Control Status Register) which
 * controls conversion start and indicates conversion status.
 *
 * ## ADCSR Bit 15 (ADST) - A/D Conversion Start
 *
 * | Value | Read Meaning | Write Meaning |
 * |-------|--------------|---------------|
 * | 0 | Conversion stopped/complete | (No effect) |
 * | 1 | Conversion in progress | Start conversion |
 *
 * @see adc_read() Uses ADST to trigger and poll conversion
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_adc_adcsr_idle = 0U,     /**< Idle state: ADST=0 (conversion complete) */
  k_adc_adcsr_adst = 32768U, /**< Start bit: ADST=1 (bit 15 = 0x8000 = 32768) */
} adc_adcsr_bits_t;

/**
 * @enum adc_channel_boundaries_t
 * @brief ADC channel selection register boundaries
 *
 * @details
 * Defines the channel boundaries for ADANSA0 and ADANSA1 registers.
 * Channels 0-15 are selected via ADANSA0, channels 16+ via ADANSA1.
 *
 * For S12AD0: Physical channels AN0-AN7 (channels 0-7)
 * For S12AD1: Physical channels AN16-AN23 (channels 0-7 mapped)
 *
 * @see adc_init() Uses these for channel enable logic
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_adc_channel_adansa0_max  = 15, /**< Maximum channel for ADANSA0 register */
  k_adc_channel_adansa1_base = 16, /**< Base channel for ADANSA1 register */
} adc_channel_boundaries_t;

/* ADC channel indices (k_adc_channel_0 through k_adc_channel_7) defined in hardware.h */

/**
 * @enum adc_misc_constants_t
 * @brief ADC timeout and voltage reference constants
 *
 * @details
 * Miscellaneous constants for timeout handling and voltage calculations.
 *
 * ## Timeout Calculation
 *
 * Total timeout iterations = k_adc_timeout_ms * k_adc_timeout_multiplier
 *                          = 100 * 1000 = 100,000 iterations
 *
 * At ~10 cycles per iteration, this provides ~4ms timeout at 240 MHz.
 *
 * ## Voltage Reference
 *
 * The RX72N STAR board uses 3.3V as the ADC reference voltage (AVCC).
 * The k_adc_reference_voltage_mv constant (3300 mV) is used in the
 * voltage calculation formula.
 *
 * @see adc_read() Uses timeout constants
 * @see adc_read_voltage_mv() Uses reference voltage
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_adc_timeout_multiplier   = 1000, /**< Loop iterations per ms of timeout */
  k_adc_timeout_expired      = 0,    /**< Sentinel value: timeout counter reached zero */
  k_adc_reference_voltage_mv = 3300, /**< ADC reference voltage: 3.3V = 3300 mV */
} adc_misc_constants_t;

/**
 * @enum adc_raw_value_t
 * @brief Raw ADC value reset/default constants
 *
 * @details
 * Default raw value used to initialize the raw_value variable before a
 * conversion result is written. Using a named constant makes the intent
 * explicit and avoids magic numeric literals.
 *
 * @see adc_read_voltage_mv() Initializes raw_value with k_adc_raw_value_reset
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_adc_raw_value_reset = 0, /**< Reset/default raw ADC value before conversion */
} adc_raw_value_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/**
 * @var s_tag
 * @brief Log tag for ADC module messages
 * @details Used with rx_log_* functions for consistent message tagging.
 */
static const char* s_tag = "ADC";

/**
 * @var s_adc_unit_initialized
 * @brief Tracks initialization state for each ADC unit
 *
 * @details
 * Array indexed by ADC unit number (0 or 1). Set to true when adc_init()
 * successfully initializes the unit. Used to prevent double-initialization
 * and to validate that a unit is ready before reading.
 *
 * | Index | Unit | Default |
 * |-------|------|---------|
 * | 0 | S12AD0 | false |
 * | 1 | S12AD1 | false |
 *
 * @warning NOT thread-safe - initialization should be done from single thread
 *
 * @see adc_init() Sets to true on successful initialization
 * @see adc_read() Checks before allowing read operation
 */
static bool s_adc_unit_initialized[k_adc_max_units] = {false, false};

/**
 * @var s_adc_unit_resolution
 * @brief Stores configured resolution for each ADC unit
 *
 * @details
 * Array indexed by ADC unit number (0 or 1). Records the resolution
 * configured during first adc_init() call for each unit. Subsequent
 * init calls must use the same resolution (enforced at runtime).
 *
 * | Index | Unit | Default | Valid Values |
 * |-------|------|---------|--------------|
 * | 0 | S12AD0 | 12-bit | 8, 10, 12 |
 * | 1 | S12AD1 | 12-bit | 8, 10, 12 |
 *
 * @note Resolution is set only on first init and cannot be changed
 *
 * @see adc_init() Sets on first initialization, validates on subsequent
 * @see adc_read_voltage_mv() Uses for voltage calculation
 */
static adc_resolution_t s_adc_unit_resolution[k_adc_max_units] = {k_adc_resolution_12bit,
                                                                  k_adc_resolution_12bit};

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Get ADC register base address from unit number
 *
 * @details
 * Returns a pointer to the register structure for the specified ADC unit.
 * The RX72N has two ADC units with registers at different base addresses:
 *
 * | Unit | Peripheral | Base Address |
 * |------|------------|--------------|
 * | 0 | S12AD0 | 0x00089000 |
 * | 1 | S12AD1 | 0x00089100 |
 *
 * @param[in] unit ADC unit number
 *                 - k_adc_unit_0: S12AD0
 *                 - k_adc_unit_1: S12AD1
 *
 * @return volatile rx_s12ad_regs_t* Pointer to ADC registers
 * @retval Non-NULL Valid register pointer for unit 0 or 1
 * @retval NULL Invalid unit number (not 0 or 1)
 *
 * @pre None (pure lookup function)
 * @post No state changes
 *
 * @note Thread-safe: Pure function with no shared state
 * @note Performance: O(1), ~10 cycles
 *
 * @see s12ad0() Accessor for S12AD0 registers
 * @see s12ad1() Accessor for S12AD1 registers
 *
 * @since Version 1.0.0
 */
static volatile rx_s12ad_regs_t* internal_get_adc_base(const uint8_t unit)
{
  switch (unit) {
    case k_adc_unit_0: {
      return s12ad0();
    }
    case k_adc_unit_1: {
      return s12ad1();
    }
    default: {
      return nullptr;
    }
  }
}

/**
 * @brief Configure ADC unit control registers and enable module clock
 *
 * @details
 * Performs one-time initialization of an ADC unit:
 * 1. Unlocks PRCR to allow MSTPCRA modification
 * 2. Clears module stop bit in MSTPCRA (enables clock)
 * 3. Locks PRCR
 * 4. Configures ADCER register for desired resolution
 * 5. Marks unit as initialized
 *
 * ## Register Modification Sequence
 *
 * @code
 * PRCR = k_rx_prcr_unlock_prc1_prc3  // Enable MSTPCRA write
 * MSTPCRA &= ~(1 << module_stop_bit) // Clear module stop bit
 * PRCR = k_rx_prcr_lock              // Disable protected writes
 * ADCER = (ADCER & ~mask) | resolution_bits  // Set resolution
 * @endcode
 *
 * @param[in] unit ADC unit number (k_adc_unit_0 or k_adc_unit_1)
 * @param[in] adc Pointer to ADC register structure (must not be NULL)
 * @param[in] bits Resolution setting (8, 10, or 12)
 *
 * @return rx_err_t Configuration result
 * @retval k_rx_ok Configuration successful, unit ready for use
 * @retval k_rx_err_null_ptr adc pointer is nullptr
 * @retval k_rx_err_invalid_arg unit > 1 or invalid resolution
 *
 * @pre adc must be valid pointer from internal_get_adc_base()
 * @pre Unit must not already be initialized
 *
 * @post Module clock enabled (MSTPCRA bit cleared)
 * @post ADCER configured with requested resolution
 * @post s_adc_unit_initialized[unit] = true
 *
 * @note NOT thread-safe: Protected register access
 * @note Performance: ~1 us @ 240 MHz
 * @note PRCR unlock window is minimized
 *
 * @warning Must not be called while ADC is converting
 * @warning PRCR access requires proper unlock sequence
 *
 * @see adc_init() Public API that calls this function
 * @see rx_register_protection.h PRCR constants
 *
 * @since Version 1.0.0
 */
static rx_err_t
internal_configure_adc_unit(const uint8_t unit, volatile rx_s12ad_regs_t* adc, const uint8_t bits)
{
  RX_CHECK_NULL_PTR(adc, s_tag, "ADC register pointer is nullptr");
  RX_CHECK_RANGE_TAG(unit, k_adc_unit_0, k_adc_unit_1, k_rx_err_invalid_arg, s_tag);

  if (bits != k_adc_resolution_8bit && bits != k_adc_resolution_10bit &&
      bits != k_adc_resolution_12bit) {
    rx_log_error(s_tag, "Invalid ADC resolution");
    return k_rx_err_invalid_arg;
  }

  *prcr_reg() = k_rx_prcr_unlock_prc1_prc3;

  if (unit == k_adc_unit_0) {
    system_regs()->mstpcra &= ~((uint32_t)k_adc_bit_one << k_adc_mstpra_s12ad0);
  } else {
    system_regs()->mstpcra &= ~((uint32_t)k_adc_bit_one << k_adc_mstpra_s12ad1);
  }

  *prcr_reg() = k_rx_prcr_lock;

  uint16_t adcer = adc->adcer;
  adcer &= (uint16_t) ~(uint16_t)(k_adc_adcer_adprc_mask << k_adc_adcer_adprc_shift);
  if (bits == k_adc_resolution_8bit) {
    adcer |= (uint16_t)(k_adc_adcer_adprc_8bit << k_adc_adcer_adprc_shift);
  } else if (bits == k_adc_resolution_10bit) {
    adcer |= (uint16_t)(k_adc_adcer_adprc_10bit << k_adc_adcer_adprc_shift);
  } else {
    adcer |= (uint16_t)(k_adc_adcer_adprc_12bit << k_adc_adcer_adprc_shift);
  }
  adc->adcer = adcer;

  s_adc_unit_initialized[unit] = true;
  rx_log_debug(s_tag, "ADC unit initialized");

  return k_rx_ok;
}

/**
 * @brief Validate ADC unit and channel numbers
 *
 * @details
 * Performs range validation for ADC unit and channel parameters before
 * any hardware access. This prevents undefined behavior from invalid
 * register access.
 *
 * ## Validation Rules
 *
 * | Parameter | Valid Range | Error Code |
 * |-----------|-------------|------------|
 * | unit | 0-1 (k_adc_unit_0 to k_adc_unit_1) | k_rx_err_invalid_arg |
 * | channel | 0-7 (k_adc_channel_0 to k_adc_channel_7) | k_rx_err_invalid_arg |
 *
 * @param[in] unit ADC unit number (0 or 1)
 * @param[in] channel ADC channel number (0-7)
 *
 * @return rx_err_t Validation result
 * @retval k_rx_ok Unit and channel are valid
 * @retval k_rx_err_invalid_arg Unit > 1 or channel > 7
 *
 * @pre None (pure validation function)
 * @post No state changes
 *
 * @note Thread-safe: Pure function with no shared state
 * @note Performance: O(1), ~20 cycles
 * @note Logs error message on validation failure
 *
 * @see adc_init() Uses for parameter validation
 * @see adc_read() Uses for parameter validation
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_validate_unit_channel(const uint8_t unit, uint8_t channel)
{
  /* Validate unit */
  if (unit > k_adc_unit_1) {
    rx_log_error(s_tag, "Invalid ADC unit");
    return k_rx_err_invalid_arg;
  }

  /* Validate channel */
  if (channel > k_adc_channel_7) {
    rx_log_error(s_tag, "Invalid ADC channel");
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Read raw ADC value from a specific channel data register
 *
 * @details
 * Helper function that reads the conversion result from the appropriate ADDRn
 * register based on channel number. Extracted from adc_read() to support
 * NASA Power of 10 Rule 4 (functions under 60 lines).
 *
 * ## Channel to Register Mapping
 *
 * | Channel | Register | Offset |
 * |---------|----------|--------|
 * | 0 | ADDR0 | 0x020 |
 * | 1 | ADDR1 | 0x022 |
 * | 2 | ADDR2 | 0x024 |
 * | 3 | ADDR3 | 0x026 |
 * | 4 | ADDR4 | 0x028 |
 * | 5 | ADDR5 | 0x02A |
 * | 6 | ADDR6 | 0x02C |
 * | 7 | ADDR7 | 0x02E |
 *
 * @param[in] adc Pointer to ADC register base (must not be NULL)
 * @param[in] channel ADC channel to read from (0-7)
 * @param[out] value Pointer to store the raw ADC value (must not be NULL)
 *
 * @return rx_err_t Read result
 * @retval k_rx_ok Value read successfully
 * @retval k_rx_err_null_ptr adc or value is nullptr
 * @retval k_rx_err_invalid_arg channel > 7
 *
 * @pre adc must be valid pointer from internal_get_adc_base()
 * @pre ADC conversion must be complete (ADST bit cleared)
 *
 * @post *value contains raw ADC reading (12/10/8-bit depending on config)
 * @post No hardware state changes
 *
 * @note Thread-safe: Read-only register access
 * @note Performance: O(1), ~30 cycles
 *
 * @see adc_read() Calls this after conversion complete
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_read_channel_value(volatile rx_s12ad_regs_t* adc,
                                            const adc_channel_t       channel,
                                            uint16_t*                 value)
{
  RX_CHECK_NULL_PTR(adc, s_tag, "ADC register pointer is nullptr");
  RX_CHECK_NULL_PTR(value, s_tag, "value pointer is nullptr");

  switch (channel) {
    case k_adc_channel_0:
      *value = adc->addr0;
      break;
    case k_adc_channel_1:
      *value = adc->addr1;
      break;
    case k_adc_channel_2:
      *value = adc->addr2;
      break;
    case k_adc_channel_3:
      *value = adc->addr3;
      break;
    case k_adc_channel_4:
      *value = adc->addr4;
      break;
    case k_adc_channel_5:
      *value = adc->addr5;
      break;
    case k_adc_channel_6:
      *value = adc->addr6;
      break;
    case k_adc_channel_7:
      *value = adc->addr7;
      break;
    default:
      rx_log_error(s_tag, "Unsupported channel");
      return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

/**
 * @brief Initialize ADC unit and enable a specific channel
 *
 * @details
 * Configures the S12ADFa peripheral for A/D conversion. On first call for
 * each unit, enables the module clock, configures resolution, and initializes
 * the hardware. Subsequent calls to the same unit enable additional channels.
 *
 * ## Initialization Behavior
 *
 * | Scenario | Action | Resolution | Result |
 * |----------|--------|------------|--------|
 * | First init for unit | Full init | Set to 'bits' | k_rx_ok |
 * | Subsequent init, same bits | Enable channel only | Unchanged | k_rx_ok |
 * | Subsequent init, different bits | Error | Unchanged | k_rx_err_invalid_arg |
 *
 * @param[in] unit ADC unit number (0 for S12AD0, 1 for S12AD1)
 * @param[in] channel ADC channel to enable (0-7)
 * @param[in] bits Resolution (k_adc_resolution_8bit/10bit/12bit)
 *
 * @return rx_err_t Initialization result
 * @retval k_rx_ok Initialization successful
 * @retval k_rx_err_invalid_arg Invalid unit, channel, or resolution
 *
 * @pre None (can be first call for unit)
 * @pre Resolution must match if unit already initialized
 *
 * @post Module clock enabled (if first init)
 * @post Channel enabled in ADANSA register
 * @post Unit marked as initialized
 *
 * @note NOT thread-safe: Call from initialization code only
 * @note Performance: ~1-2 us for first init, ~0.5 us for subsequent
 *
 * @par Example:
 * @code
 * // Initialize ADC unit 0, channel 2 with 12-bit resolution
 * rx_err_t err = adc_init(k_adc_unit_0, k_adc_channel_2, k_adc_resolution_12bit);
 * if (err != k_rx_ok) {
 *   rx_log_error("SENSOR", "ADC init failed: %d", err);
 *   return err;
 * }
 * @endcode
 *
 * @see adc_read() Read after initialization
 * @see adc_read_voltage_mv() Read as voltage
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 preconditions, 3 postconditions
 */
rx_err_t adc_init(const adc_unit_t unit, const adc_channel_t channel, const adc_resolution_t bits)
{
  /* Validate parameters */
  rx_err_t err = internal_validate_unit_channel(unit, channel);
  RX_RETURN_ON_ERROR(err, s_tag, "Unit/channel validation failed");

  /* Validate resolution */
  if (bits != k_adc_resolution_8bit && bits != k_adc_resolution_10bit &&
      bits != k_adc_resolution_12bit) {
    rx_log_error(s_tag, "Invalid resolution (must be 8, 10, or 12 bits)");
    return k_rx_err_invalid_arg;
  }

  /* Get ADC base */
  volatile rx_s12ad_regs_t* const adc = internal_get_adc_base(unit);
  if (adc == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Initialize ADC unit if not already initialized */
  if (!s_adc_unit_initialized[unit]) {
    /* Store resolution for validation on subsequent calls */
    s_adc_unit_resolution[unit] = bits;

    err = internal_configure_adc_unit(unit, adc, bits);
    if (err != k_rx_ok) {
      return err;
    }
  } else {
    /* Unit already initialized - verify resolution matches */
    if (bits != s_adc_unit_resolution[unit]) {
      rx_log_error(s_tag, "ADC resolution mismatch on subsequent init");
      return k_rx_err_invalid_arg;
    }
  }

  /* Enable the specified channel */
  if ((uint8_t)channel <= k_adc_channel_adansa0_max) {
    adc->adansa0 |= ((uint16_t)k_adc_bit_one << channel);
  } else {
    adc->adansa1 |= ((uint16_t)k_adc_bit_one << ((uint8_t)channel - k_adc_channel_adansa1_base));
  }

  rx_log_debug(s_tag, "ADC channel enabled");

  return k_rx_ok;
}

/**
 * @brief Read raw ADC conversion value from specified channel
 *
 * @details
 * Performs a single-scan A/D conversion and returns the raw digital value.
 * This is a blocking operation that polls the ADST bit until conversion
 * completes or timeout occurs.
 *
 * ## Conversion Sequence
 *
 * 1. Validate parameters and initialization state
 * 2. Start conversion by setting ADCSR.ADST bit
 * 3. Poll ADCSR.ADST until cleared (conversion complete)
 * 4. Read result from appropriate ADDRn register
 *
 * ## Output Value Ranges
 *
 * | Resolution | Range | Full-Scale |
 * |------------|-------|------------|
 * | 12-bit | 0-4095 | 3.3V |
 * | 10-bit | 0-1023 | 3.3V |
 * | 8-bit | 0-255 | 3.3V |
 *
 * @param[in] unit ADC unit number (0 for S12AD0, 1 for S12AD1)
 * @param[in] channel ADC channel to read (0-7)
 * @param[out] value Pointer to store raw ADC value (must not be NULL)
 *
 * @return rx_err_t Conversion result
 * @retval k_rx_ok Conversion successful, value contains result
 * @retval k_rx_err_null_ptr value pointer is nullptr
 * @retval k_rx_err_invalid_arg Invalid unit or channel
 * @retval k_rx_err_invalid_state ADC unit not initialized
 * @retval k_rx_err_timeout Conversion did not complete in time
 *
 * @pre ADC unit must be initialized via adc_init()
 * @pre value must point to valid uint16_t storage
 *
 * @post *value contains raw ADC reading (if k_rx_ok)
 * @post ADC ready for next conversion
 *
 * @note NOT thread-safe: Caller must synchronize if shared
 * @note Blocking: Waits up to ~100ms for conversion
 * @note Performance: ~1-10 us typical (depends on conversion time)
 *
 * @warning Do not call from ISR (blocking wait)
 *
 * @par Example:
 * @code
 * uint16_t raw_value;
 * rx_err_t err = adc_read(k_adc_unit_0, k_adc_channel_2, &raw_value);
 * if (err == k_rx_ok) {
 *   // Example: 12-bit resolution (0-4095 raw -> 0-3.3 V)
 *   static const float s_vref_volts = 3.3F;
 *   static const float s_adc_full_scale_12bit = 4095.0F;
 *   float voltage_v = (raw_value * s_vref_volts) / s_adc_full_scale_12bit;
 * }
 * @endcode
 *
 * @see adc_init() Must be called first
 * @see adc_read_voltage_mv() Convenience function for voltage
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 2: [OK] Timeout loop has fixed upper bound
 * - Rule 5: [OK] 2 preconditions, 2 postconditions
 */
rx_err_t adc_read(const adc_unit_t unit, const adc_channel_t channel, uint16_t* value)
{
  /* Validate parameters */
  RX_CHECK_NULL_PTR(value, s_tag, "Value pointer is nullptr");

  const rx_err_t err = internal_validate_unit_channel(unit, channel);
  RX_RETURN_ON_ERROR(err, s_tag, "Unit/channel validation failed");

  /* Check if unit is initialized */
  if (!s_adc_unit_initialized[unit]) {
    rx_log_error(s_tag, "ADC unit not initialized");
    return k_rx_err_invalid_state;
  }

  /* Get ADC base */
  volatile rx_s12ad_regs_t* const adc = internal_get_adc_base(unit);
  if (adc == nullptr) {
    return k_rx_err_invalid_arg;
  }

  /* Start single-scan conversion */
  adc->adcsr = k_adc_adcsr_adst;

  /* Wait for conversion to complete (poll ADCSR.ADST bit) */
  uint32_t timeout = k_adc_timeout_ms * k_adc_timeout_multiplier;
  while ((adc->adcsr & k_adc_adcsr_adst) != k_adc_adcsr_idle && timeout > k_adc_timeout_expired) {
    timeout--;
  }

  if (timeout == k_adc_timeout_expired) {
    rx_log_error(s_tag, "ADC conversion timeout");
    return k_rx_err_timeout;
  }

  /* Read conversion result from appropriate data register */
  return internal_read_channel_value(adc, channel, value);
}

/**
 * @brief Read ADC value and convert to millivolts
 *
 * @details
 * Performs an A/D conversion and scales the result to millivolts. This is
 * a convenience function that calls adc_read() and applies the voltage
 * calculation formula.
 *
 * ## Voltage Calculation Formula
 *
 * @f[
 *   V_{mV} = \frac{V_{raw} \times 3300}{2^{bits} - 1}
 * @f]
 *
 * ## Example Calculations
 *
 * | Resolution | Raw Value | Voltage (mV) | Calculation |
 * |------------|-----------|--------------|-------------|
 * | 12-bit | 2048 | 1650 | 2048 x 3300 / 4095 |
 * | 10-bit | 512 | 1650 | 512 x 3300 / 1023 |
 * | 8-bit | 128 | 1655 | 128 x 3300 / 255 |
 *
 * @param[in] unit ADC unit number (0 for S12AD0, 1 for S12AD1)
 * @param[in] channel ADC channel to read (0-7)
 * @param[in] bits Resolution setting (must match adc_init)
 * @param[out] voltage_mv Pointer to store voltage in millivolts (0-3300)
 *
 * @return rx_err_t Conversion result
 * @retval k_rx_ok Conversion successful, voltage_mv contains result
 * @retval k_rx_err_null_ptr voltage_mv pointer is nullptr
 * @retval k_rx_err_invalid_arg Invalid unit, channel, or resolution mismatch
 * @retval k_rx_err_invalid_state ADC unit not initialized
 * @retval k_rx_err_timeout Conversion did not complete in time
 *
 * @pre ADC unit must be initialized via adc_init()
 * @pre bits must match the resolution used in adc_init()
 *
 * @post *voltage_mv contains voltage in millivolts (0-3300)
 * @post ADC ready for next conversion
 *
 * @note NOT thread-safe: Caller must synchronize if shared
 * @note Blocking: Waits for conversion (see adc_read)
 * @note Reference: Uses 3.3V reference (k_adc_reference_voltage_mv = 3300)
 *
 * @par Example:
 * @code
 * // Read motor current sense (DRV8263H IPROPI output)
 * uint32_t voltage_mv;
 * rx_err_t err = adc_read_voltage_mv(k_adc_unit_0, k_adc_channel_0,
 *                                     k_adc_resolution_12bit, &voltage_mv);
 * if (err == k_rx_ok) {
 *   // voltage_mv: 0-3300 mV
 *   // For DRV8263H-Q1 IPROPI (202 uA/A mirror, 5100 Ohm sense):
 *   // I_motor = V_IPROPI / k_ipropi_divisor
 *   // (where k_ipropi_divisor = 1.0302 = 202e-6 * 5100)
 *   // Fixed-point: scale = 10000, divisor = 10302 (1.0302 * 10000)
 *   enum : uint32_t { k_scale = 10000, k_divisor = 10302 };
 *   uint32_t current_ma = (voltage_mv * k_scale) / k_divisor;
 *   rx_log_info("ADC", "Current: %lu mA", current_ma);
 * }
 * @endcode
 *
 * @see adc_read() Lower-level raw value read
 * @see adc_init() Initialize before reading
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: [OK] 2 preconditions, 2 postconditions
 */
rx_err_t adc_read_voltage_mv(const adc_unit_t unit,
                             adc_channel_t    channel,
                             adc_resolution_t bits,
                             uint32_t*        voltage_mv)
{
  /* Parameter order: unit, channel, resolution bits. */
  RX_CHECK_NULL_PTR(voltage_mv, s_tag, "Voltage pointer is nullptr");

  if (bits != k_adc_resolution_8bit && bits != k_adc_resolution_10bit &&
      bits != k_adc_resolution_12bit) {
    rx_log_error(s_tag, "Invalid resolution (must be 8, 10, or 12 bits)");
    return k_rx_err_invalid_arg;
  }

  /* Validate that passed resolution matches unit's configured resolution */
  if (unit > k_adc_unit_1) {
    rx_log_error(s_tag, "Invalid ADC unit");
    return k_rx_err_invalid_arg;
  }

  if (bits != s_adc_unit_resolution[unit]) {
    rx_log_error(s_tag, "ADC resolution mismatch in voltage read");
    return k_rx_err_invalid_arg;
  }

  /* Read raw ADC value */
  uint16_t       raw_value = k_adc_raw_value_reset;
  const rx_err_t err       = adc_read(unit, channel, &raw_value);
  RX_RETURN_ON_ERROR(err, s_tag, "ADC read failed");

  /* Calculate voltage (using ADC reference voltage) */
  const uint32_t max_value = ((uint32_t)k_adc_bit_one << bits) - k_adc_bit_one;
  *voltage_mv              = ((uint32_t)raw_value * k_adc_reference_voltage_mv) / max_value;

  return k_rx_ok;
}

#endif /* __RX__ */
