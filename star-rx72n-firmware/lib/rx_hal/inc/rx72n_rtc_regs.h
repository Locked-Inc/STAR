/* lib/rx_hal/inc/rx72n_rtc_regs.h */

/**
 * @file rx72n_rtc_regs.h
 * @brief RX72N Real-Time Clock Register Definitions
 * @details
 * Minimal RTC register definitions for sub-clock disable functionality.
 * The RTC module is not used in this project (sub-clock oscillator disabled),
 * but RCR3.RTCEN must be set to 0 to fully disable the sub-clock.
 *
 * @date 2026-01-08
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_RTC_REGS_H
#define STAR_RX72N_RTC_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * RTC Register Base Address
 * =============================================================================
 */

/**
 * @brief RTC base address constants
 */
typedef enum {
  k_rtc_base_addr = 0x0008C400, /**< RTC register base address */
} rtc_addresses_t;

/* =============================================================================
 * RTC Register Structure
 * =============================================================================
 */

/**
 * @brief RTC register map (partial - only registers needed for disable)
 */
typedef struct {
  volatile uint8_t r64cnt;    /**< Offset 0x00: 64-Hz Counter */
  uint8_t          rsecnt;    /**< Offset 0x01: Second Counter (BCD) */
  uint8_t          rbcnt;     /**< Offset 0x02: Reserved */
  uint8_t          rmincnt;   /**< Offset 0x03: Minute Counter (BCD) */
  uint8_t          rhrcnt;    /**< Offset 0x04: Hour Counter (BCD) */
  uint8_t          rwkcnt;    /**< Offset 0x05: Day-of-Week Counter */
  uint8_t          rdaycnt;   /**< Offset 0x06: Day Counter (BCD) */
  uint8_t          rmoncnt;   /**< Offset 0x07: Month Counter (BCD) */
  uint16_t         ryrcnt;    /**< Offset 0x08: Year Counter (BCD) */
  uint8_t          rsecar;    /**< Offset 0x0A: Second Alarm */
  uint8_t          rbcar;     /**< Offset 0x0B: Reserved */
  uint8_t          rminar;    /**< Offset 0x0C: Minute Alarm */
  uint8_t          rhrar;     /**< Offset 0x0D: Hour Alarm */
  uint8_t          rwkar;     /**< Offset 0x0E: Day-of-Week Alarm */
  uint8_t          rdayar;    /**< Offset 0x0F: Day Alarm */
  uint8_t          rmonar;    /**< Offset 0x10: Month Alarm */
  uint8_t          reserved1; /**< Offset 0x11: Reserved */
  uint16_t         ryrar;     /**< Offset 0x12: Year Alarm */
  uint8_t          ryraren;   /**< Offset 0x14: Year Alarm Enable */
  uint8_t          reserved2; /**< Offset 0x15: Reserved */
  volatile uint8_t rcr1;      /**< Offset 0x16: RTC Control Register 1 */
  uint8_t          reserved3; /**< Offset 0x17: Reserved */
  volatile uint8_t rcr2;      /**< Offset 0x18: RTC Control Register 2 */
  uint8_t          reserved4; /**< Offset 0x19: Reserved */
  volatile uint8_t rcr3;      /**< Offset 0x1A: RTC Control Register 3 */
  volatile uint8_t rcr4;      /**< Offset 0x1B: RTC Control Register 4 */
} rx_rtc_regs_t;

/**
 * @brief RCR3 register bits
 */
typedef enum {
  k_rcr3_rtcen_disable = 0x00, /**< RTC disabled (bit 0 = 0) */
  k_rcr3_rtcen_enable  = 0x01, /**< RTC enabled (bit 0 = 1) */
} rcr3_bits_t;

/**
 * @brief Get pointer to RTC registers
 * @return Volatile pointer to RTC register structure
 */
static inline volatile rx_rtc_regs_t* rtc_regs(void)
{
  return (volatile rx_rtc_regs_t*)k_rtc_base_addr;
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_RTC_REGS_H */
