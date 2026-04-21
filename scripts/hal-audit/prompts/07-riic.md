Audit the RX72N RIIC (I2C) HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_riic_regs.h
- star-rx72n-firmware/libs/rx_hal/src/riic.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 42 "I2C Bus Interface (RIICa)".

Verify:
1. RIIC0/RIIC1/RIIC2 base addresses (RIIC0=0x00088300 typ, channels spaced
   0x20 apart) -- chapter 5 I/O register table + section 42 register
   addresses.
2. Module-stop bits per channel:
   - RIIC0: MSTPCRB bit 21
   - RIIC1: MSTPCRB bit 20
   - RIIC2: MSTPCRC bit 17
   Verify in sections 11.2.3 / 11.2.4.
3. Register offsets within one channel (ICCR1, ICCR2, ICMR1-3, ICFER,
   ICSER, ICIER, ICSR1, ICSR2, SARLn, SARUn, ICBRL, ICBRH, ICDRT, ICDRR)
   per section 42.2.
4. ICCR1 bit positions (ICE, IICRST, CLO, SOWP, SCLO, SDAO, SCLI, SDAI).
5. ICCR2 bit positions (BBSY, MST, TRS, SP, RS, ST).
6. ICSR1/ICSR2 bit positions (ACKBR, NACKF, RDRF, TDRE, TEND, STOP, START).
7. ICBRH/ICBRL field width (5 bits for low / 5 bits for high) and prescaler
   selection in ICMR1.

Edit + fix mismatches. After edits:
  cd star-rx72n-firmware
  PATH=/opt/gnurx/bin:$PATH bash build.sh

If build passes:
  git add -A
  git commit -m "lib(riic): verify and correct RIIC register definitions per RX72N HW manual"
  git push origin bsikar/verifying-riic

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
