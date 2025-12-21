/* include/tx_user.h */

/**
 * @file tx_user.h
 * @brief ThreadX User Configuration
 *
 * This file contains application-specific configuration options for ThreadX.
 * It is included before tx_api.h to override default ThreadX settings.
 */

#ifndef STAR_RX72N_TX_USER_H
#define STAR_RX72N_TX_USER_H

/* =============================================================================
 * Basic Configuration
 * =============================================================================
 */

/* Define the system timer frequency (Hz) */
#define TX_TIMER_TICKS_PER_SECOND 100

/* Maximum thread priority levels (0 = highest, 31 = lowest) */
#define TX_MAX_PRIORITIES 32

/* Minimum stack size (bytes) */
#define TX_MINIMUM_STACK 200

/* =============================================================================
 * Feature Disabling (reduce code size)
 * =============================================================================
 */

/* Disable unused features to save Flash/RAM */
#define TX_DISABLE_ERROR_CHECKING       /* Remove parameter checking */
#define TX_DISABLE_PREEMPTION_THRESHOLD /* Disable preemption threshold */
#define TX_DISABLE_REDUNDANT_CLEARING   /* Don't clear control blocks */
#define TX_DISABLE_NOTIFY_CALLBACKS     /* Disable notify callbacks */
#define TX_NOT_INTERRUPTABLE            /* Non-interruptible service calls */

/* =============================================================================
 * Performance Optimizations
 * =============================================================================
 */

/* Inline common ThreadX functions for speed */
#define TX_INLINE_THREAD_RESUME_SUSPEND

/* Use assembly optimizations where available */
#define TX_ENABLE_STACK_CHECKING /* Detect stack overflow */

/* =============================================================================
 * Memory Configuration
 * =============================================================================
 */

/* Thread stack sizes */
#define TX_THREAD_USER_EXTENSION /* Allow user data in thread control block */

/* =============================================================================
 * Debug Configuration (disable in Release builds)
 * =============================================================================
 */

#ifdef DEBUG
/* Enable runtime stack checking in debug builds */
#undef TX_DISABLE_ERROR_CHECKING
#define TX_ENABLE_EVENT_TRACE             /* Enable TraceX support */
#define TX_TRACE_BUFFER_SIZE  (64 * 1024) /* 64KB trace buffer */
#endif

/* =============================================================================
 * RX72N-Specific Configuration
 * =============================================================================
 */

/* System clock frequency (for timing calculations) */
#define TX_RX72N_ICLK_HZ  240000000UL /* 240 MHz CPU clock */
#define TX_RX72N_PCLKB_HZ 60000000UL  /* 60 MHz peripheral clock B (CMT) */

/* CMT0 configuration for system tick */
#define TX_RX72N_CMT_CHANNEL  0  /* Use CMT0 for ThreadX tick */
#define TX_RX72N_CMT_VECTOR   28 /* CMT0 interrupt vector */
#define TX_RX72N_CMT_PRIORITY 5  /* Interrupt priority (0-15) */

/* Calculate CMT compare value for desired tick rate
 * CMT runs on PCLKB with divider of 128 (to fit in uint16_t)
 * CMCOR = (PCLKB / divider / tick_rate) - 1
 * CMCOR = (60,000,000 / 128 / 100) - 1 = 4687
 */
#define TX_RX72N_CMT_DIVIDER 128
#define TX_RX72N_CMT_CMCOR                                                                         \
  ((TX_RX72N_PCLKB_HZ / TX_RX72N_CMT_DIVIDER / TX_TIMER_TICKS_PER_SECOND) - 1)

#endif /* STAR_RX72N_TX_USER_H */
