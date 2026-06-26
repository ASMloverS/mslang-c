// test_variables.c
// T040: variable load/store compilation tests.
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

static struct MsChunk* compileStrExpectError(const char* src) {
  MsCompileResult r = msCompile(src, (uint32_t) strlen(src), "<t>");
  MS_ASSERT_TRUE(r.hadError, "expected compile error");
  if (r.chunk) {
    msChunkFree(r.chunk);
    msFree(r.chunk);
  }
  return NULL;
}

static bool chunkHasOp(struct MsChunk* ck, MsOpCode op) {
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == op) {
      return true;
    }
  }
  return false;
}

// T040: global ident read -> OP_GET_GLOBAL
static void testGlobalIdentRead(void) {
  struct MsChunk* ck = compileStr("x");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_GET_GLOBAL), "has GET_GLOBAL");
  msChunkFree(ck);
  msFree(ck);
}

// T040: global assign -> OP_SET_GLOBAL
static void testGlobalAssign(void) {
  struct MsChunk* ck = compileStr("x = 42");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_SET_GLOBAL), "has SET_GLOBAL");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_POP), "has POP");
  msChunkFree(ck);
  msFree(ck);
}

// T040: var decl at top-level -> OP_SET_GLOBAL + OP_POP
static void testVarDeclGlobal(void) {
  struct MsChunk* ck = compileStr("var x = 42");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_SET_GLOBAL), "has SET_GLOBAL");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_POP), "has POP");
  msChunkFree(ck);
  msFree(ck);
}

// T040: var decl without init -> OP_CONST_NIL + OP_SET_GLOBAL
static void testVarDeclNoInit(void) {
  struct MsChunk* ck = compileStr("var x");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_CONST_NIL), "has CONST_NIL");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_SET_GLOBAL), "has SET_GLOBAL");
  msChunkFree(ck);
  msFree(ck);
}

// T040: short decl at top-level -> OP_SET_GLOBAL
static void testShortDeclGlobal(void) {
  struct MsChunk* ck = compileStr("x := 42");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_SET_GLOBAL), "has SET_GLOBAL");
  msChunkFree(ck);
  msFree(ck);
}

// T040: del ident -> OP_DEL_GLOBAL
static void testDelGlobal(void) {
  struct MsChunk* ck = compileStr("del x");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_DEL_GLOBAL), "has DEL_GLOBAL");
  msChunkFree(ck);
  msFree(ck);
}

// T040: compound assign x += 5 -> GET_GLOBAL, CONST, ADD, SET_GLOBAL, POP
static void testCompoundAssignGlobal(void) {
  struct MsChunk* ck = compileStr("x += 5");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_GET_GLOBAL), "has GET_GLOBAL");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_ADD), "has ADD");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_SET_GLOBAL), "has SET_GLOBAL");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_POP), "has POP");
  msChunkFree(ck);
  msFree(ck);
}

// T040-fix1: compound assign on attr target -> compile error
static void testCompoundAssignAttrError(void) {
  compileStrExpectError("x.y += 1");
}

// T040-fix5: tuple unpack assignment -> compile error
static void testTupleUnpackAssignError(void) {
  compileStrExpectError("(a, b) = (1, 2)");
}

int main(void) {
  MS_RUN(testGlobalIdentRead);
  MS_RUN(testGlobalAssign);
  MS_RUN(testVarDeclGlobal);
  MS_RUN(testVarDeclNoInit);
  MS_RUN(testShortDeclGlobal);
  MS_RUN(testDelGlobal);
  MS_RUN(testCompoundAssignGlobal);
  MS_RUN(testCompoundAssignAttrError);
  MS_RUN(testTupleUnpackAssignError);
  return msTestSummary();
}
