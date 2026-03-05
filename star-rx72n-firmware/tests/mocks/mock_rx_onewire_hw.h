// SPDX-License-Identifier: MIT
/* tests/mocks/mock_rx_onewire_hw.h */

/**
 * @file mock_rx_onewire_hw.h
 * @brief Mock 1-Wire hardware layer for DS18B20 temperature sensor testing
 *
 * @details
 * Provides test double for 1-Wire protocol hardware (CMT timer, GPIO) to
 * enable unit testing of DS18B20 temperature sensor driver without RX72N
 * hardware. Simulates timing, bit-level communication, and device responses.
 *
 * Enables testing of:
 * - 1-Wire initialization (reset/presence pulse)
 * - Bit-level write operations (write-0, write-1)
 * - Bit-level read operations (read time slots)
 * - DS18B20 command sequences (Convert T, Read Scratchpad)
 * - CRC validation
 * - Timing-critical protocol compliance
 *
 * @par Test Architecture:
 * @dot
 * digraph mock_onewire {
 *   rankdir=LR;
 *   test [label="DS18B20\nTests"];
 *   mock [label="Mock 1-Wire HW\n(CMT timer + GPIO)"];
 *   real [label="Real 1-Wire\nBus", style=dashed];
 *   test -> mock [label="Simulate responses"];
 *   mock -> real [style=dashed, label="(replaced)"];
 * }
 * @enddot
 *
 * @par Mock Capabilities:
 * | Feature | Supported | Description |
 * |---------|-----------|-------------|
 * | Timing simulation | Yes | CMT timer for us delays |
 * | Bit I/O | Yes | GPIO-level wire control |
 * | Device response | Yes | Simulated DS18B20 replies |
 *
 * @par Usage: tests/test_rx_ds18b20.c
 *
 * @see rx_onewire.h Real 1-Wire driver
 * @see rx_ds18b20.h DS18B20 sensor driver
 * @see tests/test_rx_ds18b20.c Temperature sensor tests
 *
 * @par NASA Power of 10: [OK] Static allocation
 * @par SOLID: D - Dependency Inversion
 *
 * @author STAR Team
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * CMT Timer Mock Structures
 * =============================================================================
 */

/**
 * @brief Mock CMT channel register structure
 */
typedef struct {
  volatile uint16_t cmcr;  /**< Compare Match Timer Control Register */
  volatile uint16_t cmcnt; /**< Compare Match Timer Counter */
  volatile uint16_t cmcor; /**< Compare Match Timer Constant Register */
} mock_cmt_channel_t;

/**
 * @brief Mock CMT control register structure
 */
typedef struct {
  volatile uint16_t cmstr0; /**< Compare Match Start Register 0 */
  volatile uint16_t cmstr1; /**< Compare Match Start Register 1 */
} mock_cmt_ctrl_t;

/**
 * @brief Mock System registers
 */
typedef struct {
  volatile uint16_t prcr;    /**< Protect Register */
  volatile uint32_t mstpcra; /**< Module Stop Control Register A */
  volatile uint32_t mstpcrb; /**< Module Stop Control Register B */
  volatile uint32_t mstpcrc; /**< Module Stop Control Register C */
  volatile uint32_t mstpcrd; /**< Module Stop Control Register D */
} mock_system_regs_t;

/* =============================================================================
 * Mock Hardware State
 * =============================================================================
 */

/**
 * @brief Timer constants for auto-increment behavior
 */
typedef enum : uint8_t {
  k_mock_onewire_timer_auto_increment = 100, /**< Timer ticks added on each access */
} mock_onewire_timer_constants_t;

extern mock_cmt_channel_t g_mock_cmt3;
extern mock_cmt_ctrl_t    g_mock_cmt_ctrl;
extern mock_system_regs_t g_mock_onewire_system_regs;
extern uint16_t           g_mock_cmt3_counter_shadow;

/* =============================================================================
 * Mock Hardware Access Functions
 * =============================================================================
 */

/**
 * @brief Get CMT3 register pointer
 */
static inline volatile mock_cmt_channel_t* cmt3(void)
{
  g_mock_cmt3_counter_shadow =
    (uint16_t)(g_mock_cmt3_counter_shadow + k_mock_onewire_timer_auto_increment);
  g_mock_cmt3.cmcnt = g_mock_cmt3_counter_shadow;

  return &g_mock_cmt3;
}

/**
 * @brief Get CMT control register pointer
 */
static inline volatile mock_cmt_ctrl_t* cmt_ctrl(void)
{
  return &g_mock_cmt_ctrl;
}

#if !defined(MOCK_RX_MTU_REGS_H)
/**
 * @brief Get system register pointer
 */
static inline volatile mock_system_regs_t* system_regs(void)
{
  return &g_mock_onewire_system_regs;
}
#endif

/**
 * @brief Initialize mock hardware state
 */
void mock_onewire_hw_init(void);

/**
 * @brief Deinitialize mock hardware state
 */
void mock_onewire_hw_deinit(void);

/**
 * @brief Set simulated timer counter value
 *
 * Used to control timing behavior in tests.
 *
 * @param[in] count Counter value
 */
void mock_onewire_hw_set_timer_count(uint16_t count);

/**
 * @brief Advance simulated timer counter
 *
 * @param[in] ticks Number of ticks to advance
 */
void mock_onewire_hw_advance_timer(uint16_t ticks);

#ifdef __cplusplus
}
#endif
