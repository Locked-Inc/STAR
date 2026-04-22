;;
;; sci9_rxi9_vector.S -- Wire SCI9_RXI9 (vector 102) to INT_SCI9_RXI9 in uart.c.
;;
;; Without this file the GNURX linker resolves $tableentry$102$.rvectors via
;; src/boot/linker_script_rvectors.inc to $tableentry$default$.rvectors
;; (0xFFFFFFFF), leaving SCI9_RXI9 effectively disabled even when SCR.RIE=1
;; and ICU.IER[12].bit6=1. The interrupt then never fires and the polled-RX
;; fallback in uart_getc_channel times out with nothing in the ring buffer.
;;
;; Mirrors src/boot/cmt0_vector.S -- the GNURX convention for registering a
;; relocatable-vector-table entry is to define a global label named
;; `$tableentry$<N>$.rvectors` whose address is the ISR entry point, and to
;; provide an anchor symbol referenced by some translation unit so
;; --gc-sections does not drop the entry.
;;
;; The ISR body itself lives in libs/rx_hal/src/uart.c as
;; `void INT_SCI9_RXI9(void)` with __attribute__((interrupt)). That attribute
;; makes GCC emit the full save/restore + RTE sequence; this file only wires
;; the vector slot to that C entry point.
;;

    .extern _INT_SCI9_RXI9

    ;; Use a section name that matches the linker_script.ld KEEP pattern
    ;; `KEEP(*(.text.*ISR))` so --gc-sections does not drop the vector
    ;; entry. The `_ISR` suffix is structural, not semantic.
    .section .text.sci9_rxi9_vector_ISR,"ax"

    ;; Anchor symbol kept for diagnostic nm/grep.
    .global _rx_sci9_rxi9_vector_anchor
_rx_sci9_rxi9_vector_anchor:

    ;; Relocatable-vector-table slot for SCI9_RXI9 -- define it as a real
    ;; label (not a `.set` alias, which the GNU RX linker resolved as UND)
    ;; and JMP to the C ISR body at _INT_SCI9_RXI9. The CPU has already
    ;; pushed PC+PSW by the time it reaches this code, and the C ISR is
    ;; decorated with __attribute__((interrupt)) so GCC emits PUSHM / RTE
    ;; around the body. JMP preserves the already-pushed frame so RTE
    ;; pops the correct return context.
    .global $tableentry$102$.rvectors
$tableentry$102$.rvectors:
    BRA _INT_SCI9_RXI9

    .end
