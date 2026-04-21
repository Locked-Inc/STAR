## RUNTIME CONFIG

Launch via:

  bash scripts/hal-audit/launch.sh --only 17

The launcher's default flags (Opus 4.7, thinking enabled, effort high,
permission prompts skipped) apply -- no per-prompt override needed.

No wall-time cap. Take the time you need to be exhaustive. This is
bigger than the per-peripheral audits because it spans EVERY scalar
constant in the HAL.

================================================================
## TASK

Cross-check the entire RX72N HAL against the hirakuni45/RX community
library, an empirically-tested third-party C++ port_map for this exact
chip. Two prior rounds of audits verified our HAL was internally
consistent against itself but missed the fact that some PSEL constants
were silently the wrong VALUE -- including k_psel_mtu_phase = 0x03
(actual hardware needs 0x02). Wheels with MTU encoders stayed at
TCNT=0x0000 for an entire bringup cycle because of it.

Your job: for every single scalar value in our HAL -- PSEL, PFS, PMR,
PDR, port, pin, peripheral base address, register offset, bit shift,
bit mask, MSTPCR bit, IRQ vector, clock divider, prescaler, mode bit,
status bit, enum value -- look it up in the hirakuni45/RX RX72N
headers (which are easy to navigate), THEN find the same value in the
RX72N HW manual PDF, and confirm both agree with our HAL. Where they
disagree, FIX OUR HAL to match THE MANUAL (not hirakuni45 -- the
manual is the spec; hirakuni45 is just an easier index into it).

Source-of-truth policy:
- The Renesas RX72N HW manual is THE authoritative source. Every
  constant we keep or change must cite a manual page or section number
  in its doc-comment.
- The hirakuni45 repo is a CROSS-REFERENCE / NAVIGATION TOOL ONLY.
  Use it to find what to look up in the manual. Do NOT cite it in
  source comments, file headers, or commit messages. Do NOT add a
  "verified against hirakuni45" line in any .h or .c file.
- If hirakuni45 and the manual disagree, the manual wins, and flag
  the disagreement in the commit body so a human can adjudicate.
- It IS fine to mention hirakuni45 in the audit OUTPUT TABLE (the
  Phase 1 stdout report) so a reviewer can see how each value was
  cross-checked. Just keep it out of committed source.

We are a robotics team and we already wasted bringup time on bad PSELs.
Be exhaustive. Skipping a value because it "looks fine" is what got us
here.

================================================================
## AUTHORITATIVE SOURCES (read-only)

### Source 1: hirakuni45/RX (primary cross-reference)

Pull a fresh copy outside the repo:

  git clone --depth 1 https://github.com/hirakuni45/RX /tmp/hirakuni45-RX

Files of interest (all under /tmp/hirakuni45-RX/RX72N/ unless noted):

  peripheral.hpp                -- list of supported peripherals + IRQ vectors
  port.hpp / port_base.hpp      -- PORT base addresses + PMR/PDR/PIDR/PODR/DSCR struct layout
  mpc.hpp                       -- MPC PFS register addresses + bit layout
  icu.hpp / icu_mgr.hpp         -- ICU IRQ priority + group interrupt mapping
  system.hpp                    -- SYSTEM register addresses + MSTPCR* layout + SCKCR/PLL bits
  port_map.hpp                  -- generic GPIO / fixed-function pin map
  port_map_mtu.hpp              -- MTU MTIOC / MTCLK pin -> PSEL table
  port_map_gptw.hpp             -- GPTW GTIOC / GTETRG pin -> PSEL table
  port_map_tpu.hpp              -- TPU TIOC / TCLK pin -> PSEL table
  port_map_sci.hpp              -- SCI TXD / RXD / SCK / CTS pin -> PSEL table
  port_map_riic.hpp             -- RIIC SCL / SDA pin -> PSEL table
  port_map_rspi.hpp             -- RSPI MOSI / MISO / RSPCK / SSL pin -> PSEL
  port_map_can.hpp              -- CAN TX / RX pin -> PSEL
  port_map_usb.hpp              -- USB pin -> PSEL
  ../RX600/cmt.hpp, mtu3.hpp, tpu.hpp, gptw.hpp, sci_*.hpp, riic.hpp,
  rspi.hpp, s12adh.hpp, etc.   -- per-peripheral register struct + offset
                                   definitions (these are SHARED across
                                   the RX600 family and most apply to RX72N)

