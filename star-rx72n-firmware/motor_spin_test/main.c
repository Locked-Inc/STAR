/**
 * @file main.c
 * @brief Open-loop 4-motor spin test.
 *
 * @details
 * Bare-metal bench test that drives all 4 DRV8263H motor channels from the
 * RX72N GPTW peripheral with a -100..+100..-100 percent duty sweep. Reuses
 * the production motor-control libraries (rx_motor / rx_gptw / rx_mpc) and
 * the production crystal+PLL clock setup, but skips ThreadX, USB, SPI,
 * IMU, sonar, ADC, and the closed-loop PID task.
 *
 * Pin map (mirrors star-rx72n-firmware/src/inc/hardware_config.h and
 * libs/rx_hal/inc/hardware.h):
 *
 *   Motor 0: GPTW0 IN2=P23 (pin 34), IN1=P17 (pin 38)
 *            DRVOFF=P61 (pin 115), nSLEEP=P60 (pin 117)
 *   Motor 1: GPTW1 IN2=P22 (pin 35), IN1=PC3 (pin 67)
 *            DRVOFF=P63 (pin 113), nSLEEP=P62 (pin 114)
 *   Motor 2: GPTW2 IN2=PE3 (pin 108), IN1=P86 (pin 41)
 *            DRVOFF=PE0 (pin 111), nSLEEP=P64 (pin 112)
 *   Motor 3: GPTW3 IN2=PE7 (pin 101), IN1=PC6 (pin 61)
 *            DRVOFF=PE2 (pin 109), nSLEEP=PE1 (pin 110)
 *
 * Power: motors require bus voltage on the barrel/battery input.
 * USB power alone is NOT enough to spin them.
 *
 * Heartbeat: PA4 toggles each duty step so a third scope probe (or an LED
 * if one is fitted on PA4) confirms firmware is alive.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "rx_motor.h"
#include "rx_gptw.h"
#include "rx_port_constants.h"

extern void clock_init(void);

/* SCI9 polled debug UART (PB7=TXD, PB6=RXD) -> CY7C65213 -> /dev/ttyACM0.
 * Baud 115200 8N1, matches encoder_test/sci9.c verbatim. */
extern void sci9_debug_init(void);
extern void sci9_debug_putc(char c);
extern void sci9_debug_puts(const char *s);
extern void sci9_debug_puthex16(uint16_t v);

/* ==========================================================================
 * Direct port register access -- PDR / PODR / PMR for one port n live at
 * 0x0008C000+n, 0x0008C020+n, 0x0008C060+n respectively (RX72N HW manual
 * Ch.22 I/O Ports). Same direct-register pattern as pwm_test_fit; keeps this test free
 * of a dependency on rx_port_utils / gpio.c.
 * ========================================================================== */
#define REG8(a) (*(volatile uint8_t *)(uintptr_t)(a))

typedef enum : uintptr_t {
  k_port_pdr_base  = 0x0008C000U,
  k_port_podr_base = 0x0008C020U,
  k_port_pmr_base  = 0x0008C060U,
} port_reg_base_t;

static inline volatile uint8_t *port_pdr(uint8_t port)  { return &REG8(k_port_pdr_base  + port); }
static inline volatile uint8_t *port_podr(uint8_t port) { return &REG8(k_port_podr_base + port); }
static inline volatile uint8_t *port_pmr(uint8_t port)  { return &REG8(k_port_pmr_base  + port); }

/*
 * Status LED map:
 *   LED0 = PA7  (pin 88) -- heartbeat (toggles each duty step)
 *   LED1 = PB0  (pin 87) -- lit after clock_init
 *   LED2 = P71  (pin 86) -- lit after motor_drv_gpio_init
 *   LED3 = P72  (pin 85) -- lit after motor_pwm_init succeeds
 *   LED4 = PB1  (pin 84) -- lit if any motor has |duty| > 0 right now
 *   LED5 = PB2  (pin 83) -- lit SOLID on error_park (init failed)
 *
 * If you see LED1+LED2+LED3 on and LED0 toggling -> firmware is healthy
 * and PWM is being driven. If LED5 is on, rx_motor_init failed.
 */
