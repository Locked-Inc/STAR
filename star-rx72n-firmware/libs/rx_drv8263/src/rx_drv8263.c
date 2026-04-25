/**
 * @file rx_drv8263.c
 * @brief DRV8263H-Q1 Motor Driver Chip-Level Control Implementation
 *
 * @details
 * Implements DRVOFF control, latched fault reset via nSLEEP pulse,
 * and Open Load Protection (OLP) diagnostics for the DRV8263H-Q1
 * H-bridge motor driver.
 *
 * @par NASA Power of 10 Compliance:
 * | Rule | Status | Details |
 * |------|--------|---------|
 * | Rule 1 | [PASS] | No goto, setjmp, or recursion |
 * | Rule 2 | [PASS] | All loops bounded by enum constants |
 * | Rule 3 | [PASS] | Zero dynamic allocation |
 * | Rule 4 | [PASS] | All functions under 60 lines |
 * | Rule 5 | [PASS] | Minimum 2 validations per function |
 * | Rule 6 | [PASS] | Variables at smallest scope |
 * | Rule 7 | [PASS] | All return values checked |
 * | Rule 8 | [PASS] | C23 typed enums, minimal macros |
 * | Rule 9 | [WARN] | No function pointers in this module |
 * | Rule 10 | [PASS] | -Wall -Wextra -Werror clean |
 *
 * @par SOLID Principles:
 * - **S (Single Responsibility):** Each helper handles one GPIO or decode task
 * - **O (Open/Closed):** Extensible via config struct, no hard-coded pins
 * - **L (Liskov Substitution):** N/A (no polymorphism in this module)
 * - **I (Interface Segregation):** Public API split into init, DRVOFF, fault, OLP
 * - **D (Dependency Inversion):** GPIO access via rx_port_utils abstraction layer
 *
 * @see rx_drv8263.h API documentation
 *
 * @author Locked, Inc.
 * @date 2026-03-03
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @version 1.0.0
 */

#include "rx_drv8263.h"

#include "rx72n_clock.h"
#include "rx_check.h"
#include "rx_log.h"
#ifdef UNIT_TEST
#include "mock_rx_port_utils.h"
#else
#include "rx_port_utils.h"
#endif
#include "tx_api.h"

/**
 * @var s_tag
 * @brief Logging tag for this module
 *
 * @details
 * Identifies DRV8263H-Q1 log messages in the structured logging output.
 * Passed as the first argument to rx_log_info(), rx_log_warn(), rx_log_error(),
 * and rx_log_debug() calls throughout this translation unit.
 *
 * @note Access restrictions: module-local (static), read-only after initialization
 * @warning Must not be modified at runtime; changing this value would corrupt
 *          log output for the entire DRV8263 module
 * @since Version 1.0.0
 */
static const char s_tag[] = "DRV8263";

/**
 * @enum gpio_limits_t
 * @brief GPIO validation limits and bit-shift constants
 *
 * @details
 * Defines the maximum valid GPIO port and pin numbers for the RX72N
 * microcontroller, as well as the unit bit-shift constant used for
 * single-bit GPIO operations (set, clear, read).
 *
 * @code
 * if (port > k_max_port_number) { return false; }
 * base->podr |= (uint8_t)((uint8_t)k_bit_shift_one << pin);
 * @endcode
 *
 * @see internal_validate_gpio() Uses port/pin limits
 * @see internal_gpio_write() Uses k_bit_shift_one
 * @see internal_gpio_read() Uses k_bit_shift_one
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_max_port_number = 16, /**< Maximum valid port index (PORT0-PORTJ) */
  k_max_pin_number  = 7,  /**< Maximum valid pin within a port (0-7) */
  k_bit_shift_one   = 1,  /**< Unit bit value for single-bit GPIO mask operations */
} gpio_limits_t;

/**
 * @enum olp_nfault_idx_t
 * @brief Indices for OLP nFAULT reading array
 *
 * @details
 * Maps each OLP test pattern to its index in the nfault_readings array.
 * The DRV8263H-Q1 OLP diagnostic uses three IN1/IN2 patterns while
 * DRVOFF is HIGH; nFAULT is sampled after each pattern to determine
 * the load condition on OUT1 and OUT2.
 *
 * @code
 * bool nfault_readings[k_drv8263_olp_pattern_count];
 * nfault_readings[k_olp_pattern_both_low] = read_nfault();
 * nfault_readings[k_olp_pattern_in1_high] = read_nfault();
 * nfault_readings[k_olp_pattern_in2_high] = read_nfault();
 * @endcode
 *
 * @see internal_olp_apply_patterns() Writes patterns and fills array
 * @see internal_olp_decode_results() Decodes the three readings
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_olp_pattern_both_low = 0, /**< Pattern 0: IN1=0, IN2=0 */
  k_olp_pattern_in1_high = 1, /**< Pattern 1: IN1=1, IN2=0 */
  k_olp_pattern_in2_high = 2, /**< Pattern 2: IN1=0, IN2=1 */
} olp_nfault_idx_t;

