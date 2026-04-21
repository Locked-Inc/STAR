/**
 * @file main.c
 * @brief RX72N "do nothing" -- park the chip in software standby (low power).
 *
 * @details
 * Boots on the default LOCO clock (240 kHz), sets SBYCR.SSBY = 1 so the
 * next WAIT instruction enters software standby (CPU + most peripherals
 * halted), and issues WAIT. From that point only NMI, an enabled IRQn
 * pin, or one of the standby-aware peripherals (RTC alarm, IWDT
 * underflow, LVD) can wake the chip. No such wake sources are armed
 * here, so the board sleeps until you reset or power-cycle it.
 *
 * Use case: `make none` -- run after a motor / encoder / IMU bench
 * test so the board stops driving anything (no PWM, no I2C clocking,
 * no UART output) and sits in a defined low-power state.
 *
 * No clock_init: we deliberately stay on the LOCO 240 kHz post-reset
 * clock. Lower CPU clock = lower power even before standby.
 *
 * No GPIO config: PMR/PDR defaults are all-input, all-GPIO. Pins
 * float to whatever the external pulls dictate. The DRV8263H motor
 * driver chips are externally pulled to safe-disabled, so leaving
 * pins floating does not enable motors.
 *
 * Power consumption (per RX72N datasheet table 51.x, software standby):
 *   Typical IDD = 0.5 uA at 25 C.
 *
 * To wake: press the on-board reset button, or re-flash with E2 Lite.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

/* SBYCR -- Standby Control Register, 16-bit at 0x0008000C.
 *   SSBY (bit 15): 0 = sleep mode on next WAIT, 1 = software standby. */
typedef enum : uintptr_t {
  k_sbycr_addr = 0x0008000CU,
} sbycr_addr_t;

typedef enum : uint16_t {
  k_sbycr_ssby = 0x8000U,  /* SBYCR.SSBY = 1 selects software standby */
} sbycr_bits_t;

/* PRCR -- protects SBYCR writes per RX72N HW manual section 13.2.1.
 *   key = 0xA5 in upper 8 bits, PRC1 in bit 1 unlocks "operating mode /
 *   low power / software reset" registers. Value 0xA50B = key + PRC1
 *   + PRC3 (matches what the rest of the HAL uses post cross-check). */
typedef enum : uintptr_t {
  k_prcr_addr = 0x000803FEU,
} prcr_addr_t;

typedef enum : uint16_t {
  k_prcr_unlock = 0xA50BU,
  k_prcr_lock   = 0xA500U,
} prcr_keys_t;

int main(void)
{
  volatile uint16_t* prcr  = (volatile uint16_t*)k_prcr_addr;
  volatile uint16_t* sbycr = (volatile uint16_t*)k_sbycr_addr;

  /* Set SSBY = 1 so the next WAIT enters software standby, not just
   * sleep. */
  *prcr  = k_prcr_unlock;
  *sbycr = (uint16_t)(*sbycr | k_sbycr_ssby);
  *prcr  = k_prcr_lock;

  /* Enter standby. WAIT halts the CPU; software standby additionally
   * stops most peripheral clocks. Wake requires NMI / armed IRQn /
   * standby-aware peripheral. We arm none, so the board sleeps until
   * external reset. */
  for (;;) {
    __asm__ volatile("wait");
  }
}
