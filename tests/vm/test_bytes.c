#include <stdint.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_bytes.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

// msCompile()+msVMRun() cannot reach a top-level expression's value yet
// (see test_str.c/test_eval_basic.c for the same, already-documented
// workaround), so chunks are built by hand instead, exactly like every
// other T053-T057 VM test.

// OP_CONST a; OP_CONST b; op; OP_RETURN. Covers add/eq/lt/get_item (all pop
// b then a and combine them); does not init/shutdown the VM (the caller
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

static void testBytesIndexPositive(void) {
  msVMInit();
  MsValue result = runBinOp(msNewBytes((const uint8_t*) "hello", 5), MS_INT_VAL(0), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_INT(result) && MS_AS_INT(result) == 104, "b[0] == 'h' == 104");
  msVMShutdown();
}

static void testBytesIndexNegative(void) {
  msVMInit();
  MsValue result = runBinOp(msNewBytes((const uint8_t*) "hello", 5), MS_INT_VAL(-1), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_INT(result) && MS_AS_INT(result) == 111, "b[-1] == 'o' == 111");
  msVMShutdown();
}

static void testBytesIndexOutOfRange(void) {
  msVMInit();
  MsValue result = runBinOp(msNewBytes((const uint8_t*) "hi", 2), MS_INT_VAL(5), OP_GET_ITEM);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "out-of-range index is an error");
  msVMShutdown();
}

static void testBytesConcat(void) {
  msVMInit();
  MsValue result = runBinOp(msNewBytes((const uint8_t*) "ab", 2), msNewBytes((const uint8_t*) "cd", 2), OP_ADD);
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(result);
  MS_ASSERT_TRUE(b->len == 4 && b->data[2] == 'c', "concat ok");
  msVMShutdown();
}

// Overwrites ->len directly (no multi-GB alloc needed) to exercise the
// (uint64_t) overflow guard in bytesAdd; the guard fires before any
// data is touched, so the small real allocations behind a/b stay untouched.
static void testBytesConcatOverflow(void) {
  msVMInit();
  MsValue a = msNewBytes((const uint8_t*) "x", 1);
  MsValue b = msNewBytes((const uint8_t*) "y", 1);
  ((struct MsBytes*) MS_AS_OBJ(a))->len = UINT32_MAX;
  MsValue result = runBinOp(a, b, OP_ADD);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "len overflow on concat is an error");
  msVMShutdown();
}

static void testBytesSetItemMutability(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "hello", 5);
  MsValue result = runSetItem(v, MS_INT_VAL(0), MS_INT_VAL(72));  // 'H'
  MS_ASSERT_TRUE(!MS_IS_ERROR(result), "set_item did not error");
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(b->data[0] == 'H', "b[0] = 72 mutates in place");
  msVMShutdown();
}

static void testBytesSetItemOutOfRangeIndex(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "hi", 2);
  MsValue result = runSetItem(v, MS_INT_VAL(5), MS_INT_VAL(65));
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "out-of-range set index is an error");
  msVMShutdown();
}

static void testBytesSetItemValueOutOfRange(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "hi", 2);
  MsValue result = runSetItem(v, MS_INT_VAL(0), MS_INT_VAL(256));
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "byte value > 255 is an error");
  msVMShutdown();
}

static void testBytesEq(void) {
  msVMInit();
  MsValue eq = runBinOp(msNewBytes((const uint8_t*) "hello", 5), msNewBytes((const uint8_t*) "hello", 5), OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(eq) && MS_AS_BOOL(eq), "hello == hello");
  MsValue ne = runBinOp(msNewBytes((const uint8_t*) "hello", 5), msNewBytes((const uint8_t*) "world", 5), OP_EQ);
  MS_ASSERT_TRUE(MS_IS_BOOL(ne) && !MS_AS_BOOL(ne), "hello != world");
  msVMShutdown();
}

static void testBytesLt(void) {
  msVMInit();
  MsValue result = runBinOp(msNewBytes((const uint8_t*) "abc", 3), msNewBytes((const uint8_t*) "abd", 3), OP_LT);
  MS_ASSERT_TRUE(MS_IS_BOOL(result) && MS_AS_BOOL(result), "abc < abd");
  msVMShutdown();
}

static void testBytesLtWrongType(void) {
  msVMInit();
  MsValue result = runBinOp(msNewBytes((const uint8_t*) "abc", 3), MS_INT_VAL(1), OP_LT);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "bytes < int is an error");
  msVMShutdown();
}

static void testBytesLen(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "hello", 5);
  struct MsType* t = MS_AS_OBJ(v)->type;
  MS_ASSERT_TRUE(MS_AS_INT(t->tpLen(NULL, v)) == 5, "len(b\"hello\") == 5");
  msVMShutdown();
}

static void testBytesDecode(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "hello", 5);
  MsValue s = msBytesDecode(v);
  struct MsStrObj* so = (struct MsStrObj*) MS_AS_OBJ(s);
  MS_ASSERT_TRUE(so->len == 5 && memcmp(so->data, "hello", 5) == 0, "decode() == \"hello\"");
  msVMShutdown();
}

static void testBytesHex(void) {
  msVMInit();
  uint8_t raw[2] = {0x00, 0xFF};
  MsValue v = msNewBytes(raw, 2);
  MsValue s = msBytesHex(v);
  struct MsStrObj* so = (struct MsStrObj*) MS_AS_OBJ(s);
  MS_ASSERT_TRUE(so->len == 4 && memcmp(so->data, "00ff", 4) == 0, "hex() == \"00ff\"");
  msVMShutdown();
}

