;;
;; blinky/startup.S -- Minimal RX72N reset handler.
;;
;; Sets USP, ISP, then jumps to main().
;; No BSS zeroing (main.c uses no global variables).
;; No C++ init. No exception vectors (interrupts not used).
;;
;; _ustack and _istack are defined in linker.ld.
;;

    .section .text
    .global _PowerON_Reset_PC
    .global _PowerON_Reset

_PowerON_Reset_PC:
_PowerON_Reset:
    mvtc    #_ustack,USP       ;; user stack pointer
    mvtc    #_istack,ISP       ;; interrupt stack pointer
    bra     _main              ;; jump to C entry point

_loop_here:
    bra     _loop_here         ;; trap if main() ever returns

;;
;; Fixed reset vector at 0xFFFFFFFC.
;; The CPU reads this 4-byte address on power-on/reset and jumps there.
;;
    .section .fvectors,"a"
    .long   _PowerON_Reset_PC

    .end
