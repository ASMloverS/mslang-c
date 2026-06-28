// test_containers.c
// T041: container build instruction compilation tests.
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

// T041: [1,2,3] -> OP_CONST x3 + OP_BUILD_LIST
static void testBuildList(void) {
  struct MsChunk* ck = compileStr("[1, 2, 3]");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_BUILD_LIST), "BUILD_LIST");
  MS_ASSERT_EQ(ck->constLen, 3, "3 consts");
  msChunkFree(ck);
  msFree(ck);
}

// T041: [] -> OP_BUILD_LIST(0)
static void testBuildEmptyList(void) {
  struct MsChunk* ck = compileStr("[]");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_BUILD_LIST), "BUILD_LIST");
  MS_ASSERT_EQ(ck->constLen, 0, "0 consts");
  msChunkFree(ck);
  msFree(ck);
}

// T041: {"a": 1} -> OP_BUILD_MAP
static void testBuildMap(void) {
  struct MsChunk* ck = compileStr("{\"a\": 1}");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_BUILD_MAP), "BUILD_MAP");
  msChunkFree(ck);
  msFree(ck);
}

// T041: {} -> OP_BUILD_MAP(0)
static void testBuildEmptyMap(void) {
  struct MsChunk* ck = compileStr("{}");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_BUILD_MAP), "BUILD_MAP");
  msChunkFree(ck);
  msFree(ck);
}

// T041: {1, 2} -> OP_BUILD_SET
static void testBuildSet(void) {
  struct MsChunk* ck = compileStr("{1, 2}");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_BUILD_SET), "BUILD_SET");
  MS_ASSERT_EQ(ck->constLen, 2, "2 consts");
  msChunkFree(ck);
  msFree(ck);
}

// T041: (1, 2, 3) -> OP_BUILD_TUPLE
static void testBuildTuple(void) {
  struct MsChunk* ck = compileStr("(1, 2, 3)");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_BUILD_TUPLE), "BUILD_TUPLE");
  MS_ASSERT_EQ(ck->constLen, 3, "3 consts");
  msChunkFree(ck);
  msFree(ck);
}

// T041: () -> OP_BUILD_TUPLE(0)
static void testBuildEmptyTuple(void) {
  struct MsChunk* ck = compileStr("()");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_BUILD_TUPLE), "BUILD_TUPLE");
  MS_ASSERT_EQ(ck->constLen, 0, "0 consts");
  msChunkFree(ck);
  msFree(ck);
}

// T041: a[1] -> OP_GET_ITEM
static void testIndexAccess(void) {
  struct MsChunk* ck = compileStr("a[1]");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_GET_ITEM), "GET_ITEM");
  msChunkFree(ck);
  msFree(ck);
}

// T041: a[1:3] -> OP_BUILD_SLICE + OP_GET_ITEM
static void testSliceAccess(void) {
  struct MsChunk* ck = compileStr("a[1:3]");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_BUILD_SLICE), "BUILD_SLICE");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_GET_ITEM), "GET_ITEM");
  msChunkFree(ck);
  msFree(ck);
}

// T041: obj.x -> OP_GET_ATTR
static void testAttrAccess(void) {
  struct MsChunk* ck = compileStr("obj.x");
  MS_ASSERT_TRUE(chunkHasOp(ck, OP_GET_ATTR), "GET_ATTR");
  msChunkFree(ck);
  msFree(ck);
}

int main(void) {
  MS_RUN(testBuildList);
  MS_RUN(testBuildEmptyList);
  MS_RUN(testBuildMap);
  MS_RUN(testBuildEmptyMap);
  MS_RUN(testBuildSet);
  MS_RUN(testBuildTuple);
  MS_RUN(testBuildEmptyTuple);
  MS_RUN(testIndexAccess);
  MS_RUN(testSliceAccess);
  MS_RUN(testAttrAccess);
  return msTestSummary();
}
