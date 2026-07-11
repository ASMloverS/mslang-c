#include <math.h>
#include <stdint.h>

#include "ms_test.h"
#include "mslang/ms_bool.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_float.h"
#include "mslang/ms_int.h"
#include "mslang/ms_object.h"
#include "mslang/ms_vm.h"

// msCompile() cannot be used for these tests yet (no case for OP_RETURN_NIL
// in eval() before later tasks land); see test_int_arith.c for the same
// workaround. Chunks are built by hand instead.

// Shared init/run/shutdown/free lifecycle for the hand-built chunks below.
static MsValue runChunk(struct MsChunk* chunk) {
  msVMInit();
  MsValue result = msVMRun(chunk);
  msVMShutdown();
  msChunkFree(chunk);
  return result;
}

static MsValue runUnOp(MsValue a, MsOpCode op) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, a), 1);
  msChunkEmitOp(&chunk, op, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  return runChunk(&chunk);
}

// cond; JUMP_IF_FALSE else; CONST_INT 1 (body); JUMP end; else: CONST_INT 0
// (skip); end: RETURN. Mirrors compileIf's jumpFalse/jumpEnd pattern
// (ms_compiler.c compileIf).
static MsValue runIfFalseSkips(MsValue cond) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, cond), 1);
  msChunkEmitOpAX(&chunk, OP_JUMP_IF_FALSE, 0, 1);
  uint32_t jumpFalse = chunk.codeLen - 3;
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, MS_INT_VAL(1)), 1);  // body entered
  msChunkEmitOpAX(&chunk, OP_JUMP, 0, 1);
  uint32_t jumpEnd = chunk.codeLen - 3;
  msChunkPatchJump(&chunk, jumpFalse, chunk.codeLen);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, MS_INT_VAL(0)), 1);  // body skipped
  msChunkPatchJump(&chunk, jumpEnd, chunk.codeLen);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  return runChunk(&chunk);
}

// cond; JUMP_IF_TRUE truthyLabel; CONST_INT 0 (falsy path); JUMP end;
// truthyLabel: CONST_INT 1 (truthy path); end: RETURN.
static MsValue runIfTrueTaken(MsValue cond) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, cond), 1);
  msChunkEmitOpAX(&chunk, OP_JUMP_IF_TRUE, 0, 1);
  uint32_t jumpTrue = chunk.codeLen - 3;
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, MS_INT_VAL(0)), 1);  // falsy path
  msChunkEmitOpAX(&chunk, OP_JUMP, 0, 1);
  uint32_t jumpEnd = chunk.codeLen - 3;
  msChunkPatchJump(&chunk, jumpTrue, chunk.codeLen);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, MS_INT_VAL(1)), 1);  // truthy path
  msChunkPatchJump(&chunk, jumpEnd, chunk.codeLen);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  return runChunk(&chunk);
}

// left; AND_JMP/OR_JMP end (does not pop); right; end: RETURN.
// Mirrors compileBinary's OP_AND_JMP/OP_OR_JMP emission (ms_compiler.c).
static MsValue runShortCircuit(MsValue left, MsValue right, MsOpCode jmpOp) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, left), 1);
  msChunkEmitOpAX(&chunk, jmpOp, 0, 1);
  uint32_t j = chunk.codeLen - 3;
  msChunkEmitOp(&chunk, OP_POP, 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, right), 1);
  msChunkPatchJump(&chunk, j, chunk.codeLen);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  return runChunk(&chunk);
}

static void testNot(void) {
  MsValue v = runUnOp(MS_BOOL_VAL(true), OP_NOT);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "not true = false");

  v = runUnOp(MS_BOOL_VAL(false), OP_NOT);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "not false = true");

  v = runUnOp(MS_NIL_VAL, OP_NOT);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "not nil = true");

  v = runUnOp(MS_INT_VAL(0), OP_NOT);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "not 0 = true");

  v = runUnOp(MS_INT_VAL(1), OP_NOT);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "not 1 = false");

  v = runUnOp(MS_FLOAT_VAL(NAN), OP_NOT);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "not nan = false (nan is truthy)");
}