typedef enum : uint8_t {
  k_port_a   = 10, /* PORTA */
  k_port_b   = 11, /* PORTB */
  k_port_7   = 7,  /* PORT7 */
  k_bit_led0 = 7,  /* PA7 */
  k_bit_led1 = 0,  /* PB0 */
  k_bit_led2 = 1,  /* P71 */
  k_bit_led3 = 2,  /* P72 */
  k_bit_led4 = 1,  /* PB1 */
  k_bit_led5 = 2,  /* PB2 */
} status_led_pins_t;

/* RX72N port indices used by the motor map. PORT0-PORT7 share their numeric
 * index; PORT8=8, PORTC=12, PORTE=14. Match rx_port_utils convention. */
typedef enum : uint8_t {
  k_port_1 = 1,
  k_port_2 = 2,
  k_port_6 = 6,
  k_port_8 = 8,
  k_port_c = 12,
  k_port_e = 14,
} rx_port_index_t;

/* ==========================================================================
 * Per-motor wiring table -- mirrors src/inc/hardware_config.h
 * ========================================================================== */
typedef struct {
  rx_gptw_channel_t gptw_channel; /* GPTW channel 0..3 */
  rx_port_pin_t     in2_pin;      /* GTIOC*A pin (DRV8263H IN2 / direction) */
  rx_port_pin_t     in1_pin;      /* GTIOC*B pin (DRV8263H IN1 / PWM) */
  uint8_t           drvoff_port;  /* port index of DRVOFF GPIO */
  uint8_t           drvoff_pin;   /* bit position 0..7 in that port */
  uint8_t           nsleep_port;
  uint8_t           nsleep_pin;
  int8_t            direction_sign; /* +1 = motor wired so positive duty drives
                                       robot forward; -1 = wired backwards
                                       (test multiplies sweep duty by this). */
} motor_wiring_t;

typedef enum : uint8_t {
  k_motor_count = 4,
} motor_count_t;

/*
 * MOTOR_ENABLE_MASK -- compile-time bitmask selecting which motors run.
 *   bit 0 = Motor 0, bit 1 = Motor 1, bit 2 = Motor 2, bit 3 = Motor 3.
 *
 * Default 0xF spins all four motors (matches the original test). Override
 * from the build system (e.g. -DMOTOR_ENABLE_MASK=0x1 for Motor 0 only).
 * Disabled motors are completely skipped: their PWM/GPIO is never touched
 * and their DRVOFF stays HIGH (driver outputs disabled, safe).
 */
#ifndef MOTOR_ENABLE_MASK
#define MOTOR_ENABLE_MASK 0xFU
#endif

static inline bool motor_enabled(uint8_t i)
{
  return (bool)(((MOTOR_ENABLE_MASK) & (1U << i)) != 0U);
}

static const motor_wiring_t k_motors[k_motor_count] = {
  /* M0 = front-left. Wired backwards on this board -- positive duty
   * drives the wheel in reverse, so we flip the sign here. */
  {.gptw_channel = k_gptw_channel_0,
   .in2_pin      = k_rx_p2_3, .in1_pin = k_rx_p1_7,
   .drvoff_port  = k_port_6,  .drvoff_pin = 1,
   .nsleep_port  = k_port_6,  .nsleep_pin = 0,
   .direction_sign = -1},

  {.gptw_channel = k_gptw_channel_1,
   .in2_pin      = k_rx_p2_2, .in1_pin = k_rx_pc_3,
   .drvoff_port  = k_port_6,  .drvoff_pin = 3,
   .nsleep_port  = k_port_6,  .nsleep_pin = 2,
   .direction_sign = +1},

  /* M2 = back-right. */
  {.gptw_channel = k_gptw_channel_2,
   .in2_pin      = k_rx_pe_3, .in1_pin = k_rx_p8_6,
   .drvoff_port  = k_port_e,  .drvoff_pin = 0,
   .nsleep_port  = k_port_6,  .nsleep_pin = 4,
   .direction_sign = +1},

  /* M3 = back-left. Wired backwards (matches M0 front-left -- both
   * left-side wheels are mirrored on this chassis). */
  {.gptw_channel = k_gptw_channel_3,
   .in2_pin      = k_rx_pe_7, .in1_pin = k_rx_pc_6,
   .drvoff_port  = k_port_e,  .drvoff_pin = 2,
   .nsleep_port  = k_port_e,  .nsleep_pin = 1,
   .direction_sign = -1},
};

