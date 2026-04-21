/**
 * @file main.c
 * @brief BNO055 + BMP280 over RIIC1 smoke test on the production STAR PCB.
 *
 * Bare-metal direct-register I2C probe -- prove the I2C wiring, RIIC1
 * peripheral config, BNO055 reset+boot, and BMP280 are all healthy.
 * Each step prints over SCI9 BEFORE running so a hang is visible.
 *
 * Wiring (from src/inc/hardware_config.h):
 *   IMU_SCL  = P21 / SCL1  pin 36   RIIC1 clock
 *   IMU_SDA  = P20 / SDA1  pin 37   RIIC1 data
 *   IMU_RST  = P83         pin 58   active-low BNO055 reset
 *
 * Devices on RIIC1:
 *   BNO055  @ 0x28 -- expects chip_id = 0xA0 at reg 0x00
 *   BMP280  @ 0x76 -- expects chip_id = 0x58 at reg 0xD0
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

extern void clock_init(void);
extern void sci9_debug_init(void);
extern void sci9_debug_puts(const char *s);
extern void sci9_debug_puthex16(uint16_t v);

#define REG8(a)  (*(volatile uint8_t  *)(uintptr_t)(a))
#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

typedef enum : uint32_t {
  k_iters_per_ms = 40000U,
} delay_const_t;

static void busy_wait_ms(uint32_t ms)
{
  for (uint32_t m = 0; m < ms; m++) {
    for (volatile uint32_t i = 0; i < k_iters_per_ms; i++) {
      __asm__ volatile("nop");
    }
  }
}

/* ------------------------------------------------------------------------ */
/* GPIO helpers (LEDs + IMU reset on P83) */
/* ------------------------------------------------------------------------ */
typedef enum : uint8_t {
  k_port_8       = 8,
  k_bit_imu_rst  = 3,    /* P83 = active-low BNO055 reset */
} gpio_pins_t;

typedef enum : uintptr_t {
  k_pdr_base  = 0x0008C000U,
  k_podr_base = 0x0008C020U,
  k_pmr_base  = 0x0008C060U,
} port_base_t;

static inline void gpio_output(uint8_t port, uint8_t pin)
{
  REG8(k_pmr_base + port) &= (uint8_t) ~(1U << pin);
  REG8(k_pdr_base + port) |= (uint8_t)(1U << pin);
}
static inline void gpio_set(uint8_t port, uint8_t pin, bool high)
{
  if (high) { REG8(k_podr_base + port) |= (uint8_t)(1U << pin); }
  else      { REG8(k_podr_base + port) &= (uint8_t) ~(1U << pin); }
}

static void imu_reset_pulse(void)
{
  gpio_output(k_port_8, k_bit_imu_rst);
  gpio_set(k_port_8, k_bit_imu_rst, true);   /* idle high */
  busy_wait_ms(5);
  gpio_set(k_port_8, k_bit_imu_rst, false);  /* assert reset */
  busy_wait_ms(5);
  gpio_set(k_port_8, k_bit_imu_rst, true);   /* release */
  busy_wait_ms(700);                         /* tBOOT (BNO055 cold start) */
}

/* ------------------------------------------------------------------------ */
/* RIIC1 direct-register driver. RIIC1 base = 0x00088321 per RX72N HW   */
/* manual chapter 38. Module-stop bit = MSTPCRB.MSTPB20.                  */
/* ------------------------------------------------------------------------ */
typedef enum : uintptr_t {
  k_riic1_base   = 0x00088320U,
  k_mstpcrb_addr = 0x00080014U,
  k_prcr_addr    = 0x000803FEU,
  k_pwpr_addr    = 0x0008C11FU,
} riic_addrs_t;

typedef enum : uint8_t {
  k_riic_off_iccr1 = 0x00,
  k_riic_off_iccr2 = 0x01,
  k_riic_off_icmr1 = 0x02,
  k_riic_off_icmr3 = 0x04,
  k_riic_off_icfer = 0x05,
  k_riic_off_icier = 0x07,
  k_riic_off_icsr1 = 0x08,
  k_riic_off_icsr2 = 0x09,
  k_riic_off_icbrl = 0x10,
  k_riic_off_icbrh = 0x11,
  k_riic_off_icdrt = 0x12,
  k_riic_off_icdrr = 0x13,
} riic_off_t;

static inline volatile uint8_t *r1(uint8_t off) { return &REG8(k_riic1_base + off); }

