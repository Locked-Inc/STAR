/**
 * @file system_stm32f7xx.c
 * @brief STM32F7 system initialization and clock configuration (216 MHz core, 48 MHz USB).
 *
 * @details
 * Provides the three CMSIS-required system symbols and the project clock-init
 * function:
 *
 *  - SystemCoreClock          -- CMSIS global; updated to 216 MHz after PLL lock
 *  - AHBPrescTable[16]        -- CMSIS AHB prescaler shift table (HAL RCC dependency)
 *  - APBPrescTable[8]         -- CMSIS APB prescaler shift table (HAL RCC dependency)
 *  - SystemInit()             -- startup assembly hook; enables FPU (CP10/CP11)
 *  - SystemCoreClockUpdate()  -- CMSIS update function; no-op (HAL maintains value)
 *  - stm32_system_clock_init()-- HAL-based PLL configuration for 216 MHz / 48 MHz USB
 *
 * **Clock targets (216 MHz from 8 MHz HSE):**
 *
 * | Domain   | Frequency | Prescaler       |
 * |----------|-----------|-----------------|
 * | SYSCLK   | 216 MHz   | PLL / PLLP(2)  |
 * | HCLK     | 216 MHz   | AHB  / 1       |
 * | APB1     |  54 MHz   | HCLK / 4       |
 * | APB2     | 108 MHz   | HCLK / 2       |
 * | PLL48CLK |  48 MHz   | PLL / PLLQ(9)  |
 *
 * **PLL parameters (HSE = 8 MHz):**
 *   PLLM = 4  -> VCO input  = 8/4   =   2 MHz
 *   PLLN = 216 -> VCO output = 2*216 = 432 MHz
 *   PLLP = 2  -> SYSCLK     = 432/2 = 216 MHz
 *   PLLQ = 9  -> PLL48CLK   = 432/9 =  48 MHz
 *
 * **HSI fallback (STM32_USE_HSE not defined; HSI = 16 MHz):**
 *   PLLM = 8  -> VCO input  = 16/8  =   2 MHz (same VCO path)
 *   PLLN/PLLP/PLLQ unchanged.
 *
 * @note stm32_system_clock_init() must be called from main() after HAL_Init()
 *       and before stm32_usb_cdc_init(). It must NOT be called from SystemInit()
 *       because HAL tick (SysTick) is not yet running during startup assembly.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "stm32f7xx_hal.h"

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * CMSIS-required prescaler tables
 *
 * AHBPrescTable: maps RCC_CFGR.HPRE[3:0] field to a right-shift count.
 *   Divisor = 2^shift. Field values 0x0-0x7 are "not divided" (shift 0).
 *
 * APBPrescTable: maps RCC_CFGR.PPREx[2:0] field to a right-shift count.
 *   Divisor = 2^shift. Field values 0x0-0x3 are "not divided" (shift 0).
 *
 * These are read directly by stm32f7xx_hal_rcc.c (HAL_RCC_ClockConfig and
 * HAL_RCC_GetSysClockFreq). They must be defined exactly once in the project.
 * ---------------------------------------------------------------------------*/

/**
 * @var AHBPrescTable
 * @brief CMSIS AHB clock prescaler shift table indexed by RCC_CFGR.HPRE.
 *
 * @details
 * Entry N gives the right-shift count such that HCLK = SYSCLK >> entry[HPRE].
 * Values come directly from the STM32F7 Reference Manual Table 15 (RCC_CFGR
 * HPRE field encoding).
 *
 * @note Read by stm32f7xx_hal_rcc.c; never write from application code.
 * @since Version 1.0.0
 */
const uint8_t AHBPrescTable[16] = {
    0U,
    0U,
    0U,
    0U, /* HPRE 0000-0011: SYSCLK not divided (/1)  */
    0U,
    0U,
    0U,
    0U, /* HPRE 0100-0111: SYSCLK not divided (/1)  */
    1U, /* HPRE 1000: /2   */
    2U, /* HPRE 1001: /4   */
    3U, /* HPRE 1010: /8   */
    4U, /* HPRE 1011: /16  */
    6U, /* HPRE 1100: /64  */
    7U, /* HPRE 1101: /128 */
    8U, /* HPRE 1110: /256 */
    9U, /* HPRE 1111: /512 */
};

