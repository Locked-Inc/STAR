/* hw_init.h -- board-specific pin-mux + GPIO setup for the STAR PCB.
 *
 * The Renesas r_bsp FIT module brings up clocks and module-stop, but it does
 * NOT know about our custom pinout. This file does the parts r_bsp leaves to
 * us: PFS unlock, MPC PSEL writes, PMR/PDR/PODR ordering, and the GPIO
 * direction + idle level for nSLEEP / DRVOFF / IMU_RST.
 *
 * Every register touch in hw_init.c MUST cite either the RX72N HW manual
 * (R01UH0824) section number or a known-good test file under
 * star-rx72n-firmware/ -- per the P0 "no guessing on bring-up" rule in
 * /workspaces/STAR/CLAUDE.md.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Top-level board init. Call once after R_BSP_PowerON has finished -- i.e.
 * inside main() before any peripheral driver Open() calls.
 *
 * Sequence:
 *   1. SCI9 pin-mux (PB6 RXD9, PB7 TXD9) -- before R_SCI_Open(SCI_CH9, ...).
 *   2. RIIC1 pin-mux (P20 SDA1, P21 SCL1) -- before R_RIIC_Open(RIIC_CH1).
 *   3. GPTW0..3 pin-mux (motor IN1/IN2 pins) -- before R_GPT_Open().
 *   4. GPIO output: nSLEEP=0, DRVOFF=1 (motors disabled).
 *   5. GPIO output: IMU_RST -- pulsed by bno055_init(), so just configured here.
 *   6. GPIO input + pullup: IMU_INT (P32) -- polled in v0.
 *
 * MSTPCR releases for SCI9 / RIIC1 / GPTW are handled by the FIT modules'
 * Open() calls themselves, so we don't touch MSTPCR here.
 */
void hw_init_board(void);

/* Drive the BNO055 active-low reset line (P83) low for ~5 ms then high.
 * Call before riic1 chip_id probe. Caller waits ~700 ms for BNO055 tBOOT. */
void hw_pulse_imu_reset(void);

/* nSLEEP / DRVOFF helpers used by motors.c. */
void hw_motor_set_nsleep(uint8_t motor_idx, bool enabled);
void hw_motor_set_drvoff(uint8_t motor_idx, bool disabled);
