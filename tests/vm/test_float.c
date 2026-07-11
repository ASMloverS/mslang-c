#include <math.h>
#include <stdint.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_float.h"
#include "mslang/ms_int.h"
#include "mslang/ms_object.h"
#include "mslang/ms_vm.h"

// msCompile() cannot be used here yet: OP_EQ/OP_LT have no case in eval()
// before T056 lands, and a top-level expression statement ends with
// OP_RETURN_NIL (also unhandled); see test_int_arith.c for the same
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

static MsValue runUnOp(MsValue a, MsOpCode op) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, a), 1);
  msChunkEmitOp(&chunk, op, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);

  msVMInit();
  MsValue result = msVMRun(&chunk);
  msVMShutdown();
  msChunkFree(&chunk);
  return result;
}

static void testFloatArith(void) {
  MsValue v = runBinOp(MS_FLOAT_VAL(1.5), MS_FLOAT_VAL(2.5), OP_ADD);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && MS_AS_FLOAT(v) == 4.0, "1.5+2.5=4.0");

  v = runBinOp(MS_FLOAT_VAL(10.0), MS_FLOAT_VAL(4.0), OP_SUB);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && MS_AS_FLOAT(v) == 6.0, "10.0-4.0=6.0");

  v = runBinOp(MS_FLOAT_VAL(3.0), MS_FLOAT_VAL(7.0), OP_MUL);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && MS_AS_FLOAT(v) == 21.0, "3.0*7.0=21.0");

  v = runBinOp(MS_FLOAT_VAL(10.0), MS_FLOAT_VAL(3.0), OP_DIV);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && fabs(MS_AS_FLOAT(v) - 3.3333333333333335) < 1e-12, "10.0/3.0");

  v = runBinOp(MS_FLOAT_VAL(2.0), MS_FLOAT_VAL(10.0), OP_POW);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && MS_AS_FLOAT(v) == 1024.0, "2.0**10.0=1024.0");
}

static void testIntFloatPromotion(void) {
  MsValue v = runBinOp(MS_INT_VAL(1), MS_FLOAT_VAL(2.0), OP_ADD);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v), "1+2.0 is float");
  MS_ASSERT_TRUE(MS_AS_FLOAT(v) == 3.0, "1+2.0=3.0");
}

static void testDivModByZero(void) {
  MsValue v = runBinOp(MS_FLOAT_VAL(10.0), MS_FLOAT_VAL(0.0), OP_DIV);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && isinf(MS_AS_FLOAT(v)), "10.0/0.0=inf, no error");

  v = runBinOp(MS_FLOAT_VAL(10.0), MS_FLOAT_VAL(0.0), OP_MOD);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && isnan(MS_AS_FLOAT(v)), "10.0%0.0=nan, no error");
}

static void testModPythonSign(void) {
  MsValue v = runBinOp(MS_FLOAT_VAL(-7.0), MS_FLOAT_VAL(3.0), OP_MOD);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && MS_AS_FLOAT(v) == 2.0, "-7.0%3.0=2.0 (sign of b)");

  v = runBinOp(MS_FLOAT_VAL(7.0), MS_FLOAT_VAL(-3.0), OP_MOD);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && MS_AS_FLOAT(v) == -2.0, "7.0%-3.0=-2.0 (sign of b)");
}

static void testNeg(void) {
  MsValue v = runUnOp(MS_FLOAT_VAL(3.5), OP_NEG);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && MS_AS_FLOAT(v) == -3.5, "neg 3.5 = -3.5");
}

static void testEqSlotDirect(void) {
  MsValue eq = msFloatType.tpEq(NULL, MS_FLOAT_VAL(2.0), MS_INT_VAL(2));
  MS_ASSERT_TRUE(MS_IS_BOOL(eq) && MS_AS_BOOL(eq), "2.0 == 2 via tpEq");
}

static void testLtSlotDirect(void) {
  MsValue lt = msFloatType.tpLt(NULL, MS_FLOAT_VAL(1.0), MS_FLOAT_VAL(2.0));
  MS_ASSERT_TRUE(MS_IS_BOOL(lt) && MS_AS_BOOL(lt), "1.0 < 2.0 via tpLt");
}

static void testHashConsistency(void) {
  MsValue hf = msFloatType.tpHash(NULL, MS_FLOAT_VAL(3.0));
  MsValue hi = msIntType.tpHash(NULL, MS_INT_VAL(3));
  MS_ASSERT_TRUE(MS_IS_INT(hf) && MS_IS_INT(hi) && MS_AS_INT(hf) == MS_AS_INT(hi), "hash(3) == hash(3.0)");
}

int main(void) {
  MS_RUN(testFloatArith);
  MS_RUN(testIntFloatPromotion);
  MS_RUN(testDivModByZero);
  MS_RUN(testModPythonSign);
  MS_RUN(testNeg);
  MS_RUN(testEqSlotDirect);
  MS_RUN(testLtSlotDirect);
  MS_RUN(testHashConsistency);
  return msTestSummary();
}
