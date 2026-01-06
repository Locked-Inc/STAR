/* lib/rx_hal/inc/rx72n_gptw_regs.h */

/**
 * @file rx72n_gptw_regs.h
 * @brief RX72N GPTW (General PWM Timer) Register Definitions
 * @details
 * Register definitions for the General PWM Timer (GPTW) used for motor PWM
 * generation. The GPTW provides 32-bit resolution and is optimized for
 * motor control applications.
 *
 * @warning Base addresses are derived from the hirakuni45/RX open-source
 * framework. These addresses MUST be verified against the official RX72N
 * Hardware Manual (R01UH0883EJ) before production use.
 *
 * @see https://github.com/hirakuni45/RX/blob/master/RX600/gptw.hpp
 * @see RX72N Hardware Manual Section 20: General PWM Timer (GPT)
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_GPTW_REGS_H
#define STAR_RX72N_GPTW_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * General PWM Timer (GPTW) - 32-bit PWM for Motor Control
 * =============================================================================
 *
 * GPTW Features:
 * - 32-bit counter resolution (vs MTU's 16-bit)
 * - 4 independent channels (GPTW0-GPTW3)
 * - Each channel has 2 outputs (GTIOCA, GTIOCB)
 * - Optimized for motor PWM generation
 * - Dead-time control for H-bridge safety
 *
 * Pin Mapping (Port E):
 * - GPTW0: PE5/GTIOC0A, PE2/GTIOC0B
 * - GPTW1: PE4/GTIOC1A, PE1/GTIOC1B
 * - GPTW2: PE3/GTIOC2A, PE0/GTIOC2B
 * - GPTW3: PE7/GTIOC3A, PE6/GTIOC3B
 */

/**
 * @brief GPTW Channel Register Map
 * @details
 * General PWM Timer channel registers for 32-bit PWM generation.
 *
 * @warning Base addresses derived from hirakuni45/RX framework.
 * Verify against RX72N Hardware Manual before production use.
 *
 * Channel base addresses (tentative - needs verification):
 * - GPTW0: 0x000D4000
 * - GPTW1: 0x000D4100
 * - GPTW2: 0x000D4200
 * - GPTW3: 0x000D4300
 */
typedef struct {
  volatile uint32_t gtwp;      /**< 0x00: Write Protection Register */
  volatile uint32_t gtstr;     /**< 0x04: Software Start Register */
  volatile uint32_t gtstp;     /**< 0x08: Software Stop Register */
  volatile uint32_t gtclr;     /**< 0x0C: Software Clear Register */
  volatile uint32_t gtssr;     /**< 0x10: Start Source Select Register */
  volatile uint32_t gtpsr;     /**< 0x14: Stop Source Select Register */
  volatile uint32_t gtcsr;     /**< 0x18: Clear Source Select Register */
  volatile uint32_t gtupsr;    /**< 0x1C: Count-Up Source Select Register */
  volatile uint32_t gtdnsr;    /**< 0x20: Count-Down Source Select Register */
  volatile uint32_t gticasr;   /**< 0x24: Input Capture Source Select A */
  volatile uint32_t gticbsr;   /**< 0x28: Input Capture Source Select B */
  volatile uint32_t gtcr;      /**< 0x2C: Control Register */
  volatile uint32_t gtuddtyc;  /**< 0x30: Up/Down Count Duty Setting */
  volatile uint32_t gtior;     /**< 0x34: I/O Control Register */
  volatile uint32_t gtintad;   /**< 0x38: Interrupt Output Setting */
  volatile uint32_t gtst;      /**< 0x3C: Status Register */
  volatile uint32_t gtber;     /**< 0x40: Buffer Enable Register */
  volatile uint32_t gtitc;     /**< 0x44: Interrupt/ADC Trigger Control */
  volatile uint32_t gtcnt;     /**< 0x48: Counter */
  volatile uint32_t gtccra;    /**< 0x4C: Compare Capture Register A */
  volatile uint32_t gtccrb;    /**< 0x50: Compare Capture Register B */
  volatile uint32_t gtccrc;    /**< 0x54: Compare Capture Register C */
  volatile uint32_t gtccre;    /**< 0x58: Compare Capture Register E */
  volatile uint32_t gtccrd;    /**< 0x5C: Compare Capture Register D */
  volatile uint32_t gtccrf;    /**< 0x60: Compare Capture Register F */
  volatile uint32_t gtpr;      /**< 0x64: Cycle Setting Register (Period) */
  volatile uint32_t gtpbr;     /**< 0x68: Cycle Setting Buffer Register */
  volatile uint32_t gtpdbr;    /**< 0x6C: Cycle Setting Double Buffer */
  volatile uint32_t gtadtra;   /**< 0x70: A/D Trigger Register A */
  volatile uint32_t gtadtbra;  /**< 0x74: A/D Trigger Buffer Register A */
  volatile uint32_t gtadtdbra; /**< 0x78: A/D Trigger Double Buffer A */
  volatile uint32_t gtadtrb;   /**< 0x7C: A/D Trigger Register B */
  volatile uint32_t gtadtbrb;  /**< 0x80: A/D Trigger Buffer Register B */
  volatile uint32_t gtadtdbrb; /**< 0x84: A/D Trigger Double Buffer B */
  volatile uint32_t gtdtcr;    /**< 0x88: Dead Time Control Register */
  volatile uint32_t gtdvu;     /**< 0x8C: Dead Time Value Upper */
  volatile uint32_t gtdvd;     /**< 0x90: Dead Time Value Down */
  volatile uint32_t gtdbu;     /**< 0x94: Dead Time Buffer Upper */
  volatile uint32_t gtdbd;     /**< 0x98: Dead Time Buffer Down */
  volatile uint32_t gtsos;     /**< 0x9C: Output Protection */
  volatile uint32_t gtsotr;    /**< 0xA0: Output Protection Trigger */
} rx_gptw_channel_regs_t;

