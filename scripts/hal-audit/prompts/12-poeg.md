Audit the RX72N POEG (Port Output Enable for GPTW) HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_poeg_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_poeg.h
- star-rx72n-firmware/libs/rx_hal/src/rx_poeg.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 27 (or wherever "Port Output Enable for GPTW" lives -- search
for "POEG" in the manual TOC).

Verify:
1. POEGGA / POEGGB / POEGGC / POEGGD base addresses.
2. POEGGn register field encoding (PIDF, IOCF, OSTPF, SSF, PIDE, IOCE,
   OSTPE, SSE, NFEN, NFCS, INV, ST).
3. Module-stop bit -- shared with GPTW (MSTPCRA.MSTPA7) per the HAL
   comment, verify in section 11.2.2.
4. GTETRGA/B/C/D pin alt function (for emergency stop). PSEL value should
   match the GTIOC PSEL (likely 0x1E).

Note on labels: lib comments may say "POEGC = Motor 2 (Rear-left)
emergency stop" and "POEGD = Motor 3 (Rear-right) emergency stop". The
correct mapping is POEGC -> Motor 2 = back-RIGHT, POEGD -> Motor 3 =
back-LEFT (recently corrected). Fix any stale rear-* labels to back-*.

Edit + fix mismatches. After edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"

If build passes:
  git add -A
  git commit -m "lib(poeg): verify and correct POEG register definitions per RX72N HW manual"
  git push origin bsikar/verifying-poeg

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
