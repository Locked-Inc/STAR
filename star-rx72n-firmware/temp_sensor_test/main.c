/**
 * @file main.c
 * @brief Standalone DS18B20 1-Wire temperature sensor test on RX72N P51 (pin 55).
 *
 * @details
 * Bit-bangs the Dallas/Maxim 1-Wire protocol on P51 to drive a DS18B20+
 * temperature sensor wired to Tom's PCB pin 55. Scope-friendly diagnostic
 * output on P17 (pin 38) encodes the raw scratchpad temperature bytes so
 * we can verify the read over a logic analyzer.
 *
 * Wiring:
 *   - P51 (pin 55)   -> DS18B20 DQ   (external 4.7k pullup to 3.3V on PCB)
 *   - 3V3            -> DS18B20 VDD
 *   - GND            -> DS18B20 GND
 *   - P17 (pin 38)   -> scope Ch1 (diagnostic output - temperature bits)
 *   - PA7 (pin 88)   -> scope Ch2 (heartbeat LED, one toggle per loop)
 *
 * Clocks (from inline_clock_init() - copied from usb_test/hoco_pid_fix.c):
 *   - HOCO 16 MHz x 12 = 192 MHz PLL
 *   - ICLK = PCKA = 96 MHz (CPU + onewire-timing cycle-counting reference)
 *   - PCKB = 48 MHz
 *
 * Why inline delays, not CMT timer:
 *   With a known 96 MHz ICLK, a cycle-counter busy-wait is simpler and more
 *   portable than configuring CMT3. Tolerance is tight enough for 1-Wire.
 *
 * Why not the HAL:
 *   rx_bus_onewire.c pulls in rx_bus_manager (ThreadX mutexes), rx_check
 *   (runtime asserts), rx_log (printf-style logging via UART), rx_crc (CRC-8
 *   tables), rx72n_*_regs abstractions, and needs CMT3 initialization. Stubbing
 *   all of that for a diagnostic standalone blows the scope. This file mirrors
 *   the HAL's documented timing values verbatim from rx_bus_onewire.h.
 *
 * Diagnostic encoding on P17 (per loop iteration):
 *   - 2 long pulses  = start of frame marker (~10 ms HIGH each)
 *   - 1 short pulse  = presence-detect OK bit (SKIPPED if no presence)
 *   - 16 bit-pulses  = temp_msb<<8 | temp_lsb, MSB-first
 *                      bit=1: ~3 ms HIGH, bit=0: ~1 ms HIGH
 *                      each bit separated by ~2 ms LOW
 *   - long LOW gap   = ~1 second before next frame
 *
 * SPDX-License-Identifier: MIT
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#include <stdint.h>
#include <stdbool.h>

#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

/* ==========================================================================
 * System / clock registers (for inline_clock_init)
 * ========================================================================== */
#define PRCR      0x000803FEU
#define HOCOCR    0x00080036U
#define OSCOVFSR  0x0008003CU
#define PLLCR     0x00080028U
#define PLLCR2    0x0008002AU
#define MEMWAIT   0x0008101CU
#define SCKCR     0x00080020U
#define PACKCR    0x00080044U
#define SCKCR2    0x00080024U
#define SCKCR3    0x00080026U

/* ==========================================================================
 * Pin registers
 *
 * MPC (multi-function pin controller) and port registers for:
 *   - P51 (pin 55) -> DS18B20 DQ (1-Wire)
 *   - P17 (pin 38) -> Diagnostic scope output
 *   - PA7 (pin 88) -> Heartbeat LED
 * ========================================================================== */
#define PWPR       0x0008C11FU      /* PFS write protect */
#define P51PFS     0x0008C141U      /* P51 pin function select */
#define P17PFS     0x0008C14FU
/* Port direction/output registers */
#define PORT1_PMR  0x0008C061U
#define PORT1_PDR  0x0008C001U
#define PORT1_PODR 0x0008C021U
#define PORT5_PMR  0x0008C065U
#define PORT5_PDR  0x0008C005U
#define PORT5_PODR 0x0008C025U
#define PORT5_PIDR 0x0008C045U
#define PORTA_PMR  0x0008C06AU
#define PORTA_PDR  0x0008C00AU
#define PORTA_PODR 0x0008C02AU

