Audit the RX72N ICU (Interrupt Controller Unit) HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_icu_regs.h

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 14 "Interrupt Controller Unit (ICUb)".

Verify:
1. ICU base address (typ 0x00087000) and the bit-array registers IR, DTCER,
   IER, IPR per section 14.2.
2. Register block layout: IR is a per-interrupt 8-bit array (256 entries
   covering vectors 0..255), DTCER similar, IER is 16 bytes (one bit per
   vector), IPR is 256 bytes.
3. IRQCR0..15 base addresses (external IRQ pin control) and IRQFLTE0/E1
   registers.
4. SLIBR / SLIPR (selectable interrupt source registers) for vectors
   128..207 -- the lib should expose these.
5. Specific vector numbers used by the firmware -- check that the firmware's
   vector assignments (like CMT0, GPTW, TPU, RIIC, RSPI, SCI9 interrupts)
   match the manual's "Interrupt Vector Number" table.
6. The "B-prefix" naming of ICU IR fields varies per manual revision --
   verify against the table in section 14.3 (interrupt vector table).

Edit + fix mismatches. After edits:
  cd star-rx72n-firmware
  PATH=/opt/gnurx/bin:$PATH bash build.sh

If build passes:
  git add -A
  git commit -m "lib(icu): verify and correct ICU register definitions per RX72N HW manual"
  git push origin bsikar/verifying-icu

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
