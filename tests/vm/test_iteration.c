#include <stdint.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_list.h"
#include "mslang/ms_map.h"
#include "mslang/ms_opcode.h"
#include "mslang/ms_range.h"
#include "mslang/ms_set.h"
#include "mslang/ms_str.h"
#include "mslang/ms_tuple.h"
#include "mslang/ms_vm.h"

// msCompile()+msVMRun() cannot reach a top-level expression's value yet, and
// OP_CALL is not implemented (T068), so `for`/`range()`/`len()` .ms source
// cannot be driven end-to-end today -- chunks are built by hand instead,
// exactly like every other T053-T064 VM test (see test_range.c/test_list.c).

// Mirrors ms_compiler.c's emitJump: emit op with a placeholder 3-byte AX
// operand, return the operand's offset for a later msChunkPatchJump.
static uint32_t emitJump(struct MsChunk* ck, MsOpCode op, uint32_t line) {
  msChunkEmitOpAX(ck, op, 0, line);
  return ck->codeLen - 3;
}

// Builds the same shape ms_compiler.c's compileForIn emits:
//   OP_CONST 0            (sum, slot 0)
//   OP_CONST <iterable>
//   OP_GET_ITER                          (iterator, slot 1)
// loopStart:
//   OP_FOR_ITER exit                     (value, slot 2, while not exhausted)
//   OP_GET_LOCAL 0; OP_GET_LOCAL 2; OP_ADD; OP_SET_LOCAL 0; OP_POP
//   OP_POP                               (drop slot 2, continueTarget)
//   OP_JUMP loopStart
// exit:
//   OP_GET_LOCAL 0; OP_RETURN
// Sums every int the iterable produces (values must be ints) if addLoopValue,
// otherwise counts iterations (values need not be ints -- used for
// map/set/str iteration).
static MsValue runForLoop(MsValue iterable, bool addLoopValue) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, MS_INT_VAL(0)), 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, iterable), 1);
  msChunkEmitOp(&chunk, OP_GET_ITER, 1);

  uint32_t loopStart = chunk.codeLen;
  uint32_t exitJump = emitJump(&chunk, OP_FOR_ITER, 1);
  msChunkEmitOpA(&chunk, OP_GET_LOCAL, 0, 1);
  if (addLoopValue) {
    msChunkEmitOpA(&chunk, OP_GET_LOCAL, 2, 1);
  } else {
    msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, MS_INT_VAL(1)), 1);
  }
  msChunkEmitOp(&chunk, OP_ADD, 1);
  msChunkEmitOpA(&chunk, OP_SET_LOCAL, 0, 1);
  msChunkEmitOp(&chunk, OP_POP, 1);
  msChunkEmitOp(&chunk, OP_POP, 1);  // continueTarget: drop iterated value
  uint32_t backPatch = emitJump(&chunk, OP_JUMP, 1);
  msChunkPatchJump(&chunk, backPatch, loopStart);
  msChunkPatchJump(&chunk, exitJump, chunk.codeLen);

  msChunkEmitOpA(&chunk, OP_GET_LOCAL, 0, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);

  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

static MsValue runForSum(MsValue iterable) {
  return runForLoop(iterable, true);
}

static MsValue runForCount(MsValue iterable) {
  return runForLoop(iterable, false);
}

static void testForIterList(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue list = msNewList(3);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(list);
  memcpy(l->items, items, sizeof(items));
  l->len = 3;

  MsValue sum = runForSum(list);
  MS_ASSERT_TRUE(MS_IS_INT(sum) && MS_AS_INT(sum) == 6, "for x in [1,2,3]: sum == 6");
  msVMShutdown();
}

static void testForIterEmptyListStopsImmediately(void) {
  msVMInit();
  MsValue list = msNewList(0);
  MsValue sum = runForSum(list);
  MS_ASSERT_TRUE(MS_IS_INT(sum) && MS_AS_INT(sum) == 0, "for x in []: loop body never runs");
  msVMShutdown();
}

static void testForIterRange(void) {
  msVMInit();
  MsValue r = msNewRange(0, 5, 1);
  MsValue sum = runForSum(r);
  MS_ASSERT_TRUE(MS_IS_INT(sum) && MS_AS_INT(sum) == 10, "for i in range(5): sum == 10");
  msVMShutdown();
}