/**
 * @enum drv8263_delay_ticks_t
 * @brief ThreadX tick delay constants for DRV8263H-Q1 timing
 *
 * @details
 * The DRV8263H-Q1 requires a tWAKE period of approximately 1.2 ms after
 * nSLEEP returns HIGH before nFAULT becomes valid. One ThreadX tick at
 * 10 ms provides sufficient margin over the 1.2 ms specification.
 *
 * @see rx_drv8263_clear_latched_fault() Uses k_twake_threadx_ticks
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_twake_threadx_ticks =
    1, /**< ThreadX ticks to wait for tWAKE (~10 ms, margin over 1.2 ms spec) */
} drv8263_delay_ticks_t;

/* =============================================================================
 * Internal Helper Functions
 * ============================================================================= */

/**
 * @brief Busy-wait delay for microsecond-precision timing
 *
 * @details
 * CPU-cycle based busy-wait for short delays required by DRV8263H-Q1
 * timing specifications (nSLEEP pulse, OLP settling). At 240 MHz,
 * each microsecond is approximately 240 CPU cycles.
 *
 * @param[in] us Delay duration in microseconds
 *
 * @pre us must be strictly positive (us > 0; zero is not a valid delay)
 * @pre us should be small (< 100 us) to avoid excessive blocking
 *
 * @post Minimum us microseconds have elapsed
 * @post Timing tolerance: approximately 240 cycles per microsecond with
 *       possible additional cycles due to interrupts; interrupts remain
 *       in their original enable/disable state
 *
 * @note Interrupts are NOT disabled; actual delay may be slightly longer
 * @note Uses volatile counter to prevent compiler optimization
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE void internal_delay_us(uint32_t us)
{
  /** @brief Maximum allowed busy-wait delay in microseconds */
  enum : uint32_t {
    k_max_delay_us = 100,
    k_hz_per_mhz   = 1000000UL,
  };

  if (us == 0 || us > k_max_delay_us) {
    return;
  }

  /* Cycles per microsecond derive from the authoritative ICLK constant in
   * rx72n_clock.h so any future clock-tree change tracks automatically. */
  const uint32_t   cycles_per_us = (uint32_t)k_iclk_hz / (uint32_t)k_hz_per_mhz;
  volatile uint32_t cycles       = us * cycles_per_us;
  while (cycles > 0) {
    cycles--;
  }
}

/**
 * @brief Validate GPIO port/pin pair
 *
 * @details
 * Checks that the given port and pin numbers fall within the valid range
 * for the RX72N GPIO subsystem. Port must be 0-16 (PORT0 through PORTJ)
 * and pin must be 0-7 (8-bit port width).
 *
 * @param[in] port Port number
 * @param[in] pin  Pin number
 *
 * @return true if valid, false otherwise
 *
 * @pre port and pin are unsigned 8-bit values (always >= 0)
 * @pre Called only during initialization or reconfiguration
 *
 * @post No side effects; purely validates input range
 * @post Return value is deterministic for same inputs
 *
 * @note Thread safety: pure function, no shared state
 * @note This function intentionally does not use assert() for range checks
 *       because it IS the runtime validation function; callers rely on its
 *       return value to detect invalid GPIO coordinates.
 *
 * @see internal_validate_config() Calls this for each GPIO assignment
 *
 * @since Version 1.0.0
 */
static inline bool internal_validate_gpio(uint8_t port, uint8_t pin)
{
  if (port > k_max_port_number) {
    return false;
  }
  if (pin > k_max_pin_number) {
    return false;
  }
  return true;
}

