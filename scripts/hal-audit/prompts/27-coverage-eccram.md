Backfill the rx_eccram unit-test suite to 100% / 100% / 100% (line /
function / branch) coverage so it can rejoin the libs/ coverage gate.

Files (modify -- add new tests, fixtures, mocks as needed):
- star-rx72n-firmware/tests/test_rx_eccram.c
- star-rx72n-firmware/tests/mocks/mock_rx_eccram.{h,c}    (extend if needed)
- star-rx72n-firmware/libs/rx_hal/src/rx_eccram.c         (add GCOVR_EXCL_LINE
                                                        / GCOVR_EXCL_BR_LINE
                                                        only for genuinely
                                                        unreachable code,
                                                        with comment)

Source-of-truth policy:
- Every test must exercise documented behavior, not implementation
  details. If a branch in rx_eccram.c is unreachable from any caller,
  delete the branch rather than test it. If unreachable on host but
  reachable on RX (e.g. #ifdef __RX__ blocks), mark the line/branch
  with GCOVR_EXCL_LINE / GCOVR_EXCL_BR_LINE plus a comment explaining
  why.
- Reference the RX72N HW manual chapter 8 (RAM) (ECC-protected RAM) when adding tests that pin specific
  register-bit semantics. Cite the manual page in the test docstring.
- Do NOT cite hirakuni45 in source comments. Use it for navigation
  only.

Procedure:
1. Run the local coverage workflow to see exact gaps:
     bash scripts/hal-audit/devcontainer-exec.sh \
       "cd star-rx72n-firmware/tests && rm -rf build && \
        cmake -S . -B build -DCMAKE_C_FLAGS='--coverage' && \
        cmake --build build -j && \
        ctest --test-dir build --output-on-failure && \
        gcovr --object-directory build --filter '../libs/rx_hal/src/rx_eccram.c' \
              --gcov-executable 'llvm-cov-18 gcov' --txt"
   Use the gcovr report to identify each uncovered line and branch.

2. For each uncovered line / branch, decide:
     a. Genuine missing test case -> add a Unity test in test_rx_eccram.c
        that exercises it via the public API. Prefer happy-path +
        error-path pairs.
     b. Defensive code that can't be reached from public API -> delete
        it (NASA Power of 10 frowns on dead code).
     c. #ifdef __RX__ block -> add GCOVR_EXCL_LINE / GCOVR_EXCL_BR_LINE
        with a one-line comment.

3. Run again and confirm 100% / 100% / 100% on rx_eccram.c specifically:
     gcovr ... --filter '.*/rx_eccram.c' --fail-under-line 100 \
                                       --fail-under-function 100 \
                                       --fail-under-branch 100

4. Drop the rx_eccram excludes from .github/workflows/firmware-unit-tests.yml:
     --exclude '.*/rx_hal/inc/rx72n_cac_regs.h'
     --exclude '.*/rx_hal/inc/rx_eccram.h'
     --exclude '.*/rx_hal/src/rx_eccram.c'

5. Run the full host suite + cross-compile build to confirm nothing
   else broke:
     bash scripts/hal-audit/devcontainer-exec.sh \
       "cd star-rx72n-firmware && bash build.sh"
     bash scripts/hal-audit/devcontainer-exec.sh \
       "cd star-rx72n-firmware/tests && rm -rf build && \
        cmake -S . -B build && cmake --build build -j && \
        ctest --test-dir build --output-on-failure"

Coding style -- MUST follow /workspaces/STAR/CLAUDE.md. The rules that
will absolutely come up while writing these tests:

- All headers use `#pragma once` (no traditional include guards).
- All files pure 7-bit ASCII (no em-dashes, no Unicode tree drawings,
  no smart quotes). The pre-commit hook + CI ASCII Check will reject
  the commit otherwise. Use `--` for em-dashes.
- Naming:  test functions `test_<area>_<scenario>` snake_case;
  static/file-scope vars `s_<name>`; test fixtures `make_<name>` and
  `setUp` / `tearDown` per Unity convention.
- ZERO magic numbers in test fixtures. Every numeric literal -- counts,
  divider values, bit indices, expected reads -- MUST be a C23 typed
  enum with an explicit underlying type. Existing examples:
      typedef enum : uint16_t { k_test_eccram_value_first = 1234U, ... }
  Add new k_test_* enums alongside as needed.
- Every test function must have at least one TEST_ASSERT_*. Pure
  side-effect tests are not allowed.
- Doxygen file-level + function-level on EVERY new test function:
  @brief, @details, @par Manual reference (manual page citing
  the relevant ECCRAM register semantics), @since.
- NASA Power of 10:
    1. No goto / setjmp / recursion.
    4. Functions ~60 lines max (clang-tidy CI fails at 60+).
    5. At least 2 TEST_ASSERT_* per test function (one for the
       "thing under test", one for the side effect / state change).
    7. Every rx_doc / rx_eccram return value either checked with
       TEST_ASSERT_EQUAL(k_rx_ok, ...) or explicitly cast to (void).
   10. -Wall -Wextra -Werror; CI fails on any warning.
- Inclusive terminology: Controller / Peripheral, COPI / CIPO, CS,
  Primary / Main. NOT master / slave / MOSI / MISO / SS.
- Run `python3 scripts/utils/fix-encoding.py --check <file>` on every
  new file before committing.
- Run clang-format-15 (or -18) on every new .c/.h before committing.

After edits:
  bash scripts/hal-audit/devcontainer-exec.sh "cd star-rx72n-firmware && bash build.sh"
  bash scripts/hal-audit/devcontainer-exec.sh \
      "cd star-rx72n-firmware/tests && rm -rf build && cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure"

Both must pass. CAC-specific coverage must hit 100/100/100. If any
gate fails, do not commit; print the failure.

If everything passes:
  git add -A
  git commit -m "test(rx_eccram): backfill coverage to 100/100/100 + drop CI exclude"
  git push origin bsikar/verifying-coverage-eccram

No wall-time cap. Take the time you need to do this right -- the
purpose of this prompt is to RAISE quality, not to ship cheap tests
just to pass coverage. If you find yourself adding tests that don't
actually verify documented behavior, stop and reconsider.