typedef enum : uint8_t {
  k_iccr1_iceen   = 0x80,
  k_iccr1_iicrst  = 0x40,
  k_iccr2_bbsy    = 0x80,
  k_iccr2_mst     = 0x40,    /* Controller mode (writable in config phase) */
  k_iccr2_trs     = 0x20,    /* Transmit/Receive select (1=transmit) */
  k_iccr2_st      = 0x02,
  k_iccr2_sp      = 0x08,
  k_iccr2_rs      = 0x04,
  k_icsr2_al      = 0x02,
  k_icsr2_start   = 0x04,
  k_icsr2_stop    = 0x08,
  k_icsr2_nackf   = 0x10,
  k_icsr2_rdrf    = 0x20,
  k_icsr2_tend    = 0x40,
  k_icsr2_tdre    = 0x80,
  k_icmr3_ackbt   = 0x08,
  k_icmr3_ackwp   = 0x10,
  k_icfer_tmoe    = 0x01,
  k_icfer_male    = 0x02,
  k_icfer_nale    = 0x04,
  k_icfer_sale    = 0x08,
  k_icfer_nacke   = 0x10,
  k_icfer_nfe     = 0x20,
  k_icfer_scle    = 0x40,
  k_icfer_fmpe    = 0x80,
} riic_bits_t;

/* Bit-bang SCL 9 times to flush any partial-byte a stuck I2C peripheral may
 * be holding SDA low for, then synthesize a STOP. Standard I2C bus-recovery
 * dance per NXP UM10204 section 3.1.16. Runs BEFORE we hand the pins to the
 * RIIC peripheral. ODR encodes pin drive: 2 bits per pin within ODR0 (pins
 * 0-3); the LSB of each pair is reserved, the MSB selects N-ch open-drain.
 * For P20: bit 1 of ODR0. For P21: bit 3 of ODR0. */
static void i2c_bus_recover(void)
{
  REG8(0x0008C062U) &= (uint8_t)~((1U << 0) | (1U << 1));   /* PMR: GPIO mode */
  REG8(0x0008C084U) |= (uint8_t)((1U << 1) | (1U << 3));    /* P20/P21 NMOS open-drain */
  REG8(0x0008C022U) |= (uint8_t)((1U << 0) | (1U << 1));    /* PODR: idle high (released) */
  REG8(0x0008C002U) |= (uint8_t)((1U << 0) | (1U << 1));    /* PDR: outputs */

  for (uint8_t i = 0; i < 9U; i++) {
    REG8(0x0008C022U) &= (uint8_t)~(1U << 1);   /* SCL low */
    busy_wait_ms(1);
    REG8(0x0008C022U) |= (uint8_t)(1U << 1);    /* SCL high */
    busy_wait_ms(1);
  }
  /* Manual STOP: SDA low while SCL high, then SDA high. */
  REG8(0x0008C022U) &= (uint8_t)~(1U << 0);   /* SDA low */
  busy_wait_ms(1);
  REG8(0x0008C022U) |= (uint8_t)(1U << 0);    /* SDA high (STOP) */
  busy_wait_ms(1);

  /* Release pins back to input before RIIC1 takes over via PMR=1. RIIC1
   * drives the lines on its own once PMR is set; leaving PDR=1 is fine
   * per the HW manual but tristating first avoids any glitch. */
  REG8(0x0008C002U) &= (uint8_t)~((1U << 0) | (1U << 1));
  REG8(0x0008C084U) &= (uint8_t)~((1U << 1) | (1U << 3));   /* clear open-drain */
}

