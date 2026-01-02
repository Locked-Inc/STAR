/* lib/rx_hal/inc/rx72n_cmt_regs.h */

/**
 * @file rx72n_cmt_regs.h
 * @brief RX72N CMT Timer Register Definitions
 *
 * Register definitions for the Compare Match Timer (CMT) used for ThreadX
 * system tick generation.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_CMT_REGS_H
#define STAR_RX72N_CMT_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Compare Match Timer (CMT) - For ThreadX System Tick
 * =============================================================================
 */

/**
 * @brief CMT Channel Register Map
 * @details
 * Compare Match Timer (CMT) channel registers for periodic interrupts.
 * Used for ThreadX system tick generation at 100 Hz.
 * Base addresses:
 * - CMT0: 0x00088000
 * - CMT1: 0x00088008
 * - CMT2: 0x00088010
 * - CMT3: 0x00088018
 */
typedef struct {
  volatile uint16_t cmcr;  /**< Compare Match Timer Control Register (clock, interrupt) */
  volatile uint16_t cmcnt; /**< Compare Match Timer Counter (current count) */
  volatile uint16_t cmcor; /**< Compare Match Timer Compare Register (match value) */
} rx_cmt_channel_regs_t;

/**
 * @brief CMT Control Register Map
 * @details
 * CMT start/stop control registers.
 * Base address: 0x00088002
 */
typedef struct {
  volatile uint16_t cmstr0; /**< Compare Match Timer Start Register 0 (CMT0/1) */
  volatile uint16_t cmstr1; /**< Compare Match Timer Start Register 1 (CMT2/3) */
} rx_cmt_control_regs_t;

#define CMT0_BASE     ((rx_cmt_channel_regs_t*)0x00088000)
#define CMT1_BASE     ((rx_cmt_channel_regs_t*)0x00088008)
#define CMT2_BASE     ((rx_cmt_channel_regs_t*)0x00088010)
#define CMT3_BASE     ((rx_cmt_channel_regs_t*)0x00088018)
#define CMT_CTRL_BASE ((rx_cmt_control_regs_t*)0x00088002)

#define CMT0     (*CMT0_BASE)
#define CMT1     (*CMT1_BASE)
#define CMT2     (*CMT2_BASE)
#define CMT3     (*CMT3_BASE)
#define CMT_CTRL (*CMT_CTRL_BASE)

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_CMT_REGS_H */