/* ==========================================================================
 * Sweep timing -- bounded loops per NASA Rule 2
 * ========================================================================== */
typedef enum : uint32_t {
  /* ~40 ms per duty step at ICLK=240 MHz, -O1; tuned coarsely. */
  k_step_delay_iters = 1000000U,
  /* ~10 ms tWAKE margin after nSLEEP rises (DRV8263H spec: 1.2 ms min). */
  k_twake_iters = 250000U,
  /* Split each duty-step dwell around the ADC scan (half before, half after). */
  k_step_delay_halves = 2U,
} delay_iters_t;

/*
 * MOTOR_DUTY_MIN / MOTOR_DUTY_MAX -- compile-time override of the duty
 * sweep range. Defaults to -100..+100 (bidirectional). Override for the
 * forward-only / reverse-only build targets:
 *   Forward only: -DMOTOR_DUTY_MIN=0    -DMOTOR_DUTY_MAX=100
 *   Reverse only: -DMOTOR_DUTY_MIN=-100 -DMOTOR_DUTY_MAX=0
 */
#ifndef MOTOR_DUTY_MIN
#define MOTOR_DUTY_MIN (-100)
#endif
#ifndef MOTOR_DUTY_MAX
#define MOTOR_DUTY_MAX (100)
#endif

typedef enum : int8_t {
  k_duty_min     = MOTOR_DUTY_MIN,
  k_duty_max     = MOTOR_DUTY_MAX,
  k_duty_step_pc = 5,
} duty_sweep_t;

static void busy_wait(uint32_t iters)
{
  for (volatile uint32_t i = 0; i < iters; i++) {
    __asm__ volatile("nop");
  }
}

/* ==========================================================================
 * GPIO control of the DRV8263H DRVOFF / nSLEEP signals
 * ========================================================================== */
static inline void gpio_set_output(uint8_t port, uint8_t pin)
{
  *port_pmr(port) &= (uint8_t) ~(1U << pin); /* peripheral mode off -> GPIO */
  *port_pdr(port) |= (uint8_t)(1U << pin);   /* direction = output */
}

static inline void gpio_write(uint8_t port, uint8_t pin, bool high)
{
  if (high) {
    *port_podr(port) |= (uint8_t)(1U << pin);
  } else {
    *port_podr(port) &= (uint8_t) ~(1U << pin);
  }
}

/* ==========================================================================
 * Init the DRV8263H control GPIOs in the safe order:
 *   1. DRVOFF HIGH first  -> H-bridge outputs disabled
 *   2. nSLEEP HIGH second -> driver wakes up with bridge already disabled
 *   3. busy-wait tWAKE
 *   4. DRVOFF LOW         -> bridge outputs enabled, ready for PWM
 *
 * Ordering follows DRV8263H datasheet sec 7.3.1 and matches
 * internal_gpio_init_motor_driver_ctrl() in src/hardware_init.c.
 * ========================================================================== */
static void motor_drv_gpio_init(void)
{
  /* Step 1+2: configure ctrl pins as outputs, DRVOFF HIGH first.
   * Disabled motors still get DRVOFF asserted HIGH (safe: bridge off). */
  for (uint8_t i = 0; i < k_motor_count; i++) {
    gpio_set_output(k_motors[i].drvoff_port, k_motors[i].drvoff_pin);
    gpio_write(k_motors[i].drvoff_port, k_motors[i].drvoff_pin, true);
  }
  for (uint8_t i = 0; i < k_motor_count; i++) {
    if (!motor_enabled(i)) { continue; } /* leave nSLEEP at POR default */
    gpio_set_output(k_motors[i].nsleep_port, k_motors[i].nsleep_pin);
    gpio_write(k_motors[i].nsleep_port, k_motors[i].nsleep_pin, true);
  }

  /* Step 3: tWAKE busy-wait (DRV8263H needs ~1.2 ms after nSLEEP rising). */
  busy_wait(k_twake_iters);

  /* Step 4: enable bridge outputs only on motors selected by the mask. */
  for (uint8_t i = 0; i < k_motor_count; i++) {
    if (!motor_enabled(i)) { continue; }
    gpio_write(k_motors[i].drvoff_port, k_motors[i].drvoff_pin, false);
  }
}

