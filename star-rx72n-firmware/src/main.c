/* src/main.c */

/**
 * @file main.c
 * @brief STAR RX72N Firmware - ThreadX LED Blink with Full Infrastructure
 *
 * Demonstrates:
 * - ThreadX kernel initialization
 * - Full error handling and logging infrastructure
 * - Pin validation and reservation
 * - Error checking with RX_ERROR_CHECK macros
 * - GPIO toggling (LED blink)
 *
 * Hardware: Renesas RX72N (R5F572NNHGFP#30)
 * RTOS: ThreadX (Azure RTOS / Eclipse ThreadX)
 */

#include <stdbool.h>
#include <stdint.h>

#include "hardware.h" /* Hardware abstraction layer */
#include "tx_api.h"   /* ThreadX API */

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/* LED connected to PORT0, Pin 0 (update based on actual hardware) */
#define LED_PORT 0 /* PORT0 */
#define LED_PIN  0 /* Pin 0 */

/* ThreadX task stack sizes */
#define LED_TASK_STACK_SIZE  1024
#define DEMO_TASK_STACK_SIZE 1024

/* Task priorities (0 = highest, 31 = lowest) */
#define LED_TASK_PRIORITY  5
#define DEMO_TASK_PRIORITY 10

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
 *
 * @return RX_OK on success, error code on failure
 */
static rx_err_t led_init(void)
{
  rx_err_t err;

  /* Configure LED pin as output */
  err = gpio_set_output(LED_PORT, LED_PIN);
  RX_RETURN_ON_ERROR(err, "LED", "Failed to configure LED pin as output");

  /* Start with LED off */
  err = gpio_write_low(LED_PORT, LED_PIN);
  RX_RETURN_ON_ERROR(err, "LED", "Failed to set LED initial state");

  RX_LOG_INFO("LED", "LED initialized successfully");

  return RX_OK;
}

/**
 * @brief Toggle LED state
 *
 * @return RX_OK on success, error code on failure
 */
static rx_err_t led_toggle(void)
{
  /* Toggle LED GPIO pin */
  rx_err_t err = gpio_toggle(LED_PORT, LED_PIN);
  RX_RETURN_ON_ERROR(err, "LED", "Failed to toggle LED");

  return RX_OK;
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
 * - GPIO control with error handling
 */
static void led_task_entry(ULONG input)
{
  (void)input;

  /* Initialize LED */
  rx_err_t err = led_init();
  if (err != RX_OK) {
    RX_LOG_ERROR("LED_TASK", "LED initialization failed");
    /* Continue anyway - non-critical */
  }

  /* Task loop */
  while (1) {
    /* Toggle LED */
    err = led_toggle();
    if (err != RX_OK) {
      RX_LOG_ERROR("LED_TASK", "LED toggle failed");
    }

    /* Sleep for 500ms (assumes 100 ticks/sec timer) */
    tx_thread_sleep(50);
  }
}

/**
 * @brief Demo task (prints debug info via UART)
 *
 * Demonstrates a second concurrent task running at lower priority.
 * Prints a counter every second to show the system is alive.
 */
static void demo_task_entry(ULONG input)
{
  (void)input;

  ULONG counter = 0;

  RX_LOG_INFO("DEMO_TASK", "Demo task started");

  while (1) {
    /* Increment counter */
    counter++;

    /* Print debug message via UART */
    uart_puts("[Demo Task] Count: ");
    uart_putint((int32_t)counter);
    uart_puts("\r\n");

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
void tx_application_define(void* first_unused_memory)
{
  (void)first_unused_memory;

  RX_LOG_INFO("THREADX", "Creating application tasks");

  /* Create LED blink task */
  UINT status = tx_thread_create(&led_thread,         /* Thread control block */
                                  "LED Task",          /* Thread name */
                                  led_task_entry,      /* Entry function */
                                  0,                   /* Entry input (unused) */
                                  led_task_stack,      /* Stack pointer */
                                  LED_TASK_STACK_SIZE, /* Stack size */
                                  LED_TASK_PRIORITY,   /* Priority */
                                  LED_TASK_PRIORITY, /* Preemption threshold (same as priority) */
                                  TX_NO_TIME_SLICE,  /* No time slicing */
                                  TX_AUTO_START      /* Start immediately */
  );

  if (status != TX_SUCCESS) {
    RX_LOG_ERROR("THREADX", "Failed to create LED task");
  }

  /* Create demo task */
  status = tx_thread_create(&demo_thread,
                            "Demo Task",
                            demo_task_entry,
                            0,
                            demo_task_stack,
                            DEMO_TASK_STACK_SIZE,
                            DEMO_TASK_PRIORITY,
                            DEMO_TASK_PRIORITY,
                            TX_NO_TIME_SLICE,
                            TX_AUTO_START);

  if (status != TX_SUCCESS) {
    RX_LOG_ERROR("THREADX", "Failed to create demo task");
  }

  RX_LOG_INFO("THREADX", "Application tasks created successfully");
}

/* =============================================================================
 * Main Entry Point
 * =============================================================================
 */

/**
 * @brief Application entry point
 *
 * Called by startup code after hardware initialization.
 * Initializes all infrastructure and starts the ThreadX kernel.
 * This function never returns.
 */
int main(void)
{
  rx_err_t err;

  /* Initialize RX72N hardware (clocks and peripherals) */
  err = system_init();
  RX_ERROR_CHECK(err); /* Fatal: can't continue without clocks */

  /* Initialize UART first for logging */
  err = uart_init();
  RX_ERROR_CHECK(err); /* Fatal: can't continue without logging */

  /* Now we can start logging */
  uart_puts("\r\n===========================================\r\n");
  uart_puts("STAR RX72N Firmware v1.0.0\r\n");
  uart_puts("with Full Infrastructure Integration\r\n");
  uart_puts("===========================================\r\n\r\n");

  RX_LOG_INFO("MAIN", "System initialization complete");

  /* Initialize global error handling and pin validation */
  err = rx_infrastructure_init();
  RX_ERROR_CHECK(err); /* Fatal: infrastructure is critical */

  /* Initialize system tick timer */
  err = timer_init();
  RX_ERROR_CHECK(err); /* Fatal: ThreadX needs system tick */

  /* Send ThreadX startup message */
  RX_LOG_INFO("MAIN", "Starting ThreadX RTOS");
  uart_puts("\r\n");

  /* Enter ThreadX kernel - this never returns */
  tx_kernel_enter();

  /* Should never reach here */
  RX_LOG_ERROR("MAIN", "ThreadX kernel exited unexpectedly");
  while (1) {
    __asm__ volatile("wait");
  }

  return 0;
}
