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
    .extern __tx_timer_interrupt
    .extern _g_cmt0_isr_count

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
;; CMT0 CMI0 handler (vector 28) -- ThreadX 100 Hz system tick.
;;
;; MUST wrap _tx_timer_interrupt() with _tx_thread_context_save /
;; _tx_thread_context_restore per the ThreadX RXv3 port convention: the
;; context_restore routine is what actually re-evaluates the ready queue
;; and switches to the highest-priority ready task on ISR exit.  Without
;; this wrapping, a task that wakes up from tx_thread_sleep() never gets
;; scheduled until some OTHER context-switching event occurs.  Reference:
;;   libs/threadx/ports/rxv3/gnu/src/tx_timer_interrupt.S
;;   eclipse-threadx/threadx tree: SWI / CMI ISR samples for RXv3
;;   Renesas FSP INTERRUPT_HANDLER(cmt_isr) pattern
;;
;; Stack frame on entry (hardware-pushed): PC, PSW
;; After our pushes: R1, R2, PC, PSW -- this is what context_save expects.
;; ---------------------------------------------------------------------------
    .global _cmt0_isr
_cmt0_isr:
    PUSH    R2
    PUSH    R1
    BSR     __tx_thread_context_save    ;; Save full ThreadX context

    ;; ---- ISR body (interrupts are locked out at this point) ----
    ;; Clear ICU IR[28] latch (pending-IRQ bit).  RX72N CMT has no CMCSR.CMF
    ;; flag; IR[28] IS the pending-IRQ flag per RX72N HW manual section 14.2.1.
    MOV.L   #0x0008701C, R1
    MOV.L   #0, R2
    MOV.B   R2, [R1]

    ;; Bump diagnostic counter (volatile uint32_t g_cmt0_isr_count in cmt0.c)
    MOV.L   #_g_cmt0_isr_count, R1
    MOV.L   [R1], R2
    ADD     #1, R2
    MOV.L   R2, [R1]

    ;; Advance ThreadX system tick + process expirations, then chain
    ;; directly into context_restore.  Using BSR + BRA would run
    ;; context_restore twice if tx_timer_interrupt.S's RTS path is wrong
    ;; for our stack layout; BSR + BRA is what the RXv3 samples do but
    ;; some ports expect a single tail-call.  Try the safer BSR pattern
    ;; first (matches Renesas FSP INTERRUPT_HANDLER sample) -- if timer
    ;; still doesn't expire, swap this for a single BRA __tx_timer_interrupt.
    BSR     __tx_timer_interrupt

    BRA     __tx_thread_context_restore ;; Pick next thread, restore + RTE

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
