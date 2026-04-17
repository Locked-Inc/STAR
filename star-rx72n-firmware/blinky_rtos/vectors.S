;;
;; blinky_rtos/vectors.S -- Relocatable interrupt vector table + SWINT handler.
;;
;; Layout:
;;   The .rvectors section is placed by linker.ld at an aligned ROM address.
;;   startup.S loads that address into INTB so the CPU can find ISRs.
;;
;; Vectors used:
;;   27 -- SWINT (ThreadX context switch trigger, triggered via ICU.SWINTR)
;;   28 -- CMT0 CMI0 (ThreadX system tick, 100 Hz)
;;
;; All other entries point to _default_isr which loops forever so an
;; unexpected interrupt is easy to spot in a debugger.
;;
;; SWINT handler calling convention (RXv3 ThreadX port):
;;   On SWINT entry the hardware has pushed PC + PSW onto the ISP.
;;   _tx_thread_context_save expects (from SP upward):
;;     SP+0:  return address (from BSR)
;;     SP+4:  R1  (must be pushed by the ISR before calling context_save)
;;     SP+8:  R2  (must be pushed by the ISR before calling context_save)
;;     SP+12: interrupted PC  (pushed by hardware)
;;     SP+16: interrupted PSW (pushed by hardware)
;;   We push R1 and R2 manually before calling context_save.
;;

    .extern __tx_thread_context_save
    .extern __tx_thread_context_restore
    .extern _cmt0_isr

;; ---------------------------------------------------------------------------
;; SWINT handler -- used by ThreadX for interrupt-driven thread preemption.
;; ---------------------------------------------------------------------------
    .section .text
    .global  _swint_handler
_swint_handler:
    PUSH    R2                          ;; Save R2 (context_save expects it here)
    PUSH    R1                          ;; Save R1 (context_save expects it here)
    BSR     __tx_thread_context_save    ;; Save full ThreadX context
    ;;                                    (context_save returns here for ISR body)
    BRA     __tx_thread_context_restore ;; Pick next thread, restore + RETI

;; ---------------------------------------------------------------------------
;; Default ISR -- halts execution for any unexpected interrupt.
;; ---------------------------------------------------------------------------
    .global _default_isr
_default_isr:
    bra     _default_isr

;; ---------------------------------------------------------------------------
;; Relocatable vector table (base loaded into INTB by startup.S).
;;
;; The CPU computes ISR address as:  [INTB + vector_number * 4]
;; We need 29 entries to cover vector 28 (CMT0).  Pad to 32 for alignment.
;; ---------------------------------------------------------------------------
    .section .rvectors, "a"
    .global _rvectors_start
_rvectors_start:
    .long   _default_isr    ;; 0:  BUSERR
    .long   _default_isr    ;; 1:  reserved
    .long   _default_isr    ;; 2:  FIFERR
    .long   _default_isr    ;; 3:  reserved
    .long   _default_isr    ;; 4:  reserved
    .long   _default_isr    ;; 5:  FRDYI
    .long   _default_isr    ;; 6:  reserved
    .long   _default_isr    ;; 7:  reserved
    .long   _default_isr    ;; 8:  reserved
    .long   _default_isr    ;; 9:  reserved
    .long   _default_isr    ;; 10: reserved
    .long   _default_isr    ;; 11: reserved
    .long   _default_isr    ;; 12: reserved
    .long   _default_isr    ;; 13: reserved
    .long   _default_isr    ;; 14: reserved
    .long   _default_isr    ;; 15: reserved
    .long   _default_isr    ;; 16: reserved
    .long   _default_isr    ;; 17: reserved
    .long   _default_isr    ;; 18: reserved
    .long   _default_isr    ;; 19: reserved
    .long   _default_isr    ;; 20: reserved
    .long   _default_isr    ;; 21: reserved
    .long   _default_isr    ;; 22: reserved
    .long   _default_isr    ;; 23: reserved
    .long   _default_isr    ;; 24: reserved
    .long   _default_isr    ;; 25: reserved
    .long   _default_isr    ;; 26: reserved
    .long   _swint_handler  ;; 27: SWINT (ThreadX preemption, IER[3] bit 3)
    .long   _cmt0_isr       ;; 28: CMT0 CMI0 (ThreadX 100 Hz tick)
    .long   _default_isr    ;; 29: reserved
    .long   _default_isr    ;; 30: reserved
    .long   _default_isr    ;; 31: reserved

    .end
