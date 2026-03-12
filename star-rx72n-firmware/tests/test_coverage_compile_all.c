/**
 * @file test_coverage_compile_all.c
 * @brief Coverage baseline test -- verifies all firmware source files compile
 *
 * @details
 * This test executable exists solely to compile every firmware source file in
 * libs/ and src/ with host-compatible flags and --coverage instrumentation.
 * The resulting .gcno files allow lcov --initial to record all firmware files
 * in the coverage database, so even untested files appear in the HTML report
 * as 0% coverage rather than being absent from the report entirely.
 *
 * No test cases are registered here. The test passes if and only if all
 * firmware source files compile cleanly against the host test build flags.
 *
 * @note HAL files guarded by #ifdef __RX__ compile on the host but contribute
 * zero instrumented lines. They appear in the coverage report as "N/A".
 *
 * @author Locked, Inc.
 * @date 2026-03-12
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include "unity.h"

/**
 * @brief Unity test framework setup (no-op for this target).
 *
 * @details No state to initialize; test_coverage_compile_all exists solely to
 * compile firmware sources for coverage baseline, not to run test cases.
 *
 * @pre  Unity framework has been initialized by the test runner.
 * @pre  No hardware peripherals are accessed by this compilation-only target.
 * @post Test environment state is unchanged.
 * @post No resources are allocated.
 */
void setUp(void) {}

/**
 * @brief Unity test framework teardown (no-op for this target).
 *
 * @details No state to clean up; this target allocates no resources.
 *
 * @pre  setUp() has been called for the current test.
 * @pre  No test cases have been registered by this target.
 * @post Test environment state is unchanged.
 * @post No resources are released.
 */
void tearDown(void) {}

/**
 * @brief Test runner entry point -- runs zero test cases.
 *
 * @details Calls UNITY_BEGIN() and UNITY_END() with no registered test cases.
 * The sole purpose of this executable is to compile all firmware source files
 * with --coverage instrumentation so lcov --initial can record them in the
 * baseline coverage database.
 *
 * @return int Unity result code: 0 on success (no failures).
 *
 * @pre  All firmware source files linked into this target compile cleanly.
 * @pre  Unity framework headers are available on the include path.
 * @post UNITY_END() has been called and result written to stdout.
 * @post .gcno coverage files exist for all compiled firmware sources.
 */
int main(void)
{
  UNITY_BEGIN();
  /* No test cases: this target exists for coverage baseline compilation only. */
  return UNITY_END();
}