#define P17_BIT  (1U << 7)   /* P17 = bit 7 of port 1 */
#define P51_BIT  (1U << 1)   /* P51 = bit 1 of port 5 */
#define PA7_BIT  (1U << 7)

/* ==========================================================================
 * DS18B20 / 1-Wire protocol constants (mirror rx_bus_onewire.h timings)
 * ========================================================================== */
enum {
    k_onewire_reset_pulse_us   = 480,
    k_onewire_presence_wait_us = 70,
    k_onewire_presence_tail_us = 410,

    k_onewire_write_1_low_us   = 10,
    k_onewire_write_1_high_us  = 55,
    k_onewire_write_0_low_us   = 60,
    k_onewire_write_0_high_us  = 10,

    k_onewire_read_init_us     = 6,
    k_onewire_read_sample_us   = 9,
    k_onewire_read_recovery_us = 55,
};

enum {
    k_ds18b20_cmd_convert_t    = 0x44,
    k_ds18b20_cmd_read_scratch = 0xBE,
    k_onewire_cmd_skip_rom     = 0xCC,
};

/* ==========================================================================
 * Clock init (HOCO x12 = 192 MHz, PCKA = 96 MHz, PCKB = 48 MHz).
 * Copied verbatim from usb_test/hoco_pid_fix.c -- known-good on Tom's PCB.
 * ========================================================================== */
static void inline_clock_init(void)
{
    REG16(PRCR)     = 0xA503U;  /* PRC0+PRC1 unlock */

    REG8(HOCOCR)    = 0x00U;    /* HOCOCR: start HOCO */
    while ((REG8(OSCOVFSR) & 0x08U) == 0U) { __asm__ volatile("nop"); } /* HCOVF */

    REG16(PLLCR)    = 0x1710U;  /* HOCO*12, PLIDIV=/1 */
    REG8(PLLCR2)    = 0x00U;    /* PLLCR2: enable */
    while ((REG8(OSCOVFSR) & 0x04U) == 0U) { __asm__ volatile("nop"); } /* PLOVF */

    REG8(MEMWAIT)   = 0x01U;
    REG32(SCKCR)    = 0x21C21211U;  /* ICLK/2 PCKA/2 PCKB/4 */
    REG16(PACKCR)   = 0x0001U;
    REG16(SCKCR2)   = 0x0031U;      /* UCK /4 */
    REG16(SCKCR3)   = 0x0400U;      /* CKSEL = PLL */

    REG16(PRCR)     = 0xA500U;      /* PRCR lock */
}

/* ==========================================================================
 * Cycle-accurate delay at 96 MHz ICLK.
 *
 * GCC RX's inner NOP loop: ~1 cycle/iteration once the hardware pipelines, but
 * conservatively we budget ~3 cycles/iter (branch + decrement + nop). At 96
 * MHz that's ~31 ns per iteration. 1 us ~ 32 iters; we use 30 to leave margin
 * for call overhead. 1-Wire timing is generous enough (tolerance ~+/-5 us on
 * 480 us and ~+/-5 us on 60 us slots) that this is fine.
 *
 * Actual behavior: we just want >= the nominal time, never less, so we round
 * up.
 * ========================================================================== */
static inline void delay_us(uint32_t us)
{
    /* Calibrated empirically: volatile inc + compare + branch + nop takes
     * ~10 cycles per iter at 96 MHz (volatile store/load dominate), so
     * ~9.6 iters per microsecond.  Original 24× overshoots by 2.5× and
     * pushes onewire_read_init (6 us) up to 15 us where the DS18B20
     * interprets the master-low as a write-0 slot instead of a read
     * slot, causing every read to return 0xFFFF.  Using 9×. */
    uint32_t loops = us * 9U;
    if (loops == 0U) { loops = 1U; }
    for (volatile uint32_t i = 0; i < loops; i++) {
        __asm__ volatile("nop");
    }
}

static inline void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        delay_us(1000U);
    }
}

/* ==========================================================================
 * P51 GPIO control (1-Wire DQ).
 *
 * Open-drain emulation:
 *   - "Drive low"    -> PMR=0 (GPIO), PODR=0 (pre-set), PDR=1 (output)
 *   - "Release"      -> PDR=0 (input, Hi-Z); external 4.7k pullup wins
 *   - "Read"         -> sample PIDR bit 1
 *
 * Per MPC rules: P51 is GPIO (not peripheral), so PSEL=0 and PMR=0. PFS bit 6
 * (ISEL) should be 0 to avoid interrupt-sense mode.
 * ========================================================================== */
