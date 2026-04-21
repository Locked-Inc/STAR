;;
;; motor_spin_test/vectors.S -- Bare-metal vector table.
;;
;; No ThreadX, no CMT0 tick -- this bench test runs from main() with busy
;; waits. Every interrupt vector lands in _default_isr (infinite loop) so
;; an unexpected exception is at least caught instead of jumping to garbage.
;;

    .section .text
    .global _default_isr
_default_isr:
    bra     _default_isr

    .section .rvectors, "a"
    .global  _rvectors_start
_rvectors_start:
    .long   _default_isr    ;; 0  BUSERR
    .long   _default_isr    ;; 1
    .long   _default_isr    ;; 2  FIFERR
    .long   _default_isr    ;; 3
    .long   _default_isr    ;; 4
    .long   _default_isr    ;; 5  FRDYI
    .long   _default_isr    ;; 6
    .long   _default_isr    ;; 7
    .long   _default_isr    ;; 8
    .long   _default_isr    ;; 9
    .long   _default_isr    ;; 10
    .long   _default_isr    ;; 11
    .long   _default_isr    ;; 12
    .long   _default_isr    ;; 13
    .long   _default_isr    ;; 14
    .long   _default_isr    ;; 15
    .long   _default_isr    ;; 16
    .long   _default_isr    ;; 17
    .long   _default_isr    ;; 18
    .long   _default_isr    ;; 19
    .long   _default_isr    ;; 20
    .long   _default_isr    ;; 21
    .long   _default_isr    ;; 22
    .long   _default_isr    ;; 23
    .long   _default_isr    ;; 24
    .long   _default_isr    ;; 25
    .long   _default_isr    ;; 26
    .long   _default_isr    ;; 27
    .long   _default_isr    ;; 28
    .long   _default_isr    ;; 29
    .long   _default_isr    ;; 30
    .long   _default_isr    ;; 31

    .end