/**
 * @var APBPrescTable
 * @brief CMSIS APB clock prescaler shift table indexed by RCC_CFGR.PPREx.
 *
 * @details
 * Entry N gives the right-shift count such that PCLKx = HCLK >> entry[PPREx].
 * Values from STM32F7 Reference Manual Table 16 (RCC_CFGR PPREx field).
 *
 * @note Read by stm32f7xx_hal_rcc.c; never write from application code.
 * @since Version 1.0.0
 */
const uint8_t APBPrescTable[8] = {
    0U,
    0U,
    0U,
    0U, /* PPREx 000-011: HCLK not divided (/1) */
    1U, /* PPREx 100: /2  */
    2U, /* PPREx 101: /4  */
    3U, /* PPREx 110: /8  */
    4U, /* PPREx 111: /16 */
};

/* ---------------------------------------------------------------------------
 * CMSIS system clock global
 * ---------------------------------------------------------------------------*/

/**
 * @var SystemCoreClock
 * @brief Current SYSCLK frequency in Hz.
 *
 * @details
 * Required by CMSIS and TinyUSB (dwc2_stm32.h uses it for USB remote-wakeup
 * delay timing). Initialized to 16 MHz (HSI default at reset). Updated to
 * 216000000UL by both HAL_RCC_ClockConfig() (automatically) and
 * stm32_system_clock_init() (explicitly as belt-and-suspenders).
 *
 * @note Declared extern in system_stm32f7xx.h; defined exactly once here.
 * @warning Never write directly; update via stm32_system_clock_init() only.
 * @since Version 1.0.0
 */
uint32_t SystemCoreClock = 16000000UL;

/* ---------------------------------------------------------------------------
 * FPU enable constants
 * ---------------------------------------------------------------------------*/

/**
 * @enum fpu_config_t
 * @brief Cortex-M7 FPU enable register address and bit mask.
 *
 * @details
 * CPACR is the Coprocessor Access Control Register at fixed Cortex-M address
 * 0xE000ED88. Writing 0b11 to CP10[21:20] and CP11[23:22] enables full FPU
 * access from both privileged and unprivileged code.
 *
 * @note uintptr_t is mandatory for the address constant: on the 32-bit STM32F7
 *       target uintptr_t == uint32_t, but on a 64-bit x86_64 test host
 *       uintptr_t == uint64_t. Using uint32_t would silently truncate on the
 *       test host.
 * @since Version 1.0.0
 */
typedef enum {
    k_cpacr_addr = 0xE000ED88U,                   /**< CPACR fixed Cortex-M address */
    k_cp10_cp11  = ((3UL << 20U) | (3UL << 22U)), /**< Full access bits CP10+CP11 */
} fpu_config_t;

/* ---------------------------------------------------------------------------
 * PLL configuration constants
 * ---------------------------------------------------------------------------*/

/**
 * @enum pll_hse_dividers_t
 * @brief PLL input/output dividers for 216 MHz SYSCLK from 8 MHz HSE.
 *
 * @details
 * HSE clock path:
 *   VCO input  = HSE / PLLM = 8 MHz / 4 = 2 MHz
 *   VCO output = VCO_in * PLLN = 2 * 216 = 432 MHz
 *   SYSCLK     = VCO_out / PLLP = 432 / 2 = 216 MHz
 *   PLL48CLK   = VCO_out / PLLQ = 432 / 9 =  48 MHz
 *
 * PLL48CLK must be exactly 48 MHz for reliable USB enumeration.
 *
 * @since Version 1.0.0
 */
typedef enum {
    k_pll_m_hse = 4U, /**< HSE prescaler: VCO input = 8 MHz / 4 = 2 MHz    */
    k_pll_q     = 9U, /**< USB prescaler: PLL48CLK = 432 MHz / 9 = 48 MHz  */
} pll_hse_dividers_t;

/**
 * @enum pll_hsi_dividers_t
 * @brief PLL input divider for 216 MHz SYSCLK from 16 MHz HSI.
 *
 * @details
 * HSI clock path:
 *   VCO input  = HSI / PLLM = 16 MHz / 8 = 2 MHz  (same 2 MHz VCO entry)
 *   VCO output = 2 * PLLN  = same as HSE path
 *
 * Used when STM32_USE_HSE is not defined (development fallback).
 *
 * @since Version 1.0.0
 */
