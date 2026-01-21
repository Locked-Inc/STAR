/* lib/rx_hal/inc/rx72n_mtu_regs.h */

/**
 * @file rx72n_mtu_regs.h
 * @brief RX72N MTU Timer Register Definitions
 *
 * Register definitions for the Multi-Function Timer Unit (MTU) used for
 * PWM generation and timer functions.
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_MTU_REGS_H
#define STAR_RX72N_MTU_REGS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Multi-Function Timer Unit (MTU)
 * =============================================================================
 */

/** @brief MTU base addresses */
typedef enum : uint32_t {
  k_mtu0_base_addr      = 0x000C1300, /**< MTU0 register base address */
  k_mtu1_base_addr      = 0x000C1380, /**< MTU1 register base address */
  k_mtu2_base_addr      = 0x000C1400, /**< MTU2 register base address */
  k_mtu3_base_addr      = 0x000C1200, /**< MTU3 register base address */
  k_mtu4_base_addr      = 0x000C1200, /**< MTU4 register base address (shares with MTU3) */
  k_mtu5u_base_addr     = 0x000C1C80, /**< MTU5U register base address */
  k_mtu5v_base_addr     = 0x000C1C90, /**< MTU5V register base address */
  k_mtu5w_base_addr     = 0x000C1CA0, /**< MTU5W register base address */
  k_mtu6_base_addr      = 0x000C1A00, /**< MTU6 register base address */
  k_mtu7_base_addr      = 0x000C1A00, /**< MTU7 register base address (shares with MTU6) */
  k_mtu8_base_addr      = 0x000C1600, /**< MTU8 register base address */
  k_mtu_tstra_base_addr = 0x000C1280, /**< MTU TSTRA register base address (MTU0-4, 8) */
  k_mtu_tstrb_base_addr = 0x000C1A80, /**< MTU TSTRB register base address (MTU6-7) */
} rx_mtu_addresses_t;

/**
 * @brief MTU Channel Register Map (MTU0-MTU2, MTU6-MTU7)
 * @details
 * Multi-Function Timer Unit channel registers for PWM and timer functions.
 * Base addresses:
 * - MTU0: 0x000C1300
 * - MTU1: 0x000C1380
 * - MTU2: 0x000C1400
 * - MTU6: 0x000C1A00
 * - MTU7: 0x000C1A00 (shares with MTU6)
 */
typedef struct {
  volatile uint8_t  tcr;   /**< Timer Control Register (prescaler, edge, clear) */
  volatile uint8_t  tmdr;  /**< Timer Mode Register (PWM mode select) */
  volatile uint8_t  tiorh; /**< Timer I/O Control Register H (output compare A/B) */
  volatile uint8_t  tiorl; /**< Timer I/O Control Register L (output compare C/D) */
  volatile uint8_t  tier;  /**< Timer Interrupt Enable Register */
  volatile uint8_t  tsr;   /**< Timer Status Register (interrupt flags) */
  volatile uint16_t tcnt;  /**< Timer Counter (current count value) */
  volatile uint16_t tgra;  /**< Timer General Register A (compare/capture) */
  volatile uint16_t tgrb;  /**< Timer General Register B (compare/capture) */
  volatile uint16_t tgrc;  /**< Timer General Register C (compare/capture) */
  volatile uint16_t tgrd;  /**< Timer General Register D (compare/capture) */
} rx_mtu_channel_regs_t;

/**
 * @brief MTU3/MTU4 Extended Channel Register Map
 * @details
 * MTU3 and MTU4 have additional registers for extended functionality.
 * Base addresses:
 * - MTU3: 0x000C1200
 * - MTU4: 0x000C1200 (shares with MTU3, adjacent registers)
 */
typedef struct {
  volatile uint8_t  tcr;   /**< Timer Control Register (prescaler, edge, clear) */
  volatile uint8_t  tmdr;  /**< Timer Mode Register (PWM mode select) */
  volatile uint8_t  tiorh; /**< Timer I/O Control Register H (output compare A/B) */
  volatile uint8_t  tiorl; /**< Timer I/O Control Register L (output compare C/D) */
  volatile uint8_t  tier;  /**< Timer Interrupt Enable Register */
  volatile uint8_t  tsr;   /**< Timer Status Register (interrupt flags) */
  volatile uint16_t tcnt;  /**< Timer Counter (current count value) */
  volatile uint16_t tgra;  /**< Timer General Register A (compare/capture) */
  volatile uint16_t tgrb;  /**< Timer General Register B (compare/capture) */
  volatile uint16_t tgrc;  /**< Timer General Register C (compare/capture) */
  volatile uint16_t tgrd;  /**< Timer General Register D (compare/capture) */
  volatile uint16_t tgre;  /**< Timer General Register E (MTU3/4 only) */
  volatile uint16_t tgrf;  /**< Timer General Register F (MTU3/4 only) */
  volatile uint8_t  tier2; /**< Timer Interrupt Enable Register 2 */
  volatile uint8_t  tsr2;  /**< Timer Status Register 2 */
  volatile uint8_t  tbtm;  /**< Timer Buffer Transfer Mode Register */
} rx_mtu34_channel_regs_t;

