// test_closures.c
// T071: upvalue open/close runtime semantics -- msCaptureUpvalue reuse,
// msCloseUpvalues location/value transfer, OP_MAKE_FUNC capture (both the
// isLocal and enclosing-upvalue branches), OP_GET_UPVALUE/OP_SET_UPVALUE,
// OP_RETURN/OP_RETURN_NIL closing the returning frame's open upvalues, and
// GC survival of both open and closed upvalues. Chunks are built by hand,
// same workaround as test_call.c/test_vararg.c/test_kwargs.c (msCompile()+
// msVMRun() cannot reach a top-level expression's value in a C unit test).
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_func.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_map.h"
#include "mslang/ms_str.h"
#include "mslang/ms_upvalue.h"
#include "mslang/ms_vm.h"

// -- Direct msCaptureUpvalue/msCloseUpvalues API tests (no eval loop) --

// Capturing the same stack slot twice must return the same MsUpvalue object
// (closures sharing a variable rely on this -- vm.md ss5).
static void testCaptureUpvalueReusesSameSlot(void) {
  msVMInit();
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;
  MsValue* slot = t->sp;
  *t->sp++ = MS_INT_VAL(1);

  MsUpvalue* a = msCaptureUpvalue(t, slot);
  MsUpvalue* b = msCaptureUpvalue(t, slot);
  MS_ASSERT_TRUE(a == b, "capturing the same slot twice reuses the same MsUpvalue");
  MS_ASSERT_TRUE(a->location == slot, "captured upvalue is open (location == stack slot)");

  msVMShutdown();
}

// Different stack slots must produce distinct MsUpvalue objects, both linked
// into t->openUpvalues.
static void testCaptureUpvalueDistinctSlots(void) {
  msVMInit();
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;
  MsValue* lo = t->sp;
  *t->sp++ = MS_INT_VAL(10);
  MsValue* hi = t->sp;
  *t->sp++ = MS_INT_VAL(20);

  MsUpvalue* uvLo = msCaptureUpvalue(t, lo);
  MsUpvalue* uvHi = msCaptureUpvalue(t, hi);
  MS_ASSERT_TRUE(uvLo != uvHi, "different slots capture different MsUpvalue objects");

  msVMShutdown();
}

// msCloseUpvalues must copy the *current* stack value (not the value at
// capture time) into closedVal and redirect location to &closedVal.
static void testCloseUpvaluesTransfersLatestValue(void) {
  msVMInit();
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;
  MsValue* slot = t->sp;
  *t->sp++ = MS_INT_VAL(1);

  MsUpvalue* uv = msCaptureUpvalue(t, slot);
  *slot = MS_INT_VAL(99);  // variable mutated after capture, before close
  msCloseUpvalues(t, slot);

  MS_ASSERT_TRUE(uv->location == &uv->closedVal, "closed upvalue redirects location to &closedVal");
  MS_ASSERT_TRUE(MS_IS_INT(uv->closedVal), "closedVal is int");
  MS_ASSERT_EQ(MS_AS_INT(uv->closedVal), 99, "closedVal holds the latest stack value, not the captured one");
  MS_ASSERT_TRUE(t->openUpvalues == NULL, "closed upvalue is unlinked from t->openUpvalues");

  msVMShutdown();
}

// msCloseUpvalues(t, slot) only closes upvalues with location >= slot;
// upvalues below slot stay open.
static void testCloseUpvaluesOnlyAboveSlot(void) {
  msVMInit();
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;
  MsValue* lo = t->sp;
  *t->sp++ = MS_INT_VAL(1);
  MsValue* hi = t->sp;
  *t->sp++ = MS_INT_VAL(2);

  MsUpvalue* uvLo = msCaptureUpvalue(t, lo);
  MsUpvalue* uvHi = msCaptureUpvalue(t, hi);

  msCloseUpvalues(t, hi);  // close only slots >= hi

  MS_ASSERT_TRUE(uvHi->location == &uvHi->closedVal, "hi upvalue is closed");
  MS_ASSERT_TRUE(uvLo->location == lo, "lo upvalue stays open");
  MS_ASSERT_TRUE(t->openUpvalues == uvLo, "t->openUpvalues still holds the lo upvalue");

  msVMShutdown();
}

