/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws.
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
* this software. By using this software, you agree to the additional terms and conditions found by accessing the
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2019 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* MODIFICATION NOTICE
* This file has been modified by the STAR project for use in the STAR robotics platform.
* Modifications include: Documentation additions, C23 typed enum conversions, and style guide compliance.
* Modified files are maintained by Locked, Inc. as part of the STAR project.
* Original Renesas code remains under Renesas copyright as stated above.
***********************************************************************************************************************/

#pragma once

/**
 * @file r_rtos.h
 * @brief RTOS integration header for BSP
 *
 * @details
 * Provides conditional includes and configuration for supported Real-Time
 * Operating Systems (RTOS). This header enables BSP code to integrate with
 * various RTOS kernels through compile-time selection via BSP_CFG_RTOS_USED.
 *
 * **Supported RTOS:**
 * - **None (0):** Bare-metal, no RTOS
 * - **FreeRTOS (1):** Open-source RTOS from AWS
 * - **SEGGER embOS (2):** Commercial RTOS (not currently supported)
 * - **Micrium MicroC/OS (3):** Commercial RTOS (not currently supported)
 * - **Renesas RI600V4/RI600PX (4):** Renesas ITRON-based RTOS
 * - **Azure RTOS (5):** Microsoft ThreadX (currently used in STAR project)
 *
 * For Renesas RI600 RTOS variants, this header defines constants to distinguish
 * between RI600V4 and RI600PX kernels and overrides the system timer configuration.
 *
 * @par Configuration
 * RTOS selection is controlled by BSP_CFG_RTOS_USED in r_bsp_config.h.
 * Additional RTOS-specific settings:
 * - BSP_CFG_RENESAS_RTOS_USED: Select RI600V4 (0) or RI600PX (1)
 * - BSP_CFG_RTOS_SYSTEM_TIMER: CMT channel for RTOS system tick
 *
 * @par Hardware Dependencies
 * - RX72N microcontroller (R5F572NN)
 * - CMT (Compare Match Timer) for RTOS system tick (if RTOS enabled)
 *
 * @par References
 * - r_bsp_config.h for RTOS configuration macros
 * - Azure RTOS ThreadX User Guide (for BSP_CFG_RTOS_USED == 5)
 * - Renesas RI600V4/RI600PX User's Manual (for BSP_CFG_RTOS_USED == 4)
 *
 * @note Ported from Renesas Smart Configurator (SMC) generated code
 * @warning Changing BSP_CFG_RTOS_USED requires rebuild of entire project
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: No goto, setjmp/longjmp, or recursion (header-only, no control flow)
 * - Rule 8: Named enums used for RTOS variant constants
 * - Rule 10: Compiles with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - Single Responsibility: RTOS selection and integration only
 * - Open/Closed: Extensible via new BSP_CFG_RTOS_USED values
 * - Dependency Inversion: Abstracts RTOS kernel behind compile-time selection
 *
 * @author STAR Project (Locked, Inc.)
 * @date 2019 (original), 2026 (STAR modifications)
 * @version 1.11
 * @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
 */
/**********************************************************************************************************************
* History : DD.MM.YYYY Version  Description
*         : 28.02.2019 1.00     First Release
*         : 08.10.2019 1.10     Added include file and macro definitions for Renesas RTOS (RI600V4 or RI600PX).
*         : 26.02.2021 1.11     Changed BSP_CFG_RTOS_USED for Azure RTOS.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "r_bsp_config.h"

#if BSP_CFG_RTOS_USED == 0   /* Non-OS */
#elif BSP_CFG_RTOS_USED == 1 /* FreeRTOS */
#include "FreeRTOS.h"
#include "croutine.h"
#include "event_groups.h"
#include "freertos_start.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#elif BSP_CFG_RTOS_USED == 2 /* SEGGER embOS */
#elif BSP_CFG_RTOS_USED == 3 /* Micrium MicroC/OS */
#elif BSP_CFG_RTOS_USED == 4 /* Renesas RI600V4 & RI600PX */
#include "kernel.h"
#include "kernel_id.h"

