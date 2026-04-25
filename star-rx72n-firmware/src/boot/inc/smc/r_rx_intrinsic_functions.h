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
**********************************************************************************************************************
* @copyright Copyright (c) 2026 Locked Inc. Based on Renesas Electronics Corporation source.
*/
/***********************************************************************************************************************
* MODIFICATION NOTICE
* This file has been modified by the STAR project for use in the STAR robotics platform.
* Modifications include: Documentation additions, C23 typed enum conversions, and style guide compliance.
* Modified files are maintained by Locked, Inc. as part of the STAR project.
* Original Renesas code remains under Renesas copyright as stated above.
***********************************************************************************************************************/
/**
 * @file r_rx_intrinsic_functions.h
 * @brief Cross-compiler intrinsic function abstraction layer for RX architecture
 *
 * @details
 * Provides portable access to RX CPU architecture-specific intrinsic functions
 * and compiler built-ins across three supported toolchains: Renesas CC-RX, GNU RX, and IAR ICCRX.
 *
 * **Purpose:**
 * RX processors have specialized instructions for operations like byte swapping,
 * bit manipulation, register access, and mathematical functions. Each compiler
 * exposes these through different intrinsic function names. This header provides
 * a unified R_BSP_* macro interface that maps to the correct compiler intrinsic,
 * enabling portable BSP code.
 *
 * **Macro Categories:**
 * - **Arithmetic:** Max/min, multiply-accumulate, fixed-point math
 * - **Bit Operations:** Byte swap, rotation, bit set/clear/reverse
 * - **CPU Control:** IPL (interrupt priority), PSW (processor status), mode switching
 * - **Stack Pointers:** USP (user), ISP (interrupt)
 * - **Special Registers:** INTB, EXTB, FINTV, ACC, FPSW, DPSW
 * - **Trigonometry:** TFU (Trigonometric Function Unit) - sine, cosine, atan2, hypot
 * - **Special Instructions:** BRK, INT, WAIT, NOP
 *
 * **Compiler Support:**
 * - **CC-RX (Renesas):** Native intrinsics (max, min, revl, etc.)
 * - **GCC (GNURX):** __builtin_* intrinsics or R_BSP_* API functions
 * - **IAR ICCRX:** __* intrinsics (__MAX, __MIN, __REVL, etc.)
 *
 * **Usage Pattern:**
 * Application code uses R_BSP_* macros regardless of compiler:
 * @code
 * uint32_t swapped = R_BSP_REVL(value);        // Byte swap
 * R_BSP_SET_IPL(5);                             // Set interrupt priority
 * uint32_t result = R_BSP_MAX(a, b);            // Get maximum value
 * @endcode
 *
 * The preprocessor selects the correct intrinsic based on compiler:
 * - CC-RX:  R_BSP_REVL(x) -> revl(x)
 * - GCC:    R_BSP_REVL(x) -> __builtin_bswap32(x)
 * - ICCRX:  R_BSP_REVL(x) -> __REVL(x)
 *
 * **GNUC Implementation Notes:**
 * GCC lacks some RX-specific intrinsics, so certain macros call R_BSP_* functions
 * implemented in r_rx_intrinsic_functions.c using inline assembly.
 *
 * **Macro Usage Justification (CLAUDE.md Compliance):**
 * Macros in this file are **justified** per CLAUDE.md policy:
 * - **Conditional compilation:** Compiler-specific code selection
 * - **Reducing duplicated code:** Single interface for 3 compilers
 * - **NOT integer constants:** Not used for numeric values
 *
 * @par Hardware Dependencies
 * - RX72N microcontroller (RXv3 core)
 * - TFU (Trigonometric Function Unit) for trig macros (if BSP_MCU_TRIGONOMETRIC defined)
 * - DPFPU (Double-Precision FPU) for double-precision macros (if __DPFPU defined)
 *
 * @par References
 * - RX72N User's Manual (r01uh0805ej0140-rx72n.pdf) - RXv3 instruction set
 * - Renesas RX Family C/C++ Compiler Package User's Manual - CC-RX intrinsics
 * - GNU Toolchain for Renesas RX User Manual - GCC intrinsics
 * - IAR C/C++ Compiler for Renesas RX User Guide - ICCRX intrinsics
 *
 * @note Ported from Renesas Smart Configurator (SMC) generated code
 * @warning Some macros have compiler-specific performance characteristics
 * @warning TFU macros require TFU initialization (R_BSP_INIT_TFU) before use
 * @since Version 1.0.0
 */
/**********************************************************************************************************************
* History : DD.MM.YYYY Version  Description
*         : 28.02.2019 1.00     First Release
*         : 26.07.2019 1.10     Added the following function.
*                               - R_BSP_SINCOSF
*                               - R_BSP_ATAN2HYPOTF
*                               - R_BSP_CalcSine_Cosine
*                               - R_BSP_CalcAtan_SquareRoot
*         : 31.07.2019 1.11     Modified the compile condition of the below functions.
*                               - R_BSP_InitTFU
*                               - R_BSP_CalcSine_Cosine
*                               - R_BSP_CalcAtan_SquareRoot
*         : 08.10.2019 1.12     Modified the followind definition of intrinsic function of TFU for ICCRX.
*                               - R_BSP_INIT_TFU
*                               - R_BSP_SINCOSF
*                               - R_BSP_ATAN2HYPOTF
*         : 17.12.2019 1.13     Modified the comment of description.
*         : 28.02.2023 1.14     Modified the comment.
*                               Added the following function.
*                               - R_BSP_SINCOSFX
*                               - R_BSP_SINFX
*                               - R_BSP_COSFX
*                               - R_BSP_ATAN2HYPOTFX
*                               - R_BSP_ATAN2FX
*                               - R_BSP_HYPOTFX
*                               - R_BSP_CalcSine_Cosine_Fpn
*                               - R_BSP_CalcSine_Fpn
*                               - R_BSP_CalcCosine_Fpn
*                               - R_BSP_CalcAtan_SquareRoot_Fpn
*                               - R_BSP_CalcAtan_Fpn
*                               - R_BSP_CalcSquareRoot_Fpn
***********************************************************************************************************************/

#pragma once

/***********************************************************************************************************************
Includes   <System Includes> , "Project Includes"
***********************************************************************************************************************/
#include "platform.h"

/***********************************************************************************************************************
Macro definitions
***********************************************************************************************************************/

/* ---------- Maximum value and minimum value ---------- */
#if defined(__CCRX__)

/* signed long max(signed long data1, signed long data2) */
#define R_BSP_MAX(x, y) max((signed long)(x), (signed long)(y))
/* signed long min(signed long data1, signed long data2) */
#define R_BSP_MIN(x, y) min((signed long)(x), (signed long)(y))

#elif defined(__GNUC__)

/* signed long R_BSP_Max(signed long data1, signed long data2) (This macro uses API function of BSP.) */
#define R_BSP_MAX(x, y) R_BSP_Max((signed long)(x), (signed long)(y))
/* signed long R_BSP_Min(signed long data1, signed long data2) (This macro uses API function of BSP.) */
#define R_BSP_MIN(x, y) R_BSP_Min((signed long)(x), (signed long)(y))

#elif defined(__ICCRX__)

/* signed long   __MAX(signed long, signed long) */
#define R_BSP_MAX(x, y) __MAX((signed long)(x), (signed long)(y))
/* signed long   __MIN(signed long, signed long) */
#define R_BSP_MIN(x, y) __MIN((signed long)(x), (signed long)(y))

#endif

/* ---------- Byte switch ---------- */
#if defined(__CCRX__)

/* unsigned long revl(unsigned long data) */
#define R_BSP_REVL(x) revl((unsigned long)(x))
/* unsigned long revw(unsigned long data) */
#define R_BSP_REVW(x) revw((unsigned long)(x))

#elif defined(__GNUC__)

