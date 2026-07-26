// test_kwargs.c
// T070: OP_CALL_KW dispatch (ms_vm.c) + msClosureCallKw parameter binding
// (ms_func.c) -- keyword argument binding by name, out-of-order kwargs,
// kwargs hitting a default slot, **kwargs collection, and the three
// TypeError placeholders (duplicate/unknown/missing). Chunks are built by
// hand, same workaround as test_call.c/test_vararg.c (msCompile()+msVMRun()
// cannot reach a top-level expression's value in a C unit test).
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_alloc.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_func.h"
#include "mslang/ms_map.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

// Builds a closure over innerChunk with `nameCount` positional parameter
// names set up for msFindParamSlot (owned copies -- mirrors ms_compiler.c's
// compileFuncToConst change; funcProtoDestroy frees these on GC sweep).
static MsValue makeKwClosure(
    struct MsChunk* innerChunk,
    const char** names,
    const uint32_t* lens,
    uint32_t nameCount,
    uint32_t arity,
    uint32_t arityMax,
    uint32_t defaultCount,
    uint32_t localCount,
    bool hasKwarg,
    uint32_t kwargsSlot) {
  MsValue protoVal = msNewFuncProto(innerChunk, "f", arity, arityMax, defaultCount, localCount);
  MsFuncProto* proto = (MsFuncProto*) MS_AS_OBJ(protoVal);
  proto->hasKwarg = hasKwarg;
  proto->kwargsSlot = kwargsSlot;
  if (nameCount > 0) {
    proto->paramNames = MS_ALLOC_N(const char*, nameCount);
    proto->paramNameLens = MS_ALLOC_N(uint32_t, nameCount);
    for (uint32_t i = 0; i < nameCount; i++) {
      char* buf = MS_ALLOC_N(char, lens[i]);
      memcpy(buf, names[i], lens[i]);
      proto->paramNames[i] = buf;
      proto->paramNameLens[i] = lens[i];
    }
  }
  return msNewClosure(proto, 0);
}

// f(a, b) { return a*100 + b }, arity=2, arityMax=2, no defaults.
static MsValue makeAB100Closure(struct MsChunk* innerChunk) {
  msChunkInit(innerChunk, NULL);
  msChunkEmitOpA(innerChunk, OP_GET_LOCAL, 0, 1);  // a
  uint32_t hundredIdx = msChunkAddConst(innerChunk, MS_INT_VAL(100));
  msChunkEmitOpAX(innerChunk, OP_CONST, hundredIdx, 1);
  msChunkEmitOp(innerChunk, OP_MUL, 1);            // a*100
  msChunkEmitOpA(innerChunk, OP_GET_LOCAL, 1, 1);  // b
  msChunkEmitOp(innerChunk, OP_ADD, 1);            // a*100+b
  msChunkEmitOp(innerChunk, OP_RETURN, 1);
  const char* names[2] = {"a", "b"};
  const uint32_t kLens[2] = {1, 1};
  return makeKwClosure(innerChunk, names, kLens, 2, 2, 2, 0, 2, false, 0);
}

// f(a, b=2)  { return a*100 + b }, arity=1, arityMax=2, defaultCount=1.
static MsValue makeADefaultBClosure(struct MsChunk* innerChunk) {
  msChunkInit(innerChunk, NULL);
  msChunkEmitOpA(innerChunk, OP_GET_LOCAL, 0, 1);  // a
  uint32_t hundredIdx = msChunkAddConst(innerChunk, MS_INT_VAL(100));
  msChunkEmitOpAX(innerChunk, OP_CONST, hundredIdx, 1);
  msChunkEmitOp(innerChunk, OP_MUL, 1);            // a*100
  msChunkEmitOpA(innerChunk, OP_GET_LOCAL, 1, 1);  // b
  msChunkEmitOp(innerChunk, OP_ADD, 1);            // a*100+b
  msChunkEmitOp(innerChunk, OP_RETURN, 1);
  const char* names[2] = {"a", "b"};
  const uint32_t kLens[2] = {1, 1};
  MsValue closure = makeKwClosure(innerChunk, names, kLens, 2, 1, 2, 1, 2, false, 0);
  MsFuncProto* proto = ((MsClosure*) MS_AS_OBJ(closure))->proto;
  proto->defaults[0] = MS_INT_VAL(10);
  return closure;
}

