/**
 * @file hw_init.c
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

/* hw_init.c -- STAR PCB pin-mux + GPIO setup.
 *
 * All values cited from either the RX72N HW manual (R01UH0824) or a
 * known-good test file under star-rx72n-firmware/. Per CLAUDE.md "Engineering
 * Discipline" section: no guessing -- every register write has a citation.
 *
 * Citation conventions in comments:
 *   "HW Ch.23"     -> RX72N HW manual chapter 23 (Multi-Function Pin Controller)
 *   "uart_test:NN" -> star-rx72n-firmware/uart_test/sci9.c line NN
 *   "imu_test:NN"  -> star-rx72n-firmware/imu_test/main.c line NN
 *   "pinout.tex"   -> docs/sections/03_hardware_pinout.tex
 */

#include "hw_init.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Raw register addresses. RX72N port + MPC base addresses per HW Ch.22.
 * ============================================================================ */
#define REG8(a)  (*(volatile uint8_t *)(uintptr_t)(a))

typedef enum : uintptr_t {
    /* Port registers (offset = port number, e.g. PORTB = base + 0x0B). */
    k_pdr_base   = 0x0008C000U,  /* Port Direction Register, 0=in 1=out */
    k_podr_base  = 0x0008C020U,  /* Port Output Data Register */
    k_pidr_base  = 0x0008C040U,  /* Port Input Data Register */
    k_pmr_base   = 0x0008C060U,  /* Port Mode Register, 0=GPIO 1=peripheral */
    k_odr0_base  = 0x0008C080U,  /* Open-drain control 0 (pins 0-3) */
    k_odr1_base  = 0x0008C081U,  /* Open-drain control 1 (pins 4-7) */
    k_pcr_base   = 0x0008C0C0U,  /* Pull-up control */

    /* MPC (Multi-Function Pin Controller). HW Ch.23. */
    k_pwpr_addr  = 0x0008C11FU,  /* Write-protect for PFS registers */
    k_pfs_base   = 0x0008C140U,  /* PnmPFS at base + (port*8 + pin) */
} hw_addrs_t;

/* PWPR bit names (HW Ch.23 sec 23.2.1). */
typedef enum : uint8_t {
    k_pwpr_clear   = 0x00U,
    k_pwpr_pfswe   = 0x40U,  /* allow PFS writes */
    k_pwpr_b0wi    = 0x80U,  /* block PFSWE; restored after writes */
} pwpr_bits_t;

/* MPC peripheral function-select values (HW Ch.23 Table 23.6). */
typedef enum : uint8_t {
    k_psel_sci9     = 0x0AU,  /* SCI9 RXD/TXD              -- uart_test/sci9.c:38 */
    k_psel_riic1    = 0x0FU,  /* RIIC1 SCL/SDA             -- imu_test/main.c:217 */
    k_psel_gtio_a   = 0x06U,  /* GPTW GTIOCnA              -- HW Ch.23 (verify on bench) */
    k_psel_gtio_b   = 0x06U,  /* GPTW GTIOCnB              -- same PSEL on B-channel */
} mpc_psel_t;

/* ----------------------------------------------------------------------------
 * Helper: PFS write with the unlock dance. PWPR sequence per HW Ch.23.2.1
 * (also see uart_test/sci9.c:113-118).
 * ---------------------------------------------------------------------------- */
static inline void pfs_unlock(void)
{
    REG8(k_pwpr_addr) = k_pwpr_clear;   /* B0WI=0 */
    REG8(k_pwpr_addr) = k_pwpr_pfswe;   /* PFSWE=1 */
}
static inline void pfs_lock(void)
{
    REG8(k_pwpr_addr) = k_pwpr_clear;   /* PFSWE=0 */
    REG8(k_pwpr_addr) = k_pwpr_b0wi;    /* B0WI=1 */
}
static inline void pfs_write(uint8_t port, uint8_t pin, uint8_t psel_value)
{
    /* PnmPFS address = base + port*8 + pin. */
    REG8(k_pfs_base + (uint32_t)port * 8U + pin) = psel_value;
}

/* GPIO direction + level helpers. Order matters: per CLAUDE.md and
 * uart_test/sci9.c:120-124, set PDR before flipping PMR to peripheral mode. */
static inline void gpio_set_output(uint8_t port, uint8_t pin)
{
    REG8(k_pmr_base + port) &= (uint8_t)~(1U << pin);          /* GPIO mode */
    REG8(k_pdr_base + port) |= (uint8_t)(1U << pin);           /* output */
}
static inline void gpio_set_input(uint8_t port, uint8_t pin, bool pullup)
{
    REG8(k_pmr_base + port) &= (uint8_t)~(1U << pin);
    REG8(k_pdr_base + port) &= (uint8_t)~(1U << pin);
    if (pullup) {
        REG8(k_pcr_base + port) |= (uint8_t)(1U << pin);
    }
}
static inline void gpio_write(uint8_t port, uint8_t pin, bool high)
{
    if (high) {
        REG8(k_podr_base + port) |= (uint8_t)(1U << pin);
    } else {
        REG8(k_podr_base + port) &= (uint8_t)~(1U << pin);
    }
}
static inline void pin_to_peripheral(uint8_t port, uint8_t pin)
{
    REG8(k_pmr_base + port) |= (uint8_t)(1U << pin);
}

/* ============================================================================
 * Pin assignments. All from docs/sections/03_hardware_pinout.tex unless
 * otherwise noted. (port, pin) tuples make the register math obvious.
 * ============================================================================ */

/* SCI9: PB6 = RXD9 (input), PB7 = TXD9 (output). uart_test/sci9.c:120-124. */
#define PORT_PB    0x0BU
#define PIN_RXD9   6U
#define PIN_TXD9   7U