The hirakuni45 port_map_*.hpp files all follow the same shape:

  static bool clk_a_(ORDER odr, bool ena) noexcept
  {
      // MTCLKA:
      // P14 (LFQFP100:  32) (LFQFP144:  43) (LFQFP176:  51)
      // P24 (LFQFP100:  24) (LFQFP144:  33) (LFQFP176:  40)
      uint8_t sel = ena ? 0b00'0010 : 0;          // <-- THE PSEL VALUE
      switch(odr) {
      case ORDER::FIRST:
          PORT1::PMR.B4 = 0;                       // PMR=0 BEFORE PFS write
          MPC::P14PFS.PSEL = sel;
          PORT1::PMR.B4 = ena;                     // PMR=1 AFTER PFS write
          break;
      case ORDER::SECOND:
          PORT2::PMR.B4 = 0;
          MPC::P24PFS.PSEL = sel;                  // <-- pin -> PSEL
          PORT2::PMR.B4 = ena;
          break;
      ...
      }
  }

Read the hpp file, decode the binary literal (e.g. 0b00'0010 == 0x02),
and use that as the ground-truth PSEL for every (pin, peripheral)
combination this project uses. If our HAL disagrees, ours is wrong.

For peripheral register bases / struct offsets: each peripheral has its
own .hpp under hirakuni45 (e.g. mtu3.hpp, tpu.hpp, gptw.hpp). Find the
`static constexpr uint32_t base = 0x000C1380;` style declarations and
the `volatile uint8_t tcr` style struct fields and use the offsets from
that struct (relative to base) as ground truth.

### Source 2: Renesas iodefine.h (fallback when hirakuni45 doesn't cover something)

  star-rx72n-firmware/pwm_test_fit/iodefine.h

This is the Renesas-shipped header for the exact RX72N variant. Use it
when:
  * hirakuni45 doesn't have an equivalent (rare).
  * You need to disambiguate a struct field with a non-obvious name.
  * You need IRQ vector numbers (hirakuni45 has them in icu.hpp too,
    but iodefine.h is more granular).

### Source 3: RX72N HW Manual PDF (fallback when both above are silent)

  docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf

Manual chapters most relevant:
  * 7   Reset
  * 9   Clock Generation Circuit  (SCKCR, PLLCR, OSCOVFSR, PRCR)
  * 11  Module Stop / Software Reset (MSTPCRA/B/C/D, SWRR)
  * 19  GPIO PORT
  * 23  Multi-Function Pin Controller (MPC) -- PSEL pin tables 23.4..23.36
  * 24  Multi-Function Timer Pulse Unit 3 (MTU3a)
  * 25  Port Output Enable 3 (POE3)
  * 26  General PWM Timer (GPTW)
  * 27  16-Bit Timer Pulse Unit (TPU)
  * 30  Compare Match Timer (CMT)
  * 36  Serial Communications Interface (SCI)
  * 37  I2C Bus Interface (RIIC)
  * 38  Serial Peripheral Interface (RSPI)
  * 47  12-Bit A/D Converter (S12ADH)

================================================================
## SCOPE: every single scalar value in these files

Constants:
  star-rx72n-firmware/libs/rx_hal/inc/rx_mpc.h
  star-rx72n-firmware/libs/rx_hal/inc/rx_gptw.h
  star-rx72n-firmware/libs/rx_hal/inc/rx_mtu.h
  star-rx72n-firmware/libs/rx_hal/inc/rx_tpu.h
  star-rx72n-firmware/libs/rx_hal/inc/rx_poeg.h
  star-rx72n-firmware/libs/rx_hal/inc/rx_hal_iwdt.h
  star-rx72n-firmware/libs/rx_hal/inc/rx72n_*_regs.h    (every one of them)
  star-rx72n-firmware/libs/rx_core/inc/rx_port_constants.h
  star-rx72n-firmware/libs/rx_core/inc/rx_register_protection.h
  star-rx72n-firmware/libs/rx_core/inc/rx_irq_filter.h
  star-rx72n-firmware/src/inc/hardware_config.h

Helper / driver functions (verify they apply the cross-checked
constants correctly, and that PMR sequencing matches hirakuni45):
  star-rx72n-firmware/libs/rx_hal/src/rx_mpc.c
  star-rx72n-firmware/libs/rx_hal/src/rx_gptw.c
  star-rx72n-firmware/libs/rx_hal/src/rx_mtu.c
  star-rx72n-firmware/libs/rx_hal/src/rx_tpu.c
  star-rx72n-firmware/libs/rx_hal/src/uart.c
  star-rx72n-firmware/libs/rx_hal/src/timer.c
  star-rx72n-firmware/libs/rx_hal/src/riic.c
  star-rx72n-firmware/libs/rx_hal/src/rspi.c
  star-rx72n-firmware/libs/rx_hal/src/adc.c
  star-rx72n-firmware/libs/rx_hal/src/rx_iwdt.c
  star-rx72n-firmware/libs/rx_hal/src/rx_poeg.c
  star-rx72n-firmware/libs/rx_encoder/src/rx_mtu_encoder.c
  star-rx72n-firmware/libs/rx_encoder/src/rx_encoder_tpu.c
  star-rx72n-firmware/src/rx_clock_power_init.c

Specific value classes you MUST verify, by category:

(a) PSEL constants in rx_mpc.h -- every k_psel_*. PSEL values are
    PIN-SPECIFIC; check at least one canonical pin per peripheral. If
    a single PSEL constant is reused across pins of the same
    peripheral, verify that the value really is uniform across those
    pins in hirakuni45.

(b) Peripheral register base addresses (every `k_*_base_addr`,
    `k_*_addr` constant). Compare to hirakuni45 mtu3.hpp / tpu.hpp /
    gptw.hpp / etc. `base` constants.

(c) Per-register offsets within each peripheral. Walk the
    `rx_*_channel_regs_t` struct field-by-field and confirm the
    cumulative offset of each `volatile uintN_t name` matches
    hirakuni45's struct walk. If hirakuni45 has 7 reserved bytes
    between `tier` and `tcnt` and we have 6, our struct alignment is
    off and TCNT writes go to the wrong place.

(d) MSTPCR bit positions for every peripheral the project enables.
    GPTW=MSTPA7, MTU=MSTPA9, TPU=MSTPA13, CMT0/1=MSTPA15,
    CMT2/3=MSTPA14, etc. Confirm all of them.

(e) PRCR unlock keys (k_rx_prcr_unlock_*, 0xA50F / 0xA50B / 0xA503 /
    0xA502 / 0xA500). Check the hirakuni45 system.hpp / sysclk.hpp
    macros.

(f) PORT register layout: port struct field offsets for PDR / PODR /
    PIDR / PMR / DSCR / PCR / NCR (anything we use). Cross-check
    against hirakuni45 port_base.hpp.

(g) Pin enum values in rx_port_constants.h -- the (port_idx, pin_num)
    encoding for every k_rx_p#_# enum. We've already had Port J at
    0x12 vs 0x13 break this; check the entire table.

(h) hardware_config.h pin assignments -- for every k_pin_* /
    k_motor_*_in1/in2 / k_pin_enc#_pha/phb / k_pin_drvoff* / k_pin_led*
    constant, confirm the (port, pin) actually exists on the 144-pin
    LFQFP package per hirakuni45 port.hpp comments AND the chosen
    peripheral function exists on that pin per the matching
    port_map_*.hpp.

(i) ICU IRQ vector numbers for any peripheral interrupt we wire up
    (CMT tick, GPTW POEG, SCI RX, etc.). Hirakuni45 icu.hpp has
    `enum class VECTOR : uint8_t { ... }` exhaustive list.

(j) Clock dividers / PLL multiplier / SCKCR value in
    rx_clock_power_init.c. Cross-check against hirakuni45
    examples/clock_set.hpp or sysclk.hpp.

(k) Bit-mask / bit-shift constants like k_*_shift, k_*_mask -- if a
    shift is wrong, every read-modify-write through it lands on the
    wrong bit. Walk every one.

(l) Magic-number defaults: k_default_*, k_*_default, k_iters_per_ms
    (we already had to retune this empirically), k_mtu_period_zero,
    k_encoder_count_reset, etc. Confirm they're sensible against
    hirakuni45 examples or the hardware manual where relevant.

If you encounter a constant our HAL has but hirakuni45 doesn't (e.g.
project-specific enums like motor wiring direction), skip it -- not
your job.

