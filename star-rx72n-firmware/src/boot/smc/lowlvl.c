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
* Copyright (C) 2013 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/**
 * @file lowlvl.c
 * @brief Character-Level I/O for E1 Virtual Console (Debugger Console)
 *
 * @details
 * Provides low-level character input/output functions that redirect stdio
 * (printf/scanf) to the Renesas E1/E2/E20 emulator's Virtual Console feature.
 * This allows debug output to appear in the e2 studio Console view during debugging.
 *
 * **Functions:**
 * - `charput(c)` - Output single character to debugger console
 * - `charget()` - Input single character from debugger console
 *
 * **Virtual Console Mechanism:**
 * The E1 emulator provides a memory-mapped debug port at address 0x00084080
 * with TX/RX data registers and status flags. The debugger monitors these
 * registers and displays output in the IDE Console view.
 *
 * **User Override Support:**
 * Users can provide custom charput/charget implementations (e.g., UART-based)
 * by setting BSP_CFG_USER_CHARPUT_ENABLED=1 in r_bsp_config.h.
 *
 * **STAR Project Usage:**
 * STAR firmware does **not** use stdio (printf/scanf) in production code.
 * This file is only relevant when:
 * 1. BSP_CFG_IO_LIB_ENABLE=1 (stdio enabled for debugging)
 * 2. Running under E1/E2 emulator (not real hardware)
 * 3. Using printf() for temporary debug output
 *
 * For production STAR builds:
 * - BSP_CFG_IO_LIB_ENABLE=0 (stdio disabled)
 * - Use uart_debug_puts() instead of printf()
 * - This file is excluded from build
 *
 * @warning Virtual Console only works with E1/E2/E20 emulators, not real hardware
 * @warning stdio (printf) violates NASA Power of 10 Rule 3 (no malloc)
 *
 * @see lowsrc.c Stream I/O layer that calls charput/charget
 * @see uart_debug_puts() STAR custom debug output (production alternative)
 * @see r_bsp_config.h BSP_CFG_IO_LIB_ENABLE, BSP_CFG_USER_CHARPUT_ENABLED
 *
 * @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
 * @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
 *
 * @par History
 * - 28.02.2019 v3.00 - Merged all devices, fixed coding style (Renesas)
 * - 25.11.2022 v3.01 - Modified comment (Renesas)
 * - 2026       vSTAR - Added comprehensive Doxygen, converted \#defines to enums
 */
/***********************************************************************************************************************
* History : DD.MM.YYYY Version  Description
*         : 28.02.2019 3.00     Merged processing of all devices.
*                               Fixed coding style.
*         : 25.11.2022 3.01     Modiffied comment.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "platform.h"

/* When using the user startup program, disable the following code. */
#if BSP_CFG_STARTUP_DISABLE == 0

/**
 * @brief E1 Virtual Console register addresses and bit definitions
 */

/**
 * @enum e1_debug_port_addr_t
 * @brief E1 Debug Port base address
 *
 * @details
 * Memory-mapped debug port provided by Renesas E1/E2/E20 emulators for
 * Virtual Console I/O. The debugger monitors this address and displays
 * TX data in the IDE Console view.
 *
 * @see st_dbg_t Debug port register structure
 */
typedef enum : uintptr_t {
  k_e1_dbg_port_addr = 0x00084080, /**< E1 debug port base address */
} e1_debug_port_addr_t;

/**
 * @enum e1_debug_status_bits_t
 * @brief E1 Debug Port status register bit flags
 *
 * @details
 * Status flags in dbgstat register indicate TX/RX buffer readiness.
 * Poll these flags before reading/writing to prevent data loss.
 *
 * @see st_dbg_t::dbgstat Status register
 */
typedef enum : uint32_t {
  k_txfl0en = 0x00000100, /**< TX buffer full (1=full, wait before writing) */
  k_rxfl0en = 0x00001000, /**< RX buffer ready (1=data available, read now) */
} e1_debug_status_bits_t;

/**
 * @def BSP_PRV_E1_DBG_PORT
 * @brief E1 Debug Port register accessor
 *
 * @details
 * Macro to access E1 debug port registers at fixed address 0x84080.
 * Casts address to st_dbg_t pointer for structured register access.
 *
 * @warning Only valid when running under E1/E2/E20 emulator
 * @warning Accessing this on real hardware causes bus error
 */
#define BSP_PRV_E1_DBG_PORT (*(volatile st_dbg_t R_BSP_EVENACCESS_SFR*)k_e1_dbg_port_addr)

/**
 * @struct st_dbg_t
 * @brief E1 Debug Port register layout
 *
 * @details
 * Memory-mapped register structure for Renesas E1/E2/E20 emulator Virtual Console.
 * The debugger monitors this structure and:
 * - Reads tx_data and displays in Console view
 * - Writes user input to rx_data
 * - Updates dbgstat flags for flow control
 *
 * **Register Map:**
 * ```
 * 0x84080: tx_data  (write character here to output)
 * 0x84090: rx_data  (read character from here for input)
 * 0x840C0: dbgstat  (status flags: TX full, RX ready)
 * ```
 *
 * @warning Spacer fields (wk1, wk2) reflect actual hardware register layout
 * @warning Do not reorder fields - must match hardware addresses
 *
 * @see charput() Writes to tx_data, polls dbgstat
 * @see charget() Reads from rx_data, polls dbgstat
 */
