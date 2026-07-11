#include <stdint.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_vm.h"

// msCompile() cannot be used for these tests yet: a compiled top-level
// expression statement always emits OP_POP and the program ends with
// OP_RETURN_NIL (no case in eval() before later tasks land); see
// test_locals.c for the same workaround. Chunks are built by hand instead.

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

static void testBasicArith(void) {
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(1), MS_INT_VAL(2), OP_ADD)) == 3, "+ ok");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(10), MS_INT_VAL(4), OP_SUB)) == 6, "- ok");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(3), MS_INT_VAL(7), OP_MUL)) == 21, "* ok");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(10), MS_INT_VAL(3), OP_DIV)) == 3, "/ trunc ok");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(-7), MS_INT_VAL(2), OP_DIV)) == -3, "/ trunc toward zero");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(10), MS_INT_VAL(3), OP_MOD)) == 1, "% ok");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(2), MS_INT_VAL(10), OP_POW)) == 1024, "** ok");
}

static void testPowNegExponent(void) {
  MsValue v = runBinOp(MS_INT_VAL(2), MS_INT_VAL(-1), OP_POW);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v), "2**-1 is float");
  MS_ASSERT_TRUE(MS_AS_FLOAT(v) == 0.5, "2**-1 == 0.5");
}

static void testDivModByZero(void) {
  MS_ASSERT_TRUE(MS_IS_ERROR(runBinOp(MS_INT_VAL(5), MS_INT_VAL(0), OP_DIV)), "5/0 error");
  MS_ASSERT_TRUE(MS_IS_ERROR(runBinOp(MS_INT_VAL(5), MS_INT_VAL(0), OP_MOD)), "5%0 error");
}

static void testDivModIntMinByNegOne(void) {
  MS_ASSERT_TRUE(
      MS_AS_INT(runBinOp(MS_INT_VAL(INT64_MIN), MS_INT_VAL(-1), OP_DIV)) == INT64_MIN,
      "INT64_MIN / -1 wraps to INT64_MIN");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(INT64_MIN), MS_INT_VAL(-1), OP_MOD)) == 0, "INT64_MIN % -1 == 0");
}

static void testUnaryOps(void) {
  MS_ASSERT_TRUE(MS_AS_INT(runUnOp(MS_INT_VAL(42), OP_NEG)) == -42, "neg ok");
  MS_ASSERT_TRUE(MS_AS_INT(runUnOp(MS_INT_VAL(5), OP_BNOT)) == -6, "~ ok");
}

static void testBitOps(void) {
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(5), MS_INT_VAL(3), OP_BAND)) == 1, "& ok");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(5), MS_INT_VAL(3), OP_BOR)) == 7, "| ok");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(5), MS_INT_VAL(3), OP_BXOR)) == 6, "^ ok");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(3), MS_INT_VAL(2), OP_SHL)) == 12, "<< ok");
  MS_ASSERT_TRUE(MS_AS_INT(runBinOp(MS_INT_VAL(16), MS_INT_VAL(2), OP_SHR)) == 4, ">> ok");
}

static void testIntFloatPromotion(void) {
  MsValue v = runBinOp(MS_INT_VAL(1), MS_FLOAT_VAL(2.0), OP_ADD);
  MS_ASSERT_TRUE(MS_IS_FLOAT(v), "int+float is float");
  MS_ASSERT_TRUE(MS_AS_FLOAT(v) == 3.0, "int+float == 3.0");
}

int main(void) {
  MS_RUN(testBasicArith);
  MS_RUN(testPowNegExponent);
  MS_RUN(testDivModByZero);
  MS_RUN(testDivModIntMinByNegOne);
  MS_RUN(testUnaryOps);
  MS_RUN(testBitOps);
  MS_RUN(testIntFloatPromotion);
  return msTestSummary();
}
