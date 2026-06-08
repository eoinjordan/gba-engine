// Minimal single-header test framework for host-side unit tests.
//
// The GBA engine's hardware-facing code (gba_system.c, the rendering paths in
// engine.c) can only really be exercised on real hardware or in an emulator.
// But plenty of the engine is plain, portable C — most notably the bytecode
// VM/script-runner in vm.c — and that logic can and should be unit tested on
// the host with a normal compiler. This header provides just enough scaffolding
// to do that without pulling in an external dependency.
//
// Usage:
//   TEST(my_test_name) { ASSERT_EQ(1 + 1, 2); }
//   ...
//   int main(void) {
//     RUN_TEST(my_test_name);
//     return TEST_REPORT();
//   }

#ifndef GBA_ENGINE_TEST_FRAMEWORK_H
#define GBA_ENGINE_TEST_FRAMEWORK_H

#include <stdio.h>

static int gba_test_pass_count = 0;
static int gba_test_fail_count = 0;
static int gba_test_current_failed = 0;

#define TEST(name) static void gba_test_##name(void)

#define RUN_TEST(name)                                                         \
  do {                                                                         \
    gba_test_current_failed = 0;                                               \
    gba_test_##name();                                                         \
    if (gba_test_current_failed) {                                             \
      gba_test_fail_count++;                                                   \
      printf("[FAIL] %s\n", #name);                                            \
    } else {                                                                   \
      gba_test_pass_count++;                                                   \
      printf("[ ok ] %s\n", #name);                                            \
    }                                                                          \
  } while (0)

#define GBA_TEST_FAIL(msg)                                                     \
  do {                                                                         \
    gba_test_current_failed = 1;                                               \
    printf("       %s:%d: %s\n", __FILE__, __LINE__, msg);                     \
  } while (0)

#define ASSERT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      GBA_TEST_FAIL("expected true: " #cond);                                  \
    }                                                                          \
  } while (0)

#define ASSERT_FALSE(cond)                                                     \
  do {                                                                         \
    if (cond) {                                                                \
      GBA_TEST_FAIL("expected false: " #cond);                                 \
    }                                                                          \
  } while (0)

#define ASSERT_EQ(actual, expected)                                            \
  do {                                                                         \
    long gba_test_a = (long)(actual);                                          \
    long gba_test_e = (long)(expected);                                        \
    if (gba_test_a != gba_test_e) {                                            \
      gba_test_current_failed = 1;                                             \
      printf("       %s:%d: %s != %s (got %ld, expected %ld)\n", __FILE__,     \
             __LINE__, #actual, #expected, gba_test_a, gba_test_e);            \
    }                                                                          \
  } while (0)

#define ASSERT_NULL(ptr)                                                       \
  do {                                                                         \
    if ((ptr) != NULL) {                                                       \
      GBA_TEST_FAIL("expected NULL: " #ptr);                                   \
    }                                                                          \
  } while (0)

#define ASSERT_NOT_NULL(ptr)                                                   \
  do {                                                                         \
    if ((ptr) == NULL) {                                                       \
      GBA_TEST_FAIL("expected non-NULL: " #ptr);                               \
    }                                                                          \
  } while (0)

#define TEST_REPORT()                                                          \
  (printf("\n%d passed, %d failed\n", gba_test_pass_count,                     \
          gba_test_fail_count),                                                \
   gba_test_fail_count == 0 ? 0 : 1)

#endif // GBA_ENGINE_TEST_FRAMEWORK_H
