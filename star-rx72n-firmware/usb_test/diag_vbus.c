/**
 * @file diag_vbus.c
 * @brief Minimal diag: prove PB3 toggles + report USB state via sample dumps.
 *
 * Steady state after boot:
 *   - PB3 toggles at ~2 Hz (250 ms on, 250 ms off) forever
 *   - Every toggle, it samples INTSTS0 and DVSQ
 *   - The RAW INTSTS0 value is encoded into PB3 pulse widths via
 *     the pattern: longer HIGH = higher VBUS / DVSQ progression
 *
 * For reliable capture: 2 Hz toggle = 125 samples per half-cycle at 500 Hz
 */
#include <stdint.h>

#define REG8(a)  (*(volatile uint8_t  *)(a))
#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

static void busy(volatile uint32_t n) { while (n--) { __asm__("nop"); } }

#define PB3_INIT()  do { REG8(0x8C00B) |= 0x08U; REG8(0x8C02B) &= ~0x08U; } while (0)
#define PB3_HIGH()  do { REG8(0x8C02B) |=  0x08U; } while (0)
#define PB3_LOW()   do { REG8(0x8C02B) &= ~0x08U; } while (0)

int main(void)
{
    /* Init PB3 */
    PB3_INIT();
    REG8(0x8C00A) |= 0x80U;          /* LED */

    /* Short HIGH pulse at very start to mark reset */
    PB3_HIGH(); busy(50000U); PB3_LOW(); busy(50000U);

    /* Enable USB0 module */
    REG16(0x803FE) = 0xA502U;
    REG32(0x80014) &= ~(1UL << 19);
    REG16(0x803FE) = 0xA500U;

    /* Clocks: HOCO PLL */
    REG16(0x803FE) = 0xA503U;
    REG8(0x80036) = 0x00U;
    while ((REG8(0x8003C) & 0x08U) == 0) { __asm__ volatile("nop"); }
    REG16(0x80028) = 0x1710U;
    REG8(0x8002A) = 0x00U;
    uint32_t to = 10000000U;
    while (to > 0 && (REG8(0x8003C) & 0x04U) == 0) { to--; }
    REG8(0x8101C) = 0x01U;
    REG32(0x80020) = 0x21C21211U;
    REG16(0x80024) = 0x0031U;
    REG16(0x80026) = 0x0400U;
    REG16(0x803FE) = 0xA500U;

    /* VBUS pin MPC */
    REG8(0x8C001) &= ~(1U << 6);
    REG8(0x8C11F) = 0x00U;
    REG8(0x8C11F) = 0x40U;
    REG8(0x8C14E) = 0x11U;
    REG8(0x8C11F) = 0x00U;
    REG8(0x8C11F) = 0x80U;

    /* USB0 enable */
    REG16(0xA0000) = 0x0000U;
    busy(100000U);
    REG16(0xA0000) |= (1U << 0);
    REG16(0xA0000) |= (1U << 10);
    busy(100000U);

    /* DCP config + DPRPU */
    REG16(0xA005C) = 0x0000U;
    REG16(0xA005E) = 64U;
    REG16(0xA0060) = 0x0001U;
    REG16(0xA0000) |= (1U << 4);     /* DPRPU */

    /* Main loop: simple 2 Hz toggle + encode DVSQ as BRIEF HIGH pulse count
     * WITHIN each HIGH phase.
     *
     * Pattern:
     *   [HIGH for 500 ms with pulse_count mini-dips in it][LOW 500 ms]
     *
     * pulse_count = dvsq+1. So:
     *   dvsq=0 (Powered):       HIGH plateau (no dips)
     *   dvsq=1 (Default):       1 dip during HIGH phase
     *   dvsq=2 (Address):       2 dips
     *   dvsq=3 (Configured):    3 dips
     */
    #define HALF_PERIOD  3000000U  /* ~0.5 s at 96 MHz ICLK */

    for (;;) {
        uint16_t intsts = REG16(0xA0040);
        uint8_t dvsq = (intsts >> 4) & 0x7U;

        /* HIGH phase */
        PB3_HIGH();
        if (dvsq == 0) {
            busy(HALF_PERIOD);
        } else {
            /* Split HIGH into segments with brief dips */
            uint32_t seg = HALF_PERIOD / (dvsq + 1);
            for (uint8_t i = 0; i < dvsq; i++) {
                busy(seg);
                PB3_LOW();  busy(20000U);
                PB3_HIGH();
            }
            busy(seg);
        }

        /* LOW phase */
        PB3_LOW();
        busy(HALF_PERIOD);

        REG8(0x8C02A) ^= 0x80U;
    }
    return 0;
}
