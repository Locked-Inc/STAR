Audit the RX72N CMT (Compare-Match Timer) HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_cmt_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_cmt.h
- star-rx72n-firmware/libs/rx_hal/src/rx_cmt.c
- star-rx72n-firmware/libs/rx_hal/src/timer.c (CMT0 driver for ThreadX
  tick)

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 28 "Compare Match Timer (CMT)".

Verify:
1. CMSTR0 base address (0x00088000), CMSTR1 base address.
2. Per-channel base addresses: CMT0 (in CMSTR0), CMT1 (in CMSTR0), CMT2
   (in CMSTR1), CMT3 (in CMSTR1).
3. Per-channel register offsets: CMCR @+0x00, CMCNT @+0x02, CMCOR @+0x04
   per section 28.2.
4. CMSTR.STR0 / STR1 bit positions (which bit starts which channel).
5. CMCR field encoding: CKS[1:0] (clock select PCLK/8, /32, /128, /512),
   CMIE bit per section 28.2.1.
6. Module-stop bits: MSTPCRA.MSTPA15 controls CMT0+CMT1, MSTPCRA.MSTPA14
   controls CMT2+CMT3 -- verify section 11.2.2.
7. CMT0 used for ThreadX tick: timer.c programs CMCOR for the desired
   tick rate. Verify the math: tick_period = (CMCOR + 1) * prescaler /
   PCKB. The 100 Hz tick @ PCKB=60 MHz with prescaler=8 needs CMCOR=74999.
   But empirically PCKB may actually be 120 MHz -- if so CMCOR doubles to
   149999. Investigate.

Edit + fix mismatches. After edits:
  cd star-rx72n-firmware
  PATH=/opt/gnurx/bin:$PATH bash build.sh

If build passes:
  git add -A
  git commit -m "lib(cmt): verify and correct CMT register definitions per RX72N HW manual"
  git push origin bsikar/verifying-cmt

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
