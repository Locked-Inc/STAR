;;
;; blinky_rtos/startup.S -- Minimal RX72N startup for ThreadX blinky.
;;
;; Responsibilities:
;;   1. Set user and interrupt stack pointers.
;;   2. Zero the BSS section.
;;   3. Load INTB with the base address of the relocatable vector table.
;;   4. Jump to main() -- which calls tx_kernel_enter() and never returns.
;;
;; Symbols provided by linker.ld:
;;   _ustack         -- top of user stack (USP)
;;   _istack         -- top of interrupt stack (ISP)
;;   _bss, _ebss     -- BSS start/end for zero-fill
;;   _rvectors_start -- base address for INTB
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
    ;; Set stack pointers
    mvtc    #_ustack, USP
    mvtc    #_istack, ISP

    ;; Copy .data from ROM (LMA = _mdata) to RAM (VMA = _data.._edata)
    ;; Required: ThreadX __tx_thread_system_state init value = 0xF0F0F0F0
    mov     #_mdata, r2         ;; ROM source (LMA)
    mov     #_data,  r1         ;; RAM destination (VMA)
    mov     #_edata, r3         ;; RAM end
    sub     r1, r3              ;; r3 = byte count
    smovf                       ;; copy r3 bytes from r2 to r1

    ;; Zero BSS
    mov     #_bss,  r1          ;; BSS start
    mov     #_ebss, r3          ;; BSS end
    sub     r1, r3              ;; r3 = byte count
    mov     #0, r2
    sstr.b                      ;; fill r3 bytes at r1 with r2 (0)

    ;; Point INTB at our relocatable vector table
    ;; Control register index 12 (0xC) = INTB
    mvtc    #_rvectors_start, INTB

    ;; Jump to C entry point
    bra     _main

_loop_forever:
    bra     _loop_forever       ;; trap if main() ever returns

    ;; Fixed reset vector -- CPU reads this 4-byte address at 0xFFFFFFFC on reset
    .section .fvectors, "a"
    .long   _PowerON_Reset_PC

    .end
