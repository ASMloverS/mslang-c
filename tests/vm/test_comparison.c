#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_vm.h"

// msCompile() cannot be used for these tests yet (no case for OP_RETURN_NIL
// in eval() before later tasks land); see test_int_arith.c for the same
// workaround. Chunks are built by hand instead.

static MsValue runBinOp(MsValue a, MsValue b, MsOpCode op) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, a), 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, b), 1);
  msChunkEmitOp(&chunk, op, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);

  msVMInit();
  MsValue result = msVMRun(&chunk);
  msVMShutdown();
  msChunkFree(&chunk);
  return result;
}

static void testEq(void) {
  MsValue v = runBinOp(MS_INT_VAL(1), MS_FLOAT_VAL(1.0), OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "1 == 1.0");

  v = runBinOp(MS_INT_VAL(1), MS_INT_VAL(2), OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "1 == 2 is false");

  v = runBinOp(MS_NIL_VAL, MS_BOOL_VAL(false), OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "nil == false is false");
}

static void testNe(void) {
  MsValue v = runBinOp(MS_INT_VAL(2), MS_INT_VAL(1), OP_NE);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "2 != 1");

  v = runBinOp(MS_INT_VAL(1), MS_INT_VAL(1), OP_NE);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "1 != 1 is false");
}

static void testOrdering(void) {
  MsValue v = runBinOp(MS_INT_VAL(1), MS_INT_VAL(2), OP_LT);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "1 < 2");

  v = runBinOp(MS_INT_VAL(2), MS_INT_VAL(1), OP_LT);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "2 < 1 is false");

  v = runBinOp(MS_INT_VAL(2), MS_INT_VAL(1), OP_GT);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "2 > 1");

  v = runBinOp(MS_INT_VAL(1), MS_INT_VAL(1), OP_LE);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "1 <= 1");

  v = runBinOp(MS_INT_VAL(2), MS_INT_VAL(3), OP_GE);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "2 >= 3 is false");
}

static void testIncomparableTypeError(void) {
  MsValue v = runBinOp(MS_NIL_VAL, MS_INT_VAL(1), OP_LT);
  MS_ASSERT_TRUE(MS_IS_ERROR(v), "nil < 1 is TypeError");
}

static void testIs(void) {
  MsValue v = runBinOp(MS_NIL_VAL, MS_NIL_VAL, OP_IS);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "nil is nil");

  v = runBinOp(MS_NIL_VAL, MS_INT_VAL(1), OP_IS_NOT);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "nil is not 1");

  v = runBinOp(MS_INT_VAL(1), MS_INT_VAL(1), OP_IS);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "1 is 1");
}

static void testInNoContainsSlot(void) {
  // int has no tpContains yet (list/map/set land in T059-T062): `in`/`not in`
  // on a non-container must be a TypeError, not a crash.
  MsValue v = runBinOp(MS_INT_VAL(1), MS_INT_VAL(2), OP_IN);
  MS_ASSERT_TRUE(MS_IS_ERROR(v), "1 in 2 is TypeError (int has no tpContains)");

  v = runBinOp(MS_INT_VAL(1), MS_INT_VAL(2), OP_NOT_IN);
  MS_ASSERT_TRUE(MS_IS_ERROR(v), "1 not in 2 is TypeError (int has no tpContains)");
}

int main(void) {
  MS_RUN(testEq);
  MS_RUN(testNe);
  MS_RUN(testOrdering);
  MS_RUN(testIncomparableTypeError);
  MS_RUN(testIs);
  MS_RUN(testInNoContainsSlot);
  return msTestSummary();
}