/**
 * @brief Set a GPIO output pin to HIGH or LOW
 *
 * @details
 * Performs a read-modify-write on the port output data register (PODR)
 * to set or clear a single pin without affecting other pins in the port.
 * If the port base address is invalid (nullptr), the function returns
 * silently with no effect.
 *
 * @param[in] port Port number (0-16)
 * @param[in] pin  Pin number within port (0-7)
 * @param[in] high true = set HIGH, false = set LOW
 *
 * @return void
 *
 * @pre Port must be configured as output (PDR bit set)
 * @pre port must be <= k_max_port_number and pin must be <= k_max_pin_number
 *
 * @post Target pin state changed to requested level
 * @post No side-effects on other pins in the same port
 *
 * @note Not interrupt-safe; caller must provide synchronization if called
 *       from multiple contexts
 *
 * @see internal_gpio_read() Complementary read function
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE void internal_gpio_write(uint8_t port, uint8_t pin, bool high)
{
  if (!internal_validate_gpio(port, pin)) {
    rx_log_debug(s_tag, "GPIO write: invalid port/pin");
    return;
  }

  volatile rx_port_regs_t* base = rx_port_get_base(port);

  if (high) {
    base->podr |= (uint8_t)(k_bit_shift_one << pin);
  } else {
    base->podr &= (uint8_t) ~(k_bit_shift_one << pin);
  }
}

/**
 * @brief Read a GPIO input pin state
 *
 * @details
 * Reads the port input data register (PIDR) and masks the target bit to
 * determine pin state. Returns true if the pin is HIGH, false if the pin
 * is LOW or the port base address is invalid (nullptr).
 *
 * @param[in] port Port number (0-16)
 * @param[in] pin  Pin number within port (0-7)
 *
 * @return true if pin is HIGH, false if LOW or port invalid
 *
 * @pre port must be <= k_max_port_number and pin must be <= k_max_pin_number
 * @pre GPIO subsystem must be initialized (clock gating enabled for target port)
 *
 * @post No side effects; return value reflects current pin state
 * @post Port registers are not modified
 *
 * @note Thread safety: caller must provide synchronization if pin state
 *       can change asynchronously (e.g., from ISR context)
 *
 * @see internal_gpio_write() Complementary write function
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE bool internal_gpio_read(uint8_t port, uint8_t pin)
{
  if (!internal_validate_gpio(port, pin)) {
    rx_log_debug(s_tag, "GPIO read: invalid port/pin");
    return false;
  }

  volatile rx_port_regs_t* base = rx_port_get_base(port);

  return (bool)((base->pidr & (uint8_t)(k_bit_shift_one << pin)) != 0);
}

/**
 * @brief Validate all GPIO assignments in config
 *
 * @details
 * Iterates through all five GPIO assignments in the configuration struct
 * (DRVOFF, nSLEEP, nFAULT, IN1, IN2) and validates each port/pin pair.
 * Logs a descriptive error message identifying which signal has an invalid
 * assignment on failure.
 *
 * @param[in] config Configuration to validate
 *
 * @return k_rx_ok if all valid, k_rx_err_invalid_arg otherwise
 *
 * @pre config must not be nullptr (caller validates)
 * @pre config must point to a fully populated rx_drv8263_config_t struct
 *
 * @post No modification to config (const pointer)
 * @post On failure, exactly one error log message identifies the invalid signal
 *
 * @note Thread safety: pure validation, no shared state modified
 *
 * @see internal_validate_gpio() Validates individual port/pin pairs
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE rx_err_t internal_validate_config(const rx_drv8263_config_t* config)
{
  if (config == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (!internal_validate_gpio(config->port_drvoff, config->pin_drvoff)) {
    rx_log_error(s_tag, "Invalid DRVOFF GPIO");
    return k_rx_err_invalid_arg;
  }
  if (!internal_validate_gpio(config->port_nsleep, config->pin_nsleep)) {
    rx_log_error(s_tag, "Invalid nSLEEP GPIO");
    return k_rx_err_invalid_arg;
  }
  if (!internal_validate_gpio(config->port_nfault, config->pin_nfault)) {
    rx_log_error(s_tag, "Invalid nFAULT GPIO");
    return k_rx_err_invalid_arg;
  }
  if (!internal_validate_gpio(config->port_in1, config->pin_in1)) {
    rx_log_error(s_tag, "Invalid IN1 GPIO");
    return k_rx_err_invalid_arg;
  }
  if (!internal_validate_gpio(config->port_in2, config->pin_in2)) {
    rx_log_error(s_tag, "Invalid IN2 GPIO");
    return k_rx_err_invalid_arg;
  }

  return k_rx_ok;
}

/**
 * @brief Apply three OLP test patterns and read nFAULT for each
 *
 * @details
 * Drives IN1 and IN2 through the three DRV8263H-Q1 OLP test patterns
 * while DRVOFF is HIGH (caller is responsible for DRVOFF control).
 * For each pattern, the function sets IN1/IN2, waits for the OLP
 * settling time, and reads the nFAULT pin state into the corresponding
 * element of the output array.
 *
 * OLP test patterns:
 *   - Pattern 0 (k_olp_pattern_both_low): IN1=0, IN2=0
 *   - Pattern 1 (k_olp_pattern_in1_high): IN1=1, IN2=0
 *   - Pattern 2 (k_olp_pattern_in2_high): IN1=0, IN2=1
 *
 * @param[in]  handle           Initialized driver handle (provides GPIO config)
 * @param[out] nfault_readings  Array of k_drv8263_olp_pattern_count bools;
 *                               filled with nFAULT state for each pattern
 *
 * @pre handle must be initialized via rx_drv8263_init()
 * @pre DRVOFF must already be set HIGH by caller (standby mode)
 *
 * @post nfault_readings[0..2] populated with nFAULT readings
 * @post IN1 and IN2 are left in the state of the last pattern (IN1=0, IN2=1);
 *       caller must restore safe state
 *
 * @note Not thread-safe; caller must provide synchronization
 *
 * @see rx_drv8263_run_olp() Main OLP function that calls this helper
 * @see internal_olp_decode_results() Decodes the nFAULT readings
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE void internal_olp_apply_patterns(const rx_drv8263_handle_t* handle,
                                                    bool                       nfault_readings[])
{
  if (handle == nullptr || nfault_readings == nullptr) {
    return;
  }

  /* Pattern 0: IN1=0, IN2=0 */
  internal_gpio_write(handle->config.port_in1, handle->config.pin_in1, false);
  internal_gpio_write(handle->config.port_in2, handle->config.pin_in2, false);
  internal_delay_us((uint32_t)k_drv8263_olp_settle_us);
  nfault_readings[k_olp_pattern_both_low] =
    internal_gpio_read(handle->config.port_nfault, handle->config.pin_nfault);

  /* Pattern 1: IN1=1, IN2=0 */
  internal_gpio_write(handle->config.port_in1, handle->config.pin_in1, true);
  internal_gpio_write(handle->config.port_in2, handle->config.pin_in2, false);
  internal_delay_us((uint32_t)k_drv8263_olp_settle_us);
  nfault_readings[k_olp_pattern_in1_high] =
    internal_gpio_read(handle->config.port_nfault, handle->config.pin_nfault);

  /* Pattern 2: IN1=0, IN2=1 */
  internal_gpio_write(handle->config.port_in1, handle->config.pin_in1, false);
  internal_gpio_write(handle->config.port_in2, handle->config.pin_in2, true);
  internal_delay_us((uint32_t)k_drv8263_olp_settle_us);
  nfault_readings[k_olp_pattern_in2_high] =
    internal_gpio_read(handle->config.port_nfault, handle->config.pin_nfault);
}

