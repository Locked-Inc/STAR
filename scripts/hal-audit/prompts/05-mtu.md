Audit the RX72N MTU3 (Multi-Function Timer Pulse Unit 3) HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_mtu_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_mtu.h
- star-rx72n-firmware/libs/rx_hal/src/rx_mtu.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 25 "Multi-Function Timer Pulse Unit 3 (MTU3a)".

Verify:
1. MTU0..MTU8 per-channel base addresses + shared/common base.
2. Register offsets per channel (TCR, TMDR, TIORH, TIORL, TIER, TSR, TCNT,
   TGRA-TGRD, etc.) -- the layout differs between MTU0/3/4 and others.
3. Module-stop bit (MSTPCRA.MSTPA9 typically) -- verify in section 11.2.2.
4. TMDR.MD field encoding (normal, PWM mode 1/2, phase counting modes 1-4).
5. Phase counting modes 1-4 -- verify the lib's k_mtu_phase_counting_*
   constants match section 25.4.7.
6. TIOR field encoding (output disable / output low / high / toggle / etc.)
   per section 25.2.5.
7. TSTR / TSTRA bit positions (which bit starts which channel).

Edit + fix mismatches. After edits:
  cd star-rx72n-firmware
  PATH=/opt/gnurx/bin:$PATH bash build.sh

If build passes:
  git add -A
  git commit -m "lib(mtu): verify and correct MTU3 register definitions per RX72N HW manual"
  git push origin bsikar/verifying-mtu

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