// An open upvalue must survive msGCCollect() via t->openUpvalues as a GC root.
static void testOpenUpvalueSurvivesGC(void) {
  msVMInit();
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;
  MsValue* slot = t->sp;
  *t->sp++ = MS_INT_VAL(7);

  MsUpvalue* uv = msCaptureUpvalue(t, slot);
  msGCCollect();
  MS_ASSERT_TRUE(MS_IS_INT(*uv->location), "open upvalue's location still valid int after GC");
  MS_ASSERT_EQ(MS_AS_INT(*uv->location), 7, "open upvalue's value unchanged after GC");

  msVMShutdown();
}

// -- Full VM-level tests (OP_MAKE_FUNC/OP_GET_UPVALUE/OP_SET_UPVALUE/OP_RETURN) --

// f() { count += 1; return count }, upvalueCount=1 (captures outer's count).
static MsValue makeCounterBodyProto(struct MsChunk* innerChunk) {
  msChunkInit(innerChunk, NULL);
  msChunkEmitOpA(innerChunk, OP_GET_UPVALUE, 0, 1);
  msChunkEmitOpAX(innerChunk, OP_CONST, msChunkAddConst(innerChunk, MS_INT_VAL(1)), 1);
  msChunkEmitOp(innerChunk, OP_ADD, 1);
  msChunkEmitOpA(innerChunk, OP_SET_UPVALUE, 0, 1);
  msChunkEmitOp(innerChunk, OP_POP, 1);
  msChunkEmitOpA(innerChunk, OP_GET_UPVALUE, 0, 1);
  msChunkEmitOp(innerChunk, OP_RETURN, 1);
  return msNewFuncProto(innerChunk, "counterBody", 0, 0, 0, 0);
}

// makeCounter() { count := 0; return func() { ... } }, localCount=1 (count).
static MsValue makeMakeCounterProto(struct MsChunk* outerChunk, struct MsChunk* innerChunk) {
  msChunkInit(outerChunk, NULL);
  msChunkEmitOpAX(outerChunk, OP_CONST, msChunkAddConst(outerChunk, MS_INT_VAL(0)), 1);
  msChunkEmitOpA(outerChunk, OP_SET_LOCAL, 0, 1);
  msChunkEmitOp(outerChunk, OP_POP, 1);

  MsValue innerProto = makeCounterBodyProto(innerChunk);
  msChunkEmitOpAX(outerChunk, OP_MAKE_FUNC, msChunkAddConst(outerChunk, innerProto), 1);
  msChunkEmit(outerChunk, 1, 1);  // upvalueCount = 1
  msChunkEmit(outerChunk, 1, 1);  // isLocal = true
  msChunkEmit(outerChunk, 0, 1);  // index = local slot 0 (count)
  msChunkEmitOp(outerChunk, OP_RETURN, 1);

  return msNewFuncProto(outerChunk, "makeCounter", 0, 0, 0, 1);
}

// counter := makeCounter(); print(counter()) three times -> 1, 2, 3, with
// count living on the heap across each separate top-level msVMRun call.
static void testMakeCounterIncrementsOnHeap(void) {
  msVMInit();
  struct MsChunk innerChunk, makeCounterChunk;
  MsValue makeCounterProto = makeMakeCounterProto(&makeCounterChunk, &innerChunk);
  MsValue makeCounterClosure = msNewClosure((MsFuncProto*) MS_AS_OBJ(makeCounterProto), 0);

  struct MsChunk callMakeCounter;
  msChunkInit(&callMakeCounter, NULL);
  msChunkEmitOpAX(&callMakeCounter, OP_CONST, msChunkAddConst(&callMakeCounter, makeCounterClosure), 1);
  msChunkEmitOpA(&callMakeCounter, OP_CALL, 0, 1);
  msChunkEmitOp(&callMakeCounter, OP_RETURN, 1);
  MsValue counterClosure = msVMRun(&callMakeCounter);
  MS_ASSERT_TRUE(MS_IS_OBJ(counterClosure), "makeCounter() returns a closure object");
  msChunkFree(&callMakeCounter);

  int expected[3] = {1, 2, 3};
  for (int i = 0; i < 3; i++) {
    struct MsChunk callCounter;
    msChunkInit(&callCounter, NULL);
    msChunkEmitOpAX(&callCounter, OP_CONST, msChunkAddConst(&callCounter, counterClosure), 1);
    msChunkEmitOpA(&callCounter, OP_CALL, 0, 1);
    msChunkEmitOp(&callCounter, OP_RETURN, 1);
    MsValue r = msVMRun(&callCounter);
    MS_ASSERT_TRUE(MS_IS_INT(r), "counter() returns int");
    MS_ASSERT_EQ(MS_AS_INT(r), expected[i], "counter() increments count on the heap");
    msChunkFree(&callCounter);
  }

  msChunkFree(&makeCounterChunk);
  msChunkFree(&innerChunk);
  msVMShutdown();
}