typedef struct {
  uint32_t tx_data; /**< TX data register - write char to output to console */
  char     wk1[12]; /**< Reserved spacer (0x84084-0x8408F) */
  uint32_t rx_data; /**< RX data register - read char from console input */
  char     wk2[44]; /**< Reserved spacer (0x84094-0x840BF) */
  uint32_t dbgstat; /**< Status register - TX full (bit 8), RX ready (bit 12) */
} st_dbg_t;

/***********************************************************************************************************************
Exported global variables (to be accessed by other files)
***********************************************************************************************************************/
#if BSP_CFG_USER_CHARPUT_ENABLED != 0
/* If user has indicated they want to provide their own charput function then this is the prototype. */
void BSP_CFG_USER_CHARPUT_FUNCTION(char output_char);
#endif

#if BSP_CFG_USER_CHARGET_ENABLED != 0
/* If user has indicated they want to provide their own charget function then this is the prototype. */
char BSP_CFG_USER_CHARGET_FUNCTION(void);
#endif

/***********************************************************************************************************************
Private global variables and functions
***********************************************************************************************************************/

/**
 * @brief Output single character to E1 Virtual Console or user-provided handler
 *
 * @details
 * Writes one character to the debugger console (or user override function).
 * Used by stdio (printf, puts) for character output.
 *
 * **E1 Virtual Console Mode (default):**
 * 1. Poll dbgstat register until TX buffer empty (TXFL0EN=0)
 * 2. Write character to tx_data register
 * 3. Debugger reads tx_data and displays in Console view
 *
 * **User Override Mode (BSP_CFG_USER_CHARPUT_ENABLED=1):**
 * Calls user-provided function (e.g., UART-based charput).
 *
 * @pre BSP_CFG_IO_LIB_ENABLE=1 (stdio enabled)
 * @post Character written to E1 debug port or user handler
 *
 * @note Blocks until TX buffer ready (busy-wait loop)
 * @note Only works under E1/E2/E20 emulator (real hardware causes bus error)
 *
 * @warning Busy-wait loop violates real-time constraints - use uart_debug_puts() for production
 *
 * @see charget() Character input from Virtual Console
 * @see uart_debug_puts() STAR production debug output (non-blocking)
 * @see BSP_CFG_USER_CHARPUT_FUNCTION User override function (r_bsp_config.h)
 *
 * @since Renesas BSP v3.00, STAR 2026
 */
void charput(char output_char)
{
  /* If user has provided their own charput() function, then call it. */
#if BSP_CFG_USER_CHARPUT_ENABLED == 1
  BSP_CFG_USER_CHARPUT_FUNCTION(output_char);
#else
  /* Wait for transmit buffer to be empty */
  /* WAIT_LOOP */
  while (0 != (BSP_PRV_E1_DBG_PORT.dbgstat & k_txfl0en)) {
    /* do nothing */
    R_BSP_NOP();
  }

  /* Write the character out */
  /* Casting is valid because it matches the type to the right side or argument. */
  BSP_PRV_E1_DBG_PORT.tx_data = (int32_t)output_char;
#endif
}

/**
 * @brief Input single character from E1 Virtual Console or user-provided handler
 *
 * @details
 * Reads one character from the debugger console (or user override function).
 * Used by stdio (scanf, getchar) for character input.
 *
 * **E1 Virtual Console Mode (default):**
 * 1. Poll dbgstat register until RX buffer ready (RXFL0EN=1)
 * 2. Read character from rx_data register
 * 3. Debugger writes user input (from Console view) to rx_data
 *
 * **User Override Mode (BSP_CFG_USER_CHARGET_ENABLED=1):**
 * Calls user-provided function (e.g., UART-based charget).
 *
 * @return char Received character (8-bit ASCII)
 *
 * @pre BSP_CFG_IO_LIB_ENABLE=1 (stdio enabled)
 * @post Character read from E1 debug port or user handler
 *
 * @note **Blocks indefinitely** until character received (busy-wait loop)
 * @note Only works under E1/E2/E20 emulator (real hardware causes bus error)
 *
 * @warning Infinite blocking violates real-time constraints and NASA Rule 2 (bounded loops)
 * @warning STAR firmware does not use scanf - command input via Protocol Buffers
 *
 * @see charput() Character output to Virtual Console
 * @see BSP_CFG_USER_CHARGET_FUNCTION User override function (r_bsp_config.h)
 *
 * @since Renesas BSP v3.00, STAR 2026
 */
char charget(void)
{
  /* If user has provided their own charget() function, then call it. */
#if BSP_CFG_USER_CHARGET_ENABLED == 1
  return BSP_CFG_USER_CHARGET_FUNCTION();
#else
  /* Wait for recieve buffer to be ready */
  /* WAIT_LOOP */
  while (0 == (BSP_PRV_E1_DBG_PORT.dbgstat & k_rxfl0en)) {
    /* do nothing */
    R_BSP_NOP();
  }

  /* Read data, send back up */
  /* Casting is valid because it matches the type to the retern value. */
  return (char)BSP_PRV_E1_DBG_PORT.rx_data;
#endif
}

#endif /* BSP_CFG_STARTUP_DISABLE == 0 */
