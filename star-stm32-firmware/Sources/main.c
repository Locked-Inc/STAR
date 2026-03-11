/**
 * @file main.c
 * @brief STM32F7 firmware entry point.
 *
 * @details
 * System initialization sequence (Phase 1 -- USB CDC bring-up):
 * 1. FPU enable (via SystemInit in system_stm32f7xx.c -- runs before main())
 * 2. HAL_Init(): SysTick at 1 ms, NVIC priority grouping
 * 3. stm32_system_clock_init(): 216 MHz SYSCLK, 48 MHz USB PLL
 * 4. CRC self-test -- assert on failure [Phase 2]
 * 5. Shared data init [Phase 4]
 * 6. USB CDC init (TinyUSB + StreamBuffer) via stm32_usb_cdc_init()
 * 7. Create FreeRTOS tasks: vUsbDeviceTask, vCommTask [Phase 4],
 *    vMotorStubTask, vTelemetryTask [Phase 5]
 * 8. vTaskStartScheduler()
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "FreeRTOS.h"
#include "stm32_usb_cdc.h"
#include "stm32f7xx_hal.h"
#include "task.h"
#include "tusb.h"

#include <stdint.h>

extern void stm32_system_clock_init(void);

/* TODO (Phase 4): #include "tasks/inc/comm_task.h"       */
/* TODO (Phase 5): #include "tasks/inc/motor_stub_task.h" */
/* TODO (Phase 5): #include "tasks/inc/telemetry_task.h"  */

/* ---------------------------------------------------------------------------
 * FreeRTOS application hooks (required by FreeRTOSConfig.h settings)
 * ---------------------------------------------------------------------------*/

/**
 * @brief FreeRTOS malloc failure hook -- called when heap_4 is exhausted.
 *
 * @details
 * Required when configUSE_MALLOC_FAILED_HOOK = 1 in FreeRTOSConfig.h.
 * Called from xTaskCreate or xStreamBufferCreate if the heap is too small.
 * Halts with interrupts disabled; connect a debugger to diagnose.
 * Increase configTOTAL_HEAP_SIZE in FreeRTOSConfig.h to resolve.
 *
 * @pre configUSE_MALLOC_FAILED_HOOK = 1 in FreeRTOSConfig.h
 * @post CPU halted; never returns
 *
 * @since Version 1.0.0
 */
void vApplicationMallocFailedHook(void)
{
    __asm volatile("cpsid i"); /* Disable interrupts */
    for (;;) {
    }
}

/**
 * @brief FreeRTOS stack overflow hook -- called when a task stack overflows.
 *
 * @details
 * Required when configCHECK_FOR_STACK_OVERFLOW = 2 in FreeRTOSConfig.h.
 * Called by the FreeRTOS scheduler when it detects that a task has written
 * past the end of its stack. Halts with interrupts disabled.
 * Increase the offending task's stack size to resolve.
 *
 * @param[in] xTask     Handle of the task that overflowed (unused in halt)
 * @param[in] pcTaskName Name of the task that overflowed (unused in halt)
 *
 * @pre configCHECK_FOR_STACK_OVERFLOW = 2 in FreeRTOSConfig.h
 * @post CPU halted; never returns
 *
 * @since Version 1.0.0
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    __asm volatile("cpsid i"); /* Disable interrupts */
    for (;;) {
    }
}

/* ---------------------------------------------------------------------------
 * Private: USB device task stack configuration
 * ---------------------------------------------------------------------------*/

/**
 * @enum usb_task_config_t
 * @brief FreeRTOS task parameters for the USB device task.
 *
 * @details
 * Stack is 512 words (2 KB). TinyUSB event processing is lightweight but
 * the CDC callbacks use a 64-byte local buffer, so 512 words provides ample
 * headroom. Priority 6 places it above vCommTask (5) so USB events are
 * serviced promptly.
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
    k_usb_task_stack_words = 512U, /**< USB task stack depth in 32-bit words (2 KB) */
    k_usb_task_priority    = 6U,   /**< FreeRTOS task priority (above vCommTask) */
} usb_task_config_t;

/* ---------------------------------------------------------------------------
 * Private: FreeRTOS USB device task
 * ---------------------------------------------------------------------------*/

/**
 * @brief FreeRTOS task: runs TinyUSB device stack continuously.
 *
 * @details
 * Calls tud_task() in a tight loop. TinyUSB internally dispatches USB
 * events (enumeration, transfer complete, disconnect) and invokes registered
 * callbacks such as tud_cdc_rx_cb(). The CDC RX callback drains received
 * bytes into the StreamBuffer read by stm32_usb_cdc_receive().
 *
 * Priority: k_usb_task_priority (6) -- above vCommTask so USB events are
 * handled before frame parsing begins.
 *
 * @param[in] pvParameters Unused (NULL passed from xTaskCreate)
 *
 * @pre stm32_usb_cdc_init() called before vTaskStartScheduler()
 * @post tud_task() runs indefinitely; task never returns
 *
 * @note This task never blocks for long -- tud_task() yields via FreeRTOS
 *       OS abstraction when no USB events are pending
 * @since Version 1.0.0
 */
static void internal_usb_device_task(void* pvParameters)
{
    (void)pvParameters;
    for (;;) {
        tud_task();
    }
}

/* ---------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------------*/

/**
 * @brief Firmware entry point: initialize hardware and start FreeRTOS scheduler.
 *
 * @details
 * Follows the initialization sequence documented at the top of this file.
 * After vTaskStartScheduler() this function never returns.
 *
 * @return int Never returns (vTaskStartScheduler() does not return normally)
 *
 * @pre SystemInit() already ran (called by startup assembly before main())
 * @post FreeRTOS scheduler running; tasks executing concurrently
 *
 * @since Version 1.0.0
 */
int main(void)
{
    /* Step 1: FPU enabled in SystemInit() (system_stm32f7xx.c) before main() */

    /* Step 2: HAL_Init -- configure SysTick at 1 ms and NVIC priority grouping.
     * Must precede stm32_system_clock_init() because HAL RCC timeout loops
     * require a running HAL tick. */
    (void)HAL_Init();

    /* Step 3: Clock init -- 216 MHz SYSCLK, 48 MHz USB PLL.
     * Halts on failure (no recovery before scheduler). */
    stm32_system_clock_init();

    /* Step 4: CRC self-test (Phase 2)
     * TODO: stm32_crc32_selftest() -- assert on failure
     */

    /* Step 5: Shared data init (Phase 4)
     * TODO: stm32_shared_data_init()
     */

    /* Step 6: USB CDC init (creates StreamBuffer, enables OTG_HS IRQ, calls tusb_init) */
    configASSERT(stm32_usb_cdc_init() == k_stm32_err_ok);

    /* Step 7: Create FreeRTOS tasks */
    (void)xTaskCreate(internal_usb_device_task,
                      "usb",
                      k_usb_task_stack_words,
                      NULL,
                      k_usb_task_priority,
                      NULL);

    /* TODO (Phase 4): xTaskCreate(vCommTask, ...) */
    /* TODO (Phase 5): xTaskCreate(vMotorStubTask, ...) */
    /* TODO (Phase 5): xTaskCreate(vTelemetryTask, ...) */

    /* Step 7: Start FreeRTOS scheduler -- does not return */
    vTaskStartScheduler();

    /* Should never reach here; indicates scheduler heap exhaustion */
    for (;;) {
    }
}