typedef enum {
    k_pll_m_hsi = 8U, /**< HSI prescaler: VCO input = 16 MHz / 8 = 2 MHz */
} pll_hsi_dividers_t;

/**
 * @enum pll_multiplier_t
 * @brief PLL VCO multiplication factor (PLLN).
 *
 * @details
 * VCO output = VCO_input * PLLN = 2 MHz * 216 = 432 MHz.
 * 432 MHz satisfies PLLP=2 -> 216 MHz SYSCLK and PLLQ=9 -> 48 MHz USB
 * simultaneously.
 *
 * @since Version 1.0.0
 */
typedef enum {
    k_pll_n = 216U, /**< VCO multiplier: 2 MHz * 216 = 432 MHz VCO output */
} pll_multiplier_t;

/**
 * @enum clock_freq_t
 * @brief Target system clock frequencies after PLL configuration.
 *
 * @since Version 1.0.0
 */
typedef enum {
    k_sysclk_hz = 216000000UL, /**< SYSCLK / HCLK after PLL lock (216 MHz) */
} clock_freq_t;

/* ---------------------------------------------------------------------------
 * CMSIS required functions
 * ---------------------------------------------------------------------------*/

/**
 * @brief CMSIS system clock update -- no-op; HAL maintains SystemCoreClock.
 *
 * @details
 * HAL_RCC_ClockConfig() updates SystemCoreClock automatically by reading the
 * configured PLL divisors via HAL_RCC_GetSysClockFreq(). This function is
 * required by the CMSIS ABI but has no work to do in this project.
 *
 * @pre stm32_system_clock_init() has been called
 * @post SystemCoreClock unchanged (already correct)
 *
 * @since Version 1.0.0
 */
void SystemCoreClockUpdate(void)
{
    /* HAL_RCC_ClockConfig() keeps SystemCoreClock current; nothing to do. */
}

/* ---------------------------------------------------------------------------
 * SystemInit -- startup assembly hook (runs before main)
 * ---------------------------------------------------------------------------*/

/**
 * @brief Enable Cortex-M7 FPU before C runtime initializes globals.
 *
 * @details
 * Called by startup assembly (Reset_Handler) before the C runtime copies
 * .data and zeros .bss. Must complete without reading any initialized global
 * variables and without calling any function that does so.
 *
 * Only the FPU enable is performed here. Clock configuration is intentionally
 * deferred to stm32_system_clock_init() because HAL_RCC_OscConfig() and
 * HAL_PWREx_EnableOverDrive() use HAL_GetTick() for timeout accounting, which
 * requires SysTick to be running. SysTick is started by HAL_Init() in main(),
 * which executes after the C runtime and after this function.
 *
 * @pre Called by startup assembly before any C code runs
 * @post CP10 and CP11 coprocessors enabled for full FPU access
 * @post DSB and ISB barriers ensure the FPU enable is visible before return
 *
 * @note Clock configuration happens in main() via stm32_system_clock_init().
 * @since Version 1.0.0
 */
void SystemInit(void)
{
    volatile uint32_t* cpacr = (volatile uint32_t*)(uintptr_t)k_cpacr_addr;
    *cpacr |= (uint32_t)k_cp10_cp11;
    __asm volatile("dsb");
    __asm volatile("isb");
}

/* ---------------------------------------------------------------------------
 * stm32_system_clock_init -- PLL configuration called from main()
 * ---------------------------------------------------------------------------*/

