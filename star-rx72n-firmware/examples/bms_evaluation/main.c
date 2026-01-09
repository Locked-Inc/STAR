/**
 * @file main.c
 * @brief BMS Evaluation Tool - RX72N Firmware Example
 *
 * USB-to-SMBus bridge for BQ78350 battery management system.
 * Communicates with desktop tool (Wails) via Protocol Buffers over USB CDC.
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "tx_api.h"
#include "rx_bq78350.h"
#include "rx_bus_manager.h"
#include "rx_usb_comm.h"
#include "rx_err.h"

/* TODO: Include generated nanopb headers when available */
/* #include "star/v1/bms.pb.h" */
/* #include "pb_encode.h" */
/* #include "pb_decode.h" */

/* =============================================================================
 * Constants
 * =============================================================================
 */

typedef enum {
  k_num_cells          = 16,   /**< Number of battery cells (4-16) */
  k_bms_task_priority  = 5,    /**< BMS task priority */
  k_usb_task_priority  = 5,    /**< USB task priority */
  k_task_stack_size    = 2048, /**< Task stack size in bytes */
  k_telemetry_period_ms = 1000, /**< Telemetry update period */
} app_constants_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

/* ThreadX tasks */
static TX_THREAD s_bms_task;
static TX_THREAD s_usb_task;

/* Task stacks */
static uint8_t s_bms_task_stack[k_task_stack_size];
static uint8_t s_usb_task_stack[k_task_stack_size];

/* Bus manager instance */
static rx_bus_manager_t s_bus_manager;

/* USB communication handle */
static rx_usb_comm_handle_t s_usb_comm_handle;

/* =============================================================================
 * BMS Task - Command Processing
 * =============================================================================
 */

/**
 * @brief BMS task entry point
 *
 * Processes incoming BMS commands from USB and sends responses.
 * Commands are received as Protocol Buffer messages (BmsCommandRequest)
 * and responses are sent back as BmsCommandResponse.
 *
 * @param[in] input Unused parameter
 */
static void bms_task_entry(ULONG input)
{
  (void)input;

  /* TODO: Initialize bus manager */
  /* TODO: Initialize BQ78350 driver */

  while (1) {
    /* TODO: Receive frame from USB */
    /* TODO: Decode BmsCommandRequest protobuf */
    /* TODO: Execute command (read register, read telemetry, etc.) */
    /* TODO: Encode BmsCommandResponse protobuf */
    /* TODO: Send response frame via USB */

    tx_thread_sleep(10); /* 100ms */
  }
}

/* =============================================================================
 * USB Task - Communication Management
 * =============================================================================
 */

/**
 * @brief USB task entry point
 *
 * Manages USB CDC communication and frame handling.
 *
 * @param[in] input Unused parameter
 */
static void usb_task_entry(ULONG input)
{
  (void)input;

  /* TODO: Initialize USB CDC */
  /* TODO: Initialize rx_usb_comm framing layer */

  /* Wait for USB enumeration */
  while (1) {
    /* TODO: Check if USB is configured */
    tx_thread_sleep(100); /* 1 second */
    break; /* Placeholder - remove when implementing */
  }

  while (1) {
    /* TODO: Poll for incoming frames */
    /* TODO: Forward to BMS task for processing */

    tx_thread_sleep(10); /* 100ms */
  }
}

/* =============================================================================
 * ThreadX Application Entry
 * =============================================================================
 */

/**
 * @brief ThreadX application definition
 *
 * Called by ThreadX kernel to create application tasks.
 *
 * @param[in] first_unused_memory Pointer to first unused memory (unused)
 */
void tx_application_define(void* first_unused_memory)
{
  (void)first_unused_memory;

  /* Create BMS task */
  tx_thread_create(&s_bms_task,
                   "BMS",
                   bms_task_entry,
                   0,
                   s_bms_task_stack,
                   k_task_stack_size,
                   k_bms_task_priority,
                   k_bms_task_priority,
                   TX_NO_TIME_SLICE,
                   TX_AUTO_START);

  /* Create USB task */
  tx_thread_create(&s_usb_task,
                   "USB",
                   usb_task_entry,
                   0,
                   s_usb_task_stack,
                   k_task_stack_size,
                   k_usb_task_priority,
                   k_usb_task_priority,
                   TX_NO_TIME_SLICE,
                   TX_AUTO_START);
}

/* =============================================================================
 * Main Entry Point
 * =============================================================================
 */

/**
 * @brief Main entry point
 *
 * Initializes hardware and starts ThreadX kernel.
 *
 * @return Never returns
 */
int main(void)
{
  /* TODO: Initialize system (clocks, power) */
  /* TODO: Initialize hardware (GPIO, timers, UART) */

  /* Start ThreadX kernel (never returns) */
  tx_kernel_enter();

  /* Should never reach here */
  while (1) {
    __asm__ volatile("wait");
  }

  return 0;
}