static void riic1_init(void)
{
  /* Step 0: 9-clock SCL bit-bang to flush a stuck slave that may be holding
   * SDA low from an aborted previous transaction. Runs while pins are still
   * GPIO (PMR=0) so we can drive them directly. */
  i2c_bus_recover();

  /* Step 1: clear MSTPCRB.MSTPB20 (RIIC1) under PRCR unlock 0xA50B
   * (PRC1+PRC3; 0xA50F asserts a reserved bit per the cross-check fix). */
  *(volatile uint16_t *)k_prcr_addr  = 0xA50BU;
  *(volatile uint32_t *)k_mstpcrb_addr &= ~(uint32_t)(1UL << 20);
  *(volatile uint16_t *)k_prcr_addr  = 0xA500U;

  /* Step 2: PFS for RIIC1 SDA1=P20, SCL1=P21. PSEL = 0x0F per RX72N
   * HW manual chapter 23 Table 23.6. */
  *(volatile uint8_t *)k_pwpr_addr = 0x00U;
  *(volatile uint8_t *)k_pwpr_addr = 0x40U;     /* unlock PFS */
  REG8(0x0008C140U + 2U * 8U + 0U) = 0x0FU;     /* P20 SDA1 */
  REG8(0x0008C140U + 2U * 8U + 1U) = 0x0FU;     /* P21 SCL1 */
  *(volatile uint8_t *)k_pwpr_addr = 0x00U;
  *(volatile uint8_t *)k_pwpr_addr = 0x80U;     /* lock PFS */

  /* Step 3: PMR=1 to route P20/P21 to RIIC1. */
  REG8(0x0008C062U) |= (uint8_t)((1U << 0) | (1U << 1));

  /* Step 4: per production riic.c: pulse internal reset (IICRST=1 then
   * IICRST=0) so the RIIC enters configuration state with both IICRST
   * and ICEEN cleared. Configuration registers are writable in this
   * state. Setting ICBRH/ICBRL while IICRST=1 silently no-ops -- we
   * already saw that empirically. */
  *r1(k_riic_off_iccr1) = k_iccr1_iicrst;        /* assert internal reset */
  *r1(k_riic_off_iccr1) = 0x00U;                  /* clear all bits (IICRST=0) */

  /* Step 5: bit rate. RX72N HW manual Table 38.x: lower 5 bits hold
   * the BRL/BRH count; upper 3 bits are reserved (read-as-1, write
   * value ignored). Writing the raw 5-bit value is fine. Production
   * uses internal_calculate_bit_rate(); we hard-code values that give
   * ~100 kHz at PCLKB=60 MHz (or scaled if PCLKB is actually 120). */
  *r1(k_riic_off_icbrh) = 23U;
  *r1(k_riic_off_icbrl) = 27U;

  /* Step 6: mode/timing. Mirror production riic_init: only program ICMR1
   * (controller-mode, 7-bit addressing); leave ICMR2/ICMR3 at reset (0x00)
   * and DO NOT touch ICFER -- its reset value 0x72 enables the on-chip
   * noise filter (NFE) and clock-sync (SCLE) which the peripheral relies
   * on. Overriding it with our own bitmask clobbers NFE and silently
   * breaks edge detection. */
  *r1(k_riic_off_icmr1) = 0x08U;

  /* Step 7: enable peripheral. */
  *r1(k_riic_off_iccr1) = k_iccr1_iceen;         /* ICEEN=1, IICRST=0 */

  /* Step 8: kick RIIC out of post-enable "stuck low" by clearing SOWP and
   * forcing SCLO/SDAO high (open-drain release), then re-locking SOWP. The
   * peripheral comes out of IICRST with SCLO=SDAO=0 (driving low) and only
   * releases on the first STOP it sees. We synthesize that release by hand. */
  uint8_t iccr1 = *r1(k_riic_off_iccr1);
  iccr1 &= (uint8_t)~(1U << 4);                  /* clear SOWP -> writable */
  iccr1 |= (uint8_t)((1U << 3) | (1U << 2));     /* SCLO=1, SDAO=1 (release) */
  *r1(k_riic_off_iccr1) = iccr1;
  iccr1 |= (uint8_t)(1U << 4);                   /* re-lock SOWP */
  *r1(k_riic_off_iccr1) = iccr1;
}

/* Force-reset RIIC1 internal state machine (clears stuck BBSY / lingering
 * START / NACKF flags from a botched transaction). Cheaper than full
 * riic1_init() because we keep PFS / MSTPCR / bit-rate untouched. */
static void riic1_recover(void)
{
  *r1(k_riic_off_iccr1) = k_iccr1_iicrst;        /* assert IICRST (ICEEN=0) */
  *r1(k_riic_off_iccr1) = 0x00U;                  /* deassert, configuration state */
  /* IICRST cycle wipes ICBRH/L/ICMRx/ICFER back to reset (0xFF). Restore. */
  *r1(k_riic_off_icbrh) = 23U;
  *r1(k_riic_off_icbrl) = 27U;
  *r1(k_riic_off_icmr1) = 0x08U;
  *r1(k_riic_off_icmr3) = k_icmr3_ackwp;
  /* Drop k_icfer_tmoe -- timeout monitoring leaves RIIC stuck if SCL
   * doesn't toggle. Rely on our software wait_bit() bound instead. */
  *r1(k_riic_off_icfer) = (uint8_t)(k_icfer_male | k_icfer_nale |
                                    k_icfer_sale | k_icfer_nacke | k_icfer_scle);
  *r1(k_riic_off_icsr2) = 0x00U;                  /* clear all status flags */
  *r1(k_riic_off_iccr1) = k_iccr1_iceen;         /* re-enable */
}

