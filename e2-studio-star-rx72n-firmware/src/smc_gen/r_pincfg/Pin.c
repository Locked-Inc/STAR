/*
* Copyright (c) 2016 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
* File Name        : Pin.c
* Version          : 1.0.2
* Device(s)        : R5F572NNHxFB
* Description      : This file implements SMC pin code generation.
***********************************************************************************************************************/

/***********************************************************************************************************************
Pragma directive
***********************************************************************************************************************/
/* Start user code for pragma. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

/***********************************************************************************************************************
Includes
***********************************************************************************************************************/
#include "r_cg_macrodriver.h"
/* Start user code for include. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
#include "r_cg_userdefine.h"

/***********************************************************************************************************************
Global variables and functions
***********************************************************************************************************************/
/* Start user code for global. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

/***********************************************************************************************************************
* Function Name: R_Pins_Create
* Description  : This function initializes Smart Configurator pins
* Arguments    : None
* Return Value : None
***********************************************************************************************************************/

void R_Pins_Create(void)
{
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_MPC);

    /* Set AN004 pin */
    PORT4.PMR.BYTE &= 0xEFU;
    PORT4.PDR.BYTE &= 0xEFU;
    MPC.P44PFS.BYTE = 0x80U;

    /* Set AN005 pin */
    PORT4.PMR.BYTE &= 0xDFU;
    PORT4.PDR.BYTE &= 0xDFU;
    MPC.P45PFS.BYTE = 0x80U;

    /* Set AN006 pin */
    PORT4.PMR.BYTE &= 0xBFU;
    PORT4.PDR.BYTE &= 0xBFU;
    MPC.P46PFS.BYTE = 0x80U;

    /* Set AN007 pin */
    PORT4.PMR.BYTE &= 0x7FU;
    PORT4.PDR.BYTE &= 0x7FU;
    MPC.P47PFS.BYTE = 0x80U;

    /* Set CS4# pin */
    PORT7.PMR.BYTE &= 0xEFU;

    /* Set GTETRGA pin */
    MPC.P15PFS.BYTE = 0x1EU;
    PORT1.PMR.BYTE |= 0x20U;

    /* Set GTETRGB pin */
    MPC.PA6PFS.BYTE = 0x1EU;
    PORTA.PMR.BYTE |= 0x40U;

    /* Set GTETRGC pin */
    MPC.PC4PFS.BYTE = 0x1EU;
    PORTC.PMR.BYTE |= 0x10U;

    /* Set GTETRGD pin */
    MPC.P14PFS.BYTE = 0x1EU;
    PORT1.PMR.BYTE |= 0x10U;

    /* Set GTIOC0A pin */
    MPC.P23PFS.BYTE = 0x1EU;
    PORT2.PMR.BYTE |= 0x08U;

    /* Set GTIOC0B pin */
    MPC.P17PFS.BYTE = 0x1EU;
    PORT1.PMR.BYTE |= 0x80U;

    /* Set GTIOC1A pin */
    MPC.P22PFS.BYTE = 0x1EU;
    PORT2.PMR.BYTE |= 0x04U;

    /* Set GTIOC1B pin */
    MPC.PC3PFS.BYTE = 0x1EU;
    PORTC.PMR.BYTE |= 0x08U;

    /* Set GTIOC2A pin */
    MPC.PE3PFS.BYTE = 0x1EU;
    PORTE.PMR.BYTE |= 0x08U;

    /* Set GTIOC2B pin */
    MPC.P86PFS.BYTE = 0x1EU;
    PORT8.PMR.BYTE |= 0x40U;

    /* Set GTIOC3A pin */
    MPC.PE7PFS.BYTE = 0x1EU;
    PORTE.PMR.BYTE |= 0x80U;

    /* Set GTIOC3B pin */
    MPC.PC6PFS.BYTE = 0x1EU;
    PORTC.PMR.BYTE |= 0x40U;

    /* Set IRQ8 pin */
    MPC.P00PFS.BYTE = 0x40U;
    PORT0.PMR.BYTE &= 0xFEU;
    PORT0.PDR.BYTE &= 0xFEU;

    /* Set IRQ9 pin */
    MPC.P01PFS.BYTE = 0x40U;
    PORT0.PMR.BYTE &= 0xFDU;
    PORT0.PDR.BYTE &= 0xFDU;

    /* Set IRQ10 pin */
    MPC.P02PFS.BYTE = 0x40U;
    PORT0.PMR.BYTE &= 0xFBU;
    PORT0.PDR.BYTE &= 0xFBU;

    /* Set IRQ11 pin */
    MPC.P03PFS.BYTE = 0x40U;
    PORT0.PMR.BYTE &= 0xF7U;
    PORT0.PDR.BYTE &= 0xF7U;

    /* Set IRQ13 pin */
    MPC.P05PFS.BYTE = 0x40U;
    PORT0.PMR.BYTE &= 0xDFU;
    PORT0.PDR.BYTE &= 0xDFU;

    /* Set IRQ15 pin */
    MPC.P67PFS.BYTE = 0x40U;
    PORT6.PMR.BYTE &= 0x7FU;
    PORT6.PDR.BYTE &= 0x7FU;

    /* Set MISOC pin */
    MPC.PD2PFS.BYTE = 0x0DU;
    PORTD.PMR.BYTE |= 0x04U;

    /* Set MOSIC pin */
    MPC.PD1PFS.BYTE = 0x0DU;
    PORTD.PMR.BYTE |= 0x02U;

    /* Set MTCLKA pin */
    MPC.P24PFS.BYTE = 0x02U;
    PORT2.PMR.BYTE |= 0x10U;

    /* Set MTCLKB pin */
    MPC.P25PFS.BYTE = 0x02U;
    PORT2.PMR.BYTE |= 0x20U;

    /* Set MTCLKC pin */
    MPC.PA1PFS.BYTE = 0x02U;
    PORTA.PMR.BYTE |= 0x02U;

    /* Set MTCLKD pin */
    MPC.PC5PFS.BYTE = 0x02U;
    PORTC.PMR.BYTE |= 0x20U;

    /* Set RSPCKC pin */
    MPC.PD3PFS.BYTE = 0x0DU;
    PORTD.PMR.BYTE |= 0x08U;

    /* Set RXD9 pin */
    MPC.PB6PFS.BYTE = 0x0AU;
    PORTB.PMR.BYTE |= 0x40U;

    /* Set SCK12 pin */
    MPC.PE0PFS.BYTE = 0x0CU;
    PORTE.PMR.BYTE |= 0x01U;

    /* Set SCL0 pin */
    MPC.P12PFS.BYTE = 0x0FU;
    PORT1.PMR.BYTE |= 0x04U;

    /* Set SCL1 pin */
    MPC.P21PFS.BYTE = 0x0FU;
    PORT2.PMR.BYTE |= 0x02U;

    /* Set SDA0 pin */
    MPC.P13PFS.BYTE = 0x0FU;
    PORT1.PMR.BYTE |= 0x08U;

    /* Set SDA1 pin */
    MPC.P20PFS.BYTE = 0x0FU;
    PORT2.PMR.BYTE |= 0x01U;

    /* Set SMISO12 pin */
    MPC.PE2PFS.BYTE = 0x0CU;
    PORTE.PMR.BYTE |= 0x04U;

    /* Set SMOSI12 pin */
    MPC.PE1PFS.BYTE = 0x0CU;
    PORTE.PMR.BYTE |= 0x02U;

    /* Set SSLC0 pin */
    MPC.PD4PFS.BYTE = 0x0DU;
    PORTD.PMR.BYTE |= 0x10U;

    /* Set TCLKA pin */
    MPC.PC2PFS.BYTE = 0x03U;
    PORTC.PMR.BYTE |= 0x04U;

    /* Set TCLKB pin */
    MPC.PA3PFS.BYTE = 0x04U;
    PORTA.PMR.BYTE |= 0x08U;

    /* Set TCLKC pin */
    MPC.PC0PFS.BYTE = 0x03U;
    PORTC.PMR.BYTE |= 0x01U;

    /* Set TCLKD pin */
    MPC.PB3PFS.BYTE = 0x04U;
    PORTB.PMR.BYTE |= 0x08U;

    /* Set TXD9 pin */
    PORTB.PODR.BYTE |= 0x80U;
    MPC.PB7PFS.BYTE = 0x0AU;
    PORTB.PDR.BYTE |= 0x80U;
    // PORTB.PMR.BIT.B7 = 1U; // Please set the PMR bit after TE bit is set to 1.

    /* Set USB0_VBUS pin */
    MPC.P16PFS.BYTE = 0x11U;
    PORT1.PMR.BYTE |= 0x40U;

    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_MPC);
}