/**
 * @brief GPTW Common Register Map
 * @details
 * Shared registers for all GPTW channels (start/stop control).
 * Base address: 0x000D3000 (tentative - needs verification)
 */
typedef struct {
  volatile uint32_t gtstra;       /**< 0x00: General Timer Start Register A */
  volatile uint32_t gtstpa;       /**< 0x04: General Timer Stop Register A */
  volatile uint32_t gtclra;       /**< 0x08: General Timer Clear Register A */
  uint32_t          reserved0[5]; /**< Reserved */
  volatile uint32_t gtstra2;      /**< 0x20: General Timer Start Register A2 */
  volatile uint32_t gtstpa2;      /**< 0x24: General Timer Stop Register A2 */
  volatile uint32_t gtclra2;      /**< 0x28: General Timer Clear Register A2 */
} rx_gptw_common_regs_t;

/* =============================================================================
 * Base Address Definitions
 * =============================================================================
 *
 * WARNING: These addresses are derived from hirakuni45/RX framework and
 * need verification against the official RX72N Hardware Manual.
 */

/** @brief GPTW hardware addresses (tentative - verify against HW manual) */
typedef enum {
  k_gptw_channel_offset = 0x100,      /**< Channel spacing between GPTW registers */
  k_gptw_common_addr    = 0x000D3000, /**< GPTW common registers base */
  k_gptw0_base_addr     = 0x000D4000, /**< GPTW channel 0 base address */
  k_gptw1_base_addr     = 0x000D4100, /**< GPTW channel 1 base address */
  k_gptw2_base_addr     = 0x000D4200, /**< GPTW channel 2 base address */
  k_gptw3_base_addr     = 0x000D4300, /**< GPTW channel 3 base address */
} gptw_addresses_t;

/** @brief GPTW register inline accessor functions */
static inline volatile rx_gptw_common_regs_t* gptw_common(void)
{
  return (volatile rx_gptw_common_regs_t*)k_gptw_common_addr;
}

static inline volatile rx_gptw_channel_regs_t* gptw0(void)
{
  return (volatile rx_gptw_channel_regs_t*)k_gptw0_base_addr;
}

static inline volatile rx_gptw_channel_regs_t* gptw1(void)
{
  return (volatile rx_gptw_channel_regs_t*)k_gptw1_base_addr;
}

