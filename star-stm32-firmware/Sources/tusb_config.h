/**
 * @file tusb_config.h
 * @brief TinyUSB configuration for STM32F7 CDC ACM device.
 *
 * @details
 * Configures TinyUSB for a single CDC ACM interface on the STM32F767/F746
 * USB OTG HS peripheral operating in full-speed mode via the internal FS PHY
 * (OTG_HS_INTERNAL_FS). The host sees a single CDC serial port (/dev/ttyACM0).
 *
 * @note Included automatically by TinyUSB -- do not include directly.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

/* ---------------------------------------------------------------------------
 * Controller and speed
 * ---------------------------------------------------------------------------*/

/**
 * @brief TinyUSB root-hub port mode for OTG_HS (rhport 1 on STM32F7).
 *
 * @details
 * On STM32F7 with both OTG_FS and OTG_HS defined, TinyUSB assigns:
 *   rhport 0 -> OTG_FS (USB_OTG_FS_PERIPH_BASE)
 *   rhport 1 -> OTG_HS (USB_OTG_HS_PERIPH_BASE)
 * This project uses OTG_HS with its internal FS PHY (OTG_HS_INTERNAL_FS),
 * so rhport 1 must be declared here and passed to tud_int_handler().
 * OTG_HS_IRQHandler in stm32_usb_cdc.c calls tud_int_handler(1) accordingly.
 */
#define CFG_TUSB_RHPORT1_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

/** @brief MCU family selection -- tells TinyUSB which driver to use. */
#define CFG_TUSB_MCU OPT_MCU_STM32F7

/* ---------------------------------------------------------------------------
 * OS integration
 * ---------------------------------------------------------------------------*/

/** @brief Use FreeRTOS for TinyUSB task synchronization. */
#define CFG_TUSB_OS OPT_OS_FREERTOS

/* ---------------------------------------------------------------------------
 * Debug
 * ---------------------------------------------------------------------------*/

/** @brief TinyUSB debug log level. 0 = off, 1 = error, 2 = warning, 3 = info. */
#define CFG_TUSB_DEBUG 0

/* ---------------------------------------------------------------------------
 * Device class configuration
 * ---------------------------------------------------------------------------*/

/** @brief Enable CDC class (Communication Device Class). */
#define CFG_TUD_CDC 1

/** @brief Number of CDC interfaces (1 = single serial port). */
#define CFG_TUD_CDC_EP_BUFSIZE 512

/** @brief CDC RX and TX FIFO sizes in bytes. */
#define CFG_TUD_CDC_RX_BUFSIZE 512
#define CFG_TUD_CDC_TX_BUFSIZE 512

/* Disable all unused device classes */
#define CFG_TUD_MSC    0
#define CFG_TUD_HID    0
#define CFG_TUD_MIDI   0
#define CFG_TUD_VENDOR 0

/* OPT_MCU_STM32F7 confirmed present in libs/tinyusb/src/tusb_option.h line 83. */