/* uint32_t __builtin_bswap32(uint32_t x) */
#define R_BSP_REVL(x) __builtin_bswap32((uint32_t)(x))
/* int __builtin_rx_revw(int) */
#define R_BSP_REVW(x) (unsigned long)__builtin_rx_revw((int)(x))

#elif defined(__ICCRX__)

/* unsigned long __REVL(unsigned long) */
#define R_BSP_REVL(x) __REVL((unsigned long)(x))
/* unsigned long __REVW(unsigned long) */
#define R_BSP_REVW(x) __REVW((unsigned long)(x))

#endif

/* ---------- Data Exchange ---------- */
#if defined(__CCRX__)

/* void xchg(signed long *data1, signed long *data2) */
#define R_BSP_EXCHANGE(x, y) xchg((signed long*)(x), (signed long*)(y))

#elif defined(__GNUC__)

/* void __builtin_rx_xchg (int *, int *) */
#define R_BSP_EXCHANGE(x, y) __builtin_rx_xchg((int*)(x), (int*)(y))

#elif defined(__ICCRX__)

/* void _builtin_xchg(signed long *, signed long *) */
#define R_BSP_EXCHANGE(x, y) _builtin_xchg((signed long*)(x), (signed long*)(y))

#endif

/* ---------- Multiply-and-accumulate operation ---------- */
#if defined(__CCRX__)

/* long long rmpab(long long init, unsigned long count, signed char *addr1, signed char *addr2) */
#define R_BSP_RMPAB(w, x, y, z)                                                                    \
  rmpab((long long)(w), (unsigned long)(x), (signed char*)(y), (signed char*)(z))
/* long long rmpaw(long long init, unsigned long count, short *addr1, short *addr2) */
#define R_BSP_RMPAW(w, x, y, z) rmpaw((long long)(w), (unsigned long)(x), (short*)(y), (short*)(z))
/* long long rmpal(long long init, unsigned long count, long *addr1, long *addr2) */
#define R_BSP_RMPAL(w, x, y, z) rmpal((long long)(w), (unsigned long)(x), (long*)(y), (long*)(z))

#elif defined(__GNUC__)

/* long long R_BSP_MulAndAccOperation_B(long long init, unsigned long count, const signed char *addr1, const signed char *addr2)
   (This macro uses API function of BSP.) */
#define R_BSP_RMPAB(w, x, y, z)                                                                    \
  R_BSP_MulAndAccOperation_B((long long)(w),                                                       \
                             (unsigned long)(x),                                                   \
                             (const signed char*)(y),                                              \
                             (const signed char*)(z))
/* long long R_BSP_MulAndAccOperation_W(long long init, unsigned long count, const short *addr1, const short *addr2)
   (This macro uses API function of BSP.) */
#define R_BSP_RMPAW(w, x, y, z)                                                                    \
  R_BSP_MulAndAccOperation_W((long long)(w),                                                       \
                             (unsigned long)(x),                                                   \
                             (const short*)(y),                                                    \
                             (const short*)(z))
/* long long R_BSP_MulAndAccOperation_L(long long init, unsigned long count, const long *addr1, const long *addr2)
   (This macro uses API function of BSP.) */
#define R_BSP_RMPAL(w, x, y, z)                                                                    \
  R_BSP_MulAndAccOperation_L((long long)(w), (unsigned long)(x), (const long*)(y), (const long*)(z))

#elif defined(__ICCRX__)

/* long long rmpab(long long init, unsigned long count, signed char *addr1, signed char *addr2) */
#define R_BSP_RMPAB(w, x, y, z)                                                                    \
  rmpab((long long)(w), (unsigned long)(x), (signed char*)(y), (signed char*)(z))
/* long long rmpaw(long long init, unsigned long count, short *addr1, short *addr2) */
#define R_BSP_RMPAW(w, x, y, z) rmpaw((long long)(w), (unsigned long)(x), (short*)(y), (short*)(z))
/* long long rmpal(long long init, unsigned long count, long *addr1, long *addr2) */
#define R_BSP_RMPAL(w, x, y, z) rmpal((long long)(w), (unsigned long)(x), (long*)(y), (long*)(z))

#endif

/* ---------- Rotation ---------- */
#if defined(__CCRX__)

/* unsigned long rolc(unsigned long data) */
#define R_BSP_ROLC(x) rolc((unsigned long)(x))
/* unsigned long rorc(unsigned long data) */
#define R_BSP_RORC(x) rorc((unsigned long)(x))
/* unsigned long rotl(unsigned long data, unsigned long num) */
#define R_BSP_ROTL(x, y) rotl((unsigned long)(x), (unsigned long)(y))
/* unsigned long rotr (unsigned long data, unsigned long num) */
#define R_BSP_ROTR(x, y) rotr((unsigned long)(x), (unsigned long)(y))

#elif defined(__GNUC__)

/* unsigned long R_BSP_RotateLeftWithCarry(unsigned long data) (This macro uses API function of BSP.) */
#define R_BSP_ROLC(x)    R_BSP_RotateLeftWithCarry((unsigned long)(x))
/* unsigned long R_BSP_RotateRightWithCarry(unsigned long data) (This macro uses API function of BSP.) */
#define R_BSP_RORC(x)    R_BSP_RotateRightWithCarry((unsigned long)(x))
/* unsigned long R_BSP_RotateLeft(unsigned long data, unsigned long num) (This macro uses API function of BSP.) */
#define R_BSP_ROTL(x, y) R_BSP_RotateLeft((unsigned long)(x), (unsigned long)(y))
/* unsigned long R_BSP_RotateRight (unsigned long data, unsigned long num) (This macro uses API function of BSP.) */
#define R_BSP_ROTR(x, y) R_BSP_RotateRight((unsigned long)(x), (unsigned long)(y))

#elif defined(__ICCRX__)

/* unsigned long __ROLC(unsigned long) */
#define R_BSP_ROLC(x)    __ROLC((unsigned long)(x))
/* unsigned long __RORC(unsigned long) */
#define R_BSP_RORC(x)    __RORC((unsigned long)(x))
/* unsigned long __ROTL(unsigned long, unsigned long) */
#define R_BSP_ROTL(x, y) __ROTL((unsigned long)(y), (unsigned long)(x))
/* unsigned long __ROTR(unsigned long, unsigned long) */
#define R_BSP_ROTR(x, y) __ROTR((unsigned long)(y), (unsigned long)(x))

#endif

/* ---------- Special Instructions ---------- */
#if defined(__CCRX__)

/* void brk(void) */
#define R_BSP_BRK() brk()
/* void int_exception(signed long num) */
#define R_BSP_INT(x) int_exception((signed long)(x))
/* void wait(void) */
#define R_BSP_WAIT() wait()
/* void nop(void) */
#define R_BSP_NOP() nop()

#elif defined(__GNUC__)

/* void __builtin_rx_brk (void)  */
#define R_BSP_BRK()  __builtin_rx_brk()
/* void __builtin_rx_int (int) */
#define R_BSP_INT(x) __builtin_rx_int((int)(x))
/* void __builtin_rx_wait (void) */
#define R_BSP_WAIT() __builtin_rx_wait()
/* __asm("nop") */
#define R_BSP_NOP()  __asm("nop")

#elif defined(__ICCRX__)

/* void __break(void) */
#define R_BSP_BRK()  __break()
/* void __software_interrupt(unsigned char) */
#define R_BSP_INT(x) __software_interrupt((unsigned char)(x))
/* void __wait_for_interrupt(void) */
#define R_BSP_WAIT() __wait_for_interrupt()
/* void __no_operation(void) */
#define R_BSP_NOP()  __no_operation()

#endif

/* ---------- Processor interrupt priority level (IPL) ---------- */
#if defined(__CCRX__)

/* void set_ipl(signed long level) */
#define R_BSP_SET_IPL(x) set_ipl((signed long)(x))
/* unsigned char get_ipl(void) */
#define R_BSP_GET_IPL() get_ipl()