/* ==========================================================================
 * Bring up GPTW pins via the production MPC helper, then init each motor.
 * Returns false on any error -- caller should park the heartbeat LED on
 * solid as a visual indication of init failure.
 * ========================================================================== */
static bool motor_pwm_init(rx_motor_handle_t handles[k_motor_count])
{
  for (uint8_t i = 0; i < k_motor_count; i++) {
    if (!motor_enabled(i)) { continue; }
    const rx_motor_config_t cfg = {
      .channel      = k_motors[i].gptw_channel,
      .output_a     = k_gptw_output_a,           /* IN2 */
      .output_b     = k_gptw_output_b,           /* IN1 */
      .pwm_freq_hz  = 20000U,
      .dead_time_ns = 1000U,
      .invert_pwm   = false,
      /* Per-motor pad coordinates -- lib uses these to mux GTIOC*A/B
       * to the right pads. Sourced from the test's k_motors[] table
       * (which mirrors src/inc/hardware_config.h). */
      .port_a_idx   = (uint8_t)rx_port_from_pin(k_motors[i].in2_pin),
      .bit_a        = (uint8_t)rx_pin_from_pin(k_motors[i].in2_pin),
      .port_b_idx   = (uint8_t)rx_port_from_pin(k_motors[i].in1_pin),
      .bit_b        = (uint8_t)rx_pin_from_pin(k_motors[i].in1_pin),
    };
    if (rx_motor_init(&handles[i], &cfg) != k_rx_ok) { return false; }
  }

  return true;
}

/* ==========================================================================
 * S12AD0 (12-bit ADC) bare-metal driver -- reads AN004-AN007 on P44-P47
 * (DRV8263H IPROPI for motors 3,2,1,0 respectively).
 *
 *   S12AD0 base = 0x00089000
 *   ADCSR    @ +0x00 (16b)  - start/mode control
 *   ADANSA0  @ +0x04 (16b)  - channel enable mask AN000..AN015
 *   ADCER    @ +0x0E (16b)  - resolution/data format
 *   ADSSTR4..7 @ +0xE4..0xE7 (8b each) - sampling state count
 *   ADDR4..7 @ +0x28..0x2E (16b each) - conversion results
 *
 *   MSTPCRA.MSTPA17 releases S12AD0 from module-stop.
 *   PFS.ASEL=1 turns P44-P47 into analog inputs.
 * ========================================================================== */
#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))

typedef enum : uintptr_t {
  k_s12ad0_adcsr_addr   = 0x00089000U,
  k_s12ad0_adansa0_addr = 0x00089004U,
  k_s12ad0_adcer_addr   = 0x0008900EU,
  k_s12ad0_adsstr4_addr = 0x000890E4U,
  k_s12ad0_addr4_addr   = 0x00089028U, /* AN004 result */
  k_s12ad0_addr5_addr   = 0x0008902AU,
  k_s12ad0_addr6_addr   = 0x0008902CU,
  k_s12ad0_addr7_addr   = 0x0008902EU,
  k_mstpcra_addr        = 0x00080010U,
  k_prcr_addr           = 0x000803FEU,
  k_mpc_pwpr_addr       = 0x0008C11FU,
  k_mpc_p44pfs_addr     = 0x0008C124U,
  k_mpc_p45pfs_addr     = 0x0008C125U,
  k_mpc_p46pfs_addr     = 0x0008C126U,
  k_mpc_p47pfs_addr     = 0x0008C127U,
} adc_addrs_t;

typedef enum : uint16_t {
  k_adcsr_adst        = (uint16_t)(1U << 15),
  k_adansa_ch4567     = (uint16_t)0x00F0U, /* enable AN004..AN007 */
  k_adcer_12bit_right = (uint16_t)0x0000U, /* ADPRC=00, ADRFMT=0 */
  k_prcr_unlock       = (uint16_t)0xA50BU, /* unlock PRC0+PRC1+PRC3 */
  k_prcr_lock         = (uint16_t)0xA500U,
} adc_cfg16_t;