// -- Shared upvalue (getter/setter) + OP_RETURN_NIL closing the frame --

// getter() { return value }, upvalueCount=1.
static MsValue makeGetterProto(struct MsChunk* ch) {
  msChunkInit(ch, NULL);
  msChunkEmitOpA(ch, OP_GET_UPVALUE, 0, 1);
  msChunkEmitOp(ch, OP_RETURN, 1);
  return msNewFuncProto(ch, "getter", 0, 0, 0, 0);
}

// setter(newValue) { value = newValue } -- no explicit return: ends via
// OP_RETURN_NIL (manually appended here, mirroring ms_compiler.c's
// compileFuncToConst fall-through). arity=1, localCount=1 (newValue).
static MsValue makeSetterProto(struct MsChunk* ch) {
  msChunkInit(ch, NULL);
  msChunkEmitOpA(ch, OP_GET_LOCAL, 0, 1);
  msChunkEmitOpA(ch, OP_SET_UPVALUE, 0, 1);
  msChunkEmitOp(ch, OP_POP, 1);
  msChunkEmitOp(ch, OP_RETURN_NIL, 1);
  return msNewFuncProto(ch, "setter", 1, 1, 0, 1);
}

// makePair() { value := 0; getter := func...; setter := func...; <falls
// through, no explicit return> } -- stores getter/setter into globals (since
// this test has no tuple-building helper) and ends via OP_RETURN_NIL,
// exercising msCloseUpvalues on makePair's OWN captured local ("value") via
// the RETURN_NIL path (impl/P5-T071-closures-upvalue.md "实现要点 6").
static MsValue makeMakePairProto(struct MsChunk* ch, struct MsChunk* getterCh, struct MsChunk* setterCh) {
  msChunkInit(ch, NULL);
  msChunkEmitOpAX(ch, OP_CONST, msChunkAddConst(ch, MS_INT_VAL(0)), 1);
  msChunkEmitOpA(ch, OP_SET_LOCAL, 0, 1);
  msChunkEmitOp(ch, OP_POP, 1);

  MsValue getterProto = makeGetterProto(getterCh);
  msChunkEmitOpAX(ch, OP_MAKE_FUNC, msChunkAddConst(ch, getterProto), 1);
  msChunkEmit(ch, 1, 1);
  msChunkEmit(ch, 1, 1);
  msChunkEmit(ch, 0, 1);
  msChunkEmitOpAX(ch, OP_SET_GLOBAL, msChunkAddConst(ch, msNewStr("getter", 6)), 1);
  msChunkEmitOp(ch, OP_POP, 1);

  MsValue setterProto = makeSetterProto(setterCh);
  msChunkEmitOpAX(ch, OP_MAKE_FUNC, msChunkAddConst(ch, setterProto), 1);
  msChunkEmit(ch, 1, 1);
  msChunkEmit(ch, 1, 1);
  msChunkEmit(ch, 0, 1);
  msChunkEmitOpAX(ch, OP_SET_GLOBAL, msChunkAddConst(ch, msNewStr("setter", 6)), 1);
  msChunkEmitOp(ch, OP_POP, 1);

  msChunkEmitOp(ch, OP_RETURN_NIL, 1);
  return msNewFuncProto(ch, "makePair", 0, 0, 0, 1);
}

