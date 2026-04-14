/**
 * @file main.c
 * @brief RX72N multi-LED breathing cycle using ThreadX RTOS.
 *
 * @details
 * Two ThreadX threads drive the breathing animation:
 *
 *   breathe_task -- cycles through all 6 LEDs sequentially, applying a
 *                   software-PWM breathing effect to each one in turn.
 *                   After completing a full cycle it posts a semaphore.
 *
 *   flash_task   -- waits on the semaphore, flashes all 6 LEDs together
 *                   three times as a cycle-complete indicator, then signals
 *                   breathe_task to start the next cycle.
 *
 * Threading replaces the single infinite loop of the bare-metal version.
 * tx_thread_sleep() drives the flash timing; software PWM busy-waits are
 * still used for sub-tick LED dimming (the 100 Hz tick is too coarse for
 * smooth PWM).
 *
 * Clock: LOCO default at 240 kHz (OFS1 = 0xFFFFFFFF, factory reset state).
 * Tick:  CMT0 at PCLKB/8 = 30 kHz, CMCOR = 299 -> 100 Hz (10 ms/tick).
 *
 * Port register addresses: RX72N Hardware Manual r01uh0824ej0111, Ch 22.
 *   PDR  base = 0x0008C000 + port_index
 *   PODR base = 0x0008C020 + port_index
 *   Port indices: 7=0x07, A=0x0A, B=0x0B
 *
 * ICU and CMT0 addresses: RX72N Hardware Manual, Ch 13/15.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include "tx_api.h"

/* ==========================================================================
 * Hardware register addresses
 * uintptr_t is mandatory: on RX72N target = uint32_t, on x86-64 host =
 * uint64_t.  Using uint32_t directly silently truncates on the host.
 * ========================================================================== */
typedef enum : uintptr_t {
    /* GPIO */
    k_port7_pdr_addr  = 0x0008C007U, /**< PORT7 direction register */
    k_port7_podr_addr = 0x0008C027U, /**< PORT7 output data register */
    k_porta_pdr_addr  = 0x0008C00AU, /**< PORTA direction register */
    k_porta_podr_addr = 0x0008C02AU, /**< PORTA output data register */
    k_portb_pdr_addr  = 0x0008C00BU, /**< PORTB direction register */
    k_portb_podr_addr = 0x0008C02BU, /**< PORTB output data register */

    /* CMT0 (compare match timer 0) */
    k_cmstr0_addr = 0x00088000U, /**< CMT start register (shared CH0/CH1) */
    k_cmt0_cmcr   = 0x00088002U, /**< CMT0 control register */
    k_cmt0_cmcnt  = 0x00088004U, /**< CMT0 counter */
    k_cmt0_cmcor  = 0x00088006U, /**< CMT0 compare match register */

    /* ICU (interrupt controller) */
    k_icu_ir_base  = 0x00087000U, /**< IR[] base (IR[n] = base + n) */
    k_icu_ier_base = 0x00087200U, /**< IER[] base (IER[n/8] bit n%8) */
    k_icu_ipr_base = 0x00087300U, /**< IPR[] base (IPR[n] = base + n) */
} hw_addr_t;

/* ==========================================================================
 * Pin bit masks (one enum per port to avoid duplicate-value collisions)
 * ========================================================================== */
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

/* ==========================================================================
 * CMT0 configuration constants (LOCO = 240 kHz)
 *   PCLKB/8 = 30 000 Hz timer clock.
 *   CMCOR = 30000/100 - 1 = 299  ->  100 Hz tick (10 ms/tick).
 *   CMCR  = 0x40  ->  CMIE=1 (interrupt enable), CKS=00 (PCLKB/8).
 * ========================================================================== */
typedef enum : uint16_t {
    k_cmt0_cmcor_val  = 299U,  /**< Compare match for 100 Hz at LOCO/8 */
    k_cmt0_cmcr_val   = 0x40U, /**< CMIE=1, CKS=00 (PCLKB/8) */
    k_cmstr0_ch0_bit  = 0x01U, /**< CMSTR0 bit 0 = CMT0 start/stop */
} cmt0_cfg_t;

typedef enum : uint8_t {
    k_vect_cmt0       = 28U,  /**< CMT0 CMI0 interrupt vector number */
    k_cmt0_priority   = 3U,   /**< ICU interrupt priority for CMT0 */
    k_icu_ier_cmt0    = 3U,   /**< IER register index for vector 28 (28/8) */
    k_icu_ier_cmt0_bit = 4U,  /**< Bit position within IER[3] (28%8) */
} icu_cmt0_cfg_t;

/* ==========================================================================
 * Breathing / flash timing
 *   Each delay() iteration ~= 50 us at LOCO 240 kHz (same as bare-metal).
 *   PWM period = k_max_bright iters = 50 * 50 us = 2.5 ms -> 400 Hz PWM.
 *   tx_thread_sleep() granularity = 10 ms/tick.
 * ========================================================================== */