typedef enum : uint32_t {
  k_mstpa17 = (uint32_t)(1UL << 17), /* S12AD0 stop bit */
} adc_mstp_t;

typedef enum : uint8_t {
  k_pfs_asel     = 0x80U,
  k_pwpr_unlock  = 0x00U,
  k_pwpr_pfswe   = 0x40U,
  k_pwpr_b0wi    = 0x80U,
  k_adsstr_def   = 0x14U, /* 20 states sampling, conservative */
  /* ADSSTR channel offsets in bytes from ADSSTR4 base (one per channel). */
  k_adsstr_off_ch4 = 0U,
  k_adsstr_off_ch5 = 1U,
  k_adsstr_off_ch6 = 2U,
  k_adsstr_off_ch7 = 3U,
  /* Decimal print helpers. */
  k_u16_dec_max_digits = 5U,  /* 65535 is 5 digits */
  k_dec_radix          = 10U,
} adc_cfg8_t;

typedef enum : uint32_t {
  /* Bound on polling loop waiting for ADCSR.ADST to self-clear; the actual
   * conversion completes in <5 us even at 60 MHz PCLKB, so 100k iters is
   * pure safety against a hung peripheral (~400 us wall-clock @ 240 MHz). */
  k_adc_adst_poll_max = 100000U,
} adc_poll_t;

static void adc0_init(void)
{
  /* 1. Release S12AD0 from module-stop (MSTPA17 in MSTPCRA). */
  REG16(k_prcr_addr) = k_prcr_unlock;
  *(volatile uint32_t *)k_mstpcra_addr &= ~(uint32_t)k_mstpa17;
  REG16(k_prcr_addr) = k_prcr_lock;

  /* 2. Set PFS.ASEL=1 for P44-P47 (analog input, digital buffer off). */
  *(volatile uint8_t *)k_mpc_pwpr_addr    = k_pwpr_unlock;
  *(volatile uint8_t *)k_mpc_pwpr_addr    = k_pwpr_pfswe;
  *(volatile uint8_t *)k_mpc_p44pfs_addr  = k_pfs_asel;
  *(volatile uint8_t *)k_mpc_p45pfs_addr  = k_pfs_asel;
  *(volatile uint8_t *)k_mpc_p46pfs_addr  = k_pfs_asel;
  *(volatile uint8_t *)k_mpc_p47pfs_addr  = k_pfs_asel;
  *(volatile uint8_t *)k_mpc_pwpr_addr    = k_pwpr_unlock;
  *(volatile uint8_t *)k_mpc_pwpr_addr    = k_pwpr_b0wi;

  /* 3. Configure S12AD0: 12-bit right-justified, single-scan, software trigger. */
  REG16(k_s12ad0_adcer_addr)   = k_adcer_12bit_right;
  REG16(k_s12ad0_adansa0_addr) = k_adansa_ch4567;

  /* 4. Sampling state count = 20 for each of the 4 channels. */
  *(volatile uint8_t *)(k_s12ad0_adsstr4_addr + k_adsstr_off_ch4) = k_adsstr_def;
  *(volatile uint8_t *)(k_s12ad0_adsstr4_addr + k_adsstr_off_ch5) = k_adsstr_def;
  *(volatile uint8_t *)(k_s12ad0_adsstr4_addr + k_adsstr_off_ch6) = k_adsstr_def;
  *(volatile uint8_t *)(k_s12ad0_adsstr4_addr + k_adsstr_off_ch7) = k_adsstr_def;

  /* 5. ADCSR = 0: single-scan, software trigger, no interrupt. */
  REG16(k_s12ad0_adcsr_addr) = 0U;
}