/**
 * @brief Decode OLP nFAULT readings into diagnostic results
 *
 * @details
 * Interprets the three nFAULT readings from the OLP test patterns using
 * the DRV8263H-Q1 truth table to determine the load condition on OUT1
 * and OUT2. Logs warnings or errors for abnormal conditions.
 *
 * Truth table (nFAULT readings for {pattern0, pattern1, pattern2}):
 *   - {1,1,1} = Normal (no fault)
 *   - {1,1,0} = Open Load OUT2
 *   - {1,0,1} = Open Load OUT1
 *   - {0,1,1} = Short to GND
 *   - {0,0,0} = Short to VM (or both shorted)
 *   - Other   = Unknown / ambiguous
 *
 * @param[in]  f0          nFAULT reading for pattern 0 (IN1=0, IN2=0)
 * @param[in]  f1          nFAULT reading for pattern 1 (IN1=1, IN2=0)
 * @param[in]  f2          nFAULT reading for pattern 2 (IN1=0, IN2=1)
 * @param[out] result_out1 OLP diagnostic result for OUT1. Must not be nullptr.
 * @param[out] result_out2 OLP diagnostic result for OUT2. Must not be nullptr.
 *
 * @pre result_out1 and result_out2 must point to valid memory
 * @pre f0, f1, f2 must be valid nFAULT readings from OLP test patterns
 *
 * @post result_out1 and result_out2 contain the decoded diagnostic results
 * @post A warning or error log is emitted for any abnormal condition
 *
 * @note Thread safety: no shared state; logging must be thread-safe
 *
 * @see internal_olp_apply_patterns() Produces the nFAULT readings
 * @see rx_drv8263_run_olp() Main OLP function that calls this helper
 *
 * @since Version 1.0.0
 */
RX_STATIC_TESTABLE void internal_olp_decode_results(bool                     f0,
                                                    bool                     f1,
                                                    bool                     f2,
                                                    rx_drv8263_olp_result_t* result_out1,
                                                    rx_drv8263_olp_result_t* result_out2)
{
  if (result_out1 == nullptr || result_out2 == nullptr) {
    rx_log_error(s_tag, "OLP: null output pointer");
    return;
  }

  if ((int)f0 && (int)f1 && (int)f2) {
    /* {1,1,1} = Normal */
    *result_out1 = k_drv8263_olp_normal;
    *result_out2 = k_drv8263_olp_normal;
  } else if ((int)f0 && (int)f1) {
    /* {1,1,0} = Open Load on OUT2 (f2 must be false; f0&&f1&&f2 ruled out above) */
    *result_out1 = k_drv8263_olp_normal;
    *result_out2 = k_drv8263_olp_open_load;
    rx_log_warn(s_tag, "OLP: Open load on OUT2");
  } else if ((int)f0 && (int)f2) {
    /* {1,0,1} = Open Load on OUT1 (f1 must be false; f0&&f1 ruled out above) */
    *result_out1 = k_drv8263_olp_open_load;
    *result_out2 = k_drv8263_olp_normal;
    rx_log_warn(s_tag, "OLP: Open load on OUT1");
  } else if (!(int)f0 && (int)f1 && (int)f2) {
    /* {0,1,1} = Short to GND */
    *result_out1 = k_drv8263_olp_short_to_gnd;
    *result_out2 = k_drv8263_olp_short_to_gnd;
    rx_log_error(s_tag, "OLP: Short to GND detected");
  } else if (!(int)f0 && !(int)f1 && !(int)f2) {
    /* {0,0,0} = Short to VM */
    *result_out1 = k_drv8263_olp_short_to_vm;
    *result_out2 = k_drv8263_olp_short_to_vm;
    rx_log_error(s_tag, "OLP: Short to VM detected");
  } else {
    /* Unexpected pattern */
    *result_out1 = k_drv8263_olp_unknown;
    *result_out2 = k_drv8263_olp_unknown;
    rx_log_warn(s_tag, "OLP: Unknown nFAULT pattern");
  }
}

