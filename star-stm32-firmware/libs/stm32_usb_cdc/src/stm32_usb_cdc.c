/**
 * @file stm32_usb_cdc.c
 * @brief USB CDC ACM transport layer implementation (TinyUSB + FreeRTOS).
 *
 * @details
 * Implements the USB CDC transport for STM32F7 using TinyUSB as the USB stack
 * and FreeRTOS StreamBuffer for decoupled RX delivery.
 *
 * Architecture:
 *   - stm32_usb_cdc_init() creates the RX StreamBuffer, enables the OTG_HS
 *     peripheral clock and IRQ, then calls tusb_init().
 *   - OTG_HS_IRQHandler() forwards USB interrupts to TinyUSB via tud_int_handler().
 *   - vUsbDeviceTask (in main.c) calls tud_task() in a tight loop; TinyUSB
 *     invokes tud_cdc_rx_cb() from within that task when data arrives.
 *   - tud_cdc_rx_cb() drains the TinyUSB RX FIFO into s_rx_stream.
 *   - stm32_usb_cdc_receive() reads from s_rx_stream (non-blocking).
 *   - stm32_usb_cdc_send() writes to the TinyUSB TX FIFO and flushes.
 *
 * NVIC priority:
 *   OTG_HS_IRQn is set to configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5).
 *   TinyUSB CDC callbacks run from tud_task() (task context), so they may
 *   use FreeRTOS API freely.
 *
 * @note Requires stm32f7xx.h (STM32F7 CMSIS device header) in the include
 *       path. Add CMSIS/Device/ST/STM32F7xx/Include/ from STM32CubeF7 to
 *       CMakeLists.txt target_include_directories when vendoring CMSIS.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "stm32_usb_cdc.h"
#include "tusb.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

/* stm32f7xx.h provides: RCC, OTG_HS_IRQn, NVIC_SetPriority, NVIC_EnableIRQ.
 * This header must be in the include path (from STM32CubeF7 CMSIS package). */
#include "stm32f7xx.h"

#include "FreeRTOSConfig.h"

/* ---------------------------------------------------------------------------
 * Private constants
 * ---------------------------------------------------------------------------*/

/**
 * @enum usb_cdc_timing_t
 * @brief Timing constants for CDC TX flow control.
 *
 * @details
 * k_tx_timeout_ms is the maximum time stm32_usb_cdc_send() will wait for
 * the TinyUSB TX FIFO to drain before returning k_stm32_err_timeout.
 * k_tx_poll_delay_ms is the sleep between TX FIFO space polls.
 *
 * @since Version 1.0.0
 */
typedef enum {
    k_tx_timeout_ms    = 20U, /**< Max TX wait time before timeout (ms) */
    k_tx_poll_delay_ms = 1U,  /**< Poll interval while waiting for TX FIFO space (ms) */
} usb_cdc_timing_t;

/**
 * @enum usb_cdc_buf_sizes_t
 * @brief Buffer sizes for the CDC layer.
 *
 * @details
 * k_rx_stream_size_bytes is the FreeRTOS StreamBuffer capacity. Must be
 * large enough to hold bursts between vCommTask wakeups (100 Hz = 10 ms
 * max latency). At 115200 baud equivalent that is ~1150 bytes; 1024 is
 * sufficient for STAR's frame sizes.
 * k_rx_drain_chunk is how many bytes are read from the TinyUSB FIFO per
 * tud_cdc_rx_cb() invocation.
 *
 * @since Version 1.0.0
 */
typedef enum {
    k_rx_stream_size_bytes = 1024U, /**< RX StreamBuffer capacity (bytes) */
    k_rx_drain_chunk       = 64U,   /**< Bytes read per CDC RX callback */
} usb_cdc_buf_sizes_t;

