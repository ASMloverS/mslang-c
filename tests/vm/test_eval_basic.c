#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_vm.h"

// Builds "OP_CONST 42; OP_RETURN" directly instead of going through
// msCompile("42", ...): the existing (T037-T047) statement compiler always
// emits OP_POP after an expression statement and appends OP_RETURN_NIL at
// program end, so a compiled top-level "42" never leaves a value on the
// stack for OP_RETURN. Building the chunk by hand isolates the checklist
// item under test here -- "top-level OP_CONST + OP_RETURN returns the
// constant" -- from that unrelated, already-implemented compiler behavior.
static void testConstReturn(void) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  uint32_t idx = msChunkAddConst(&chunk, MS_INT_VAL(42));
  msChunkEmitOpAX(&chunk, OP_CONST, idx, 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);

  msVMInit();
  MsValue result = msVMRun(&chunk);
  MS_ASSERT_TRUE(MS_IS_INT(result), "is int");
  MS_ASSERT_TRUE(MS_AS_INT(result) == 42, "value 42");
  msVMShutdown();
  msChunkFree(&chunk);
}

int main(void) {
  MS_RUN(testConstReturn);
  return msTestSummary();
}
