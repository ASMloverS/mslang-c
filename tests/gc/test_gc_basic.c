#include "ms_test.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

// destroy stub with a counter
static int gFreeCount = 0;
static void stubDestroy(struct MsObject* obj) {
  (void) obj;
  gFreeCount++;
}
static struct MsType gStubType = {.name = "stub", .objSize = sizeof(struct MsObject), .destroy = stubDestroy};

static void testAllocFree(void) {
  msGCInit();
  struct MsObject* o = msGCAlloc(&gStubType, sizeof(*o));
  MS_ASSERT_TRUE(o != NULL, "alloc ok");
  MS_ASSERT_EQ(gGC.numObjects, 1, "1 object");
  // no root reference -> collected by GC
  msGCCollect();
  MS_ASSERT_EQ(gFreeCount, 1, "free called");
  MS_ASSERT_EQ(gGC.numObjects, 0, "0 objects");
  msGCShutdown();
}

static void testRootKeepsAlive(void) {
  msGCInit();
  gFreeCount = 0;
  struct MsObject* o = msGCAlloc(&gStubType, sizeof(*o));
  msGCPushRoot(MS_OBJ_VAL(o));
  // rooted -> survives GC
  msGCCollect();
  MS_ASSERT_EQ(gFreeCount, 0, "not freed while rooted");
  MS_ASSERT_EQ(gGC.numObjects, 1, "still 1 object");
  msGCPopRoot();
  // unrooted -> collected by GC
  msGCCollect();
  MS_ASSERT_EQ(gFreeCount, 1, "freed after root popped");
  msGCShutdown();
}

int main(void) {
  MS_RUN(testAllocFree);
  MS_RUN(testRootKeepsAlive);
  return msTestSummary();
}
