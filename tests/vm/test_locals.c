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

// OP_GET_UPVALUE/OP_SET_UPVALUE/OP_CLOSE_UPVALUE now have real semantics
// (P5-T071, tests/vm/test_closures.c) instead of the P4-T052 stub this file
// used to smoke-test here (stub pushed a nil / ignored its operand; real
// GET_UPVALUE/SET_UPVALUE dereference frame->closure, which is NULL in a
// bare top-level chunk like the ones built by hand in this file, and real
// CLOSE_UPVALUE takes no operand -- vm.md ss3.2's P5-T071 correction).

int main(void) {
  MS_RUN(testRot2Swap);
  return msTestSummary();
}
