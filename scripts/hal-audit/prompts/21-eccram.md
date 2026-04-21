Add RX72N ECCRAM (ECC-protected RAM) configuration to the HAL.

Files (create -- none of these exist yet):
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_eccram_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_eccram.h
- star-rx72n-firmware/libs/rx_hal/src/rx_eccram.c
- star-rx72n-firmware/tests/test_rx_eccram.c
- star-rx72n-firmware/tests/mocks/mock_rx_eccram.h
- star-rx72n-firmware/tests/mocks/mock_rx_eccram.c

Source-of-truth policy:
- Renesas RX72N HW manual is THE authoritative source.
  docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf chapter 8
  "RAM" (find the ECCRAM subsection -- search for "ECC", "ECRAM",
  "single-bit error correction", "double-bit error detection").
  The ECCRAM region is a separate 32 KB block on RX72N (verify exact
  size and physical address range against the manual memory map).
  Every constant in our generated code must cite a manual page or
  section number in the doc-comment.
- /tmp/hirakuni45-RX/RX72N/{ peripheral.hpp (ECCRAM entry),
  power_mgr.hpp (MSTP bit if any), icu.hpp (ECCRAM 1-bit / 2-bit
  error vectors), system.hpp } and any /tmp/hirakuni45-RX/RX600/
  ECCRAM-related .hpp files are CROSS-REFERENCE ONLY -- use them to
  find what to look up in the manual, then cite the manual page in
  source comments. Do NOT cite hirakuni45 in source comments, file
  headers, or commit messages. Treat it like a peer's notes -- helpful
  for navigation, never the citation. If hirakuni45 and the manual
  disagree, the manual wins, and flag the disagreement in the commit
  body so a human can adjudicate.

Use case for STAR: bit-flip-resistant storage for safety-critical
mutable state -- motor PID accumulators, last-good encoder counts,
frame sequence numbers, the watchdog kick counter, and the
shared_data_t handoff between motor_control_task and comm_task.
Single-bit errors get corrected silently; double-bit errors raise an
NMI/interrupt so we can e-stop instead of acting on garbage.

Implement:
1. rx72n_eccram_regs.h: register struct following the pattern of every
   other rx72n_*_regs.h in the repo (typed enums for addresses, packed
   volatile struct of registers, bit-field constants for each register).
   The ECCRAM control block has at minimum: ECCRAM enable (RAMECCEN /
   ECCMODE), ECC mode select (correct-only vs correct-and-detect vs
   detect-only), 1-bit error status + flag clear, 2-bit error status +
   flag clear, error address capture register. Verify register names,
   addresses, and bit layouts against the manual.
2. rx_eccram.h: clean public API.
     rx_err_t rx_eccram_init(rx_eccram_mode_t mode);
     rx_err_t rx_eccram_get_error_status(rx_eccram_status_t* out);
     rx_err_t rx_eccram_clear_errors(void);
     rx_err_t rx_eccram_register_error_isr(
                          void (*on_1bit)(uintptr_t addr, void* ctx),
                          void (*on_2bit)(uintptr_t addr, void* ctx),
                          void* ctx);
     uintptr_t rx_eccram_region_start(void);
     uintptr_t rx_eccram_region_end(void);
   Mode enum:
     k_eccram_mode_disabled
     k_eccram_mode_correct_only         (no double-bit detection)
     k_eccram_mode_correct_and_detect   (correct 1-bit, detect 2-bit -- recommended)
     k_eccram_mode_detect_only          (detect both, no correction)
3. rx_eccram.c: driver. Must
     - Clear any required MSTP bit (verify manual + power_mgr.hpp).
     - Configure ECC mode register under PRCR unlock if protected.
     - On init, write 0 across the entire ECCRAM region using 32-bit
       stores so the ECC syndrome matches the data on first read
       (uninitialized RAM has random ECC bits and would falsely flag).
     - Hook 1-bit and 2-bit error vectors per ICU table.
     - 2-bit error handler must save the failing address before
       clearing the status flag.
     - Provide a build-time symbol (or linker symbol) the linker
       script can use to PLACE specific .data / .bss sections in the
       ECCRAM region. Add an example `.eccram` section to the linker
       script (linker.ld in apps that should use it).
     - NASA Power of 10 compliant (>=2 pre/post conditions per fn,
       no goto/recursion, return-value check on every call).
4. tests/test_rx_eccram.c + mocks: same shape as test_rx_iwdt.c +
   mocks/mock_rx_iwdt.{h,c}. Cover init in each mode, simulated
   1-bit / 2-bit error injection via mock, ISR dispatch, address
   capture.
5. Wire test_rx_eccram into tests/CMakeLists.txt next to test_rx_iwdt.

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
  git commit -m "lib(eccram): add ECC-protected RAM driver per RX72N HW manual"
  git push origin bsikar/verifying-eccram

Output:
  OK  <file>:<line> <const>=<value>  (manual page <N>)
  NEW <file>:<line> <const>=<value>  (manual page <N>)

No wall-time cap. Take the time you need to do this right.