#elif defined(__GNUC__)

/* void __builtin_rx_mvtipl (int) */
#define R_BSP_SET_IPL(x) __builtin_rx_mvtipl((int)(x))
/* uint32_t R_BSP_CpuInterruptLevelRead (void) (This macro uses API function of BSP.) */
#define R_BSP_GET_IPL()  (unsigned char)R_BSP_CpuInterruptLevelRead()

#elif defined(__ICCRX__)

/* void __set_interrupt_level(__ilevel_t) */
#define R_BSP_SET_IPL(x) __set_interrupt_level((__ilevel_t)(x))
/* __ilevel_t __get_interrupt_level(void) */
#define R_BSP_GET_IPL()  (unsigned char)__get_interrupt_level()

#endif

/* ---------- Processor status word (PSW) ---------- */
#if defined(__CCRX__)

/* void set_psw(unsigned long data) */
#define R_BSP_SET_PSW(x) set_psw((unsigned long)(x))
/* unsigned long get_psw(void) */
#define R_BSP_GET_PSW() get_psw()

#elif defined(__GNUC__)

/* void __builtin_rx_mvtc (int reg, int val) */
#define R_BSP_SET_PSW(x) __builtin_rx_mvtc(0x0, (int)(x))
/* int __builtin_rx_mvfc (int) */
#define R_BSP_GET_PSW()  (unsigned long)__builtin_rx_mvfc(0x0)

#elif defined(__ICCRX__)

/* void __set_PSW_register(unsigned long) */
#define R_BSP_SET_PSW(x) __set_PSW_register((unsigned long)(x))
/* unsigned long __get_PSW_register(void) */
#define R_BSP_GET_PSW()  __get_PSW_register()

#endif

/* ---------- Floating-point status word (FPSW) ---------- */
#ifdef __FPU
#if defined(__CCRX__)

/* void set_fpsw(unsigned long data) */
#define R_BSP_SET_FPSW(x) set_fpsw((unsigned long)(x))
/* unsigned long get_fpsw(void) */
#define R_BSP_GET_FPSW() get_fpsw()

#elif defined(__GNUC__)

/* void __builtin_rx_mvtc (int reg, int val) */
#define R_BSP_SET_FPSW(x) __builtin_rx_mvtc(0x3, (int)(x))
/* int __builtin_rx_mvfc (int) */
#define R_BSP_GET_FPSW()  (unsigned long)__builtin_rx_mvfc(0x3)

#elif defined(__ICCRX__)

/* void __set_FPSW_register(unsigned long) */
#define R_BSP_SET_FPSW(x) __set_FPSW_register((unsigned long)(x))
/* unsigned long __get_FPSW_register(void) */
#define R_BSP_GET_FPSW()  __get_FPSW_register()

#endif
#endif

/* ---------- User Stack Pointer (USP) ---------- */
#if defined(__CCRX__)

/* void set_usp(void *data) */
#define R_BSP_SET_USP(x) set_usp((void*)(x))
/* void *get_usp(void) */
#define R_BSP_GET_USP() get_usp()

#elif defined(__GNUC__)

/* void __builtin_rx_mvtc (int reg, int val) */
#define R_BSP_SET_USP(x) __builtin_rx_mvtc(0x2, (int)(x))
/* int __builtin_rx_mvfc (int) */
#define R_BSP_GET_USP()  (void*)__builtin_rx_mvfc(0x2)

#elif defined(__ICCRX__)

/* void __set_USP_register(unsigned long) */
#define R_BSP_SET_USP(x) __set_USP_register((unsigned long)(x))
/* unsigned long __get_USP_register(void) */
#define R_BSP_GET_USP()  (void*)__get_USP_register()

#endif

/* ---------- Interrupt Stack Pointer (ISP) ---------- */
#if defined(__CCRX__)

/* void set_isp(void *data) */
#define R_BSP_SET_ISP(x) set_isp((void*)(x))
/* void *get_isp(void) */
#define R_BSP_GET_ISP() get_isp()

#elif defined(__GNUC__)

/* void __builtin_rx_mvtc (int reg, int val) */
#define R_BSP_SET_ISP(x) __builtin_rx_mvtc(0xA, (int)(x))
/* int __builtin_rx_mvfc (int) */
#define R_BSP_GET_ISP()  (void*)__builtin_rx_mvfc(0xA)

#elif defined(__ICCRX__)

/* void __set_ISP_register(unsigned long) */
#define R_BSP_SET_ISP(x) __set_ISP_register((unsigned long)(x))
/* unsigned long __get_ISP_register(void) */
#define R_BSP_GET_ISP()  (void*)__get_ISP_register()

#endif

/* ---------- Interrupt Table Register (INTB) ---------- */
#if defined(__CCRX__)

/* void set_intb(void *data) */
#define R_BSP_SET_INTB(x) set_intb((void*)(x))
/* void *get_intb(void) */
#define R_BSP_GET_INTB() get_intb()

#elif defined(__GNUC__)

/* void __builtin_rx_mvtc (int reg, int val) */
#define R_BSP_SET_INTB(x) __builtin_rx_mvtc(0xC, (int)(x))
/* int __builtin_rx_mvfc (int) */
#define R_BSP_GET_INTB()  (void*)__builtin_rx_mvfc(0xC)

#elif defined(__ICCRX__)

/* void __set_interrupt_table(unsigned long address) */
#define R_BSP_SET_INTB(x) __set_interrupt_table((unsigned long)(x))
/* unsigned long __get_interrupt_table(void); */
#define R_BSP_GET_INTB()  (void*)__get_interrupt_table()

#endif

/* ---------- Backup PSW (BPSW) ---------- */
#if defined(__CCRX__)

/* void set_bpsw(unsigned long data) */
#define R_BSP_SET_BPSW(x) set_bpsw((unsigned long)(x))
/* unsigned long get_bpsw(void) */
#define R_BSP_GET_BPSW() get_bpsw()

#elif defined(__GNUC__)

/* void __builtin_rx_mvtc (int reg, int val) */
#define R_BSP_SET_BPSW(x) __builtin_rx_mvtc(0x8, (int)(x))
/* int __builtin_rx_mvfc (int) */
#define R_BSP_GET_BPSW()  (unsigned long)__builtin_rx_mvfc(0x8)

#elif defined(__ICCRX__)

/* void R_BSP_SetBPSW(uint32_t data) (This macro uses API function of BSP.) */
#define R_BSP_SET_BPSW(x) R_BSP_SetBPSW((uint32_t)(x))
/* uint32_t R_BSP_GetBPSW(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_BPSW()  R_BSP_GetBPSW()

#endif

/* ---------- Backup PC (BPC) ---------- */
#if defined(__CCRX__)

/* void set_bpc(void *data) */
#define R_BSP_SET_BPC(x) set_bpc((void*)(x))
/* void *get_bpc(void) */
#define R_BSP_GET_BPC() get_bpc()

#elif defined(__GNUC__)

/* void __builtin_rx_mvtc (int reg, int val) */
#define R_BSP_SET_BPC(x) __builtin_rx_mvtc(0x9, (int)(x))
/* int __builtin_rx_mvfc (int) */
#define R_BSP_GET_BPC()  (void*)__builtin_rx_mvfc(0x9)

#elif defined(__ICCRX__)

/* void R_BSP_SetBPC(void * data) (This macro uses API function of BSP.) */
#define R_BSP_SET_BPC(x) R_BSP_SetBPC((void*)(x))
/* void *R_BSP_GetBPC(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_BPC()  R_BSP_GetBPC()

#endif

/* ---------- Fast Interrupt Vector Register (FINTV) ---------- */
#if defined(__CCRX__)

/* void set_fintv(void *data) */
#define R_BSP_SET_FINTV(x) set_fintv((void*)(x))
/* void *get_fintv(void) */
#define R_BSP_GET_FINTV() get_fintv()