/**
 * @brief Configure system clocks: 216 MHz SYSCLK, 48 MHz USB PLL.
 *
 * @details
 * Configures the STM32F7 PLL using the STM32 HAL RCC API. Call from main()
 * after HAL_Init() and before stm32_usb_cdc_init().
 *
 * Initialization sequence:
 * 1. Enable PWR peripheral clock and set voltage scaling to Scale 1
 * 2. Configure PLL source (HSE or HSI) and dividers via HAL_RCC_OscConfig()
 * 3. Enable Over-Drive mode (mandatory for SYSCLK > 180 MHz on STM32F7)
 * 4. Switch SYSCLK to PLL, configure AHB/APB prescalers and flash latency
 *    via HAL_RCC_ClockConfig() (also updates SystemCoreClock automatically)
 * 5. Set SystemCoreClock = 216000000UL explicitly as belt-and-suspenders
 *
 * Compile-time clock source selection:
 *   - STM32_USE_HSE defined: 8 MHz HSE crystal (preferred; better USB accuracy)
 *   - STM32_USE_HSE not defined: 16 MHz HSI fallback (development only)
 *
 * On any HAL failure the CPU halts with interrupts disabled. There is no
 * graceful recovery path when the clock tree cannot be configured before
 * the RTOS scheduler starts.
 *
 * @pre HAL_Init() called before this function (configures SysTick for HAL timeouts)
 * @pre SystemInit() has already enabled the FPU
 * @post SYSCLK = 216 MHz, HCLK = 216 MHz
 * @post APB1 = 54 MHz, APB2 = 108 MHz
 * @post PLL48CLK = 48 MHz (USB clock source)
 * @post SystemCoreClock = 216000000UL
 * @post Flash latency = 7 wait states with ART accelerator enabled
 *
 * @note Must be called from main() before stm32_usb_cdc_init()
 * @note Not re-entrant; call once during startup only
 *
 * @since Version 1.0.0
 */
void stm32_system_clock_init(void)
{
    /* Step 1: Enable PWR peripheral clock; set voltage Scale 1 (required at 216 MHz) */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Step 2: Configure oscillator and PLL */
    RCC_OscInitTypeDef osc_init = {0};

#ifdef STM32_USE_HSE
    /* HSE path: 8 MHz crystal -> PLLM=4 -> VCO input = 2 MHz */
    osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc_init.HSEState       = RCC_HSE_ON;
    osc_init.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc_init.PLL.PLLM       = (uint32_t)k_pll_m_hse;
#else
    /* HSI fallback: 16 MHz internal -> PLLM=8 -> VCO input = 2 MHz */
    osc_init.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc_init.HSIState            = RCC_HSI_ON;
    osc_init.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc_init.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc_init.PLL.PLLM            = (uint32_t)k_pll_m_hsi;
#endif /* STM32_USE_HSE */

    osc_init.PLL.PLLState = RCC_PLL_ON;
    osc_init.PLL.PLLN     = (uint32_t)k_pll_n; /* VCO = 2 MHz * 216 = 432 MHz  */
    osc_init.PLL.PLLP     = RCC_PLLP_DIV2;     /* SYSCLK = 432 / 2 = 216 MHz   */
    osc_init.PLL.PLLQ     = (uint32_t)k_pll_q; /* USB = 432 / 9 = 48 MHz       */

    if (HAL_RCC_OscConfig(&osc_init) != HAL_OK) {
        /* Clock source unavailable; halt -- no recovery possible before scheduler */
        __asm volatile("cpsid i");
        for (;;) {
        }
    }

    /* Step 3: Enable Over-Drive mode -- mandatory for STM32F7 at 216 MHz */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        __asm volatile("cpsid i");
        for (;;) {
        }
    }

    /* Step 4: Switch SYSCLK to PLL; configure bus prescalers and flash latency */
    RCC_ClkInitTypeDef clk_init = {0};
    clk_init.ClockType =
        RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk_init.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk_init.AHBCLKDivider  = RCC_SYSCLK_DIV1; /* HCLK = 216 MHz              */
    clk_init.APB1CLKDivider = RCC_HCLK_DIV4;   /* APB1 = 54 MHz (max 54 MHz)  */
    clk_init.APB2CLKDivider = RCC_HCLK_DIV2;   /* APB2 = 108 MHz (max 108 MHz)*/

    if (HAL_RCC_ClockConfig(&clk_init, FLASH_LATENCY_7) != HAL_OK) {
        __asm volatile("cpsid i");
        for (;;) {
        }
    }

    /* Step 5: Explicitly confirm SystemCoreClock (HAL_RCC_ClockConfig also sets it) */
    SystemCoreClock = (uint32_t)k_sysclk_hz;
}