/**
 * @enum renesas_rtos_variant_t
 * @brief Renesas ITRON RTOS variant selection
 *
 * @details
 * Identifies which Renesas ITRON-based RTOS kernel variant is in use
 * when BSP_CFG_RTOS_USED == 4 (Renesas RI600V4 & RI600PX).
 *
 * The variant is selected via BSP_CFG_RENESAS_RTOS_USED in r_bsp_config.h:
 * - BSP_CFG_RENESAS_RTOS_USED == 0: Use RI600V4
 * - BSP_CFG_RENESAS_RTOS_USED == 1: Use RI600PX
 *
 * **RI600V4:** Standard ITRON kernel with T-Kernel extensions
 * **RI600PX:** High-performance variant with additional real-time features
 *
 * @invariant Value must be 0 or 1 (only two kernel variants supported)
 *
 * @code
 *   // Example: Check RTOS variant (use integer literal, not enum in #if)
 *   #if BSP_CFG_RENESAS_RTOS_USED == 0
 *     // Using RI600V4 kernel (0 = k_renesas_rtos_ri600v4)
 *   #endif
 *
 *   // Or use the provided macros (they are integer literals):
 *   #if BSP_CFG_RENESAS_RTOS_USED == RENESAS_RI600V4
 *     // Using RI600V4 kernel
 *   #endif
 *
 *   // Or use runtime C check with enum:
 *   if (BSP_CFG_RENESAS_RTOS_USED == (uint8_t)k_renesas_rtos_ri600v4) {
 *     // Runtime RTOS variant check
 *   }
 * @endcode
 *
 * @see BSP_CFG_RTOS_USED RTOS type selection
 * @see BSP_CFG_RENESAS_RTOS_USED Renesas RTOS variant selection
 * @see RENESAS_RI600V4 Integer literal for preprocessor use
 * @see RENESAS_RI600PX Integer literal for preprocessor use
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_renesas_rtos_ri600v4 = 0, /**< RI600V4 kernel (T-Kernel extensions) */
  k_renesas_rtos_ri600px = 1, /**< RI600PX kernel (high-performance variant) */
} renesas_rtos_variant_t;

/**
 * @def RENESAS_RI600V4
 * @brief Renesas RI600V4 kernel selection value
 *
 * @details
 * Use this value with BSP_CFG_RENESAS_RTOS_USED to select RI600V4 kernel.
 * Corresponds to renesas_rtos_variant_t::k_renesas_rtos_ri600v4.
 *
 * Integer literal (not enum) to allow preprocessor evaluation in #if directives.
 *
 * @see renesas_rtos_variant_t
 * @since Version 1.0.0
 */
#define RENESAS_RI600V4 0

/**
 * @def RENESAS_RI600PX
 * @brief Renesas RI600PX kernel selection value
 *
 * @details
 * Use this value with BSP_CFG_RENESAS_RTOS_USED to select RI600PX kernel.
 * Corresponds to renesas_rtos_variant_t::k_renesas_rtos_ri600px.
 *
 * Integer literal (not enum) to allow preprocessor evaluation in #if directives.
 *
 * @see renesas_rtos_variant_t
 * @since Version 1.0.0
 */
#define RENESAS_RI600PX 1

#undef BSP_CFG_RTOS_SYSTEM_TIMER
/**
 * @def BSP_CFG_RTOS_SYSTEM_TIMER
 * @brief Override RTOS system timer for Renesas RI600 kernels
 *
 * @details
 * Redefines BSP_CFG_RTOS_SYSTEM_TIMER to use the kernel-defined timer
 * (_RI_CLOCK_TIMER) for Renesas RI600V4 and RI600PX RTOS variants.
 *
 * This overrides any user-configured CMT channel from r_bsp_config.h to
 * ensure compatibility with the RI600 kernel's timer requirements.
 *
 * @note Only active when BSP_CFG_RTOS_USED == 4
 * @since Version 1.0.0
 */
#define BSP_CFG_RTOS_SYSTEM_TIMER _RI_CLOCK_TIMER
#elif BSP_CFG_RTOS_USED == 5 /* Azure RTOS */
#else
#endif

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/
