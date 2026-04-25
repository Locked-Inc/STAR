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
* Copyright (C) 2019 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* MODIFICATION NOTICE
* This file has been modified by the STAR project for use in the STAR robotics platform.
* Modifications include: Documentation additions, C23 typed enum conversions, and style guide compliance.
* Modified files are maintained by Locked, Inc. as part of the STAR project.
* Original Renesas code remains under Renesas copyright as stated above.
***********************************************************************************************************************/
/**
 * @file lowlvl.h
 * @brief Low-level character I/O functions for standard input/output
 *
 * @details
 * Provides character-level I/O primitives for standard input and output.
 * These functions serve as the foundation for stdio library operations
 * (printf, scanf, etc.) by implementing single-character read/write.
 *
 * Output destination and input source depend on BSP configuration:
 * - **E1 Virtual Console:** Debug output via Renesas E1/E2/E2 Lite emulator
 * - **Serial Port (UART):** User-defined functions via BSP_CFG_USER_CHARPUT_ENABLED
 *   and BSP_CFG_USER_CHARGET_ENABLED in r_bsp_config.h
 *
 * @par Hardware Dependencies
 * - RX72N microcontroller (R5F572NN)
 * - Debug interface (E1/E2/E2 Lite) for virtual console output
 * - UART peripheral if using serial port I/O
 *
 * @par References
 * - r_bsp_config.h for I/O redirection configuration
 * - Renesas RX Family C/C++ Compiler Package User's Manual
 *
 * @note Ported from Renesas Smart Configurator (SMC) generated code
 * @warning These functions are NOT thread-safe - caller must provide synchronization
 * @since Version 1.0.0
 *
 * @author Locked, Inc.
 * @date 2019 (original), 2026 (STAR modifications)
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
 *
 * @par NASA Power of 10 Compliance
 * - Rule 4: Functions kept short and verifiable
 * - Rule 7: All return values checked by callers
 * - All rules maintained throughout modifications
 *
 * @par SOLID Principles
 * - Single Responsibility: Provides only character I/O primitives
 * - Open/Closed: Extensible via BSP configuration without modifying code
 * - Liskov Substitution: I/O functions maintain consistent contracts
 * - Interface Segregation: Minimal API (charput, charget)
 * - Dependency Inversion: Abstracts I/O destination through configuration
 */
/***********************************************************************************************************************
* History : DD.MM.YYYY Version  Description
*         : 28.02.2019 1.00     First Release
***********************************************************************************************************************/

#pragma once

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Typedef definitions
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/

/**
 * @brief Output one character to standard output
 *
 * @details
 * Writes a single character to the configured standard output device.
 * This function serves as the low-level output primitive for the C
 * standard library's printf() and related functions.
 *
 * The actual output destination is determined by BSP configuration:
 * - If BSP_CFG_USER_CHARPUT_ENABLED == 0: Output to E1 Virtual Console
 * - If BSP_CFG_USER_CHARPUT_ENABLED == 1: Output to user-defined function
 *
 * @param[in] output_char Character to output (ASCII or binary data)
 *
 * @pre BSP I/O library must be initialized (BSP_CFG_IO_LIB_ENABLE == 1)
 * @pre If using user charput, BSP_CFG_USER_CHARPUT_FUNCTION must be defined
 * @post Character written to configured output device
 * @post Output buffer flushed if device requires it
 *
 * @note Not thread-safe, caller must provide synchronization
 * @note Function blocks until character is written to output device
 * @warning Do not call from interrupt context if output is buffered
 *
 * @par Example:
 * @code
 * // Output single character (requires BSP_CFG_IO_LIB_ENABLE == 1)
 * charput('A');  // Writes 'A' to stdout
 *
 * // Output newline character
 * charput('\n');  // Sends newline to console
 * @endcode
 *
 * @see charget() Input counterpart function
 * @see r_bsp_config.h BSP I/O configuration
 *
 * @since Version 1.0.0
 */
void charput(char output_char);

/**
 * @brief Input one character from standard input
 *
 * @details
 * Reads a single character from the configured standard input device.
 * This function serves as the low-level input primitive for the C
 * standard library's scanf() and related functions.
 *
 * The actual input source is determined by BSP configuration:
 * - If BSP_CFG_USER_CHARGET_ENABLED == 0: Input from E1 Virtual Console
 * - If BSP_CFG_USER_CHARGET_ENABLED == 1: Input from user-defined function
 *
 * @return char Character read from input device (ASCII or binary data)
 * @retval 0x00-0x7F Valid ASCII character
 * @retval 0x80-0xFF Extended character or binary data
 *
 * @pre BSP I/O library must be initialized (BSP_CFG_IO_LIB_ENABLE == 1)
 * @pre If using user charget, BSP_CFG_USER_CHARGET_FUNCTION must be defined
 * @post Character read from configured input device
 * @post Input buffer advanced by one character
 *
 * @note Not thread-safe, caller must provide synchronization
 * @note Function blocks until character is available from input device
 * @warning Do not call from interrupt context
 *
 * @par Example:
 * @code
 * // Read single character from stdin (requires BSP_CFG_IO_LIB_ENABLE == 1)
 * char input_char = charget();  // Blocks until character available
 *
 * // Use in input loop
 * while (1) {
 *     char c = charget();
 *     if (c == '\n') break;
 * }
 * @endcode
 *
 * @see charput() Output counterpart function
 * @see r_bsp_config.h BSP I/O configuration
 *
 * @since Version 1.0.0
 */
char charget(void);
