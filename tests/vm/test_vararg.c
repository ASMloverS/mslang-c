// test_vararg.c
// T069: `...args` collection (msClosureCall's hasVararg branch, ms_func.c)
// and OP_CALL_EX expand-call (ms_vm.c). Chunks are built by hand, same
// workaround as test_call.c (msCompile()+msVMRun() cannot reach a top-level
// expression's value in a C unit test).
#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_func.h"
#include "mslang/ms_list.h"
#include "mslang/ms_vm.h"

// f(first, ...args) { return args } -- arity=1, arityMax=1 (excludes the
// vararg slot itself, ms_func.h), localCount=2 (first, args).
static MsValue makeVarargClosure(struct MsChunk* innerChunk) {
  msChunkInit(innerChunk, NULL);
  msChunkEmitOpA(innerChunk, OP_GET_LOCAL, 1, 1);
  msChunkEmitOp(innerChunk, OP_RETURN, 1);
  MsValue protoVal = msNewFuncProto(innerChunk, "f", 1, 1, 0, 2);
  ((MsFuncProto*) MS_AS_OBJ(protoVal))->hasVararg = true;
  return msNewClosure((MsFuncProto*) MS_AS_OBJ(protoVal), 0);
}

// f(1, 2, 3) -> args == [2, 3].
static void testVarargCollectsExtra(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeVarargClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(2)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(3)), 1);
  msChunkEmitOpA(&outer, OP_CALL, 3, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_OBJ(result), "args is a list object");
  struct MsListObj* args = (struct MsListObj*) MS_AS_OBJ(result);
  MS_ASSERT_EQ(args->len, 2, "args has 2 elements");
  MS_ASSERT_EQ(MS_AS_INT(args->items[0]), 2, "args[0] == 2");
  MS_ASSERT_EQ(MS_AS_INT(args->items[1]), 3, "args[1] == 3");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(1) -- no extra positional args -> args == [] (empty list, not nil).
static void testVarargEmptyWhenNoExtra(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeVarargClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(1)), 1);
  msChunkEmitOpA(&outer, OP_CALL, 1, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_OBJ(result), "args is a list object");
  MS_ASSERT_EQ(((struct MsListObj*) MS_AS_OBJ(result))->len, 0, "args is empty");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(a, b=1, ...args) -- vararg collection must run *after* default filling
// (impl/P5-T069-vararg.md "风险与边界"): f(10) -> b stays defaulted to 1,
// args == [] (missing arg fill and vararg collection must not clobber sp).
static void testVarargAfterDefaultFill(void) {
  msVMInit();
  struct MsChunk innerChunk;
  msChunkInit(&innerChunk, NULL);
  msChunkEmitOpA(&innerChunk, OP_GET_LOCAL, 1, 1);  // b
  msChunkEmitOp(&innerChunk, OP_RETURN, 1);
  MsValue protoVal = msNewFuncProto(&innerChunk, "f", 1, 2, 1, 3);
  MsFuncProto* proto = (MsFuncProto*) MS_AS_OBJ(protoVal);
  proto->defaults[0] = MS_INT_VAL(1);
  proto->hasVararg = true;
  MsValue closure = msNewClosure(proto, 0);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(10)), 1);
  msChunkEmitOpA(&outer, OP_CALL, 1, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_INT(result), "b is int");
  MS_ASSERT_EQ(MS_AS_INT(result), 1, "b == default(1), unclobbered by vararg collection");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// Same proto shape as above, but the body returns `args` instead of `b`:
// f(10, 20, 30, 40) -> b == 20 (override), args == [30, 40].
static void testVarargWithOverriddenDefault(void) {
  msVMInit();
  struct MsChunk innerChunk;
  msChunkInit(&innerChunk, NULL);
  msChunkEmitOpA(&innerChunk, OP_GET_LOCAL, 2, 1);  // args
  msChunkEmitOp(&innerChunk, OP_RETURN, 1);
  MsValue protoVal = msNewFuncProto(&innerChunk, "f", 1, 2, 1, 3);
  MsFuncProto* proto = (MsFuncProto*) MS_AS_OBJ(protoVal);
  proto->defaults[0] = MS_INT_VAL(1);
  proto->hasVararg = true;
  MsValue closure = msNewClosure(proto, 0);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(10)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(20)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(30)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(40)), 1);
  msChunkEmitOpA(&outer, OP_CALL, 4, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_OBJ(result), "args is a list object");
  struct MsListObj* args = (struct MsListObj*) MS_AS_OBJ(result);
  MS_ASSERT_EQ(args->len, 2, "args has 2 elements");
  MS_ASSERT_EQ(MS_AS_INT(args->items[0]), 30, "args[0] == 30");
  MS_ASSERT_EQ(MS_AS_INT(args->items[1]), 40, "args[1] == 40");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// OP_CALL_EX: add(a, b) { return a + b } called as add(*[3, 4]) -> 7.
static void testCallExExpandsList(void) {
  msVMInit();
  struct MsChunk innerChunk;
  msChunkInit(&innerChunk, NULL);
  msChunkEmitOpA(&innerChunk, OP_GET_LOCAL, 0, 1);
  msChunkEmitOpA(&innerChunk, OP_GET_LOCAL, 1, 1);
  msChunkEmitOp(&innerChunk, OP_ADD, 1);
  msChunkEmitOp(&innerChunk, OP_RETURN, 1);
  MsValue proto = msNewFuncProto(&innerChunk, "add", 2, 2, 0, 2);
  MsValue closure = msNewClosure((MsFuncProto*) MS_AS_OBJ(proto), 0);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(3)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(4)), 1);
  msChunkEmitOpA(&outer, OP_BUILD_LIST, 2, 1);
  msChunkEmitOp(&outer, OP_CALL_EX, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_INT(result), "add(*[3, 4]) is int");
  MS_ASSERT_EQ(MS_AS_INT(result), 7, "add(*[3, 4]) == 7");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

int main(void) {
  MS_RUN(testVarargCollectsExtra);
  MS_RUN(testVarargEmptyWhenNoExtra);
  MS_RUN(testVarargAfterDefaultFill);
  MS_RUN(testVarargWithOverriddenDefault);
  MS_RUN(testCallExExpandsList);
  return msTestSummary();
}