/* RIIC1: P20 = SDA1, P21 = SCL1. imu_test/main.c:11-12. */
#define PORT_P0    0x00U  /* No, P20 means port 2 pin 0 -- see below */
#define PORT_P2    0x02U
#define PIN_SDA1   0U     /* P20 */
#define PIN_SCL1   1U     /* P21 */

/* IMU control: P83 = active-low reset, P32 = INT (input, polled in v0). */
#define PORT_P8    0x08U
#define PIN_IMU_RST 3U
#define PORT_P3    0x03U
#define PIN_IMU_INT 2U

/* Motor IN1/IN2 -> GPTW GTIOCnA/GTIOCnB. From plan/pinout.tex.
 * NOTE: PSEL value for GPTW GTIOC pins varies per pin; verify each against
 * HW Ch.23 Table 23.6 before promoting this code from "untested". For v0 we
 * stub them out and only initialise SCI/RIIC -- motor PWM init lives in
 * motors.c which checks an `armed` flag at runtime. */
#define PORT_P1    0x01U
#define PORT_PC    0x0CU
#define PORT_P6    0x06U
#define PORT_PE    0x0EU

/* nSLEEP / DRVOFF GPIO assignments (per pinout.tex; one pair per motor). */
typedef struct {
    uint8_t nsleep_port;
    uint8_t nsleep_pin;
    uint8_t drvoff_port;
    uint8_t drvoff_pin;
} motor_ctrl_pins_t;

static const motor_ctrl_pins_t s_motor_pins[4] = {
    /* Motor 0: P60 nSLEEP, P61 DRVOFF */
    { .nsleep_port = PORT_P6, .nsleep_pin = 0U, .drvoff_port = PORT_P6, .drvoff_pin = 1U },
    /* Motor 1: P62 nSLEEP, P63 DRVOFF */
    { .nsleep_port = PORT_P6, .nsleep_pin = 2U, .drvoff_port = PORT_P6, .drvoff_pin = 3U },
    /* Motor 2: P64 nSLEEP, PE0 DRVOFF */
    { .nsleep_port = PORT_P6, .nsleep_pin = 4U, .drvoff_port = PORT_PE, .drvoff_pin = 0U },
    /* Motor 3: PE1 nSLEEP, PE2 DRVOFF */
    { .nsleep_port = PORT_PE, .nsleep_pin = 1U, .drvoff_port = PORT_PE, .drvoff_pin = 2U },
};

/* ============================================================================
 * Public API
 * ============================================================================ */
void hw_init_board(void)
{
    pfs_unlock();

    /* SCI9 pin-mux is owned by sci9_init() now (sci9_polled.c) -- avoid
     * double-init that could leave PMR in inconsistent state. */

    /* RIIC1 pin-mux. Citation: imu_test/main.c:221-222. */
    pfs_write(PORT_P2, PIN_SDA1, k_psel_riic1);
    pfs_write(PORT_P2, PIN_SCL1, k_psel_riic1);

    /* GPTW pin-mux is deferred to v1 motor driver (no FIT module exists for
     * PWM on RX72N -- direct GPTW register access from r01an5995 reference). */

    pfs_lock();

    /* RIIC1 pins to peripheral. imu_test/main.c:227. */
    pin_to_peripheral(PORT_P2, PIN_SDA1);
    pin_to_peripheral(PORT_P2, PIN_SCL1);

    /* IMU reset (P83) as output, idle high (deasserted). Pulse done by
     * hw_pulse_imu_reset() called from main(). imu_test/main.c:107-114. */
    gpio_set_output(PORT_P8, PIN_IMU_RST);
    gpio_write(PORT_P8, PIN_IMU_RST, true);

    /* IMU INT (P32) as input with pullup. Polled in v0. */
    gpio_set_input(PORT_P3, PIN_IMU_INT, true);

    /* Motor control GPIOs: nSLEEP=0 (sleep), DRVOFF=1 (disabled). Both safe
     * idle states. motors_arm() flips them when commanded. */
    for (uint8_t i = 0; i < 4U; i++) {
        gpio_set_output(s_motor_pins[i].nsleep_port, s_motor_pins[i].nsleep_pin);
        gpio_set_output(s_motor_pins[i].drvoff_port, s_motor_pins[i].drvoff_pin);
        gpio_write(s_motor_pins[i].nsleep_port, s_motor_pins[i].nsleep_pin, false);
        gpio_write(s_motor_pins[i].drvoff_port, s_motor_pins[i].drvoff_pin, true);
    }
}

void hw_pulse_imu_reset(void)
{
    /* imu_test/main.c:105-114. Idle high -> low 5 ms -> high -> caller waits. */
    gpio_write(PORT_P8, PIN_IMU_RST, true);
    for (volatile uint32_t i = 0; i < 200000U; i++) { __asm__ volatile("nop"); }
    gpio_write(PORT_P8, PIN_IMU_RST, false);
    for (volatile uint32_t i = 0; i < 200000U; i++) { __asm__ volatile("nop"); }
    gpio_write(PORT_P8, PIN_IMU_RST, true);
}

void hw_motor_set_nsleep(uint8_t motor_idx, bool enabled)
{
    if (motor_idx >= 4U) { return; }
    gpio_write(s_motor_pins[motor_idx].nsleep_port,
               s_motor_pins[motor_idx].nsleep_pin, enabled);
}

void hw_motor_set_drvoff(uint8_t motor_idx, bool disabled)
{
    if (motor_idx >= 4U) { return; }
    gpio_write(s_motor_pins[motor_idx].drvoff_port,
               s_motor_pins[motor_idx].drvoff_pin, disabled);
}
