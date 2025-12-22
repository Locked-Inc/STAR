/**
 * @file startup_rx72n.S
 * @brief Startup code and reset vector for RX72N
 *
 * Minimal startup code for RX72N microcontroller with ThreadX RTOS.
 */

    .section .fvectors, "ax"
    .global _PowerON_Reset

/**
 * Fixed interrupt vector table (at 0xFFFFFF80)
 * Contains exception vectors and reset vector
 */
_vectors:
    /* Exception vectors (0xFFFFFF80 - 0xFFFFFFFC) */
    .long   _Default_Handler        /* Vector  0: Reserved */
    .long   _Default_Handler        /* Vector  1: Reserved */
    .long   _Default_Handler        /* Vector  2: Reserved */
    .long   _Default_Handler        /* Vector  3: Reserved */
    .long   _Default_Handler        /* Vector  4: Reserved */
    .long   _Default_Handler        /* Vector  5: Reserved */
    .long   _Default_Handler        /* Vector  6: Reserved */
    .long   _Default_Handler        /* Vector  7: Reserved */
    .long   _Default_Handler        /* Vector  8: Reserved */
    .long   _Default_Handler        /* Vector  9: Reserved */
    .long   _Default_Handler        /* Vector 10: Reserved */
    .long   _Default_Handler        /* Vector 11: Reserved */
    .long   _Default_Handler        /* Vector 12: Reserved */
    .long   _Default_Handler        /* Vector 13: Reserved */
    .long   _Default_Handler        /* Vector 14: Reserved */
    .long   _Default_Handler        /* Vector 15: Reserved */
    .long   _Default_Handler        /* Vector 16: Reserved */
    .long   _Default_Handler        /* Vector 17: Reserved */
    .long   _Default_Handler        /* Vector 18: Reserved */
    .long   _Default_Handler        /* Vector 19: Reserved */
    .long   _Default_Handler        /* Vector 20: Reserved */
    .long   _Default_Handler        /* Vector 21: Reserved */
    .long   _Default_Handler        /* Vector 22: Reserved */
    .long   _Default_Handler        /* Vector 23: Reserved */
    .long   _Default_Handler        /* Vector 24: Reserved */
    .long   _Default_Handler        /* Vector 25: Reserved */
    .long   _Default_Handler        /* Vector 26: Reserved */
    .long   _Default_Handler        /* Vector 27: Reserved */
    .long   _Default_Handler        /* Vector 28: Reserved */
    .long   _Default_Handler        /* Vector 29: Reserved */
    .long   _Default_Handler        /* Vector 30: Reserved */
    .long   _PowerON_Reset          /* Vector 31: Reset (0xFFFFFFFC) */

/**
 * Relocatable interrupt vector table
 * RX72N uses this for peripheral interrupts
 */
    .section .rvectors, "ax"
    .global _relocatable_vectors

_relocatable_vectors:
    /* Vectors 0-27: Software/NMI/Exceptions */
    .rept 28
    .long   _Default_Handler
    .endr

    /* Vector 28: CMT0 CMI0 (Compare Match Interrupt 0) */
    .long   _cmt0_isr

    /* Vector 29: CMT1 CMI1 */
    .long   _Default_Handler

    /* Vectors 30-255: Other peripherals */
    .rept 226
    .long   _Default_Handler
    .endr

/**
 * @brief Power-on reset handler
 *
 * This is the entry point after reset. It:
 * 1. Initializes stack pointer
 * 2. Copies .data section from ROM to RAM
 * 3. Zeros .bss section
 * 4. Calls main()
 */
    .section .text
    .global _PowerON_Reset
    .type _PowerON_Reset, @function

_PowerON_Reset:
    /* Disable interrupts */
    clrpsw  i

    /* Initialize stack pointer */
    mov.l   #_StackTop, r0
    mvtc    r0, isp             /* Interrupt stack pointer */
    mvtc    r0, usp             /* User stack pointer */

    /* Copy relocatable interrupt vectors to RAM */
    mov.l   #__rvectors_load, r1    /* Source (ROM) */
    mov.l   #__rvectors_start, r2   /* Destination (RAM) */
    mov.l   #__rvectors_end, r3     /* End */
    sub     r2, r3                  /* Length */
    smovf                           /* Block copy */

    /* Set INTB register to point to relocatable vectors */
    mov.l   #__rvectors_start, r1
    mvtc    r1, intb

    /* Initialize data section (copy from ROM to RAM) */
    mov.l   #_sidata, r1        /* Source (ROM) */
    mov.l   #_sdata, r2         /* Destination (RAM) */
    mov.l   #_edata, r3         /* End */
    sub     r2, r3              /* Length */
    smovf                       /* Block copy */

    /* Initialize BSS section (zero fill) */
    mov.l   #0, r1
    mov.l   #_sbss, r2
    mov.l   #_ebss, r3
    sub     r2, r3
    sstr.b                      /* Block fill with zero */

    /* Call C++ constructors (if any) */
    bsr.a   _init

    /* Call main() */
    bsr.a   _main

    /* If main returns, loop forever */
_exit_loop:
    wait
    bra     _exit_loop

/**
 * @brief Dummy _init function (C++ constructors)
 */
    .weak _init
    .type _init, @function
_init:
    rts

/**
 * @brief Default exception handler
 */
    .global _Default_Handler
    .type _Default_Handler, @function
_Default_Handler:
    wait
    bra.s   _Default_Handler

    .size _Default_Handler, . - _Default_Handler

/* External symbols defined in C code */
    .extern _main
    .extern _cmt0_isr
