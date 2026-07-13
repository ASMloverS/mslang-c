#include <stdint.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_list.h"
#include "mslang/ms_map.h"
#include "mslang/ms_str.h"
#include "mslang/ms_tuple.h"
#include "mslang/ms_vm.h"

// msCompile()+msVMRun() cannot reach a top-level expression's value yet (see
// test_str.c/test_list.c/test_map.c for the same, already-documented
// workaround), so chunks are built by hand instead, exactly like every other
// T053-T060 VM test.

// OP_CONST a; OP_CONST b; op; OP_RETURN. Covers eq/get_item/in/lt (all pop b
// then a and combine them); does not init/shutdown the VM (the caller must
// msVMInit() before constructing a/b, and msVMShutdown() only after
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
// mirrors runSetItem in test_list.c (value, then obj, then key), so
// OP_SET_ITEM pops key, obj, value.
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

// OP_CONST items...; OP_BUILD_TUPLE(count); OP_RETURN -- exercises the VM's
// OP_BUILD_TUPLE dispatch (ms_vm.c, wired by T061).
static MsValue runBuildTuple(MsValue* items, uint8_t count) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  for (uint8_t i = 0; i < count; i++) {
    msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, items[i]), 1);
  }
  msChunkEmitOpA(&chunk, OP_BUILD_TUPLE, count, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

static MsValue str(const char* s) {
  return msNewStr(s, (uint32_t) strlen(s));
}

static void testBuildTupleAndIndex(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildTuple(items, 3);
  MS_ASSERT_TRUE(MS_IS_OBJ(v), "is obj");
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(t->len == 3, "len 3");
  MsValue r = runBinOp(v, MS_INT_VAL(1), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_INT(r) && MS_AS_INT(r) == 2, "(1,2,3)[1] == 2");
  msVMShutdown();
}

static void testEmptyTuple(void) {
  msVMInit();
  MsValue v = runBuildTuple(NULL, 0);
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(t->len == 0, "() has len 0");
  msVMShutdown();
}

static void testConcat(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[1] = {MS_INT_VAL(3)};
  MsValue va = runBuildTuple(a, 2);
  MsValue vb = runBuildTuple(b, 1);
  MsValue r = runBinOp(va, vb, OP_ADD);
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(t->len == 3, "(1,2)+(3,) len 3");
  MS_ASSERT_TRUE(
      MS_AS_INT(t->items[0]) == 1 && MS_AS_INT(t->items[1]) == 2 && MS_AS_INT(t->items[2]) == 3,
      "(1,2)+(3,) == (1,2,3)");
  msVMShutdown();
}

static void testLen(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildTuple(items, 3);
  MsValue r = msTupleType.tpLen(&gVM, v);
  MS_ASSERT_TRUE(MS_IS_INT(r) && MS_AS_INT(r) == 3, "len((1,2,3)) == 3");
  msVMShutdown();
}

static void testEqWithStrElement(void) {
  msVMInit();
  MsValue items1[2] = {MS_INT_VAL(1), str("a")};
  MsValue items2[2] = {MS_INT_VAL(1), str("a")};
  MsValue v1 = runBuildTuple(items1, 2);
  MsValue v2 = runBuildTuple(items2, 2);
  MsValue r = runBinOp(v1, v2, OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(r) && MS_AS_BOOL(r), "(1,\"a\") == (1,\"a\")");
  msVMShutdown();
}

static void testHashIsCallableAndReturnsInt(void) {
  msVMInit();
  MsValue items[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue v = runBuildTuple(items, 2);
  MsValue r = msTupleType.tpHash(&gVM, v);
  MS_ASSERT_TRUE(MS_IS_INT(r), "hash((1,2)) is int");
  msVMShutdown();
}

static void testHashUnhashableElementIsError(void) {
  msVMInit();
  MsValue listElem = msNewList(0);
  MsValue items[2] = {MS_INT_VAL(1), listElem};
  MsValue v = runBuildTuple(items, 2);
  MsValue r = msTupleType.tpHash(&gVM, v);
  MS_ASSERT_TRUE(MS_IS_ERROR(r), "hash((1,[])) -> TypeError (list not hashable)");
  msVMShutdown();
}

static void testTupleAsMapKey(void) {
  msVMInit();
  MsValue items[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue key = runBuildTuple(items, 2);
  MsValue map = msNewMap(0);
  MsValue setResult = msMapSet(&gVM, map, key, str("ok"));
  MS_ASSERT_TRUE(!MS_IS_ERROR(setResult), "map[(1,2)] = \"ok\" did not error");

  MsValue key2 = runBuildTuple(items, 2);
  MsValue r = msMapGet(&gVM, map, key2);
  MS_ASSERT_TRUE(MS_IS_OBJ(r), "{(1,2): \"ok\"}[(1,2)] found");
  msVMShutdown();
}

static void testImmutableSetItemIsError(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildTuple(items, 3);
  MsValue r = runSetItem(v, MS_INT_VAL(0), MS_INT_VAL(99));
  MS_ASSERT_TRUE(MS_IS_ERROR(r), "t[0] = 1 -> TypeError (tuple has no tpSetitem)");
  msVMShutdown();
}

static void testContainsOperator(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildTuple(items, 3);
  MsValue r = runBinOp(MS_INT_VAL(2), v, OP_IN);
  MS_ASSERT_TRUE(MS_IS_BOOL(r) && MS_AS_BOOL(r), "2 in (1,2,3)");
  msVMShutdown();
}

static void testLexicographicLt(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[2] = {MS_INT_VAL(1), MS_INT_VAL(3)};
  MsValue va = runBuildTuple(a, 2);
  MsValue vb = runBuildTuple(b, 2);
  MsValue r = runBinOp(va, vb, OP_LT);
  MS_ASSERT_TRUE(MS_IS_BOOL(r) && MS_AS_BOOL(r), "(1,2) < (1,3)");
  msVMShutdown();
}

int main(void) {
  MS_RUN(testBuildTupleAndIndex);
  MS_RUN(testEmptyTuple);
  MS_RUN(testConcat);
  MS_RUN(testLen);
  MS_RUN(testEqWithStrElement);
  MS_RUN(testHashIsCallableAndReturnsInt);
  MS_RUN(testHashUnhashableElementIsError);
  MS_RUN(testTupleAsMapKey);
  MS_RUN(testImmutableSetItemIsError);
  MS_RUN(testContainsOperator);
  MS_RUN(testLexicographicLt);
  return msTestSummary();
}