// f(a, **kwargs) { return kwargs }, arity=1, arityMax=1, hasKwarg,
// kwargsSlot=1, localCount=2.
static MsValue makeKwargsCollectClosure(struct MsChunk* innerChunk) {
  msChunkInit(innerChunk, NULL);
  msChunkEmitOpA(innerChunk, OP_GET_LOCAL, 1, 1);  // kwargs
  msChunkEmitOp(innerChunk, OP_RETURN, 1);
  const char* names[1] = {"a"};
  const uint32_t kLens[1] = {1};
  return makeKwClosure(innerChunk, names, kLens, 1, 1, 1, 0, 2, true, 1);
}

// f(a, b) { return a } -- for the three TypeError placeholder cases; body
// never runs when binding fails, so its exact content does not matter.
static MsValue makeABClosure(struct MsChunk* innerChunk) {
  msChunkInit(innerChunk, NULL);
  msChunkEmitOpA(innerChunk, OP_GET_LOCAL, 0, 1);
  msChunkEmitOp(innerChunk, OP_RETURN, 1);
  const char* names[2] = {"a", "b"};
  const uint32_t kLens[2] = {1, 1};
  return makeKwClosure(innerChunk, names, kLens, 2, 2, 2, 0, 2, false, 0);
}

// f(b=2, a=1) -- out-of-order keyword args, posArgc=0 -> a*100+b == 102.
static void testKwargsOutOfOrder(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeAB100Closure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, msNewStr("b", 1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(2)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, msNewStr("a", 1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(1)), 1);
  msChunkEmitOpA(&outer, OP_BUILD_MAP, 2, 1);
  msChunkEmitOpA(&outer, OP_CALL_KW, 0, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_INT(result), "f(b=2, a=1) is int");
  MS_ASSERT_EQ(MS_AS_INT(result), 102, "f(b=2, a=1) == a*100+b == 102");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(a=5) -- kwarg hits the default slot, posArgc=0 -> a*100+b == 500+10.
static void testKwargsHitsDefaultSlot(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeADefaultBClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, msNewStr("a", 1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(5)), 1);
  msChunkEmitOpA(&outer, OP_BUILD_MAP, 1, 1);
  msChunkEmitOpA(&outer, OP_CALL_KW, 0, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_INT(result), "f(a=5) is int");
  MS_ASSERT_EQ(MS_AS_INT(result), 510, "f(a=5) == a*100+default(10) == 510");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(1, x=2, y=3) -- posArgc=1, kwargs={x:2,y:3} collected into **kwargs.
static void testKwargsCollectExtra(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeKwargsCollectClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, msNewStr("x", 1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(2)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, msNewStr("y", 1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(3)), 1);
  msChunkEmitOpA(&outer, OP_BUILD_MAP, 2, 1);
  msChunkEmitOpA(&outer, OP_CALL_KW, 1, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_OBJ(result), "kwargs is a map object");
  MsValue xv = msMapGet(&gVM, result, msNewStr("x", 1));
  MsValue yv = msMapGet(&gVM, result, msNewStr("y", 1));
  MS_ASSERT_TRUE(MS_IS_INT(xv), "kwargs['x'] is int");
  MS_ASSERT_EQ(MS_AS_INT(xv), 2, "kwargs['x'] == 2");
  MS_ASSERT_TRUE(MS_IS_INT(yv), "kwargs['y'] is int");
  MS_ASSERT_EQ(MS_AS_INT(yv), 3, "kwargs['y'] == 3");
  MS_ASSERT_FALSE(MS_AS_BOOL(msMapHas(&gVM, result, msNewStr("a", 1))), "kwargs does not include bound param 'a'");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(1, a=2) -- positional already filled 'a', keyword re-supplies it -> error.
static void testKwargsDuplicateIsError(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeABClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, msNewStr("a", 1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(2)), 1);
  msChunkEmitOpA(&outer, OP_BUILD_MAP, 1, 1);
  msChunkEmitOpA(&outer, OP_CALL_KW, 1, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "f(1, a=2) is an error (duplicate keyword)");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(1, 2, c=3) -- unknown keyword 'c', no **kwargs collector -> error.
static void testKwargsUnknownIsError(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeABClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(2)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, msNewStr("c", 1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(3)), 1);
  msChunkEmitOpA(&outer, OP_BUILD_MAP, 1, 1);
  msChunkEmitOpA(&outer, OP_CALL_KW, 2, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "f(1, 2, c=3) is an error (unknown keyword)");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(b=2) -- required 'a' provided neither positionally nor by keyword -> error.
static void testKwargsMissingIsError(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeABClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, msNewStr("b", 1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(2)), 1);
  msChunkEmitOpA(&outer, OP_BUILD_MAP, 1, 1);
  msChunkEmitOpA(&outer, OP_CALL_KW, 0, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "f(b=2) is an error (missing required argument a)");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(**5) -- kwargsMap operand is a non-map value (int); OP_CALL_KW must
// reject it as a TypeError instead of msClosureCallKw casting it straight
// to MsMapObj* (type confusion -> garbage pointer deref/segfault).
static void testKwargsNonMapValueIsError(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeABClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(5)), 1);  // bogus "kwargsMap"
  msChunkEmitOpA(&outer, OP_CALL_KW, 0, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "f(**5) is an error (kwargsMap must be a map)");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(a, ...rest) { return a }; f(a=1) -- a hasVararg proto called through
// OP_CALL_KW must be rejected rather than leaving the vararg slot
// uninitialized (yet GC-traced) or silently discarding kwargs (out of
// scope per impl/P5-T070-kwargs.md "风险与边界": vararg+kwarg mixing).
static MsValue makeVarargAClosure(struct MsChunk* innerChunk) {
  msChunkInit(innerChunk, NULL);
  msChunkEmitOpA(innerChunk, OP_GET_LOCAL, 0, 1);  // a
  msChunkEmitOp(innerChunk, OP_RETURN, 1);
  const char* names[1] = {"a"};
  const uint32_t kLens[1] = {1};
  MsValue closure = makeKwClosure(innerChunk, names, kLens, 1, 1, 1, 0, 2, false, 0);
  ((MsClosure*) MS_AS_OBJ(closure))->proto->hasVararg = true;
  return closure;
}

static void testKwargsVarargProtoIsError(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeVarargAClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, msNewStr("a", 1)), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(1)), 1);
  msChunkEmitOpA(&outer, OP_BUILD_MAP, 1, 1);
  msChunkEmitOpA(&outer, OP_CALL_KW, 0, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "f(a=1) on a hasVararg proto is an error (vararg+kwarg mixing out of scope)");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// f(a, **kwargs) { return kwargs }; f(1) via plain OP_CALL (no keyword args