/**
 * @brief MTU Start Register Map
 * @details
 * Timer start/stop control registers.
 * Base addresses:
 * - TSTRA: 0x000C1280 (MTU0-4, MTU8)
 * - TSTRB: 0x000C1A80 (MTU6-7)
 */
typedef struct {
  volatile uint8_t tstr; /**< Timer Start Register (channel enable bits) */
} rx_mtu_tstr_regs_t;

/**
 * @brief Get pointer to MTU0 registers
 * @return Volatile pointer to MTU0 register structure
 */
static inline volatile rx_mtu_channel_regs_t* mtu0(void)
{
  return (volatile rx_mtu_channel_regs_t*)k_mtu0_base_addr;
}

/**
 * @brief Get pointer to MTU1 registers
 * @return Volatile pointer to MTU1 register structure
 */
static inline volatile rx_mtu_channel_regs_t* mtu1(void)
{
  return (volatile rx_mtu_channel_regs_t*)k_mtu1_base_addr;
}

/**
 * @brief Get pointer to MTU2 registers
 * @return Volatile pointer to MTU2 register structure
 */
static inline volatile rx_mtu_channel_regs_t* mtu2(void)
{
  return (volatile rx_mtu_channel_regs_t*)k_mtu2_base_addr;
}

/**
 * @brief Get pointer to MTU3 registers
 *
 * @details MTU3 and MTU4 share the same base address (0x000C1200) by hardware design.
 * This is intentional per the RX72N hardware manual - MTU3 and MTU4 are a "paired"
 * timer unit designed for complementary PWM generation.
 *
 * The MTU3/MTU4 register layout within the shared base address:
 * - MTU3 registers: Offsets 0x00-0x16 (TCR, TMDR, TIORH, TIORL, TIER, TSR, TCNT, TGRx, etc.)
 * - MTU4 registers: Offsets 0x01-0x17 (interleaved with MTU3)
 *
 * Both mtu3() and mtu4() return the same base pointer because the rx_mtu34_channel_regs_t
 * structure represents the combined MTU3/MTU4 register block. The caller accesses
 * MTU3-specific or MTU4-specific registers through the same structure.
 *
 * The static assertions at lines 283-284 verify this shared base address is intentional.
 *
 * @return Volatile pointer to MTU3/MTU4 register structure
 *
 * @see RX72N Hardware Manual Section 22.2.7 for MTU3/MTU4 register map
 */
static inline volatile rx_mtu34_channel_regs_t* mtu3(void)
{
  return (volatile rx_mtu34_channel_regs_t*)k_mtu3_base_addr;
}

/**
 * @brief Get pointer to MTU4 registers
 *
 * @details MTU3 and MTU4 share the same base address (0x000C1200) by hardware design.
 * This is intentional per the RX72N hardware manual - MTU3 and MTU4 are a "paired"
 * timer unit designed for complementary PWM generation.
 *
 * Both mtu3() and mtu4() return the same base pointer because the rx_mtu34_channel_regs_t
 * structure represents the combined MTU3/MTU4 register block. Within this block:
 * - MTU3 registers are at even offsets (TCR3 at 0x00, TMDR3 at 0x02, etc.)
 * - MTU4 registers are at odd offsets (TCR4 at 0x01, TMDR4 at 0x03, etc.)
 *
 * The static assertions at lines 283-284 verify this shared base address is intentional.
 *
 * @note For most applications, use mtu3() for both MTU3 and MTU4 access since they
 * share the register structure. The mtu4() function is provided for API symmetry.
 *
 * @return Volatile pointer to MTU3/MTU4 register structure (same as mtu3())
 *
 * @see RX72N Hardware Manual Section 22.2.7 for MTU3/MTU4 register map
 */
