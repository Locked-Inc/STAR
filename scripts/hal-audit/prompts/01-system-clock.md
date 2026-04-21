Audit the RX72N system clock and module-stop register definitions.

Files (verify ONLY these):
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_system_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_clock.h
- star-rx72n-firmware/src/rx_clock_power_init.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Use `pdftotext -layout` to extract sections.

Verify against manual:
1. Chapter 9 "Clock Generation Circuit" -- SYSTEM register addresses (MOSCCR,
   MOSCWTCR, PLLCR, PLLCR2, SCKCR, SCKCR2, SCKCR3, OSCOVFSR, MEMWAIT, etc.)
   and bit field positions.
2. Chapter 11 "Low Power Consumption", section 11.2 "Module Stop Control
   Registers" -- verify MSTPCRA / MSTPCRB / MSTPCRC / MSTPCRD bit positions
   for EVERY peripheral mentioned in rx72n_system_regs.h or used in
   rx_clock_power_init.c.
3. Section 9.2.18 "Memory Wait Cycle Setting Register (MEMWAIT)".
4. Verify the SCKCR layout used in rx_clock_power_init.c. Specifically:
   `k_system_clock_dividers = 0x21C21211` claims "ICLK=240MHz, PCLKA=120MHz,
   others=60MHz" -- confirm this against manual section 9.2.1 SCKCR bit
   layout (PCKA/PCKB/PCKC/PCKD/FCK/BCK/ICK divider fields). Empirical
   evidence suggests PCKB is actually 120 MHz on this configuration, not
   60 MHz -- investigate.
5. Verify k_pclka_hz value in rx72n_clock.h matches the actual computed
   PCKA from the clock tree.

For each constant verified:
  OK <file>:<line> <constant> = <value>  (manual page <N>)
  FIX <file>:<line> <constant>: <old> -> <new>  (manual page <N>)

For each FIX, edit the file. After all edits build to confirm:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"

If build passes, commit + push:
  git add -A
  git commit -m "lib(clock): verify and correct system register / MSTP definitions per RX72N HW manual"
  git push origin bsikar/verifying-system-clock

If build fails: do NOT commit. Print the error.

Do not modify files outside the three listed above.
Do not chase code quality / formatting -- only register correctness.
Cap wall time: 30 minutes. Commit partial progress with TODO if needed.