#elif defined(__GNUC__)

/* void __builtin_rx_mvtc (int reg, int val) */
#define R_BSP_SET_FINTV(x) __builtin_rx_mvtc(0xB, (int)(x))
/* int __builtin_rx_mvfc (int) */
#define R_BSP_GET_FINTV()  (void*)__builtin_rx_mvfc(0xB)

#elif defined(__ICCRX__)

/* void __set_FINTV_register(__fast_int_f) */
#define R_BSP_SET_FINTV(x) __set_FINTV_register((__fast_int_f)(x))
/* __fast_int_f __get_FINTV_register(void) */
#define R_BSP_GET_FINTV()  (void*)__get_FINTV_register()

#endif

/* ---------- Significant 64-bit multiplication ---------- */
#if defined(__CCRX__)

/* signed long long emul(signed long data1, signed long data2) */
#define R_BSP_EMUL(x, y) emul((signed long)(x), (signed long)(y))
/* unsigned long long emulu(unsigned long data1, unsigned long data2) */
#define R_BSP_EMULU(x, y) emulu((unsigned long)(x), (unsigned long)(y))

#elif defined(__GNUC__)

/* signed long long R_BSP_SignedMultiplication(signed long data1, signed long data2)
   (This macro uses API function of BSP.) */
#define R_BSP_EMUL(x, y)  R_BSP_SignedMultiplication((signed long)(x), (signed long)(y))
/* unsigned long long R_BSP_UnsignedMultiplication(unsigned long data1, unsigned long data2)
   (This macro uses API function of BSP.) */
#define R_BSP_EMULU(x, y) R_BSP_UnsignedMultiplication((unsigned long)(x), (unsigned long)(y))

#elif defined(__ICCRX__)

/* signed long long R_BSP_SignedMultiplication(signed long data1, signed long data2)
   (This macro uses API function of BSP.) */
#define R_BSP_EMUL(x, y)  R_BSP_SignedMultiplication((signed long)(x), (signed long)(y))
/* unsigned long long R_BSP_UnsignedMultiplication(unsigned long data1, unsigned long data2)
   (This macro uses API function of BSP.) */
#define R_BSP_EMULU(x, y) R_BSP_UnsignedMultiplication((unsigned long)(x), (unsigned long)(y))

#endif

/* ---------- Processor mode (PM) ---------- */
#if defined(__CCRX__)

/* void chg_pmusr(void) */
#define R_BSP_CHG_PMUSR() chg_pmusr()

#elif defined(__GNUC__)

/* void R_BSP_ChangeToUserMode(void) (This macro uses API function of BSP.) */
#define R_BSP_CHG_PMUSR() R_BSP_ChangeToUserMode()

#elif defined(__ICCRX__)

/* void R_BSP_ChangeToUserMode(void) (This macro uses API function of BSP.) */
#define R_BSP_CHG_PMUSR() R_BSP_ChangeToUserMode()

#endif

/* ---------- Accumulator (ACC) ---------- */
#if defined(__CCRX__)

/* void set_acc(signed long long data) */
#define R_BSP_SET_ACC(x) set_acc((signed long long)(x))
/* signed long long get_acc(void) */
#define R_BSP_GET_ACC() get_acc()

#elif defined(__GNUC__)

/* void R_BSP_SetACC(signed long long data) (This macro uses API function of BSP.) */
#define R_BSP_SET_ACC(x) R_BSP_SetACC((signed long long)(x))
/* signed long long R_BSP_GetACC(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_ACC()  R_BSP_GetACC()

#elif defined(__ICCRX__)

/* void R_BSP_SetACC(signed long long data) (This macro uses API function of BSP.) */
#define R_BSP_SET_ACC(x) R_BSP_SetACC((signed long long)(x))
/* signed long long R_BSP_GetACC(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_ACC()  R_BSP_GetACC()

#endif

/* ---------- Control of the interrupt enable bits ---------- */
#if defined(__CCRX__)

/* void setpsw_i(void) */
#define R_BSP_SETPSW_I() setpsw_i()
/* void clrpsw_i(void) */
#define R_BSP_CLRPSW_I() clrpsw_i()

#elif defined(__GNUC__)

/* void __builtin_rx_setpsw (int) */
#define R_BSP_SETPSW_I() __builtin_rx_setpsw('I')
/* void __builtin_rx_clrpsw (int) */
#define R_BSP_CLRPSW_I() __builtin_rx_clrpsw('I')

#elif defined(__ICCRX__)

/* void __enable_interrupt(void) */
#define R_BSP_SETPSW_I() __enable_interrupt()
/* void __disable_interrupt(void) */
#define R_BSP_CLRPSW_I() __disable_interrupt()

#endif

/* ---------- Multiply-and-accumulate operation ---------- */
#if defined(__CCRX__)

/* long macl(short *data1, short *data2, unsigned long count) */
#define R_BSP_MACL(x, y, z) macl((short*)(x), (short*)(y), (unsigned long)(z))
/* short macw1(short *data1, short *data2, unsigned long count) */
#define R_BSP_MACW1(x, y, z) macw1((short*)(x), (short*)(y), (unsigned long)(z))
/* short macw2(short *data1, short *data2, unsigned long count) */
#define R_BSP_MACW2(x, y, z) macw2((short*)(x), (short*)(y), (unsigned long)(z))

#elif defined(__GNUC__)

/* long R_BSP_MulAndAccOperation_2byte(const short *data1, const short *data2, unsigned long count)
   (This macro uses API function of BSP.) */
#define R_BSP_MACL(x, y, z)                                                                        \
  R_BSP_MulAndAccOperation_2byte((const short*)(x), (const short*)(y), (unsigned long)(z))
/* short R_BSP_MulAndAccOperation_FixedPoint1(const short *data1, const short *data2, unsigned long count)
   (This macro uses API function of BSP.) */
#define R_BSP_MACW1(x, y, z)                                                                       \
  R_BSP_MulAndAccOperation_FixedPoint1((const short*)(x), (const short*)(y), (unsigned long)(z))
/* short R_BSP_MulAndAccOperation_FixedPoint2(const short *data1, const short *data2, unsigned long count)
   (This macro uses API function of BSP.) */
#define R_BSP_MACW2(x, y, z)                                                                       \
  R_BSP_MulAndAccOperation_FixedPoint2((const short*)(x), (const short*)(y), (unsigned long)(z))

#elif defined(__ICCRX__)

/* long __macl(short * data1, short * data2, unsigned long count) */
#define R_BSP_MACL(x, y, z)  __macl((short*)(x), (short*)(y), (unsigned long)(z))
/* short __macw1(short * data1, short * data2, unsigned long count) */
#define R_BSP_MACW1(x, y, z) __macw1((short*)(x), (short*)(y), (unsigned long)(z))
/* short __macw2(short * data1, short * data2, unsigned long count) */
#define R_BSP_MACW2(x, y, z) __macw2((short*)(x), (short*)(y), (unsigned long)(z))

#endif

/* ---------- Exception Table Register (EXTB) ---------- */
#ifdef BSP_MCU_EXCEPTION_TABLE
#if defined(__CCRX__)

/* void set_extb(void *data) */
#define R_BSP_SET_EXTB(x) set_extb((void*)(x))
/* void *get_extb(void) */
#define R_BSP_GET_EXTB() get_extb()

#elif defined(__GNUC__)

/* void __builtin_rx_mvtc (int reg, int val) */
#define R_BSP_SET_EXTB(x) __builtin_rx_mvtc(0xD, (int)(x))
/* int __builtin_rx_mvfc (int) */
#define R_BSP_GET_EXTB()  (void*)__builtin_rx_mvfc(0xD)

#elif defined(__ICCRX__)

