// test_class_compile.c
// T044: class compilation tests.
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

// T044: empty class -> OP_MAKE_CLASS with u16 nameIdx, methodCount=0, baseCount=0
static void testClassEmpty(void) {
  struct MsChunk* ck = compileStr("class Foo {}");
  int pos = findOp(ck, OP_MAKE_CLASS);
  MS_ASSERT_TRUE(pos >= 0, "has MAKE_CLASS");
  // after opcode: u16 nameIdx (2 bytes), methodCount (1 byte), baseCount (1 byte)
  MS_ASSERT_TRUE(pos + 4 < (int) ck->codeLen, "enough bytes after MAKE_CLASS");
  uint8_t methodCount = ck->code[pos + 3];
  uint8_t baseCount = ck->code[pos + 4];
  MS_ASSERT_EQ(methodCount, 0, "methodCount == 0");
  MS_ASSERT_EQ(baseCount, 0, "baseCount == 0");
  msChunkFree(ck);
  msFree(ck);
}

// T044: class with one method -> OP_MAKE_CLASS with methodCount=1, no OP_MAKE_FUNC
static void testClassWithMethod(void) {
  struct MsChunk* ck = compileStr("class Foo { func f(self) { } }");
  int pos = findOp(ck, OP_MAKE_CLASS);
  MS_ASSERT_TRUE(pos >= 0, "has MAKE_CLASS");
  // layout: opcode(1) + nameIdx(2) + methodCount(1) + baseCount(1) + per-method(4)
  MS_ASSERT_TRUE(pos + 8 < (int) ck->codeLen, "enough bytes for 1 method");
  uint8_t methodCount = ck->code[pos + 3];
  uint8_t baseCount = ck->code[pos + 4];
  MS_ASSERT_EQ(methodCount, 1, "methodCount == 1");
  MS_ASSERT_EQ(baseCount, 0, "baseCount == 0");
  // methods are protos in constant pool, NOT runtime OP_MAKE_FUNC
  MS_ASSERT_FALSE(chunkHasOp(ck, OP_MAKE_FUNC), "no MAKE_FUNC in outer chunk");
  msChunkFree(ck);
  msFree(ck);
}

// T044: class with base -> OP_MAKE_CLASS with baseCount=1
static void testClassExtends(void) {
  struct MsChunk* ck = compileStr("class Base {}\nclass Bar extends Base {}");
  // find the second OP_MAKE_CLASS (Bar, not Base)
  int pos = -1;
  int count = 0;
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == OP_MAKE_CLASS) {
      count++;
      if (count == 2) {
        pos = (int) i;
        break;
      }
    }
  }
  MS_ASSERT_TRUE(pos >= 0, "has second MAKE_CLASS for Bar");
  uint8_t methodCount = ck->code[pos + 3];
  uint8_t baseCount = ck->code[pos + 4];
  MS_ASSERT_EQ(methodCount, 0, "Bar methodCount == 0");
  MS_ASSERT_EQ(baseCount, 1, "Bar baseCount == 1");
  msChunkFree(ck);
  msFree(ck);
}

// T044: class with multiple methods -> methodCount=2, no OP_MAKE_FUNC
static void testClassMultipleMethods(void) {
  struct MsChunk* ck = compileStr("class Foo { func a(self) { } func b(self) { } }");
  int pos = findOp(ck, OP_MAKE_CLASS);
  MS_ASSERT_TRUE(pos >= 0, "has MAKE_CLASS");
  uint8_t methodCount = ck->code[pos + 3];
  MS_ASSERT_EQ(methodCount, 2, "methodCount == 2");
  MS_ASSERT_FALSE(chunkHasOp(ck, OP_MAKE_FUNC), "no MAKE_FUNC in outer chunk");
  msChunkFree(ck);
  msFree(ck);
}

// T044: class attribute (var decl in body) -> compile error
static void testClassAttrError(void) {
  compileStrExpectError("class Foo { var x = 1 }");
}

// T044: class attribute (short decl in body) -> compile error
static void testClassAttrShortDeclError(void) {
  compileStrExpectError("class Foo { x := 1 }");
}

int main(void) {
  MS_RUN(testClassEmpty);
  MS_RUN(testClassWithMethod);
  MS_RUN(testClassExtends);
  MS_RUN(testClassMultipleMethods);
  MS_RUN(testClassAttrError);
  MS_RUN(testClassAttrShortDeclError);
  return msTestSummary();
}
