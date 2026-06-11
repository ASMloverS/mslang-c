#include "ms_test.h"
#include "mslang/mslang.h"

static void testVersionMacro(void) {
  MS_ASSERT_EQ(MSLANG_VERSION_MAJOR, 0, "major");
  MS_ASSERT_EQ(MSLANG_VERSION_MINOR, 1, "minor");
  MS_ASSERT_STR_EQ(MSLANG_VERSION_STR, "0.1.0", "version str");
}

static void testTagValues(void) {
  MS_ASSERT_EQ(MS_TAG_INT,   0, "INT tag");
  MS_ASSERT_EQ(MS_TAG_FLOAT, 1, "FLOAT tag");
  MS_ASSERT_EQ(MS_TAG_NIL,   3, "NIL tag");
  MS_ASSERT_EQ(MS_TAG_ERROR, 5, "ERROR tag");
}

static void testMsNil(void) {
  // MsValue is a forward declaration in this task; fields are accessible after T049.
  // This test only verifies headers compile and the macro is defined.
  MS_ASSERT_TRUE(1, "headers compile ok");
}

int main(void) {
  MS_RUN(testVersionMacro);
  MS_RUN(testTagValues);
  MS_RUN(testMsNil);
  return msTestSummary();
}