================================================================
## WHAT TO DO

### Phase 1 -- decode and tabulate (NO edits yet)

For each constant in scope:
  1. Identify what hardware feature it represents (the doc comment
     usually says, e.g. "MTU phase counting input PSEL").
  2. Find the matching value in hirakuni45 (or fallback iodefine.h /
     manual). If a constant is package-specific, restrict to 144-pin
     LFQFP (matches our STAR PCB).
  3. Print one line:
       <kind>  <file>:<line>  <name>  ours=<value>  ref=<value>  source=<where>  status=<OK|MISMATCH|UNVERIFIABLE>

Group the table output by kind (PSEL / BASE / OFFSET / MSTP / PRCR /
PORT / PIN_ENUM / PIN_ASSIGN / IRQ / CLOCK / BIT_FIELD / DEFAULT). One
line per constant. Do NOT paste source code from hirakuni45. The
table is what reviewers will eyeball.

UNVERIFIABLE means hirakuni45 doesn't cover this constant AND the
manual doesn't say. Flag those for human review -- don't change them.

### Phase 2 -- fix every MISMATCH

Edit our HAL files to use the reference value. Update the doc comment
too if it cites a wrong PSEL or pin list. Be especially careful with
struct-field offsets -- if hirakuni45 has a reserved-byte gap that we
don't, INSERT the gap (named `reserved` field) -- don't just shift the
later fields, that breaks every other write.

