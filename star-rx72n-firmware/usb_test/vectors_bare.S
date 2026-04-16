;;
;; vectors_bare.S -- Minimal vector table for bare_test (no ThreadX).
;;
;; Only vectors used:
;;   36 -- USB0 USBI (main USB interrupt)
;;   34, 35, 90 -- USB0 D0FIFO, D1FIFO, USBR (stubs)
;;

    .section .text
    .global _default_isr
_default_isr:
    bra     _default_isr

    ;; No SWINT handler needed without ThreadX.

    .extern _usb0_usbi_isr
    .extern _usb0_d0fifo_isr
    .extern _usb0_d1fifo_isr
    .extern _usb0_usbr_isr

;; ---------------------------------------------------------------------------
;; Relocatable vector table -- 256 entries so we can reach vector 90.
;; ---------------------------------------------------------------------------
    .section .rvectors, "a"
    .global  _rvectors_start
_rvectors_start:
    ;; 0..26
    .rept 27
    .long   _default_isr
    .endr
    ;; 27 SWINT -- not used, just default
    .long   _default_isr
    ;; 28 CMT0 -- not used in bare test
    .long   _default_isr
    ;; 29..33
    .rept 5
    .long   _default_isr
    .endr
    ;; 34 USB0 D0FIFO
    .long   _usb0_d0fifo_isr
    ;; 35 USB0 D1FIFO
    .long   _usb0_d1fifo_isr
    ;; 36 USB0 USBI
    .long   _usb0_usbi_isr
    ;; 37..89
    .rept 53
    .long   _default_isr
    .endr
    ;; 90 USB0 USBR
    .long   _usb0_usbr_isr
    ;; 91..255
    .rept 165
    .long   _default_isr
    .endr
