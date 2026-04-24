/**
 * @file bno055.c
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

/* bno055.c -- BNO055 IMU driver via Renesas r_riic_rx FIT module.
 *
 * Verified against vendor/FITModules/r_riic_rx/r_riic_rx/r_riic_rx_if.h
 * (v3.11) and r_riic_rx.c. Key API facts that drove this implementation:
 *
 *   - R_RIIC_Open(riic_info_t *)          -- single struct arg
 *   - R_RIIC_MasterSend(riic_info_t *)
 *   - R_RIIC_MasterReceive(riic_info_t *)
 *   - typedef void (*riic_callback)(void) -- callback takes NO arguments
 *   - p_slv_adr is a pointer to a single 7-bit address byte
 *   - completion status comes via the global g_riic_ChStatus[ch] array
 *     after the callback fires (RIIC_FINISH = success, others = error)
 *
 * IMU mode (OPR_MODE = 0x08) gives raw accel + gyro at 100 Hz with no
 * orientation -- which is what robot_localization wants on the Pi side.
 */

#include "bno055.h"

#include "platform.h"
#include "r_riic_rx_if.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ---- BNO055 register map (subset, datasheet rev 1.6 sec 4.3) ----------- */
typedef enum : uint8_t {
    k_bno_addr        = 0x28U,
    k_bno_reg_chip_id = 0x00U,
    k_bno_reg_opr     = 0x3DU,
    k_bno_reg_acc_x   = 0x08U,
    k_bno_chip_id_val = 0xA0U,
    k_bno_mode_config = 0x00U,
    k_bno_mode_imu    = 0x08U,
} bno055_reg_t;

#define BNO_ACC_LSB_PER_MPS2   (100.0f)
#define BNO_GYR_LSB_PER_DPS    (16.0f)
#define DEG_TO_RAD             (0.017453292519943295f)

#define BNO_RIIC_CH            (1U)

/* g_riic_ChStatus[ch] is updated by the RIIC driver after every transfer.
 * Header declares it as plain (non-volatile) -- match exactly to avoid type
 * conflicts. */
extern riic_ch_dev_status_t g_riic_ChStatus[];

/* Slave address byte -- referenced via riic_info_t.p_slv_adr. Must outlive the
 * transfer. RIIC API requires a writable pointer, so non-const. */
static uint8_t s_bno_addr_byte = k_bno_addr;

/* Callback flag -- set inside the no-arg callback below; the calling code
 * spins on this with a timeout, then inspects g_riic_ChStatus[CH] to learn
 * whether the transfer succeeded or NACKed. */
static volatile bool s_riic_done = false;

static void riic_done_cb(void)
{
    s_riic_done = true;
}

/* Coarse busy-wait. r_bsp also exports R_BSP_SoftwareDelay(time, units) once
 * BSP_DELAY_MICROSECS / BSP_DELAY_MILLISECS are visible; promote to that
 * after first build succeeds. */
static void busy_wait_ms(uint32_t ms)
{
    for (uint32_t m = 0; m < ms; m++) {
        for (volatile uint32_t i = 0; i < 60000U; i++) {
            __asm__ volatile("nop");
        }
    }
}

static bool wait_for_riic(uint32_t timeout_ms)
{
    const uint32_t poll = timeout_ms * 100000U; /* generous, NOP loop */
    for (uint32_t i = 0; i < poll; i++) {
        if (s_riic_done) {
            return (g_riic_ChStatus[BNO_RIIC_CH] == RIIC_FINISH);
        }
        __asm__ volatile("nop");
    }
    return false;
}

/* Helper: build the riic_info_t for a controller transfer. The struct lives
 * on the caller's stack and is passed by pointer to R_RIIC_*. */
static void fill_info(riic_info_t *info,
                      uint8_t *p_data1, uint32_t cnt1,
                      uint8_t *p_data2, uint32_t cnt2)
{
    /* Zero entire struct first to clear reserved fields. */
    memset((void *)info, 0, sizeof(*info));
    info->ch_no        = BNO_RIIC_CH;
    info->callbackfunc = riic_done_cb;
    info->p_slv_adr    = &s_bno_addr_byte;
    info->p_data1st    = p_data1;
    info->cnt1st       = cnt1;
    info->p_data2nd    = p_data2;
    info->cnt2nd       = cnt2;
    /* dev_sts gets populated by the driver itself; leave as 0 (RIIC_NO_INIT
     * marker for the very first call, RIIC_IDLE thereafter). */
}