static void testForIterStrCountsCodepoints(void) {
  msVMInit();
  MsValue s = msNewStr("hello", 5);
  MsValue count = runForCount(s);
  MS_ASSERT_TRUE(MS_IS_INT(count) && MS_AS_INT(count) == 5, "for c in \"hello\": 5 iterations");
  msVMShutdown();
}

static void testStrIterYieldsSingleCharStrings(void) {
  msVMInit();
  MsValue s = msNewStr("hi", 2);
  MsValue it = msStrType.tpIter(&gVM, s);
  MsValue c0 = msTypeOf(it)->tpNext(&gVM, it);
  MS_ASSERT_TRUE(MS_IS_OBJ(c0), "first char is a str object");
  struct MsStrObj* s0 = (struct MsStrObj*) MS_AS_OBJ(c0);
  MS_ASSERT_TRUE(s0->len == 1 && s0->data[0] == 'h', "first char is \"h\"");
  MsValue c1 = msTypeOf(it)->tpNext(&gVM, it);
  struct MsStrObj* s1 = (struct MsStrObj*) MS_AS_OBJ(c1);
  MS_ASSERT_TRUE(s1->len == 1 && s1->data[0] == 'i', "second char is \"i\"");
  MsValue done = msTypeOf(it)->tpNext(&gVM, it);
  MS_ASSERT_TRUE(MS_IS_NIL(done), "iterator exhausted after 2 chars");
  msVMShutdown();
}

static void testForIterMapCountsKeys(void) {
  msVMInit();
  MsValue m = msNewMap(4);
  msMapSet(&gVM, m, msNewStr("a", 1), MS_INT_VAL(1));
  msMapSet(&gVM, m, msNewStr("b", 1), MS_INT_VAL(2));
  MsValue count = runForCount(m);
  MS_ASSERT_TRUE(MS_IS_INT(count) && MS_AS_INT(count) == 2, "for k in map: 2 iterations");
  msVMShutdown();
}

static void testForIterSetCountsElements(void) {
  msVMInit();
  MsValue s = msNewSet(4);
  msSetAdd(&gVM, s, MS_INT_VAL(1));
  msSetAdd(&gVM, s, MS_INT_VAL(2));
  msSetAdd(&gVM, s, MS_INT_VAL(3));
  MsValue count = runForCount(s);
  MS_ASSERT_TRUE(MS_IS_INT(count) && MS_AS_INT(count) == 3, "for x in set: 3 iterations");
  msVMShutdown();
}

static void testTupleIterInOrder(void) {
  msVMInit();
  MsValue tup = msNewTuple(3);
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(tup);
  t->items[0] = MS_INT_VAL(10);
  t->items[1] = MS_INT_VAL(20);
  t->items[2] = MS_INT_VAL(30);

  MsValue it = msTupleType.tpIter(&gVM, tup);
  int64_t expect[3] = {10, 20, 30};
  for (int i = 0; i < 3; i++) {
    MsValue v = msTypeOf(it)->tpNext(&gVM, it);
    MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == expect[i], "tuple iterates in order");
  }
  MsValue done = msTypeOf(it)->tpNext(&gVM, it);
  MS_ASSERT_TRUE(MS_IS_NIL(done), "tuple iterator exhausted");
  msVMShutdown();
}

static void testNonIterableTypeErrors(void) {
  msVMInit();
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, MS_INT_VAL(42)), 1);
  msChunkEmitOp(&chunk, OP_GET_ITER, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "iter(42) -> TypeError");
  msChunkFree(&chunk);
  msVMShutdown();
}