static void testBytesFromHex(void) {
  msVMInit();
  MsValue r = msBytesFromHex(msNewStr("deadbeef", 8));
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(r);
  uint8_t expected[4] = {0xde, 0xad, 0xbe, 0xef};
  MS_ASSERT_TRUE(b->len == 4 && memcmp(b->data, expected, 4) == 0, "fromHex(\"deadbeef\")");

  MsValue oddLen = msBytesFromHex(msNewStr("abc", 3));
  MS_ASSERT_TRUE(MS_IS_ERROR(oddLen), "odd-length hex string is an error");

  MsValue badChar = msBytesFromHex(msNewStr("zz", 2));
  MS_ASSERT_TRUE(MS_IS_ERROR(badChar), "non-hex character is an error");
  msVMShutdown();
}

static void testBytesAppend(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "ab", 2);
  MsValue result = msBytesAppend(v, MS_INT_VAL('c'));
  MS_ASSERT_TRUE(!MS_IS_ERROR(result), "append did not error");
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(b->len == 3 && memcmp(b->data, "abc", 3) == 0, "append('c') -> \"abc\"");

  MsValue bad = msBytesAppend(v, MS_INT_VAL(256));
  MS_ASSERT_TRUE(MS_IS_ERROR(bad), "append(256) is an error");
  msVMShutdown();
}

// Overwrites ->len to UINT32_MAX (no multi-GB alloc needed) to exercise the
// overflow guard in msBytesAppend; it fires before bytesEnsureCap/realloc run.
static void testBytesAppendOverflow(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "x", 1);
  ((struct MsBytes*) MS_AS_OBJ(v))->len = UINT32_MAX;
  MsValue result = msBytesAppend(v, MS_INT_VAL('a'));
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "append at UINT32_MAX len is an error");
  msVMShutdown();
}

static void testBytesExtend(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "ab", 2);
  MsValue result = msBytesExtend(v, msNewBytes((const uint8_t*) "cd", 2));
  MS_ASSERT_TRUE(!MS_IS_ERROR(result), "extend did not error");
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  MS_ASSERT_TRUE(b->len == 4 && memcmp(b->data, "abcd", 4) == 0, "extend(b\"cd\") -> \"abcd\"");
  msVMShutdown();
}

// Same technique as testBytesAppendOverflow, exercising msBytesExtend's
// (uint64_t) length-sum guard instead.
static void testBytesExtendOverflow(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "x", 1);
  MsValue other = msNewBytes((const uint8_t*) "y", 1);
  ((struct MsBytes*) MS_AS_OBJ(v))->len = UINT32_MAX;
  MsValue result = msBytesExtend(v, other);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "extend causing len overflow is an error");
  msVMShutdown();
}

static void testBytesCopy(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "abc", 3);
  MsValue c = msBytesCopy(v);
  MS_ASSERT_TRUE(MS_AS_OBJ(v) != MS_AS_OBJ(c), "copy() returns a distinct object");
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(c);
  MS_ASSERT_TRUE(b->len == 3 && memcmp(b->data, "abc", 3) == 0, "copy() has same content");
  msVMShutdown();
}

static void testBytesFind(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "hello", 5);
  MsValue found = msBytesFind(v, msNewBytes((const uint8_t*) "llo", 3), 0);
  MS_ASSERT_TRUE(MS_AS_INT(found) == 2, "find(llo) == 2");
  MsValue notFound = msBytesFind(v, msNewBytes((const uint8_t*) "xyz", 3), 0);
  MS_ASSERT_TRUE(MS_AS_INT(notFound) == -1, "find(xyz) == -1");
  MsValue withStart = msBytesFind(v, msNewBytes((const uint8_t*) "l", 1), 3);
  MS_ASSERT_TRUE(MS_AS_INT(withStart) == 3, "find(l, start=3) == 3");
  msVMShutdown();
}

static void testBytesStartsEndsWith(void) {
  msVMInit();
  MsValue v = msNewBytes((const uint8_t*) "hello", 5);
  MS_ASSERT_TRUE(MS_AS_BOOL(msBytesStartsWith(v, msNewBytes((const uint8_t*) "he", 2))), "startsWith(he)");
  MS_ASSERT_TRUE(!MS_AS_BOOL(msBytesStartsWith(v, msNewBytes((const uint8_t*) "lo", 2))), "!startsWith(lo)");
  MS_ASSERT_TRUE(MS_AS_BOOL(msBytesEndsWith(v, msNewBytes((const uint8_t*) "lo", 2))), "endsWith(lo)");
  MS_ASSERT_TRUE(!MS_AS_BOOL(msBytesEndsWith(v, msNewBytes((const uint8_t*) "he", 2))), "!endsWith(he)");
  msVMShutdown();
}

int main(void) {
  MS_RUN(testBytesIndexPositive);
  MS_RUN(testBytesIndexNegative);
  MS_RUN(testBytesIndexOutOfRange);
  MS_RUN(testBytesConcat);
  MS_RUN(testBytesConcatOverflow);
  MS_RUN(testBytesSetItemMutability);
  MS_RUN(testBytesSetItemOutOfRangeIndex);
  MS_RUN(testBytesSetItemValueOutOfRange);
  MS_RUN(testBytesEq);
  MS_RUN(testBytesLt);
  MS_RUN(testBytesLtWrongType);
  MS_RUN(testBytesLen);
  MS_RUN(testBytesDecode);
  MS_RUN(testBytesHex);
  MS_RUN(testBytesFromHex);
  MS_RUN(testBytesAppend);
  MS_RUN(testBytesAppendOverflow);
  MS_RUN(testBytesExtend);
  MS_RUN(testBytesExtendOverflow);
  MS_RUN(testBytesCopy);
  MS_RUN(testBytesFind);
  MS_RUN(testBytesStartsEndsWith);
  return msTestSummary();
}