/* Single-shot scan of all 4 channels. Blocks until conversion completes. */
static void adc0_read_all(uint16_t *m0, uint16_t *m1, uint16_t *m2, uint16_t *m3)
{
  REG16(k_s12ad0_adcsr_addr) |= k_adcsr_adst;
  for (volatile uint32_t i = 0U; i < (uint32_t)k_adc_adst_poll_max; i++) {
    if ((REG16(k_s12ad0_adcsr_addr) & k_adcsr_adst) == 0U) { break; }
  }
  /* Per motor_spin_test k_motors[] ordering: M0 uses IN on P1/P2 ports, but
   * ISENSE pins are the DRV8263H IPROPI outputs wired per docs/03_hardware_pinout:
   *   AN007 (ADDR7) = Motor 0, AN006 (ADDR6) = Motor 1,
   *   AN005 (ADDR5) = Motor 2, AN004 (ADDR4) = Motor 3. */
  if (m0 != (uint16_t *)0) { *m0 = REG16(k_s12ad0_addr7_addr); }
  if (m1 != (uint16_t *)0) { *m1 = REG16(k_s12ad0_addr6_addr); }
  if (m2 != (uint16_t *)0) { *m2 = REG16(k_s12ad0_addr5_addr); }
  if (m3 != (uint16_t *)0) { *m3 = REG16(k_s12ad0_addr4_addr); }
}

/* Print a uint16_t as up-to-5-digit decimal (no padding). */
static void print_u16_dec(uint16_t v)
{
  char    buf[k_u16_dec_max_digits];
  uint8_t n = 0;
  if (v == 0U) {
    sci9_debug_putc('0');
    return;
  }
  while (v > 0U && n < (uint8_t)sizeof(buf)) {
    buf[n++] = (char)('0' + (v % k_dec_radix));
    v = (uint16_t)(v / k_dec_radix);
  }
  while (n > 0U) {
    sci9_debug_putc(buf[--n]);
  }
}

/* Print a signed int8 as decimal with leading sign. */
static void print_i8_dec(int8_t v)
{
  if (v < 0) {
    sci9_debug_putc('-');
    print_u16_dec((uint16_t)(-(int16_t)v));
  } else {
    sci9_debug_putc('+');
    print_u16_dec((uint16_t)v);
  }
}

/* ==========================================================================
 * Status LED helpers (production board)
 * ==========================================================================
 *
 * NOTE: All 6 status LEDs are *active-high*: driving the GPIO HIGH
 * sources current through the LED. Silk-screen reference: D10 = LED1 (PB0,
 * pin 87); full 6-LED map at the top of this file.
 */
static inline void led_set(uint8_t port, uint8_t bit, bool on)
{
  /* on==true -> drive output HIGH (LED lights) */
  gpio_write(port, bit, on);
}

static void status_leds_init(void)
{
  gpio_set_output(k_port_a, k_bit_led0); led_set(k_port_a, k_bit_led0, false);
  gpio_set_output(k_port_b, k_bit_led1); led_set(k_port_b, k_bit_led1, false);
  gpio_set_output(k_port_7, k_bit_led2); led_set(k_port_7, k_bit_led2, false);
  gpio_set_output(k_port_7, k_bit_led3); led_set(k_port_7, k_bit_led3, false);
  gpio_set_output(k_port_b, k_bit_led4); led_set(k_port_b, k_bit_led4, false);
  gpio_set_output(k_port_b, k_bit_led5); led_set(k_port_b, k_bit_led5, false);
}

static void heartbeat_toggle(void)
{
  *port_podr(k_port_a) ^= (uint8_t)(1U << k_bit_led0);
}

static void error_park(void)
{
  /* Light LED5 SOLID -- bright, visible, unmistakeable "init failed". */
  led_set(k_port_b, k_bit_led5, true);
  for (;;) { __asm__ volatile("nop"); }
}

/* ==========================================================================
 * Duty sweep: walk -100 -> +100 -> -100 in 5% steps on all 4 motors at once.
 * Bounded outer loop because it lives inside an unbounded `for(;;)` -- this
 * is the same pattern as rx72n_main task loops.
 * ========================================================================== */
