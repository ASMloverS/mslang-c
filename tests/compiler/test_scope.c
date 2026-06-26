#include <stdio.h>

#include "compiler/ms_scope.h"
#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_opcode.h"

static void testLocalResolve(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  MsCompiler c;
  msCompilerInit(&c, NULL, &ck, false);

  int slot0 = msScopeDeclareLocal(&c, "x", 1);
  msScopeMarkInitialized(&c);
  int slot1 = msScopeDeclareLocal(&c, "y", 1);
  msScopeMarkInitialized(&c);

  MS_ASSERT_EQ(slot0, 0, "x -> slot 0");
  MS_ASSERT_EQ(slot1, 1, "y -> slot 1");
  MS_ASSERT_EQ(msScopeResolveLocal(&c, "x", 1), 0, "resolve x == 0");
  MS_ASSERT_EQ(msScopeResolveLocal(&c, "z", 1), -1, "resolve z == -1");

  msCompilerFree(&c);
  msChunkFree(&ck);
}

static void testScopeBeginEnd(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  MsCompiler c;
  msCompilerInit(&c, NULL, &ck, false);

  msScopeBegin(&c);
  msScopeDeclareLocal(&c, "y", 1);
  msScopeMarkInitialized(&c);
  msScopeEnd(&c);

  MS_ASSERT_EQ(ck.codeLen, 1, "1 byte emitted");
  MS_ASSERT_EQ(ck.code[0], OP_POP, "OP_POP emitted");

  msCompilerFree(&c);
  msChunkFree(&ck);
}

static void testCapturedUpvalue(void) {
  struct MsChunk outerChunk;
  msChunkInit(&outerChunk, NULL);
  MsCompiler outer;
  msCompilerInit(&outer, NULL, &outerChunk, true);

  msScopeDeclareLocal(&outer, "x", 1);
  msScopeMarkInitialized(&outer);

  struct MsChunk innerChunk;
  msChunkInit(&innerChunk, NULL);
  MsCompiler inner;
  msCompilerInit(&inner, &outer, &innerChunk, true);

  int uv = msScopeResolveUpvalue(&inner, "x", 1);
  MS_ASSERT_EQ(uv, 0, "upvalue index 0");
  MS_ASSERT_EQ(inner.upvalueCount, 1, "inner has 1 upvalue");
  MS_ASSERT_TRUE(inner.upvalues[0].isLocal, "upvalue isLocal");
  MS_ASSERT_EQ(inner.upvalues[0].index, 0, "upvalue index refs slot 0");
  MS_ASSERT_TRUE(outer.locals[0].captured, "outer local marked captured");

  msCompilerFree(&inner);
  msChunkFree(&innerChunk);
  msCompilerFree(&outer);
  msChunkFree(&outerChunk);
}

static void testUpvalueOfUpvalue(void) {
  struct MsChunk outerChunk;
  msChunkInit(&outerChunk, NULL);
  MsCompiler outer;
  msCompilerInit(&outer, NULL, &outerChunk, true);

  msScopeDeclareLocal(&outer, "x", 1);
  msScopeMarkInitialized(&outer);

  struct MsChunk middleChunk;
  msChunkInit(&middleChunk, NULL);
  MsCompiler middle;
  msCompilerInit(&middle, &outer, &middleChunk, true);

  struct MsChunk innerChunk;
  msChunkInit(&innerChunk, NULL);
  MsCompiler inner;
  msCompilerInit(&inner, &middle, &innerChunk, true);

  int uv = msScopeResolveUpvalue(&inner, "x", 1);
  MS_ASSERT_EQ(uv, 0, "inner upvalue index 0");
  MS_ASSERT_TRUE(outer.locals[0].captured, "outer local captured");
  MS_ASSERT_EQ(middle.upvalueCount, 1, "middle has 1 upvalue");
  MS_ASSERT_TRUE(middle.upvalues[0].isLocal, "middle upvalue isLocal");
  MS_ASSERT_EQ(inner.upvalueCount, 1, "inner has 1 upvalue");
  MS_ASSERT_FALSE(inner.upvalues[0].isLocal, "inner upvalue NOT isLocal");

  msCompilerFree(&inner);
  msChunkFree(&innerChunk);
  msCompilerFree(&middle);
  msChunkFree(&middleChunk);
  msCompilerFree(&outer);
  msChunkFree(&outerChunk);
}