/**
 * @enum usb_cdc_rhport_t
 * @brief TinyUSB root hub port for the OTG_HS peripheral.
 *
 * @details
 * On STM32F7 with both OTG_FS and OTG_HS peripherals defined, TinyUSB
 * assigns rhport 0 to OTG_FS and rhport 1 to OTG_HS. This project uses
 * OTG_HS with internal full-speed PHY (configured via CFG_TUSB_RHPORT1_MODE
 * in tusb_config.h).
 *
 * @since Version 1.0.0
 */
typedef enum {
    k_rhport_hs = 1U, /**< TinyUSB rhport index for OTG_HS peripheral */
} usb_cdc_rhport_t;

/* ---------------------------------------------------------------------------
 * Private state
 * ---------------------------------------------------------------------------*/

/**
 * @var s_rx_stream
 * @brief FreeRTOS StreamBuffer for received CDC data.
 *
 * @details
 * Written by tud_cdc_rx_cb() (called from vUsbDeviceTask via tud_task()).
 * Read by stm32_usb_cdc_receive() (called from vCommTask). The StreamBuffer
 * provides thread-safe single-producer / single-consumer transfer with no
 * additional locking required.
 *
 * @note Initialized in stm32_usb_cdc_init(). NULL before initialization.
 * @warning Do not access directly; use stm32_usb_cdc_receive() only.
 * @since Version 1.0.0
 */
static StreamBufferHandle_t s_rx_stream = NULL;

/* ---------------------------------------------------------------------------
 * TinyUSB callbacks (called from vUsbDeviceTask context via tud_task())
 * ---------------------------------------------------------------------------*/

/**
 * @brief TinyUSB CDC receive callback -- drain RX FIFO into StreamBuffer.
 *
 * @details
 * Called by TinyUSB (from within tud_task()) when CDC data is available.
 * Reads all available bytes from the TinyUSB CDC FIFO in k_rx_drain_chunk
 * sized chunks and pushes them into s_rx_stream for consumption by vCommTask.
 *
 * Uses xStreamBufferSend (not ISR variant) because this runs in the
 * vUsbDeviceTask context, not in an ISR. Timeout is 0 (non-blocking):
 * bytes are silently dropped if the StreamBuffer is full, which is preferable
 * to blocking the USB task and stalling enumeration.
 *
 * @param[in] itf CDC interface number (always 0 for this single-interface device)
 *
 * @pre stm32_usb_cdc_init() called -- s_rx_stream must not be NULL
 * @post Received bytes appended to s_rx_stream
 *
 * @note Called from vUsbDeviceTask (tud_task()) context, NOT from ISR
 * @since Version 1.0.0
 */
void tud_cdc_rx_cb(uint8_t itf)
{
    (void)itf;

    uint8_t buf[k_rx_drain_chunk];

    while (tud_cdc_available() > 0U) {
        uint32_t count = tud_cdc_read(buf, sizeof(buf));
        if (count > 0U) {
            (void)xStreamBufferSend(s_rx_stream, buf, count, 0U);
        }
    }
}

/**
 * @brief TinyUSB CDC line-state callback -- USB disconnect fast-path.
 *
 * @details
 * Called by TinyUSB when the host asserts or de-asserts DTR (Data Terminal
 * Ready). When DTR goes low the host has closed the port. In Phase 4 this
 * will trigger an immediate e-stop via stm32_shared_data without waiting
 * for the 500 ms watchdog.
 *
 * @param[in] itf CDC interface number
 * @param[in] dtr Data Terminal Ready -- true when host port is open
 * @param[in] rts Request To Send (unused)
 *
 * @pre stm32_usb_cdc_init() called
 * @post No state modified (Phase 4 TODO: stm32_shared_data_set_estop)
 *
 * @note Called from vUsbDeviceTask (tud_task()) context, NOT from ISR
 * @todo Phase 4: if (!dtr) { stm32_shared_data_set_estop(true); }
 * @since Version 1.0.0
 */
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void)itf;
    (void)rts;

    if (!dtr) {
        /* Host closed the port -- Phase 4: trigger e-stop immediately */
        /* TODO Phase 4: stm32_shared_data_set_estop(true); */
    }
}