static void callGlobalNoArgAndCheckInt(const char* name, int64_t expected, const char* msg) {
  struct MsChunk ch;
  msChunkInit(&ch, NULL);
  msChunkEmitOpAX(&ch, OP_GET_GLOBAL, msChunkAddConst(&ch, msNewStr(name, (uint32_t) strlen(name))), 1);
  msChunkEmitOpA(&ch, OP_CALL, 0, 1);
  msChunkEmitOp(&ch, OP_RETURN, 1);
  MsValue r = msVMRun(&ch);
  MS_ASSERT_TRUE(MS_IS_INT(r), msg);
  MS_ASSERT_EQ(MS_AS_INT(r), expected, msg);
  msChunkFree(&ch);
}

static void testSharedUpvalueAndReturnNilCloses(void) {
  msVMInit();
  struct MsChunk pairChunk, getterChunk, setterChunk;
  MsValue pairProto = makeMakePairProto(&pairChunk, &getterChunk, &setterChunk);
  MsValue pairClosure = msNewClosure((MsFuncProto*) MS_AS_OBJ(pairProto), 0);

  struct MsChunk callPair;
  msChunkInit(&callPair, NULL);
  msChunkEmitOpAX(&callPair, OP_CONST, msChunkAddConst(&callPair, pairClosure), 1);
  msChunkEmitOpA(&callPair, OP_CALL, 0, 1);
  msChunkEmitOp(&callPair, OP_RETURN_NIL, 1);
  msVMRun(&callPair);
  msChunkFree(&callPair);

  // setter(42)
  struct MsChunk callSetter;
  msChunkInit(&callSetter, NULL);
  msChunkEmitOpAX(&callSetter, OP_GET_GLOBAL, msChunkAddConst(&callSetter, msNewStr("setter", 6)), 1);
  msChunkEmitOpAX(&callSetter, OP_CONST, msChunkAddConst(&callSetter, MS_INT_VAL(42)), 1);
  msChunkEmitOpA(&callSetter, OP_CALL, 1, 1);
  msChunkEmitOp(&callSetter, OP_RETURN_NIL, 1);
  msVMRun(&callSetter);
  msChunkFree(&callSetter);

  // getter() -- both must see 42, and makePair's own frame is long gone.
  callGlobalNoArgAndCheckInt("getter", 42, "getter() sees setter's write through the shared upvalue");

  // GC must not collect the closed upvalue / closures reachable only via globals.
  msGCCollect();
  callGlobalNoArgAndCheckInt("getter", 42, "getter() still works after an explicit GC collection");

  msChunkFree(&pairChunk);
  msChunkFree(&getterChunk);
  msChunkFree(&setterChunk);
  msVMShutdown();
}

// -- Nested closures: upvalue of upvalue (OP_MAKE_FUNC's non-local branch) --

// inner() { return x }, upvalueCount=1, captures middle's upvalues[0] (not local).
static MsValue makeInnerProto(struct MsChunk* ch) {
  msChunkInit(ch, NULL);
  msChunkEmitOpA(ch, OP_GET_UPVALUE, 0, 1);
  msChunkEmitOp(ch, OP_RETURN, 1);
  return msNewFuncProto(ch, "inner", 0, 0, 0, 0);
}

// middle() { return inner }, upvalueCount=1 (captured from outer as local x;
// re-exposed to inner via the isLocal=0 branch).
static MsValue makeMiddleProto(struct MsChunk* ch, struct MsChunk* innerCh) {
  msChunkInit(ch, NULL);
  MsValue innerProto = makeInnerProto(innerCh);
  msChunkEmitOpAX(ch, OP_MAKE_FUNC, msChunkAddConst(ch, innerProto), 1);
  msChunkEmit(ch, 1, 1);  // upvalueCount = 1
  msChunkEmit(ch, 0, 1);  // isLocal = false -- copy from enclosing (middle)'s upvalues[0]
  msChunkEmit(ch, 0, 1);  // index = 0
  msChunkEmitOp(ch, OP_RETURN, 1);
  return msNewFuncProto(ch, "middle", 0, 0, 0, 0);
}