// at the call site at all) -- msClosureCall (not msClosureCallKw) must still
// initialize the **kwargs collector slot to an empty map, not MS_NIL_VAL,
// since OP_CALL/OP_CALL_EX never populate it via msClosureCallKw's kwargsMap
// binding path.
static void testKwargsCollectEmptyViaPlainCall(void) {
  msVMInit();
  struct MsChunk innerChunk;
  MsValue closure = makeKwargsCollectClosure(&innerChunk);

  struct MsChunk outer;
  msChunkInit(&outer, NULL);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, closure), 1);
  msChunkEmitOpAX(&outer, OP_CONST, msChunkAddConst(&outer, MS_INT_VAL(1)), 1);
  msChunkEmitOpA(&outer, OP_CALL, 1, 1);
  msChunkEmitOp(&outer, OP_RETURN, 1);

  MsValue result = msVMRun(&outer);
  MS_ASSERT_TRUE(MS_IS_OBJ(result), "f(1) kwargs slot is an object (map), not nil");
  MS_ASSERT_EQ(((struct MsMapObj*) MS_AS_OBJ(result))->len, 0u, "f(1) kwargs is empty when no keyword args are passed");

  msChunkFree(&outer);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

int main(void) {
  MS_RUN(testKwargsOutOfOrder);
  MS_RUN(testKwargsHitsDefaultSlot);
  MS_RUN(testKwargsCollectExtra);
  MS_RUN(testKwargsCollectEmptyViaPlainCall);
  MS_RUN(testKwargsDuplicateIsError);
  MS_RUN(testKwargsUnknownIsError);
  MS_RUN(testKwargsMissingIsError);
  MS_RUN(testKwargsNonMapValueIsError);
  MS_RUN(testKwargsVarargProtoIsError);
  return msTestSummary();
}
