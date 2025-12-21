/**
 * @file main.c
 * @brief STAR RX72N Firmware - ThreadX LED Blink Example
 *
 * Simple ThreadX application demonstrating:
 * - ThreadX kernel initialization
 * - Task creation
 * - Task delays
 * - GPIO toggling (LED blink)
 *
 * Hardware: Renesas RX72N (R5F572NNHGFP#30)
 * RTOS: ThreadX (Azure RTOS / Eclipse ThreadX)
 */

#include <stdint.h>
#include <stdbool.h>
#include "tx_api.h"  /* ThreadX API */

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/* LED connected to GPIO (placeholder - update based on actual hardware) */
#define LED_PORT_BASE   ((volatile uint8_t *)0x0008C000)  /* Example port address */
#define LED_PIN         (1 << 0)  /* Bit 0 */

/* ThreadX task stack sizes */
#define LED_TASK_STACK_SIZE     1024
#define DEMO_TASK_STACK_SIZE    1024

/* Task priorities (0 = highest, 31 = lowest) */
#define LED_TASK_PRIORITY       5
#define DEMO_TASK_PRIORITY      10

/* =============================================================================
 * ThreadX Objects
 * =============================================================================
 */

/* Task control blocks */
TX_THREAD led_thread;
TX_THREAD demo_thread;

/* Task stacks */
static uint8_t led_task_stack[LED_TASK_STACK_SIZE];
static uint8_t demo_task_stack[DEMO_TASK_STACK_SIZE];

/* =============================================================================
 * Hardware Abstraction
 * =============================================================================
 */

/**
 * @brief Initialize LED GPIO
 */
static void led_init(void)
{
    /* TODO: Configure GPIO pin as output */
    /* This is a placeholder - actual implementation depends on RX72N GPIO registers */

    /* For now, just a stub */
    (void)LED_PORT_BASE;
}

/**
 * @brief Toggle LED state
 */
static void led_toggle(void)
{
    /* TODO: Toggle GPIO pin */
    /* Placeholder for actual GPIO toggle */
    static uint8_t state = 0;
    state = !state;

    /* Would actually write to GPIO register */
    /* *LED_PORT_BASE ^= LED_PIN; */
}

/* =============================================================================
 * ThreadX Tasks
 * =============================================================================
 */

/**
 * @brief LED blink task (500ms interval)
 *
 * Demonstrates:
 * - ThreadX task loop
 * - tx_thread_sleep() for delays
 * - GPIO control
 */
static void led_task_entry(ULONG input)
{
    (void)input;

    /* Initialize LED */
    led_init();

    /* Task loop */
    while (1)
    {
        /* Toggle LED */
        led_toggle();

        /* Sleep for 500ms (assumes 100 ticks/sec timer) */
        tx_thread_sleep(50);
    }
}

/**
 * @brief Demo task (prints to console via semihosting or UART)
 *
 * Demonstrates a second concurrent task running at lower priority.
 */
static void demo_task_entry(ULONG input)
{
    (void)input;

    ULONG counter = 0;

    while (1)
    {
        /* Increment counter */
        counter++;

        /* In a real application, this would print via UART:
         * printf("ThreadX running, count = %lu\n", counter);
         */

        /* Sleep for 1 second */
        tx_thread_sleep(100);
    }
}

/* =============================================================================
 * ThreadX Application Definition
 * =============================================================================
 */

/**
 * @brief ThreadX application definition
 *
 * Called by tx_kernel_enter() to create all application objects.
 * This is where we create tasks, semaphores, queues, etc.
 */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;

    /* Create LED blink task */
    tx_thread_create(
        &led_thread,                /* Thread control block */
        "LED Task",                 /* Thread name */
        led_task_entry,             /* Entry function */
        0,                          /* Entry input (unused) */
        led_task_stack,             /* Stack pointer */
        LED_TASK_STACK_SIZE,        /* Stack size */
        LED_TASK_PRIORITY,          /* Priority */
        LED_TASK_PRIORITY,          /* Preemption threshold (same as priority) */
        TX_NO_TIME_SLICE,           /* No time slicing */
        TX_AUTO_START               /* Start immediately */
    );

    /* Create demo task */
    tx_thread_create(
        &demo_thread,
        "Demo Task",
        demo_task_entry,
        0,
        demo_task_stack,
        DEMO_TASK_STACK_SIZE,
        DEMO_TASK_PRIORITY,
        DEMO_TASK_PRIORITY,
        TX_NO_TIME_SLICE,
        TX_AUTO_START
    );
}

/* =============================================================================
 * Main Entry Point
 * =============================================================================
 */

/**
 * @brief Application entry point
 *
 * Called by startup code after hardware initialization.
 * Starts the ThreadX kernel - this function never returns.
 */
int main(void)
{
    /* Note: Hardware initialization (clocks, interrupts) would go here */
    /* For now, we rely on default reset state */

    /* Enter ThreadX kernel - this never returns */
    tx_kernel_enter();

    /* Should never reach here */
    while (1)
    {
        __asm__ volatile ("wait");
    }

    return 0;
}
