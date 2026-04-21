Audit the RX72N S12AD (12-bit ADC) HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_adc_regs.h
- star-rx72n-firmware/libs/rx_hal/src/adc.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 50 "12-Bit A/D Converter (S12ADFa)" (or equivalent S12ADc).

Verify:
1. S12AD0 / S12AD1 base addresses -- chapter 5 I/O register table.
2. Module-stop bit:
   - S12AD0: MSTPCRC bit 17 (matches code)
   - S12AD1: typically MSTPCRC bit 16
   Verify in section 11.2.4.
3. Register offsets: ADCSR, ADANSAn, ADANSBn, ADADS0/1, ADADC, ADCER,
   ADSTRGR, ADGSPCR, ADSSTRn, ADDR0..n, ADRD per chapter 50.
4. ADCSR bit positions (ADIE, ADCS, ADST, GBADIE, EXTRG, TRGE).
5. ADCER bit positions (ACE, ADRFMT) and ADC clock selection.
6. Per-channel ADANSA / ADANSB bit-to-channel mapping (which bit selects
   which AN0/AN1/... pin).
7. Conversion result register offsets (ADDR0..ADDR7 spaced 2 bytes).

Note: motor current sensing uses AN004..AN007 on S12AD0. Verify those
specifically.

Edit + fix mismatches. After edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"

If build passes:
  git add -A
  git commit -m "lib(adc): verify and correct S12AD register definitions per RX72N HW manual"
  git push origin bsikar/verifying-adc

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
