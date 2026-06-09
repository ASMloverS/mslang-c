// test_framework_self.c — pass path self-verification (passed=4, failed=0, exit 0)
#include "ms_test.h"

static void testPassingAsserts(void) {
  MS_ASSERT_EQ(1 + 1, 2, "1+1==2");
  MS_ASSERT_STR_EQ("hello", "hello", "str eq");
  MS_ASSERT_TRUE(1 > 0, "1>0");
  MS_ASSERT_FALSE(0, "false");
}

int main(void) {
  MS_RUN(testPassingAsserts);
  return msTestSummary(); // expect exit 0
}
