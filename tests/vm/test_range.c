#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_range.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

// msBuiltinRange/msNewRange are exercised directly (no OP_CALL/globals
// dispatch yet, T068/T096), and iteration is driven by hand via tpIter/tpNext
// instead of a for-loop script (OP_GET_ITER/OP_FOR_ITER land in T065) -- same
// workaround as test_frozenset.c/test_set.c for the lack of top-level
// expression evaluation.

static void collectIter(MsValue rangeVal, int64_t* out, int max, int* count) {
  MsValue it = msRangeType.tpIter(&gVM, rangeVal);
  int n = 0;
  for (;;) {
    MsValue v = msRangeIterType.tpNext(&gVM, it);
    if (MS_IS_NIL(v)) {
      break;
    }
    MS_ASSERT_TRUE(n < max, "iterator produced fewer values than max");
    out[n++] = MS_AS_INT(v);
  }
  *count = n;
}

static void testRangeStopOnlyIterates(void) {
  msVMInit();
  MsValue args[1] = {MS_INT_VAL(5)};
  MsValue r = msBuiltinRange(&gVM, args, 1);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "range(5) did not error");
  int64_t out[8];
  int n;
  collectIter(r, out, 8, &n);
  MS_ASSERT_TRUE(n == 5, "range(5) produces 5 values");
  int64_t expect[5] = {0, 1, 2, 3, 4};
  for (int i = 0; i < 5; i++) {
    MS_ASSERT_TRUE(out[i] == expect[i], "range(5) value matches 0..4");
  }
  msVMShutdown();
}

static void testRangeStartStopStepIterates(void) {
  msVMInit();
  MsValue args[3] = {MS_INT_VAL(2), MS_INT_VAL(8), MS_INT_VAL(2)};
  MsValue r = msBuiltinRange(&gVM, args, 3);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "range(2,8,2) did not error");
  int64_t out[8];
  int n;
  collectIter(r, out, 8, &n);
  MS_ASSERT_TRUE(n == 3, "range(2,8,2) produces 3 values");
  int64_t expect[3] = {2, 4, 6};
  for (int i = 0; i < 3; i++) {
    MS_ASSERT_TRUE(out[i] == expect[i], "range(2,8,2) value matches 2,4,6");
  }
  msVMShutdown();
}

static void testRangeNegativeStepIterates(void) {
  msVMInit();
  MsValue args[3] = {MS_INT_VAL(5), MS_INT_VAL(0), MS_INT_VAL(-1)};
  MsValue r = msBuiltinRange(&gVM, args, 3);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "range(5,0,-1) did not error");
  int64_t out[8];
  int n;
  collectIter(r, out, 8, &n);
  MS_ASSERT_TRUE(n == 5, "range(5,0,-1) produces 5 values");
  int64_t expect[5] = {5, 4, 3, 2, 1};
  for (int i = 0; i < 5; i++) {
    MS_ASSERT_TRUE(out[i] == expect[i], "range(5,0,-1) value matches 5..1");
  }
  msVMShutdown();
}

static void testLenIsO1(void) {
  msVMInit();
  MsValue args[1] = {MS_INT_VAL(10)};
  MsValue r = msBuiltinRange(&gVM, args, 1);
  MsValue len = msRangeType.tpLen(&gVM, r);
  MS_ASSERT_TRUE(MS_IS_INT(len) && MS_AS_INT(len) == 10, "len(range(10)) == 10");
  msVMShutdown();
}

static void testGetItem(void) {
  msVMInit();
  MsValue args[1] = {MS_INT_VAL(10)};
  MsValue r = msBuiltinRange(&gVM, args, 1);
  MsValue v = msRangeType.tpGetitem(&gVM, r, MS_INT_VAL(3));
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 3, "range(10)[3] == 3");
  MsValue neg = msRangeType.tpGetitem(&gVM, r, MS_INT_VAL(-1));
  MS_ASSERT_TRUE(MS_IS_INT(neg) && MS_AS_INT(neg) == 9, "range(10)[-1] == 9");
  MsValue oob = msRangeType.tpGetitem(&gVM, r, MS_INT_VAL(10));
  MS_ASSERT_TRUE(MS_IS_ERROR(oob), "range(10)[10] -> IndexError");
  msVMShutdown();
}

static void testContains(void) {
  msVMInit();
  MsValue args[1] = {MS_INT_VAL(10)};
  MsValue r = msBuiltinRange(&gVM, args, 1);
  MsValue hit = msRangeType.tpContains(&gVM, r, MS_INT_VAL(5));
  MS_ASSERT_TRUE(MS_IS_BOOL(hit) && MS_AS_BOOL(hit), "5 in range(10)");
  MsValue miss = msRangeType.tpContains(&gVM, r, MS_INT_VAL(10));
  MS_ASSERT_TRUE(MS_IS_BOOL(miss) && !MS_AS_BOOL(miss), "10 not in range(10)");
  msVMShutdown();
}

static void testEmptyRangeStopLessThanStart(void) {
  msVMInit();
  MsValue args[1] = {MS_INT_VAL(0)};
  MsValue r = msBuiltinRange(&gVM, args, 1);
  MsValue len = msRangeType.tpLen(&gVM, r);
  MS_ASSERT_TRUE(MS_AS_INT(len) == 0, "range(0) has len 0");
  msVMShutdown();
}

static void testEmptyRangeWrongDirection(void) {
  msVMInit();
  MsValue args[3] = {MS_INT_VAL(0), MS_INT_VAL(0), MS_INT_VAL(-1)};
  MsValue r = msBuiltinRange(&gVM, args, 3);
  MS_ASSERT_TRUE(!MS_IS_ERROR(r), "range(0,0,-1) did not error");
  MsValue len = msRangeType.tpLen(&gVM, r);
  MS_ASSERT_TRUE(MS_AS_INT(len) == 0, "range(0,0,-1) has len 0");
  msVMShutdown();
}