static void run_sweep_step(rx_motor_handle_t handles[k_motor_count], int8_t duty_pc)
{
  for (uint8_t i = 0; i < k_motor_count; i++) {
    if (!motor_enabled(i)) { continue; }
    /* Apply per-motor direction sign so 'positive duty' always means
     * 'wheel rolls robot forward', regardless of how each motor is
     * wired. Per-motor signs live in k_motors[].direction_sign. */
    const int duty_signed = (int)duty_pc * (int)k_motors[i].direction_sign;
    (void)rx_motor_set_duty(&handles[i], (float)duty_signed);
  }
  heartbeat_toggle();

  /* Let PWM settle, then sample IPROPI on all 4 motors and report.
   * Delay is split in half before/after the ADC scan so the duty step
   * is visible on the UART roughly mid-dwell, not at its edges. */
  busy_wait(k_step_delay_iters / k_step_delay_halves);

  uint16_t adc_m0 = 0;
  uint16_t adc_m1 = 0;
  uint16_t adc_m2 = 0;
  uint16_t adc_m3 = 0;
  adc0_read_all(&adc_m0, &adc_m1, &adc_m2, &adc_m3);

  /* Format: "D=<duty> M0=<raw> M1=<raw> M2=<raw> M3=<raw>\r\n"
   * Raw is 12-bit ADC count 0..4095. Convert on host:
   *   V_mV  = raw * 3300 / 4095
   *   I_mA  = V_mV * 10000 / 10302   (DRV8263H: 5.1k sense, 202 uA/A mirror) */
  sci9_debug_puts("D=");
  print_i8_dec(duty_pc);
  sci9_debug_puts(" M0=");
  print_u16_dec(adc_m0);
  sci9_debug_puts(" M1=");
  print_u16_dec(adc_m1);
  sci9_debug_puts(" M2=");
  print_u16_dec(adc_m2);
  sci9_debug_puts(" M3=");
  print_u16_dec(adc_m3);
  sci9_debug_puts("\r\n");

  busy_wait(k_step_delay_iters / k_step_delay_halves);
}

int main(void)
{
  /* Step 0: bring up the status LEDs FIRST -- before any clock change
   * so a hung clock_init still leaves a lit LED telling us "main was
   * entered and GPIO works". RX72N GPIO is not gated by module-stop;
   * direct port writes work on the default LOCO 240 kHz clock. */
  status_leds_init();
  led_set(k_port_b, k_bit_led1, true); /* LED1 = main entered */

  /* Step 1: 24 MHz crystal -> PLL 240 MHz -> ICLK=240, PCKA=120 MHz.
   * If this hangs/crashes, only LED1 is lit -- tells us clock_init
   * is the culprit (likely PLL not locking). */
  clock_init();
  led_set(k_port_7, k_bit_led2, true); /* LED2 = clock_init returned */

  /* Step 1a: bring up debug UART so every subsequent init stage reports. */
  sci9_debug_init();
  sci9_debug_puts("\r\n[motor_spin_test] boot, clocks up\r\n");

  /* Step 1b: init S12AD0 for AN004..AN007 (motor IPROPI sense). */
  adc0_init();
  sci9_debug_puts("[motor_spin_test] S12AD0 AN004-AN007 ready\r\n");

  /* Step 2: DRV8263H control GPIOs in the safe order
   * (DRVOFF HIGH -> nSLEEP HIGH -> tWAKE -> DRVOFF LOW). */
  motor_drv_gpio_init();
  led_set(k_port_7, k_bit_led3, true); /* LED3 = bridges enabled */

  /* Step 3: init GPTW PWM pins + motor handles. LED4 lit confirms
   * rx_motor_init succeeded for every enabled channel. */
  static rx_motor_handle_t s_motors[k_motor_count];
  if (!motor_pwm_init(s_motors)) {
    error_park();
  }
  led_set(k_port_b, k_bit_led4, true); /* LED4 = PWM channels live */

  /* Step 5: forever sweep duty. */
  int8_t duty_pc = k_duty_min;
  int8_t dir     = k_duty_step_pc;
  for (;;) {
    run_sweep_step(s_motors, duty_pc);

    const int next = (int)duty_pc + (int)dir;
    if (next >= (int)k_duty_max) {
      duty_pc = k_duty_max;
      dir     = (int8_t)(-k_duty_step_pc);
    } else if (next <= (int)k_duty_min) {
      duty_pc = k_duty_min;
      dir     = (int8_t)k_duty_step_pc;
    } else {
      duty_pc = (int8_t)next;
    }
  }

  /* Unreachable. */
  return 0;
}