// outer() { x := 1; return middle }, localCount=1 (x).
static MsValue makeOuterProto(struct MsChunk* ch, struct MsChunk* middleCh, struct MsChunk* innerCh) {
  msChunkInit(ch, NULL);
  msChunkEmitOpAX(ch, OP_CONST, msChunkAddConst(ch, MS_INT_VAL(1)), 1);
  msChunkEmitOpA(ch, OP_SET_LOCAL, 0, 1);
  msChunkEmitOp(ch, OP_POP, 1);

  MsValue middleProto = makeMiddleProto(middleCh, innerCh);
  msChunkEmitOpAX(ch, OP_MAKE_FUNC, msChunkAddConst(ch, middleProto), 1);
  msChunkEmit(ch, 1, 1);  // upvalueCount = 1
  msChunkEmit(ch, 1, 1);  // isLocal = true -- outer's own local slot 0 (x)
  msChunkEmit(ch, 0, 1);  // index = 0
  msChunkEmitOp(ch, OP_RETURN, 1);
  return msNewFuncProto(ch, "outer", 0, 0, 0, 1);
}

static void testNestedUpvalueOfUpvalue(void) {
  msVMInit();
  struct MsChunk outerCh, middleCh, innerCh;
  MsValue outerProto = makeOuterProto(&outerCh, &middleCh, &innerCh);
  MsValue outerClosure = msNewClosure((MsFuncProto*) MS_AS_OBJ(outerProto), 0);

  struct MsChunk callOuter;
  msChunkInit(&callOuter, NULL);
  msChunkEmitOpAX(&callOuter, OP_CONST, msChunkAddConst(&callOuter, outerClosure), 1);
  msChunkEmitOpA(&callOuter, OP_CALL, 0, 1);
  msChunkEmitOp(&callOuter, OP_RETURN, 1);
  MsValue middleClosure = msVMRun(&callOuter);
  MS_ASSERT_TRUE(MS_IS_OBJ(middleClosure), "outer() returns middle closure");
  msChunkFree(&callOuter);

  struct MsChunk callMiddle;
  msChunkInit(&callMiddle, NULL);
  msChunkEmitOpAX(&callMiddle, OP_CONST, msChunkAddConst(&callMiddle, middleClosure), 1);
  msChunkEmitOpA(&callMiddle, OP_CALL, 0, 1);
  msChunkEmitOp(&callMiddle, OP_RETURN, 1);
  MsValue innerClosure = msVMRun(&callMiddle);
  MS_ASSERT_TRUE(MS_IS_OBJ(innerClosure), "middle() returns inner closure");
  msChunkFree(&callMiddle);

  struct MsChunk callInner;
  msChunkInit(&callInner, NULL);
  msChunkEmitOpAX(&callInner, OP_CONST, msChunkAddConst(&callInner, innerClosure), 1);
  msChunkEmitOpA(&callInner, OP_CALL, 0, 1);
  msChunkEmitOp(&callInner, OP_RETURN, 1);
  MsValue result = msVMRun(&callInner);
  MS_ASSERT_TRUE(MS_IS_INT(result), "inner() returns int");
  MS_ASSERT_EQ(MS_AS_INT(result), 1, "inner() sees outer's x through middle's upvalue array");
  msChunkFree(&callInner);

  msChunkFree(&outerCh);
  msChunkFree(&middleCh);
  msChunkFree(&innerCh);
  msVMShutdown();
}

// -- Regression: msVMRun must reset t->openUpvalues, not just t->sp --