static void p51_onewire_init(void)
{
    /* Unlock PFS */
    REG8(PWPR) = 0x00U;
    REG8(PWPR) = 0x40U;

    /* P51 PFS: PSEL=0, ISEL=0, ASEL=0 -> pure GPIO */
    REG8(P51PFS) = 0x00U;

    /* Re-lock PFS */
    REG8(PWPR) = 0x00U;
    REG8(PWPR) = 0x80U;

    /* PMR=0: GPIO mode (not peripheral). */
    REG8(PORT5_PMR) &= (uint8_t)~P51_BIT;

    /* Pre-set PODR=0: when we flip PDR to output, we drive LOW. */
    REG8(PORT5_PODR) &= (uint8_t)~P51_BIT;

    /* Start in input (released) mode -- pull-up drives line HIGH. */
    REG8(PORT5_PDR) &= (uint8_t)~P51_BIT;
}

static inline void p51_drive_low(void)
{
    /* PODR already 0, just flip direction to output. */
    REG8(PORT5_PDR) |= P51_BIT;
}

static inline void p51_release(void)
{
    /* Back to input -> external pull-up drives line HIGH. */
    REG8(PORT5_PDR) &= (uint8_t)~P51_BIT;
}

static inline bool p51_read(void)
{
    return (REG8(PORT5_PIDR) & P51_BIT) != 0U;
}

/* ==========================================================================
 * P17 diagnostic GPIO (scope output for temperature encoding) + PA7 LED.
 * ========================================================================== */
static void p17_diag_init(void)
{
    REG8(PWPR) = 0x00U;
    REG8(PWPR) = 0x40U;
    REG8(P17PFS) = 0x00U;
    REG8(PWPR) = 0x00U;
    REG8(PWPR) = 0x80U;

    REG8(PORT1_PMR)  &= (uint8_t)~P17_BIT;  /* GPIO mode */
    REG8(PORT1_PODR) &= (uint8_t)~P17_BIT;  /* start LOW */
    REG8(PORT1_PDR)  |= P17_BIT;            /* output */
}

static inline void p17_set(bool high)
{
    if (high) {
        REG8(PORT1_PODR) |= P17_BIT;
    } else {
        REG8(PORT1_PODR) &= (uint8_t)~P17_BIT;
    }
}

static void pa7_led_init(void)
{
    REG8(PORTA_PMR)  &= (uint8_t)~PA7_BIT;
    REG8(PORTA_PDR)  |= PA7_BIT;
    REG8(PORTA_PODR) &= (uint8_t)~PA7_BIT;
}

static inline void pa7_toggle(void)
{
    REG8(PORTA_PODR) ^= PA7_BIT;
}

/* ==========================================================================
 * 1-Wire protocol primitives.
 * ========================================================================== */

/**
 * Reset + presence detect. Returns true if a device pulled the bus low
 * during the presence window.
 */
static bool onewire_reset(void)
{
    p51_drive_low();
    delay_us(k_onewire_reset_pulse_us);      /* 480 us LOW */
    p51_release();
    delay_us(k_onewire_presence_wait_us);    /* 70 us -- device should be pulling low */
    bool line_high = p51_read();
    delay_us(k_onewire_presence_tail_us);    /* 410 us recovery */
    return !line_high;                       /* presence = LOW during sample */
}

static void onewire_write_bit(bool bit)
{
    p51_drive_low();
    if (bit) {
        delay_us(k_onewire_write_1_low_us);     /* 10 us */
        p51_release();
        delay_us(k_onewire_write_1_high_us);    /* 55 us */
    } else {
        delay_us(k_onewire_write_0_low_us);     /* 60 us */
        p51_release();
        delay_us(k_onewire_write_0_high_us);    /* 10 us */
    }
}

static bool onewire_read_bit(void)
{
    p51_drive_low();
    delay_us(k_onewire_read_init_us);           /* 6 us LOW */
    p51_release();
    /* Sample at 9 us from slot start, so wait 9-6 = 3 us more. */
    delay_us(k_onewire_read_sample_us - k_onewire_read_init_us);
    bool b = p51_read();
    delay_us(k_onewire_read_recovery_us);       /* 55 us */
    return b;
}

