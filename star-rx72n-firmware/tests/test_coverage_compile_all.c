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
 * @since Version 1.0.0
 */

#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

int main(void)
{
  UNITY_BEGIN();
  /* No test cases: this target exists for coverage baseline compilation only. */
  return UNITY_END();
}
