Audit a batch of smaller RX72N peripheral register headers. Each is small
enough that grouping is fine.

Files (verify ALL of these, but a quick spot-check per file is OK):
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_dmac_regs.h    (DMAC)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_dtc_regs.h     (DTC)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_elc_regs.h     (ELC)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_lpc_regs.h     (Low-Power Ctrl)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_lvda_regs.h    (LVD)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_ram_regs.h     (RAM ECC)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_rtc_regs.h     (RTC)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_temps_regs.h   (Temp sensor)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_crc_regs.h     (CRC)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_flash_regs.h   (Flash)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_mpu_regs.h     (MPU)
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_poe3_regs.h    (POE3)

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
For each peripheral above, find its chapter via the TOC (search the manual
table of contents for the peripheral name).

For each file, verify:
1. Module base address against chapter 5 I/O register address table.
2. Module-stop bit position (if any) against chapter 11.2.
3. The 2-3 most heavily used registers' field bit positions.

Spot-check level: if a header has 200 register-bit definitions, just verify
a representative sample (~20 per file). Pay special attention to anything
the firmware actually writes (grep `git grep <register-name>` to find use
sites).

Edit + fix mismatches. After edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"

If build passes:
  git add -A
  git commit -m "lib(misc-regs): verify and correct misc peripheral register headers per RX72N HW manual"
  git push origin bsikar/verifying-misc-regs

If build fails: stop, do not commit, print error.

Output (group findings by file):
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes total. If a file is too dense to spot-check
within the budget, list the unverified constants in a TODO comment and
move on.
