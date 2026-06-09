// test_framework_fail.c - fail path self-verification (failed=1, exit 1)
// CTest registers with WILL_FAIL TRUE: exit code 1 == test passes
#include "ms_test.h"

static void testFailingAssert(void) {
  MS_ASSERT_EQ(1, 2, "intentional failure");
}

int main(void) {
  MS_RUN(testFailingAssert);
  return msTestSummary(); // expect exit 1
}
