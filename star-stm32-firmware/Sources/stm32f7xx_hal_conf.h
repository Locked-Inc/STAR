/**
 * @file stm32f7xx_hal_conf.h
 * @brief STM32F7 HAL driver configuration for STAR firmware.
 *
 * @details
 * Selects the minimal set of HAL modules needed for Phase 1 USB CDC bring-up.
 * Only modules with corresponding .c files listed in CMakeLists.txt are enabled
 * here. Enabling a module without its source file causes link errors.
 *
 * Enabled modules:
 *   HAL_MODULE_ENABLED   -- HAL base (HAL_Init, HAL_GetTick)
 *   HAL_CORTEX_MODULE    -- NVIC helpers (HAL_NVIC_SetPriority etc.)
 *   HAL_RCC_MODULE       -- Clock enable macros (__HAL_RCC_USB_OTG_HS_CLK_ENABLE)
 *   HAL_DMA_MODULE       -- DMA (required by RCC internally)
 *   HAL_GPIO_MODULE      -- GPIO (required by HAL base)
 *   HAL_PWR_MODULE       -- Power (required by RCC for clock scaling)
 *
 * @note Derived from libs/stm32f7xx_hal_driver/Inc/stm32f7xx_hal_conf_template.h
 *       (ST Microelectronics, MIT license).
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * HAL module selection -- only enable what we compile
 * ---------------------------------------------------------------------------*/

#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
/* FLASH header is header-only (macros only); needed by stm32f7xx_hal.c for
 * __HAL_FLASH_ART_ENABLE and __HAL_FLASH_PREFETCH_BUFFER_ENABLE.
 * We do NOT compile stm32f7xx_hal_flash.c (no flash write/erase needed). */
#define HAL_FLASH_MODULE_ENABLED

/* ---------------------------------------------------------------------------
 * Oscillator values
 * Nucleo-F767ZI: 8 MHz HSE crystal, 16 MHz HSI (internal)
 * ---------------------------------------------------------------------------*/

/** @brief External High Speed oscillator frequency in Hz (8 MHz on Nucleo-F767ZI). */
#if !defined(HSE_VALUE)
#define HSE_VALUE       8000000U
#endif

/** @brief HSE startup timeout in ms. */
#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT  100U
#endif

/** @brief Internal High Speed oscillator frequency in Hz. */
#if !defined(HSI_VALUE)
#define HSI_VALUE       16000000U
#endif

/** @brief Internal Low Speed oscillator typical frequency in Hz. */
#if !defined(LSI_VALUE)
#define LSI_VALUE       32000U
#endif

/** @brief External Low Speed oscillator frequency in Hz. */
#if !defined(LSE_VALUE)
#define LSE_VALUE       32768U
#endif

/** @brief LSE startup timeout in ms. */
#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT  5000U
#endif

/** @brief External clock for I2S (used by stm32f7xx_hal_rcc_ex.c even without I2S enabled). */
#if !defined(EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE    12288000U
#endif

/* ---------------------------------------------------------------------------
 * System configuration
 * ---------------------------------------------------------------------------*/

/** @brief VDD supply voltage in mV. */
#define VDD_VALUE                   3300U

/**
 * @brief HAL SysTick interrupt priority.
 *
 * @details
 * Must be the lowest priority (15) so FreeRTOS SysTick preempts it at
 * configKERNEL_INTERRUPT_PRIORITY. In a FreeRTOS system, SysTick is
 * managed by the FreeRTOS port; HAL_IncTick() is called from
 * vApplicationTickHook() or the FreeRTOS SysTick handler.
 */
#define TICK_INT_PRIORITY           15U

/** @brief Set to 0; FreeRTOS provides task scheduling. */
#define USE_RTOS                    0U

/** @brief Enable Flash prefetch buffer for 216 MHz operation. */
#define PREFETCH_ENABLE             1U

/** @brief Enable ART Accelerator (instruction cache + prefetch). */
#define ART_ACCELERATOR_ENABLE      1U

/**
 * @brief Disable HAL assertion checking (no assert handler linked in).
 *
 * @details
 * HAL sources call assert_param() to validate arguments. Without defining
 * this macro it remains an implicit function, causing -Werror failures.
 * Expanding to (void)(expr) evaluates the expression (no side effects lost)
 * and satisfies the compiler without linking an assertion handler.
 */
#define assert_param(expr)          ((void)(expr))

/* All register callback variants disabled -- not needed for Phase 1 */
#define USE_HAL_RCC_REGISTER_CALLBACKS    0U
#define USE_HAL_GPIO_REGISTER_CALLBACKS   0U
#define USE_HAL_DMA_REGISTER_CALLBACKS    0U
#define USE_HAL_PWR_REGISTER_CALLBACKS    0U

/* ---------------------------------------------------------------------------
 * HAL module headers
 * Each header includes stm32f7xx_hal_def.h which defines HAL_StatusTypeDef.
 * Only include headers for modules with corresponding .c files in CMakeLists.txt.
 * ---------------------------------------------------------------------------*/

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32f7xx_hal_rcc.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32f7xx_hal_gpio.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f7xx_hal_dma.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32f7xx_hal_cortex.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32f7xx_hal_pwr.h"
#include "stm32f7xx_hal_pwr_ex.h"
#endif

#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f7xx_hal_flash.h"
#endif

#ifdef __cplusplus
}
#endif
