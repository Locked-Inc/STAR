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

typedef struct {
  volatile uint16_t CMCR;  /* Compare Match Timer Control Register */
  volatile uint16_t CMCNT; /* Compare Match Timer Counter */
  volatile uint16_t CMCOR; /* Compare Match Timer Compare Register */
} rx_cmt_channel_regs_t;

typedef struct {
  volatile uint16_t CMSTR0; /* Compare Match Timer Start Register 0 */
  volatile uint16_t CMSTR1; /* Compare Match Timer Start Register 1 */
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