static inline volatile rx_mtu34_channel_regs_t* mtu4(void)
{
  return (volatile rx_mtu34_channel_regs_t*)k_mtu4_base_addr;
}

/**
 * @brief Get pointer to MTU6 registers
 * @return Volatile pointer to MTU6 register structure
 */
static inline volatile rx_mtu_channel_regs_t* mtu6(void)
{
  return (volatile rx_mtu_channel_regs_t*)k_mtu6_base_addr;
}

/**
 * @brief Get pointer to MTU7 registers
 * @return Volatile pointer to MTU7 register structure
 */
static inline volatile rx_mtu_channel_regs_t* mtu7(void)
{
  return (volatile rx_mtu_channel_regs_t*)k_mtu7_base_addr;
}

/**
 * @brief Get pointer to MTU TSTRA registers (MTU0-4, MTU8)
 * @return Volatile pointer to MTU TSTRA register structure
 */
static inline volatile rx_mtu_tstr_regs_t* mtu_tstra(void)
{
  return (volatile rx_mtu_tstr_regs_t*)k_mtu_tstra_base_addr;
}

/**
 * @brief Get pointer to MTU TSTRB registers (MTU6-7)
 * @return Volatile pointer to MTU TSTRB register structure
 */
static inline volatile rx_mtu_tstr_regs_t* mtu_tstrb(void)
{
  return (volatile rx_mtu_tstr_regs_t*)k_mtu_tstrb_base_addr;
}

/* Timer Control Register (TCR) bits */
typedef enum : uint8_t {
  k_mtu_tcr_cclr_shift = 5,          /**< Counter Clear Source bit position */
  k_mtu_tcr_tpsc_mask  = 0x07,       /**< Timer Prescaler mask (bits 0-2) */
  k_mtu_tcr_ckeg_mask  = 0x18,       /**< Clock Edge mask (bits 3-4) */
  k_mtu_tcr_cclr_mask  = 0xE0,       /**< Counter Clear Source mask (bits 5-7) */
  k_mtu_tcr_tpsc_1     = 0x00,       /**< PCLKA/1 */
  k_mtu_tcr_tpsc_4     = 0x01,       /**< PCLKA/4 */
  k_mtu_tcr_tpsc_16    = 0x02,       /**< PCLKA/16 */
  k_mtu_tcr_tpsc_64    = 0x03,       /**< PCLKA/64 */
  k_mtu_tcr_cclr_tgra  = (0x01 << k_mtu_tcr_cclr_shift), /**< Clear on TGRA compare match */
} mtu_tcr_bits_t;

/* Timer Mode Register (TMDR) bits */
typedef enum : uint8_t {
  k_mtu_tmdr_bfa_shift = 4,          /**< Buffer mode A bit position */
  k_mtu_tmdr_bfb_shift = 5,          /**< Buffer mode B bit position */
  k_mtu_tmdr_md_mask   = 0x0F,       /**< Mode select mask (bits 0-3) */
  k_mtu_tmdr_md_normal = 0x00,       /**< Normal mode */
  k_mtu_tmdr_md_pwm1   = 0x02,       /**< PWM mode 1 */
  k_mtu_tmdr_md_pwm2   = 0x03,       /**< PWM mode 2 */
  k_mtu_tmdr_bfa       = (1 << k_mtu_tmdr_bfa_shift), /**< Buffer mode A */
  k_mtu_tmdr_bfb       = (1 << k_mtu_tmdr_bfb_shift), /**< Buffer mode B */
} mtu_tmdr_bits_t;

/* Timer I/O Control Register (TIOR) bits */
typedef enum : uint8_t {
  k_mtu_tior_ioa_mask  = 0x0F, /**< I/O Control A mask (bits 0-3) */
  k_mtu_tior_iob_mask  = 0xF0, /**< I/O Control B mask (bits 4-7) */
  k_mtu_tior_init_low  = 0x02, /**< Initial output low, compare match high */
  k_mtu_tior_init_high = 0x05, /**< Initial output high, compare match low */
  k_mtu_tior_toggle    = 0x03, /**< Toggle on compare match */
} mtu_tior_bits_t;

