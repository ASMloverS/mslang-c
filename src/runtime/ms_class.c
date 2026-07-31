// ms_class.c -- user-defined class runtime objects: MsTypeObj/MsInstanceObj,
// msMetaType, instance attribute get/set, __init__ lookup (P5-T072)
#include "mslang/ms_class.h"

#include <stddef.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_map.h"

MsValue gInitNameVal;

// ms_func.c's closureTraverse convention: wrap a raw MsObject* in a temporary
// MsValue for this mark-only visit; the mark-sweep GC never moves objects, so
// no write-back through inst->attrs needs to be observed.
//
// Also keeps the owning class alive as long as any instance is reachable:
// T072's "class is type" design (type-system.md ss3.2) makes head.type point
// directly into the class object's embedded mstype, so an instance
// outliving its class would leave head.type dangling. mstype is not
// MsTypeObj's first member, so recovering the owning MsTypeObj* from
// obj->type needs an offsetof adjustment (unlike the usual head-is-first-
// member cast).
void instanceTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsInstanceObj* inst = (struct MsInstanceObj*) obj;
  struct MsTypeObj* tp = (struct MsTypeObj*) ((char*) obj->type - offsetof(struct MsTypeObj, mstype));
  MsValue tpVal = MS_OBJ_VAL(tp);
  visit(&tpVal, ctx);

  if (inst->attrs) {
    MsValue attrsVal = MS_OBJ_VAL(inst->attrs);
    visit(&attrsVal, ctx);
  }
}

static void typeTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsTypeObj* tp = (struct MsTypeObj*) obj;
  if (tp->mstype.baseClass) {
    MsValue v = MS_OBJ_VAL(tp->mstype.baseClass);
    visit(&v, ctx);
  }
  if (tp->mstype.methods) {
    MsValue v = MS_OBJ_VAL(tp->mstype.methods);
    visit(&v, ctx);
  }
  if (tp->mstype.mro) {
    MsValue v = MS_OBJ_VAL(tp->mstype.mro);
    visit(&v, ctx);
  }
}

// tp->mstype.name is an owned copy (ms_vm.c's OP_MAKE_CLASS); baseClass/
// methods/mro are GC-managed objects freed by the collector itself.
static void typeDestroy(struct MsObject* obj) {
  struct MsTypeObj* tp = (struct MsTypeObj*) obj;
  msFree((void*) tp->mstype.name);
}

struct MsType msMetaType = {
    .name = "type",
    .objSize = sizeof(struct MsTypeObj),
    .traverse = typeTraverse,
    .destroy = typeDestroy,
};

MsValue instanceGetAttr(struct MsVM* vm, MsValue obj, MsValue name) {
  struct MsInstanceObj* inst = (struct MsInstanceObj*) MS_AS_OBJ(obj);
  if (MS_AS_BOOL(msMapHas(vm, MS_OBJ_VAL(inst->attrs), name))) {
    return msMapGet(vm, MS_OBJ_VAL(inst->attrs), name);
  }
  struct MsType* tp = MS_AS_OBJ(obj)->type;
  if (tp->methods) {
    MsValue m = msMapGet(vm, MS_OBJ_VAL(tp->methods), name);
    if (!MS_IS_NIL(m)) {
      return m;
    }
  }
  return MS_ERROR_VALUE;  // not found: OP_GET_ATTR propagates this as AttributeError
}

MsValue instanceSetAttr(struct MsVM* vm, MsValue obj, MsValue name, MsValue val) {
  struct MsInstanceObj* inst = (struct MsInstanceObj*) MS_AS_OBJ(obj);
  return msMapSet(vm, MS_OBJ_VAL(inst->attrs), name, val);
}

MsValue msFindInit(struct MsVM* vm, struct MsType* tp) {
  while (tp) {
    if (tp->methods) {
      MsValue m = msMapGet(vm, MS_OBJ_VAL(tp->methods), gInitNameVal);
      if (!MS_IS_NIL(m)) {
        return m;
      }
    }
    tp = tp->baseClass ? &((struct MsTypeObj*) tp->baseClass)->mstype : NULL;
  }
  return MS_NIL_VAL;
}