static inline volatile rx_gptw_channel_regs_t* gptw2(void)
{
  return (volatile rx_gptw_channel_regs_t*)k_gptw2_base_addr;
}

static inline volatile rx_gptw_channel_regs_t* gptw3(void)
{
  return (volatile rx_gptw_channel_regs_t*)k_gptw3_base_addr;
}

/* =============================================================================
 * Write Protection Register (GTWP) Bits
 * =============================================================================
 */

typedef enum {
  k_gptw_gtwp_wp0    = (1 << 0), /**< Register write protect bit 0 */
  k_gptw_gtwp_wp1    = (1 << 1), /**< Register write protect bit 1 */
  k_gptw_gtwp_wp2    = (1 << 2), /**< Register write protect bit 2 */
  k_gptw_gtwp_wp3    = (1 << 3), /**< Register write protect bit 3 */
  k_gptw_gtwp_unlock = 0xA500,   /**< Write protection unlock key */
  k_gptw_gtwp_lock   = 0xA501,   /**< Write protection lock key */
} gptw_gtwp_bits_t;

/* =============================================================================
 * Control Register (GTCR) Bits
 * =============================================================================
 */

typedef enum {
  /* Counter Start bit */
  k_gptw_gtcr_cst = (1 << 0), /**< Counter Start (0=stop, 1=count) */

  /* Timer Prescaler Select (bits 24-26) */
  k_gptw_gtcr_tpcs_shift = 24,
  k_gptw_gtcr_tpcs_mask  = (0x07 << 24),
  k_gptw_gtcr_tpcs_1     = (0x00 << 24), /**< PCLKA/1 */
  k_gptw_gtcr_tpcs_4     = (0x01 << 24), /**< PCLKA/4 */
  k_gptw_gtcr_tpcs_16    = (0x02 << 24), /**< PCLKA/16 */
  k_gptw_gtcr_tpcs_64    = (0x03 << 24), /**< PCLKA/64 */
  k_gptw_gtcr_tpcs_256   = (0x04 << 24), /**< PCLKA/256 */
  k_gptw_gtcr_tpcs_1024  = (0x05 << 24), /**< PCLKA/1024 */

  /* Mode Select (bits 16-18) */
  k_gptw_gtcr_md_shift     = 16,
  k_gptw_gtcr_md_mask      = (0x07 << 16),
  k_gptw_gtcr_md_saw_1shot = (0x00 << 16), /**< Sawtooth wave one-shot */
  k_gptw_gtcr_md_saw_cont  = (0x01 << 16), /**< Sawtooth wave continuous */
  k_gptw_gtcr_md_tri_1shot = (0x04 << 16), /**< Triangle wave one-shot */
  k_gptw_gtcr_md_tri_cont  = (0x05 << 16), /**< Triangle wave continuous */
} gptw_gtcr_bits_t;

/* =============================================================================
 * I/O Control Register (GTIOR) Bits
 * =============================================================================
 */

typedef enum {
  /* GTIOCA Output Control (bits 0-4) */
  k_gptw_gtior_oa_shift      = 0,
  k_gptw_gtior_oa_mask       = 0x1F,
  k_gptw_gtior_oa_disabled   = 0x00, /**< Output disabled */
  k_gptw_gtior_oa_low_cmpup  = 0x01, /**< Low on compare match (up) */
  k_gptw_gtior_oa_high_cmpup = 0x02, /**< High on compare match (up) */
  k_gptw_gtior_oa_toggle     = 0x03, /**< Toggle on compare match */
  k_gptw_gtior_oa_init_low   = 0x09, /**< Initial low, PWM output */
  k_gptw_gtior_oa_init_high  = 0x0A, /**< Initial high, PWM output */

  /* GTIOCA Output Enable (bit 6) */
  k_gptw_gtior_oae = (1 << 6), /**< GTIOCA Output Enable */

  /* GTIOCB Output Control (bits 16-20) */
  k_gptw_gtior_ob_shift      = 16,
  k_gptw_gtior_ob_mask       = (0x1F << 16),
  k_gptw_gtior_ob_disabled   = (0x00 << 16), /**< Output disabled */
  k_gptw_gtior_ob_low_cmpup  = (0x01 << 16), /**< Low on compare match (up) */
  k_gptw_gtior_ob_high_cmpup = (0x02 << 16), /**< High on compare match (up) */
  k_gptw_gtior_ob_toggle     = (0x03 << 16), /**< Toggle on compare match */
  k_gptw_gtior_ob_init_low   = (0x09 << 16), /**< Initial low, PWM output */
  k_gptw_gtior_ob_init_high  = (0x0A << 16), /**< Initial high, PWM output */

  /* GTIOCB Output Enable (bit 22) */
  k_gptw_gtior_obe = (1 << 22), /**< GTIOCB Output Enable */
} gptw_gtior_bits_t;

