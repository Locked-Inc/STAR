/**
 * @file main.c
 * @brief GPIO breakout verification firmware for RX72N, ThreadX edition.
 *
 * @details
 * Mirrors the structure that works in blinky_rtos: one ThreadX thread that
 * drives the pins with a busy-wait delay (NOT tx_thread_sleep, which stalls
 * when the scheduler/tick isn't ticking for any reason). A single 100 Hz
 * CMT0 tick keeps the kernel happy even though we do not use it for
 * sleep-based timing.
 *
 * Test pattern per pin:
 *   1. Drive HIGH for ~5 ms (busy-wait loop)
 *   2. Drive LOW  for ~5 ms
 *   3. Advance to next pin
 *
 * After all pins are tested a long quiet gap (~2.3 s) separates cycles so
 * the host script can anchor capture timing to the gap.
 *
 * Clock path:
 *   clock_init() brings us from HOCO 16 MHz -> PLL 192 MHz -> ICLK 96 MHz
 *   / PCKB 48 MHz. CMT0 is then programmed at PCKB/32 for 100 Hz tick.
 *
 * PJ3 and PJ5 are excluded: on RX72N they double as JTAG TMS/TDO and driving
 * them while the E2 Lite has the debug interface latched hangs the MCU.
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include "tx_api.h"

/* ==========================================================================
 * GPIO register base addresses (RX72N Hardware Manual Ch 22)
 * ========================================================================== */
/**
 * @enum port_reg_base_t
 * @brief Base addresses of the per-port GPIO register banks.
 *
 * @details
 * Each GPIO port has three 8-bit registers indexed by `port_offset_t`:
 *   - PDR  (direction):  0 = input,  1 = output
 *   - PODR (output data): 0 = LOW,   1 = HIGH
 *   - PMR  (mode):       0 = GPIO,  1 = peripheral
 * Address = base + port_offset.
 *
 * @see RX72N Hardware Manual, Chapter 22 (I/O Ports)
 */
typedef enum : uintptr_t {
    k_pdr_base  = 0x0008C000U, /**< Port direction register bank base */
    k_podr_base = 0x0008C020U, /**< Port output data register bank base */
    k_pmr_base  = 0x0008C060U, /**< Port mode register bank base */
} port_reg_base_t;

/* ==========================================================================
 * Port offsets (PDR[k_port_off_x] = PDR_base + offset)
 * ========================================================================== */
/**
 * @enum port_offset_t
 * @brief Per-port index added to every GPIO register bank base.
 *
 * @details
 * Ports 0..9 map to offsets 0x00..0x09, ports A..F to 0x0A..0x0F. Port J
 * lives at offset 0x12 because offsets 0x10 (Port G) and 0x11 (Port H) are
 * reserved on this package even though the address space is allocated.
 */
typedef enum : uint8_t {
    k_port_off_0 = 0x00U, /**< Port 0 offset within each register bank */
    k_port_off_1 = 0x01U, /**< Port 1 offset within each register bank */
    k_port_off_2 = 0x02U, /**< Port 2 offset within each register bank */
    k_port_off_3 = 0x03U, /**< Port 3 offset within each register bank */
    k_port_off_4 = 0x04U, /**< Port 4 offset within each register bank */
    k_port_off_5 = 0x05U, /**< Port 5 offset within each register bank */
    k_port_off_6 = 0x06U, /**< Port 6 offset within each register bank */
    k_port_off_7 = 0x07U, /**< Port 7 offset within each register bank */
    k_port_off_8 = 0x08U, /**< Port 8 offset within each register bank */
    k_port_off_9 = 0x09U, /**< Port 9 offset within each register bank */
    k_port_off_a = 0x0AU, /**< Port A offset within each register bank */
    k_port_off_b = 0x0BU, /**< Port B offset within each register bank */
    k_port_off_c = 0x0CU, /**< Port C offset within each register bank */
    k_port_off_d = 0x0DU, /**< Port D offset within each register bank */
    k_port_off_e = 0x0EU, /**< Port E offset within each register bank */
    k_port_off_f = 0x0FU, /**< Port F offset within each register bank */
} port_offset_t;