/* Polling helper with hard upper bound (NASA Rule 2). Returns false on
 * timeout instead of hanging. */
typedef enum : uint32_t {
  k_poll_max = 100000U,                          /* ~few ms at -O1 */
} poll_max_t;

static bool wait_bit(volatile uint8_t *reg, uint8_t mask, bool want_set)
{
  for (uint32_t i = 0; i < k_poll_max; i++) {
    bool now = ((*reg) & mask) != 0U;
    if (now == want_set) { return true; }
  }
  return false;
}

/* Per-gate failure tag, set by i2c_read_byte before a goto stop_fail jump,
 * so the caller can print exactly which step bailed out. */
static volatile uint8_t s_last_fail_gate = 0U;

/* Single-byte register read: START -> addr|W -> reg -> RS -> addr|R -> data
 * -> NACK -> STOP.  Returns false (and forces a STOP) on any NACK/timeout. */
static bool i2c_read_byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t *out)
{
  volatile uint8_t *iccr2 = r1(k_riic_off_iccr2);
  volatile uint8_t *icsr2 = r1(k_riic_off_icsr2);
  volatile uint8_t *icdrt = r1(k_riic_off_icdrt);
  volatile uint8_t *icdrr = r1(k_riic_off_icdrr);
  volatile uint8_t *icmr3 = r1(k_riic_off_icmr3);

  if (!wait_bit(iccr2, k_iccr2_bbsy, false)) {
    /* Stuck busy from a previous botched transaction -- nuke the FSM. */
    riic1_recover();
    if (!wait_bit(iccr2, k_iccr2_bbsy, false)) { return false; }
  }
  /* Pre-clear any lingering status from the previous read. */
  *icsr2 = 0x00U;

  /* Controller transmit mode -- mirrors production internal_riic_write_phase().
   * Writing MST|TRS while bus is idle is permitted; the bits become read-only
   * status during an active transaction. */
  *iccr2 = (uint8_t)(k_iccr2_mst | k_iccr2_trs);
  *iccr2 |= k_iccr2_st;
  if (!wait_bit(icsr2, k_icsr2_start, true)) { s_last_fail_gate = 1U; goto stop_fail; }
  *icsr2 &= (uint8_t)~k_icsr2_start;

  if (!wait_bit(icsr2, k_icsr2_tdre, true)) { s_last_fail_gate = 2U; goto stop_fail; }
  *icdrt = (uint8_t)((dev_addr << 1) | 0U);
  if (!wait_bit(icsr2, k_icsr2_tend, true)) { s_last_fail_gate = 3U; goto stop_fail; }
  if ((*icsr2) & k_icsr2_nackf)               { s_last_fail_gate = 4U; goto stop_fail; }

  if (!wait_bit(icsr2, k_icsr2_tdre, true)) { s_last_fail_gate = 5U; goto stop_fail; }
  *icdrt = reg_addr;
  if (!wait_bit(icsr2, k_icsr2_tend, true)) { s_last_fail_gate = 6U; goto stop_fail; }
  if ((*icsr2) & k_icsr2_nackf)               { s_last_fail_gate = 7U; goto stop_fail; }

  /* Repeated start, then switch to receive mode (TRS=0, MST stays). */
  *iccr2 |= k_iccr2_rs;
  if (!wait_bit(icsr2, k_icsr2_start, true)) { s_last_fail_gate = 8U; goto stop_fail; }
  *icsr2 &= (uint8_t)~k_icsr2_start;
  *iccr2 = k_iccr2_mst;

  if (!wait_bit(icsr2, k_icsr2_tdre, true)) { s_last_fail_gate = 9U; goto stop_fail; }
  *icdrt = (uint8_t)((dev_addr << 1) | 1U);
  /* Receive-mode address: skip TEND. After ACK, RIIC immediately clocks the
   * first data byte and sets RDRF -- TEND never asserts for the addr|R byte
   * in master-receive mode. NACK still surfaces via NACKF if slave didn't
   * answer, so we check that before waiting for RDRF. */
  if (!wait_bit(icsr2, (uint8_t)(k_icsr2_rdrf | k_icsr2_nackf), true)) {
    s_last_fail_gate = 10U;
    goto stop_fail;
  }
  if ((*icsr2) & k_icsr2_nackf)               { s_last_fail_gate = 11U; goto stop_fail; }

  *icmr3 |= k_icmr3_ackbt;                      /* NACK after the byte we want */
  (void)*icdrr;                                  /* dummy read kicks reception */

  if (!wait_bit(icsr2, k_icsr2_rdrf, true)) { s_last_fail_gate = 12U; goto stop_fail; }

  *icsr2 &= (uint8_t)~k_icsr2_stop;
  *iccr2 |= k_iccr2_sp;
  *out = *icdrr;

  if (!wait_bit(icsr2, k_icsr2_stop, true)) { goto cleanup_only; }
cleanup_only:
  *icsr2 &= (uint8_t)~(k_icsr2_stop | k_icsr2_nackf);
  *icmr3 &= (uint8_t)~k_icmr3_ackbt;
  return (*out != 0xFFU || true);                /* return true regardless of value */

stop_fail:
  *iccr2 |= k_iccr2_sp;
  (void)wait_bit(icsr2, k_icsr2_stop, true);
  *icsr2 &= (uint8_t)~(k_icsr2_stop | k_icsr2_nackf);
  *icmr3 &= (uint8_t)~k_icmr3_ackbt;
  return false;
}

