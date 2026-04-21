Audit the RX72N PORT (GPIO) HAL.

Files:
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_port_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_port_utils.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_port_constants.h  (read-only, do
  not modify -- this is the public pin enum)
- star-rx72n-firmware/libs/rx_hal/src/gpio.c

Authoritative source: docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf
Manual chapter 22 "I/O Ports".

Verify:
1. PORT register block base addresses: PDR @ 0x0008C000+n, PODR @
   0x0008C020+n, PIDR @ 0x0008C040+n, PMR @ 0x0008C060+n, etc. for each
   port n (where PORT0=0, PORT1=1, ..., PORT9=9, PORTA=10, PORTB=11,
   PORTC=12, PORTD=13, PORTE=14, PORTF=15, PORTG=16, PORTH=17, PORTJ=18).
   Some ports may NOT exist on the 144-pin LFQFP package -- the lib should
   either omit them or guard them.
2. ODR0 / ODR1 register offsets (open-drain control), DSCR / DSCR2 (drive
   strength), PCR (pull-up control).
3. rx_port_get_base() in rx_port_utils.h: every port the function returns
   an address for must match the manual's per-port register block.
4. The k_rx_port_X enum values in rx_port_constants.h must match the port
   index used by the address calculations.
5. The GPIO helpers in gpio.c (set output, set input, write high/low):
   verify they touch PDR/PMR in the right order (PMR=0 for GPIO mode, then
   PDR=1 for output).

Edit + fix mismatches. Do NOT modify rx_port_constants.h itself, only
verify it against the manual; if a value is wrong, FIX in the .h or .c
that reads it (not the public enum which has many call sites).

After edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"

If build passes:
  git add -A
  git commit -m "lib(port): verify and correct PORT register definitions per RX72N HW manual"
  git push origin bsikar/verifying-port-gpio

If build fails: stop, do not commit, print error.

Output:
  OK <file>:<line> <const> = <value>  (manual page <N>)
  FIX <file>:<line> <const>: <old> -> <new>  (manual page <N>)

Cap wall time: 30 minutes.
