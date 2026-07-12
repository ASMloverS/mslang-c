#include <math.h>
#include <stdint.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_list.h"
#include "mslang/ms_map.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

// msCompile()+msVMRun() cannot reach a top-level expression's value yet (see
// test_str.c/test_list.c for the same, already-documented workaround), so
// chunks are built by hand instead, exactly like every other T053-T059 VM
// test.

// OP_CONST a; OP_CONST b; op; OP_RETURN. Covers eq/get_item/in (all pop b
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

// OP_CONST key0; OP_CONST val0; ...; OP_BUILD_MAP(count); OP_RETURN --
// mirrors the push order ms_compiler.c's compileMap emits (key then val per
// pair, bottom-most pair first).
static MsValue runBuildMap(MsValue* pairs, uint8_t count) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  for (uint8_t i = 0; i < count; i++) {
    msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, pairs[(size_t) i * 2]), 1);
    msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, pairs[(size_t) i * 2 + 1]), 1);
  }
  msChunkEmitOpA(&chunk, OP_BUILD_MAP, count, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

static MsValue str(const char* s) {
  return msNewStr(s, (uint32_t) strlen(s));
}

static void testBuildMapAndIndex(void) {
  msVMInit();
  MsValue pairs[4] = {str("a"), MS_INT_VAL(1), str("b"), MS_INT_VAL(2)};
  MsValue v = runBuildMap(pairs, 2);
  MS_ASSERT_TRUE(MS_IS_OBJ(v), "is obj");
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(m->len == 2, "len 2");

  MsValue r = runBinOp(v, str("a"), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_INT(r) && MS_AS_INT(r) == 1, "{\"a\":1,\"b\":2}[\"a\"] == 1");
  msVMShutdown();
}

static void testSetItemThenGet(void) {
  msVMInit();
  MsValue v = runBuildMap(NULL, 0);
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MsValue setResult = msMapSet(&gVM, v, str("x"), MS_INT_VAL(42));
  MS_ASSERT_TRUE(!MS_IS_ERROR(setResult), "set did not error");
  MS_ASSERT_TRUE(m->len == 1, "len 1 after set");
  MsValue r = runBinOp(v, str("x"), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_INT(r) && MS_AS_INT(r) == 42, "m[\"x\"] == 42");
  msVMShutdown();
}

static void testContainsOperator(void) {
  msVMInit();
  MsValue pairs[2] = {str("a"), MS_INT_VAL(1)};
  MsValue v = runBuildMap(pairs, 1);
  MsValue hit = runBinOp(str("a"), v, OP_IN);
  MS_ASSERT_TRUE(MS_IS_BOOL(hit) && MS_AS_BOOL(hit), "\"a\" in {\"a\":1}");
  MsValue miss = runBinOp(str("b"), v, OP_IN);
  MS_ASSERT_TRUE(MS_IS_BOOL(miss) && !MS_AS_BOOL(miss), "\"b\" not in {\"a\":1}");
  msVMShutdown();
}

static void testDel(void) {
  msVMInit();
  MsValue pairs[2] = {str("a"), MS_INT_VAL(1)};
  MsValue v = runBuildMap(pairs, 1);
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MsValue r = msMapDel(&gVM, v, str("a"));
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "del did not error");
  MS_ASSERT_TRUE(m->len == 0, "len 0 after del");
  MsValue has = msMapHas(&gVM, v, str("a"));
  MS_ASSERT_TRUE(MS_IS_BOOL(has) && !MS_AS_BOOL(has), "\"a\" no longer present");
  msVMShutdown();
}

static void testDelMissingIsError(void) {
  msVMInit();
  MsValue v = runBuildMap(NULL, 0);
  MsValue r = msMapDel(&gVM, v, str("missing"));
  MS_ASSERT_TRUE(MS_IS_ERROR(r), "del of missing key is an error (KeyError)");
  msVMShutdown();
}

static void testGetDefault(void) {
  msVMInit();
  MsValue v = runBuildMap(NULL, 0);
  MsValue r = msMapGetDefault(&gVM, v, str("missing"), MS_INT_VAL(0));
  MS_ASSERT_TRUE(MS_IS_INT(r) && MS_AS_INT(r) == 0, "m.get(\"missing\", 0) == 0");
  msVMShutdown();
}

