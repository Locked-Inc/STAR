/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS kernel configuration for STM32F7 (Cortex-M7).
 *
 * @details
 * Application-specific FreeRTOS configuration. Included by the FreeRTOS
 * kernel headers; do not include directly from application code.
 *
 * **Priority notes:**
 * configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5 on a 4-bit NVIC priority
 * system. Any ISR that calls FreeRTOS API (including TinyUSB DMA callbacks)
 * must have a numerical NVIC priority >= 5. Lower numbers = higher priority
 * on ARM Cortex-M, so ISRs at priority 0-4 must NEVER call FreeRTOS API.
 *
 * The OTG_HS IRQ is set to exactly this value in stm32_usb_cdc_init().
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

/* ---------------------------------------------------------------------------
 * Processor and clock
 * ---------------------------------------------------------------------------*/

/** @brief Cortex-M7: use CMSIS port. */
#define configCPU_CLOCK_HZ 216000000UL

/** @brief Tick rate in Hz. 1000 Hz = 1 ms tick. */
#define configTICK_RATE_HZ 1000U

/**
 * @brief Tick type width for 32-bit Cortex-M7.
 *
 * @details
 * FreeRTOS >= 10.5.0 requires either configUSE_16_BIT_TICKS or
 * configTICK_TYPE_WIDTH_IN_BITS. Use 32-bit ticks on Cortex-M7 to
 * avoid TickType_t overflow at ~49 days (65 s with 16-bit at 1000 Hz).
 * TICK_TYPE_WIDTH_32_BITS is defined in FreeRTOS.h.
 */
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS

/* ---------------------------------------------------------------------------
 * Memory and heap
 * ---------------------------------------------------------------------------*/

/** @brief Use heap_4 (best-fit, coalescence). No malloc in application code. */
#define configTOTAL_HEAP_SIZE (64U * 1024U)

/** @brief Minimum stack size in words (not bytes). */
#define configMINIMAL_STACK_SIZE 128U

/* ---------------------------------------------------------------------------
 * Task priorities (lower number = lower priority on FreeRTOS)
 * ---------------------------------------------------------------------------*/

/** @brief Maximum number of task priorities available. */
#define configMAX_PRIORITIES 10U

/* Application task priority assignments (informational -- defined in task headers):
 *   vUsbDeviceTask  : 6  (USB processing, above comm)
 *   vCommTask       : 5  (100 Hz frame dispatch)
 *   vMotorStubTask  : 8  (motor control, highest application priority)
 *   vTelemetryTask  : 3  (10 Hz telemetry, lowest application priority)
 *   Idle task       : 0  (FreeRTOS internal)
 */

/* ---------------------------------------------------------------------------
 * Interrupt priority configuration (CRITICAL for TinyUSB + FreeRTOS)
 * ---------------------------------------------------------------------------*/

/** @brief Number of priority bits implemented in NVIC (STM32F7 = 4 bits). */
#define configPRIO_BITS 4U

/**
 * @brief Lowest interrupt priority (numerically highest on ARM Cortex-M).
 *
 * @details
 * On a 4-bit NVIC system the lowest priority is (2^4 - 1) = 15.
 * FreeRTOS uses this for the SysTick and PendSV handlers.
 */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15U

/**
 * @brief Highest interrupt priority from which FreeRTOS API may be called.
 *
 * @details
 * ISRs at priorities 0 to (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY - 1)
 * must NEVER call any FreeRTOS API. The OTG_HS IRQ is set to exactly this
 * value. Any TinyUSB callback that uses FreeRTOS primitives (StreamBuffer,
 * semaphore, etc.) must also be at or below this priority.
 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U

/** @brief Kernel-internal form of the syscall priority (left-shifted). */
#define configKERNEL_INTERRUPT_PRIORITY                                                            \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

/** @brief Kernel-internal form of the max syscall priority (left-shifted). */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY                                                       \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

/* ---------------------------------------------------------------------------
 * Kernel features
 * ---------------------------------------------------------------------------*/

#define configUSE_PREEMPTION              1
#define configUSE_IDLE_HOOK               0
#define configUSE_TICK_HOOK               0
#define configUSE_MUTEXES                 1
#define configUSE_RECURSIVE_MUTEXES       0
#define configUSE_COUNTING_SEMAPHORES     0
#define configUSE_STREAM_BUFFERS          1
#define configUSE_TIMERS                  0
#define configCHECK_FOR_STACK_OVERFLOW    2
#define configUSE_MALLOC_FAILED_HOOK      1
#define INCLUDE_vTaskDelay                1
#define INCLUDE_xTaskGetCurrentTaskHandle 1

/* ---------------------------------------------------------------------------
 * Assert (fires on FreeRTOS internal errors -- check NVIC priority violations)
 * ---------------------------------------------------------------------------*/

/**
 * @brief FreeRTOS assert: disable interrupts and halt on failure.
 *
 * @details
 * Fires on FreeRTOS internal assertion failures (e.g. NVIC priority violations,
 * heap exhaustion detected internally). Also used by main() to assert that
 * stm32_usb_cdc_init() succeeds.
 *
 * Halting with interrupts disabled is the safest option before the RTOS
 * scheduler and watchdog are running. Attach a debugger and inspect the
 * call stack to find the assertion site.
 */
#define configASSERT(x)                                                                            \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            __asm volatile("cpsid i");                                                             \
            for (;;) {                                                                             \
            }                                                                                      \
        }                                                                                          \
    } while (0)
