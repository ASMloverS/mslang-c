#include <string.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

// msCompile()+msVMRun() cannot reach a top-level expression's value yet
// (OP_RETURN_NIL has no case in eval() before later tasks land; see
// test_comparison.c/test_locals.c for the same workaround), so chunks are
// built by hand instead, exactly like every other T053-T056 VM test.

// OP_CONST a; OP_CONST b; op; OP_RETURN. Covers add/mul/lt/eq/get_item/in
// (all pop b then a and combine them); does not init/shutdown the VM (the
// caller must msVMInit() before constructing a/b, and msVMShutdown() only
// after asserting on the -- possibly heap-object -- result).
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

static void testConcat(void) {
  msVMInit();
  MsValue result = runBinOp(msNewStr("hello", 5), msNewStr(" world", 6), OP_ADD);
  MS_ASSERT_TRUE(MS_IS_OBJ(result), "is obj");
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(result);
  MS_ASSERT_TRUE(s->len == 11 && memcmp(s->data, "hello world", 11) == 0, "concat");
  msVMShutdown();
}

static void testRepeat(void) {
  msVMInit();
  MsValue result = runBinOp(msNewStr("ab", 2), MS_INT_VAL(3), OP_MUL);
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(result);
  MS_ASSERT_TRUE(s->len == 6 && memcmp(s->data, "ababab", 6) == 0, "repeat 3x");
  msVMShutdown();
}

static void testRepeatNonPositive(void) {
  msVMInit();
  MsValue result = runBinOp(msNewStr("ab", 2), MS_INT_VAL(0), OP_MUL);
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(result);
  MS_ASSERT_TRUE(s->len == 0, "repeat 0x is empty");
  msVMShutdown();
}

static void testIndexPositive(void) {
  msVMInit();
  MsValue result = runBinOp(msNewStr("hello", 5), MS_INT_VAL(1), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_INT(result) && MS_AS_INT(result) == 'e', "hello[1] == 'e'");
  msVMShutdown();
}

static void testIndexNegative(void) {
  msVMInit();
  MsValue result = runBinOp(msNewStr("hello", 5), MS_INT_VAL(-1), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_INT(result) && MS_AS_INT(result) == 'o', "hello[-1] == 'o'");
  msVMShutdown();
}

static void testIndexOutOfRange(void) {
  msVMInit();
  MsValue result = runBinOp(msNewStr("hi", 2), MS_INT_VAL(5), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "out-of-range index is an error");
  msVMShutdown();
}

static void testLenBytes(void) {
  msVMInit();
  MsValue ascii = msNewStr("hello", 5);
  MsValue utf8 = msNewStr("\xe4\xbd\xa0\xe5\xa5\xbd", 6);  // "你好", 2 codepoints, 6 bytes
  struct MsType* t = MS_AS_OBJ(ascii)->type;
  MS_ASSERT_TRUE(MS_AS_INT(t->tpLen(NULL, ascii)) == 5, "len(hello) == 5");
  MS_ASSERT_TRUE(MS_AS_INT(t->tpLen(NULL, utf8)) == 6, "len(utf8 2-char) == 6 bytes");
  msVMShutdown();
}

static void testCodepointCount(void) {
  msVMInit();
  MsValue utf8 = msNewStr("\xe4\xbd\xa0\xe5\xa5\xbd", 6);
  MsValue cp = msStrCodepointCount(utf8);
  MS_ASSERT_TRUE(MS_IS_INT(cp) && MS_AS_INT(cp) == 2, "codepointCount == 2");
  msVMShutdown();
}

static void testLessThan(void) {
  msVMInit();
  MsValue result = runBinOp(msNewStr("abc", 3), msNewStr("abd", 3), OP_LT);
  MS_ASSERT_TRUE(MS_IS_BOOL(result) && MS_AS_BOOL(result), "abc < abd");
  msVMShutdown();
}

static void testEqualityInterned(void) {
  msVMInit();
  MsValue a = msNewStr("hello", 5);
  MsValue b = msNewStr("hello", 5);
  MS_ASSERT_TRUE(MS_AS_OBJ(a) == MS_AS_OBJ(b), "short equal strings share one MsStrObj*");
  MsValue result = runBinOp(a, b, OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(result) && MS_AS_BOOL(result), "hello == hello");
  msVMShutdown();
}

static void testNoInternAcrossNoIntern(void) {
  msVMInit();
  MsValue a = msNewStrNoIntern("abcxyz", 6);
  MsValue b = msNewStrNoIntern("abcxyz", 6);
  MS_ASSERT_TRUE(MS_AS_OBJ(a) != MS_AS_OBJ(b), "msNewStrNoIntern never shares pointers");
  MsValue result = runBinOp(a, b, OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(result) && MS_AS_BOOL(result), "still equal by content");
  msVMShutdown();
}

static void testHashEqual(void) {
  msVMInit();
  MsValue a = msNewStr("abc", 3);
  MsValue b = msNewStrNoIntern("abc", 3);
  struct MsType* t = MS_AS_OBJ(a)->type;
  MsValue ha = t->tpHash(NULL, a);
  MsValue hb = t->tpHash(NULL, b);
  MS_ASSERT_TRUE(MS_AS_INT(ha) == MS_AS_INT(hb), "hash(abc) == hash(abc)");
  msVMShutdown();
}

