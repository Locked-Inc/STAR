Audit the RX72N IWDT (Independent Watchdog) and WDT HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_iwdt_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_wdt_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_hal_iwdt.h
- star-rx72n-firmware/libs/rx_hal/src/rx_iwdt.c
- star-rx72n-firmware/libs/rx_core/src/rx_iwdt.c     (RX core wrapper)
- star-rx72n-firmware/libs/rx_core/src/rx_wdt.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 30 "Independent Watchdog Timer (IWDT)" and chapter 29
"Watchdog Timer (WDT)".

Verify:
1. IWDT base address (0x00088030 typ): IWDTRR @+0x00, IWDTCR @+0x02 (16-bit
   read, 8-bit write), IWDTSR @+0x04, IWDTRCR @+0x06, IWDTCSTPR @+0x08.
2. WDT base address (0x00088020 typ): WDTRR, WDTCR, WDTSR, WDTRCR.
3. IWDTCR field encoding: TOPS[1:0] timeout, CKS[3:0] clock div, RPSS[1:0],
   RPES[1:0].
4. IWDTSR / WDTSR bit positions (CNTVAL, REFEF, UNDFF).
5. IWDT registers fed via OFS0 option-setting register at 0xFE7F5D04 -- the
   driver should NOT modify OFS0 at runtime (it's flash-resident).
6. IWDT refresh (writing 0x00 then 0xFF to IWDTRR) per section 30.2.1.
7. Module-stop: IWDT typically has no MSTP (always running once OFS0
   enables it). Verify.

Edit + fix mismatches. After edits:
  cd star-rx72n-firmware
  PATH=/opt/gnurx/bin:$PATH bash build.sh

If build passes:
  git add -A
  git commit -m "lib(iwdt/wdt): verify and correct watchdog register definitions per RX72N HW manual"
  git push origin bsikar/verifying-iwdt-wdt

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
