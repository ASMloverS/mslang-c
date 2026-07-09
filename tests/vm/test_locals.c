#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_vm.h"

// Builds chunks by hand instead of going through msCompile(): at top level
// "x := 42" compiles to OP_SET_GLOBAL (a stub through T060) and every
// program-level chunk ends with OP_RETURN_NIL (no case in eval() yet), so
// neither path can reach a compiled VM test today. See test_eval_basic.c
// for the same workaround.

// OP_CONST 10; OP_CONST 20; OP_ROT2; OP_POP; OP_RETURN.
// Without the swap, POP would drop the top (20) and RETURN would yield the
// original bottom value (10); with the swap, POP drops the new top (10,
// the pre-swap bottom) and RETURN yields 20 -- so this distinguishes a
// correct OP_ROT2 from a no-op.
static void testRot2Swap(void) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  uint32_t idx10 = msChunkAddConst(&chunk, MS_INT_VAL(10));
  uint32_t idx20 = msChunkAddConst(&chunk, MS_INT_VAL(20));
  msChunkEmitOpAX(&chunk, OP_CONST, idx10, 1);
  msChunkEmitOpAX(&chunk, OP_CONST, idx20, 1);
  msChunkEmitOp(&chunk, OP_ROT2, 1);
  msChunkEmitOp(&chunk, OP_POP, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);

  msVMInit();
  MsValue result = msVMRun(&chunk);
  MS_ASSERT_TRUE(MS_IS_INT(result), "is int");
  MS_ASSERT_TRUE(MS_AS_INT(result) == 20, "top two values swapped");
  msVMShutdown();
  msChunkFree(&chunk);
}

// OP_CONST 42; OP_GET_UPVALUE 0; OP_SET_UPVALUE 0; OP_CLOSE_UPVALUE 0;
// OP_POP; OP_RETURN.
// GET_UPVALUE pushes a stub nil (net +1), SET_UPVALUE/CLOSE_UPVALUE only
// consume their operand byte and must not touch the value stack (net 0
// each). The trailing POP then removes exactly the nil pushed by
// GET_UPVALUE, leaving 42 as the sole remaining value -- if any stub
// mishandled the stack, RETURN would yield something other than 42 (or
// underflow).
static void testUpvalueStubsKeepStackBalanced(void) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  uint32_t idx42 = msChunkAddConst(&chunk, MS_INT_VAL(42));
  msChunkEmitOpAX(&chunk, OP_CONST, idx42, 1);
  msChunkEmitOpA(&chunk, OP_GET_UPVALUE, 0, 1);
  msChunkEmitOpA(&chunk, OP_SET_UPVALUE, 0, 1);
  msChunkEmitOpA(&chunk, OP_CLOSE_UPVALUE, 0, 1);
  msChunkEmitOp(&chunk, OP_POP, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);

  msVMInit();
  MsValue result = msVMRun(&chunk);
  MS_ASSERT_TRUE(MS_IS_INT(result), "is int");
  MS_ASSERT_TRUE(MS_AS_INT(result) == 42, "upvalue stubs kept stack balanced");
  msVMShutdown();
  msChunkFree(&chunk);
}

int main(void) {
  MS_RUN(testRot2Swap);
  MS_RUN(testUpvalueStubsKeepStackBalanced);
  return msTestSummary();
}