/* void R_BSP_SetEXTB(void * data) (This macro uses API function of BSP.) */
#define R_BSP_SET_EXTB(x) R_BSP_SetEXTB((void*)(x))
/* void *R_BSP_GetEXTB(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_EXTB()  R_BSP_GetEXTB()

#endif
#endif

/* ---------- Bit Manipulation ---------- */
#if defined(__CCRX__)

/* void __bclr(unsigned char *data, unsigned long bit) */
#define R_BSP_BIT_CLEAR(x, y) __bclr((unsigned char*)(x), (unsigned long)(y))
/* void __bset(unsigned char *data, unsigned long bit) */
#define R_BSP_BIT_SET(x, y) __bset((unsigned char*)(x), (unsigned long)(y))
/* void __bnot(unsigned char *data, unsigned long bit) */
#define R_BSP_BIT_REVERSE(x, y) __bnot((unsigned char*)(x), (unsigned long)(y))

#elif defined(__GNUC__)

/* void R_BSP_BitClear(uint8_t *data, uint32_t bit) (This macro uses API function of BSP.) */
#define R_BSP_BIT_CLEAR(x, y)   R_BSP_BitClear((uint8_t*)(x), (uint32_t)(y))
/* void R_BSP_BitSet(uint8_t *data, uint32_t bit) (This macro uses API function of BSP.) */
#define R_BSP_BIT_SET(x, y)     R_BSP_BitSet((uint8_t*)(x), (uint32_t)(y))
/* void R_BSP_BitReverse(uint8_t *data, uint32_t bit) (This macro uses API function of BSP.) */
#define R_BSP_BIT_REVERSE(x, y) R_BSP_BitReverse((uint8_t*)(x), (uint32_t)(y))

#elif defined(__ICCRX__)

/* void R_BSP_BitClear(uint8_t *data, uint32_t bit) (This macro uses API function of BSP.) */
#define R_BSP_BIT_CLEAR(x, y)   R_BSP_BitClear((uint8_t*)(x), (uint32_t)(y))
/* void R_BSP_BitSet(uint8_t *data, uint32_t bit) (This macro uses API function of BSP.) */
#define R_BSP_BIT_SET(x, y)     R_BSP_BitSet((uint8_t*)(x), (uint32_t)(y))
/* void R_BSP_BitReverse(uint8_t *data, uint32_t bit) (This macro uses API function of BSP.) */
#define R_BSP_BIT_REVERSE(x, y) R_BSP_BitReverse((uint8_t*)(x), (uint32_t)(y))

#endif

#ifdef BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT
#ifdef __DPFPU
/* ---------- Double-Precision Floating-Point Status Word (DPSW) ---------- */
#if defined(__CCRX__)

/* void set_dpsw(unsigned long data) */
#define R_BSP_SET_DPSW(x) __set_dpsw((unsigned long)(x))
/* unsigned long get_dpsw(void) */
#define R_BSP_GET_DPSW() __get_dpsw()

#elif defined(__GNUC__)

/* void R_BSP_SetDPSW(uint32_t data) (This macro uses API function of BSP.) */
#define R_BSP_SET_DPSW(x) R_BSP_SetDPSW((uint32_t)(x))
/* uint32_t R_BSP_GetDPSW(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_DPSW()  R_BSP_GetDPSW()

#elif defined(__ICCRX__)

/* void R_BSP_SetDPSW(uint32_t data) (This macro uses API function of BSP.) */
#define R_BSP_SET_DPSW(x) R_BSP_SetDPSW((uint32_t)(x))
/* uint32_t R_BSP_GetDPSW(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_DPSW()  R_BSP_GetDPSW()

#endif

/* ---------- Double-precision floating-point exception handling operation control register (DECNT) ---------- */
#if defined(__CCRX__)

/* void __set_decnt(unsigned long data) */
#define R_BSP_SET_DECNT(x) __set_decnt((unsigned long)(x))
/* unsigned long __get_decnt(void) */
#define R_BSP_GET_DECNT() __get_decnt()

#elif defined(__GNUC__)

/* void R_BSP_SetDECNT(uint32_t data) (This macro uses API function of BSP.) */
#define R_BSP_SET_DECNT(x) R_BSP_SetDECNT((uint32_t)(x))
/* uint32_t R_BSP_GetDECNT(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_DECNT()  R_BSP_GetDECNT()

#elif defined(__ICCRX__)

/* void R_BSP_SetDECNT(uint32_t data) (This macro uses API function of BSP.) */
#define R_BSP_SET_DECNT(x) R_BSP_SetDECNT((uint32_t)(x))
/* uint32_t R_BSP_GetDECNT(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_DECNT()  R_BSP_GetDECNT()

#endif

/* ---------- Double-precision floating-point exception program counter (DEPC) ---------- */
#if defined(__CCRX__)

/* void *__get_depc(void) */
#define R_BSP_GET_DEPC() __get_depc()

#elif defined(__GNUC__)

/* void *R_BSP_GetDEPC(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_DEPC() R_BSP_GetDEPC()

#elif defined(__ICCRX__)

/* void *R_BSP_GetDEPC(void) (This macro uses API function of BSP.) */
#define R_BSP_GET_DEPC() R_BSP_GetDEPC()

#endif
#endif /* __DPFPU */
#endif /* BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT */

/* ---------- Initializes the trigonometric function unit. ---------- */
#ifdef BSP_MCU_TRIGONOMETRIC
#if BSP_MCU_TFU_VERSION == 1
#if defined(__CCRX__)

/* void __init_tfu(void) */
#define R_BSP_INIT_TFU() __init_tfu()

#elif defined(__GNUC__)

/* void R_BSP_InitTFU(void) (This macro uses API function of BSP.) */
#define R_BSP_INIT_TFU() R_BSP_InitTFU()

#elif defined(__ICCRX__)

/* Invalid for ICCRX.
   Because the initilaze function of TFU is called automatically when the TFU function is called. */
#define R_BSP_INIT_TFU()
#endif /* BSP_MCU_TFU_VERSION == 1 */
#endif

/* ---------- Uses the trigonometric function unit to calculate the sine and cosine of an angle at the same time.
   (single precision) ---------- */
#if defined(__CCRX__)

/* void __sincosf(float f, float *sin, float *cos) */
#define R_BSP_SINCOSF(x, y, z) __sincosf((float)(x), (float*)(y), (float*)(z))

#elif defined(__GNUC__)

/* void R_BSP_CalcSine_Cosine(float f, float *sin, float *cos) (This macro uses API function of BSP.) */
#define R_BSP_SINCOSF(x, y, z) R_BSP_CalcSine_Cosine((float)(x), (float*)(y), (float*)(z))

#elif defined(__ICCRX__)

/* void  __sincosf(float _F, float *dstSin, float *dstCos) */
#define R_BSP_SINCOSF(x, y, z) __sincosf((float)(x), (float*)(y), (float*)(z))

#endif

/* ---------- Uses the trigonometric function unit to calculate the arc tangent of x and y and the square root of the 
   sum of squares of these values at the same time. (single precision) ---------- */
#if defined(__CCRX__)

/* void __atan2hypotf(float y, float x, float *atan2, float *hypot) */
#define R_BSP_ATAN2HYPOTF(w, x, y, z)                                                              \
  __atan2hypotf((float)(w), (float)(x), (float*)(y), (float*)(z))

#elif defined(__GNUC__)

/* void R_BSP_CalcAtan_SquareRoot(float y, float x, float *atan2, float *hypot)
   (This macro uses API function of BSP.) */
#define R_BSP_ATAN2HYPOTF(w, x, y, z)                                                              \
  R_BSP_CalcAtan_SquareRoot((float)(w), (float)(x), (float*)(y), (float*)(z))

#elif defined(__ICCRX__)

/* void  __atan2hypotf(float _Y, float _X, float *dstAtan2, float *dstHypot) */
#define R_BSP_ATAN2HYPOTF(w, x, y, z)                                                              \
  __atan2hypotf((float)(w), (float)(x), (float*)(y), (float*)(z))

