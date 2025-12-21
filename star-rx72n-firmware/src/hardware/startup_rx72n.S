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
 * RX72N has 256 interrupt vectors
 */
_vectors:
    /* Exception vectors */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   0                       /* Reserved */
    .long   _PowerON_Reset          /* Reset vector (0xFFFFFFFC) */

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
    bra.s   _exit_loop

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

/* Weak aliases for all interrupt handlers */
    .weak   _main
    .set    _main, main
