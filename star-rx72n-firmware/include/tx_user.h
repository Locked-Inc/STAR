/* include/tx_user.h */

/**
 * @file tx_user.h
 * @brief ThreadX user configuration for RX72N
 * @details
 * Minimal configuration for ThreadX RTOS on RX72N.
 * Based on STAR project requirements.
 *
 * @date 2025-12-31
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef TX_USER_H
#define TX_USER_H

/* =============================================================================
 * System Tick Configuration (100 Hz = 10ms tick)
 * =============================================================================
 */

#define TX_TIMER_TICKS_PER_SECOND 100 /* 100 Hz system tick */

/* =============================================================================
 * RX72N CMT (Compare Match Timer) Configuration
 * =============================================================================
 * CMT0 is used for ThreadX system tick
 * PCLKB = 60 MHz
 * Divider = 128
 * Desired frequency = 100 Hz (10ms)
 * CMCOR = (PCLKB / divider / frequency) - 1
 *       = (60000000 / 128 / 100) - 1
 *       = 4687
 */

#define TX_RX72N_CMT_DIVIDER  128  /* CMT clock divider */
#define TX_RX72N_CMT_CMCOR    4687 /* Compare match value */
#define TX_RX72N_CMT_PRIORITY 3    /* Interrupt priority (0-15) */

/* =============================================================================
 * ThreadX Features
 * =============================================================================
 */

#define TX_ENABLE_STACK_CHECKING /* Catch stack overflow */
#define TX_TIMER_PROCESS_IN_ISR  /* Timer processing in ISR */

#endif /* TX_USER_H */
