# STAR RX72N Board Variants

STAR's RX72N firmware supports two board variants, selected at compile time
via the `STAR_BOARD` CMake option (forwarded as a `STAR_BOARD_PROD=1` or
`STAR_BOARD_TOM=1` preprocessor macro).

```
cmake -DSTAR_BOARD=PROD ..    # default -- STAR production PCB
cmake -DSTAR_BOARD=TOM  ..    # Tom's bench PCB
```

The macro is consumed by `src/boot/inc/star_board.h` (acronym glossary),
`src/boot/inc/smc/r_bsp_config.h` (BSP clock-source overrides), and
`src/rx_clock_power_init.c` (runtime clock bring-up).

## Variant table

| Aspect                  | PROD (default)                 | TOM                                |
|-------------------------|--------------------------------|------------------------------------|
| Physical board          | STAR production PCB            | Tom's bench breakout PCB           |
| External 24 MHz crystal | Present (MOSC path)            | **Not present**                    |
| PLL input source        | MOSC (24 MHz crystal)          | HOCO (16 MHz internal RC)          |
| PLL multiplier          | `x 10.0` -> 240 MHz output     | `x 12.0` -> 192 MHz output         |
| ICLK (CPU)              | 240 / 2 = 120 MHz              | 192 / 2 = 96 MHz                   |
| PCKA (PWM/GPT)          | 240 / 2 = 120 MHz              | 192 / 2 = 96 MHz                   |
| PCKB (slower peripheral)| 240 / 4 = 60 MHz               | 192 / 4 = 48 MHz                   |
| UCLK (USB)              | 240 / 5 = 48 MHz (PPLL path)   | 192 / 4 = 48 MHz (main PLL path)   |
| USB clock accuracy      | ~30 ppm (crystal)              | **~10 000 ppm (HOCO)**             |
| USB FS spec compliance  | Yes (well under 500 ppm)       | **No** (20x over the 500 ppm limit)|

## HOCO and USB: does USB still work on TOM?

USB Full Speed runs at 12 Mbps regardless of which oscillator the SoC uses;
the "limit" imposed by HOCO is on **timing stability**, not raw speed.
USB FS encodes data using NRZI with bit-stuffing, and the spec requires
host+device clocks stay aligned to within half a bit over a maximum-size
packet (~667 bit times).  The math:

- Crystal (PROD): ~30 ppm -> 0.02 bits of drift per max packet -- trivial.
- HOCO (TOM): 10 000 ppm -> ~6.7 bits of drift per max packet -- **more than
  a full bit of phase error**.  The host's PLL has to pull in an off-spec
  signal.

Practical consequences observed on Tom's PCB:

1. **Enumeration works.**  Small descriptors (< 64 bytes) finish in under
   50 bit-times, so drift is negligible.  `bulk_in_fix.c` enumerated as
   1209:0002 on a Raspberry Pi 5 host immediately.
2. **Short bulk transfers work.**  Our diagnostic test issues 21-byte
   payloads; 5/5 reads completed at ~1 ms each with no CRC errors.
3. **Sustained high-throughput streaming may corrupt.**  Long packets and
   continuous back-to-back transfers are where bit-stuff miscounts,
   CRC mismatches and host-side timeouts creep in.
4. **Host-dependent.**  USB 2.0 host controllers must tolerate at least
   +/-0.25 % on FS, and most go wider.  Pi 5 / xHCI appears permissive;
   stricter hosts (some older USB 2.0 chips, some industrial gateways)
   may reject the device outright.

**Bottom line:**

- For bench bring-up, interactive debug, ISEL validation, CDC demos,
  occasional firmware-flash via USB-DFU -- the TOM variant is fine.
- For production reliability, USB-IF compliance testing, or any
  always-connected use case -- the PROD board (with the crystal) is
  required.  TOM deliberately accepts a spec violation in exchange for
  not needing the crystal; it is labeled as "bench only" for that
  reason.

The firmware itself does not know whether its USB peripheral will be
trusted by the host.  The bug fixes that work for both variants
(PIPEnCTR bit layout, ISEL on non-DCP pipes, etc.) are board-agnostic.

## Adding a new variant

Follow the three-point contract:

1. Add the name to the CMake option in `CMakeLists.txt`
   (`set_property(CACHE STAR_BOARD PROPERTY STRINGS ...)`).
2. Add an entry to the acronym + board table in `src/boot/inc/star_board.h`.
3. Add a `#ifdef STAR_BOARD_<name>` override block in
   `src/boot/inc/smc/r_bsp_config.h` and a parallel branch in
   `src/rx_clock_power_init.c::internal_start_oscillators_and_plls()` and
   `::internal_switch_to_pll_clock()` if the clock tree differs.

Keep PROD as the default so existing bench workflows continue to build
unchanged.
