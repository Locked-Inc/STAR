;;
;; gpio_test/vectors.S -- Relocatable vector table for the ThreadX build.
;;
;; Vectors used:
;;   27 -- SWINT (ThreadX preemption, triggered via ICU.SWINTR)
;;   28 -- CMT0 CMI0 (ThreadX 100 Hz tick -- cmt0_isr in cmt0.c)
;;
;; All other entries point to _default_isr (infinite loop).
;;
;; Layout copied from blinky_rtos/vectors.S which is the known-working
;; reference; the SWINT handler's manual R1/R2 push sequence is required
;; by ThreadX's RXv3 port convention for _tx_thread_context_save.
;;

    .extern __tx_thread_context_save
    .extern __tx_thread_context_restore
    .extern _cmt0_isr

    .section .text
    .global  _swint_handler
_swint_handler:
    PUSH    R2
    PUSH    R1
    BSR     __tx_thread_context_save
    BRA     __tx_thread_context_restore

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
    .long   _swint_handler  ;; 27 SWINT (ThreadX preemption)
    .long   _cmt0_isr       ;; 28 CMT0 CMI0 (ThreadX tick)
    .long   _default_isr    ;; 29
    .long   _default_isr    ;; 30
    .long   _default_isr    ;; 31

    .end