typedef enum : uint32_t {
    k_max_bright       = 50U,  /**< PWM duty-cycle upper bound */
    k_reps_per_step    = 5U,   /**< PWM cycles per brightness step */
    k_flash_count      = 3U,   /**< All-LED flashes at cycle end */
    k_flash_on_ticks   = 8U,   /**< Flash ON duration (ticks, 10 ms each) */
    k_flash_off_ticks  = 8U,   /**< Flash OFF duration (ticks) */
} breathe_params_t;

/* ==========================================================================
 * ThreadX objects
 * ========================================================================== */
typedef enum : uint8_t {
    k_num_leds         = 6U,  /**< LEDs in the breathing sequence */
    k_breathe_priority = 10U, /**< breathe_task ThreadX priority */
    k_flash_priority   = 9U,  /**< flash_task ThreadX priority (runs first) */
} rtos_prio_t;

typedef enum : uint16_t {
    k_breathe_stack_sz = 512U, /**< breathe_task stack in bytes */
    k_flash_stack_sz   = 256U, /**< flash_task stack in bytes */
} rtos_stack_t;

static TX_THREAD    s_breathe_tcb;
static TX_THREAD    s_flash_tcb;
static TX_SEMAPHORE s_cycle_done_sem;  /**< Posted by breathe_task, waited by flash_task */
static TX_SEMAPHORE s_flash_done_sem;  /**< Posted by flash_task, waited by breathe_task */

static uint8_t s_breathe_stack[k_breathe_stack_sz];
static uint8_t s_flash_stack[k_flash_stack_sz];

/* ==========================================================================
 * LED descriptor
 * ========================================================================== */
typedef struct {
    volatile uint8_t *podr; /**< Port output data register */
    uint8_t           mask; /**< Bit mask for this LED's pin */
} led_t;

/* Shared LED table (read-only after init in tx_application_define) */
static led_t s_leds[k_num_leds];

/* ==========================================================================
 * Inline port register accessors
 * ========================================================================== */
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

/* ==========================================================================
 * CMT0 register accessors
 * ========================================================================== */
static inline volatile uint16_t *cmstr0(void)
{
    return (volatile uint16_t *)k_cmstr0_addr;
}
static inline volatile uint16_t *cmt0_cmcr(void)
{
    return (volatile uint16_t *)k_cmt0_cmcr;
}
static inline volatile uint16_t *cmt0_cmcnt(void)
{
    return (volatile uint16_t *)k_cmt0_cmcnt;
}
static inline volatile uint16_t *cmt0_cmcor(void)
{
    return (volatile uint16_t *)k_cmt0_cmcor;
}

/* ==========================================================================
 * ICU register accessors
 * ========================================================================== */
static inline volatile uint8_t *icu_ir(uint8_t vect)
{
    return (volatile uint8_t *)(k_icu_ir_base + vect);
}
static inline volatile uint8_t *icu_ier(uint8_t idx)
{
    return (volatile uint8_t *)(k_icu_ier_base + idx);
}
static inline volatile uint8_t *icu_ipr(uint8_t vect)
{
    return (volatile uint8_t *)(k_icu_ipr_base + vect);
}

/* ==========================================================================
 * ThreadX tick source -- CMT0 ISR
 * ========================================================================== */
extern void _tx_timer_interrupt(void);

void __attribute__((interrupt)) cmt0_isr(void)
{
    *icu_ir(k_vect_cmt0) = 0U;   /* Clear interrupt request flag */
    _tx_timer_interrupt();         /* Drive ThreadX system clock */
}

/* ==========================================================================
 * CMT0 initialisation (called before tx_kernel_enter)
 * ========================================================================== */
static void cmt0_init(void)
{
    *cmstr0()    &= (uint16_t)~(uint16_t)k_cmstr0_ch0_bit; /* Stop CMT0 */
    *cmt0_cmcr() = k_cmt0_cmcr_val;                        /* PCLKB/8, IRQ enabled */
    *cmt0_cmcor() = k_cmt0_cmcor_val;                      /* 100 Hz */
    *cmt0_cmcnt() = 0U;                                     /* Reset counter */

    *icu_ir(k_vect_cmt0)  = 0U;                            /* Clear pending IRQ */
    *icu_ipr(k_vect_cmt0) = k_cmt0_priority;               /* Set priority */
    *icu_ier(k_icu_ier_cmt0) |= (uint8_t)(1U << k_icu_ier_cmt0_bit); /* Enable */

    *cmstr0() |= k_cmstr0_ch0_bit;                         /* Start CMT0 */
    __asm__ volatile("setpsw i");                           /* Global interrupt enable */
}

/* ==========================================================================
 * LED helpers
 * ========================================================================== */
static inline void led_on(const led_t *led)
{
    *led->podr |= led->mask;
}

static inline void led_off(const led_t *led)
{
    *led->podr &= (uint8_t)~led->mask;
}