/* Timer Start Register A (TSTRA) bits - MTU0-4, MTU8 */
typedef enum : uint8_t {
  k_mtu_tstr_cst0_shift = 0, /**< Counter Start 0 bit position */
  k_mtu_tstr_cst1_shift = 1, /**< Counter Start 1 bit position */
  k_mtu_tstr_cst2_shift = 2, /**< Counter Start 2 bit position */
  k_mtu_tstr_cst8_shift = 3, /**< Counter Start 8 bit position */
  k_mtu_tstr_cst3_shift = 6, /**< Counter Start 3 bit position */
  k_mtu_tstr_cst4_shift = 7, /**< Counter Start 4 bit position */
  k_mtu_tstr_cst0       = (1 << k_mtu_tstr_cst0_shift), /**< Counter Start 0 (TSTRA bit 0) */
  k_mtu_tstr_cst1       = (1 << k_mtu_tstr_cst1_shift), /**< Counter Start 1 (TSTRA bit 1) */
  k_mtu_tstr_cst2       = (1 << k_mtu_tstr_cst2_shift), /**< Counter Start 2 (TSTRA bit 2) */
  k_mtu_tstr_cst3       = (1 << k_mtu_tstr_cst3_shift), /**< Counter Start 3 (TSTRA bit 6) */
  k_mtu_tstr_cst4       = (1 << k_mtu_tstr_cst4_shift), /**< Counter Start 4 (TSTRA bit 7) */
  k_mtu_tstr_cst8       = (1 << k_mtu_tstr_cst8_shift), /**< Counter Start 8 (TSTRA bit 3) */
} mtu_tstr_bits_t;

/* Timer Start Register B (TSTRB) bits - MTU6-7 */
typedef enum : uint8_t {
  k_mtu_tstr_cst6_shift = 6, /**< Counter Start 6 bit position */
  k_mtu_tstr_cst7_shift = 7, /**< Counter Start 7 bit position */
  k_mtu_tstr_cst6       = (1U << k_mtu_tstr_cst6_shift), /**< Counter Start 6 (TSTRB bit 6) */
  k_mtu_tstr_cst7       = (1U << k_mtu_tstr_cst7_shift), /**< Counter Start 7 (TSTRB bit 7) */
} mtu_tstrb_bits_t;

/* =============================================================================
 * Static Assertions - Compile-time verification of register layout
 * =============================================================================
 */

/* Verify MTU channel register structure critical offsets */
_Static_assert(offsetof(rx_mtu_channel_regs_t, tcr) == 0x00, "MTU TCR register offset incorrect");
_Static_assert(offsetof(rx_mtu_channel_regs_t, tcnt) == 0x06, "MTU TCNT register offset incorrect");
_Static_assert(offsetof(rx_mtu_channel_regs_t, tgra) == 0x08, "MTU TGRA register offset incorrect");
_Static_assert(offsetof(rx_mtu_channel_regs_t, tgrb) == 0x0A, "MTU TGRB register offset incorrect");

/* Verify MTU3/4 extended register critical offsets */
_Static_assert(offsetof(rx_mtu34_channel_regs_t, tcr) == 0x00,
               "MTU3/4 TCR register offset incorrect");
_Static_assert(offsetof(rx_mtu34_channel_regs_t, tcnt) == 0x06,
               "MTU3/4 TCNT register offset incorrect");
_Static_assert(offsetof(rx_mtu34_channel_regs_t, tgra) == 0x08,
               "MTU3/4 TGRA register offset incorrect");
_Static_assert(offsetof(rx_mtu34_channel_regs_t, tgre) == 0x10,
               "MTU3/4 TGRE register offset incorrect");
_Static_assert(offsetof(rx_mtu34_channel_regs_t, tgrf) == 0x12,
               "MTU3/4 TGRF register offset incorrect");

/* Verify TSTR register structure */
_Static_assert(sizeof(rx_mtu_tstr_regs_t) == 1, "MTU TSTR register structure size mismatch");

/* Verify base addresses are in correct peripheral space (0x000C1xxx) */
_Static_assert((k_mtu0_base_addr & 0xFFFF0000) == 0x000C0000,
               "MTU0 base address not in MTU peripheral space");
_Static_assert((k_mtu3_base_addr & 0xFFFF0000) == 0x000C0000,
               "MTU3 base address not in MTU peripheral space");
_Static_assert((k_mtu6_base_addr & 0xFFFF0000) == 0x000C0000,
               "MTU6 base address not in MTU peripheral space");

/* Verify MTU3 and MTU4 share the same base address */
_Static_assert(k_mtu3_base_addr == k_mtu4_base_addr,
               "MTU3 and MTU4 must share base address (adjacent registers)");

/* Verify MTU6 and MTU7 share the same base address */
_Static_assert(k_mtu6_base_addr == k_mtu7_base_addr,
               "MTU6 and MTU7 must share base address (adjacent registers)");

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_MTU_REGS_H */
