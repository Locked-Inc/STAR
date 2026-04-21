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

/* ==========================================================================
 * Direct port register access -- PDR / PODR / PMR for one port n live at
 * 0x0008C000+n, 0x0008C020+n, 0x0008C060+n respectively (RX72N HW manual
 * Ch.22 I/O Ports). Same pattern pwm_test_motor used; keeps this test free
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
  busy_wait(k_step_delay_iters);
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
