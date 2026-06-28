// test_func_compile.c
// T043: function compilation tests.
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

// T043: func f() { return 1 } -> has OP_MAKE_FUNC in outer chunk
static void testFuncMake(void) {
  struct MsChunk* ck = compileStr("func f() { return 1 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_MAKE_FUNC), "has MAKE_FUNC");
  msChunkFree(ck);
  msFree(ck);
}

// T043: func f(a, b) { return a + b } -> compiles ok, has OP_MAKE_FUNC
static void testFuncParams(void) {
  struct MsChunk* ck = compileStr("func f(a, b) { return a + b }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_MAKE_FUNC), "has MAKE_FUNC");
  msChunkFree(ck);
  msFree(ck);
}

// T043: func f() { 1 } -> no error (implicit return nil)
static void testFuncImplicitReturn(void) {
  struct MsChunk* ck = compileStr("func f() { 1 }");
  MS_ASSERT_TRUE(ck != NULL, "compiled ok");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_MAKE_FUNC), "has MAKE_FUNC");
  msChunkFree(ck);
  msFree(ck);
}

// T043: func f() { return 42 } -> no error, outer has OP_MAKE_FUNC
static void testFuncReturn(void) {
  struct MsChunk* ck = compileStr("func f() { return 42 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_MAKE_FUNC), "has MAKE_FUNC");
  msChunkFree(ck);
  msFree(ck);
}

// T043: closure capturing upvalue
static void testFuncClosure(void) {
  struct MsChunk* ck = compileStr("func outer() { var x = 1\nfunc inner() { return x } }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_MAKE_FUNC), "has MAKE_FUNC");
  msChunkFree(ck);
  msFree(ck);
}

// T043: 3-level nested closure
static void testNestedClosure(void) {
  struct MsChunk* ck = compileStr("func a() { var x = 1\nfunc b() { func c() { return x } } }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_MAKE_FUNC), "has MAKE_FUNC");
  msChunkFree(ck);
  msFree(ck);
}

// T043: return outside function -> compile error
static void testReturnOutsideFunc(void) {
  compileStrExpectError("return 1");
}

// T043: async func f() { } -> no compile error, has OP_MAKE_FUNC
static void testAsyncFunc(void) {
  struct MsChunk* ck = compileStr("async func f() { }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_MAKE_FUNC), "has MAKE_FUNC");
  msChunkFree(ck);
  msFree(ck);
}

// T043: var f = func() { return 1 } -> function literal as expression
static void testFuncLiteral(void) {
  struct MsChunk* ck = compileStr("var f = func() { return 1 }");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_MAKE_FUNC), "has MAKE_FUNC");
  msChunkFree(ck);
  msFree(ck);
}

// T043: duplicate parameter name -> compile error
static void testDuplicateParam(void) {
  compileStrExpectError("func f(a, a) { }");
}

int main(void) {
  MS_RUN(testFuncMake);
  MS_RUN(testFuncParams);
  MS_RUN(testFuncImplicitReturn);
  MS_RUN(testFuncReturn);
  MS_RUN(testFuncClosure);
  MS_RUN(testNestedClosure);
  MS_RUN(testReturnOutsideFunc);
  MS_RUN(testAsyncFunc);
  MS_RUN(testFuncLiteral);
  MS_RUN(testDuplicateParam);
  return msTestSummary();
}
