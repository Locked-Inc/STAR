Add RX72N CAC (Clock Frequency Accuracy Measurement Circuit) to the HAL.

Files (create -- none of these exist yet):
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_cac_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_cac.h
- star-rx72n-firmware/libs/rx_hal/src/rx_cac.c
- star-rx72n-firmware/tests/test_rx_cac.c
- star-rx72n-firmware/tests/mocks/mock_rx_cac.h
- star-rx72n-firmware/tests/mocks/mock_rx_cac.c

Source-of-truth policy:
- Renesas RX72N HW manual is THE authoritative source.
  docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf chapter 11
  "Clock Frequency Accuracy Measurement Circuit (CAC)".
  Every constant in our generated code must cite a manual page or
  section number in the doc-comment.
- /tmp/hirakuni45-RX/RX72N/{cac.hpp -- if present, peripheral.hpp,
  icu.hpp, power_mgr.hpp} is a CROSS-REFERENCE ONLY -- use it to find
  what to look up in the manual, then cite the manual page in source
  comments. Do NOT cite hirakuni45 in source comments, file headers,
  or commit messages. Treat it like a peer's notes -- helpful for
  navigation, never the citation.
  If hirakuni45 and the manual disagree, the manual wins, and flag the
  disagreement in the commit body so a human can adjudicate.

Use case for STAR: monitor MOSC (24 MHz crystal) against HOCO (16 MHz
internal). If MOSC drifts or stops, raise an IRQ so the motor stack can
e-stop instead of running on a glitched clock. PCLKA and the GPTW PWM
period both depend on MOSC -> PLL; a bad MOSC silently breaks current
sensing windows + PWM phasing.

Implement:
1. rx72n_cac_regs.h: register struct following the pattern of every
   other rx72n_*_regs.h in the repo (typed enums for addresses, packed
   volatile struct of registers, bit-field constants for each register).
   CAC has: CACR0 (control), CACR1 (clock-edge select), CACR2 (digital
   filter + ref clock div), CAICR (interrupt control + flag clear),
   CASTR (status flags FERRF/MENDF/OVFF), CAULVR (upper-limit value),
   CALLVR (lower-limit value), CACNTBR (counter buffer / readback).
   Verify the base address against hirakuni45 cac.hpp + manual section
   11.2. Verify each field offset, bit position, mask.
2. rx_cac.h: clean public API. At minimum:
     rx_err_t rx_cac_init(const rx_cac_config_t* config);
     rx_err_t rx_cac_start(void);
     rx_err_t rx_cac_stop(void);
     bool     rx_cac_check(uint32_t* out_count);
     rx_err_t rx_cac_deinit(void);
   Config struct should let the caller pick: measured-clock source
   (typed enum -- MOSC / HOCO / LOCO / MAINOSC / IWDTCLK / PCLKB /
   PLLCK), reference-clock source (CACREF pin, internal HOCO, etc.),
   ref-clock divider (1/32/100/8192), upper limit, lower limit, and
   whether to enable the FERRF interrupt.
3. rx_cac.c: driver. Must
     - Clear MSTPCRC.MSTPCxx (verify exact bit against hirakuni45
       power_mgr.hpp) under PRCR unlock.
     - Configure CACR1/CACR2/CAULVR/CALLVR before setting CACR0.CFME.
     - rx_cac_check() reads CASTR + CACNTBR.
     - Follow rx_register_protection.h for PRCR sequencing.
     - Use rx_log_error / rx_log_info for diagnostics.
     - NASA Power of 10 compliant (>=2 pre/post conditions per fn,
       no goto/recursion, return-value check on every call).
4. tests/test_rx_cac.c + mocks: same shape as test_rx_iwdt.c +
   mocks/mock_rx_iwdt.{h,c}. Cover init / start / stop / check /
   deinit error paths and a happy-path measure-and-verify cycle
   using the mock.
5. Wire test_rx_cac into tests/CMakeLists.txt next to test_rx_iwdt.

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
  git commit -m "lib(cac): add Clock Frequency Accuracy Measurement Circuit driver per RX72N HW manual"
  git push origin bsikar/verifying-cac

Output:
  OK  <file>:<line> <const>=<value>  (source: <manual page or hirakuni45 file>)
  NEW <file>:<line> <const>=<value>  (source: <manual page or hirakuni45 file>)

No wall-time cap. Take the time you need to do this right.
