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
/***********************************************************************************************************************
* MODIFICATION NOTICE
* This file has been modified by the STAR project for use in the STAR robotics platform.
* Modifications include: Documentation additions, C23 typed enum conversions, and style guide compliance.
* Modified files are maintained by Locked, Inc. as part of the STAR project.
* Original Renesas code remains under Renesas copyright as stated above.
***********************************************************************************************************************/
/**
 * @file lowsrc.h
 * @brief Low-level stream I/O support functions for C standard library
 *
 * @details
 * Implements low-level file I/O operations required by the C standard library
 * (stdio.h). These functions provide the bridge between high-level library
 * calls (fopen, fread, fwrite, etc.) and hardware-specific I/O operations.
 *
 * **Compiler Support:**
 * This header provides different function signatures for three supported compilers:
 * - **CC-RX (Renesas):** Full POSIX-like interface with semaphore support
 * - **GCC (GNURX):** Newlib-based interface with stubs for unsupported operations
 * - **IAR ICCRX:** IAR-specific __write/__read interface
 *
 * **Standard I/O Integration:**
 * - File descriptor 0: stdin (standard input)
 * - File descriptor 1: stdout (standard output)
 * - File descriptor 2: stderr (standard error)
 * - File descriptors 3+: User files (if filesystem support added)
 *
 * **Reentrant Support (CC-RX only):**
 * When _REENTRANT is defined, provides thread-safe errno access and
 * semaphore operations for multi-threaded environments.
 *
 * @par Hardware Dependencies
 * - RX72N microcontroller (R5F572NN)
 * - UART peripheral for serial I/O (if configured)
 * - Debug interface (E1/E2/E2 Lite) for virtual console
 *
 * @par References
 * - Renesas RX Family C/C++ Compiler Package User's Manual (CC-RX)
 * - GNU Arm Embedded Toolchain Newlib Documentation (GNUC)
 * - IAR C/C++ Compiler for Renesas RX User Guide (ICCRX)
 *
 * @note Ported from Renesas Smart Configurator (SMC) generated code
 * @warning Most functions are NOT thread-safe unless _REENTRANT is defined (CC-RX only)
 * @since Version 2.0.0
 */
/***********************************************************************************************************************
* History : DD.MM.YYYY Version  Description
*         : 28.02.2019 2.00     Merged processing of all devices.
*                               Added support for GNUC and ICCRX.
*                               Fixed coding style.
*         : 31.07.2020 2.01     Fixed an issue that caused build errors when the _REENTRANT option was specified in
*                               the CCRX compiler.
*         : 29.01.2021 3.01     Added tha __write function and __read function for ICCRX.
***********************************************************************************************************************/

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/

#pragma once

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
#if defined(__CCRX__)

/**
 * @brief Initialize I/O library and file descriptors
 *
 * @details
 * Initializes the C standard library I/O subsystem. Sets up standard
 * file descriptors (stdin, stdout, stderr) and prepares internal
 * file descriptor table for user file operations.
 *
 * This function is automatically called during C runtime initialization
 * if BSP_CFG_IO_LIB_ENABLE == 1 in r_bsp_config.h.
 *
 * @return void
 *
 * @pre BSP_CFG_IO_LIB_ENABLE must be 1
 * @pre Called before any stdio operations
 * @post Standard file descriptors initialized
 * @post File descriptor table ready for use
 *
 * @note Called automatically by resetprg.c startup code
 * @warning Do not call manually unless you know what you're doing
 *
 * @see close_all() Cleanup counterpart function
 * @since Version 2.0.0
 */
void init_iolib(void);

/**
 * @brief Close all open file descriptors
 *
 * @details
 * Closes all open file descriptors and flushes any pending output buffers.
 * Called during program termination to ensure clean shutdown of I/O subsystem.
 *
 * This function is automatically called during C runtime cleanup
 * if BSP_CFG_IO_LIB_ENABLE == 1 in r_bsp_config.h.
 *
 * @return void
 *
 * @pre init_iolib() was called
 * @pre No I/O operations in progress on any file descriptor
 * @post All file descriptors closed
 * @post Output buffers flushed
 *
 * @note Called automatically by exit() or program termination
 * @warning Do not access file descriptors after calling this function
 *
 * @see init_iolib() Initialization counterpart function
 * @since Version 2.0.0
 */
void close_all(void);