/* =============================================================================
 * Public API Implementation
 * ============================================================================= */

/**
 * @brief Initialize DRV8263H-Q1 driver instance
 *
 * @details
 * Validates all GPIO assignments in the provided configuration, copies
 * the configuration into the handle, and marks the handle as initialized.
 * If the configuration flag olp_enable_boot is set, the function
 * additionally runs a full Open Load Protection (OLP) diagnostic sequence
 * during initialization to detect disconnected or shorted motors before
 * the control loop begins. OLP failures at boot are logged as warnings
 * but do not prevent initialization from succeeding.
 *
 * Initialization sequence:
 * 1. Validate handle and config pointers (null check)
 * 2. Validate all five GPIO port/pin pairs (DRVOFF, nSLEEP, nFAULT, IN1, IN2)
 * 3. Copy configuration into handle
 * 4. Set handle->initialized = true
 * 5. (Optional) Run OLP diagnostic if olp_enable_boot is true
 *
 * @param[out] handle Pointer to caller-allocated handle structure to initialize.
 *                     Must not be nullptr. Typically declared as a static variable.
 * @param[in]  config Pointer to configuration containing GPIO assignments and
 *                     OLP flags. Must not be nullptr. Copied into handle.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok             Initialization succeeded (handle ready for use)
 * @retval k_rx_err_null_ptr   handle or config is nullptr
 * @retval k_rx_err_invalid_arg GPIO port/pin value out of valid range
 *
 * @pre handle must point to valid, writable memory of size >= sizeof(rx_drv8263_handle_t)
 * @pre config must contain valid GPIO port (0-16) and pin (0-7) assignments for all signals
 * @pre GPIO ports referenced in config must have their clocks enabled and pin directions set
 *
 * @post handle->initialized == true on success
 * @post handle->config contains a complete copy of *config
 * @post On failure, handle is not modified (remains in its prior state)
 *
 * @note Not thread-safe; do not call concurrently for the same handle
 *
 * @par Example:
 * @code
 * static rx_drv8263_handle_t drv;
 * const rx_drv8263_config_t cfg = {
 *     .port_drvoff = 6, .pin_drvoff = 1,
 *     .port_nsleep = 6, .pin_nsleep = 0,
 *     .port_nfault = 1, .pin_nfault = 5,
 *     .port_in1    = 1, .pin_in1    = 7,
 *     .port_in2    = 2, .pin_in2    = 3,
 *     .olp_enable_boot  = true,
 *     .olp_enable_fault = true,
 * };
 * rx_err_t err = rx_drv8263_init(&drv, &cfg);
 * @endcode
 *
 * @see rx_drv8263_set_drvoff() Control DRVOFF after initialization
 * @see rx_drv8263_run_olp() Manual OLP diagnostic
 * @see rx_drv8263_clear_latched_fault() Fault reset
 *
 * @since Version 1.0.0
 */
rx_err_t rx_drv8263_init(rx_drv8263_handle_t* handle, const rx_drv8263_config_t* config)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is nullptr");
  RX_CHECK_NULL_PTR(config, s_tag, "config is nullptr");

  const rx_err_t err = internal_validate_config(config);
  if (err != k_rx_ok) {
    return err;
  }

  handle->config      = *config;
  handle->initialized = true;

  rx_log_info(s_tag, "DRV8263H-Q1 driver initialized");

  /* Run OLP diagnostic at boot if configured */
  if (config->olp_enable_boot) {
    rx_drv8263_olp_result_t result_out1 = k_drv8263_olp_unknown;
    rx_drv8263_olp_result_t result_out2 = k_drv8263_olp_unknown;

    (void)rx_drv8263_run_olp(handle, &result_out1, &result_out2);
    bool out1_abnormal = (bool)(result_out1 != k_drv8263_olp_normal);
    bool out2_abnormal = (bool)(result_out2 != k_drv8263_olp_normal);
    if ((bool)((int)out1_abnormal | (int)out2_abnormal)) {
      rx_log_warn(s_tag, "Boot OLP detected abnormal load condition");
    }
  }

  return k_rx_ok;
}

