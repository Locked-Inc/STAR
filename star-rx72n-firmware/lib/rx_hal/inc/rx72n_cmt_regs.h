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

/** @brief CMT base addresses */
typedef enum {
  k_cmt0_base_addr     = 0x00088000, /**< CMT0 register base address */
  k_cmt1_base_addr     = 0x00088008, /**< CMT1 register base address */
  k_cmt2_base_addr     = 0x00088010, /**< CMT2 register base address */
  k_cmt3_base_addr     = 0x00088018, /**< CMT3 register base address */
  k_cmt_ctrl_base_addr = 0x00088002, /**< CMT control register base address */
} rx_cmt_addresses_t;

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

/**
 * @brief Get pointer to CMT0 registers
 * @return Volatile pointer to CMT0 register structure
 */
static inline volatile rx_cmt_channel_regs_t* cmt0(void)
{
  return (volatile rx_cmt_channel_regs_t*)k_cmt0_base_addr;
}

/**
 * @brief Get pointer to CMT1 registers
 * @return Volatile pointer to CMT1 register structure
 */
static inline volatile rx_cmt_channel_regs_t* cmt1(void)
{
  return (volatile rx_cmt_channel_regs_t*)k_cmt1_base_addr;
}

/**
 * @brief Get pointer to CMT2 registers
 * @return Volatile pointer to CMT2 register structure
 */
static inline volatile rx_cmt_channel_regs_t* cmt2(void)
{
  return (volatile rx_cmt_channel_regs_t*)k_cmt2_base_addr;
}

/**
 * @brief Get pointer to CMT3 registers
 * @return Volatile pointer to CMT3 register structure
 */
static inline volatile rx_cmt_channel_regs_t* cmt3(void)
{
  return (volatile rx_cmt_channel_regs_t*)k_cmt3_base_addr;
}

/**
 * @brief Get pointer to CMT control registers
 * @return Volatile pointer to CMT control register structure
 */
static inline volatile rx_cmt_control_regs_t* cmt_ctrl(void)
{
  return (volatile rx_cmt_control_regs_t*)k_cmt_ctrl_base_addr;
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_CMT_REGS_H */