/* ==========================================================================
 * Busy-wait delay (used for sub-tick software PWM)
 * One iteration ~= 50 us at LOCO 240 kHz.
 * ========================================================================== */
static void pwm_delay(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0U; i < count; i++) {
        __asm__ __volatile__("nop");
    }
}

/* ==========================================================================
 * Software PWM ramp -- one direction of the breathe animation.
 * ========================================================================== */
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
                pwm_delay(on_time);
            }
            led_off(led);
            if (off_time > 0U) {
                pwm_delay(off_time);
            }
        }
    }
}

/* ==========================================================================
 * breathe_task -- cycles through all LEDs, posts semaphore when done.
 * ========================================================================== */
static void breathe_task_entry(ULONG arg)
{
    (void)arg;

    for (;;) {
        uint8_t i;
        for (i = 0U; i < k_num_leds; i++) {
            breathe_ramp(&s_leds[i], 0U); /* fade in  */
            breathe_ramp(&s_leds[i], 1U); /* fade out */
            led_off(&s_leds[i]);
        }

        /* Signal flash_task that one full cycle is complete */
        (void)tx_semaphore_put(&s_cycle_done_sem);

        /* Wait for flash_task to finish before starting the next cycle */
        (void)tx_semaphore_get(&s_flash_done_sem, TX_WAIT_FOREVER);
    }
}

/* ==========================================================================
 * flash_task -- waits for cycle complete, flashes all LEDs, signals back.
 * ========================================================================== */
static void flash_task_entry(ULONG arg)
{
    (void)arg;

    for (;;) {
        uint32_t f;
        uint8_t  i;

        /* Wait for breathe_task to complete one full cycle */
        (void)tx_semaphore_get(&s_cycle_done_sem, TX_WAIT_FOREVER);

        /* Flash all LEDs k_flash_count times using ThreadX sleep for timing */
        for (f = 0U; f < k_flash_count; f++) {
            for (i = 0U; i < k_num_leds; i++) {
                led_on(&s_leds[i]);
            }
            tx_thread_sleep(k_flash_on_ticks);

            for (i = 0U; i < k_num_leds; i++) {
                led_off(&s_leds[i]);
            }
            tx_thread_sleep(k_flash_off_ticks);
        }

        /* Tell breathe_task it can start the next cycle */
        (void)tx_semaphore_put(&s_flash_done_sem);
    }
}

/* ==========================================================================
 * tx_application_define -- called by ThreadX after tx_kernel_enter().
 * Creates semaphores, configures GPIO, and starts both threads.
 * ========================================================================== */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;

    /* Semaphores: initial count 0 (both tasks wait until signalled) */
    (void)tx_semaphore_create(&s_cycle_done_sem, "cycle_done", 0U);
    (void)tx_semaphore_create(&s_flash_done_sem, "flash_done", 1U);

    /* Configure LED pins as outputs (PDR bit = 1).
     * After reset PMR = 0x00 (all GPIO), no MPC unlock needed. */
    *porta_pdr() |= (uint8_t)k_pin_pa7;
    *port7_pdr() |= (uint8_t)(k_pin_p71 | k_pin_p72);
    *portb_pdr() |= (uint8_t)(k_pin_pb0 | k_pin_pb1 | k_pin_pb2);

    /* Build LED table -- order defines breathing sequence */
    s_leds[0].podr = porta_podr(); s_leds[0].mask = (uint8_t)k_pin_pa7;
    s_leds[1].podr = portb_podr(); s_leds[1].mask = (uint8_t)k_pin_pb0;
    s_leds[2].podr = port7_podr(); s_leds[2].mask = (uint8_t)k_pin_p71;
    s_leds[3].podr = port7_podr(); s_leds[3].mask = (uint8_t)k_pin_p72;
    s_leds[4].podr = portb_podr(); s_leds[4].mask = (uint8_t)k_pin_pb1;
    s_leds[5].podr = portb_podr(); s_leds[5].mask = (uint8_t)k_pin_pb2;

    /* Create breathe_task */
    (void)tx_thread_create(&s_breathe_tcb, "breathe",
                           breathe_task_entry, 0U,
                           s_breathe_stack, k_breathe_stack_sz,
                           k_breathe_priority, k_breathe_priority,
                           TX_NO_TIME_SLICE, TX_AUTO_START);

    /* Create flash_task */
    (void)tx_thread_create(&s_flash_tcb, "flash",
                           flash_task_entry, 0U,
                           s_flash_stack, k_flash_stack_sz,
                           k_flash_priority, k_flash_priority,
                           TX_NO_TIME_SLICE, TX_AUTO_START);
}

/* ==========================================================================
 * main -- initialise CMT0 tick source, then hand off to ThreadX forever.
 * ========================================================================== */
int main(void)
{
    cmt0_init();
    tx_kernel_enter(); /* Never returns */
    return 0;
}