Already-known fixes (do these AND keep looking for more):
  * rx_mpc.h k_psel_mtu_phase: 0x03 -> 0x02
    (hirakuni45 port_map_mtu.hpp clk_a_/clk_b_ uses 0b00'0010 = 0x02
    for MTCLKA on P24 and MTCLKB on P25.)
  * rx_mpc.c rx_mpc_set_mtu_encoder / rx_mpc_set_mtu_pwm /
    rx_mpc_set_tpu_encoder / rx_mpc_set_gptw helpers: enforce the
        PMR=0 -> PFS write -> PMR=1
    sequence in that order. Mirror hirakuni45's clk_a_() structure.

If a constant is shared across multiple call sites, change the
constant once and grep to confirm no caller still relies on the old
value implicitly.

### Phase 3 -- build + test + push

  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"
  bash scripts/hal-audit/devcontainer-exec.sh \
      "cd star-rx72n-firmware/tests && rm -rf build && cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure"

Both must pass. If host unit tests fail because mock TCR/TMDR/PSEL
constants now disagree, update the mocks/tests too -- they are part of
the same coherent change. Do not skip, do not --no-verify.

If build + tests pass:

  git add -A
  git commit -m "$(cat <<'EOF'
hal: cross-check every scalar against hirakuni45/RX RX72N + iodefine.h + manual

Two prior internal-consistency audits passed but MTU encoders still
read 0x0000 because k_psel_mtu_phase was 0x03 -- the chip silicon
needs 0x02. The wrong constant was internally consistent, so the
audit could not flag it.

This pass walked EVERY scalar constant in the HAL and compared it
against:
  1. hirakuni45/RX RX72N headers (community-validated, primary)
  2. Renesas iodefine.h (per-MCU shipped header, fallback)
  3. RX72N HW manual r01uh0824ej0111 (paper truth, last fallback)

Per-category fix counts:
  PSEL constants     <N> verified, <M> fixed
  Register bases     <N> verified, <M> fixed
  Register offsets   <N> verified, <M> fixed
  MSTP bits          <N> verified, <M> fixed
  PRCR keys          <N> verified, <M> fixed
  PORT layout        <N> verified, <M> fixed
  Pin enum values    <N> verified, <M> fixed
  Pin assignments    <N> verified, <M> fixed
  IRQ vectors        <N> verified, <M> fixed
  Clock setup        <N> verified, <M> fixed
  Bit fields         <N> verified, <M> fixed
  Defaults           <N> verified, <M> fixed

Specific MISMATCH fixes (one line each, file:line  name  old -> new):
  [list]

Unverifiable (left untouched, flagged for human review):
  [list, may be empty]

Verified on Pi5:
  - bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh" -> ok
  - host unit tests   -> N/N pass
  - encoder_test on real RX72N -> all 4 motors' encoders count
EOF
)"
  git push origin bsikar/verifying-community-cross-check

If build OR tests fail: do not commit. Print the failure, leave the
working tree dirty so a human can review.

