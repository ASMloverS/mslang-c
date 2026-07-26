// test_call_compile.c
// T045: call compilation tests.
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

// Find the byte offset of an opcode in the chunk, or -1 if not found.
static int findOp(struct MsChunk* ck, MsOpCode op) {
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == op) {
      return (int) i;
    }
  }
  return -1;
}

// T045: f(1, 2) -> has OP_CALL, operand byte == 2
static void testSimpleCall(void) {
  struct MsChunk* ck = compileStr("f(1, 2)");
  int pos = findOp(ck, OP_CALL);
  MS_ASSERT_TRUE(pos >= 0, "has OP_CALL");
  // OP_CALL is followed by A operand byte (argc)
  MS_ASSERT_EQ(ck->code[pos + 1], 2, "OP_CALL argc == 2");
  msChunkFree(ck);
  msFree(ck);
}

// T045: f() -> has OP_CALL with argc == 0
static void testCallNoArgs(void) {
  struct MsChunk* ck = compileStr("f()");
  int pos = findOp(ck, OP_CALL);
  MS_ASSERT_TRUE(pos >= 0, "has OP_CALL");
  MS_ASSERT_EQ(ck->code[pos + 1], 0, "OP_CALL argc == 0");
  msChunkFree(ck);
  msFree(ck);
}

// T045: f(a=1, b=2) -> BUILD_MAP(2), CALL_KW(0)
static void testCallKw(void) {
  struct MsChunk* ck = compileStr("f(a=1, b=2)");
  int mapPos = findOp(ck, OP_BUILD_MAP);
  MS_ASSERT_TRUE(mapPos >= 0, "has OP_BUILD_MAP");
  MS_ASSERT_EQ(ck->code[mapPos + 1], 2, "BUILD_MAP count == 2");
  int kwPos = findOp(ck, OP_CALL_KW);
  MS_ASSERT_TRUE(kwPos >= 0, "has OP_CALL_KW");
  MS_ASSERT_EQ(ck->code[kwPos + 1], 0, "CALL_KW argc_pos == 0");
  msChunkFree(ck);
  msFree(ck);
}

// T045: f(1, x=2) -> CALL_KW(1) with argc_pos == 1
static void testCallMixed(void) {
  struct MsChunk* ck = compileStr("f(1, x=2)");
  int kwPos = findOp(ck, OP_CALL_KW);
  MS_ASSERT_TRUE(kwPos >= 0, "has OP_CALL_KW");
  MS_ASSERT_EQ(ck->code[kwPos + 1], 1, "CALL_KW argc_pos == 1");
  msChunkFree(ck);
  msFree(ck);
}

// T045: f(*args) -> has OP_CALL_EX
static void testCallStar(void) {
  struct MsChunk* ck = compileStr("f(*args)");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_CALL_EX), "has OP_CALL_EX");
  msChunkFree(ck);
  msFree(ck);
}

// T045: async func with call inside -> outer has OP_MAKE_FUNC
// NOTE: OP_AWAIT checklist item deferred until parser adds await expr support.
static void testAsyncFuncCall(void) {
  struct MsChunk* ck = compileStr("async func g() { f() }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_MAKE_FUNC), "outer has OP_MAKE_FUNC");
  msChunkFree(ck);
  msFree(ck);
}

// T045: obj.method(1) -> has OP_GET_ATTR and OP_CALL
static void testMethodCall(void) {
  struct MsChunk* ck = compileStr("obj.method(1)");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_GET_ATTR), "has OP_GET_ATTR");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_CALL), "has OP_CALL");
  msChunkFree(ck);
  msFree(ck);
}

// T070: f(**opts) -- single `**expr`, no literal kwarg mixed in -- goes
// through compileCallKw (opts pushed directly as the kwargs map, no
// OP_BUILD_MAP), not compileCallEx (impl/P5-T070-kwargs.md "实现要点 0").
static void testCallDoublestarOnly(void) {
  struct MsChunk* ck = compileStr("f(**opts)");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_CALL_KW), "has OP_CALL_KW");
  MS_ASSERT_FALSE(chunkHasOp(ck, OP_BUILD_MAP), "no OP_BUILD_MAP (opts used directly as kwargs map)");
  MS_ASSERT_FALSE(chunkHasOp(ck, OP_CALL_EX), "does not go through OP_CALL_EX");
  int kwPos = findOp(ck, OP_CALL_KW);
  MS_ASSERT_EQ(ck->code[kwPos + 1], 0, "CALL_KW argc_pos == 0");
  msChunkFree(ck);
  msFree(ck);
}

// T070: f(1, **opts) -- single `**expr` plus positional args (no literal
// kwarg pairs) -- still the doublestar-only case, posArgc == 1.
static void testCallDoublestarOnlyWithPositional(void) {
  struct MsChunk* ck = compileStr("f(1, **opts)");
  int kwPos = findOp(ck, OP_CALL_KW);
  MS_ASSERT_TRUE(kwPos >= 0, "has OP_CALL_KW");
  MS_ASSERT_FALSE(chunkHasOp(ck, OP_BUILD_MAP), "no OP_BUILD_MAP");
  MS_ASSERT_EQ(ck->code[kwPos + 1], 1, "CALL_KW argc_pos == 1");
  msChunkFree(ck);
  msFree(ck);
}

// T070 (out of scope): literal kwarg mixed with `**expr` needs merging a
// literal-built map with a runtime map, which needs an opcode this task
// does not add (impl/P5-T070-kwargs.md "风险与边界") -- compileCallEx used to
// silently miscompile this (dropping the literal kwarg); it must now be a
// compile error instead.
static void testCallMixedKwargAndDoublestarIsCompileError(void) {
  compileStrExpectError("f(a=1, **opts)");
}

// T070 (out of scope): `*expr` positional spread mixed with a literal kwarg
// also needs a runtime merge this task's opcode set cannot express;
// compileCallEx used to silently drop the kwarg -- must now be a compile
// error.
static void testCallStarMixedWithKwargIsCompileError(void) {
  compileStrExpectError("f(*lst, b=7)");
}

int main(void) {
  MS_RUN(testSimpleCall);
  MS_RUN(testCallNoArgs);
  MS_RUN(testCallKw);
  MS_RUN(testCallMixed);
  MS_RUN(testCallStar);
  MS_RUN(testAsyncFuncCall);
  MS_RUN(testMethodCall);
  MS_RUN(testCallDoublestarOnly);
  MS_RUN(testCallDoublestarOnlyWithPositional);
  MS_RUN(testCallMixedKwargAndDoublestarIsCompileError);
  MS_RUN(testCallStarMixedWithKwargIsCompileError);
  return msTestSummary();
}