#endif

#if BSP_MCU_TFU_VERSION == 2
/* ---------- Uses the trigonometric function unit to calculate the sine and cosine of an angle.
   (fixed-point numbers) ---------- */
#if defined(__CCRX__)

#if __RENESAS_VERSION__ >= 0x03050000
/* void __sincosfx(signed long fx, signed long *sin, signed long *cos) */
#define R_BSP_SINCOSFX(x, y, z) __sincosfx((int32_t)(x), (int32_t*)(y), (int32_t*)(z))
#else
#define R_BSP_SINCOSFX(x, y, z)
#endif

#elif defined(__GNUC__)

/* void R_BSP_CalcSine_Cosine_Fpn(int32_t fx, int32_t *sin, int32_t *cos) (This macro uses API function of BSP.) */
#define R_BSP_SINCOSFX(x, y, z)                                                                    \
  R_BSP_CalcSine_Cosine_Fpn((int32_t)(x), (int32_t*)(y), (int32_t*)(z))

#elif defined(__ICCRX__)

/* void R_BSP_CalcSine_Cosine_Fpn(int32_t fx, int32_t *sin, int32_t *cos) (This macro uses API function of BSP.) */
#define R_BSP_SINCOSFX(x, y, z)                                                                    \
  R_BSP_CalcSine_Cosine_Fpn((int32_t)(x), (int32_t*)(y), (int32_t*)(z))

#endif

/* ---------- Uses the trigonometric function unit to calculate the sine of an angle. (fixed-point numbers)
   ---------- */
#if defined(__CCRX__)

#if __RENESAS_VERSION__ >= 0x03050000
/* signed long __sinfx(signed long fx) */
#define R_BSP_SINFX(x) __sinfx((int32_t)(x))
#else
#define R_BSP_SINFX(x)
#endif

#elif defined(__GNUC__)

/* int32_t R_BSP_CalcSine_Fpn(int32_t fx) (This macro uses API function of BSP.) */
#define R_BSP_SINFX(x) R_BSP_CalcSine_Fpn((int32_t)(x))

#elif defined(__ICCRX__)

/* int32_t R_BSP_CalcSine_Fpn(int32_t fx) (This macro uses API function of BSP.) */
#define R_BSP_SINFX(x) R_BSP_CalcSine_Fpn((int32_t)(x))

#endif

/* ---------- Uses the trigonometric function unit to calculate the cosine of an angle. (fixed-point numbers)
   ---------- */
#if defined(__CCRX__)

#if __RENESAS_VERSION__ >= 0x03050000
/* signed long __cosfx(signed long fx) */
#define R_BSP_COSFX(x) __cosfx((int32_t)(x))
#else
#define R_BSP_COSFX(x)
#endif

#elif defined(__GNUC__)

/* int32_t R_BSP_CalcCosine_Fpn(int32_t fx) (This macro uses API function of BSP.) */
#define R_BSP_COSFX(x) R_BSP_CalcCosine_Fpn((int32_t)(x))

#elif defined(__ICCRX__)

/* int32_t R_BSP_CalcCosine_Fpn(int32_t fx) (This macro uses API function of BSP.) */
#define R_BSP_COSFX(x) R_BSP_CalcCosine_Fpn((int32_t)(x))

#endif

/* ---------- Uses the trigonometric function unit to calculate the arc tangent of x and y and the square root of the 
   sum of squares of these values. (fixed-point numbers) ---------- */
#if defined(__CCRX__)

#if __RENESAS_VERSION__ >= 0x03050000
/* __atan2hypotfx(signed long y, signed long x, signed long *atan2, signed long *hypot) */
#define R_BSP_ATAN2HYPOTFX(w, x, y, z)                                                             \
  __atan2hypotfx((int32_t)(w), (int32_t)(x), (int32_t*)(y), (int32_t*)(z))
#else
#define R_BSP_ATAN2HYPOTFX(w, x, y, z)
#endif

#elif defined(__GNUC__)

/* void R_BSP_CalcAtan_SquareRoot_Fpn(int32_t y, int32_t x, int32_t *atan2, int32_t *hypot)
   (This macro uses API function of BSP.) */
#define R_BSP_ATAN2HYPOTFX(w, x, y, z)                                                             \
  R_BSP_CalcAtan_SquareRoot_Fpn((int32_t)(w), (int32_t)(x), (int32_t*)(y), (int32_t*)(z))

#elif defined(__ICCRX__)

/* void R_BSP_CalcAtan_SquareRoot_Fpn(int32_t y, int32_t x, int32_t *atan2, int32_t *hypot)
   (This macro uses API function of BSP.) */
#define R_BSP_ATAN2HYPOTFX(w, x, y, z)                                                             \
  R_BSP_CalcAtan_SquareRoot_Fpn((int32_t)(w), (int32_t)(x), (int32_t*)(y), (int32_t*)(z))

#endif

/* ---------- Uses the trigonometric function unit to calculate the arc tangent of x and y. (fixed-point numbers)
   ---------- */
#if defined(__CCRX__)

#if __RENESAS_VERSION__ >= 0x03050000
/* signed long __atan2fx(signed long y, signed long x) */
#define R_BSP_ATAN2FX(x, y) __atan2fx((int32_t)(x), (int32_t)(y))
#else
#define R_BSP_ATAN2FX(x, y)
#endif

#elif defined(__GNUC__)

/* int32_t R_BSP_CalcAtan_Fpn(int32_t y, int32_t x) (This macro uses API function of BSP.) */
#define R_BSP_ATAN2FX(x, y) R_BSP_CalcAtan_Fpn((int32_t)(x), (int32_t)(y))

#elif defined(__ICCRX__)

/* int32_t R_BSP_CalcAtan_Fpn(int32_t y, int32_t x) (This macro uses API function of BSP.) */
#define R_BSP_ATAN2FX(x, y) R_BSP_CalcAtan_Fpn((int32_t)(x), (int32_t)(y))

#endif

/* ---------- Uses the trigonometric function unit to calculate the square root of the 
   sum of squares of x and y. (fixed-point numbers) ---------- */
#if defined(__CCRX__)

#if __RENESAS_VERSION__ >= 0x03050000
/* signed long __hypotfx(signed long x, signed long y) */
#define R_BSP_HYPOTFX(x, y) __hypotfx((int32_t)(x), (int32_t)(y))
#else
#define R_BSP_HYPOTFX(x, y)
#endif

#elif defined(__GNUC__)

/* int32_t R_BSP_CalcSquareRoot_Fpn(int32_t x, int32_t y) (This macro uses API function of BSP.) */
#define R_BSP_HYPOTFX(x, y) R_BSP_CalcSquareRoot_Fpn((int32_t)(x), (int32_t)(y))

#elif defined(__ICCRX__)

/* int32_t R_BSP_CalcSquareRoot_Fpn(int32_t x, int32_t y) (This macro uses API function of BSP.) */
#define R_BSP_HYPOTFX(x, y) R_BSP_CalcSquareRoot_Fpn((int32_t)(x), (int32_t)(y))

#endif

#endif /* BSP_MCU_TFU_VERSION == 2 */
#endif /* BSP_MCU_TRIGONOMETRIC */

/***********************************************************************************************************************
Exported global variables
***********************************************************************************************************************/

/***********************************************************************************************************************
Exported global functions (to be accessed by other files)
***********************************************************************************************************************/

/* ==== GNUC-specific function implementations ==== */
#if defined(__GNUC__)

/**
 * @brief Get maximum of two signed long values
 * @param[in] data1 First value
 * @param[in] data2 Second value
 * @return signed long Maximum value (data1 or data2)
 * @note GNUC implementation - CC-RX and ICCRX use intrinsic
 * @since Version 1.0.0
 */
signed long R_BSP_Max(signed long data1, signed long data2);

