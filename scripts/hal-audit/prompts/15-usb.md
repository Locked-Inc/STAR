Audit the RX72N USB0 (USB 2.0 FS Host/Function) register definitions.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_usb_regs.h
- star-rx72n-firmware/libs/rx_usb/  (driver layer, only verify register
  addresses/bit positions used here)

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 25 "USB 2.0 Host/Function Module (USBb)" (or "USB0").

Verify:
1. USB0 base address (typ 0x000A0000).
2. SYSCFG, SYSSTS0, DVSTCTR0 register offsets and bit positions
   (DPRPU, DRPD, DCFM, USBE, etc.) per section 25.2.
3. PIPE 0..9 control registers: PIPESEL, PIPECFG, PIPEMAXP, PIPEPERI,
   PIPEnCTR (n=1..9), PIPEnTRE/PIPEnTRN -- verify per-PIPE register
   addresses + offsets.
4. CFIFO/D0FIFO/D1FIFO register addresses and FIFOSEL bit layout.
5. Module-stop bit: USB0 = MSTPCRB bit 19 (per manual page 409).
6. PIPEnCTR bit positions -- known prior bug area (the "bulk-IN root-cause
   fix" commit aa39e0a7b touched this). Verify CSCLR, BCLR, ATREPM, ACLRM,
   SQSET, SQCLR, SQMON, PBUSY, PID[1:0] field positions.
7. INTENB0/INTSTS0 bit positions for VBINT, RESM, SOFR, DVST, CTRT, BEMP,
   NRDY, BRDY.

Edit + fix mismatches in headers. Do NOT modify the rx_usb driver source
files unless a fix in the .h cascades and a quick .c update is needed; in
that case keep the .c change minimal and obvious.

After edits:
  cd star-rx72n-firmware
  PATH=/opt/gnurx/bin:$PATH bash build.sh

If build passes:
  git add -A
  git commit -m "lib(usb): verify and correct USB0 register definitions per RX72N HW manual"
  git push origin bsikar/verifying-usb

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