static bool i2c_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t       buf[2] = { reg, val };
    riic_info_t   info;
    fill_info(&info, buf, sizeof(buf), NULL, 0);

    s_riic_done = false;
    if (R_RIIC_MasterSend(&info) != RIIC_SUCCESS) {
        return false;
    }
    return wait_for_riic(50U);
}

static bool i2c_read_burst(uint8_t reg, uint8_t *buf, uint32_t count)
{
    uint8_t     reg_byte = reg;
    riic_info_t info;
    /* For controller-receive, data1st = register address (sent), data2nd =
     * receive buffer (filled). The driver issues START + addr|W + reg, then
     * RS + addr|R + N data bytes + STOP. */
    fill_info(&info, &reg_byte, 1U, buf, count);

    s_riic_done = false;
    if (R_RIIC_MasterReceive(&info) != RIIC_SUCCESS) {
        return false;
    }
    return wait_for_riic(50U);
}

bool bno055_open_bus(void)
{
    /* Open the RIIC1 channel once at startup. The driver associates the
     * passed callback with this channel; subsequent MasterSend/Receive can
     * pass a fresh riic_info_t each time. */
    riic_info_t info;
    fill_info(&info, NULL, 0, NULL, 0);
    return (R_RIIC_Open(&info) == RIIC_SUCCESS);
}

bool bno055_init(void)
{
    /* Probe CHIP_ID. */
    uint8_t chip_id = 0xFFU;
    if (!i2c_read_burst(k_bno_reg_chip_id, &chip_id, 1U)) {
        return false;
    }
    if (chip_id != k_bno_chip_id_val) {
        return false;
    }

    /* CONFIG -> IMU per datasheet sec 3.3.1 (mode changes via CONFIG only). */
    if (!i2c_write_reg(k_bno_reg_opr, k_bno_mode_config)) { return false; }
    busy_wait_ms(25);
    if (!i2c_write_reg(k_bno_reg_opr, k_bno_mode_imu))    { return false; }
    busy_wait_ms(25);

    return true;
}

bool bno055_read_imu(float out_accel_mps2[3], float out_gyro_rps[3])
{
    /* Burst 18 bytes covering ACC (6) + MAG (6) + GYR (6) starting at 0x08.
     * MAG bytes are discarded -- IMU mode runs the mag at low rate. */
    uint8_t block[18] = {0};
    if (!i2c_read_burst(k_bno_reg_acc_x, block, sizeof(block))) {
        return false;
    }

    int16_t ax = (int16_t)((uint16_t)block[0]  | ((uint16_t)block[1]  << 8));
    int16_t ay = (int16_t)((uint16_t)block[2]  | ((uint16_t)block[3]  << 8));
    int16_t az = (int16_t)((uint16_t)block[4]  | ((uint16_t)block[5]  << 8));
    int16_t gx = (int16_t)((uint16_t)block[12] | ((uint16_t)block[13] << 8));
    int16_t gy = (int16_t)((uint16_t)block[14] | ((uint16_t)block[15] << 8));
    int16_t gz = (int16_t)((uint16_t)block[16] | ((uint16_t)block[17] << 8));

    out_accel_mps2[0] = (float)ax / BNO_ACC_LSB_PER_MPS2;
    out_accel_mps2[1] = (float)ay / BNO_ACC_LSB_PER_MPS2;
    out_accel_mps2[2] = (float)az / BNO_ACC_LSB_PER_MPS2;
    out_gyro_rps[0]   = ((float)gx / BNO_GYR_LSB_PER_DPS) * DEG_TO_RAD;
    out_gyro_rps[1]   = ((float)gy / BNO_GYR_LSB_PER_DPS) * DEG_TO_RAD;
    out_gyro_rps[2]   = ((float)gz / BNO_GYR_LSB_PER_DPS) * DEG_TO_RAD;
    return true;
}