/* ==========================================================================
 * Port bit positions (0..7 within each 8-bit PDR/PODR/PMR register)
 * ========================================================================== */
/**
 * @enum port_bit_t
 * @brief Bit position within an 8-bit port register.
 *
 * @details
 * RX72N ports are 8 bits wide; pin N of a given port is bit N of the port's
 * PDR/PODR/PMR. Using a typed enum in `s_pins[]` keeps the table readable
 * and satisfies the STAR coding rule against raw numeric literals.
 */
typedef enum : uint8_t {
    k_bit_0 = 0U, /**< Bit 0 (pin 0) of the port */
    k_bit_1 = 1U, /**< Bit 1 (pin 1) of the port */
    k_bit_2 = 2U, /**< Bit 2 (pin 2) of the port */
    k_bit_3 = 3U, /**< Bit 3 (pin 3) of the port */
    k_bit_4 = 4U, /**< Bit 4 (pin 4) of the port */
    k_bit_5 = 5U, /**< Bit 5 (pin 5) of the port */
    k_bit_6 = 6U, /**< Bit 6 (pin 6) of the port */
    k_bit_7 = 7U, /**< Bit 7 (pin 7) of the port */
} port_bit_t;

/* ==========================================================================
 * Pin descriptor
 * ========================================================================== */
/**
 * @struct pin_desc_t
 * @brief One entry in the firmware's pin sweep table.
 *
 * @details
 * The sweep iterates `s_pins[]` in order; its index is the firmware pin
 * index matched by the host capture script.
 */
typedef struct {
    port_offset_t port_off; /**< Port offset, used with k_pdr_base/k_podr_base/k_pmr_base */
    port_bit_t    bit;      /**< Bit position (0..7) within the 8-bit port register */
} pin_desc_t;

/* ==========================================================================
 * Complete pin table. Array index = firmware sweep index. BGA pin numbers
 * from `pin_map.md` are deliberately NOT encoded here -- the firmware only
 * cares about port/bit. PJ3 and PJ5 are intentionally omitted: on RX72N
 * they double as JTAG TMS/TDO; driving them while the E2 Lite holds the
 * debug interface hangs the MCU (empirically observed, every pin after
 * the first PJ write went silent).
 * ========================================================================== */