/* ---------------------------------------------------------------------------
 * OTG_HS IRQ handler
 * ---------------------------------------------------------------------------*/

/**
 * @brief USB OTG HS interrupt handler -- forward to TinyUSB.
 *
 * @details
 * Called by the Cortex-M7 NVIC when an OTG_HS USB event occurs (SOF,
 * transfer complete, enumeration, disconnect, etc.). Forwards the interrupt
 * to TinyUSB's DCD driver via tud_int_handler(k_rhport_hs).
 *
 * NVIC priority is set to configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY in
 * stm32_usb_cdc_init(), allowing TinyUSB to use FreeRTOS API from within
 * its interrupt-level code (e.g. dcd_event_bus_reset uses task notifications).
 *
 * @pre stm32_usb_cdc_init() called and NVIC priority configured
 * @post TinyUSB DCD driver processes the pending USB event
 *
 * @note NVIC priority must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
 * @since Version 1.0.0
 */
void OTG_HS_IRQHandler(void)
{
    tud_int_handler(k_rhport_hs);
}

/* ---------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------------*/

/**
 * @brief Initialize the USB CDC transport and TinyUSB stack.
 *
 * @details
 * Performs the following initialization sequence:
 * 1. Create FreeRTOS StreamBuffer (k_rx_stream_size_bytes capacity, 1-byte trigger)
 * 2. Enable OTG_HS AHB1 peripheral clock (RCC->AHB1ENR |= RCC_AHB1ENR_OTGHSEN)
 * 3. Set OTG_HS NVIC priority to configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
 * 4. Enable OTG_HS IRQ via NVIC_EnableIRQ(OTG_HS_IRQn)
 * 5. Call tusb_init() to start the TinyUSB device stack
 *
 * Must be called after system clock init (216 MHz core, 48 MHz USB PLL) and
 * before vTaskStartScheduler().
 *
 * @return stm32_err_t
 * @retval k_stm32_err_ok              Initialization successful
 * @retval k_stm32_err_hardware        StreamBuffer creation or tusb_init() failed
 *
 * @pre System clock configured: 216 MHz SYSCLK, 48 MHz PLL48CLK for USB
 * @pre FreeRTOS scheduler not yet started (xStreamBufferCreate is safe pre-scheduler)
 * @post s_rx_stream created and valid
 * @post OTG_HS clock enabled and IRQ active at correct FreeRTOS-safe priority
 * @post TinyUSB device stack running; vUsbDeviceTask may now call tud_task()
 *
 * @note Call once from main() before vTaskStartScheduler()
 * @since Version 1.0.0
 */
stm32_err_t stm32_usb_cdc_init(void)
{
    /* Step 1: Create RX StreamBuffer (single-producer/single-consumer) */
    s_rx_stream = xStreamBufferCreate((size_t)k_rx_stream_size_bytes, 1U);
    if (s_rx_stream == NULL) {
        return k_stm32_err_hardware;
    }

    /* Step 2: Enable OTG_HS peripheral clock on AHB1 bus */
    RCC->AHB1ENR |= RCC_AHB1ENR_OTGHSEN;

    /* Step 3: Set NVIC priority -- must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
     * so that TinyUSB callbacks can safely call FreeRTOS API from IRQ context. */
    NVIC_SetPriority(OTG_HS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);

    /* Step 4: Enable OTG_HS interrupt in NVIC */
    NVIC_EnableIRQ(OTG_HS_IRQn);

    /* Step 5: Initialize TinyUSB device stack */
    if (!tusb_init()) {
        return k_stm32_err_hardware;
    }

    return k_stm32_err_ok;
}

