#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_opcode.h"
#include "mslang/ms_value.h"

static void testChunkBasic(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  msChunkEmitOp(&ck, OP_CONST_NIL, 1);
  msChunkEmitOp(&ck, OP_RETURN, 1);

  MS_ASSERT_EQ(ck.codeLen, 2, "2 bytes");
  MS_ASSERT_EQ(ck.code[0], OP_CONST_NIL, "CONST_NIL");
  MS_ASSERT_EQ(ck.code[1], OP_RETURN, "RETURN");
  MS_ASSERT_EQ(msChunkGetLine(&ck, 0), 1, "line 1");

  msChunkFree(&ck);
}

static void testChunkAddConst(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  MsValue v = MS_INT_VAL(42);
  uint32_t idx = msChunkAddConst(&ck, v);
  MS_ASSERT_EQ(idx, 0, "first const idx=0");
  MS_ASSERT_EQ(ck.constLen, 1, "1 const");

  msChunkFree(&ck);
}

static void testPatchJump(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  // emit: JUMP [placeholder AX=0x000000] CONST_NIL
  msChunkEmitOpAX(&ck, OP_JUMP, 0, 1);
  msChunkEmitOp(&ck, OP_CONST_NIL, 1);

  // targetOffset = 4 (CONST_NIL at offset 4)
  // rel = targetOffset - (patchOffset + 4) = 4 - (1 + 4) = -1
  // Wait: patchOffset=1 (first operand byte after OP_JUMP)
  // Next instr after JUMP is at offset 4 (1 opcode + 3 operand)
  // rel = target - nextInstr = 4 - 4 = 0
  uint32_t targetOffset = ck.codeLen - 1;
  msChunkPatchJump(&ck, 1, targetOffset);

  int32_t rel = ((int32_t) ck.code[1] << 16) | ((int32_t) ck.code[2] << 8) | ck.code[3];
  if (rel & 0x800000) {
    rel |= (int32_t) 0xFF000000;
  }
  MS_ASSERT_EQ(rel, 0, "rel offset=0 for forward jump to next instr");

  // backward jump: CONST_NIL then JUMP back to offset 0
  struct MsChunk ck2;
  msChunkInit(&ck2, NULL);
  msChunkEmitOp(&ck2, OP_CONST_NIL, 1);
  uint32_t loopStart = 0;
  msChunkEmitOpAX(&ck2, OP_JUMP, 0, 1);  // patchOffset=2 (operand starts)
  // rel = 0 - (2 + 3) = -5
  msChunkPatchJump(&ck2, 2, loopStart);
  int32_t rel2 = ((int32_t) ck2.code[2] << 16) | ((int32_t) ck2.code[3] << 8) | ck2.code[4];
  if (rel2 & 0x800000) {
    rel2 |= (int32_t) 0xFF000000;
  }
  MS_ASSERT_EQ(rel2, -5, "rel offset=-5 for backward jump");

  msChunkFree(&ck);
  msChunkFree(&ck2);
}

static void testConstOverflow(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  for (uint32_t i = 0; i < 300; i++) {
    uint32_t idx = msChunkAddConst(&ck, MS_INT_VAL((int64_t) i));
    MS_ASSERT_EQ(idx, i, "const index matches");
  }
  MS_ASSERT_EQ(ck.constLen, 300, "300 constants");

  msChunkFree(&ck);
}

static void testChunkAsan(void) {
  struct MsChunk ck;
  msChunkInit(&ck, NULL);

  for (int i = 0; i < 100; i++) {
    msChunkEmitOp(&ck, OP_CONST_NIL, (uint32_t) i);
    msChunkAddConst(&ck, MS_INT_VAL(i));
  }

  msChunkFree(&ck);
  // ASAN will catch leaks if msChunkFree is incomplete
  MS_ASSERT_EQ(ck.codeLen, 0, "freed codeLen=0");
  MS_ASSERT_EQ(ck.constLen, 0, "freed constLen=0");
  MS_ASSERT_EQ(ck.linesLen, 0, "freed linesLen=0");
}

int main(void) {
  MS_RUN(testChunkBasic);
  MS_RUN(testChunkAddConst);
  MS_RUN(testPatchJump);
  MS_RUN(testConstOverflow);
  MS_RUN(testChunkAsan);
  return msTestSummary();
}
