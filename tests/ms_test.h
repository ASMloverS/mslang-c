// ms_test.h
// Zero-dependency single-header C unit test framework.
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// module-private counters, single-threaded test runner only
static int gMsTestPassed = 0;
static int gMsTestFailed = 0;
static const char* gMsTestCurrent = NULL;

// Run a test function and record current test name.
#define MS_RUN(fn) do {              \
  gMsTestCurrent = #fn;            \
  fn();                            \
} while (0)

// Assert integer equality (widened to int64_t).
#define MS_ASSERT_EQ(actual, expected, msg) do {                          \
  int64_t a_ = (int64_t)(actual);                                        \
  int64_t e_ = (int64_t)(expected);                                      \
  if (a_ == e_) { gMsTestPassed++; }                                     \
  else {                                                                  \
    fprintf(stderr, "FAIL [%s] %s: got %lld, want %lld\n",              \
        gMsTestCurrent, (msg), (long long)a_, (long long)e_);           \
    gMsTestFailed++;                                                     \
  }                                                                       \
} while (0)

// Assert string equality (NULL-safe).
#define MS_ASSERT_STR_EQ(actual, expected, msg) do {                      \
  const char* a_ = (actual);                                             \
  const char* e_ = (expected);                                           \
  if (a_ && e_ && strcmp(a_, e_) == 0) { gMsTestPassed++; }             \
  else {                                                                  \
    fprintf(stderr, "FAIL [%s] %s: got \"%s\", want \"%s\"\n",          \
        gMsTestCurrent, (msg), a_ ? a_ : "(null)", e_);                 \
    gMsTestFailed++;                                                     \
  }                                                                       \
} while (0)

// Assert condition is true.
#define MS_ASSERT_TRUE(cond, msg) do {                                    \
  if (cond) { gMsTestPassed++; }                                         \
  else {                                                                  \
    fprintf(stderr, "FAIL [%s] %s: condition is false\n",               \
        gMsTestCurrent, (msg));                                          \
    gMsTestFailed++;                                                     \
  }                                                                       \
} while (0)

// Assert condition is false.
#define MS_ASSERT_FALSE(cond, msg) MS_ASSERT_TRUE(!(cond), msg)

// Assert two memory regions are equal.
#define MS_ASSERT_MEM_EQ(actual, expected, len, msg) do {                 \
  const void* a_ = (actual);                                             \
  const void* e_ = (expected);                                           \
  size_t n_ = (size_t)(len);                                             \
  if (memcmp(a_, e_, n_) == 0) { gMsTestPassed++; }                     \
  else {                                                                  \
    fprintf(stderr, "FAIL [%s] %s: memory mismatch (%zu bytes)\n",      \
        gMsTestCurrent, (msg), n_);                                      \
    gMsTestFailed++;                                                     \
  }                                                                       \
} while (0)

// Unconditionally fail with a message.
#define MS_FAIL(msg) do {                                                  \
  fprintf(stderr, "FAIL [%s] %s\n", gMsTestCurrent, (msg));             \
  gMsTestFailed++;                                                        \
} while (0)

// Print summary and return exit code (0=all passed, 1=any failed).
static inline int msTestSummary(void) {
  fprintf(stderr, "\n%d passed, %d failed\n",
      gMsTestPassed, gMsTestFailed);
  return gMsTestFailed > 0 ? 1 : 0;
}
