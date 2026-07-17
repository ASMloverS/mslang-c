#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_chunk.h"
#include "mslang/ms_map.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

// msCompile()+msVMRun() cannot reach a top-level expression's value yet (see
// test_list.c/test_map.c for the same, already-documented workaround), so
// chunks are built by hand instead.

static MsValue str(const char* s) {
  return msNewStr(s, (uint32_t) strlen(s));
}

static bool gSetAttrCalled = false;
static bool gDelAttrCalled = false;

static MsValue stubGetAttr(struct MsVM* vm, MsValue obj, MsValue name) {
  (void) vm;
  (void) obj;
  (void) name;
  return MS_INT_VAL(42);
}

static MsValue stubSetAttr(struct MsVM* vm, MsValue obj, MsValue name, MsValue val) {
  (void) vm;
  (void) obj;
  (void) name;
  (void) val;
  gSetAttrCalled = true;
  return MS_NIL_VAL;
}

static MsValue stubDelAttr(struct MsVM* vm, MsValue obj, MsValue name) {
  (void) vm;
  (void) obj;
  (void) name;
  gDelAttrCalled = true;
  return MS_NIL_VAL;
}

// obj on stack; OP_GET_ATTR nameIdx; OP_RETURN.
static MsValue runGetAttr(MsValue obj, MsValue name) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, obj), 1);
  msChunkEmitOpAX(&chunk, OP_GET_ATTR, msChunkAddConst(&chunk, name), 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

// val, obj on stack (compileAssign's push order); OP_SET_ATTR nameIdx;
// OP_POP (mirrors compileAssign's trailing pop); OP_CONST sentinel;
// OP_RETURN -- exercises that OP_SET_ATTR leaves exactly one balancing value
// on the stack for the compiler-emitted OP_POP to consume.
static MsValue runSetAttrThenPop(MsValue obj, MsValue name, MsValue val, MsValue sentinel) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, val), 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, obj), 1);
  msChunkEmitOpAX(&chunk, OP_SET_ATTR, msChunkAddConst(&chunk, name), 1);
  msChunkEmitOp(&chunk, OP_POP, 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, sentinel), 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

// obj on stack; OP_DEL_ATTR nameIdx (statement form, no trailing pop);
// OP_CONST sentinel; OP_RETURN -- exercises that OP_DEL_ATTR leaves the
// stack exactly balanced (pops obj, pushes nothing).
static MsValue runDelAttrThenConst(MsValue obj, MsValue name, MsValue sentinel) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, obj), 1);
  msChunkEmitOpAX(&chunk, OP_DEL_ATTR, msChunkAddConst(&chunk, name), 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, sentinel), 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

// obj, key on stack (compileDel's push order); OP_DEL_ITEM; OP_CONST
// sentinel; OP_RETURN.
static MsValue runDelItemThenConst(MsValue obj, MsValue key, MsValue sentinel) {
  struct MsChunk chunk;
  msChunkInit(&chunk, NULL);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, obj), 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, key), 1);
  msChunkEmitOp(&chunk, OP_DEL_ITEM, 1);
  msChunkEmitOpAX(&chunk, OP_CONST, msChunkAddConst(&chunk, sentinel), 1);
  msChunkEmitOp(&chunk, OP_RETURN, 1);
  MsValue result = msVMRun(&chunk);
  msChunkFree(&chunk);
  return result;
}

static void testGetAttrDispatchesToTypeSlot(void) {
  msVMInit();
  gVM.intType->tpGetattr = stubGetAttr;
  MsValue result = runGetAttr(MS_INT_VAL(1), str("foo"));
  MS_ASSERT_TRUE(MS_IS_INT(result) && MS_AS_INT(result) == 42, "GET_ATTR dispatches to tpGetattr slot");
  gVM.intType->tpGetattr = NULL;
  msVMShutdown();
}