/**
 * @brief Open a file and return file descriptor
 *
 * @details
 * Opens a file with specified mode and flags, returning a file descriptor
 * for subsequent I/O operations. This is a low-level function called by
 * the C standard library's fopen().
 *
 * **Note:** Current implementation does not support actual file system
 * operations - only standard streams (stdin/stdout/stderr) are functional.
 *
 * @param[in] name File path (not currently used - no filesystem support)
 * @param[in] mode Open mode (read/write/append - not currently used)
 * @param[in] flg Flags (not currently used)
 *
 * @return long File descriptor (>=0) on success, negative on error
 * @retval -1 Error (file not found, invalid mode, or no filesystem support)
 *
 * @pre init_iolib() called successfully
 * @post File descriptor allocated if successful
 *
 * @note Not thread-safe unless _REENTRANT is defined
 * @warning Filesystem not implemented - only standard streams work
 *
 * @see close() Close file descriptor
 * @see read() Read from file descriptor
 * @see write() Write to file descriptor
 * @since Version 2.0.0
 */
long open(const char* name, long mode, long flg);

/**
 * @brief Close a file descriptor
 *
 * @details
 * Closes the specified file descriptor and flushes any pending output.
 * Called by the C standard library's fclose().
 *
 * @param[in] fileno File descriptor to close (>=0)
 *
 * @return long Status code
 * @retval 0 Success
 * @retval -1 Error (invalid file descriptor)
 *
 * @pre File descriptor was opened via open()
 * @post File descriptor released
 * @post Output buffer flushed
 *
 * @note Not thread-safe unless _REENTRANT is defined
 * @warning Do not use file descriptor after closing
 *
 * @see open() Open file descriptor
 * @since Version 2.0.0
 */
long close(long fileno);

/**
 * @brief Write data to file descriptor
 *
 * @details
 * Writes count bytes from buf to the specified file descriptor.
 * For stdout/stderr, outputs to configured device (UART or virtual console).
 *
 * This function is called by the C standard library's fwrite(), fprintf(),
 * and printf() functions.
 *
 * @param[in] fileno File descriptor (0=stdin, 1=stdout, 2=stderr, 3+=files)
 * @param[in] buf Data buffer to write (must be valid for count bytes)
 * @param[in] count Number of bytes to write (must be >= 0)
 *
 * @return long Number of bytes actually written
 * @retval count Success (all bytes written)
 * @retval <count Partial write (output buffer full or error)
 * @retval -1 Error (invalid file descriptor or null buffer)
 *
 * @pre File descriptor is open
 * @pre buf points to valid memory of at least count bytes
 * @post count bytes written to file descriptor (if successful)
 *
 * @note Not thread-safe unless _REENTRANT is defined
 * @note Function may block until all data is written
 * @warning Do not call from interrupt context
 *
 * @see read() Read from file descriptor
 * @see charput() Character-level output
 * @since Version 2.0.0
 */
long write(long fileno, const unsigned char* buf, long count);

/**
 * @brief Read data from file descriptor
 *
 * @details
 * Reads up to count bytes from the specified file descriptor into buf.
 * For stdin, reads from configured input device (UART or virtual console).
 *
 * This function is called by the C standard library's fread(), fscanf(),
 * and scanf() functions.
 *
 * @param[in] fileno File descriptor (0=stdin, 1=stdout, 2=stderr, 3+=files)
 * @param[out] buf Buffer to receive data (must be at least count bytes)
 * @param[in] count Maximum number of bytes to read (must be >= 0)
 *
 * @return long Number of bytes actually read
 * @retval >0 Success (bytes read)
 * @retval 0 End of file or no data available
 * @retval -1 Error (invalid file descriptor or null buffer)
 *
 * @pre File descriptor is open
 * @pre buf points to valid memory of at least count bytes
 * @post Up to count bytes read into buf (if successful)
 *
 * @note Not thread-safe unless _REENTRANT is defined
 * @note Function may block until data is available
 * @warning Do not call from interrupt context
 *
 * @see write() Write to file descriptor
 * @see charget() Character-level input
 * @since Version 2.0.0
 */
long read(long fileno, unsigned char* buf, long count);

/**
 * @brief Seek to position in file
 *
 * @details
 * Changes the file position indicator for the specified file descriptor.
 * Called by the C standard library's fseek() function.
 *
 * **Note:** Current implementation does not support seeking - always fails
 * because no filesystem is present.
 *
 * @param[in] fileno File descriptor
 * @param[in] offset Byte offset from base position
 * @param[in] base Seek base (SEEK_SET=0, SEEK_CUR=1, SEEK_END=2)
 *
 * @return long New file position, or -1 on error
 * @retval -1 Error (seek not supported without filesystem)
 *
 * @pre File descriptor is open
 * @post File position unchanged (seek not implemented)
 *
 * @note Not thread-safe unless _REENTRANT is defined
 * @warning Always fails - no filesystem support
 *
 * @since Version 2.0.0
 */