/** @brief Full sweep table, one entry per tested GPIO pin. */
static const pin_desc_t s_pins[] = {
    { k_port_off_0, k_bit_0 }, { k_port_off_0, k_bit_1 }, { k_port_off_0, k_bit_2 },
    { k_port_off_0, k_bit_3 }, { k_port_off_0, k_bit_5 }, { k_port_off_0, k_bit_7 },
    { k_port_off_1, k_bit_2 }, { k_port_off_1, k_bit_3 }, { k_port_off_1, k_bit_4 },
    { k_port_off_1, k_bit_5 }, { k_port_off_1, k_bit_7 },
    { k_port_off_2, k_bit_0 }, { k_port_off_2, k_bit_1 }, { k_port_off_2, k_bit_2 },
    { k_port_off_2, k_bit_3 }, { k_port_off_2, k_bit_4 }, { k_port_off_2, k_bit_5 },
    { k_port_off_3, k_bit_2 }, { k_port_off_3, k_bit_3 },
    { k_port_off_4, k_bit_0 }, { k_port_off_4, k_bit_1 }, { k_port_off_4, k_bit_2 },
    { k_port_off_4, k_bit_3 }, { k_port_off_4, k_bit_4 }, { k_port_off_4, k_bit_5 },
    { k_port_off_4, k_bit_6 }, { k_port_off_4, k_bit_7 },
    { k_port_off_5, k_bit_0 }, { k_port_off_5, k_bit_1 }, { k_port_off_5, k_bit_2 },
    { k_port_off_5, k_bit_3 }, { k_port_off_5, k_bit_4 }, { k_port_off_5, k_bit_5 },
    { k_port_off_5, k_bit_6 },
    { k_port_off_6, k_bit_0 }, { k_port_off_6, k_bit_1 }, { k_port_off_6, k_bit_2 },
    { k_port_off_6, k_bit_3 }, { k_port_off_6, k_bit_4 }, { k_port_off_6, k_bit_5 },
    { k_port_off_6, k_bit_6 }, { k_port_off_6, k_bit_7 },
    { k_port_off_7, k_bit_0 }, { k_port_off_7, k_bit_1 }, { k_port_off_7, k_bit_2 },
    { k_port_off_7, k_bit_3 }, { k_port_off_7, k_bit_4 }, { k_port_off_7, k_bit_5 },
    { k_port_off_7, k_bit_6 }, { k_port_off_7, k_bit_7 },
    { k_port_off_8, k_bit_0 }, { k_port_off_8, k_bit_1 }, { k_port_off_8, k_bit_2 },
    { k_port_off_8, k_bit_3 }, { k_port_off_8, k_bit_6 }, { k_port_off_8, k_bit_7 },
    { k_port_off_9, k_bit_0 }, { k_port_off_9, k_bit_1 }, { k_port_off_9, k_bit_2 },
    { k_port_off_9, k_bit_3 },
    { k_port_off_a, k_bit_0 }, { k_port_off_a, k_bit_1 }, { k_port_off_a, k_bit_2 },
    { k_port_off_a, k_bit_3 }, { k_port_off_a, k_bit_4 }, { k_port_off_a, k_bit_5 },
    { k_port_off_a, k_bit_6 }, { k_port_off_a, k_bit_7 },
    { k_port_off_b, k_bit_0 }, { k_port_off_b, k_bit_1 }, { k_port_off_b, k_bit_2 },
    { k_port_off_b, k_bit_3 }, { k_port_off_b, k_bit_4 }, { k_port_off_b, k_bit_5 },
    { k_port_off_c, k_bit_0 }, { k_port_off_c, k_bit_1 }, { k_port_off_c, k_bit_2 },
    { k_port_off_c, k_bit_3 }, { k_port_off_c, k_bit_4 }, { k_port_off_c, k_bit_5 },
    { k_port_off_c, k_bit_6 },
    { k_port_off_d, k_bit_0 }, { k_port_off_d, k_bit_1 }, { k_port_off_d, k_bit_2 },
    { k_port_off_d, k_bit_3 }, { k_port_off_d, k_bit_4 }, { k_port_off_d, k_bit_5 },
    { k_port_off_d, k_bit_6 }, { k_port_off_d, k_bit_7 },
    { k_port_off_e, k_bit_0 }, { k_port_off_e, k_bit_1 }, { k_port_off_e, k_bit_2 },
    { k_port_off_e, k_bit_3 }, { k_port_off_e, k_bit_4 }, { k_port_off_e, k_bit_5 },
    { k_port_off_e, k_bit_6 }, { k_port_off_e, k_bit_7 },
    { k_port_off_f, k_bit_5 },
};

/* ==========================================================================
 * Timing constants. Busy-wait delay at ICLK = 96 MHz. The actual pin
 * period drifts a little with optimisation level; gpio_verify.py measures
 * it empirically from the capture.
 * ========================================================================== */
/**
 * @enum timing_t
 * @brief Sweep loop iteration and gap counts.
 */
typedef enum : uint32_t {
    k_num_pins   = (uint32_t)(sizeof(s_pins) / sizeof(s_pins[0])), /**< Pin count in s_pins */
    k_delay_iter = 50000U, /**< nop iterations per HIGH/LOW state (~5 ms at ICLK 96 MHz) */
    k_gap_iters  = 500U,   /**< delay() calls in the inter-cycle quiet gap */
} timing_t;

/* ==========================================================================
 * ThreadX objects
 * ========================================================================== */
/**
 * @enum rtos_prio_t
 * @brief ThreadX priority numbers used by this firmware.
 */
typedef enum : uint8_t {
    k_sweep_priority = 10U, /**< Priority of the gpio_sweep thread */
} rtos_prio_t;

/**
 * @enum rtos_stack_t
 * @brief ThreadX stack sizes in bytes.
 */
