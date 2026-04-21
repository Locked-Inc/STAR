Audit the RX72N MPC (Multi-Function Pin Controller) HAL.

Files (verify ONLY these):
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_mpc_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_mpc.h
- star-rx72n-firmware/libs/rx_hal/src/rx_mpc.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 23 "Multi-Function Pin Controller (MPC)".

Verify:
1. MPC base register addresses (PWPR=0x0008C11F, PFCSE, PFCSS, PFAOE, PFBCR,
   PFENET, PFS register block start at 0x0008C140) -- cross-check section
   23.2 register descriptions.
2. PFS register block layout: per-port stride, bit layout
   (b5:0=PSEL, b6=ISEL, b7=ASEL).
3. Every k_psel_* constant in rx_mpc.h. PSEL values are PIN-SPECIFIC --
   verify against the per-port PFS register tables (Tables 23.4 through
   23.36). Specifically:
   - k_psel_gptw value
   - k_psel_mtu_ioc, k_psel_mtu_clk, k_psel_mtu_phase
   - k_psel_sci_tx (SCI TXD) and any per-channel SCI PSEL constants
   - k_psel_rspi_*, k_psel_riic_*, k_psel_usb_*
4. The rx_mpc_set_*() helper functions: do they correctly write PFS *and*
   set the corresponding PORT.PMR bit (peripheral-mode enable)? The manual
   requires both. If a helper only writes PFS, that is a bug.
5. PWPR unlock sequence (clear B0WI then set PFSWE) -- verify against
   section 23.2.1.

Edit and fix any mismatches. After all edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"

If build passes:
  git add -A
  git commit -m "lib(mpc): verify and correct PSEL values + PMR handling per RX72N HW manual"
  git push origin bsikar/verifying-mpc

If build fails: stop, do not commit, print error.

Output format per constant:
  OK <file>:<line> <constant> = <value>  (manual page <N>)
  FIX <file>:<line> <constant>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