long lseek(long fileno, long offset, long base);

#ifdef _REENTRANT
/**
 * @brief Get address of thread-local errno variable
 *
 * @details
 * Returns the address of the thread-local errno variable for the current
 * thread. Required for thread-safe errno access in multi-threaded environments.
 *
 * Only available when _REENTRANT is defined (CC-RX reentrant mode).
 *
 * @return long* Pointer to thread-local errno variable
 *
 * @pre _REENTRANT defined (reentrant mode enabled)
 * @pre RTOS running (for thread-local storage)
 * @post Pointer to valid errno variable returned
 *
 * @note Thread-safe by design
 * @see wait_sem() Semaphore wait
 * @see signal_sem() Semaphore signal
 * @since Version 2.0.0
 */
long* errno_addr(void);

/**
 * @brief Wait on semaphore (P operation)
 *
 * @details
 * Waits (blocks) until the specified semaphore becomes available, then
 * decrements it. Used for thread synchronization in reentrant library code.
 *
 * Only available when _REENTRANT is defined (CC-RX reentrant mode).
 *
 * @param[in] semnum Semaphore number (0-based index into semaphore table)
 *
 * @return long Status code
 * @retval 0 Success (semaphore acquired)
 * @retval -1 Error (invalid semaphore number or timeout)
 *
 * @pre _REENTRANT defined (reentrant mode enabled)
 * @pre RTOS running
 * @pre semnum is valid semaphore index
 * @post Semaphore acquired (decremented)
 *
 * @note Thread-safe by design
 * @note Function blocks until semaphore available
 * @warning Do not call from interrupt context
 *
 * @see signal_sem() Release semaphore
 * @see errno_addr() Thread-local errno
 * @since Version 2.0.0
 */
long wait_sem(long semnum);

/**
 * @brief Signal semaphore (V operation)
 *
 * @details
 * Increments the specified semaphore, potentially unblocking a waiting thread.
 * Used for thread synchronization in reentrant library code.
 *
 * Only available when _REENTRANT is defined (CC-RX reentrant mode).
 *
 * @param[in] semnum Semaphore number (0-based index into semaphore table)
 *
 * @return long Status code
 * @retval 0 Success (semaphore released)
 * @retval -1 Error (invalid semaphore number)
 *
 * @pre _REENTRANT defined (reentrant mode enabled)
 * @pre RTOS running
 * @pre semnum is valid semaphore index
 * @post Semaphore released (incremented)
 *
 * @note Thread-safe by design
 * @warning Do not call from interrupt context
 *
 * @see wait_sem() Acquire semaphore
 * @see errno_addr() Thread-local errno
 * @since Version 2.0.0
 */
long signal_sem(long semnum);
#endif /* _REENTRANT */
#endif /* defined(__CCRX__) */

#if defined(__GNUC__)

/**
 * @brief Write data to file descriptor (GNUC/Newlib)
 *
 * @details
 * GNUC/Newlib implementation of write(). Writes count bytes from buf
 * to the specified file descriptor. For stdout/stderr, outputs to
 * configured device.
 *
 * @param[in] fileno File descriptor (0=stdin, 1=stdout, 2=stderr)
 * @param[in] buf Data buffer to write
 * @param[in] count Number of bytes to write
 *
 * @return int Number of bytes written, or -1 on error
 *
 * @pre File descriptor is open
 * @post count bytes written to file descriptor
 *
 * @note Not thread-safe
 * @see _write() Alternative GNUC write implementation
 * @since Version 2.0.0
 */
int write(int fileno, char* buf, int count);

/**
 * @brief Read data from file descriptor (GNUC/Newlib)
 *
 * @details
 * GNUC/Newlib implementation of read(). Reads up to count bytes from
 * the specified file descriptor into buf.
 *
 * @param[in] fileno File descriptor (0=stdin, 1=stdout, 2=stderr)
 * @param[out] buf Buffer to receive data
 * @param[in] count Maximum bytes to read
 *
 * @return int Number of bytes read, or -1 on error
 *
 * @pre File descriptor is open
 * @post Up to count bytes read into buf
 *
 * @note Not thread-safe
 * @see _read() Alternative GNUC read implementation
 * @since Version 2.0.0
 */
int read(int fileno, char* buf, int count);

/**
 * @brief Write data to file descriptor (GNUC/Newlib alternative)
 *
 * @details
 * Alternative GNUC/Newlib write() implementation with underscore prefix.
 * Behavior identical to write().
 *
 * @param[in] fileno File descriptor
 * @param[in] buf Data buffer to write
 * @param[in] count Number of bytes to write
 *
 * @return int Number of bytes written, or -1 on error
 *
 * @pre File descriptor is open
 * @post count bytes written to file descriptor
 *
 * @note Not thread-safe
 * @see write() Standard write implementation
 * @since Version 2.0.0
 */