typedef enum : uint16_t {
    k_sweep_stack_sz = 512U, /**< gpio_sweep thread stack size */
} rtos_stack_t;

/** @var s_sweep_tcb @brief ThreadX control block for the gpio_sweep task. */
static TX_THREAD s_sweep_tcb;

/** @var s_sweep_stack @brief Static stack backing s_sweep_tcb. */
static uint8_t   s_sweep_stack[k_sweep_stack_sz];

/* ==========================================================================
 * Inline accessors
 * ========================================================================== */

/**
 * @brief Pointer to the PDR (direction) register for a given port offset.
 * @param[in] port_off Port offset constant from `port_offset_t`.
 * @return Volatile pointer to the 8-bit PDR register for that port.
 */
static inline volatile uint8_t *internal_pdr(uint8_t port_off)
{
    return (volatile uint8_t *)(k_pdr_base + port_off);
}

/**
 * @brief Pointer to the PODR (output data) register for a given port offset.
 * @param[in] port_off Port offset constant from `port_offset_t`.
 * @return Volatile pointer to the 8-bit PODR register for that port.
 */
static inline volatile uint8_t *internal_podr(uint8_t port_off)
{
    return (volatile uint8_t *)(k_podr_base + port_off);
}

/**
 * @brief Pointer to the PMR (mode) register for a given port offset.
 * @param[in] port_off Port offset constant from `port_offset_t`.
 * @return Volatile pointer to the 8-bit PMR register for that port.
 */
static inline volatile uint8_t *internal_pmr(uint8_t port_off)
{
    return (volatile uint8_t *)(k_pmr_base + port_off);
}

/**
 * @brief Busy-wait for one pin state (~5 ms at ICLK 96 MHz).
 *
 * @details
 * Used in place of `tx_thread_sleep`: the 100 Hz ThreadX tick is too coarse
 * for a ~5 ms pulse, and the earlier `tx_thread_sleep`-based version stalled
 * the sweep thread indefinitely (same pattern blinky_rtos avoids for its
 * sub-tick timing).
 *
 * @pre ICLK is running at 96 MHz (clock_init has completed).
 * @pre The compiler is honouring `volatile` on the loop counter.
 * @post `k_delay_iter` nop instructions have retired and control returns.
 */
static void internal_delay(void)
{
    for (volatile uint32_t i = 0U; i < k_delay_iter; i++) {
        /* Inline asm: emit a real NOP so the RX back-end cannot fold the
         * loop body away at higher optimisation levels. `volatile i`
         * alone is not enough on GCC-RX 14.2 when the body is empty.
         * Project coding rule requires a justification at each __asm__ site. */
        __asm__ volatile("nop");
    }
}

/**
 * @brief Configure every pin in `s_pins` as a GPIO output driven LOW.
 *
 * @details
 * For each entry: clears the PMR bit (GPIO mode, not peripheral), sets the
 * PDR bit (output direction), and clears the PODR bit (LOW). Default RX72N
 * reset state is PMR=0 across the board, so the PMR clear is defensive.
 *
 * @pre `clock_init()` has run so PCKB is stable when the writes retire.
 * @pre `s_pins` is a `const` table in ROM and is safe to read here.
 * @post Every pin in `s_pins` has PMR=0, PDR=1, PODR=0.
 * @post No other port bits were modified (read-modify-write per bit).
 * @note Called exactly once, from `tx_application_define`.
 */
static void internal_gpio_init_all(void)
{
    for (uint32_t i = 0U; i < k_num_pins; i++) {
        uint8_t off  = (uint8_t)s_pins[i].port_off;
        uint8_t mask = (uint8_t)(1U << (uint8_t)s_pins[i].bit);
        *internal_pmr(off)  &= (uint8_t)~mask;
        *internal_pdr(off)  |= mask;
        *internal_podr(off) &= (uint8_t)~mask;
    }
}