/**
 * @brief Get minimum of two signed long values
 * @param[in] data1 First value
 * @param[in] data2 Second value
 * @return signed long Minimum value (data1 or data2)
 * @note GNUC implementation - CC-RX and ICCRX use intrinsic
 * @since Version 1.0.0
 */
signed long R_BSP_Min(signed long data1, signed long data2);

/**
 * @brief Multiply-and-accumulate operation on byte arrays
 * @param[in] init Initial accumulator value
 * @param[in] count Number of elements to process
 * @param[in] addr1 First array (signed char)
 * @param[in] addr2 Second array (signed char)
 * @return long long Accumulated result: init + sum(addr1[i] * addr2[i])
 * @note GNUC implementation using inline assembly
 * @since Version 1.0.0
 */
long long R_BSP_MulAndAccOperation_B(long long          init,
                                     unsigned long      count,
                                     const signed char* addr1,
                                     const signed char* addr2);

/**
 * @brief Multiply-and-accumulate operation on word arrays
 * @param[in] init Initial accumulator value
 * @param[in] count Number of elements to process
 * @param[in] addr1 First array (short)
 * @param[in] addr2 Second array (short)
 * @return long long Accumulated result: init + sum(addr1[i] * addr2[i])
 * @note GNUC implementation using inline assembly
 * @since Version 1.0.0
 */
long long R_BSP_MulAndAccOperation_W(long long     init,
                                     unsigned long count,
                                     const short*  addr1,
                                     const short*  addr2);

/**
 * @brief Multiply-and-accumulate operation on long arrays
 * @param[in] init Initial accumulator value
 * @param[in] count Number of elements to process
 * @param[in] addr1 First array (long)
 * @param[in] addr2 Second array (long)
 * @return long long Accumulated result: init + sum(addr1[i] * addr2[i])
 * @note GNUC implementation using inline assembly
 * @since Version 1.0.0
 */
long long R_BSP_MulAndAccOperation_L(long long     init,
                                     unsigned long count,
                                     const long*   addr1,
                                     const long*   addr2);

/**
 * @brief Rotate left with carry flag
 * @param[in] data Value to rotate
 * @return unsigned long Rotated value (includes carry flag)
 * @note GNUC implementation - CC-RX/ICCRX use intrinsic
 * @since Version 1.0.0
 */
unsigned long R_BSP_RotateLeftWithCarry(unsigned long data);

/**
 * @brief Rotate right with carry flag
 * @param[in] data Value to rotate
 * @return unsigned long Rotated value (includes carry flag)
 * @note GNUC implementation - CC-RX/ICCRX use intrinsic
 * @since Version 1.0.0
 */
unsigned long R_BSP_RotateRightWithCarry(unsigned long data);

/**
 * @brief Rotate left by specified number of bits
 * @param[in] data Value to rotate
 * @param[in] num Number of bits to rotate (0-31)
 * @return unsigned long Rotated value
 * @note GNUC implementation - CC-RX/ICCRX use intrinsic
 * @since Version 1.0.0
 */
unsigned long R_BSP_RotateLeft(unsigned long data, unsigned long num);

/**
 * @brief Rotate right by specified number of bits
 * @param[in] data Value to rotate
 * @param[in] num Number of bits to rotate (0-31)
 * @return unsigned long Rotated value
 * @note GNUC implementation - CC-RX/ICCRX use intrinsic
 * @since Version 1.0.0
 */
unsigned long R_BSP_RotateRight(unsigned long data, unsigned long num);

/**
 * @brief Multiply-and-accumulate with 2-byte operands
 * @param[in] data1 First array
 * @param[in] data2 Second array
 * @param[in] count Number of elements
 * @return long Accumulated product sum
 * @note GNUC implementation using inline assembly
 * @since Version 1.01
 */
long R_BSP_MulAndAccOperation_2byte(const short* data1, const short* data2, unsigned long count);

/**
 * @brief Multiply-and-accumulate with fixed-point format 1
 * @param[in] data1 First array
 * @param[in] data2 Second array
 * @param[in] count Number of elements
 * @return short Fixed-point result
 * @note GNUC implementation using inline assembly
 * @since Version 1.01
 */
short R_BSP_MulAndAccOperation_FixedPoint1(const short*  data1,
                                           const short*  data2,
                                           unsigned long count);

/**
 * @brief Multiply-and-accumulate with fixed-point format 2
 * @param[in] data1 First array
 * @param[in] data2 Second array
 * @param[in] count Number of elements
 * @return short Fixed-point result
 * @note GNUC implementation using inline assembly
 * @since Version 1.01
 */
short R_BSP_MulAndAccOperation_FixedPoint2(const short*  data1,
                                           const short*  data2,
                                           unsigned long count);

#endif /* defined(__GNUC__) */

/* ==== GNUC and ICCRX shared function implementations ==== */
#if defined(__GNUC__) || defined(__ICCRX__)

/**
 * @brief 64-bit signed multiplication (EMUL instruction)
 * @param[in] data1 First operand
 * @param[in] data2 Second operand
 * @return signed long long 64-bit product
 * @note GNUC/ICCRX implementation - CC-RX uses intrinsic
 * @since Version 1.0.0
 */
signed long long R_BSP_SignedMultiplication(signed long data1, signed long data2);

/**
 * @brief 64-bit unsigned multiplication (EMULU instruction)
 * @param[in] data1 First operand
 * @param[in] data2 Second operand
 * @return unsigned long long 64-bit product
 * @note GNUC/ICCRX implementation - CC-RX uses intrinsic
 * @since Version 1.0.0
 */
unsigned long long R_BSP_UnsignedMultiplication(unsigned long data1, unsigned long data2);

/**
 * @brief Set accumulator register (ACC)
 * @param[in] data 64-bit value to write to ACC
 * @note GNUC/ICCRX implementation using inline assembly
 * @since Version 1.0.0
 */
void R_BSP_SetACC(signed long long data);

/**
 * @brief Get accumulator register (ACC)
 * @return signed long long Current ACC value
 * @note GNUC/ICCRX implementation using inline assembly
 * @since Version 1.0.0
 */
signed long long R_BSP_GetACC(void);

#endif /* defined(__GNUC__) || defined(__ICCRX__)  */

/* ==== Common function implementations (all compilers) ==== */

/**
 * @brief Change processor mode from supervisor to user
 * @pre Processor in supervisor mode
 * @post Processor in user mode
 * @note Implemented with inline assembly
 * @warning Cannot return to supervisor mode without exception
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_ChangeToUserMode(void);

/**
 * @brief Set backup processor status word (BPSW)
 * @param[in] data BPSW value to set
 * @note Implemented with inline assembly
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_SetBPSW(uint32_t data);

/**
 * @brief Get backup processor status word (BPSW)
 * @return uint32_t Current BPSW value
 * @note Implemented with inline assembly (ICCRX) or C wrapper (GNUC/CCRX)
 * @since Version 1.0.0
 */
uint32_t R_BSP_GetBPSW(void);

/**
 * @brief Set backup program counter (BPC)
 * @param[in] data BPC address to set
 * @note Implemented with inline assembly
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_SetBPC(void* data);

/**
 * @brief Get backup program counter (BPC)
 * @return void* Current BPC address
 * @note Implemented with inline assembly (ICCRX) or C wrapper (GNUC/CCRX)
 * @since Version 1.0.0
 */
void* R_BSP_GetBPC(void);

#ifdef BSP_MCU_EXCEPTION_TABLE
/**
 * @brief Set exception table base register (EXTB)
 * @param[in] data Exception table base address
 * @pre Address must be aligned to exception table requirements
 * @note Only available if BSP_MCU_EXCEPTION_TABLE defined
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_SetEXTB(void* data);

/**
 * @brief Get exception table base register (EXTB)
 * @return void* Current EXTB address
 * @note Only available if BSP_MCU_EXCEPTION_TABLE defined
 * @since Version 1.0.0
 */
void* R_BSP_GetEXTB(void);
#endif /* BSP_MCU_EXCEPTION_TABLE */

