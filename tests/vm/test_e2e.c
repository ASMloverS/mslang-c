#include <stdint.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

// msCompile()+msVMRun() cannot reach a top-level expression's value in a C
// unit test easily, so chunks are built by hand (same workaround as
// test_locals.c/test_attr_index.c).

static MsValue str(const char* s) {
  return msNewStr(s, (uint32_t) strlen(s));
}

// OP_CONST 42; OP_SET_GLOBAL "x"; OP_POP; OP_GET_GLOBAL "x"; OP_RETURN.
static void testGlobalSetGetRoundTrip(void) {
  msVMInit();
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  MsValue name = str("x");
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, MS_INT_VAL(42)), 1);
  msChunkEmitOpAX(&chunk, OP_SET_GLOBAL, msChunkAddConst(&chunk, name), 1);
  msChunkEmitOp(&chunk, OP_POP, 1);
  msChunkEmitOpAX(&chunk, OP_GET_GLOBAL, msChunkAddConst(&chunk, name), 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  MS_ASSERT_TRUE(MS_IS_INT(result), "global round trip is int");
  MS_ASSERT_TRUE(MS_AS_INT(result) == 42, "global round trip value == 42");
  msChunkFree(&chunk);
  msVMShutdown();
}

// OP_GET_GLOBAL "nope"; OP_RETURN -> MS_ERROR_VALUE (NameError placeholder).
static void testUndefinedGlobalIsError(void) {
  msVMInit();
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_GET_GLOBAL, msChunkAddConst(&chunk, str("nope")), 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "undefined global name is an error");
  msChunkFree(&chunk);
  msVMShutdown();
}

// OP_RETURN_NIL with an empty chunk -> nil.
static void testReturnNilYieldsNil(void) {
  msVMInit();
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOp(&chunk, OP_RETURN_NIL, 1);
  MsValue result = msVMRun(&chunk);
  MS_ASSERT_TRUE(MS_IS_NIL(result), "OP_RETURN_NIL yields nil");
  msChunkFree(&chunk);
  msVMShutdown();
}

// OP_GET_GLOBAL "len"; OP_CONST "hello"; OP_CALL 1; OP_RETURN -> 5. Exercises
// OP_CALL's native-function fast path (msVMInit registers "len" as a builtin).
static void testCallBuiltinLen(void) {
  msVMInit();
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_GET_GLOBAL, msChunkAddConst(&chunk, str("len")), 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, str("hello")), 1);
  msChunkEmitOpA(&chunk, OP_CALL, 1, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  MS_ASSERT_TRUE(MS_IS_INT(result), "len(\"hello\") is int");
  MS_ASSERT_TRUE(MS_AS_INT(result) == 5, "len(\"hello\") == 5");
  msChunkFree(&chunk);
  msVMShutdown();
}

int main(void) {
  MS_RUN(testGlobalSetGetRoundTrip);
  MS_RUN(testUndefinedGlobalIsError);
  MS_RUN(testReturnNilYieldsNil);
  MS_RUN(testCallBuiltinLen);
  return msTestSummary();
}
