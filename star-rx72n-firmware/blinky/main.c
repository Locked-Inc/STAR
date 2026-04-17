/**
 * @file main.c
 * @brief RX72N multi-LED breathing cycle test: PA7, PB0, P71, P72, PB1, PB2.
 *
 * @details
 * Bare-metal test with no RTOS, no BSP, and no external clock configuration.
 * Six LEDs breathe sequentially using software PWM, then all flash together
 * to signal the end of one full cycle before repeating.
 *
 * Breathing sequence:
 *   PA7 -> PB0 -> P71 -> P72 -> PB1 -> PB2 -> [flash all x3] -> repeat
 *
 * Runs on the default internal LOCO clock at 240 kHz (OFS1 = 0xFFFFFFFF).
 * Software PWM timing is calibrated for this clock.
 *
 * Port register addresses from RX72N Hardware Manual (r01uh0824ej0111):
 *   PDR  = 0x0008C000 + port_index  (direction: 0=input, 1=output)
 *   PODR = 0x0008C020 + port_index  (output data)
 *   Port indices: 7=0x07, A=0x0A, B=0x0B
 *
 * After reset, PMR registers default to 0x00 (all GPIO) so no MPC
 * unlock is required.
 *
 * Built with -O0 so volatile delay loops are not optimised out.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>

/* --------------------------------------------------------------------------
 * Port register addresses.
 *
 * uintptr_t is mandatory for hardware addresses: on the RX72N target it maps
 * to uint32_t, but on an x86_64 unit-test host it is uint64_t.  Using
 * uint32_t directly would silently truncate on the 64-bit host.
 * -------------------------------------------------------------------------- */
typedef enum : uintptr_t {
    k_port7_pdr_addr  = 0x0008C007U, /**< PORT7 direction register */
    k_port7_podr_addr = 0x0008C027U, /**< PORT7 output data register */
    k_porta_pdr_addr  = 0x0008C00AU, /**< PORTA direction register */
    k_porta_podr_addr = 0x0008C02AU, /**< PORTA output data register */
    k_portb_pdr_addr  = 0x0008C00BU, /**< PORTB direction register */
    k_portb_podr_addr = 0x0008C02BU, /**< PORTB output data register */
} port_addr_t;

/* --------------------------------------------------------------------------
 * Pin bit masks (one enum per port to avoid duplicate-value confusion).
 * -------------------------------------------------------------------------- */
typedef enum : uint8_t {
    k_pin_pa7 = (uint8_t)(1U << 7U), /**< PORTA pin 7 */
} porta_pins_t;

typedef enum : uint8_t {
    k_pin_p71 = (uint8_t)(1U << 1U), /**< PORT7 pin 1 */
    k_pin_p72 = (uint8_t)(1U << 2U), /**< PORT7 pin 2 */
} port7_pins_t;

typedef enum : uint8_t {
    k_pin_pb0 = (uint8_t)(1U << 0U), /**< PORTB pin 0 */
    k_pin_pb1 = (uint8_t)(1U << 1U), /**< PORTB pin 1 */
    k_pin_pb2 = (uint8_t)(1U << 2U), /**< PORTB pin 2 */
} portb_pins_t;

/* --------------------------------------------------------------------------
 * Breathing and flash timing.
 *
 * Calibration (LOCO = 240 kHz, -O0):
 *   One delay() iteration = ~12 LOCO cycles = ~50 us.
 *   PWM period = k_max_bright iterations = 50 * 50 us = 2.5 ms (400 Hz).
 *   Each brightness step is held for k_reps_per_step PWM cycles:
 *     5 * 2.5 ms = 12.5 ms per step.
 *   Full breathe (in + out) = 2 * 51 steps * 12.5 ms ~= 1.3 s per LED.
 * -------------------------------------------------------------------------- */
typedef enum : uint32_t {
    k_max_bright     = 50U,   /**< PWM duty-cycle upper bound (steps 0..50) */
    k_reps_per_step  = 5U,    /**< PWM cycles held at each brightness step */
    k_flash_count    = 3U,    /**< Completion flashes after one full cycle */
    k_flash_on_cnt   = 8000U, /**< Flash ON busy-wait count */
    k_flash_off_cnt  = 8000U, /**< Flash OFF busy-wait count */
} breathe_params_t;

/* --------------------------------------------------------------------------
 * LED count
 * -------------------------------------------------------------------------- */
typedef enum : uint8_t {
    k_num_leds = 6U, /**< LEDs in the breathing sequence */
} led_count_t;

/* --------------------------------------------------------------------------
 * LED descriptor
 * -------------------------------------------------------------------------- */
/**
 * @brief Binds a GPIO output register to a single pin's bit mask.
 */
typedef struct {
    volatile uint8_t *podr; /**< Port output data register for this LED */
    uint8_t           mask; /**< Bit mask selecting this LED's pin */
} led_t;

