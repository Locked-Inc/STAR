Add RX72N TMR (8-bit Timer, channels 0..3) to the HAL.

Files (create -- none of these exist yet):
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_tmr_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_tmr.h
- star-rx72n-firmware/libs/rx_hal/src/rx_tmr.c
- star-rx72n-firmware/tests/test_rx_tmr.c
- star-rx72n-firmware/tests/mocks/mock_rx_tmr.h
- star-rx72n-firmware/tests/mocks/mock_rx_tmr.c

Source-of-truth policy:
- Renesas RX72N HW manual is THE authoritative source.
  docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf chapter 32
  "8-Bit Timer (TMR)" (verify chapter number against the table of
  contents -- TMR chapter location varies by manual revision).
  Every constant in our generated code must cite a manual page or
  section number in the doc-comment.
- /tmp/hirakuni45-RX/RX600/tmr.hpp + /tmp/hirakuni45-RX/RX72N/{
  peripheral.hpp (TMR0..3 entries), icu.hpp (CMIA*, CMIB*, OVI*
  vectors), power_mgr.hpp (MSTP bits) } is a CROSS-REFERENCE ONLY --
  use it to find what to look up in the manual, then cite the manual
  page in source comments. Do NOT cite hirakuni45 in source comments,
  file headers, or commit messages. Treat it like a peer's notes --
  helpful for navigation, never the citation. If hirakuni45 and the
  manual disagree, the manual wins, and flag the disagreement in the
  commit body so a human can adjudicate.

Use case for STAR: cheap software-poll timers / one-shots / periodic
tick sources that don't deserve a CMT (CMT is reserved for the ThreadX
1 kHz tick) or a GPTW (GPTWs are reserved for motor PWM). 4 independent
8-bit channels = 4 cheap timers. Pairs (TMR0+TMR1) and (TMR2+TMR3) can
cascade to 16-bit if 8 bits is too coarse.

Implement:
1. rx72n_tmr_regs.h: register struct following the pattern of every
   other rx72n_*_regs.h in the repo (typed enums for addresses, packed
   volatile struct of registers, bit-field constants for each register).
   TMR per channel has: TCR (control), TCSR (control/status), TCORA,
   TCORB, TCNT, TCCR (extended -- 16-bit count + clock select).
   Verify each base address (TMR0, TMR1, TMR2, TMR3 all separate) and
   each field offset against the manual register description tables.
   Note the 16-bit cascade quirk: writes to TMR0/TMR2 affect both
   channels in a pair when the pair is in 16-bit mode -- document
   this clearly.
2. rx_tmr.h: clean public API. Modes:
     k_tmr_mode_8bit_independent
     k_tmr_mode_16bit_cascade   (uses pair, identifies the lower channel only)
   API:
     rx_err_t rx_tmr_init(rx_tmr_channel_t channel,
                          const rx_tmr_config_t* config);
     rx_err_t rx_tmr_start(rx_tmr_channel_t channel);
     rx_err_t rx_tmr_stop(rx_tmr_channel_t channel);
     rx_err_t rx_tmr_read(rx_tmr_channel_t channel, uint16_t* out_count);
     rx_err_t rx_tmr_set_period_us(rx_tmr_channel_t channel,
                                   uint32_t period_us);
     rx_err_t rx_tmr_register_compare_match_isr(
                          rx_tmr_channel_t channel,
                          void (*cb)(void*), void* ctx);
     rx_err_t rx_tmr_deinit(rx_tmr_channel_t channel);
   Config struct: clock source (PCLK/1, /2, /8, /32, /64, /1024, /8192,
   external TMCI), counter clear source (TCORA match / TCORB match /
   external pin / none), output behavior on TMOn pin if any, optional
   compare-match interrupt enable.
3. rx_tmr.c: driver. Must
     - Clear MSTPCRA.MSTPxxx for the requested channel under PRCR
       unlock (TMR0/1 share an MSTP bit, TMR2/3 share another -- verify
       both bits against the manual).
     - Configure TCR/TCSR before enabling the timer (start = clock
       source != stop in TCCR.CKS).
     - rx_tmr_set_period_us() converts microseconds to (TCORA, prescaler)
       choosing the smallest prescaler that fits the target period in 8
       bits (or 16 bits in cascade mode).
     - For ISR registration: hook the matching ICU::CMIA/CMIB vector,
       verify the vector number against rx72n_icu_regs.h.
     - Follow rx_register_protection.h for PRCR sequencing.
     - NASA Power of 10 compliant (>=2 pre/post conditions per fn,
       no goto/recursion, return-value check on every call).
