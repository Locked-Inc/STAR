Audit the RX72N RSPI (SPI) HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_rspi_regs.h
- star-rx72n-firmware/libs/rx_hal/src/rspi.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 44 "Renesas Serial Peripheral Interface (RSPIa)".

Verify:
1. RSPI0/1/2 base addresses (RSPI0=0x000D0100 typ, RSPI1=0x000D0140,
   RSPI2=0x000D0300) -- chapter 5 I/O register table.
2. Module-stop bits:
   - RSPI0: MSTPCRB bit 17
   - RSPI1: MSTPCRB bit 16
   - RSPI2: MSTPCRC bit 22
   Verify in sections 11.2.3 / 11.2.4.
3. Register offsets within one channel: SPCR, SSLP, SPPCR, SPSR, SPDR,
   SPSCR, SPSSR, SPBR, SPDCR, SPCKD, SSLND, SPND, SPCR2, SPCMD0..7, SPDCR2.
4. SPCR bit positions (SPRIE bit7, SPE bit6, SPTIE bit5, SPEIE bit4, MSTR
   bit3, MODFEN bit2, TXMD bit1, SPMS bit0).
5. SPSR bit positions (SPRF bit7, SPTEF bit5, UDRF bit4, PERF bit3, MODF
   bit2, IDLNF bit1, OVRF bit0).
6. SPPCR bit positions (MOIFE/MOIFV) -- known prior bug, verify against
   manual section 44.2.3.
7. SPDCR bit positions (SPLW, SPRDTD) -- known prior bug, verify section
   44.2.10.
8. SPCMD register field encoding (CPOL, CPHA, BRDV, BPSEL, etc.) per
   section 44.2.13.

Edit + fix mismatches. After edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"

If build passes:
  git add -A
  git commit -m "lib(rspi): verify and correct RSPI register definitions per RX72N HW manual"
  git push origin bsikar/verifying-rspi

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
