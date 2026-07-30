// ms_upvalue.c -- MsUpvalue open/close semantics + GC traverse (P5-T071)
#include "mslang/ms_upvalue.h"

#include "mslang/ms_gc.h"
#include "mslang/ms_object.h"
#include "mslang/ms_vm.h"

MsUpvalue* msCaptureUpvalue(struct MsThread* t, MsValue* location) {
  MsUpvalue* prev = NULL;
  MsUpvalue* cur = t->openUpvalues;
  while (cur && cur->location > location) {
    prev = cur;
    cur = cur->nextOpen;
  }
  if (cur && cur->location == location) {
    return cur;  // already open, reuse (closures sharing a variable, vm.md ss5)
  }

  // msGCAlloc may trigger GC; fields are fully initialized before the new
  // node is linked into t->openUpvalues, so a mid-init collection never
  // traverses a partially-initialized node.
  MsUpvalue* uv = (MsUpvalue*) msGCAlloc(&msUpvalueType, sizeof(*uv));
  uv->location = location;
  uv->closedVal = MS_NIL_VAL;
  uv->nextOpen = cur;
  if (prev) {
    prev->nextOpen = uv;
  } else {
    t->openUpvalues = uv;
  }
  return uv;
}

void msCloseUpvalues(struct MsThread* t, MsValue* slot) {
  while (t->openUpvalues && t->openUpvalues->location >= slot) {
    MsUpvalue* uv = t->openUpvalues;
    uv->closedVal = *uv->location;
    uv->location = &uv->closedVal;
    t->openUpvalues = uv->nextOpen;
    uv->nextOpen = NULL;
  }
}

// Open state is covered by markRoots' stack enumeration (gc.md ss8); only
// the closed state's heap-resident closedVal needs an explicit visit here.
static void upvalueTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  MsUpvalue* uv = (MsUpvalue*) obj;
  if (uv->location == &uv->closedVal) {
    visit(&uv->closedVal, ctx);
  }
}

struct MsType msUpvalueType = {
    .name = "upvalue",
    .objSize = sizeof(MsUpvalue),
    .traverse = upvalueTraverse,
};