4. tests/test_rx_tmr.c + mocks: same shape as test_rx_iwdt.c +
   mocks/mock_rx_iwdt.{h,c}. Cover all 4 channels in 8-bit mode,
   one cascaded pair in 16-bit mode, period conversion edge cases
   (1us, 1ms, 1s), ISR registration and dispatch via mock IRQ table.
5. Wire test_rx_tmr into tests/CMakeLists.txt next to test_rx_iwdt.

Coding style -- MUST follow /workspaces/STAR/CLAUDE.md. The rules that
will absolutely come up while writing this peripheral:

- All headers use `#pragma once` (no traditional include guards).
- All files pure 7-bit ASCII (no em-dashes, no Unicode tree drawings,
  no smart quotes). The pre-commit hook + CI ASCII Check will reject
  the commit otherwise. Use `--` for em-dashes and `|--` `` `-- `` for
  tree drawings if any.
- Naming:  functions/vars `snake_case`,  types `snake_case_t`,
  static/file-scope vars `s_<name>`,  static (testable) functions
  `internal_<name>`,  private helpers `priv_<name>`,  globals avoided
  (and `g_` if unavoidable).
- ZERO magic numbers. Every numeric literal -- including bit shifts,
  array indices, register offsets, address constants -- MUST be a C23
  typed enum with an explicit underlying type:
      typedef enum : uint8_t  { ... } foo_t;
      typedef enum : uintptr_t{ k_xxx_base = 0x000C1380, ... } addresses_t;
  Use `uintptr_t` for register-base-address enums (NOT uint32_t -- it
  silently truncates on the 64-bit unit-test host). uint8_t for shifts
  and small constants, uint16_t for medium, uint32_t for masks.
- Floating-point constants only via `static const float s_xxx = ...F;`
  (enum can't hold floats).
- Hardware register access via inline accessors that return
  `volatile <type> *` (NOT macros) -- see existing mtu1() / tpu_control()
  / system_regs() / prcr_reg() in libs/rx_hal/inc/. Add new accessors
  for the new peripheral.
- NASA Power of 10:
    1. No goto / setjmp / recursion.
    2. All loops have static upper bounds (use the typed enum).
    3. No dynamic allocation after init -- everything static.
    4. Functions ~60 lines max (clang-tidy CI fails at 60+).
    5. At least 2 pre-conditions and 2 post-conditions per function
       (use RX_CHECK_NULL_PTR / RX_VALIDATE_PTR / RX_VALIDATE_INIT).
    7. Every function-call return value either checked or explicitly
       cast to (void).  Use RX_RETURN_ON_ERROR.
    8. Macros only for: code-deduplication (RX_RETURN_ON_ERROR style),
       conditional compilation, build-config flags. NOT for register
       addresses -- use accessors.
    9. Function pointers ONLY for the dependency-injection interface
       struct pattern (test mock injection); document why with @par.
   10. -Wall -Wextra -Werror; CI fails on any warning.
- Doxygen on EVERY file, function, struct, enum, variable, typedef,
  and macro. Use ALL applicable tags. Reference rx_pid_compute() in
  libs/rx_pid/src/rx_pid.c as the canonical example. Minimum per fn:
  @brief, @details, @param[in/out], @return, @retval (every value),
  @pre (>=2), @post (>=2), @note (thread safety), @see, @since.
- Inclusive terminology: Controller / Peripheral, COPI / CIPO, CS,
  Primary / Main. NOT master / slave / MOSI / MISO / SS.
- No backward-compatibility shims, no deprecation aliases, no
  function-name `#define`s -- per CLAUDE.md "Backward Compatibility
  Policy": zero compat layers, breaking changes encouraged.
- Tests follow the same style and live under tests/. Mocks under
  tests/mocks/. Match the exact shape of existing test_rx_iwdt.c +
  mocks/mock_rx_iwdt.{h,c}.
- Run `python3 scripts/utils/fix-encoding.py --check <file>` on every
  new file before committing to confirm ASCII-only.
- Run clang-format-15 on every new .c/.h before committing.

After edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"
  bash scripts/hal-audit/devcontainer-exec.sh \
      "cd star-rx72n-firmware/tests && rm -rf build && cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure"

Both must pass. If either fails, do not commit; print the failure.

If both pass:
  git add -A
  git commit -m "lib(tmr): add 8-Bit Timer (TMR0..3) driver per RX72N HW manual"
  git push origin bsikar/verifying-tmr

Output:
  OK  <file>:<line> <const>=<value>  (manual page <N>)
  NEW <file>:<line> <const>=<value>  (manual page <N>)

No wall-time cap. Take the time you need to do this right.
