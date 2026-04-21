# HAL Register-Level Verification Prompt

Run this against an agent (or use yourself) to systematically verify every
register address, bit position, PSEL value, and module-stop bit declared in
`star-rx72n-firmware/libs/rx_hal/` against the authoritative RX72N HW
manual PDF at `docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf`.

## Why this is needed

Multiple HAL constants have already been found wrong against the manual,
each one silently breaking a different peripheral path:

| HAL file | Wrong value | Manual value | Symptom |
|---|---|---|---|
| `inc/rx_mpc.h` | `k_psel_gptw = 0x14` | `0x1E` (Tables 23.4-23.16) | Motor PWM never reached pad |
| `inc/rx72n_gptw_regs.h` | `k_pfs_psel_gptw = 0x14` | `0x1E` | Same as above |
| `src/rx_mpc.c` | `rx_mpc_set_peripheral` writes PFS but never sets PMR | docstring promises PMR=1 | Pin stays in GPIO mode |
| `src/rx_gptw.c` | `internal_configure_mpc/port_pins` hardcoded to port-E | Caller-supplied pins | Wrong pad muxed |
| `src/uart.c` | `k_uart_max_mstpb_channel = 11` (claims SCI8-11 in MSTPCRB) | Manual: SCI8-11 are in MSTPCRC bits 24-27 (page 410) | SCI9 module never enabled, no UART output |
| `usb_test/sci9_debug.c` | `k_mstpb_sci9_bit = 22U` in MSTPCRB | MSTPCRC bit 26 | Same as above |
| `inc/rx72n_gptw_regs.h` (pre-fix) | GPTW MSTP bit at MSTPCRC.MSTPC6 | MSTPCRA.MSTPA7 (commit aa39e0a7b) | GPTW never clocked |

There are almost certainly more. The HAL was clearly written from a
mix of memory, copy-paste, and guesses without per-bit verification
against the manual.

## Prompt for the verification agent

```
You are a register-level verification agent. Your job is to walk every
hardware register definition in /workspaces/STAR/star-rx72n-firmware/libs/rx_hal/
and confirm each value matches the RX72N HW manual PDF at
/workspaces/STAR/docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf.

Use `pdftotext -layout` to extract the manual's text, then `grep`/`sed`
to navigate. The manual is the single source of truth -- if HAL and
manual disagree, the HAL is wrong.

For EACH file under libs/rx_hal/inc/ and libs/rx_hal/src/, verify:

1. **Module base addresses** (e.g. SCI9 base = 0x000D0020, GPTW0 base =
   0x000C2000). Check against the manual's "I/O Register Address Table"
   (chapter 5 of the manual).

2. **Module-stop bit positions** (any `k_mstp*_bit` or constant used to
   clear/set MSTPCRA/B/C/D bits). Check against manual section 11.2
   "Module Stop Control Registers" (pages ~408-413). Specifically:
   - MSTPCRA: chapter 11.2.2
   - MSTPCRB: chapter 11.2.3
   - MSTPCRC: chapter 11.2.4
   - MSTPCRD: chapter 11.2.5
   Pay special attention to peripherals whose channels span
   MSTPCRB AND MSTPCRC (e.g. SCI0-7 in B, SCI8-11 in C).

3. **PSEL values for pin alt functions** (any `k_psel_*` constant). Check
   against manual chapter 23 "Multi-Function Pin Controller (MPC)",
   tables 23.1 (function summary) and 23.4-23.36 (per-port PFS register
   tables). The PSEL is per-pin -- the same function may have different
   PSEL on different ports.

4. **Register field bit positions** (any `k_*_bit_*` enum that names a
   single-bit field, e.g. ICCR1.ICE = bit 7). Check against the
   register-field tables in each peripheral's chapter.

5. **Multi-bit field positions** (e.g. PSEL[5:0] occupies bits 0-5).
   Verify the lib's bit shift/mask matches.

6. **Register layout structs** (anything `typedef struct __attribute__((packed))`
   that maps register layouts). Check that the byte offsets between
   fields match the manual's address listing for the peripheral.

For each verified item, output one of:
- `OK <file>:<line> <constant> = <value> matches manual <chapter/page>`
- `MISMATCH <file>:<line> <constant> = <value>; manual says <correct value> at <chapter/page>`
- `UNVERIFIED <file>:<line> <constant> -- could not locate in manual; need human review`

At the end, produce a fix patch list (file/line/old/new) for every
MISMATCH grouped by peripheral.

Do NOT modify any source files yourself -- the human will apply the
patches. Just produce the report.

Skip:
- Comments and docstrings (only verify executable constants)
- Mock/test files under libs/*/tests/ or tests/mocks/
- Already-verified constants if the file has a comment like
  "verified <date> against manual page <N>" (commit aa39e0a7b style)

Cap the per-file walk at the most common register classes:
- Module-stop bits
- Base addresses
- PSEL values
- Module enable/disable bits
- Pin-direction / pin-mode bits

Where the manual covers many channels of one peripheral, spot-check 2-3
channels per peripheral; if those match, trust the rest.

Report budget: aim for under 3000 words. If the report would exceed
that, prioritize the peripherals already used by motor_control_task
and comm_task (the production hot path):
GPTW, MTU, TPU, MPC, SCI (especially SCI9), RIIC, RSPI.

Build verification (CRITICAL on macOS hosts):
The GNURX cross-compiler `rx-elf-gcc` is blocked by macOS Gatekeeper. Do
NOT call `rx-elf-gcc`, `make`, or `bash build.sh` directly. For ALL build /
cross-compile commands, wrap them with the dev-container helper:

  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"

The helper auto-spins the dev container the first time and runs the
command inside it. If you propose any HAL fix, validate the build through
the helper before reporting the patch -- a fix that doesn't compile is
not a fix.
```

## How to run

```
# In Claude Code:
claude code "$(cat docs/hal_verification_prompt.md | sed -n '/^## Prompt/,/^```$/p' | sed '1d;$d;1d')"
```

Or paste the prompt text into a fresh agent yourself.