// Reproduces the abandoned-frame scenario: run1 captures a local into a
// closure (leaked into a global so it survives), then hits an unimplemented
// opcode so eval() bails out via `return MS_ERROR_VALUE` *without* going
// through OP_RETURN's msCloseUpvalues call, leaving the frame's open
// upvalue dangling in t->openUpvalues. run2 then captures an unrelated
// local at the same stack address (msVMRun resets t->sp to t->stack every
// run); msCaptureUpvalue's exact-address match (ms_upvalue.c) must not hand
// back run1's stale upvalue for this new, unrelated capture.
static void testSequentialVMRunsDoNotAliasUpvalues(void) {
  msVMInit();

  struct MsChunk run1Ch, getter1Ch;
  msChunkInit(&run1Ch, NULL);
  msChunkEmitOpAX(&run1Ch, OP_CONST, msChunkAddConst(&run1Ch, MS_INT_VAL(999)), 1);
  msChunkEmitOpA(&run1Ch, OP_SET_LOCAL, 0, 1);
  msChunkEmitOp(&run1Ch, OP_POP, 1);
  MsValue getter1Proto = makeGetterProto(&getter1Ch);
  msChunkEmitOpAX(&run1Ch, OP_MAKE_FUNC, msChunkAddConst(&run1Ch, getter1Proto), 1);
  msChunkEmit(&run1Ch, 1, 1);  // upvalueCount = 1
  msChunkEmit(&run1Ch, 1, 1);  // isLocal = true
  msChunkEmit(&run1Ch, 0, 1);  // index = local slot 0
  msChunkEmitOpAX(&run1Ch, OP_SET_GLOBAL, msChunkAddConst(&run1Ch, msNewStr("leaked1", 7)), 1);
  msChunkEmitOp(&run1Ch, OP_POP, 1);
  msChunkEmit(&run1Ch, (uint8_t) OP_COUNT, 1);  // unimplemented opcode: eval() bails without closing upvalues

  MsValue r1 = msVMRun(&run1Ch);
  MS_ASSERT_TRUE(MS_IS_ERROR(r1), "run1 bails via the unimplemented-opcode error path");

  struct MsChunk run2Ch, getter2Ch;
  msChunkInit(&run2Ch, NULL);
  msChunkEmitOpAX(&run2Ch, OP_CONST, msChunkAddConst(&run2Ch, MS_INT_VAL(111)), 1);
  msChunkEmitOpA(&run2Ch, OP_SET_LOCAL, 0, 1);
  msChunkEmitOp(&run2Ch, OP_POP, 1);
  MsValue getter2Proto = makeGetterProto(&getter2Ch);
  msChunkEmitOpAX(&run2Ch, OP_MAKE_FUNC, msChunkAddConst(&run2Ch, getter2Proto), 1);
  msChunkEmit(&run2Ch, 1, 1);  // upvalueCount = 1
  msChunkEmit(&run2Ch, 1, 1);  // isLocal = true
  msChunkEmit(&run2Ch, 0, 1);  // index = local slot 0 -- same stack address as run1's local 0
  msChunkEmitOp(&run2Ch, OP_RETURN, 1);

  MsValue closure2Val = msVMRun(&run2Ch);
  MS_ASSERT_TRUE(MS_IS_OBJ(closure2Val), "run2 returns closure2");

  MsThread* t = &gVM.mainThread;
  MsValue closure1Val = msMapGet(&gVM, t->globals, msNewStr("leaked1", 7));
  MS_ASSERT_TRUE(MS_IS_OBJ(closure1Val), "leaked1 global holds closure1");

  MsClosure* closure1 = (MsClosure*) MS_AS_OBJ(closure1Val);
  MsClosure* closure2 = (MsClosure*) MS_AS_OBJ(closure2Val);
  MS_ASSERT_TRUE(
      closure1->upvalues[0] != closure2->upvalues[0],
      "run2's capture must not alias run1's abandoned, still-open upvalue (t->openUpvalues reset per msVMRun)");

  msChunkFree(&run1Ch);
  msChunkFree(&getter1Ch);
  msChunkFree(&run2Ch);
  msChunkFree(&getter2Ch);
  msVMShutdown();
}

int main(void) {
  MS_RUN(testCaptureUpvalueReusesSameSlot);
  MS_RUN(testCaptureUpvalueDistinctSlots);
  MS_RUN(testCloseUpvaluesTransfersLatestValue);
  MS_RUN(testCloseUpvaluesOnlyAboveSlot);
  MS_RUN(testOpenUpvalueSurvivesGC);
  MS_RUN(testMakeCounterIncrementsOnHeap);
  MS_RUN(testSharedUpvalueAndReturnNilCloses);
  MS_RUN(testNestedUpvalueOfUpvalue);
  MS_RUN(testSequentialVMRunsDoNotAliasUpvalues);
  return msTestSummary();
}