static void testIntKeys(void) {
  msVMInit();
  MsValue pairs[4] = {MS_INT_VAL(1), str("one"), MS_INT_VAL(2), str("two")};
  MsValue v = runBuildMap(pairs, 2);
  MsValue r = runBinOp(v, MS_INT_VAL(2), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_OBJ(r), "{1:\"one\",2:\"two\"}[2] is a str object");
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  (void) m;
  msVMShutdown();
}

static void testUnhashableKeyIsTypeError(void) {
  msVMInit();
  MsValue listKey = msNewList(0);
  MsValue pairs[2] = {listKey, MS_INT_VAL(1)};
  MsValue r = runBuildMap(pairs, 1);
  MS_ASSERT_TRUE(MS_IS_ERROR(r), "list as map key -> TypeError (unhashable)");
  msVMShutdown();
}

static void testNanKeyIsTypeError(void) {
  msVMInit();
  MsValue pairs[2] = {MS_FLOAT_VAL(NAN), MS_INT_VAL(1)};
  MsValue r = runBuildMap(pairs, 1);
  MS_ASSERT_TRUE(MS_IS_ERROR(r), "nan as map key -> TypeError");
  msVMShutdown();
}

static void testMissingIndexIsKeyError(void) {
  msVMInit();
  MsValue pairs[2] = {str("a"), MS_INT_VAL(1)};
  MsValue v = runBuildMap(pairs, 1);
  MsValue r = runBinOp(v, str("missing"), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_ERROR(r), "m[\"missing\"] -> KeyError, unlike m.get(\"missing\")");
  msVMShutdown();
}

static void testNilKey(void) {
  msVMInit();
  MsValue pairs[2] = {MS_NIL_VAL, MS_INT_VAL(1)};
  MsValue v = runBuildMap(pairs, 1);
  MsValue r = runBinOp(v, MS_NIL_VAL, OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_INT(r) && MS_AS_INT(r) == 1, "{nil:1}[nil] == 1");
  msVMShutdown();
}

static void testResizeGrowsCorrectly(void) {
  msVMInit();
  MsValue v = runBuildMap(NULL, 0);
  for (int64_t i = 0; i < 100; i++) {
    MsValue r = msMapSet(&gVM, v, MS_INT_VAL(i), MS_INT_VAL(i * 2));
    MS_ASSERT_TRUE(!MS_IS_ERROR(r), "set(i) did not error");
  }
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(m->len == 100, "len == 100 after 100 inserts");
  for (int64_t i = 0; i < 100; i++) {
    MsValue got = msMapGet(&gVM, v, MS_INT_VAL(i));
    if (MS_AS_INT(got) != i * 2) {
      MS_ASSERT_TRUE(false, "value mismatch after resize");
      break;
    }
  }
  MS_ASSERT_TRUE(true, "all 100 values intact after resizes");
  msVMShutdown();
}

static void testEq(void) {
  msVMInit();
  MsValue pairs[2] = {str("a"), MS_INT_VAL(1)};
  MsValue v1 = runBuildMap(pairs, 1);
  MsValue v2 = runBuildMap(pairs, 1);
  MsValue r = runBinOp(v1, v2, OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(r) && MS_AS_BOOL(r), "{\"a\":1} == {\"a\":1}");
  msVMShutdown();
}

static void testKeysValuesItems(void) {
  msVMInit();
  MsValue pairs[4] = {str("a"), MS_INT_VAL(1), str("b"), MS_INT_VAL(2)};
  MsValue v = runBuildMap(pairs, 2);

  MsValue keys = msMapKeys(&gVM, v);
  struct MsListObj* kl = (struct MsListObj*) MS_AS_OBJ(keys);
  MS_ASSERT_TRUE(kl->len == 2, "keys() len 2");

  MsValue values = msMapValues(&gVM, v);
  struct MsListObj* vl = (struct MsListObj*) MS_AS_OBJ(values);
  MS_ASSERT_TRUE(vl->len == 2, "values() len 2");

  MsValue items = msMapItems(&gVM, v);
  struct MsListObj* il = (struct MsListObj*) MS_AS_OBJ(items);
  MS_ASSERT_TRUE(il->len == 2, "items() len 2");
  struct MsListObj* firstPair = (struct MsListObj*) MS_AS_OBJ(il->items[0]);
  MS_ASSERT_TRUE(firstPair->len == 2, "items() element is a 2-element [k, v] pair");
  msVMShutdown();
}

