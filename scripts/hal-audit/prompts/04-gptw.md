Audit the RX72N GPTW (General PWM Timer) HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_gptw_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_gptw.h
- star-rx72n-firmware/libs/rx_hal/src/rx_gptw.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 26 "General PWM Timer (GPTW)".

Verify:
1. GPTW0/1/2/3 base addresses (k_gptwN_base_addr) -- chapter 5 I/O table.
2. GPTW_COMMON base address.
3. Per-channel register offsets (GTWP, GTCR, GTUDDTYC, GTIOR, GTBER, GTCNT,
   GTPR, GTPBR, GTCCRA, GTCCRB, GTSTR, GTSTP, GTCLR, GTSSR, GTPSR, GTCSR,
   GTUPSR, GTDNSR) and the common atomic-start GTSTPA/GTSTRA/etc.
4. Module-stop bit for GPTW. Manual says MSTPCRA.MSTPA7 (page 408 area).
   The lib has `k_pfs_psel_gptw` and `k_psel_gptw` -- both should be 0x1E
   per Tables 23.4..23.36 (PSEL for GTIOC alt function).
5. GTIOR field layout (GTIOA[3:0], OAE bit 8, GTIOB[19:16], OBE bit 24)
   per section 26.2.7.
6. GTUDDTYC field layout (UD bit 0, UDF bit 1) per section 26.2.6.
7. GTCR.MD field encoding (sawtooth=0, triangle modes 1/2/3).
8. GTBER buffering bit positions.
9. Phase-staggering math in `internal_calculate_phase_offset` (compare
   formulas to manual section 26.3 or equivalent).

Edit + fix mismatches. After edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"

If build passes:
  git add -A
  git commit -m "lib(gptw): verify and correct GPTW register definitions per RX72N HW manual"
  git push origin bsikar/verifying-gptw

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