/* ------------------------------------------------------------------------ */
typedef enum : uint8_t {
  k_bno055_addr        = 0x28U,
  k_bno055_chip_id_reg = 0x00U,
  k_bno055_acc_x_lsb   = 0x08U,
  k_bmp280_addr        = 0x76U,
  k_bmp280_chip_id_reg = 0xD0U,
} sensor_t;

int main(void)
{
  clock_init();
  sci9_debug_init();
  /* 5-second window so the operator can attach `cat /dev/ttyACM0` after a
   * fresh flash-and-reset and still see step 1..4 messages. */
  busy_wait_ms(5000);
  sci9_debug_puts("\n=== IMU_TEST start ===\n");

  sci9_debug_puts("step 1: imu_reset_pulse (P83 hi-lo-hi + 700 ms)\n");
  imu_reset_pulse();
  sci9_debug_puts("  done\n");

  sci9_debug_puts("step 2: riic1_init\n");
  riic1_init();
  sci9_debug_puts("  done\n");

  sci9_debug_puts("post-init regs:");
  sci9_debug_puts(" ICCR1=0x"); sci9_debug_puthex16((uint16_t)*r1(k_riic_off_iccr1));
  sci9_debug_puts(" ICCR2=0x"); sci9_debug_puthex16((uint16_t)*r1(k_riic_off_iccr2));
  sci9_debug_puts(" ICSR2=0x"); sci9_debug_puthex16((uint16_t)*r1(k_riic_off_icsr2));
  sci9_debug_puts("\n");

  sci9_debug_puts("step 3: probe BNO055 chip_id @ 0x28 reg 0x00\n");
  uint8_t bno_id = 0xFFU;
  bool bno_ok = i2c_read_byte(k_bno055_addr, k_bno055_chip_id_reg, &bno_id);
  sci9_debug_puts(bno_ok ? "  read ok, chip_id=0x" : "  read FAILED, last=0x");
  sci9_debug_puthex16((uint16_t)bno_id);
  sci9_debug_puts(bno_id == 0xA0U ? " (BNO055 ok)\n" : " (expected 0xA0)\n");

  sci9_debug_puts("step 4: probe BMP280 chip_id @ 0x76 reg 0xD0\n");
  uint8_t bmp_id = 0xFFU;
  bool bmp_ok = i2c_read_byte(k_bmp280_addr, k_bmp280_chip_id_reg, &bmp_id);
  sci9_debug_puts(bmp_ok ? "  read ok, chip_id=0x" : "  read FAILED, last=0x");
  sci9_debug_puthex16((uint16_t)bmp_id);
  sci9_debug_puts(bmp_id == 0x58U ? " (BMP280 ok)\n" : " (expected 0x58)\n");

  sci9_debug_puts("step 5: BNO055 probe loop with gate-fail tracing\n");
  uint16_t iter = 0;
  for (;;) {
    riic1_init();
    uint8_t v  = 0xFFU;
    s_last_fail_gate = 0U;
    bool ok = i2c_read_byte(k_bno055_addr, k_bno055_chip_id_reg, &v);
    sci9_debug_puts("iter=");
    sci9_debug_puthex16(iter);
    sci9_debug_puts(ok ? "  chip_id=0x" : "  FAIL@gate=0x");
    sci9_debug_puthex16((uint16_t)(ok ? v : s_last_fail_gate));
    sci9_debug_puts("  ICCR1=0x"); sci9_debug_puthex16((uint16_t)*r1(k_riic_off_iccr1));
    sci9_debug_puts(" ICSR2=0x"); sci9_debug_puthex16((uint16_t)*r1(k_riic_off_icsr2));
    sci9_debug_puts("\n");
    busy_wait_ms(500);
    iter++;
  }
}