/**
 * @brief Sweep task: pulses each pin in `s_pins` order, forever.
 *
 * @details
 * Each iteration drives one pin HIGH, busy-waits, drives it LOW,
 * busy-waits, moves to the next entry. After every pin has pulsed once
 * the thread busy-waits through `k_gap_iters` additional `delay()` calls
 * so the host capture script can anchor on the quiet gap.
 *
 * @param[in] arg Unused ThreadX thread argument.
 * @pre `internal_gpio_init_all()` has completed.
 * @pre The ThreadX kernel is running (scheduler + tick are up).
 * @post Never returns; the loop drives the full `s_pins` cycle exactly
 *       once per iteration.
 */
static void internal_sweep_task_entry(ULONG arg)
{
    (void)arg;

    for (;;) {
        for (uint32_t i = 0U; i < k_num_pins; i++) {
            uint8_t off  = (uint8_t)s_pins[i].port_off;
            uint8_t mask = (uint8_t)(1U << (uint8_t)s_pins[i].bit);
            *internal_podr(off) |= mask;
            internal_delay();
            *internal_podr(off) &= (uint8_t)~mask;
            internal_delay();
        }
        for (uint32_t g = 0U; g < k_gap_iters; g++) {
            internal_delay();
        }
    }
}

/* ==========================================================================
 * tx_application_define -- called by ThreadX from tx_kernel_enter().
 * ========================================================================== */
/**
 * @brief ThreadX hook: configure GPIO and start the sweep task.
 *
 * @details
 * ThreadX invokes this from inside `tx_kernel_enter()` once the kernel
 * is initialised. If `tx_thread_create` fails the function parks in an
 * infinite loop so the bench observes a flat line instead of a silently
 * missing sweep (which would otherwise look identical to a crashed
 * thread on the logic analyser).
 *
 * @param[in] first_unused_memory Unused; kernel-provided heap anchor.
 * @pre The ThreadX kernel has been initialised by `tx_kernel_enter()`.
 * @post All `s_pins` are GPIO outputs driven LOW.
 * @post Either the gpio_sweep thread is created (success) or control is
 *       parked in an infinite nop loop (failure).
 */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;

    internal_gpio_init_all();

    UINT status = tx_thread_create(&s_sweep_tcb, "gpio_sweep",
                                   internal_sweep_task_entry, 0U,
                                   s_sweep_stack, k_sweep_stack_sz,
                                   k_sweep_priority, k_sweep_priority,
                                   TX_NO_TIME_SLICE, TX_AUTO_START);
    if (status != TX_SUCCESS) {
        /* Park here so the bench observes a flat line instead of a
         * ghost sweep. Any failure (TX_SIZE_ERROR / TX_PTR_ERROR /
         * TX_START_ERROR) surfaces loudly as "no edges ever" which
         * is easy to spot on the logic analyser. */
        for (;;) {
            __asm__ volatile("nop");
        }
    }
}

/* ==========================================================================
 * External init hooks
 * ========================================================================== */
extern void clock_init(void); /**< HOCO 16 MHz -> PLL 192 MHz -> ICLK 96 MHz */
extern void cmt0_init(void);  /**< 100 Hz CMT0 tick source for ThreadX */

/* ==========================================================================
 * main
 * ========================================================================== */
/**
 * @brief Firmware entry point.
 *
 * @details
 * Brings up the PLL (96 MHz), arms the CMT0 100 Hz tick, then hands off
 * to ThreadX. Order matters: `clock_init()` must run first so PCKB is
 * 48 MHz before `cmt0_init()` programs the CMT registers; `cmt0_init()`
 * runs before `tx_kernel_enter()` as in blinky_rtos.
 *
 * @return Nominally 0, but control never reaches the `return`: the
 *         ThreadX scheduler is the final owner of CPU time.
 * @retval 0 Unreachable.
 * @pre Called from `startup.S` with the user/interrupt stack pointers
 *      valid and BSS zeroed.
 * @pre The RX72N reset vector resolves to `_PowerON_Reset_PC`.
 * @post Never returns; ThreadX scheduler owns control.
 */
int main(void)
{
    clock_init();
    cmt0_init();
    tx_kernel_enter();
    return 0;
}
