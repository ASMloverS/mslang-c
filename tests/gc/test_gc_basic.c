#include "ms_test.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_object.h"
#include "mslang/ms_value.h"
#include "mslang/ms_vm.h"

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

// regression: markRoots() must scan the VM value stack (T067 exposed this --
// t->globals/t->stack were never enumerated as roots, so a collection firing
// while an object was live only on the stack would free it out from under
// the interpreter).
static void testStackValuesSurviveGC(void) {
  msGCInit();
  gFreeCount = 0;
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;
  t->globals = MS_NIL_VAL;

  struct MsObject* o = msGCAlloc(&gStubType, sizeof(*o));
  *t->sp++ = MS_OBJ_VAL(o);  // live on the value stack, no msGCPushRoot

  msGCCollect();
  MS_ASSERT_EQ(gFreeCount, 0, "stack-resident object survives GC");
  MS_ASSERT_EQ(gGC.numObjects, 1, "still 1 object");

  t->sp = t->stack;  // pop it off
  msGCCollect();
  MS_ASSERT_EQ(gFreeCount, 1, "freed once popped off the stack");

  msGCShutdown();
}

// regression: markRoots() must mark t->globals (the global namespace map
// added by T067, holding all builtin functions and user globals).
static void testGlobalsValueSurvivesGC(void) {
  msGCInit();
  gFreeCount = 0;
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;

  struct MsObject* o = msGCAlloc(&gStubType, sizeof(*o));
  t->globals = MS_OBJ_VAL(o);  // live only via t->globals, no msGCPushRoot

  msGCCollect();
  MS_ASSERT_EQ(gFreeCount, 0, "globals-resident object survives GC");
  MS_ASSERT_EQ(gGC.numObjects, 1, "still 1 object");

  t->globals = MS_NIL_VAL;  // leave a clean slate for the next test
  msGCCollect();
  MS_ASSERT_EQ(gFreeCount, 1, "freed once removed from globals");

  msGCShutdown();
}

int main(void) {
  MS_RUN(testAllocFree);
  MS_RUN(testRootKeepsAlive);
  MS_RUN(testStackValuesSurviveGC);
  MS_RUN(testGlobalsValueSurvivesGC);
  return msTestSummary();
}