/**
 * @brief Set bit in byte (atomic operation)
 * @param[in,out] data Pointer to byte
 * @param[in] bit Bit position (0-7)
 * @post Bit at position is set to 1
 * @note Implemented with inline assembly (BSET instruction)
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_BitSet(uint8_t* data, uint32_t bit);

/**
 * @brief Clear bit in byte (atomic operation)
 * @param[in,out] data Pointer to byte
 * @param[in] bit Bit position (0-7)
 * @post Bit at position is cleared to 0
 * @note Implemented with inline assembly (BCLR instruction)
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_BitClear(uint8_t* data, uint32_t bit);

/**
 * @brief Reverse (toggle) bit in byte (atomic operation)
 * @param[in,out] data Pointer to byte
 * @param[in] bit Bit position (0-7)
 * @post Bit at position is inverted
 * @note Implemented with inline assembly (BNOT instruction)
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_BitReverse(uint8_t* data, uint32_t bit);

/**
 * @brief Move value to accumulator high 32 bits
 * @param[in] data Value to write to ACC[63:32]
 * @note Implemented with inline assembly (MVTACHI instruction)
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_MoveToAccHiLong(int32_t data);

/**
 * @brief Move value to accumulator low 32 bits
 * @param[in] data Value to write to ACC[31:0]
 * @note Implemented with inline assembly (MVTACLO instruction)
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_MoveToAccLoLong(int32_t data);

/**
 * @brief Move accumulator high 32 bits to register
 * @return int32_t ACC[63:32] value
 * @note Implemented with inline assembly (MVFACHI instruction)
 * @since Version 1.0.0
 */
int32_t R_BSP_MoveFromAccHiLong(void);

/**
 * @brief Move accumulator middle 32 bits to register
 * @return int32_t ACC[47:16] value (middle portion)
 * @note Implemented with inline assembly (MVFACMI instruction)
 * @since Version 1.0.0
 */
int32_t R_BSP_MoveFromAccMiLong(void);

/* ==== Double-precision floating-point support ==== */
#ifdef BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT
#ifdef __DPFPU

/**
 * @brief Set double-precision floating-point status word (DPSW)
 * @param[in] data DPSW value to set
 * @note Only available if DPFPU hardware present
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_SetDPSW(uint32_t data);

/**
 * @brief Get double-precision floating-point status word (DPSW)
 * @return uint32_t Current DPSW value
 * @note Only available if DPFPU hardware present
 * @since Version 1.0.0
 */
uint32_t R_BSP_GetDPSW(void);

/**
 * @brief Set double-precision exception control register (DECNT)
 * @param[in] data DECNT value to set
 * @note Only available if DPFPU hardware present
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_SetDECNT(uint32_t data);

/**
 * @brief Get double-precision exception control register (DECNT)
 * @return uint32_t Current DECNT value
 * @note Only available if DPFPU hardware present
 * @since Version 1.0.0
 */
uint32_t R_BSP_GetDECNT(void);

/**
 * @brief Get double-precision exception program counter (DEPC)
 * @return void* Address where double-precision exception occurred
 * @note Only available if DPFPU hardware present
 * @since Version 1.0.0
 */
void* R_BSP_GetDEPC(void);

#endif /* __DPFPU */
#endif /* BSP_MCU_DOUBLE_PRECISION_FLOATING_POINT */

/* ==== Trigonometric Function Unit (TFU) support ==== */
#ifdef BSP_MCU_TRIGONOMETRIC
#ifdef __TFU

#if BSP_MCU_TFU_VERSION == 1
/**
 * @brief Initialize Trigonometric Function Unit (TFU)
 * @pre Called before using any TFU functions
 * @post TFU hardware initialized and ready for use
 * @note Only for TFU version 1 (RX64M, RX71M, RX65N, RX66N, RX72M, RX72N)
 * @warning Must call before R_BSP_SINCOSF or R_BSP_ATAN2HYPOTF
 * @since Version 1.0.0
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_InitTFU(void);
#endif /* BSP_MCU_TFU_VERSION == 1 */

#ifdef __FPU
/**
 * @brief Calculate sine and cosine simultaneously (single-precision)
 * @param[in] f Angle in radians (float)
 * @param[out] sin Pointer to store sine result
 * @param[out] cos Pointer to store cosine result
 * @pre TFU initialized (R_BSP_INIT_TFU called if TFU_VERSION == 1)
 * @post sin and cos contain computed values
 * @note Uses TFU hardware for fast computation
 * @since Version 1.10
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_CalcSine_Cosine(float f, float* sin, float* cos);

/**
 * @brief Calculate atan2 and hypot simultaneously (single-precision)
 * @param[in] y Y coordinate
 * @param[in] x X coordinate
 * @param[out] atan2 Pointer to store atan2(y,x) result (angle in radians)
 * @param[out] hypot Pointer to store sqrt(x^2+y^2) result (magnitude)
 * @pre TFU initialized (R_BSP_INIT_TFU called if TFU_VERSION == 1)
 * @post atan2 and hypot contain computed values
 * @note Uses TFU hardware for fast computation
 * @since Version 1.10
 */
R_BSP_ATTRIB_INLINE_ASM void
R_BSP_CalcAtan_SquareRoot(float y, float x, float* atan2, float* hypot);
#endif /* __FPU */

#if BSP_MCU_TFU_VERSION == 2
/**
 * @brief Calculate sine and cosine simultaneously (fixed-point)
 * @param[in] f Angle in fixed-point format
 * @param[out] sin Pointer to store sine result
 * @param[out] cos Pointer to store cosine result
 * @note Only for TFU version 2 (newer MCUs)
 * @since Version 1.14
 */
R_BSP_ATTRIB_INLINE_ASM void R_BSP_CalcSine_Cosine_Fpn(int32_t f, int32_t* sin, int32_t* cos);

/**
 * @brief Calculate sine (fixed-point)
 * @param[in] fx Angle in fixed-point format
 * @return int32_t Sine result in fixed-point
 * @note Only for TFU version 2
 * @since Version 1.14
 */
int32_t R_BSP_CalcSine_Fpn(int32_t fx);

/**
 * @brief Calculate cosine (fixed-point)
 * @param[in] fx Angle in fixed-point format
 * @return int32_t Cosine result in fixed-point
 * @note Only for TFU version 2
 * @since Version 1.14
 */
int32_t R_BSP_CalcCosine_Fpn(int32_t fx);

/**
 * @brief Calculate atan2 and hypot simultaneously (fixed-point)
 * @param[in] y Y coordinate (fixed-point)
 * @param[in] x X coordinate (fixed-point)
 * @param[out] atan2 Pointer to store atan2 result
 * @param[out] hypot Pointer to store hypot result
 * @note Only for TFU version 2
 * @since Version 1.14
 */
R_BSP_ATTRIB_INLINE_ASM void
R_BSP_CalcAtan_SquareRoot_Fpn(int32_t y, int32_t x, int32_t* atan2, int32_t* hypot);

/**
 * @brief Calculate atan2 (fixed-point)
 * @param[in] y Y coordinate
 * @param[in] x X coordinate
 * @return int32_t atan2(y,x) result in fixed-point
 * @note Only for TFU version 2
 * @since Version 1.14
 */
int32_t R_BSP_CalcAtan_Fpn(int32_t y, int32_t x);

/**
 * @brief Calculate square root of sum of squares (fixed-point)
 * @param[in] y First value
 * @param[in] x Second value
 * @return int32_t sqrt(x^2+y^2) result in fixed-point
 * @note Only for TFU version 2
 * @since Version 1.14
 */
int32_t R_BSP_CalcSquareRoot_Fpn(int32_t y, int32_t x);

#endif /* BSP_MCU_TFU_VERSION == 2 */
#endif /* __TFU */
#endif /* BSP_MCU_TRIGONOMETRIC */