static void testJumpIfFalseSkipsBody(void) {
  MsValue v = runIfFalseSkips(MS_INT_VAL(0));
  MS_ASSERT_TRUE(MS_AS_INT(v) == 0, "if 0 {...} does not enter body");

  v = runIfFalseSkips(MS_NIL_VAL);
  MS_ASSERT_TRUE(MS_AS_INT(v) == 0, "if nil {...} does not enter body");

  v = runIfFalseSkips(MS_INT_VAL(1));
  MS_ASSERT_TRUE(MS_AS_INT(v) == 1, "if 1 {...} enters body (sanity)");
}

static void testJumpIfTrueTakesBranch(void) {
  MsValue v = runIfTrueTaken(MS_BOOL_VAL(true));
  MS_ASSERT_TRUE(MS_AS_INT(v) == 1, "true takes the JUMP_IF_TRUE branch");

  v = runIfTrueTaken(MS_BOOL_VAL(false));
  MS_ASSERT_TRUE(MS_AS_INT(v) == 0, "false falls through JUMP_IF_TRUE");
}

static void testBoolIntEqSymmetric(void) {
  MsValue v = msBoolType.tpEq(NULL, MS_BOOL_VAL(true), MS_INT_VAL(1));
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "true == 1");

  v = msIntType.tpEq(NULL, MS_INT_VAL(1), MS_BOOL_VAL(true));
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "1 == true");

  v = msIntType.tpEq(NULL, MS_INT_VAL(0), MS_BOOL_VAL(true));
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "0 != true");

  // Both directions must agree: centralized in msValueEqual (ms_value.c),
  // not hand-patched per-type, so float's side wasn't missed.
  v = msBoolType.tpEq(NULL, MS_BOOL_VAL(true), MS_FLOAT_VAL(1.0));
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "true == 1.0");

  v = msFloatType.tpEq(NULL, MS_FLOAT_VAL(1.0), MS_BOOL_VAL(true));
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "1.0 == true");
}

static void testNilEq(void) {
  MsValue v = msNilType.tpEq(NULL, MS_NIL_VAL, MS_NIL_VAL);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "nil == nil");

  v = msNilType.tpEq(NULL, MS_NIL_VAL, MS_BOOL_VAL(false));
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "nil != false");
}

static void testShortCircuitAnd(void) {
  // false and (right never evaluated) -> false, no crash.
  MsValue v = runShortCircuit(MS_BOOL_VAL(false), MS_INT_VAL(999), OP_AND_JMP);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "false and x = false, short-circuits");
}

static void testShortCircuitOr(void) {
  // true or (right never evaluated) -> true, no crash.
  MsValue v = runShortCircuit(MS_BOOL_VAL(true), MS_INT_VAL(999), OP_OR_JMP);
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "true or x = true, short-circuits");
}

static void testAndOrReturnOperandNotBool(void) {
  // 1 and 2 -> 2 (right operand, not coerced to bool).
  MsValue v = runShortCircuit(MS_INT_VAL(1), MS_INT_VAL(2), OP_AND_JMP);
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 2, "1 and 2 = 2");

  // nil or 42 -> 42 (right operand).
  v = runShortCircuit(MS_NIL_VAL, MS_INT_VAL(42), OP_OR_JMP);
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 42, "nil or 42 = 42");
}

int main(void) {
  MS_RUN(testNot);
  MS_RUN(testJumpIfFalseSkipsBody);
  MS_RUN(testJumpIfTrueTakesBranch);
  MS_RUN(testBoolIntEqSymmetric);
  MS_RUN(testNilEq);
  MS_RUN(testShortCircuitAnd);
  MS_RUN(testShortCircuitOr);
  MS_RUN(testAndOrReturnOperandNotBool);
  return msTestSummary();
}
