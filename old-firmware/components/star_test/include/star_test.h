/* esp32-firmware/components/star_test/include/star_test.h */

/**
 * @file star_test.h
 * @brief STAR Testing Framework - Lightweight testing for ESP32
 *
 * Simple, embedded-friendly testing framework designed specifically
 * for the STAR ESP32 firmware project.
 */

#ifndef STAR_TEST_H
#define STAR_TEST_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Test result structure */
typedef struct {
  uint32_t total;
  uint32_t passed;
  uint32_t failed;
} star_test_results_t;

/* Test function pointer type */
typedef void (*star_test_func_t)(void);

/* Test registration structure */
typedef struct star_test_case {
  const char*            name;
  const char*            group;
  star_test_func_t       func;
  struct star_test_case* next;
} star_test_case_t;

/* Global test results */
extern star_test_results_t g_star_test_results;
extern star_test_case_t*   g_star_test_current;

/* Test registration functions */
void star_test_register(star_test_case_t* test);

/* Test runner functions */
void                star_test_run_all(void);
void                star_test_run_group(const char* group);
star_test_results_t star_test_get_results(void);
void                star_test_print_results(void);

/* Test configuration functions */
void star_test_set_verbose(bool verbose);

/* Internal assertion failure handler */
void star_test_fail(const char* file, int32_t line, const char* message);

/**
 * @brief Define a test case
 *
 * Usage:
 *   STAR_TEST_CASE(group, test_name) {
 *     // test code here
 *   }
 *
 * Note: After defining tests, add them to the test list using STAR_TEST_LIST_BEGIN/END
 */
#define STAR_TEST_CASE(group_name, test_name)                                                      \
  static void             test_##group_name##_##test_name(void);                                   \
  static star_test_case_t test_case_##group_name##_##test_name = {                                 \
    .name  = #test_name,                                                                           \
    .group = #group_name,                                                                          \
    .func  = test_##group_name##_##test_name,                                                      \
    .next  = NULL};                                                                                 \
  static void test_##group_name##_##test_name(void)

/**
 * @brief Begin the test list definition
 *
 * Usage:
 *   STAR_TEST_LIST_BEGIN()
 *     STAR_TEST_REF(group, test_name)
 *     ...
 *   STAR_TEST_LIST_END()
 */
#define STAR_TEST_LIST_BEGIN() static star_test_case_t* star_test_list[] = {

/**
 * @brief Reference a test in the test list
 */
#define STAR_TEST_REF(group_name, test_name) &test_case_##group_name##_##test_name,

/**
 * @brief End the test list and create registration function
 *
 * Uses GCC constructor attribute to automatically register tests
 * when the module loads, avoiding linker conflicts between multiple
 * test files.
 */
#define STAR_TEST_LIST_END()                                                                       \
  NULL /* sentinel */                                                                              \
  }                                                                                                \
  ;                                                                                                \
  __attribute__((constructor)) static void star_test_register_module(void)                         \
  {                                                                                                \
    for (uint32_t i = 0; star_test_list[i] != NULL; i++) {                                         \
      star_test_register(star_test_list[i]);                                                       \
    }                                                                                              \
  }

/**
 * @brief Assert that a condition is true
 */
#define STAR_ASSERT(condition)                                                                     \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      star_test_fail(__FILE__, __LINE__, "Assertion failed: " #condition);                         \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that two values are equal
 */
#define STAR_ASSERT_EQUAL(expected, actual)                                                        \
  do {                                                                                             \
    if ((expected) != (actual)) {                                                                  \
      char msg[128];                                                                               \
      snprintf(msg,                                                                                \
               sizeof(msg),                                                                        \
               "Expected %" PRId32 ", got %" PRId32 "",                                            \
               (int32_t)(expected),                                                                \
               (int32_t)(actual));                                                                 \
      star_test_fail(__FILE__, __LINE__, msg);                                                     \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that two values are not equal
 */
#define STAR_ASSERT_NOT_EQUAL(expected, actual)                                                    \
  do {                                                                                             \
    if ((expected) == (actual)) {                                                                  \
      char msg[128];                                                                               \
      snprintf(msg, sizeof(msg), "Expected not equal to %" PRId32 "", (int32_t)(expected));        \
      star_test_fail(__FILE__, __LINE__, msg);                                                     \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that a value is NULL
 */
#define STAR_ASSERT_NULL(ptr)                                                                      \
  do {                                                                                             \
    if ((ptr) != NULL) {                                                                           \
      star_test_fail(__FILE__, __LINE__, "Expected NULL pointer");                                 \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that a value is not NULL
 */
#define STAR_ASSERT_NOT_NULL(ptr)                                                                  \
  do {                                                                                             \
    if ((ptr) == NULL) {                                                                           \
      star_test_fail(__FILE__, __LINE__, "Expected non-NULL pointer");                             \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief Assert that a condition is true (alias for STAR_ASSERT)
 */
#define STAR_ASSERT_TRUE(condition) STAR_ASSERT(condition)

/**
 * @brief Assert that a condition is false
 */
#define STAR_ASSERT_FALSE(condition) STAR_ASSERT(!(condition))

/**
 * @brief Assert that two strings are equal
 */
#define STAR_ASSERT_STR_EQUAL(expected, actual)                                                    \
  do {                                                                                             \
    if (strcmp((expected), (actual)) != 0) {                                                       \
      char msg[512];                                                                               \
      snprintf(msg, sizeof(msg), "Expected \"%s\", got \"%s\"", (expected), (actual));             \
      star_test_fail(__FILE__, __LINE__, msg);                                                     \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif /* STAR_TEST_H */
