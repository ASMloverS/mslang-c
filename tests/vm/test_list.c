#include <stdint.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_list.h"
#include "mslang/ms_vm.h"

// msCompile()+msVMRun() cannot reach a top-level expression's value yet (see
// test_str.c/test_bytes.c for the same, already-documented workaround), so
// chunks are built by hand instead, exactly like every other T053-T058 VM test.

// OP_CONST a; OP_CONST b; op; OP_RETURN. Covers add/mul/eq/get_item/in (all
// pop b then a and combine them); does not init/shutdown the VM (the caller
// must msVMInit() before constructing a/b, and msVMShutdown() only after
// asserting on the -- possibly heap-object -- result).
static MsValue runBinOp(MsValue a, MsValue b, MsOpCode op) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, a), 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, b), 1);
  msChunkEmitOp(&chunk, op, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

// OP_CONST val; OP_CONST container; OP_CONST key; OP_SET_ITEM; OP_RETURN --
// mirrors the push order ms_compiler.c's compileAssign emits for `obj[key] =
// val` (value, then obj, then key), so OP_SET_ITEM pops key, obj, value.
static MsValue runSetItem(MsValue container, MsValue key, MsValue val) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, val), 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, container), 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, key), 1);
  msChunkEmitOp(&chunk, OP_SET_ITEM, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

// OP_CONST items...; OP_BUILD_LIST(count); OP_RETURN -- exercises the VM's
// OP_BUILD_LIST dispatch (ms_vm.c, wired by T059).
static MsValue runBuildList(MsValue* items, uint8_t count) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  for (uint8_t i = 0; i < count; i++) {
    msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, items[i]), 1);
  }
  msChunkEmitOpA(&chunk, OP_BUILD_LIST, count, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

static void testBuildListAndIndex(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildList(items, 3);
  MS_ASSERT_TRUE(MS_IS_OBJ(v), "is obj");
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(l->len == 3, "len 3");
  MS_ASSERT_TRUE(MS_AS_INT(l->items[0]) == 1, "items[0]=1");
  MS_ASSERT_TRUE(MS_AS_INT(l->items[2]) == 3, "items[2]=3");

  MsValue idx0 = runBinOp(v, MS_INT_VAL(0), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_AS_INT(idx0) == 1, "v[0] == 1");
  MsValue idxNeg1 = runBinOp(v, MS_INT_VAL(-1), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_AS_INT(idxNeg1) == 3, "v[-1] == 3");
  msVMShutdown();
}

static void testBuildListEmpty(void) {
  msVMInit();
  MsValue v = runBuildList(NULL, 0);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(l->len == 0, "empty list len 0");
  msVMShutdown();
}

static void testIndexOutOfRange(void) {
  msVMInit();
  MsValue items[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue v = runBuildList(items, 2);
  MsValue result = runBinOp(v, MS_INT_VAL(5), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "out-of-range index is an error");
  msVMShutdown();
}

static void testSetItemMutability(void) {
  msVMInit();
  MsValue items[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue v = runBuildList(items, 2);
  MsValue result = runSetItem(v, MS_INT_VAL(0), MS_INT_VAL(99));
  MS_ASSERT_TRUE(!MS_IS_ERROR(result), "set_item did not error");
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(MS_AS_INT(l->items[0]) == 99, "v[0] = 99 mutates in place");
  msVMShutdown();
}

static void testConcat(void) {
  msVMInit();
  MsValue a1[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue a2[2] = {MS_INT_VAL(3), MS_INT_VAL(4)};
  MsValue la = runBuildList(a1, 2);
  MsValue lb = runBuildList(a2, 2);
  MsValue result = runBinOp(la, lb, OP_ADD);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(result);
  MS_ASSERT_TRUE(l->len == 4, "concat len 4");
  MS_ASSERT_TRUE(MS_AS_INT(l->items[0]) == 1 && MS_AS_INT(l->items[3]) == 4, "concat content");
  msVMShutdown();
}

// Regression for the zero-count memcpy(NULL, ..., 0) UB found in review:
// concatenating two empty lists must not crash (msNewList(0) leaves
// items==NULL, so listConcat's memcpy calls must guard on count).
static void testConcatBothEmpty(void) {
  msVMInit();
  MsValue la = runBuildList(NULL, 0);
  MsValue lb = runBuildList(NULL, 0);
  MsValue result = runBinOp(la, lb, OP_ADD);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(result);
  MS_ASSERT_TRUE(l->len == 0, "[] + [] -> len 0");
  msVMShutdown();
}

static void testRepeat(void) {
  msVMInit();
  MsValue a[1] = {MS_INT_VAL(0)};
  MsValue la = runBuildList(a, 1);
  MsValue result = runBinOp(la, MS_INT_VAL(3), OP_MUL);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(result);
  MS_ASSERT_TRUE(l->len == 3, "repeat len 3");
  MS_ASSERT_TRUE(MS_AS_INT(l->items[0]) == 0 && MS_AS_INT(l->items[2]) == 0, "repeat content");
  msVMShutdown();
}

static void testLen(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildList(a, 3);
  struct MsType* t = MS_AS_OBJ(v)->type;
  MS_ASSERT_TRUE(MS_AS_INT(t->tpLen(NULL, v)) == 3, "len([1,2,3]) == 3");
  msVMShutdown();
}

static void testContainsOperator(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildList(a, 3);
  // OP_IN pops container then item (compiler pushes item, then container).
  MsValue result = runBinOp(MS_INT_VAL(2), v, OP_IN);
  MS_ASSERT_TRUE(MS_IS_BOOL(result) && MS_AS_BOOL(result), "2 in [1,2,3]");

  MsValue notResult = runBinOp(MS_INT_VAL(5), v, OP_NOT_IN);
  MS_ASSERT_TRUE(MS_IS_BOOL(notResult) && MS_AS_BOOL(notResult), "5 not in [1,2,3]");
  msVMShutdown();
}

static void testEq(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue v1 = runBuildList(a, 2);
  MsValue v2 = runBuildList(a, 2);
  MsValue result = runBinOp(v1, v2, OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(result) && MS_AS_BOOL(result), "[1,2] == [1,2]");
  msVMShutdown();
}

static void testAppend(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildList(a, 3);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MsValue result = msListAppend(l, MS_INT_VAL(4));
  MS_ASSERT_TRUE(!MS_IS_ERROR(result), "append did not error");
  MS_ASSERT_TRUE(l->len == 4 && MS_AS_INT(l->items[3]) == 4, "[1,2,3].append(4) -> [1,2,3,4]");
  msVMShutdown();
}

// Overwrites ->len directly (no multi-GB alloc needed) to exercise the
// (uint64_t) overflow guard in msListAppend; the guard fires before any
// realloc is attempted.
static void testAppendOverflow(void) {
  msVMInit();
  MsValue v = msNewList(1);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  l->len = UINT32_MAX;
  MsValue result = msListAppend(l, MS_INT_VAL(1));
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "append at UINT32_MAX len is an error");
  msVMShutdown();
}

static void testPop(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildList(a, 3);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MsValue popped = msListPop(l, -1);
  MS_ASSERT_TRUE(MS_AS_INT(popped) == 3, "[1,2,3].pop() == 3");
  MS_ASSERT_TRUE(l->len == 2, "list becomes [1,2]");
  msVMShutdown();
}

static void testPopOutOfRange(void) {
  msVMInit();
  MsValue v = msNewList(0);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MsValue result = msListPop(l, 0);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "pop from empty list is an error");
  msVMShutdown();
}

static void testInsert(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue v = runBuildList(a, 2);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MsValue result = msListInsert(l, 0, MS_INT_VAL(0));
  MS_ASSERT_TRUE(!MS_IS_ERROR(result), "insert did not error");
  MS_ASSERT_TRUE(
      l->len == 3 && MS_AS_INT(l->items[0]) == 0 && MS_AS_INT(l->items[1]) == 1, "[1,2].insert(0, 0) -> [0,1,2]");
  msVMShutdown();
}

static void testSort(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(3), MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue v = runBuildList(a, 3);
  MsValue result = msListSort(v, false);
  MS_ASSERT_TRUE(!MS_IS_ERROR(result), "sort did not error");
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(
      MS_AS_INT(l->items[0]) == 1 && MS_AS_INT(l->items[1]) == 2 && MS_AS_INT(l->items[2]) == 3,
      "[3,1,2].sort() -> [1,2,3]");
  msVMShutdown();
}

static void testSortReverse(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(3), MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue v = runBuildList(a, 3);
  msListSort(v, true);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(MS_AS_INT(l->items[0]) == 3 && MS_AS_INT(l->items[2]) == 1, "[3,1,2].sort(reverse=true) -> [3,2,1]");
  msVMShutdown();
}

static void testReverse(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildList(a, 3);
  msListReverse(v);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(MS_AS_INT(l->items[0]) == 3 && MS_AS_INT(l->items[2]) == 1, "[1,2,3].reverse() -> [3,2,1]");
  msVMShutdown();
}

static void testRemove(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildList(a, 3);
  MsValue result = msListRemove(v, MS_INT_VAL(2));
  MS_ASSERT_TRUE(!MS_IS_ERROR(result), "remove did not error");
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(
      l->len == 2 && MS_AS_INT(l->items[0]) == 1 && MS_AS_INT(l->items[1]) == 3, "[1,2,3].remove(2) -> [1,3]");

  MsValue missing = msListRemove(v, MS_INT_VAL(99));
  MS_ASSERT_TRUE(MS_IS_ERROR(missing), "remove(missing) is an error");
  msVMShutdown();
}

static void testIndexMethod(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildList(a, 3);
  MsValue idx = msListIndex(v, MS_INT_VAL(2), 0);
  MS_ASSERT_TRUE(MS_AS_INT(idx) == 1, "index(2) == 1");
  MsValue notFound = msListIndex(v, MS_INT_VAL(99), 0);
  MS_ASSERT_TRUE(MS_AS_INT(notFound) == -1, "index(99) == -1 (not found)");
  msVMShutdown();
}

static void testCount(void) {
  msVMInit();
  MsValue a[4] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(1), MS_INT_VAL(1)};
  MsValue v = runBuildList(a, 4);
  MsValue c = msListCount(v, MS_INT_VAL(1));
  MS_ASSERT_TRUE(MS_AS_INT(c) == 3, "count(1) == 3");
  msVMShutdown();
}

static void testExtend(void) {
  msVMInit();
  MsValue a1[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue a2[2] = {MS_INT_VAL(3), MS_INT_VAL(4)};
  MsValue v = runBuildList(a1, 2);
  MsValue other = runBuildList(a2, 2);
  MsValue result = msListExtend(v, other);
  MS_ASSERT_TRUE(!MS_IS_ERROR(result), "extend did not error");
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(l->len == 4 && MS_AS_INT(l->items[3]) == 4, "[1,2].extend([3,4]) -> [1,2,3,4]");
  msVMShutdown();
}

// Regression for the zero-count memcpy UB found in review: extending an
// empty list with an empty other must not crash (both sides may have
// items==NULL).
static void testExtendBothEmpty(void) {
  msVMInit();
  MsValue v = runBuildList(NULL, 0);
  MsValue other = runBuildList(NULL, 0);
  MsValue result = msListExtend(v, other);
  MS_ASSERT_TRUE(!MS_IS_ERROR(result), "extend did not error");
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(l->len == 0, "[].extend([]) -> len 0");
  msVMShutdown();
}

static void testClear(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildList(a, 3);
  msListClear(v);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(l->len == 0, "clear() -> len 0");
  msVMShutdown();
}

// [1, [2,3]].copy() -> shallow copy: outer is a distinct object, but the
// inner [2,3] list is the exact same object (not deep-copied).
static void testCopyShallow(void) {
  msVMInit();
  MsValue inner1[2] = {MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue innerList = runBuildList(inner1, 2);
  MsValue outer[2] = {MS_INT_VAL(1), innerList};
  MsValue v = runBuildList(outer, 2);

  MsValue copy = msListCopy(v);
  MS_ASSERT_TRUE(MS_AS_OBJ(copy) != MS_AS_OBJ(v), "copy() returns a distinct outer object");
  struct MsListObj* lc = (struct MsListObj*) MS_AS_OBJ(copy);
  MS_ASSERT_TRUE(lc->len == 2, "copy has same length");
  MS_ASSERT_TRUE(MS_AS_OBJ(lc->items[1]) == MS_AS_OBJ(innerList), "inner list is the same object (shallow)");
  msVMShutdown();
}

// Regression for the zero-count memcpy UB found in review: copying an empty
// list must not crash (both source and destination items may be NULL).
static void testCopyEmpty(void) {
  msVMInit();
  MsValue v = runBuildList(NULL, 0);
  MsValue copy = msListCopy(v);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(copy);
  MS_ASSERT_TRUE(l->len == 0, "[].copy() -> len 0");
  msVMShutdown();
}

int main(void) {
  MS_RUN(testBuildListAndIndex);
  MS_RUN(testBuildListEmpty);
  MS_RUN(testIndexOutOfRange);
  MS_RUN(testSetItemMutability);
  MS_RUN(testConcat);
  MS_RUN(testConcatBothEmpty);
  MS_RUN(testRepeat);
  MS_RUN(testLen);
  MS_RUN(testContainsOperator);
  MS_RUN(testEq);
  MS_RUN(testAppend);
  MS_RUN(testAppendOverflow);
  MS_RUN(testPop);
  MS_RUN(testPopOutOfRange);
  MS_RUN(testInsert);
  MS_RUN(testSort);
  MS_RUN(testSortReverse);
  MS_RUN(testReverse);
  MS_RUN(testRemove);
  MS_RUN(testIndexMethod);
  MS_RUN(testCount);
  MS_RUN(testExtend);
  MS_RUN(testExtendBothEmpty);
  MS_RUN(testClear);
  MS_RUN(testCopyShallow);
  MS_RUN(testCopyEmpty);
  return msTestSummary();
}