================================================================
## OUTPUT FORMAT (the only thing you print to stdout)

  === PHASE 1: TABLE (every single scalar in scope) ===

  --- PSEL ---
  PSEL  rx_mpc.h:599  k_psel_gptw           ours=0x1E  ref=0x1E   src=hirakuni45 port_map_gptw.hpp  OK
  PSEL  rx_mpc.h:480  k_psel_mtu_phase      ours=0x03  ref=0x02   src=hirakuni45 port_map_mtu.hpp   MISMATCH
  PSEL  rx_mpc.h:470  k_psel_mtu_clk        ours=0x02  ref=0x02   src=hirakuni45 port_map_mtu.hpp   OK
  PSEL  rx_mpc.h:490  k_psel_sci_tx         ours=0x0A  ref=0x0A   src=hirakuni45 port_map_sci.hpp   OK
  ... (every PSEL constant) ...

  --- BASE ---
  BASE  rx72n_mtu_regs.h:204  k_mtu1_base_addr        ours=0x000C1380  ref=0x000C1380  src=hirakuni45 mtu3.hpp  OK
  BASE  rx72n_mtu_regs.h:246  k_mtu_tstra_base_addr   ours=0x000C1280  ref=0x000C1280  src=hirakuni45 mtu3.hpp  OK
  ... (every base address) ...

  --- OFFSET ---
  OFFSET  rx_mtu_channel_regs_t.tcr    ours=+0x00  ref=+0x00  src=hirakuni45 mtu3.hpp  OK
  OFFSET  rx_mtu_channel_regs_t.tmdr   ours=+0x01  ref=+0x01  src=hirakuni45 mtu3.hpp  OK
  ... (every struct field) ...

  --- MSTP ---
  MSTP    k_mtu_mstpa_mtu0_4    ours=bit9    ref=bit9    src=hirakuni45 system.hpp  OK
  MSTP    k_tpu_mstpcra_mstpa13 ours=bit13   ref=bit13   src=hirakuni45 system.hpp  OK
  ... (every MSTP bit) ...

  --- PRCR ---
  PRCR    k_rx_prcr_unlock_all     ours=0xA50F  ref=0xA50F  src=hirakuni45 system.hpp  OK
  ... (every PRCR key) ...

  --- PORT ---
  PORT    rx_port_regs_t.pmr       ours=+0x60   ref=+0x60   src=hirakuni45 port_base.hpp  OK
  ... (every port struct field) ...

  --- PIN_ENUM ---
  PIN_ENUM  k_rx_pj_3   ours=0x1203  ref=0x1203  src=hirakuni45 port.hpp PORTJ at 0x12  OK
  ... (every pin enum) ...

  --- PIN_ASSIGN ---
  PIN_ASSIGN  k_pin_enc0_pha=P24    valid as MTCLKA on 144-pin per hirakuni45 port_map_mtu.hpp  OK
  ... (every pin assignment in hardware_config.h) ...

  --- IRQ ---
  IRQ     k_irq_cmt0_cmi   ours=28   ref=28   src=hirakuni45 icu.hpp VECTOR::CMI0  OK
  ... (every hardcoded IRQ number) ...

  --- CLOCK ---
  CLOCK   k_sckcr_value    ours=0x21C21211  ref=0x21C21211  src=hirakuni45 sysclk.hpp example  OK
  ... (every SCKCR / PLL / divider) ...

  --- BIT_FIELD ---
  BIT_FIELD  k_mtu_tcr_tpsc_shift   ours=0   ref=0   src=hirakuni45 mtu3.hpp BIT.TPSC  OK
  ... (every bit-field shift / mask) ...

  --- DEFAULT ---
  DEFAULT  k_iters_per_ms   ours=40000  ref=N/A (project-specific, retain)  UNVERIFIABLE
  ... (every k_default_* / k_*_default) ...

  === PHASE 2: FIXES APPLIED ===

  [file:line  name  old -> new   one line per fix]

  === PHASE 3: BUILD + TEST ===

  build.sh         -> OK
  ctest            -> 55/55 PASS
  pushed to origin/bsikar/verifying-community-cross-check  (commit <sha>)

================================================================
## CONSTRAINTS

- Be exhaustive in Phase 1. Every constant. If you find yourself
  thinking "this one's obvious, skip it" -- that's exactly the kind
  of constant we want verified, because that's how the PSEL=0x03 bug
  got past two audit rounds.
- Do NOT touch test code unless a constant change requires the
  matching mock/test change (then do them in the same commit).
- Do NOT change struct layout silently -- if a hirakuni45 struct has
  a reserved-byte gap we lack, INSERT a named `reserved` field of the
  right size; don't just shift the later fields.
- Do NOT delete the rx_mpc.h k_psel_mtu_clk / k_psel_mtu_phase split
  even if they end up the same value -- update the docstrings to say
  "same PSEL on RX72N; the distinction is selected by MTU.TMDR" so
  callers stop guessing.
- Do NOT remove a pin from hardware_config.h if hirakuni45 marks it
  package-unavailable on 144-pin -- that's a separate hardware bug
  to flag in the commit body, not "fix" by removing the pin.
- Do NOT use --no-verify on commits. If a hook fails, fix it.
- No wall-time cap; take what you need to be exhaustive.