/**
 * @brief Control DRVOFF output disable signal
 *
 * @details
 * Drives the DRVOFF GPIO to enable or disable the DRV8263H-Q1 H-bridge
 * outputs. DRVOFF is active-high: when active=true, the pin is driven
 * HIGH and both half-bridge outputs (OUT1, OUT2) are placed in a
 * high-impedance state. When active=false, the pin is driven LOW and
 * normal PWM-controlled operation resumes.
 *
 * DRVOFF must be asserted HIGH before running Open Load Protection (OLP)
 * diagnostics, and deasserted LOW to return to normal motor operation.
 *
 * @param[in] handle Pointer to initialized driver handle. Must not be nullptr.
 * @param[in] active true = DRVOFF HIGH (outputs disabled / standby),
 *                    false = DRVOFF LOW (outputs enabled / active)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok               DRVOFF pin set to requested level
 * @retval k_rx_err_null_ptr     handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized via rx_drv8263_init()
 *
 * @pre handle must be initialized via rx_drv8263_init()
 * @pre DRVOFF GPIO must be configured as an output pin (hardware_init responsibility)
 *
 * @post DRVOFF GPIO reflects the requested level (HIGH if active, LOW otherwise)
 * @post No other GPIO pins (nSLEEP, IN1, IN2) are modified
 *
 * @note Not thread-safe; caller must provide synchronization when accessing
 *       the same handle from multiple contexts
 *
 * @see rx_drv8263_init() Must be called before this function
 * @see rx_drv8263_run_olp() Internally uses DRVOFF HIGH for OLP sequence
 *
 * @since Version 1.0.0
 */
rx_err_t rx_drv8263_set_drvoff(rx_drv8263_handle_t* handle, bool active)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is nullptr");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Not initialized");
    return k_rx_err_invalid_state;
  }

  internal_gpio_write(handle->config.port_drvoff, handle->config.pin_drvoff, active);

  return k_rx_ok;
}

/**
 * @brief Clear latched fault by pulsing nSLEEP LOW
 *
 * @details
 * The DRV8263H-Q1 latches faults (overcurrent, thermal shutdown) until
 * the nSLEEP pin is pulsed LOW for a duration within the 5-35 us
 * datasheet window. This function executes the following sequence:
 *
 * 1. Drive nSLEEP LOW to begin the fault reset pulse
 * 2. Busy-wait for k_drv8263_nsleep_pulse_us (~20 us), targeting the
 *    middle of the 5-35 us specification window
 * 3. Drive nSLEEP HIGH to wake the driver
 * 4. Wait for tWAKE using tx_thread_sleep(k_twake_threadx_ticks). One
 *    ThreadX tick (~10 ms) provides sufficient margin over the ~1.2 ms
 *    tWAKE specification. After tWAKE, nFAULT returns HIGH if the
 *    underlying fault condition has been removed.
 *
 * @par Timing Notes:
 * The nSLEEP pulse uses a CPU busy-wait loop (internal_delay_us) since
 * the required precision (~20 us) is below ThreadX tick granularity
 * (~10 ms). The tWAKE delay uses tx_thread_sleep to yield the CPU.
 * nFAULT cannot be polled after the pulse because it may be configured
 * as a POEG (Port Output Enable for GPT) input rather than a standard
 * GPIO input.
 *
 * @param[in] handle Pointer to initialized driver handle. Must not be nullptr.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok               Fault reset pulse completed successfully
 * @retval k_rx_err_null_ptr     handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized via rx_drv8263_init()
 *
 * @pre handle must be initialized via rx_drv8263_init()
 * @pre Motor outputs should be stopped (PWM disabled) before clearing a fault
 *      to prevent unexpected motor movement when the driver re-enables
 *
 * @post nSLEEP pin is HIGH (driver awake)
 * @post Latched fault is cleared if the underlying fault condition has been removed
 * @post At least k_twake_threadx_ticks ThreadX ticks have elapsed (tWAKE satisfied)
 *
 * @note Not thread-safe; caller must provide synchronization
 * @note Cannot verify fault clearance because nFAULT may be a POEG input
 *
 * @see rx_drv8263_init() Must be called before this function
 * @see rx_drv8263_run_olp() Run OLP after clearing fault to diagnose load condition
 * @see k_drv8263_nsleep_pulse_us nSLEEP pulse duration constant
 * @see k_twake_threadx_ticks ThreadX tick delay for tWAKE
 *
 * @since Version 1.0.0
 */
rx_err_t rx_drv8263_clear_latched_fault(rx_drv8263_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is nullptr");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Not initialized");
    return k_rx_err_invalid_state;
  }

  /* Step 1: Drive nSLEEP LOW to begin fault reset pulse */
  internal_gpio_write(handle->config.port_nsleep, handle->config.pin_nsleep, false);

  /* Step 2: Hold LOW for ~20 us (within 5-35 us spec window) */
  internal_delay_us((uint32_t)k_drv8263_nsleep_pulse_us);

  /* Step 3: Drive nSLEEP HIGH to wake driver */
  internal_gpio_write(handle->config.port_nsleep, handle->config.pin_nsleep, true);

  /* Step 4: Wait for tWAKE (~1.2 ms). One ThreadX tick = 10 ms provides
   * sufficient margin. Cannot poll nFAULT as it may be configured as POEG input. */
  /* NASA Rule 7: capture return value; intentionally discarded (sleep cannot
   * meaningfully fail in this context, but we document the discard). */
  const UINT sleep_status = tx_thread_sleep((ULONG)k_twake_threadx_ticks);
  (void)sleep_status;

  rx_log_info(s_tag, "Latched fault cleared");

  return k_rx_ok;
}

