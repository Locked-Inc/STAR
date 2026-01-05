/* tests/mocks/mock_rx_onewire_hw.h */

/**
 * @file mock_rx_onewire_hw.h
 * @brief Mock Hardware Stubs for OneWire Testing
 *
 * Provides stub implementations of hardware-specific functions
 * used by the OneWire driver (CMT timer, system registers, etc.)
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_ONEWIRE_HW_H
#define MOCK_RX_ONEWIRE_HW_H

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
  volatile uint16_t cmcr;   /**< Compare Match Timer Control Register */
  volatile uint16_t cmcnt;  /**< Compare Match Timer Counter */
  volatile uint16_t cmcor;  /**< Compare Match Timer Constant Register */
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
  volatile uint16_t prcr;     /**< Protect Register */
  volatile uint32_t mstpcrb;  /**< Module Stop Control Register B */
} mock_system_regs_t;

/* =============================================================================
 * Mock Hardware Access Functions
 * =============================================================================
 */

/**
 * @brief Get CMT3 register pointer
 */
mock_cmt_channel_t* cmt3(void);

/**
 * @brief Get CMT control register pointer
 */
mock_cmt_ctrl_t* cmt_ctrl(void);

/**
 * @brief Get system register pointer
 */
mock_system_regs_t* system_regs(void);

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

#endif /* MOCK_RX_ONEWIRE_HW_H */
