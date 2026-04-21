Audit the RX72N SCI / UART HAL.

Files (verify ONLY these):
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_sci_regs.h
- star-rx72n-firmware/libs/rx_hal/src/uart.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Use `pdftotext -layout` and grep.

Manual references:
- Chapter 36 "Serial Communications Interface (SCI)" -- register layout, BRR
  formula, SCR/SMR/SSR bit positions.
- Section 5 "I/O Register Address Table" -- per-channel SCI base addresses
  (SCI0..SCI12).
- Section 11.2.3 (MSTPCRB) and 11.2.4 (MSTPCRC) -- MODULE STOP BITS for
  EACH SCI CHANNEL. Pay attention: SCI0..SCI7 are in MSTPCRB; SCI8, SCI9,
  SCI10, SCI11 are in MSTPCRC; SCI12 is in MSTPCRC at a different bit.
  This is a known bug area -- the existing `internal_get_mstpb_bit()`
  function in uart.c assumes SCI8..SCI11 also live in MSTPCRB which is
  WRONG. Find the right register + bit per channel.

Verify:
1. SCI0..SCI12 base addresses (every k_sciN_base_addr).
2. SCR/SMR/BRR/TDR/RDR/SSR/SCMR/SEMR offsets within one channel.
3. SCR bit positions (TIE, RIE, TE, RE, MPIE, TEIE, CKE).
4. SMR/SCMR/SEMR bit positions.
5. Module-stop bit + register for EACH SCI channel.
6. BRR formula correctness for the actual PCKB used by the system. Note:
   the `01-system-clock` audit owns confirming PCKB; cross-reference its
   findings if available -- empirical evidence says PCKB is 120 MHz, not
   60 MHz as currently documented.

Edit and fix mismatches. After edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"

If build passes:
  git add -A
  git commit -m "lib(sci/uart): verify and correct per-channel module-stop bits and register layout per RX72N HW manual"
  git push origin bsikar/verifying-sci-uart

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
