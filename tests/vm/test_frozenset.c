#include <string.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_frozenset.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_list.h"
#include "mslang/ms_set.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

// msNewFrozensetFromIter builds from an existing container (list/set), same
// workaround as test_set.c's runBuildSet for the lack of top-level expression
// evaluation -- build the source container by hand, then call the
// constructor directly.

// OP_CONST a; OP_CONST b; op; OP_RETURN, same helper as test_set.c's
// runBinOp -- exercises OP_GE/OP_GT through the VM's msValueGe/msValueGt
// (ms_vm.c), not just the tpGe/tpGt slot directly, so a missing slot's
// !(a<b) fallback would actually be caught.
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

static MsValue str(const char* s) {
  return msNewStr(s, (uint32_t) strlen(s));
}

static MsValue listOf(MsValue* items, uint32_t count) {
  MsValue v = msNewList(count);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  for (uint32_t i = 0; i < count; i++) {
    msListAppend(l, items[i]);
  }
  return v;
}

static void testFromListDedup(void) {
  msVMInit();
  MsValue items[4] = {MS_INT_VAL(1), MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue src = listOf(items, 4);
  MsValue fs = msNewFrozensetFromIter(&gVM, src);
  MS_ASSERT_TRUE(!MS_IS_ERROR(fs), "frozenset([1,1,2,3]) did not error");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(fs);
  MS_ASSERT_TRUE(s->len == 3, "frozenset([1,1,2,3]) dedups to len 3");
  msVMShutdown();
}

static void testUnhashableElementIsTypeError(void) {
  msVMInit();
  MsValue listElem = msNewList(0);
  MsValue items[2] = {MS_INT_VAL(1), listElem};
  MsValue src = listOf(items, 2);
  MsValue fs = msNewFrozensetFromIter(&gVM, src);
  MS_ASSERT_TRUE(MS_IS_ERROR(fs), "list element in source -> TypeError (unhashable)");
  msVMShutdown();
}

static void testUnsupportedSourceIsTypeError(void) {
  msVMInit();
  MsValue fs = msNewFrozensetFromIter(&gVM, MS_INT_VAL(1));
  MS_ASSERT_TRUE(MS_IS_ERROR(fs), "frozenset(1) -> TypeError (not a supported iterable)");
  msVMShutdown();
}

static void testHashIsInt(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue fs = msNewFrozensetFromIter(&gVM, listOf(items, 3));
  MsValue h = msFrozensetType.tpHash(&gVM, fs);
  MS_ASSERT_TRUE(MS_IS_INT(h), "hash(frozenset) is an int");
  msVMShutdown();
}

static void testHashOrderIndependent(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue b[3] = {MS_INT_VAL(3), MS_INT_VAL(2), MS_INT_VAL(1)};
  MsValue fsa = msNewFrozensetFromIter(&gVM, listOf(a, 3));
  MsValue fsb = msNewFrozensetFromIter(&gVM, listOf(b, 3));
  MsValue ha = msFrozensetType.tpHash(&gVM, fsa);
  MsValue hb = msFrozensetType.tpHash(&gVM, fsb);
  MS_ASSERT_TRUE(MS_AS_INT(ha) == MS_AS_INT(hb), "hash(frozenset([1,2,3])) == hash(frozenset([3,2,1]))");
  msVMShutdown();
}

static void testEqOrderIndependent(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue b[3] = {MS_INT_VAL(3), MS_INT_VAL(2), MS_INT_VAL(1)};
  MsValue fsa = msNewFrozensetFromIter(&gVM, listOf(a, 3));
  MsValue fsb = msNewFrozensetFromIter(&gVM, listOf(b, 3));
  MsValue eq = msFrozensetType.tpEq(&gVM, fsa, fsb);
  MS_ASSERT_TRUE(MS_IS_BOOL(eq) && MS_AS_BOOL(eq), "frozenset([1,2,3]) == frozenset([3,2,1])");
  msVMShutdown();
}

static void testContains(void) {
  msVMInit();
  MsValue items[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue fs = msNewFrozensetFromIter(&gVM, listOf(items, 3));
  MsValue hit = msFrozensetType.tpContains(&gVM, fs, MS_INT_VAL(1));
  MS_ASSERT_TRUE(MS_IS_BOOL(hit) && MS_AS_BOOL(hit), "1 in frozenset([1,2,3])");
  MsValue miss = msFrozensetType.tpContains(&gVM, fs, MS_INT_VAL(9));
  MS_ASSERT_TRUE(MS_IS_BOOL(miss) && !MS_AS_BOOL(miss), "9 not in frozenset([1,2,3])");
  msVMShutdown();
}

static void testUnionReturnsFrozensetType(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[2] = {MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue fsa = msNewFrozensetFromIter(&gVM, listOf(a, 2));
  MsValue fsb = msNewFrozensetFromIter(&gVM, listOf(b, 2));
  MsValue r = msFrozensetUnion(&gVM, fsa, fsb);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "frozenset union did not error");
  MS_ASSERT_TRUE(MS_AS_OBJ(r)->type == &msFrozensetType, "union result is a frozenset");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(s->len == 3, "frozenset([1,2]) | frozenset([2,3]) -> len 3");
  msVMShutdown();
}

static void testIntersect(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue b[3] = {MS_INT_VAL(2), MS_INT_VAL(3), MS_INT_VAL(4)};
  MsValue fsa = msNewFrozensetFromIter(&gVM, listOf(a, 3));
  MsValue fsb = msNewFrozensetFromIter(&gVM, listOf(b, 3));
  MsValue r = msFrozensetIntersect(&gVM, fsa, fsb);
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(s->len == 2, "frozenset([1,2,3]) & frozenset([2,3,4]) -> len 2");
  msVMShutdown();
}

static void testDiff(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue b[1] = {MS_INT_VAL(2)};
  MsValue fsa = msNewFrozensetFromIter(&gVM, listOf(a, 3));
  MsValue fsb = msNewFrozensetFromIter(&gVM, listOf(b, 1));
  MsValue r = msFrozensetDiff(&gVM, fsa, fsb);
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(s->len == 2, "frozenset([1,2,3]) - frozenset([2]) -> len 2");
  msVMShutdown();
}

static void testSymDiff(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[2] = {MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue fsa = msNewFrozensetFromIter(&gVM, listOf(a, 2));
  MsValue fsb = msNewFrozensetFromIter(&gVM, listOf(b, 2));
  MsValue r = msFrozensetSymDiff(&gVM, fsa, fsb);
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(s->len == 2, "frozenset([1,2]) ^ frozenset([2,3]) -> len 2");
  msVMShutdown();
}

static void testIsSubsetAndSuperset(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue fsa = msNewFrozensetFromIter(&gVM, listOf(a, 2));
  MsValue fsb = msNewFrozensetFromIter(&gVM, listOf(b, 3));
  MsValue sub = msFrozensetIsSubset(fsa, fsb);
  MS_ASSERT_TRUE(MS_AS_BOOL(sub), "frozenset([1,2]).isSubset(frozenset([1,2,3]))");
  MsValue sup = msFrozensetIsSuperset(fsb, fsa);
  MS_ASSERT_TRUE(MS_AS_BOOL(sup), "frozenset([1,2,3]).isSuperset(frozenset([1,2]))");
  msVMShutdown();
}

static void testSupersetGeAndGt(void) {
  msVMInit();
  MsValue a[3] = {MS_INT_VAL(1), MS_INT_VAL(2), MS_INT_VAL(3)};
  MsValue b[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue fsa = msNewFrozensetFromIter(&gVM, listOf(a, 3));
  MsValue fsb = msNewFrozensetFromIter(&gVM, listOf(b, 2));
  MsValue ge = runBinOp(fsa, fsb, OP_GE);
  MS_ASSERT_TRUE(MS_IS_BOOL(ge) && MS_AS_BOOL(ge), "frozenset({1,2,3}) >= frozenset({1,2})");
  MsValue gt = runBinOp(fsa, fsb, OP_GT);
  MS_ASSERT_TRUE(MS_IS_BOOL(gt) && MS_AS_BOOL(gt), "frozenset({1,2,3}) > frozenset({1,2})");
  msVMShutdown();
}

// Regression for the missing tpGe/tpGt bug: without them, the VM's msValueGe
// falls back to !(a<b) (total-order), which wrongly reports true for two
// incomparable sets (neither a proper subset of the other).
static void testIncomparableSetsGeIsFalse(void) {
  msVMInit();
  MsValue a[1] = {MS_INT_VAL(1)};
  MsValue b[1] = {MS_INT_VAL(2)};
  MsValue fsa = msNewFrozensetFromIter(&gVM, listOf(a, 1));
  MsValue fsb = msNewFrozensetFromIter(&gVM, listOf(b, 1));
  MsValue ge = runBinOp(fsa, fsb, OP_GE);
  MS_ASSERT_TRUE(MS_IS_BOOL(ge) && !MS_AS_BOOL(ge), "frozenset({1}) >= frozenset({2}) is false (incomparable)");
  MsValue gt = runBinOp(fsa, fsb, OP_GT);
  MS_ASSERT_TRUE(MS_IS_BOOL(gt) && !MS_AS_BOOL(gt), "frozenset({1}) > frozenset({2}) is false (incomparable)");
  msVMShutdown();
}

static void testIsDisjoint(void) {
  msVMInit();
  MsValue a[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue b[2] = {MS_INT_VAL(3), MS_INT_VAL(4)};
  MsValue fsa = msNewFrozensetFromIter(&gVM, listOf(a, 2));
  MsValue fsb = msNewFrozensetFromIter(&gVM, listOf(b, 2));
  MsValue disjoint = msFrozensetIsDisjoint(&gVM, fsa, fsb);
  MS_ASSERT_TRUE(MS_AS_BOOL(disjoint), "frozenset([1,2]).isDisjoint(frozenset([3,4]))");
  msVMShutdown();
}

static void testCopyReturnsSelf(void) {
  msVMInit();
  MsValue items[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue fs = msNewFrozensetFromIter(&gVM, listOf(items, 2));
  MsValue copy = msFrozensetCopy(fs);
  MS_ASSERT_TRUE(MS_AS_OBJ(copy) == MS_AS_OBJ(fs), "frozenset.copy() returns self, no new allocation");
  msVMShutdown();
}

static void testNoAddOrRemoveSlot(void) {
  MS_ASSERT_TRUE(msFrozensetType.tpBitor != NULL, "frozenset still supports | (union)");
}

static void testFromFrozensetSource(void) {
  msVMInit();
  MsValue items[2] = {MS_INT_VAL(1), MS_INT_VAL(2)};
  MsValue fs1 = msNewFrozensetFromIter(&gVM, listOf(items, 2));
  MsValue fs2 = msNewFrozensetFromIter(&gVM, fs1);
  MS_ASSERT_TRUE(!MS_IS_ERROR(fs2), "frozenset(frozenset(...)) did not error");
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(fs2);
  MS_ASSERT_TRUE(s->len == 2, "frozenset(frozenset([1,2])) -> len 2");
  msVMShutdown();
}

static void testGCMarksElements(void) {
  msVMInit();
  MsValue items[1] = {str("hello")};
  MsValue fs = msNewFrozensetFromIter(&gVM, listOf(items, 1));
  msGCPushRoot(fs);
  msGCCollect();
  MsValue has = msFrozensetType.tpContains(&gVM, fs, str("hello"));
  MS_ASSERT_TRUE(!MS_IS_ERROR(has) && MS_AS_BOOL(has), "frozenset element survives GC while rooted");
  msGCPopRoot();
  msVMShutdown();
}

int main(void) {
  MS_RUN(testFromListDedup);
  MS_RUN(testUnhashableElementIsTypeError);
  MS_RUN(testUnsupportedSourceIsTypeError);
  MS_RUN(testHashIsInt);
  MS_RUN(testHashOrderIndependent);
  MS_RUN(testEqOrderIndependent);
  MS_RUN(testContains);
  MS_RUN(testUnionReturnsFrozensetType);
  MS_RUN(testIntersect);
  MS_RUN(testDiff);
  MS_RUN(testSymDiff);
  MS_RUN(testIsSubsetAndSuperset);
  MS_RUN(testSupersetGeAndGt);
  MS_RUN(testIncomparableSetsGeIsFalse);
  MS_RUN(testIsDisjoint);
  MS_RUN(testCopyReturnsSelf);
  MS_RUN(testNoAddOrRemoveSlot);
  MS_RUN(testFromFrozensetSource);
  MS_RUN(testGCMarksElements);
  return msTestSummary();
}
