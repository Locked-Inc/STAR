/* main.c -- MINIMAL SCI9 TX TEST (DIAGNOSTIC; real main saved as main.c.real).
 *
 * Strips everything except:
 *   - D10 (PB0) raw GPIO output (proves main() reached)
 *   - sci9_init() (no FIT, no R_RIIC, no BNO055)
 *   - tight loop: sci9_putc('A') + D10 toggle + 100 ms delay
 *
 * Expected behaviour after flash:
 *   - D10 toggles ~5 Hz: chip is alive, sci9_putc returns each call
 *   - Host: continuous 'A' chars on /dev/ttyACM0 at 115200 8N1
 *
 * Failure interpretation:
 *   - D10 dark forever       -> r_bsp boot still hung (clock/PLL issue resurfaced)
 *   - D10 stuck on/off       -> sci9_putc spinning forever on TDRE (TX buffer never drains
 *                                = TE not actually enabled = SCR/MSTP/PMR write didn't take)
 *   - D10 toggles, no UART   -> SCI thinks it transmitted but pad isn't driven; pin-mux
 *                                (PFS/PMR) didn't take effect at the pad level despite the
 *                                register writes succeeding
 *   - D10 toggles + UART OK  -> entire UART path works; the silence in the IMU build was
 *                                init_all returning false (likely RIIC interrupt wiring) and
 *                                masking the visible output
 *
 * To restore the real firmware:
 *   mv src/main.c.real src/main.c && cmake --build build -j
 */

#include "platform.h"  /* iodefine.h for PORTB struct */
#include "sci9_polled.h"

#include <stdint.h>

static inline void d10_init(void)
{
    PORTB.PMR.BIT.B0  = 0;
    PORTB.PDR.BIT.B0  = 1;
    PORTB.PODR.BIT.B0 = 1;
}
static inline void d10_toggle(void) { PORTB.PODR.BIT.B0 ^= 1; }

static void busy_ms(uint32_t ms)
{
    for (uint32_t m = 0; m < ms; m++) {
        for (volatile uint32_t i = 0; i < 40000U; i++) {
            __asm__ volatile("nop");
        }
    }
}

int main(void)
{
    d10_init();
    busy_ms(200);          /* 200 ms LED-on so a fast watcher sees "main reached" */

    sci9_init();

    /* Tight TX loop: 'A' every 100 ms, D10 toggles per character. */
    for (;;) {
        sci9_putc('A');
        d10_toggle();
        busy_ms(100);
    }
}
