# GPIO Pin Test (gpio_test)

Automated GPIO verification for TOM's RX72N breakout PCB. The firmware
sweeps every GPIO pin in a known order while 3x Digilent Analog Discovery 2
units (48 digital channels total) capture the resulting transitions. The
host script matches captured edges against the known firmware sequence and
reports pass/fail per wired pin.

## Hardware setup

- **MCU**: R5F572NNHDFB (RX72N, 144/145-pin LFBGA or LFQFP)
- **Breakout PCB**: TOM's custom board, 98 GPIO pins exposed on labelled
  header pins (silkscreen on the back). See [pin_map.md](pin_map.md).
- **Test instruments**: 3x Digilent Analog Discovery 2 at
  `/Library/Frameworks/dwf.framework` 3.25.1 (macOS arm64 native) via
  `pydwf` 1.1.19.
- **Programmer**: Renesas E2 Lite connected to the FINE debug header,
  driven by `rfp-cli` v3.22 (macOS arm64 native at
  `~/tools/rfp-cli/bin/rfp-cli`).

### Analog Discovery 2 inventory

| Role  | Serial number  | Index in `AD2_SERIAL_NUMBERS` |
|-------|----------------|-------------------------------|
| AD2 A | `210321A36AA3` | 0                             |
| AD2 B | `210321A36AAE` | 1                             |
| AD2 C | `210321A2AE49` | 2                             |

Current wiring and the resolved CHANNEL_TO_PIN dict live in
[`../../docs/bench/ad2_wiring.md`](../../docs/bench/ad2_wiring.md).

## Firmware architecture

Single ThreadX thread (`gpio_sweep`) that walks `s_pins[]` in order:

1. For each pin:
   * Set PMR=0 (GPIO mode), PDR=1 (output) during `gpio_init_all()`.
   * Drive HIGH, busy-wait ~5 ms, drive LOW, busy-wait ~5 ms.
2. After every pin has pulsed once, busy-wait ~2.3 s so the host script
   can anchor its capture on the quiet gap.

Key structural choices (inherited from
`../blinky_rtos/` which was the known-working reference):

- **Busy-wait, not `tx_thread_sleep`.** The 100 Hz ThreadX tick is too
  coarse for ~5 ms pulses, and the earlier `tx_thread_sleep` version
  stalled the sweep thread indefinitely. `delay()` is a plain
  `volatile` nop loop.
- **`cmt0_init()` runs before `tx_kernel_enter()`**, producing a 100 Hz
  tick on vector 28 at PCKB=48 MHz (CMCOR=14999, CKS=01).
- **SWINT handler wired in `vectors.S`** with the manual R1/R2 push
  sequence `_tx_thread_context_save` on the RXv3 GNURX port requires.
- **PJ3 and PJ5 excluded** from `s_pins[]`. On RX72N they double as
  JTAG TMS/TDO; driving them while the E2 Lite holds the debug
  interface hangs the MCU (empirically observed, every pin after the
  first PJ write went silent).

Clock path: `clock_init()` switches the CPU from HOCO 16 MHz to PLL 192
MHz, giving ICLK 96 MHz and PCKB 48 MHz.

## Files

```text
gpio_test/
  README.md           -- this file
  pin_map.md          -- breakout pin-to-GPIO silkscreen map
  main.c              -- gpio_init_all + sweep_task + tx_application_define
  clock.c             -- HOCO 16 MHz -> PLL 192 MHz -> ICLK 96 MHz
  cmt0.c              -- 100 Hz CMT0 tick for ThreadX
  startup.S           -- stack setup, .data copy, BSS clear, INTB load
  vectors.S           -- rvector table with SWINT (27) + CMT0 (28) hooks
  linker.ld           -- RX72N memory map: ROM 0xFFE00000+, RAM 0x00000004+
  Makefile            -- build + flash
  host/
    gpio_verify.py    -- AD2 capture + verify (--auto-map, --verbose)
    .gitignore        -- venv/
```

## Build, flash, verify

```bash
# 1. Build inside the dev container (GNURX toolchain lives there)
devcontainer exec --workspace-folder <repo-root> bash -lc \
    'cd star-rx72n-firmware/gpio_test && make'

# 2. Flash from the host (native-arm64 rfp-cli talks to the E2 Lite
#    over USB; Docker Desktop does not pass USB through)
cd star-rx72n-firmware/gpio_test
make flash

# 3. Verify with the three AD2s
cd host
./venv/bin/python3 gpio_verify.py --verbose
```

First-time host setup (once only):

```bash
cd star-rx72n-firmware/gpio_test/host
python3 -m venv venv
./venv/bin/pip install pydwf
```

## `gpio_verify.py` modes

- **Verify (default)**: uses the committed `CHANNEL_TO_PIN` dict. Each
  wired pin's expected rising/falling edges are matched against the
  capture with a timing tolerance; prints PASS/FAIL per pin.
- **`--auto-map`**: captures two firmware cycles, identifies the cycle
  gap, measures the true pin period from two cycle boundaries, and
  prints a paste-ready `CHANNEL_TO_PIN` dict. Useful when you rewire
  the probes and want the script to figure out what landed where.
- **`--dry-run`**: prints config without opening the AD2s.
- **`--cycles N`**: capture N firmware cycles (default 1).

## Verified on hardware (2026-04-16)

13 s capture, all three AD2s, current wiring in
`docs/bench/ad2_wiring.md`:

| Check                                        | Result              |
|----------------------------------------------|---------------------|
| Wired channels toggling                      | 46 / 46             |
| Rising edges per pin (over ~4 cycles)        | 4-5 (consistent)    |
| Pin sweep order matches `FIRMWARE_PIN_ORDER` | MATCH               |
| Unwired or VCC-rail channels showing noise   | 0                   |

The two "dead" AD2 A channels (DIO 3 and DIO 5 = breakout pins 105 and
103) are intentionally unmapped: pins 105 and 103 on the breakout are
VCC rails, not GPIOs, so the corresponding AD2 inputs simply read a
constant HIGH. See `docs/bench/ad2_wiring.md` for the alternative
pins to rewire if you want 16 live channels on AD2 A.

## Gotchas

- **Host-side flash.** `rfp-cli` inside the dev container would need
  USB passthrough, which macOS Docker Desktop does not support. Flash
  lives on the host; the container is used only for the cross-compile.
- **First `make flash` after powering up the board can fail once** with
  `E4000004: framing error`. Re-run -- subsequent attempts succeed.
- **WaveForms framework.** The `dwf.framework` *must* live under
  `/Library/Frameworks/` (system) or `~/Library/Frameworks/` (user) --
  `pydwf` auto-locates it, but if both exist and one is stale the
  loader picks the wrong one.
- **Do NOT add PJ3 or PJ5 to `s_pins[]`.** Those are JTAG TMS/TDO. The
  E2 Lite keeps the JTAG interface latched, and the first PODR write
  to one of those pins hardfaults the MCU.
- **Pin period drifts.** The nop-based delay is nominally ~5 ms but
  the compiler/optimisation level can shift it. `gpio_verify.py`
  measures the true period from the capture (cycle length minus gap
  divided by NUM_PINS) so the verify step works without hand-tuning.
