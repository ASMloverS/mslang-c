// test_control_flow.c
// T042: control flow compilation tests.
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_alloc.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

static struct MsChunk* compileStr(const char* src) {
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no compile error");
  return r.chunk;
}

static void compileStrExpectError(const char* src) {
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MS_ASSERT_TRUE(r.hadError, "expected compile error");
  if (r.chunk) {
    msChunkFree(r.chunk);
    msFree(r.chunk);
  }
}

static bool chunkHasOp(struct MsChunk* ck, MsOpCode op) {
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == op) {
      return true;
    }
  }
  return false;
}

// T042: if x { 1 } -> OP_JUMP_IF_FALSE
static void testIfBasic(void) {
  struct MsChunk* ck = compileStr("if x { 1 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP_IF_FALSE), "JUMP_IF_FALSE");
  msChunkFree(ck);
  msFree(ck);
}

// T042: if x { 1 } else { 2 } -> OP_JUMP_IF_FALSE + OP_JUMP
static void testIfElse(void) {
  struct MsChunk* ck = compileStr("if x { 1 } else { 2 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP_IF_FALSE), "JUMP_IF_FALSE");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP), "JUMP");
  msChunkFree(ck);
  msFree(ck);
}

// T042: if x { 1 } else if y { 2 } -> two JUMP_IF_FALSE
static void testIfElseIf(void) {
  struct MsChunk* ck = compileStr("if x { 1 } else if y { 2 }");
  // count JUMP_IF_FALSE occurrences
  int count = 0;
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == OP_JUMP_IF_FALSE) {
      count++;
    }
  }
  MS_ASSERT_EQ(count, 2, "two JUMP_IF_FALSE");
  msChunkFree(ck);
  msFree(ck);
}

// T042: for { 1 } -> OP_JUMP (backward)
static void testInfiniteLoop(void) {
  struct MsChunk* ck = compileStr("for { 1 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP), "JUMP");
  msChunkFree(ck);
  msFree(ck);
}

// T042: for x > 0 { 1 } -> OP_JUMP_IF_FALSE + OP_JUMP
static void testWhileLoop(void) {
  struct MsChunk* ck = compileStr("for x > 0 { 1 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP_IF_FALSE), "JUMP_IF_FALSE");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP), "JUMP");
  msChunkFree(ck);
  msFree(ck);
}

// T042: for i in lst { 1 } -> GET_ITER + FOR_ITER + JUMP
static void testForIn(void) {
  struct MsChunk* ck = compileStr("for i in lst { 1 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_GET_ITER), "GET_ITER");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_FOR_ITER), "FOR_ITER");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP), "JUMP");
  msChunkFree(ck);
  msFree(ck);
}

// T042: for { break } -> OP_JUMP (break forward + loop backward)
static void testBreakInLoop(void) {
  struct MsChunk* ck = compileStr("for { break }");
  // should have at least two JUMP instructions (break + backjump)
  int count = 0;
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == OP_JUMP) {
      count++;
    }
  }
  MS_ASSERT_TRUE(count >= 2, "at least 2 JUMPs");
  msChunkFree(ck);
  msFree(ck);
}

// T042: for { continue } -> OP_JUMP (continue + backjump)
static void testContinueInLoop(void) {
  struct MsChunk* ck = compileStr("for { continue }");
  int count = 0;
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == OP_JUMP) {
      count++;
    }
  }
  MS_ASSERT_TRUE(count >= 2, "at least 2 JUMPs");
  msChunkFree(ck);
  msFree(ck);
}

// T042: break outside loop -> compile error
static void testBreakOutsideLoop(void) {
  compileStrExpectError("break");
}

// T042: continue outside loop -> compile error
static void testContinueOutsideLoop(void) {
  compileStrExpectError("continue");
}

// T042: switch x { case 1: 1 } -> DUP + EQ + JUMP_IF_TRUE
static void testSwitchBasic(void) {
  struct MsChunk* ck = compileStr("switch x { case 1: 1 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_DUP), "DUP");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_EQ), "EQ");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP_IF_TRUE), "JUMP_IF_TRUE");
  msChunkFree(ck);
  msFree(ck);
}

// T042: switch x { case 1: 1\ndefault: 2 } -> compiles ok
static void testSwitchDefault(void) {
  struct MsChunk* ck = compileStr("switch x { case 1: 1\ndefault: 2 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_DUP), "DUP");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_EQ), "EQ");
  msChunkFree(ck);
  msFree(ck);
}

// T042: switch { case x > 0: 1 } -> JUMP_IF_TRUE, no EQ (no switch expr)
static void testSwitchNoExpr(void) {
  struct MsChunk* ck = compileStr("switch { case x > 0: 1 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP_IF_TRUE), "JUMP_IF_TRUE");
  MS_ASSERT_FALSE(chunkHasOp(ck, OP_EQ), "no EQ");
  msChunkFree(ck);
  msFree(ck);
}

// T042: pass -> no-op (compiles ok)
static void testPass(void) {
  struct MsChunk* ck = compileStr("pass");
  // pass should produce no ops other than RETURN_NIL
  MS_ASSERT_TRUE(ck != NULL, "compiled ok");
  msChunkFree(ck);
  msFree(ck);
}

int main(void) {
  MS_RUN(testIfBasic);
  MS_RUN(testIfElse);
  MS_RUN(testIfElseIf);
  MS_RUN(testInfiniteLoop);
  MS_RUN(testWhileLoop);
  MS_RUN(testForIn);
  MS_RUN(testBreakInLoop);
  MS_RUN(testContinueInLoop);
  MS_RUN(testBreakOutsideLoop);
  MS_RUN(testContinueOutsideLoop);
  MS_RUN(testSwitchBasic);
  MS_RUN(testSwitchDefault);
  MS_RUN(testSwitchNoExpr);
  MS_RUN(testPass);
  return msTestSummary();
}
