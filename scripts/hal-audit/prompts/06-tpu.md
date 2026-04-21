Audit the RX72N TPU (16-bit Timer Pulse Unit) HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_tpu_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_tpu.h
- star-rx72n-firmware/libs/rx_hal/src/rx_tpu.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 27 "16-Bit Timer Pulse Unit (TPUa)".

Verify:
1. TPU0..TPU5 base addresses.
2. Per-channel register offsets (TCR, TMDR, TIOR, TIER, TSR, TCNT, TGRA-D)
   per section 27.2.
3. Module-stop bit (MSTPCRA.MSTPA13 typically) -- verify section 11.2.2.
4. TMDR.MD encoding for phase counting modes 1-4 (section 27.4.7).
5. TSTR bit positions (CST0..CST5) per section 27.2.13.
6. TCR.CCLR / TCR.TPSC / TCR.CKEG fields per section 27.2.1.
7. Phase counting input clock mapping -- which TCLKx pin pairs feed which
   TPU channels (chapter 27 + chapter 23 PFS tables).

Note on labels: lib comments may say "TPU1 = rear-left, TPU2 = rear-right".
That terminology was recently corrected -- the actual mapping is TPU1 ->
back-right wheel, TPU2 -> back-left wheel. If you see stale rear-* labels,
fix them to back-right / back-left.

Edit + fix mismatches. After edits:
  cd star-rx72n-firmware
  PATH=/opt/gnurx/bin:$PATH bash build.sh

If build passes:
  git add -A
  git commit -m "lib(tpu): verify and correct TPU register definitions per RX72N HW manual"
  git push origin bsikar/verifying-tpu

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
