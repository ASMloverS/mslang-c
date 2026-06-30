// test_exception_compile.c
// T046: exception compilation tests.
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

static bool chunkHasOp(struct MsChunk* ck, MsOpCode op) {
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == op) {
      return true;
    }
  }
  return false;
}

static bool chunkHasOpA(struct MsChunk* ck, MsOpCode op, uint8_t a) {
  for (uint32_t i = 0; i + 1 < ck->codeLen; i++) {
    if (ck->code[i] == op && ck->code[i + 1] == a) {
      return true;
    }
  }
  return false;
}

// T046: try/catch compiles with OP_PUSH_EXCEPT and OP_POP_EXCEPT
static void testTryCatch(void) {
  struct MsChunk* ck = compileStr("try { pass } catch (e: Exception) { pass }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_PUSH_EXCEPT), "has PUSH_EXCEPT");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_POP_EXCEPT), "has POP_EXCEPT");
  msChunkFree(ck);
  msFree(ck);
}

// T046: try/catch has OP_ISINSTANCE for type matching
static void testTryCatchIsinstance(void) {
  struct MsChunk* ck = compileStr("try { pass } catch (e: Exception) { pass }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_ISINSTANCE), "has ISINSTANCE");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_LOAD_EXCEPTION), "has LOAD_EXCEPTION");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_CLEAR_EXCEPTION), "has CLEAR_EXCEPTION");
  msChunkFree(ck);
  msFree(ck);
}

// T046: try/finally inlines finally block
static void testTryFinally(void) {
  struct MsChunk* ck = compileStr("try { pass } finally { pass }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_PUSH_EXCEPT), "has PUSH_EXCEPT");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_RERAISE), "has RERAISE in finally path");
  msChunkFree(ck);
  msFree(ck);
}

// T046: raise expr compiles to OP_RAISE
static void testRaise(void) {
  struct MsChunk* ck = compileStr("raise err");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_RAISE), "has RAISE");
  msChunkFree(ck);
  msFree(ck);
}

// T046: bare raise compiles to OP_RERAISE
static void testBareRaise(void) {
  struct MsChunk* ck = compileStr("raise");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_RERAISE), "has RERAISE");
  msChunkFree(ck);
  msFree(ck);
}

// T046: assert compiles to OP_JUMP_IF_TRUE + OP_RAISE_ASSERT
static void testAssert(void) {
  struct MsChunk* ck = compileStr("assert x");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP_IF_TRUE), "has JUMP_IF_TRUE");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_RAISE_ASSERT), "has RAISE_ASSERT");
  msChunkFree(ck);
  msFree(ck);
}

// T046: assert with message pushes message before OP_RAISE_ASSERT
static void testAssertMsg(void) {
  struct MsChunk* ck = compileStr("assert x, msg");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_JUMP_IF_TRUE), "has JUMP_IF_TRUE");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_RAISE_ASSERT), "has RAISE_ASSERT");
  // without msg, OP_CONST_NIL is emitted; with msg, no extra NIL needed
  msChunkFree(ck);
  msFree(ck);
}

// T046: multi-type catch uses multiple OP_ISINSTANCE checks
static void testMultiTypeCatch(void) {
  struct MsChunk* ck = compileStr("try { pass } catch (e: TypeError, ValueError) { pass }");
  // Should have at least 2 ISINSTANCE checks and a JUMP_IF_TRUE for OR
  int count = 0;
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == OP_ISINSTANCE) {
      count++;
    }
  }
  MS_ASSERT_TRUE(count >= 2, "has >= 2 ISINSTANCE for multi-type");
  msChunkFree(ck);
  msFree(ck);
}

// T046-fix: raise expr emits OP_RAISE with A=1
static void testRaiseOperand(void) {
  struct MsChunk* ck = compileStr("raise err");
  MS_ASSERT_TRUE(chunkHasOpA(ck, OP_RAISE, 1), "RAISE has A=1");
  msChunkFree(ck);
  msFree(ck);
}

// T046-fix: assert without message emits OP_RAISE_ASSERT with A=0
static void testAssertNoMsgOperand(void) {
  struct MsChunk* ck = compileStr("assert x");
  MS_ASSERT_TRUE(chunkHasOpA(ck, OP_RAISE_ASSERT, 0), "RAISE_ASSERT has A=0");
  // should NOT have OP_CONST_NIL before OP_RAISE_ASSERT
  MS_ASSERT_TRUE(!chunkHasOp(ck, OP_CONST_NIL), "no CONST_NIL for bare assert");
  msChunkFree(ck);
  msFree(ck);
}

// T046-fix: assert with message emits OP_RAISE_ASSERT with A=1
static void testAssertMsgOperand(void) {
  struct MsChunk* ck = compileStr("assert x, msg");
  MS_ASSERT_TRUE(chunkHasOpA(ck, OP_RAISE_ASSERT, 1), "RAISE_ASSERT has A=1");
  msChunkFree(ck);
  msFree(ck);
}

// T046-fix: same bind name in multiple catch clauses should not conflict
static void testMultiCatchSameName(void) {
  struct MsChunk* ck = compileStr(
      "try { pass } "
      "catch (e: TypeError) { pass } "
      "catch (e: ValueError) { pass }");
  MS_ASSERT_TRUE(ck != NULL, "compiles without error");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_PUSH_EXCEPT), "has PUSH_EXCEPT");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_POP_EXCEPT), "has POP_EXCEPT");
  msChunkFree(ck);
  msFree(ck);
}

// T046-fix: catch binding without SET_LOCAL (uses scope-based local)
static void testCatchBindNoSetLocal(void) {
  struct MsChunk* ck = compileStr("try { pass } catch (e: Exception) { pass }");
  // After fix, binding uses scope-based local, no SET_LOCAL needed
  MS_ASSERT_TRUE(!chunkHasOp(ck, OP_SET_LOCAL), "no SET_LOCAL for catch bind");
  MS_ASSERT_TRUE(!chunkHasOp(ck, OP_CONST_NIL), "no CONST_NIL cleanup");
  msChunkFree(ck);
  msFree(ck);
}

// T046-fix: catch-all with binding `catch (e)` binds exception locally
static void testCatchAllWithBinding(void) {
  struct MsChunk* ck = compileStr("try { pass } catch (e) { pass }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_LOAD_EXCEPTION), "has LOAD_EXCEPTION for catch-all binding");
  MS_ASSERT_TRUE(!chunkHasOp(ck, OP_GET_GLOBAL), "no GET_GLOBAL (e is local, not global)");
  msChunkFree(ck);
  msFree(ck);
}

int main(void) {
  MS_RUN(testTryCatch);
  MS_RUN(testTryCatchIsinstance);
  MS_RUN(testTryFinally);
  MS_RUN(testRaise);
  MS_RUN(testBareRaise);
  MS_RUN(testAssert);
  MS_RUN(testAssertMsg);
  MS_RUN(testMultiTypeCatch);
  MS_RUN(testRaiseOperand);
  MS_RUN(testAssertNoMsgOperand);
  MS_RUN(testAssertMsgOperand);
  MS_RUN(testMultiCatchSameName);
  MS_RUN(testCatchBindNoSetLocal);
  MS_RUN(testCatchAllWithBinding);
  return msTestSummary();
}