static void testPopWithDefault(void) {
  msVMInit();
  MsValue pairs[2] = {str("a"), MS_INT_VAL(1)};
  MsValue v = runBuildMap(pairs, 1);
  MsValue popped = msMapPop(&gVM, v, str("a"), MS_NIL_VAL);
  MS_ASSERT_TRUE(MS_IS_INT(popped) && MS_AS_INT(popped) == 1, "pop(\"a\") == 1");
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(m->len == 0, "len 0 after pop");
  MsValue defaulted = msMapPop(&gVM, v, str("missing"), MS_INT_VAL(-1));
  MS_ASSERT_TRUE(MS_IS_INT(defaulted) && MS_AS_INT(defaulted) == -1, "pop(missing, -1) == -1, not an error");
  msVMShutdown();
}

static void testUpdate(void) {
  msVMInit();
  MsValue pairs1[2] = {str("a"), MS_INT_VAL(1)};
  MsValue pairs2[2] = {str("b"), MS_INT_VAL(2)};
  MsValue v = runBuildMap(pairs1, 1);
  MsValue other = runBuildMap(pairs2, 1);
  MsValue r = msMapUpdate(&gVM, v, other);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "update did not error");
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(m->len == 2, "{\"a\":1}.update({\"b\":2}) -> len 2");
  msVMShutdown();
}

static void testClear(void) {
  msVMInit();
  MsValue pairs[2] = {str("a"), MS_INT_VAL(1)};
  MsValue v = runBuildMap(pairs, 1);
  msMapClear(v);
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(m->len == 0, "clear() -> len 0");
  msVMShutdown();
}

static void testCopyShallow(void) {
  msVMInit();
  MsValue pairs[2] = {str("a"), MS_INT_VAL(1)};
  MsValue v = runBuildMap(pairs, 1);
  MsValue copy = msMapCopy(v);
  MS_ASSERT_TRUE(MS_AS_OBJ(copy) != MS_AS_OBJ(v), "copy() returns a distinct object");
  struct MsMapObj* mc = (struct MsMapObj*) MS_AS_OBJ(copy);
  MS_ASSERT_TRUE(mc->len == 1, "copy has same length");
  MsValue r = msMapGet(&gVM, copy, str("a"));
  MS_ASSERT_TRUE(MS_IS_INT(r) && MS_AS_INT(r) == 1, "copy[\"a\"] == 1");
  msVMShutdown();
}

static void testSetDefault(void) {
  msVMInit();
  MsValue v = runBuildMap(NULL, 0);
  MsValue r1 = msMapSetDefault(&gVM, v, str("a"), MS_INT_VAL(1));
  MS_ASSERT_TRUE(MS_IS_INT(r1) && MS_AS_INT(r1) == 1, "setDefault on missing key sets and returns 1");
  MsValue r2 = msMapSetDefault(&gVM, v, str("a"), MS_INT_VAL(99));
  MS_ASSERT_TRUE(MS_IS_INT(r2) && MS_AS_INT(r2) == 1, "setDefault on existing key leaves value untouched");
  msVMShutdown();
}

int main(void) {
  MS_RUN(testBuildMapAndIndex);
  MS_RUN(testSetItemThenGet);
  MS_RUN(testContainsOperator);
  MS_RUN(testDel);
  MS_RUN(testDelMissingIsError);
  MS_RUN(testGetDefault);
  MS_RUN(testIntKeys);
  MS_RUN(testUnhashableKeyIsTypeError);
  MS_RUN(testNanKeyIsTypeError);
  MS_RUN(testMissingIndexIsKeyError);
  MS_RUN(testNilKey);
  MS_RUN(testResizeGrowsCorrectly);
  MS_RUN(testEq);
  MS_RUN(testKeysValuesItems);
  MS_RUN(testPopWithDefault);
  MS_RUN(testUpdate);
  MS_RUN(testClear);
  MS_RUN(testCopyShallow);
  MS_RUN(testSetDefault);
  return msTestSummary();
}
