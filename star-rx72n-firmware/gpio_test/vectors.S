;;
;; gpio_test/vectors.S -- Minimal relocatable interrupt vector table.
;;
;; The polling-based gpio_test does not use any interrupts, but the RX72N
;; still expects INTB to point at a 256-entry relocatable vector table.
;; Every slot is wired to _default_isr (an infinite loop) so any unexpected
;; interrupt traps in an easy-to-debug location.
;;

    .section .text
    .global _default_isr
_default_isr:
    bra     _default_isr

    .section .rvectors, "a"
    .global _rvectors_start
_rvectors_start:
    .rept   256
    .long   _default_isr
    .endr