static void testZeroStepIsError(void) {
  msVMInit();
  MsValue args[3] = {MS_INT_VAL(0), MS_INT_VAL(10), MS_INT_VAL(0)};
  MsValue r = msBuiltinRange(&gVM, args, 3);
  MS_ASSERT_TRUE(MS_IS_ERROR(r), "range(0,10,0) -> ValueError placeholder");
  msVMShutdown();
}

static void testWrongArgCountIsError(void) {
  msVMInit();
  MsValue r0 = msBuiltinRange(&gVM, NULL, 0);
  MS_ASSERT_TRUE(MS_IS_ERROR(r0), "range() with 0 args -> TypeError");
  MsValue args[4] = {MS_INT_VAL(0), MS_INT_VAL(1), MS_INT_VAL(1), MS_INT_VAL(1)};
  MsValue r4 = msBuiltinRange(&gVM, args, 4);
  MS_ASSERT_TRUE(MS_IS_ERROR(r4), "range() with 4 args -> TypeError");
  msVMShutdown();
}

static void testNonIntArgIsError(void) {
  msVMInit();
  MsValue str = msNewStr("x", 1);
  MsValue args[1] = {str};
  MsValue r = msBuiltinRange(&gVM, args, 1);
  MS_ASSERT_TRUE(MS_IS_ERROR(r), "range(\"x\") -> TypeError");
  msVMShutdown();
}

static void testRepr(void) {
  msVMInit();
  MsValue r = msNewRange(1, 5, 2);
  MsValue rep = msRangeType.tpRepr(&gVM, r);
  MS_ASSERT_TRUE(MS_IS_OBJ(rep), "repr(range) is a str object");
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(rep);
  MS_ASSERT_TRUE(strncmp(s->data, "range(1, 5, 2)", s->len) == 0, "repr(range(1,5,2)) == 'range(1, 5, 2)'");
  msVMShutdown();
}

static void testReprExtremeValuesFitBuffer(void) {
  msVMInit();
  MsValue r = msNewRange(INT64_MIN, INT64_MIN, INT64_MIN);
  MsValue rep = msRangeType.tpRepr(&gVM, r);
  MS_ASSERT_TRUE(MS_IS_OBJ(rep), "repr(range) with INT64_MIN fields is a str object");
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(rep);
  char expect[96];
  int n = snprintf(
      expect,
      sizeof(expect),
      "range(%lld, %lld, %lld)",
      (long long) INT64_MIN,
      (long long) INT64_MIN,
      (long long) INT64_MIN);
  MS_ASSERT_TRUE(s->len == (uint32_t) n, "repr length matches full formatted string, not truncated");
  MS_ASSERT_TRUE(strncmp(s->data, expect, s->len) == 0, "repr(range(INT64_MIN,...)) is not truncated/corrupted");
  msVMShutdown();
}

static void testEqEmptyRangesAlwaysEqual(void) {
  msVMInit();
  MsValue a = msNewRange(0, 0, 1);
  MsValue b = msNewRange(5, 2, 1);
  MsValue eq = msRangeType.tpEq(&gVM, a, b);
  MS_ASSERT_TRUE(MS_AS_BOOL(eq), "two empty ranges are equal regardless of start/step");
  msVMShutdown();
}

static void testEqSameStartAndStep(void) {
  msVMInit();
  MsValue a = msNewRange(0, 10, 2);
  MsValue b = msNewRange(0, 10, 2);
  MsValue eq = msRangeType.tpEq(&gVM, a, b);
  MS_ASSERT_TRUE(MS_AS_BOOL(eq), "range(0,10,2) == range(0,10,2)");
  msVMShutdown();
}

static void testEqDifferentStepDiffers(void) {
  msVMInit();
  MsValue a = msNewRange(0, 10, 1);
  MsValue b = msNewRange(0, 10, 2);
  MsValue eq = msRangeType.tpEq(&gVM, a, b);
  MS_ASSERT_TRUE(!MS_AS_BOOL(eq), "range(0,10,1) != range(0,10,2)");
  msVMShutdown();
}

static void testIterSelfReturnsSameObject(void) {
  msVMInit();
  MsValue r = msNewRange(0, 3, 1);
  MsValue it = msRangeType.tpIter(&gVM, r);
  MsValue self = msRangeIterType.tpIter(&gVM, it);
  MS_ASSERT_TRUE(MS_AS_OBJ(self) == MS_AS_OBJ(it), "iter(range_iterator) returns self");
  msVMShutdown();
}

int main(void) {
  MS_RUN(testRangeStopOnlyIterates);
  MS_RUN(testRangeStartStopStepIterates);
  MS_RUN(testRangeNegativeStepIterates);
  MS_RUN(testLenIsO1);
  MS_RUN(testGetItem);
  MS_RUN(testContains);
  MS_RUN(testEmptyRangeStopLessThanStart);
  MS_RUN(testEmptyRangeWrongDirection);
  MS_RUN(testZeroStepIsError);
  MS_RUN(testWrongArgCountIsError);
  MS_RUN(testNonIntArgIsError);
  MS_RUN(testRepr);
  MS_RUN(testReprExtremeValuesFitBuffer);
  MS_RUN(testEqEmptyRangesAlwaysEqual);
  MS_RUN(testEqSameStartAndStep);
  MS_RUN(testEqDifferentStepDiffers);
  MS_RUN(testIterSelfReturnsSameObject);
  return msTestSummary();
}
