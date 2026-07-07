#include "ms_test.h"
#include "mslang/ms_value.h"

static void testTagging(void) {
  MsValue iv = MS_INT_VAL(42);
  MS_ASSERT_TRUE(MS_IS_INT(iv), "is int");
  MS_ASSERT_TRUE(MS_AS_INT(iv) == 42, "value 42");
  MS_ASSERT_TRUE(sizeof(MsValue) <= 16, "size ok");
}

static void testTruthy(void) {
  MS_ASSERT_TRUE(!msValueTruthy(MS_NIL_VAL), "nil false");
  MS_ASSERT_TRUE(!msValueTruthy(MS_INT_VAL(0)), "0 false");
  MS_ASSERT_TRUE(msValueTruthy(MS_INT_VAL(1)), "1 true");
  MS_ASSERT_TRUE(!msValueTruthy(MS_BOOL_VAL(false)), "false→false");
  MS_ASSERT_TRUE(msValueTruthy(MS_BOOL_VAL(true)), "true→true");
}

static void testEqual(void) {
  MS_ASSERT_TRUE(msValueEqual(MS_INT_VAL(3), MS_FLOAT_VAL(3.0)), "3==3.0");
  MS_ASSERT_TRUE(!msValueEqual(MS_NIL_VAL, MS_BOOL_VAL(false)), "nil!=false");
  MS_ASSERT_TRUE(msValueEqual(MS_NIL_VAL, MS_NIL_VAL), "nil==nil");
}

int main(void) {
  MS_RUN(testTagging);
  MS_RUN(testTruthy);
  MS_RUN(testEqual);
  return msTestSummary();
}
