#include <string.h>

#include "ms_test.h"
#include "mslang/ms_alloc.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"
#include "mslang/ms_value.h"

// helper: compile source, assert no error, return chunk (caller frees)
static struct MsChunk* compileStr(const char* src) {
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no compile error");
  return r.chunk;
}

static bool chunkHasOp(struct MsChunk* ck, MsOpCode op) {
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == op) {
      return true;
    }
  }
  return false;
}

// T039_int_literal
static void testIntLiteral(void) {
  struct MsChunk* ck = compileStr("42");
  // expect: OP_CONST <3-byte AX> OP_POP OP_RETURN_NIL
  MS_ASSERT_EQ(ck->code[0], OP_CONST, "CONST");
  MS_ASSERT_EQ(ck->constLen, 1, "1 const");
  MS_ASSERT_EQ(ck->constants[0].tag, MS_TAG_INT, "int tag");
  MS_ASSERT_EQ(ck->constants[0].as.i, 42, "val 42");
  msChunkFree(ck);
  msFree(ck);
}

// T039_addition
static void testAddition(void) {
  struct MsChunk* ck = compileStr("1 + 2");
  // expect: OP_CONST(1), OP_CONST(2), OP_ADD, OP_POP, OP_RETURN_NIL
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_ADD), "has OP_ADD");
  MS_ASSERT_EQ(ck->constLen, 2, "2 consts");
  msChunkFree(ck);
  msFree(ck);
}

// T039_and_shortcircuit
static void testAndShortcircuit(void) {
  struct MsChunk* ck = compileStr("1 and 2");
  // expect: OP_CONST(1), OP_AND_JMP <3-byte>, OP_CONST(2), OP_POP, OP_RETURN_NIL
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_AND_JMP), "has OP_AND_JMP");
  msChunkFree(ck);
  msFree(ck);
}

// T039_or_shortcircuit
static void testOrShortcircuit(void) {
  struct MsChunk* ck = compileStr("1 or 2");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_OR_JMP), "has OP_OR_JMP");
  msChunkFree(ck);
  msFree(ck);
}

// T039_unary_not
static void testUnaryNot(void) {
  struct MsChunk* ck = compileStr("not true");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_NOT), "has OP_NOT");
  msChunkFree(ck);
  msFree(ck);
}

// T039_nil
static void testNil(void) {
  struct MsChunk* ck = compileStr("nil");
  MS_ASSERT_EQ(ck->code[0], OP_CONST_NIL, "CONST_NIL");
  msChunkFree(ck);
  msFree(ck);
}

// T039_bool_true
static void testBoolTrue(void) {
  struct MsChunk* ck = compileStr("true");
  MS_ASSERT_EQ(ck->code[0], OP_CONST_TRUE, "CONST_TRUE");
  msChunkFree(ck);
  msFree(ck);
}

int main(void) {
  MS_RUN(testIntLiteral);
  MS_RUN(testAddition);
  MS_RUN(testAndShortcircuit);
  MS_RUN(testOrShortcircuit);
  MS_RUN(testUnaryNot);
  MS_RUN(testNil);
  MS_RUN(testBoolTrue);
  return msTestSummary();
}