static void testGetAttrNoSlotIsError(void) {
  msVMInit();
  MsValue result = runGetAttr(MS_INT_VAL(1), str("foo"));
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "GET_ATTR with no tpGetattr/methods is an error (AttributeError)");
  msVMShutdown();
}

static void testSetAttrDispatchesAndBalancesStack(void) {
  msVMInit();
  gSetAttrCalled = false;
  gVM.intType->tpSetattr = stubSetAttr;
  MsValue result = runSetAttrThenPop(MS_INT_VAL(1), str("foo"), MS_INT_VAL(9), MS_INT_VAL(7));
  MS_ASSERT_TRUE(gSetAttrCalled, "SET_ATTR dispatches to tpSetattr slot");
  MS_ASSERT_TRUE(MS_IS_INT(result) && MS_AS_INT(result) == 7, "SET_ATTR leaves stack balanced for compiler's OP_POP");
  gVM.intType->tpSetattr = NULL;
  msVMShutdown();
}

static void testSetAttrNoSlotIsError(void) {
  msVMInit();
  MsValue result = runSetAttrThenPop(MS_INT_VAL(1), str("foo"), MS_INT_VAL(9), MS_INT_VAL(7));
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "SET_ATTR with no tpSetattr is an error (AttributeError)");
  msVMShutdown();
}

static void testDelAttrDispatchesToTypeSlot(void) {
  msVMInit();
  gDelAttrCalled = false;
  gVM.intType->tpDelattr = stubDelAttr;
  MsValue result = runDelAttrThenConst(MS_INT_VAL(1), str("foo"), MS_INT_VAL(7));
  MS_ASSERT_TRUE(gDelAttrCalled, "DEL_ATTR dispatches to tpDelattr slot");
  MS_ASSERT_TRUE(MS_IS_INT(result) && MS_AS_INT(result) == 7, "DEL_ATTR leaves the stack exactly balanced");
  gVM.intType->tpDelattr = NULL;
  msVMShutdown();
}

static void testDelAttrNoSlotIsError(void) {
  msVMInit();
  MsValue result = runDelAttrThenConst(MS_INT_VAL(1), str("foo"), MS_INT_VAL(7));
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "DEL_ATTR with no tpDelattr is an error (AttributeError)");
  msVMShutdown();
}

static void testDelItemMapRemovesKey(void) {
  msVMInit();
  MsValue m = msNewMap(1);
  msMapSet(&gVM, m, str("a"), MS_INT_VAL(1));
  struct MsMapObj* mo = (struct MsMapObj*) MS_AS_OBJ(m);
  MS_ASSERT_TRUE(mo->len == 1, "map starts with 1 key");

  MsValue result = runDelItemThenConst(m, str("a"), MS_INT_VAL(7));
  MS_ASSERT_TRUE(MS_IS_INT(result) && MS_AS_INT(result) == 7, "DEL_ITEM leaves the stack exactly balanced");
  MS_ASSERT_TRUE(mo->len == 0, "del m[\"a\"] removes the key");
  msVMShutdown();
}

static void testDelItemNoSlotIsError(void) {
  msVMInit();
  MsValue result = runDelItemThenConst(MS_INT_VAL(1), MS_INT_VAL(0), MS_INT_VAL(7));
  MS_ASSERT_TRUE(MS_IS_ERROR(result), "DEL_ITEM on a type with no tpDelitem is an error (TypeError)");
  msVMShutdown();
}

int main(void) {
  MS_RUN(testGetAttrDispatchesToTypeSlot);
  MS_RUN(testGetAttrNoSlotIsError);
  MS_RUN(testSetAttrDispatchesAndBalancesStack);
  MS_RUN(testSetAttrNoSlotIsError);
  MS_RUN(testDelAttrDispatchesToTypeSlot);
  MS_RUN(testDelAttrNoSlotIsError);
  MS_RUN(testDelItemMapRemovesKey);
  MS_RUN(testDelItemNoSlotIsError);
  return msTestSummary();
}
