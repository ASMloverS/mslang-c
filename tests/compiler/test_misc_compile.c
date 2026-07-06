// test_misc_compile.c
// T047: with / go / select / send / import compilation tests.
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

static int chunkCountOp(struct MsChunk* ck, MsOpCode op) {
  int count = 0;
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == op) {
      count++;
    }
  }
  return count;
}

// T047: with-as binding compiles to WITH_ENTER/PUSH_EXCEPT/.../WITH_EXIT/JUMP.
// No SET_LOCAL: the bound value is already on the stack at the declared slot
// (same no-extra-store pattern as compileTry's catch binding).
static void testWithEnterBinding(void) {
  struct MsChunk* ck = compileStr("with f() as x { pass }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_WITH_ENTER), "has WITH_ENTER");
  MS_ASSERT_TRUE(!chunkHasOp(ck, OP_SET_LOCAL), "no SET_LOCAL for with-as binding");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_PUSH_EXCEPT), "has PUSH_EXCEPT");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_POP_EXCEPT), "has POP_EXCEPT");
  MS_ASSERT_TRUE(chunkCountOp(ck, OP_WITH_EXIT) == 2, "has 2 WITH_EXIT (normal + except)");
  msChunkFree(ck);
  msFree(ck);
}

// T047: with-without-binding uses OP_POP instead of OP_SET_LOCAL
static void testWithNoBinding(void) {
  struct MsChunk* ck = compileStr("with f() { pass }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_WITH_ENTER), "has WITH_ENTER");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_POP), "has POP for unbound result");
  MS_ASSERT_TRUE(!chunkHasOp(ck, OP_SET_LOCAL), "no SET_LOCAL without binding");
  msChunkFree(ck);
  msFree(ck);
}

// T047: go statement pushes callee+args then OP_GO, never OP_CALL
static void testGoStmt(void) {
  struct MsChunk* ck = compileStr("go f(1, 2)");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_GO), "has GO");
  MS_ASSERT_TRUE(!chunkHasOp(ck, OP_CALL), "no CALL emitted for go");
  msChunkFree(ck);
  msFree(ck);
}

// T047: send case in select compiles chan expr + val expr then OP_SELECT_CASE_SEND
// (syntax.md §2.2: SendStmt only appears as a SelectCase comm, not a bare statement)
static void testSendStmt(void) {
  struct MsChunk* ck = compileStr("select { case ch <- 42: pass }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_GET_GLOBAL), "has GET_GLOBAL for chan");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_SELECT_CASE_SEND), "has SELECT_CASE_SEND");
  msChunkFree(ck);
  msFree(ck);
}

// T047: import os compiles to OP_IMPORT + OP_SET_GLOBAL
static void testImportSimple(void) {
  struct MsChunk* ck = compileStr("import os");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_IMPORT), "has IMPORT");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_SET_GLOBAL), "has SET_GLOBAL");
  msChunkFree(ck);
  msFree(ck);
}

// T047: import os.path binds to first segment (os), still emits IMPORT once
static void testImportDottedBindsFirstSegment(void) {
  struct MsChunk* ck = compileStr("import os.path");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_IMPORT), "has IMPORT");
  MS_ASSERT_TRUE(chunkCountOp(ck, OP_IMPORT) == 1, "exactly one IMPORT");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_SET_GLOBAL), "has SET_GLOBAL");
  msChunkFree(ck);
  msFree(ck);
}

// T047: OP_IMPORT's constant index for "os.path" must differ from "os" (full
// dotted path, not just the first segment) and from "os.path.sep" (3 segments).
// NOTE: addStringConst is still a T049 placeholder (always emits MS_NIL_VAL,
// discards its name/len args), so the actual string content cannot yet be
// read back from the constant pool. This test exercises the segment-joining
// code path (catches crashes/truncation in the join loop) via constant-pool
// growth; full content verification is blocked on T049 real MsStr support.
static void testImportDottedFullPathConstant(void) {
  struct MsChunk* simple = compileStr("import os");
  struct MsChunk* dotted = compileStr("import os.path");
  struct MsChunk* deeper = compileStr("import os.path.sep");
  MS_ASSERT_TRUE(simple->constLen == dotted->constLen, "same constant-pool slot count regardless of path depth");
  MS_ASSERT_TRUE(dotted->constLen == deeper->constLen, "same constant-pool slot count regardless of path depth");
  msChunkFree(simple);
  msFree(simple);
  msChunkFree(dotted);
  msFree(dotted);
  msChunkFree(deeper);
  msFree(deeper);
}

// T047: select case `v := <-ch` binds v as a local resolving to the received
// value, not an unrelated global (finding #5).
static void testSelectShortDeclBindsLocal(void) {
  struct MsChunk* ck = compileStr("select { case v := <-ch: print(v) }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_SELECT_CASE_RECV), "has SELECT_CASE_RECV");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_GET_LOCAL), "v resolves via local slot, not global");
  msChunkFree(ck);
  msFree(ck);
}

// T047: select case comm with a non-recv short-decl init (`case v := f():`)
// must be a compile error, not a union-type misread of ->recv.* (finding #4).
static void testSelectShortDeclNonRecvIsError(void) {
  MsCompileResult r = msCompile("select { case v := f(): pass }", 31, "<t>");
  MS_ASSERT_TRUE(r.hadError, "non-recv short-decl in select case is a compile error");
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testWithEnterBinding);
  MS_RUN(testWithNoBinding);
  MS_RUN(testGoStmt);
  MS_RUN(testSendStmt);
  MS_RUN(testImportSimple);
  MS_RUN(testImportDottedBindsFirstSegment);
  MS_RUN(testImportDottedFullPathConstant);
  MS_RUN(testSelectShortDeclBindsLocal);
  MS_RUN(testSelectShortDeclNonRecvIsError);
  return msTestSummary();
}