/**
 * @brief Run Open Load Protection (OLP) diagnostic sequence
 *
 * @details
 * Executes the full DRV8263H-Q1 OLP diagnostic to determine the load
 * condition on motor outputs OUT1 and OUT2. The sequence temporarily
 * places the driver in standby mode (DRVOFF HIGH), applies three IN1/IN2
 * test patterns while sampling nFAULT after each, then restores active
 * mode (DRVOFF LOW).
 *
 * Full OLP test sequence:
 * 1. Set DRVOFF = HIGH (standby mode, enables OLP detection logic)
 * 2. Wait k_drv8263_olp_settle_us (~50 us) for DRVOFF to take effect
 * 3. Apply 3 test patterns, each followed by k_drv8263_olp_settle_us delay:
 *    - Pattern 0: IN1=0, IN2=0 -> read nFAULT (baseline)
 *    - Pattern 1: IN1=1, IN2=0 -> read nFAULT (tests OUT1 path)
 *    - Pattern 2: IN1=0, IN2=1 -> read nFAULT (tests OUT2 path)
 * 4. Return IN1 and IN2 to LOW (safe state)
 * 5. Set DRVOFF = LOW (return to active motor control mode)
 * 6. Decode 3-bit nFAULT result vector against the truth table
 *
 * @par nFAULT Pattern Ordering:
 * Patterns are applied in index order (k_olp_pattern_both_low,
 * k_olp_pattern_in1_high, k_olp_pattern_in2_high). The nFAULT reading
 * for each pattern is stored at the corresponding array index.
 *
 * @par OLP Truth Table:
 * | Pattern 0 | Pattern 1 | Pattern 2 | OUT1 Result     | OUT2 Result     |
 * |-----------|-----------|-----------|-----------------|-----------------|
 * | 1         | 1         | 1         | Normal          | Normal          |
 * | 1         | 1         | 0         | Normal          | Open Load       |
 * | 1         | 0         | 1         | Open Load       | Normal          |
 * | 0         | 1         | 1         | Short to GND    | Short to GND    |
 * | 0         | 0         | 0         | Short to VM     | Short to VM     |
 * | other     | other     | other     | Unknown         | Unknown         |
 *
 * @param[in]  handle      Pointer to initialized driver handle. Must not be nullptr.
 * @param[out] result_out1 OLP diagnostic result for OUT1 (IN1 side). Must not be nullptr.
 *                          Set to k_drv8263_olp_unknown on entry; updated with decoded result.
 * @param[out] result_out2 OLP diagnostic result for OUT2 (IN2 side). Must not be nullptr.
 *                          Set to k_drv8263_olp_unknown on entry; updated with decoded result.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok               OLP diagnostic completed, results written
 * @retval k_rx_err_null_ptr     handle, result_out1, or result_out2 is nullptr
 * @retval k_rx_err_invalid_state handle not initialized via rx_drv8263_init()
 *
 * @pre handle must be initialized via rx_drv8263_init()
 * @pre Motor PWM must be stopped (IN1/IN2 are driven as GPIO during OLP)
 * @pre nFAULT pin must be readable as a standard GPIO input (not solely POEG)
 *
 * @post DRVOFF returns to LOW (outputs re-enabled)
 * @post IN1 and IN2 are both LOW (safe / coast state)
 * @post result_out1 and result_out2 contain decoded OLP diagnostic results
 *
 * @note Not thread-safe; caller must provide synchronization
 * @note Execution time: approximately 200 us total (4 x k_drv8263_olp_settle_us
 *       settling delays plus GPIO read/write overhead)
 *
 * @see rx_drv8263_init() Calls this at boot when olp_enable_boot is true
 * @see rx_drv8263_set_drvoff() DRVOFF control used internally
 * @see internal_olp_apply_patterns() Applies the 3 test patterns
 * @see internal_olp_decode_results() Decodes the nFAULT readings
 * @see k_drv8263_olp_settle_us Settling delay between patterns
 *
 * @since Version 1.0.0
 */