static void onewire_write_byte(uint8_t b)
{
    for (uint8_t i = 0; i < 8; i++) {
        onewire_write_bit((bool)((b >> i) & 1U));  /* LSB first */
    }
}

static uint8_t onewire_read_byte(void)
{
    uint8_t b = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (onewire_read_bit()) {
            b |= (uint8_t)(1U << i);
        }
    }
    return b;
}

/* ==========================================================================
 * Diagnostic encoder on P17 -- emits the 16-bit temperature MSB-first with
 * wide pulses so an AD2 or scope can read values by eye.
 *
 * Frame format:
 *   1. Two ~10 ms HIGH pulses w/ ~5 ms gaps (frame start)
 *   2. Status pulse: one short HIGH (~3 ms) if presence detected, else none
 *   3. 16 bit cells, MSB first:
 *       - Each cell: LOW for 2 ms (separator), then:
 *           bit 1 -> HIGH for 3 ms
 *           bit 0 -> HIGH for 1 ms
 *   4. LOW for ~500 ms inter-frame gap
 * ========================================================================== */
static void diag_wide_pulse_high(uint32_t ms)
{
    p17_set(true);
    delay_ms(ms);
    p17_set(false);
}

static void diag_emit_frame(bool presence, uint16_t temp_raw)
{
    /* Start marker: two wide pulses. */
    diag_wide_pulse_high(10);
    delay_ms(5);
    diag_wide_pulse_high(10);
    delay_ms(10);

    /* Presence indicator -- single short pulse if OK. */
    if (presence) {
        diag_wide_pulse_high(3);
    }
    delay_ms(10);

    /* 16 temperature bits, MSB first. */
    for (int i = 15; i >= 0; i--) {
        delay_ms(2);  /* inter-bit LOW separator */
        uint32_t width = ((temp_raw >> i) & 1U) ? 3U : 1U;
        diag_wide_pulse_high(width);
    }

    /* Inter-frame gap. */
    delay_ms(500);
}

/* ==========================================================================
 * Main: init clocks, init pins, forever trigger Convert T + read scratchpad
 * + emit scope frame.
 * ========================================================================== */
int main(void)
{
    inline_clock_init();

    p51_onewire_init();
    p17_diag_init();
    pa7_led_init();

    /* Give the external pull-up time to settle the bus HIGH after reset. */
    delay_ms(10);

    for (;;) {
        pa7_toggle();

        /* --- Step 1: Reset + presence --- */
        bool presence = onewire_reset();

        if (presence) {
            /* --- Step 2: Skip ROM + Convert T (single-sensor bus) --- */
            onewire_write_byte(k_onewire_cmd_skip_rom);
            onewire_write_byte(k_ds18b20_cmd_convert_t);

            /* --- Step 3: Wait 750 ms for 12-bit conversion --- */
            delay_ms(750);

            /* --- Step 4: Reset + Skip ROM + Read Scratchpad --- */
            (void)onewire_reset();
            onewire_write_byte(k_onewire_cmd_skip_rom);
            onewire_write_byte(k_ds18b20_cmd_read_scratch);

            /* --- Step 5: Read first 2 bytes: temp LSB, temp MSB --- */
            uint8_t temp_lsb = onewire_read_byte();
            uint8_t temp_msb = onewire_read_byte();

            /* Drain the remaining 7 bytes of scratchpad to terminate cleanly. */
            for (int i = 0; i < 7; i++) {
                (void)onewire_read_byte();
            }

            uint16_t temp_raw = (uint16_t)((uint16_t)temp_msb << 8) | temp_lsb;

            /* --- Step 6: Emit diagnostic frame on P17 --- */
            diag_emit_frame(true, temp_raw);
        } else {
            /* No device: emit frame with presence=false and 0xFFFF so scope
             * shows us we ran but couldn't find the sensor. */
            diag_emit_frame(false, 0xFFFFU);
        }
    }

    return 0;
}

/* =============================================================================
 * Stubs for symbols referenced by vectors.S (ThreadX ISRs, CMT0) that this
 * minimal firmware doesn't use. Prevents linker errors without dragging in the
 * full ThreadX library or a real CMT0 driver.
 * ============================================================================= */
void _tx_thread_context_save(void)    {}
void _tx_thread_context_restore(void) {}
void cmt0_isr(void)                   {}