/* =============================================================================
 * Buffer Enable Register (GTBER) Bits
 * =============================================================================
 */

typedef enum {
  k_gptw_gtber_ccra_buf = (1 << 0),  /**< GTCCRA buffer enable */
  k_gptw_gtber_ccrb_buf = (1 << 1),  /**< GTCCRB buffer enable */
  k_gptw_gtber_pr_buf   = (1 << 16), /**< GTPR buffer enable */
} gptw_gtber_bits_t;

/* =============================================================================
 * Dead Time Control Register (GTDTCR) Bits
 * =============================================================================
 */

typedef enum {
  k_gptw_gtdtcr_tde  = (1 << 0), /**< Dead time enable */
  k_gptw_gtdtcr_tdfe = (1 << 4), /**< Dead time buffer enable */
} gptw_gtdtcr_bits_t;

/* =============================================================================
 * Status Register (GTST) Bits
 * =============================================================================
 */

typedef enum {
  k_gptw_gtst_tcfa  = (1 << 0),  /**< Compare match flag A */
  k_gptw_gtst_tcfb  = (1 << 1),  /**< Compare match flag B */
  k_gptw_gtst_tcfc  = (1 << 2),  /**< Compare match flag C */
  k_gptw_gtst_tcfd  = (1 << 3),  /**< Compare match flag D */
  k_gptw_gtst_tcfe  = (1 << 4),  /**< Compare match flag E */
  k_gptw_gtst_tcff  = (1 << 5),  /**< Compare match flag F */
  k_gptw_gtst_tcfpo = (1 << 6),  /**< Overflow flag (at period) */
  k_gptw_gtst_tcfpu = (1 << 7),  /**< Underflow flag (at 0) */
  k_gptw_gtst_tucf  = (1 << 15), /**< Count direction flag (1=up) */
} gptw_gtst_bits_t;

/* =============================================================================
 * Start/Stop Register Bits (GTSTRA)
 * =============================================================================
 */

typedef enum {
  k_gptw_gtstr_cst0 = (1 << 0), /**< Channel 0 count start */
  k_gptw_gtstr_cst1 = (1 << 1), /**< Channel 1 count start */
  k_gptw_gtstr_cst2 = (1 << 2), /**< Channel 2 count start */
  k_gptw_gtstr_cst3 = (1 << 3), /**< Channel 3 count start */
} gptw_gtstr_bits_t;

/* =============================================================================
 * Module Stop Control (MSTPCRC)
 * =============================================================================
 */

typedef enum {
  /**
   * @brief GPTW module stop bit in MSTPCRC
   * @warning Verify this bit position against RX72N Hardware Manual.
   * Different RX series may use different bit positions.
   */
  k_mstpc_gptw = 6, /**< GPTW module stop bit in MSTPCRC */
} gptw_module_stop_bits_t;

/* =============================================================================
 * MPC PFS Select Values for GPTW Pins
 * =============================================================================
 */

typedef enum {
  /**
   * @brief Peripheral select value for GPTW outputs on Port E
   * @warning Verify this value against RX72N Hardware Manual.
   * PSEL values are chip-specific.
   */
  k_pfs_psel_gptw = 0x14, /**< PSEL value for GPTW alternate function */
} gptw_mpc_psel_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_GPTW_REGS_H */