/**
 * @brief Send bytes over USB CDC.
 *
 * @details
 * Writes data to the TinyUSB CDC TX FIFO in chunks, polling for available
 * space with a 1 ms sleep between attempts. Flushes the TX FIFO when all
 * bytes are written. If the FIFO does not drain within k_tx_timeout_ms,
 * returns k_stm32_err_timeout without dropping bytes silently.
 *
 * The flush triggers vUsbDeviceTask (via tud_task()) to send the data over
 * USB. This function blocks up to k_tx_timeout_ms waiting for FIFO space;
 * the actual USB transmission happens asynchronously in vUsbDeviceTask.
 *
 * @param[in] data  Buffer to transmit (must not be NULL)
 * @param[in] len   Number of bytes to send
 *
 * @return stm32_err_t
 * @retval k_stm32_err_ok       All bytes queued and flush triggered
 * @retval k_stm32_err_null_ptr data is NULL
 * @retval k_stm32_err_timeout  TX FIFO did not drain within k_tx_timeout_ms
 *
 * @pre stm32_usb_cdc_init() must have been called
 * @pre data must point to a valid buffer of at least len bytes
 * @post Bytes queued in TinyUSB TX FIFO and flush triggered
 *
 * @note Not re-entrant; only one task should call this at a time
 * @since Version 1.0.0
 */
stm32_err_t stm32_usb_cdc_send(const uint8_t* data, uint32_t len)
{
    if (data == NULL) {
        return k_stm32_err_null_ptr;
    }

    TickType_t const deadline = xTaskGetTickCount() +
                                pdMS_TO_TICKS(k_tx_timeout_ms);
    uint32_t written = 0U;

    while (written < len) {
        uint32_t const avail = tud_cdc_write_available();
        if (avail == 0U) {
            if (xTaskGetTickCount() >= deadline) {
                return k_stm32_err_timeout;
            }
            vTaskDelay(pdMS_TO_TICKS(k_tx_poll_delay_ms));
            continue;
        }

        uint32_t const remaining = len - written;
        uint32_t const chunk = (remaining < avail) ? remaining : avail;
        written += tud_cdc_write(&data[written], chunk);
    }

    (void)tud_cdc_write_flush();
    return k_stm32_err_ok;
}

/**
 * @brief Receive bytes from the USB CDC RX StreamBuffer.
 *
 * @details
 * Reads up to max_len bytes from the FreeRTOS StreamBuffer populated by
 * tud_cdc_rx_cb(). Returns immediately (non-blocking, timeout = 0); sets
 * *out_len = 0 if no data is currently available.
 *
 * @param[out] buf      Destination buffer
 * @param[in]  max_len  Maximum bytes to read
 * @param[out] out_len  Actual bytes written to buf (0 if no data)
 *
 * @return stm32_err_t
 * @retval k_stm32_err_ok       Read succeeded (out_len may be 0)
 * @retval k_stm32_err_null_ptr buf or out_len is NULL
 *
 * @pre stm32_usb_cdc_init() must have been called
 * @pre buf must point to a valid buffer of at least max_len bytes
 * @post *out_len set to number of bytes written to buf
 *
 * @note Non-blocking; caller must poll or use vTaskDelay between calls
 * @since Version 1.0.0
 */
stm32_err_t stm32_usb_cdc_receive(uint8_t* buf, uint32_t max_len,
                                                 uint32_t* out_len)
{
    if (buf == NULL || out_len == NULL) {
        return k_stm32_err_null_ptr;
    }

    *out_len = (uint32_t)xStreamBufferReceive(s_rx_stream, buf,
                                               (size_t)max_len, 0U);
    return k_stm32_err_ok;
}

/**
 * @brief Check whether a CDC host is connected (DTR asserted).
 *
 * @details
 * Returns true when the host has opened the CDC serial port and asserted
 * DTR. False during enumeration, when no host is connected, or when the
 * host port is closed.
 *
 * @return bool true if host CDC port is open, false otherwise
 *
 * @pre stm32_usb_cdc_init() must have been called
 * @post No side effects
 *
 * @since Version 1.0.0
 */
bool stm32_usb_cdc_is_connected(void)
{
    return tud_cdc_connected();
}
