/* tests/mocks/mock_rx_system_regs.h */

/**
 * @file mock_rx_system_regs.h
 * @brief Mock Reset Status Registers for Unit Testing
 *
 * @details
 * Provides mock implementation of RX72N reset status registers (RSTSR0/1/2)
 * for host-side unit testing. Allows tests to simulate various reset conditions
 * (power-on, watchdog, brownout, etc.) without actual hardware.
 *
 * ## Supported Registers
 * - RSTSR0 (Power-On Reset, LVD Reset flags)
 * - RSTSR1 (Cold/Warm Start flag)
 * - RSTSR2 (IWDT, WDT, Software Reset flags)
 *
 * ## Usage Pattern
 * @code
 * void setUp(void) {
 *     mock_system_regs_reset();  // Clear all flags
 * }
 *
 * void test_iwdt_reset_detected(void) {
 *     mock_system_regs_set_iwdtrf(true);  // Simulate IWDT timeout
 *     bool ok = internal_check_iwdtrf();
 *     TEST_ASSERT_FALSE(ok);  // Should detect the IWDT reset
 * }
 * @endcode
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Control Functions - Call from test setUp/tearDown
 * =============================================================================
 */

/**
 * @brief Reset all mock register state to defaults
 *
 * @details
 * Clears all reset flags to their "normal boot" state:
 * - PORF = 1 (power-on reset occurred - normal for cold start)
 * - All other flags = 0 (no abnormal resets)
 *
 * Call this in setUp() before each test to ensure clean state.
 */
void mock_system_regs_reset(void);

/* =============================================================================
 * RSTSR0 Flag Setters (Power-On, Voltage Monitoring)
 * =============================================================================
 */

/**
 * @brief Set Power-On Reset Detect Flag (PORF) in RSTSR0
 *
 * @param[in] set true = flag set (cold start), false = flag clear (warm start)
 *
 * @details
 * PORF=1 indicates a cold boot (power applied).
 * PORF=0 indicates a warm boot (software reset, watchdog, debugger, etc.)
 */
void mock_system_regs_set_porf(bool set);

/**
 * @brief Set Voltage Monitor 0 Reset Detect Flag (LVD0RF) in RSTSR0
 *
 * @param[in] set true = brownout detected, false = no brownout
 *
 * @details
 * LVD0RF=1 indicates a low voltage (brownout) reset occurred.
 * This is typically a hardware power supply issue.
 */
void mock_system_regs_set_lvd0rf(bool set);

/* =============================================================================
 * RSTSR1 Flag Setters (Cold/Warm Start)
 * =============================================================================
 */

/**
 * @brief Set Cold/Warm Start Determination Flag (CWSF) in RSTSR1
 *
 * @param[in] set true = warm start, false = cold start
 *
 * @details
 * CWSF provides additional cold/warm start information.
 * This is informational only (not an error condition).
 */
void mock_system_regs_set_cwsf(bool set);

/* =============================================================================
 * RSTSR2 Flag Setters (Watchdog, Software Reset)
 * =============================================================================
 */

/**
 * @brief Set Independent Watchdog Timer Reset Detect Flag (IWDTRF) in RSTSR2
 *
 * @param[in] set true = IWDT timeout (CRITICAL), false = normal
 *
 * @details
 * IWDTRF=1 is a **critical firmware error** - indicates prior execution hung.
 * The startup code should halt (RX_ASSERT) if this flag is set.
 *
 * @warning Setting this to true simulates a firmware crash scenario.
 */
void mock_system_regs_set_iwdtrf(bool set);

/**
 * @brief Set Watchdog Timer Reset Detect Flag (WDTRF) in RSTSR2
 *
 * @param[in] set true = WDT timeout, false = normal
 *
 * @details
 * WDTRF=1 indicates a standard watchdog timeout. Unlike IWDTRF, this may
 * be intentional in some scenarios (bootloader timeout).
 */
void mock_system_regs_set_wdtrf(bool set);

/**
 * @brief Set Software Reset Detect Flag (SWRF) in RSTSR2
 *
 * @param[in] set true = software reset, false = normal
 *
 * @details
 * SWRF=1 indicates a software-initiated reset (via SWRR register).
 * This is typically intentional (bootloader, firmware update, etc.).
 */
void mock_system_regs_set_swrf(bool set);

/* =============================================================================
 * Scenario Helpers - Simulate common reset conditions
 * =============================================================================
 */

/**
 * @brief Simulate a normal cold boot (power-on reset)
 *
 * @details
 * Sets: PORF=1, all other flags=0
 * This is the expected state for a normal power-on boot.
 */
void mock_system_regs_simulate_cold_boot(void);

/**
 * @brief Simulate a warm boot (software reset, debugger)
 *
 * @details
 * Sets: PORF=0, CWSF=1, all other flags=0
 * This is common when using a debugger or after firmware update.
 */
void mock_system_regs_simulate_warm_boot(void);

/**
 * @brief Simulate an IWDT timeout reset (firmware crash)
 *
 * @details
 * Sets: IWDTRF=1, PORF=0
 * This simulates a prior firmware hang that triggered IWDT reset.
 */
void mock_system_regs_simulate_iwdt_timeout(void);

/**
 * @brief Simulate a brownout reset (low voltage)
 *
 * @details
 * Sets: LVD0RF=1, PORF=0
 * This simulates a power supply issue causing voltage drop.
 */
void mock_system_regs_simulate_brownout(void);

/* =============================================================================
 * Query Functions - Get current mock state
 * =============================================================================
 */

/**
 * @brief Get current RSTSR0 register value
 * @return uint8_t Current RSTSR0 value with all flags
 */
uint8_t mock_system_regs_get_rstsr0(void);

/**
 * @brief Get current RSTSR1 register value
 * @return uint8_t Current RSTSR1 value with all flags
 */
uint8_t mock_system_regs_get_rstsr1(void);

/**
 * @brief Get current RSTSR2 register value
 * @return uint8_t Current RSTSR2 value with all flags
 */
uint8_t mock_system_regs_get_rstsr2(void);

/**
 * @brief Get number of times rstsr01() was called
 * @return uint32_t Call count for rstsr01() accessor
 */
uint32_t mock_system_regs_get_rstsr01_read_count(void);

/**
 * @brief Get number of times rstsr2() was called
 * @return uint32_t Call count for rstsr2() accessor
 */
uint32_t mock_system_regs_get_rstsr2_read_count(void);

#ifdef __cplusplus
}
#endif
