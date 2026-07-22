// test_call.c
// T068: OP_CALL/OP_MAKE_FUNC call convention -- MsFuncProto/MsClosure
// (ms_func.h), arity binding, default arguments, arity-mismatch errors.
//
// Chunks are built by hand (same workaround as test_e2e.c/test_locals.c):
// msCompile()+msVMRun() cannot reach a top-level expression's value in a C
// unit test, since the compiler always appends OP_RETURN_NIL to the
// top-level chunk. Recursion/default-argument .ms-level acceptance is
// covered by tests/ms/m2/ golden tests instead.
#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_func.h"
#include "mslang/ms_vm.h"

// Builds a 1-instruction-per-line closure add(a, b) { return a + b } with
// arity == arityMax == 2, no defaults, no upvalues.
static MsValue makeAddClosure(struct MsChunk* innerChunk) {
  msChunkInit(innerChunk, NULL);
  msChunkEmitOpA(innerChunk, OP_GET_LOCAL, 0, 1);
  msChunkEmitOpA(innerChunk, OP_GET_LOCAL, 1, 1);
  msChunkEmitOp(innerChunk, OP_ADD, 1);
  msChunkEmitOp(innerChunk, OP_RETURN, 1);
  MsValue proto = msNewFuncProto(innerChunk, "add", 2, 2, 0, 2);
  return msNewClosure((MsFuncProto*) MS_AS_OBJ(proto), 0);
}

// OP_CONST closure; OP_CONST 3; OP_CONST 4; OP_CALL 2; OP_RETURN -> 7.
static void testBasicCall(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeAddClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(3)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(4)), 1);
  msChunkEmitOpA(&outer, OP_CALL, 2, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_INT(result), "add(3, 4) is int");
  MS_ASSERT_EQ(MS_AS_INT(result), 7, "add(3, 4) == 7");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// add(3) -- one arg short of arity 2 -> TypeError (MS_ERROR_VALUE placeholder).
static void testMissingArgIsError(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeAddClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(3)), 1);
  msChunkEmitOpA(&outer, OP_CALL, 1, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "missing required argument is an error");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// add(3, 4, 5) -- one arg over arityMax 2, no vararg -> TypeError.
static void testTooManyArgsIsError(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeAddClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(3)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(4)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(5)), 1);
  msChunkEmitOpA(&outer, OP_CALL, 3, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "too many arguments is an error");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(a, b=99) { return a + b }; f(1) -> 100. proto->defaults[0] is set
// directly (bypassing OP_MAKE_FUNC, which is exercised by ms/m2 golden tests
// instead) to isolate msClosureCall's default-fill logic.
static void testDefaultArgFilled(void) {
  msVMInit();
  struct MsChunk innerChunk;
  msChunkInit(&innerChunk, NULL);
  msChunkEmitOpA(&innerChunk, OP_GET_LOCAL, 0, 1);
  msChunkEmitOpA(&innerChunk, OP_GET_LOCAL, 1, 1);
  msChunkEmitOp(&innerChunk, OP_ADD, 1);
  msChunkEmitOp(&innerChunk, OP_RETURN, 1);
  MsValue protoVal = msNewFuncProto(&innerChunk, "f", 1, 2, 1, 2);
  MsFuncProto* proto = (MsFuncProto*) MS_AS_OBJ(protoVal);
  proto->defaults[0] = MS_INT_VAL(99);
  MsValue closure = msNewClosure(proto, 0);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(1)), 1);
  msChunkEmitOpA(&outer, OP_CALL, 1, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_INT(result), "f(1) is int");
  MS_ASSERT_EQ(MS_AS_INT(result), 100, "f(1) == 1 + default(99)");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// Overriding the default: f(1, 2) -> 3, not 1 + 99.
static void testDefaultArgOverridden(void) {
  msVMInit();
  struct MsChunk innerChunk;
  msChunkInit(&innerChunk, NULL);
  msChunkEmitOpA(&innerChunk, OP_GET_LOCAL, 0, 1);
  msChunkEmitOpA(&innerChunk, OP_GET_LOCAL, 1, 1);
  msChunkEmitOp(&innerChunk, OP_ADD, 1);
  msChunkEmitOp(&innerChunk, OP_RETURN, 1);
  MsValue protoVal = msNewFuncProto(&innerChunk, "f", 1, 2, 1, 2);
  MsFuncProto* proto = (MsFuncProto*) MS_AS_OBJ(protoVal);
  proto->defaults[0] = MS_INT_VAL(99);
  MsValue closure = msNewClosure(proto, 0);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(2)), 1);
  msChunkEmitOpA(&outer, OP_CALL, 2, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_INT(result), "f(1, 2) is int");
  MS_ASSERT_EQ(MS_AS_INT(result), 3, "f(1, 2) == 3 (default overridden)");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

int main(void) {
  MS_RUN(testBasicCall);
  MS_RUN(testMissingArgIsError);
  MS_RUN(testTooManyArgsIsError);
  MS_RUN(testDefaultArgFilled);
  MS_RUN(testDefaultArgOverridden);
  return msTestSummary();
}
