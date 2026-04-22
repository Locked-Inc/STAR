;;
;; cmt0_vector.S -- Wire CMT0 CMI0 (vector 28) to the ThreadX system tick
;; ISR using the RXv3 context-save/restore pattern.
;;
;; # Why this file exists
;;
;; The production firmware's timer.c has a C `cmt0_isr` decorated with
;; `__attribute__((interrupt))`, BUT that symbol was never placed at
;; vector slot 28 -- nothing in src/boot/ or libs/ defined
;; `$tableentry$28$.rvectors`, so the linker filled slot 28 with
;; 0xFFFFFFFF (see linker_script_rvectors.inc:56).  The CMT0 compare-
;; match ISR therefore never ran, `_tx_timer_system_clock` never
;; advanced, and every `tx_thread_sleep()` hung forever.
;;
;; Even with the vector wired, a plain `__attribute__((interrupt))`
;; handler is insufficient for the ThreadX RXv3 port: the port's timer
;; ISR MUST bracket `_tx_timer_interrupt` with `_tx_thread_context_save`
;; and `_tx_thread_context_restore`, because context_restore is what
;; actually re-evaluates the ready queue and dispatches the highest-
;; priority runnable thread on ISR exit.  Without that, a task waking
;; from tx_thread_sleep() never gets scheduled back in.  Reference:
;;   libs/threadx/ports/rxv3/gnu/src/tx_thread_schedule.S
;;     ($tableentry$27$.rvectors -- the SWINT handler, same pattern)
;;   motors_rtos/vectors.S     -- the bench-verified reference
;;   motors_rtos/cmt0.c        -- matching CMT0 peripheral init
;;
;; # What this file does
;;
;; 1. Declares `$tableentry$28$.rvectors` as a global label -- the RX
;;    GNURX linker convention used by linker_script_rvectors.inc to
;;    populate relocatable vector slot 28.  The label address becomes
;;    the ISR entry point stored at that vector.
;;
;; 2. Implements the ISR body per the RXv3 ThreadX port convention:
;;    a. Push R1, R2 so the stack frame matches what
;;       `__tx_thread_context_save` expects.
;;    b. BSR to context_save (saves full thread context).
;;    c. Clear ICU.IR[28] (0x0008701C) -- RX72N CMT has no CMCSR.CMF
;;       flag; IR[n] IS the pending-request latch for vector n.
;;    d. BSR to `__tx_timer_interrupt` (advances _tx_timer_system_clock
;;       and processes timer expirations).
;;    e. BRA to `__tx_thread_context_restore` (restores thread context
;;       and chains into RTE -- ThreadX RXv3 convention).
;;
;; # CPU register conventions used
;;
;; On ISR entry the CPU has already pushed PC and PSW onto ISP.  The
;; ThreadX port's context_save expects the stack layout:
;;   SP+0 : return address (from BSR)
;;   SP+4 : R1  (pushed here)
;;   SP+8 : R2  (pushed here)
;;   SP+12: interrupted PC   (pushed by hardware)
;;   SP+16: interrupted PSW  (pushed by hardware)
;;

    .extern __tx_thread_context_save
    .extern __tx_thread_context_restore
    .extern __tx_timer_interrupt

    .section .text

    ;; Anchor symbol the C code references via inline asm to prevent
    ;; --gc-sections from dropping this entire .text sub-section.  The
    ;; linker script pulls `$tableentry$28$.rvectors` via DEFINED(...)
    ;; but that does NOT count as a GC root; a direct reference from
    ;; some compilation unit does.  See timer.c:timer_init().
    .global _rx_cmt0_vector_anchor
_rx_cmt0_vector_anchor:

    .global $tableentry$28$.rvectors
$tableentry$28$.rvectors:

    PUSHM R1-R2                         ; Save R1, R2 per context_save contract
    BSR   __tx_thread_context_save      ; Save full ThreadX context

    ;; ---- ISR body runs with interrupts masked here ----

    ;; Clear ICU.IR[28] (0x0008_701C) -- pending-IRQ latch for CMT0_CMI0.
    ;; RX72N CMT lacks a CMCSR.CMF flag, so IR[n] serves that role.
    ;; Address computed per RX72N HW manual section 14.2.1:
    ;;   ICU base = 0x00087000; IR region offset 0x000; IR[28] = base + 28.
    MOV.L #0x0008701C, R1
    MOV.L #0, R2
    MOV.B R2, [R1]

    ;; Advance ThreadX system tick + process expirations.
    BSR   __tx_timer_interrupt

    ;; context_restore re-evaluates the ready queue and issues RTE.
    ;; Using BRA (not BSR) because context_restore never returns --
    ;; it dispatches the next thread directly.
    BRA   __tx_thread_context_restore

    .end
