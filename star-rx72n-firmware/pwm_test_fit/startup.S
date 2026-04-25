;;
;; gpio_test/startup.S -- RX72N startup for the GPIO breakout board test.
;;
;; Standard RXv3 bare-metal startup flow:
;;   1. Set USP and ISP from linker symbols.
;;   2. Copy .data from ROM (LMA) to RAM (VMA).
;;   3. Zero BSS.
;;   4. Load INTB with the relocatable vector table base.
;;   5. Jump to main(), which calls clock_init() before tx_kernel_enter().
;;

    .section .text
    .global  _PowerON_Reset_PC
    .global  _PowerON_Reset

    .extern  _main
    .extern  _bss
    .extern  _ebss
    .extern  _mdata
    .extern  _data
    .extern  _edata
    .extern  _ustack
    .extern  _istack
    .extern  _rvectors_start

_PowerON_Reset_PC:
_PowerON_Reset:
    ;; Stack pointers
    mvtc    #_ustack, USP
    mvtc    #_istack, ISP

    ;; Copy .data from ROM (LMA = _mdata) to RAM (VMA = _data.._edata)
    mov     #_mdata, r2
    mov     #_data,  r1
    mov     #_edata, r3
    sub     r1, r3
    smovf

    ;; Zero BSS
    mov     #_bss,  r1
    mov     #_ebss, r3
    sub     r1, r3
    mov     #0, r2
    sstr.b

    ;; INTB = base of relocatable vector table
    mvtc    #_rvectors_start, INTB

    ;; Jump to C entry point
    bra     _main

_loop_forever:
    bra     _loop_forever

    ;; Fixed reset vector @ 0xFFFFFFFC -- CPU reads this on reset
    .section .fvectors, "a"
    .long   _PowerON_Reset_PC

    .end