static void testUpperLower(void) {
  msVMInit();
  MsValue v = msNewStr("HeLLo", 5);
  MsValue lo = msStrLower(v);
  MsValue up = msStrUpper(v);
  struct MsStrObj* slo = (struct MsStrObj*) MS_AS_OBJ(lo);
  struct MsStrObj* sup = (struct MsStrObj*) MS_AS_OBJ(up);
  MS_ASSERT_TRUE(memcmp(slo->data, "hello", 5) == 0, "lower()");
  MS_ASSERT_TRUE(memcmp(sup->data, "HELLO", 5) == 0, "upper()");
  msVMShutdown();
}

static void testStrip(void) {
  msVMInit();
  MsValue v = msNewStr(" ab ", 4);
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(msStrStrip(v, NULL, 0));
  MS_ASSERT_TRUE(s->len == 2 && memcmp(s->data, "ab", 2) == 0, "strip()");

  struct MsStrObj* l = (struct MsStrObj*) MS_AS_OBJ(msStrLStrip(v, NULL, 0));
  MS_ASSERT_TRUE(l->len == 3 && memcmp(l->data, "ab ", 3) == 0, "lstrip()");

  struct MsStrObj* r = (struct MsStrObj*) MS_AS_OBJ(msStrRStrip(v, NULL, 0));
  MS_ASSERT_TRUE(r->len == 3 && memcmp(r->data, " ab", 3) == 0, "rstrip()");
  msVMShutdown();
}

static void testContainsOperator(void) {
  msVMInit();
  // OP_IN pops container then item (compiler pushes item, then container).
  MsValue result = runBinOp(msNewStr("ell", 3), msNewStr("hello", 5), OP_IN);
  MS_ASSERT_TRUE(MS_IS_BOOL(result) && MS_AS_BOOL(result), "ell in hello");

  MsValue notResult = runBinOp(msNewStr("xyz", 3), msNewStr("hello", 5), OP_NOT_IN);
  MS_ASSERT_TRUE(MS_IS_BOOL(notResult) && MS_AS_BOOL(notResult), "xyz not in hello");
  msVMShutdown();
}

static void testHasPrefixSuffix(void) {
  msVMInit();
  MsValue v = msNewStr("hello", 5);
  MS_ASSERT_TRUE(MS_AS_BOOL(msStrHasPrefix(v, msNewStr("he", 2))), "hasPrefix(he)");
  MS_ASSERT_TRUE(!MS_AS_BOOL(msStrHasPrefix(v, msNewStr("lo", 2))), "!hasPrefix(lo)");
  MS_ASSERT_TRUE(MS_AS_BOOL(msStrHasSuffix(v, msNewStr("lo", 2))), "hasSuffix(lo)");
  MS_ASSERT_TRUE(!MS_AS_BOOL(msStrHasSuffix(v, msNewStr("he", 2))), "!hasSuffix(he)");
  msVMShutdown();
}

static void testIndexMethod(void) {
  msVMInit();
  MsValue v = msNewStr("hello", 5);
  MsValue idx = msStrIndex(v, msNewStr("llo", 3), 0, -1);
  MS_ASSERT_TRUE(MS_AS_INT(idx) == 2, "index(llo) == 2");
  MsValue notFound = msStrIndex(v, msNewStr("xyz", 3), 0, -1);
  MS_ASSERT_TRUE(MS_AS_INT(notFound) == -1, "index(xyz) == -1 (not found)");
  msVMShutdown();
}

static void testReplace(void) {
  msVMInit();
  MsValue v = msNewStr("a,b,c", 5);
  MsValue r = msStrReplace(v, msNewStr(",", 1), msNewStr("-", 1), -1);
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(r);
  MS_ASSERT_TRUE(s->len == 5 && memcmp(s->data, "a-b-c", 5) == 0, "replace all");

  MsValue r1 = msStrReplace(v, msNewStr(",", 1), msNewStr("-", 1), 1);
  struct MsStrObj* s1 = (struct MsStrObj*) MS_AS_OBJ(r1);
  MS_ASSERT_TRUE(s1->len == 5 && memcmp(s1->data, "a-b,c", 5) == 0, "replace count=1");
  msVMShutdown();
}

static void testFromInt(void) {
  msVMInit();
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(msStrFromInt(-42));
  MS_ASSERT_TRUE(s->len == 3 && memcmp(s->data, "-42", 3) == 0, "msStrFromInt(-42)");
  struct MsStrObj* z = (struct MsStrObj*) MS_AS_OBJ(msStrFromInt(0));
  MS_ASSERT_TRUE(z->len == 1 && memcmp(z->data, "0", 1) == 0, "msStrFromInt(0)");
  msVMShutdown();
}

int main(void) {
  MS_RUN(testConcat);
  MS_RUN(testRepeat);
  MS_RUN(testRepeatNonPositive);
  MS_RUN(testIndexPositive);
  MS_RUN(testIndexNegative);
  MS_RUN(testIndexOutOfRange);
  MS_RUN(testLenBytes);
  MS_RUN(testCodepointCount);
  MS_RUN(testLessThan);
  MS_RUN(testEqualityInterned);
  MS_RUN(testNoInternAcrossNoIntern);
  MS_RUN(testHashEqual);
  MS_RUN(testUpperLower);
  MS_RUN(testStrip);
  MS_RUN(testContainsOperator);
  MS_RUN(testHasPrefixSuffix);
  MS_RUN(testIndexMethod);
  MS_RUN(testReplace);
  MS_RUN(testFromInt);
  return msTestSummary();
}