/* --------------------------------------------------------------------------
 * Inline port register accessors
 * -------------------------------------------------------------------------- */
static inline volatile uint8_t *port7_pdr(void)
{
    return (volatile uint8_t *)k_port7_pdr_addr;
}
static inline volatile uint8_t *port7_podr(void)
{
    return (volatile uint8_t *)k_port7_podr_addr;
}
static inline volatile uint8_t *porta_pdr(void)
{
    return (volatile uint8_t *)k_porta_pdr_addr;
}
static inline volatile uint8_t *porta_podr(void)
{
    return (volatile uint8_t *)k_porta_podr_addr;
}
static inline volatile uint8_t *portb_pdr(void)
{
    return (volatile uint8_t *)k_portb_pdr_addr;
}
static inline volatile uint8_t *portb_podr(void)
{
    return (volatile uint8_t *)k_portb_podr_addr;
}

/* --------------------------------------------------------------------------
 * Busy-wait delay
 * -------------------------------------------------------------------------- */
/**
 * @brief Spin for approximately @p count loop iterations.
 * @param[in] count Iteration count. Each iteration is ~50 us at LOCO 240 kHz.
 */
static void delay(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0U; i < count; i++) {
        __asm__ __volatile__("nop");
    }
}

/* --------------------------------------------------------------------------
 * LED helpers
 * -------------------------------------------------------------------------- */
static inline void led_on(const led_t *led)
{
    *led->podr |= led->mask;
}

static inline void led_off(const led_t *led)
{
    *led->podr &= (uint8_t)~led->mask;
}

/* --------------------------------------------------------------------------
 * Breathing effect
 * -------------------------------------------------------------------------- */
/**
 * @brief Run one PWM ramp pass on @p led.
 *
 * @details
 * Steps @p on_time from 0 to @c k_max_bright (or vice versa depending on
 * @p invert), holding each level for @c k_reps_per_step PWM cycles.
 *
 * @param[in] led    LED to animate.
 * @param[in] invert When 0, ramp 0->max (fade in). When 1, ramp max->0 (fade out).
 */
static void breathe_ramp(const led_t *led, uint32_t invert)
{
    uint32_t b;
    uint32_t rep;
    uint32_t on_time;
    uint32_t off_time;

    for (b = 0U; b <= k_max_bright; b++) {
        on_time  = (invert == 0U) ? b : (k_max_bright - b);
        off_time = k_max_bright - on_time;
        for (rep = 0U; rep < k_reps_per_step; rep++) {
            if (on_time > 0U) {
                led_on(led);
                delay(on_time);
            }
            led_off(led);
            if (off_time > 0U) {
                delay(off_time);
            }
        }
    }
}

/**
 * @brief Run one full breathe cycle (fade in then fade out) on @p led.
 * @param[in] led LED to animate.
 */
static void breathe(const led_t *led)
{
    breathe_ramp(led, 0U); /* in  */
    breathe_ramp(led, 1U); /* out */
    led_off(led);
}

/* --------------------------------------------------------------------------
 * Completion flash
 * -------------------------------------------------------------------------- */
/**
 * @brief Flash all LEDs @c k_flash_count times to signal cycle completion.
 * @param[in] leds  LED descriptor array.
 * @param[in] count Number of entries in @p leds.
 */
static void flash_all(const led_t *leds, uint8_t count)
{
    uint32_t f;
    uint8_t  i;

    for (f = 0U; f < k_flash_count; f++) {
        for (i = 0U; i < count; i++) {
            led_on(&leds[i]);
        }
        delay(k_flash_on_cnt);

        for (i = 0U; i < count; i++) {
            led_off(&leds[i]);
        }
        delay(k_flash_off_cnt);
    }
}

/* --------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------- */
/**
 * @brief Configure GPIO outputs and run the breathing cycle forever.
 * @return Never returns.
 */
int main(void)
{
    /* Set LED pins as outputs (PDR bit = 1). */
    *porta_pdr() |= (uint8_t)k_pin_pa7;
    *port7_pdr() |= (uint8_t)(k_pin_p71 | k_pin_p72);
    *portb_pdr() |= (uint8_t)(k_pin_pb0 | k_pin_pb1 | k_pin_pb2);

    /* LED table -- order defines the breathing sequence. */
    const led_t leds[k_num_leds] = {
        { porta_podr(), (uint8_t)k_pin_pa7 },
        { portb_podr(), (uint8_t)k_pin_pb0 },
        { port7_podr(), (uint8_t)k_pin_p71 },
        { port7_podr(), (uint8_t)k_pin_p72 },
        { portb_podr(), (uint8_t)k_pin_pb1 },
        { portb_podr(), (uint8_t)k_pin_pb2 },
    };

    for (;;) {
        uint8_t i;
        for (i = 0U; i < k_num_leds; i++) {
            breathe(&leds[i]);
        }
        flash_all(leds, k_num_leds);
    }
}
