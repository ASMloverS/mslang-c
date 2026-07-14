#include <string.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_list.h"
#include "mslang/ms_set.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

// msCompile()+msVMRun() cannot reach a top-level expression's value yet (see
// test_str.c/test_list.c/test_map.c/test_tuple.c for the same,
// already-documented workaround), so chunks are built by hand instead,
// exactly like every other T053-T061 VM test.

// OP_CONST a; OP_CONST b; op; OP_RETURN. Covers eq/in/lt/le/ge/gt/bitwise ops
// (all pop b then a and combine them).
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

// OP_CONST items...; OP_BUILD_SET(count); OP_RETURN -- exercises the VM's
// OP_BUILD_SET dispatch (ms_vm.c, wired by T062).
static MsValue runBuildSet(MsValue* items, uint8_t count) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  for (uint8_t i = 0; i < count; i++) {
    msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, items[i]), 1);
  }
  msChunkEmitOpA(&chunk, OP_BUILD_SET, count, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

static MsValue str(const char* s) {
  return msNewStr(s, (uint32_t) strlen(s));
}

static void testBuildSetDedup(void) {
  msVMInit();
  MsValue items[4] = {MS_INT_VAL(1), MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(2)};
  MsValue v = runBuildSet(items, 4);
  MS_ASSERT_TRUE(MS_IS_OBJ(v), "is obj");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(s->len == 2, "{1,1,2,2} dedups to len 2");
  msVMShutdown();
}

static void testBuildSetLen3(void) {
  msVMInit();
  MsValue items[4] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildSet(items, 4);
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(s->len == 3, "{1,2,2,3} dedups to len 3");
  msVMShutdown();
}

static void testEmptySet(void) {
  msVMInit();
  MsValue v = runBuildSet(NULL, 0);
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(s->len == 0, "empty set has len 0");
  msVMShutdown();
}

static void testContainsOperator(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue v = runBuildSet(items, 3);
  MsValue hit = runBinOp(MS_INT_VAL(1), v, OP_IN);
  MS_ASSERT_TRUE(MS_IS_BOOL(hit) && MS_AS_BOOL(hit), "1 in {1,2,3}");
  MsValue miss = runBinOp(MS_INT_VAL(5), v, OP_IN);
  MS_ASSERT_TRUE(MS_IS_BOOL(miss) && !MS_AS_BOOL(miss), "5 not in {1,2,3}");
  msVMShutdown();
}

static void testUnion(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[2] = {MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue va = runBuildSet(a, 2);
  MsValue vb = runBuildSet(b, 2);
  MsValue r = runBinOp(va, vb, OP_BOR);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "{1,2}|{2,3} did not error");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(s->len == 3, "{1,2}|{2,3} len 3");
  MsValue has1 = msSetHas(&gVM, r, MS_INT_VAL(1));
  MsValue has2 = msSetHas(&gVM, r, MS_INT_VAL(2));
  MsValue has3 = msSetHas(&gVM, r, MS_INT_VAL(3));
  MS_ASSERT_TRUE(MS_AS_BOOL(has1) && MS_AS_BOOL(has2) && MS_AS_BOOL(has3), "union contains 1,2,3");
  msVMShutdown();
}

static void testIntersect(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue b[3] = {MS_INT_VAL(2), MS_INT_VAL(3), MS_INT_VAL(4)};
  MsValue va = runBuildSet(a, 3);
  MsValue vb = runBuildSet(b, 3);
  MsValue r = runBinOp(va, vb, OP_BAND);
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(s->len == 2, "{1,2,3}&{2,3,4} len 2");
  MS_ASSERT_TRUE(MS_AS_BOOL(msSetHas(&gVM, r, MS_INT_VAL(2))), "intersection contains 2");
  MS_ASSERT_TRUE(MS_AS_BOOL(msSetHas(&gVM, r, MS_INT_VAL(3))), "intersection contains 3");
  MS_ASSERT_TRUE(!MS_AS_BOOL(msSetHas(&gVM, r, MS_INT_VAL(1))), "intersection excludes 1");
  msVMShutdown();
}

static void testDiff(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue b[1] = {MS_INT_VAL(2)};
  MsValue va = runBuildSet(a, 3);
  MsValue vb = runBuildSet(b, 1);
  MsValue r = runBinOp(va, vb, OP_SUB);
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(s->len == 2, "{1,2,3}-{2} len 2");
  MS_ASSERT_TRUE(MS_AS_BOOL(msSetHas(&gVM, r, MS_INT_VAL(1))), "diff contains 1");
  MS_ASSERT_TRUE(MS_AS_BOOL(msSetHas(&gVM, r, MS_INT_VAL(3))), "diff contains 3");
  MS_ASSERT_TRUE(!MS_AS_BOOL(msSetHas(&gVM, r, MS_INT_VAL(2))), "diff excludes 2");
  msVMShutdown();
}

static void testSymDiff(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[2] = {MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue va = runBuildSet(a, 2);
  MsValue vb = runBuildSet(b, 2);
  MsValue r = runBinOp(va, vb, OP_BXOR);
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(s->len == 2, "{1,2}^{2,3} len 2");
  MS_ASSERT_TRUE(MS_AS_BOOL(msSetHas(&gVM, r, MS_INT_VAL(1))), "symdiff contains 1");
  MS_ASSERT_TRUE(MS_AS_BOOL(msSetHas(&gVM, r, MS_INT_VAL(3))), "symdiff contains 3");
  MS_ASSERT_TRUE(!MS_AS_BOOL(msSetHas(&gVM, r, MS_INT_VAL(2))), "symdiff excludes 2");
  msVMShutdown();
}

static void testSubsetLe(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue va = runBuildSet(a, 2);
  MsValue vb = runBuildSet(b, 3);
  MsValue r = runBinOp(va, vb, OP_LE);
  MS_ASSERT_TRUE(MS_IS_BOOL(r) && MS_AS_BOOL(r), "{1,2} <= {1,2,3}");
  msVMShutdown();
}

static void testProperSubsetLtFalseWhenEqual(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue va1 = runBuildSet(a, 2);
  MsValue va2 = runBuildSet(a, 2);
  MsValue r = runBinOp(va1, va2, OP_LT);
  MS_ASSERT_TRUE(MS_IS_BOOL(r) && !MS_AS_BOOL(r), "{1,2} < {1,2} is false (not a proper subset)");
  msVMShutdown();
}

static void testProperSubsetLtTrue(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue va = runBuildSet(a, 2);
  MsValue vb = runBuildSet(b, 3);
  MsValue r = runBinOp(va, vb, OP_LT);
  MS_ASSERT_TRUE(MS_IS_BOOL(r) && MS_AS_BOOL(r), "{1,2} < {1,2,3}");
  msVMShutdown();
}

static void testSupersetGeAndGt(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue b[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue va = runBuildSet(a, 3);
  MsValue vb = runBuildSet(b, 2);
  MsValue ge = runBinOp(va, vb, OP_GE);
  MS_ASSERT_TRUE(MS_IS_BOOL(ge) && MS_AS_BOOL(ge), "{1,2,3} >= {1,2}");
  MsValue gt = runBinOp(va, vb, OP_GT);
  MS_ASSERT_TRUE(MS_IS_BOOL(gt) && MS_AS_BOOL(gt), "{1,2,3} > {1,2}");
  msVMShutdown();
}

static void testEq(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue va1 = runBuildSet(a, 2);
  MsValue va2 = runBuildSet(a, 2);
  MsValue r = runBinOp(va1, va2, OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(r) && MS_AS_BOOL(r), "{1,2} == {1,2}");
  msVMShutdown();
}

static void testUnhashableElementIsTypeError(void) {
  msVMInit();
  MsValue listElem = msNewList(0);
  MsValue items[2] = {MS_INT_VAL(1), listElem};
  MsValue r = runBuildSet(items, 2);
  MS_ASSERT_TRUE(MS_IS_ERROR(r), "list element in set literal -> TypeError (unhashable)");
  msVMShutdown();
}

static void testAddRemoveDiscard(void) {
  msVMInit();
  MsValue v = runBuildSet(NULL, 0);
  MsValue addR = msSetAdd(&gVM, v, MS_INT_VAL(1));
  MS_ASSERT_TRUE(!MS_IS_ERROR(addR), "add(1) did not error");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(s->len == 1, "len 1 after add");

  MsValue removeR = msSetRemove(&gVM, v, MS_INT_VAL(1));
  MS_ASSERT_TRUE(!MS_IS_ERROR(removeR), "remove(1) did not error");
  MS_ASSERT_TRUE(s->len == 0, "len 0 after remove");

  MsValue removeMissing = msSetRemove(&gVM, v, MS_INT_VAL(99));
  MS_ASSERT_TRUE(MS_IS_ERROR(removeMissing), "remove(missing) -> KeyError");

  MsValue discardMissing = msSetDiscard(&gVM, v, MS_INT_VAL(99));
  MS_ASSERT_TRUE(!MS_IS_ERROR(discardMissing), "discard(missing) is silently ignored");
  msVMShutdown();
}

static void testPopAndClear(void) {
  msVMInit();
  MsValue items[1] = {MS_INT_VAL(42)};
  MsValue v = runBuildSet(items, 1);
  MsValue popped = msSetPop(&gVM, v);
  MS_ASSERT_TRUE(MS_IS_INT(popped) && MS_AS_INT(popped) == 42, "pop() returns the sole element");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(s->len == 0, "len 0 after pop");

  MsValue popEmpty = msSetPop(&gVM, v);
  MS_ASSERT_TRUE(MS_IS_ERROR(popEmpty), "pop() on empty set -> KeyError");

  MsValue items2[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue v2 = runBuildSet(items2, 2);
  msSetClear(v2);
  struct MsSetObj* s2 = (struct MsSetObj*) MS_AS_OBJ(v2);
  MS_ASSERT_TRUE(s2->len == 0, "clear() -> len 0");
  msVMShutdown();
}

static void testCopyShallow(void) {
  msVMInit();
  MsValue items[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue v = runBuildSet(items, 2);
  MsValue copy = msSetCopy(v);
  MS_ASSERT_TRUE(MS_AS_OBJ(copy) != MS_AS_OBJ(v), "copy() returns a distinct object");
  struct MsSetObj* sc = (struct MsSetObj*) MS_AS_OBJ(copy);
  MS_ASSERT_TRUE(sc->len == 2, "copy has same length");
  MS_ASSERT_TRUE(MS_AS_BOOL(msSetHas(&gVM, copy, MS_INT_VAL(1))), "copy contains 1");
  msVMShutdown();
}

static void testIsDisjoint(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[2] = {MS_INT_VAL(3), MS_INT_VAL(4)};
  MsValue c[2] = {MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue va = runBuildSet(a, 2);
  MsValue vb = runBuildSet(b, 2);
  MsValue vc = runBuildSet(c, 2);
  MsValue disjoint = msSetIsDisjoint(va, vb);
  MS_ASSERT_TRUE(MS_AS_BOOL(disjoint), "{1,2}.isDisjoint({3,4}) is true");
  MsValue notDisjoint = msSetIsDisjoint(va, vc);
  MS_ASSERT_TRUE(!MS_AS_BOOL(notDisjoint), "{1,2}.isDisjoint({2,3}) is false");
  msVMShutdown();
}

static void testUpdate(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[2] = {MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue va = runBuildSet(a, 2);
  MsValue vb = runBuildSet(b, 2);
  MsValue r = msSetUpdate(&gVM, va, vb);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "update did not error");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(va);
  MS_ASSERT_TRUE(s->len == 3, "{1,2}.update({2,3}) -> len 3 (|=)");
  msVMShutdown();
}

static void testIntersectionUpdate(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue b[2] = {MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue va = runBuildSet(a, 3);
  MsValue vb = runBuildSet(b, 2);
  MsValue r = msSetIntersectionUpdate(&gVM, va, vb);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "intersectionUpdate did not error");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(va);
  MS_ASSERT_TRUE(s->len == 2, "{1,2,3}.intersectionUpdate({2,3}) -> len 2 (&=)");
  MS_ASSERT_TRUE(!MS_AS_BOOL(msSetHas(&gVM, va, MS_INT_VAL(1))), "1 removed by intersectionUpdate");
  msVMShutdown();
}

static void testDifferenceUpdate(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue b[1] = {MS_INT_VAL(2)};
  MsValue va = runBuildSet(a, 3);
  MsValue vb = runBuildSet(b, 1);
  MsValue r = msSetDifferenceUpdate(&gVM, va, vb);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "differenceUpdate did not error");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(va);
  MS_ASSERT_TRUE(s->len == 2, "{1,2,3}.differenceUpdate({2}) -> len 2 (-=)");
  MS_ASSERT_TRUE(!MS_AS_BOOL(msSetHas(&gVM, va, MS_INT_VAL(2))), "2 removed by differenceUpdate");
  msVMShutdown();
}

static void testSymmetricDifferenceUpdate(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[2] = {MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue va = runBuildSet(a, 2);
  MsValue vb = runBuildSet(b, 2);
  MsValue r = msSetSymmetricDifferenceUpdate(&gVM, va, vb);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "symmetricDifferenceUpdate did not error");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(va);
  MS_ASSERT_TRUE(s->len == 2, "{1,2}.symmetricDifferenceUpdate({2,3}) -> len 2 (^=)");
  MS_ASSERT_TRUE(MS_AS_BOOL(msSetHas(&gVM, va, MS_INT_VAL(1))), "1 kept");
  MS_ASSERT_TRUE(MS_AS_BOOL(msSetHas(&gVM, va, MS_INT_VAL(3))), "3 added");
  MS_ASSERT_TRUE(!MS_AS_BOOL(msSetHas(&gVM, va, MS_INT_VAL(2))), "2 removed (common element)");
  msVMShutdown();
}

static void testGCMarksElements(void) {
  msVMInit();
  MsValue items[1] = {str("hello")};
  MsValue v = runBuildSet(items, 1);
  msGCPushRoot(v);
  msGCCollect();
  MsValue has = msSetHas(&gVM, v, str("hello"));
  MS_ASSERT_TRUE(!MS_IS_ERROR(has) && MS_AS_BOOL(has), "set element survives GC while set is rooted");
  msGCPopRoot();
  msVMShutdown();
}

int main(void) {
  MS_RUN(testBuildSetDedup);
  MS_RUN(testBuildSetLen3);
  MS_RUN(testEmptySet);
  MS_RUN(testContainsOperator);
  MS_RUN(testUnion);
  MS_RUN(testIntersect);
  MS_RUN(testDiff);
  MS_RUN(testSymDiff);
  MS_RUN(testSubsetLe);
  MS_RUN(testProperSubsetLtFalseWhenEqual);
  MS_RUN(testProperSubsetLtTrue);
  MS_RUN(testSupersetGeAndGt);
  MS_RUN(testEq);
  MS_RUN(testUnhashableElementIsTypeError);
  MS_RUN(testAddRemoveDiscard);
  MS_RUN(testPopAndClear);
  MS_RUN(testCopyShallow);
  MS_RUN(testIsDisjoint);
  MS_RUN(testUpdate);
  MS_RUN(testIntersectionUpdate);
  MS_RUN(testDifferenceUpdate);
  MS_RUN(testSymmetricDifferenceUpdate);
  MS_RUN(testGCMarksElements);
  return msTestSummary();
}