// OP_CONST list; OP_CONST lo; OP_CONST hi; OP_BUILD_SLICE flags; OP_GET_ITEM;
// OP_RETURN. lo/hi/step are MS_NIL_VAL when the corresponding flag bit is
// clear (and then not pushed/const'd at all).
static MsValue runSlice(MsValue obj, MsValue lo, MsValue hi, MsValue step, uint8_t flags) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, obj), 1);
  if (flags & MS_SLICE_HAS_LO) {
    msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, lo), 1);
  }
  if (flags & MS_SLICE_HAS_HI) {
    msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, hi), 1);
  }
  if (flags & MS_SLICE_HAS_STEP) {
    msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, step), 1);
  }
  msChunkEmitOpA(&chunk, OP_BUILD_SLICE, flags, 1);
  msChunkEmitOp(&chunk, OP_GET_ITEM, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

static void testListSlice(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue list = msNewList(3);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(list);
  memcpy(l->items, items, sizeof(items));
  l->len = 3;

  MsValue r = runSlice(list, MS_INT_VAL(1), MS_INT_VAL(2), MS_NIL_VAL, MS_SLICE_HAS_LO | MS_SLICE_HAS_HI);
  MS_ASSERT_TRUE(MS_IS_OBJ(r), "[1,2,3][1:2] is a list");
  struct MsListObj* lr = (struct MsListObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(lr->len == 1 && MS_AS_INT(lr->items[0]) == 2, "[1,2,3][1:2] == [2]");
  msVMShutdown();
}

static void testStrSlice(void) {
  msVMInit();
  MsValue s = msNewStr("hello", 5);
  MsValue r = runSlice(s, MS_INT_VAL(1), MS_INT_VAL(4), MS_NIL_VAL, MS_SLICE_HAS_LO | MS_SLICE_HAS_HI);
  MS_ASSERT_TRUE(MS_IS_OBJ(r), "\"hello\"[1:4] is a str");
  struct MsStrObj* sr = (struct MsStrObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(sr->len == 3 && memcmp(sr->data, "ell", 3) == 0, "\"hello\"[1:4] == \"ell\"");
  msVMShutdown();
}

static void testStrSliceNegativeIndex(void) {
  msVMInit();
  MsValue s = msNewStr("hello", 5);
  MsValue r = runSlice(s, MS_INT_VAL(-1), MS_NIL_VAL, MS_NIL_VAL, MS_SLICE_HAS_LO);
  struct MsStrObj* sr = (struct MsStrObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(sr->len == 1 && sr->data[0] == 'o', "\"hello\"[-1:] == \"o\"");
  msVMShutdown();
}

static void testTupleStepSlice(void) {
  msVMInit();
  MsValue tup = msNewTuple(3);
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(tup);
  t->items[0] = MS_INT_VAL(1);
  t->items[1] = MS_INT_VAL(2);
  t->items[2] = MS_INT_VAL(3);

  MsValue r = runSlice(tup, MS_NIL_VAL, MS_NIL_VAL, MS_INT_VAL(2), MS_SLICE_HAS_STEP);
  MS_ASSERT_TRUE(MS_IS_OBJ(r), "(1,2,3)[::2] is a tuple");
  struct MsTupleObj* tr = (struct MsTupleObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(tr->len == 2 && MS_AS_INT(tr->items[0]) == 1 && MS_AS_INT(tr->items[1]) == 3, "(1,2,3)[::2] == (1,3)");
  msVMShutdown();
}

static void testSliceStepZeroErrors(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue list = msNewList(3);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(list);
  memcpy(l->items, items, sizeof(items));
  l->len = 3;

  MsValue r = runSlice(list, MS_NIL_VAL, MS_NIL_VAL, MS_INT_VAL(0), MS_SLICE_HAS_STEP);
  MS_ASSERT_TRUE(MS_IS_ERROR(r), "[1,2,3][::0] -> ValueError instead of crashing");
  msVMShutdown();
}

int main(void) {
  MS_RUN(testForIterList);
  MS_RUN(testForIterEmptyListStopsImmediately);
  MS_RUN(testForIterRange);
  MS_RUN(testForIterStrCountsCodepoints);
  MS_RUN(testStrIterYieldsSingleCharStrings);
  MS_RUN(testForIterMapCountsKeys);
  MS_RUN(testForIterSetCountsElements);
  MS_RUN(testTupleIterInOrder);
  MS_RUN(testNonIterableTypeErrors);
  MS_RUN(testListSlice);
  MS_RUN(testStrSlice);
  MS_RUN(testStrSliceNegativeIndex);
  MS_RUN(testTupleStepSlice);
  MS_RUN(testSliceStepZeroErrors);
  return msTestSummary();
}