static void testScopeEndCloseUpvalue(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  MsCompiler c;
  msCompilerInit(&c, NULL, &ck, false);

  msScopeBegin(&c);
  msScopeDeclareLocal(&c, "x", 1);
  msScopeMarkInitialized(&c);
  c.locals[0].captured = true;
  msScopeEnd(&c);

  MS_ASSERT_EQ(ck.codeLen, 1, "1 byte emitted");
  MS_ASSERT_EQ(ck.code[0], OP_CLOSE_UPVALUE, "OP_CLOSE_UPVALUE emitted");

  msCompilerFree(&c);
  msChunkFree(&ck);
}

static void testLocalOverflow(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  MsCompiler c;
  msCompilerInit(&c, NULL, &ck, false);

  char names[256][8];
  for (int i = 0; i < 256; i++) {
    snprintf(names[i], sizeof(names[i]), "v%d", i);
    int slot = msScopeDeclareLocal(&c, names[i], (uint32_t) strlen(names[i]));
    msScopeMarkInitialized(&c);
    MS_ASSERT_EQ(slot, i, "slot matches i");
  }

  // 257th should fail
  int overflow = msScopeDeclareLocal(&c, "overflow", 8);
  MS_ASSERT_EQ(overflow, -1, "257th local returns -1");

  msCompilerFree(&c);
  msChunkFree(&ck);
}

static void testDuplicateLocal(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  MsCompiler c;
  msCompilerInit(&c, NULL, &ck, false);

  int s0 = msScopeDeclareLocal(&c, "x", 1);
  msScopeMarkInitialized(&c);
  MS_ASSERT_EQ(s0, 0, "first x -> slot 0");

  int s1 = msScopeDeclareLocal(&c, "x", 1);
  MS_ASSERT_EQ(s1, -1, "duplicate x returns -1");

  msCompilerFree(&c);
  msChunkFree(&ck);
}

static void testUninitializedSentinel(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  MsCompiler c;
  msCompilerInit(&c, NULL, &ck, false);

  msScopeDeclareLocal(&c, "x", 1);
  // x is declared but not initialized (depth == -1)
  int r = msScopeResolveLocal(&c, "x", 1);
  MS_ASSERT_EQ(r, -1, "uninitialized local returns -1");

  msScopeMarkInitialized(&c);
  r = msScopeResolveLocal(&c, "x", 1);
  MS_ASSERT_EQ(r, 0, "after init, resolves to slot 0");

  msCompilerFree(&c);
  msChunkFree(&ck);
}

static void testUpvalueOverflow(void) {
  struct MsChunk outerChunk;
  msChunkInit(&outerChunk, NULL);
  MsCompiler outer;
  msCompilerInit(&outer, NULL, &outerChunk, true);

  struct MsChunk innerChunk;
  msChunkInit(&innerChunk, NULL);
  MsCompiler inner;
  msCompilerInit(&inner, &outer, &innerChunk, true);

  // fill 256 upvalues with unique (index, isLocal) pairs
  for (int i = 0; i < 256; i++) {
    int uv = msScopeAddUpvalue(&inner, (uint8_t) i, true);
    MS_ASSERT_EQ(uv, i, "upvalue index matches i");
  }

  // 257th should fail
  int overflow = msScopeAddUpvalue(&inner, 0, false);
  MS_ASSERT_EQ(overflow, -1, "257th upvalue returns -1");

  msCompilerFree(&inner);
  msChunkFree(&innerChunk);
  msCompilerFree(&outer);
  msChunkFree(&outerChunk);
}

int main(void) {
  MS_RUN(testLocalResolve);
  MS_RUN(testScopeBeginEnd);
  MS_RUN(testCapturedUpvalue);
  MS_RUN(testUpvalueOfUpvalue);
  MS_RUN(testScopeEndCloseUpvalue);
  MS_RUN(testLocalOverflow);
  MS_RUN(testDuplicateLocal);
  MS_RUN(testUninitializedSentinel);
  MS_RUN(testUpvalueOverflow);
  return msTestSummary();
}
