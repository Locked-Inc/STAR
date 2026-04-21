Add RX72N DOC (Data Operation Circuit) to the HAL.

Files (create -- none of these exist yet):
- star-rx72n-firmware/libs/rx_hal/inc/rx72n_doc_regs.h
- star-rx72n-firmware/libs/rx_hal/inc/rx_doc.h
- star-rx72n-firmware/libs/rx_hal/src/rx_doc.c
- star-rx72n-firmware/tests/test_rx_doc.c
- star-rx72n-firmware/tests/mocks/mock_rx_doc.h
- star-rx72n-firmware/tests/mocks/mock_rx_doc.c

Source-of-truth policy:
- Renesas RX72N HW manual is THE authoritative source.
  docs/rx72n-manual/r01uh0824ej0111_rx72n-2931480.pdf chapter 67
  "Data Operation Circuit (DOC)" (verify chapter number against the
  table of contents -- DOC chapter location varies by manual revision).
  Every constant in our generated code must cite a manual page or
  section number in the doc-comment.
- /tmp/hirakuni45-RX/RX600/doc.hpp + /tmp/hirakuni45-RX/RX72N/{
  peripheral.hpp, icu.hpp (DOPCF vector), power_mgr.hpp } is a
  CROSS-REFERENCE ONLY -- use it to find what to look up in the manual,
  then cite the manual page in source comments. Do NOT cite hirakuni45
  in source comments, file headers, or commit messages. Treat it like
  a peer's notes -- helpful for navigation, never the citation. If
  hirakuni45 and the manual disagree, the manual wins, and flag the
  disagreement in the commit body so a human can adjudicate.

Use case for STAR: hardware-accelerated integrity check on PID gains,
shared_data block, and the read-only firmware ID block. DOC compares
two 16-bit words in one cycle and raises DOPCF on match/mismatch -- much
faster than a software CRC for short repeated checks. Candidate uses:
periodic shared_data canary verify in motor_control_task; runtime
check that DRV8263H register cache hasn't been corrupted by EMI.

Implement:
1. rx72n_doc_regs.h: register struct following the pattern of every
   other rx72n_*_regs.h in the repo (typed enums for addresses, packed
   volatile struct of registers, bit-field constants for each register).
   DOC has: DOCR (control: OMS[1:0] mode select, DCSEL detect, DOPCFCL
   flag clear, DOPCIE intr enable, DOPCF status), DODIR (16-bit data
   input register), DODSR (16-bit data setting register / output).
   Verify base address against the manual section 67.2 register
   descriptions. Verify each field offset, bit position, mask.
2. rx_doc.h: clean public API. Modes:
     k_doc_mode_compare       (raise DOPCF when DODIR == DODSR)
     k_doc_mode_compare_neq   (raise when !=)
     k_doc_mode_add           (DODSR = DODSR + DODIR)
     k_doc_mode_subtract      (DODSR = DODSR - DODIR; flag on borrow)
   API:
     rx_err_t rx_doc_init(rx_doc_mode_t mode);
     rx_err_t rx_doc_set_reference(uint16_t reference);  /* writes DODSR */
     rx_err_t rx_doc_compare(uint16_t value, bool* out_match);
     rx_err_t rx_doc_add(uint16_t value, uint16_t* out_sum);
     rx_err_t rx_doc_subtract(uint16_t value, uint16_t* out_diff,
                              bool* out_borrow);
     rx_err_t rx_doc_deinit(void);
3. rx_doc.c: driver. Must
     - Clear MSTPCRA.MSTPxxx (verify exact bit against manual section
       11.2.1 + hirakuni45 power_mgr.hpp) under PRCR unlock.
     - Configure DOCR before any data operation.
     - DOPCF flag is write-1-to-clear -- handle correctly.
     - Follow rx_register_protection.h for PRCR sequencing.
     - Use rx_log_error / rx_log_info for diagnostics.
     - NASA Power of 10 compliant (>=2 pre/post conditions per fn,
       no goto/recursion, return-value check on every call).
4. tests/test_rx_doc.c + mocks: same shape as test_rx_iwdt.c +
   mocks/mock_rx_iwdt.{h,c}. Cover all 4 modes + error paths
   (uninitialized, NULL pointers, double-init).
5. Wire test_rx_doc into tests/CMakeLists.txt next to test_rx_iwdt.

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
  git commit -m "lib(doc): add Data Operation Circuit driver per RX72N HW manual"
  git push origin bsikar/verifying-doc

Output:
  OK  <file>:<line> <const>=<value>  (manual page <N>)
  NEW <file>:<line> <const>=<value>  (manual page <N>)

No wall-time cap. Take the time you need to do this right.