int _write(int fileno, char* buf, int count);

/**
 * @brief Read data from file descriptor (GNUC/Newlib alternative)
 *
 * @details
 * Alternative GNUC/Newlib read() implementation with underscore prefix.
 * Behavior identical to read().
 *
 * @param[in] fileno File descriptor
 * @param[out] buf Buffer to receive data
 * @param[in] count Maximum bytes to read
 *
 * @return int Number of bytes read, or -1 on error
 *
 * @pre File descriptor is open
 * @post Up to count bytes read into buf
 *
 * @note Not thread-safe
 * @see read() Standard read implementation
 * @since Version 2.0.0
 */
int _read(int fileno, char* buf, int count);

/**
 * @brief Close file descriptor stub (GNUC/Newlib)
 *
 * @details
 * Stub implementation - no actual file closure supported.
 * Required by Newlib but not implemented.
 *
 * @return void
 *
 * @pre Newlib runtime initialized
 * @pre No invalid file descriptors passed (not verified - stub only)
 * @post No side effects on system state
 * @post File descriptors remain unchanged
 *
 * @warning Does nothing - stub only
 * @since Version 2.0.0
 */
void close(void);

/**
 * @brief File status stub (GNUC/Newlib)
 *
 * @details
 * Stub implementation - no file status support.
 * Required by Newlib but not implemented.
 *
 * @return void
 *
 * @pre Newlib runtime initialized
 * @pre No invalid file descriptors passed (not verified - stub only)
 * @post No side effects on system state
 * @post File status remains unchanged
 *
 * @warning Does nothing - stub only
 * @since Version 2.0.0
 */
void fstat(void);

/**
 * @brief Terminal detection stub (GNUC/Newlib)
 *
 * @details
 * Stub implementation - no terminal detection.
 * Required by Newlib but not implemented.
 *
 * @return void
 *
 * @pre Newlib runtime initialized
 * @pre No invalid file descriptors passed (not verified - stub only)
 * @post No side effects on system state
 * @post Terminal detection status remains unchanged
 *
 * @warning Does nothing - stub only
 * @since Version 2.0.0
 */
void isatty(void);

/**
 * @brief Seek stub (GNUC/Newlib)
 *
 * @details
 * Stub implementation - no file seeking support.
 * Required by Newlib but not implemented.
 *
 * @return void
 *
 * @pre Newlib runtime initialized
 * @pre No invalid file descriptors passed (not verified - stub only)
 * @post No side effects on system state
 * @post File position remains unchanged
 *
 * @warning Does nothing - stub only
 * @since Version 2.0.0
 */
void lseek(void);
#endif /* defined(__GNUC__) */

#if defined(__ICCRX__)

/**
 * @brief Write data to file handle (IAR ICCRX)
 *
 * @details
 * IAR compiler-specific write function. Writes bufSize bytes from buf
 * to the specified file handle. For stdout/stderr, outputs to configured device.
 *
 * @param[in] handle File handle (0=stdin, 1=stdout, 2=stderr)
 * @param[in] buf Data buffer to write (must be valid for bufSize bytes)
 * @param[in] bufSize Number of bytes to write
 *
 * @return size_t Number of bytes written
 * @retval bufSize Success (all bytes written)
 * @retval <bufSize Partial write or error
 *
 * @pre File handle is valid
 * @post bufSize bytes written to file handle
 *
 * @note Not thread-safe
 * @see __read() IAR read implementation
 * @since Version 3.01
 */
size_t __write(int handle, const unsigned char* buf, size_t bufSize);

/**
 * @brief Read data from file handle (IAR ICCRX)
 *
 * @details
 * IAR compiler-specific read function. Reads up to bufSize bytes from
 * the specified file handle into buf. For stdin, reads from configured input device.
 *
 * @param[in] handle File handle (0=stdin, 1=stdout, 2=stderr)
 * @param[out] buf Buffer to receive data (must be at least bufSize bytes)
 * @param[in] bufSize Maximum number of bytes to read
 *
 * @return size_t Number of bytes read
 * @retval >0 Success (bytes read)
 * @retval 0 End of file or no data available
 *
 * @pre File handle is valid
 * @post Up to bufSize bytes read into buf
 *
 * @note Not thread-safe
 * @see __write() IAR write implementation
 * @since Version 3.01
 */
size_t __read(int handle, unsigned char* buf, size_t bufSize);
#endif /* defined(__ICCRX__) */