rx_err_t rx_drv8263_run_olp(rx_drv8263_handle_t*     handle,
                            rx_drv8263_olp_result_t* result_out1,
                            rx_drv8263_olp_result_t* result_out2)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is nullptr");
  RX_CHECK_NULL_PTR(result_out1, s_tag, "result_out1 is nullptr");
  RX_CHECK_NULL_PTR(result_out2, s_tag, "result_out2 is nullptr");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Not initialized");
    return k_rx_err_invalid_state;
  }

  /* Default results */
  *result_out1 = k_drv8263_olp_unknown;
  *result_out2 = k_drv8263_olp_unknown;

  /* Step 1: Set DRVOFF = HIGH (standby mode, enables OLP detection) */
  internal_gpio_write(handle->config.port_drvoff, handle->config.pin_drvoff, true);
  internal_delay_us((uint32_t)k_drv8263_olp_settle_us);

  /* Step 2: Apply 3 test patterns and read nFAULT for each */
  bool nfault_readings[k_drv8263_olp_pattern_count] = {false};
  internal_olp_apply_patterns(handle, nfault_readings);

  /* Step 3: Return IN1/IN2 to LOW (safe state) */
  internal_gpio_write(handle->config.port_in1, handle->config.pin_in1, false);
  internal_gpio_write(handle->config.port_in2, handle->config.pin_in2, false);

  /* Step 4: Set DRVOFF = LOW (return to active mode) */
  internal_gpio_write(handle->config.port_drvoff, handle->config.pin_drvoff, false);

  /* Step 5: Decode OLP results from nFAULT readings */
  internal_olp_decode_results(nfault_readings[k_olp_pattern_both_low],
                              nfault_readings[k_olp_pattern_in1_high],
                              nfault_readings[k_olp_pattern_in2_high],
                              result_out1,
                              result_out2);

  return k_rx_ok;
}

/**
 * @brief Enable or disable OLP diagnostic at boot time
 *
 * @details
 * Updates the olp_enable_boot flag in the driver configuration stored
 * within the handle. When enabled, subsequent calls to rx_drv8263_init()
 * with this handle's configuration will execute the full 3-pattern OLP
 * diagnostic sequence during initialization. This provides early
 * detection of disconnected or shorted motors before the control loop
 * begins.
 *
 * This is a configuration-only change with no immediate hardware side
 * effects. The flag takes effect the next time rx_drv8263_init() is
 * called with a configuration that has olp_enable_boot set.
 *
 * @param[in] handle Pointer to initialized driver handle. Must not be nullptr.
 * @param[in] enable true to enable OLP diagnostic at boot, false to disable
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok               Configuration flag updated successfully
 * @retval k_rx_err_null_ptr     handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized via rx_drv8263_init()
 *
 * @pre handle must be initialized via rx_drv8263_init()
 * @pre handle must point to valid, writable memory
 *
 * @post handle->config.olp_enable_boot == enable
 * @post No hardware pins are modified (configuration change only)
 *
 * @note Not thread-safe; caller must provide synchronization when
 *       accessing the same handle from multiple contexts
 *
 * @see rx_drv8263_init() Executes OLP at boot when olp_enable_boot is true
 * @see rx_drv8263_set_olp_fault_enable() Companion function for fault-triggered OLP
 *
 * @since Version 1.0.0
 */
rx_err_t rx_drv8263_set_olp_boot_enable(rx_drv8263_handle_t* handle, bool enable)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is nullptr");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Not initialized");
    return k_rx_err_invalid_state;
  }

  handle->config.olp_enable_boot = enable;

  return k_rx_ok;
}

/**
 * @brief Enable or disable OLP diagnostic in fault handler
 *
 * @details
 * Updates the olp_enable_fault flag in the driver configuration stored
 * within the handle. When enabled, the fault handling path will execute
 * the full 3-pattern OLP diagnostic sequence after clearing a latched
 * fault via rx_drv8263_clear_latched_fault(). This helps determine
 * whether the fault was caused by an open or shorted load condition.
 *
 * This is a configuration-only change with no immediate hardware side
 * effects. The flag is checked by the fault handling logic to decide
 * whether to run OLP after fault clearance.
 *
 * @param[in] handle Pointer to initialized driver handle. Must not be nullptr.
 * @param[in] enable true to enable OLP diagnostic after fault clearance,
 *                    false to disable
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok               Configuration flag updated successfully
 * @retval k_rx_err_null_ptr     handle is nullptr
 * @retval k_rx_err_invalid_state handle not initialized via rx_drv8263_init()
 *
 * @pre handle must be initialized via rx_drv8263_init()
 * @pre handle must point to valid, writable memory
 *
 * @post handle->config.olp_enable_fault == enable
 * @post No hardware pins are modified (configuration change only)
 *
 * @note Not thread-safe; caller must provide synchronization when
 *       accessing the same handle from multiple contexts
 *
 * @see rx_drv8263_clear_latched_fault() Fault clearance function
 * @see rx_drv8263_set_olp_boot_enable() Companion function for boot-time OLP
 *
 * @since Version 1.0.0
 */
rx_err_t rx_drv8263_set_olp_fault_enable(rx_drv8263_handle_t* handle, bool enable)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle is nullptr");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Not initialized");
    return k_rx_err_invalid_state;
  }

  handle->config.olp_enable_fault = enable;

  return k_rx_ok;
}
