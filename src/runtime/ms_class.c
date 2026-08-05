// ms_class.c -- user-defined class runtime objects: MsTypeObj/MsInstanceObj,
// msMetaType, instance attribute get/set, __init__ lookup (P5-T072)
#include "mslang/ms_class.h"

#include <stddef.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_list.h"
#include "mslang/ms_map.h"

MsValue gInitNameVal;

// P5-T074: dunder name constants, interned once by ms_vm.c's msVMInit
// (same gInitNameVal convention; storage lives here, initialization there).
MsValue gDunderAdd, gDunderRadd;
MsValue gDunderSub, gDunderRsub;
MsValue gDunderMul, gDunderRmul;
MsValue gDunderDiv, gDunderRdiv;
MsValue gDunderMod, gDunderRmod;
MsValue gDunderPow, gDunderRpow;
MsValue gDunderOr, gDunderAnd, gDunderXor;
MsValue gDunderNeg, gDunderInvert;
MsValue gDunderEq, gDunderNe;
MsValue gDunderLt, gDunderLe, gDunderGt, gDunderGe;
MsValue gDunderContains;
MsValue gDunderGetitem, gDunderSetitem, gDunderDelitem;
MsValue gDunderIter, gDunderNext;

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

// ms_func.c's closureTraverse convention (mark-only, no write-back needed).
static void boundMethodTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsBoundMethodObj* bm = (struct MsBoundMethodObj*) obj;
  visit(&bm->func, ctx);
  visit(&bm->self, ctx);
}

struct MsType msBoundMethodType = {
    .name = "method",
    .objSize = sizeof(struct MsBoundMethodObj),
    .traverse = boundMethodTraverse,
};

MsValue msNewBoundMethod(MsValue func, MsValue self) {
  struct MsBoundMethodObj* bm =
      (struct MsBoundMethodObj*) msGCAlloc(&msBoundMethodType, sizeof(struct MsBoundMethodObj));
  bm->func = func;
  bm->self = self;
  return MS_OBJ_VAL(bm);
}

// Single-inheritance MRO: [cls, parent, grandparent, ...], terminated at
// baseClass == NULL (no `object` root class, see impl/P5-T073 ss0).
// cls->mstype.baseClass must already be set (OP_MAKE_CLASS calls this after
// assigning it).
void msBuildMRO(struct MsTypeObj* cls) {
  // Length isn't known upfront without a separate walk, so grow the list as
  // we go (same single-pass pattern as ms_vm.c's collectIntoList).
  MsValue mroVal = msNewList(0);
  msGCPushRoot(mroVal);
  struct MsListObj* mroList = (struct MsListObj*) MS_AS_OBJ(mroVal);
  for (struct MsTypeObj* cur = cls; cur != NULL;
       cur = cur->mstype.baseClass ? (struct MsTypeObj*) cur->mstype.baseClass : NULL) {
    msListAppend(mroList, MS_OBJ_VAL(cur));
  }
  cls->mstype.mro = MS_AS_OBJ(mroVal);
  msGCPopRoot();
}

// Walks mroObj (precomputed by msBuildMRO) from index startIdx (inclusive)
// checking each class's own methods dict; MS_ERROR_VALUE (not MS_NIL_VAL, a
// legal attribute value) means not found anywhere in the scanned range.
// Shared by msTypeLookupMethodMRO (startIdx=0) and superGetAttr (startIdx=
// one past startType, P5-T075).
static MsValue lookupMethodFromIndex(struct MsVM* vm, struct MsObject* mroObj, uint32_t startIdx, MsValue name) {
  if (!mroObj) {
    return MS_ERROR_VALUE;
  }
  struct MsListObj* mro = (struct MsListObj*) mroObj;
  for (uint32_t i = startIdx; i < mro->len; i++) {
    struct MsTypeObj* cur = (struct MsTypeObj*) MS_AS_OBJ(mro->items[i]);
    if (cur->mstype.methods) {
      MsValue m = msMapGet(vm, MS_OBJ_VAL(cur->mstype.methods), name);
      if (!MS_IS_NIL(m)) {
        return m;
      }
    }
  }
  return MS_ERROR_VALUE;
}

MsValue msTypeLookupMethodMRO(struct MsVM* vm, struct MsType* tp, MsValue name) {
  return lookupMethodFromIndex(vm, tp->mro, 0, name);
}

// Shared by instanceGetAttr and superGetAttr: self is not itself a GC root
// at this point (already popped off the value stack by OP_GET_ATTR's call
// site), and m is reachable only via the defining class's methods dict, not
// the dict itself, so both need rooting before msNewBoundMethod's allocation
// can trigger GC.
static MsValue bindMethod(MsValue m, MsValue self) {
  msGCPushRoot(self);
  msGCPushRoot(m);
  MsValue bound = msNewBoundMethod(m, self);
  msGCPopRoot();  // m
  msGCPopRoot();  // self
  return bound;
}

// tpGetattr for msSuperType (P5-T075): scans su->instance's MRO starting
// right after su->startType. startType not found in the MRO (cross-class
// misuse of super) or startType being the MRO's last element both naturally
// fall out as "not found" via lookupMethodFromIndex -- no separate guard
// needed.
static MsValue superGetAttr(struct MsVM* vm, MsValue v, MsValue name) {
  struct MsSuperObj* su = (struct MsSuperObj*) MS_AS_OBJ(v);
  if (!MS_IS_OBJ(su->instance)) {
    return MS_ERROR_VALUE;  // self was reassigned to a non-object value (T080 placeholder)
  }
  struct MsType* instType = MS_AS_OBJ(su->instance)->type;
  if (!instType->mro) {
    return MS_ERROR_VALUE;
  }
  struct MsListObj* mro = (struct MsListObj*) instType->mro;
  uint32_t startIdx = mro->len;  // default: startType not found => scan nothing
  for (uint32_t i = 0; i < mro->len; i++) {
    if (MS_AS_OBJ(mro->items[i]) == MS_AS_OBJ(su->startType)) {
      startIdx = i + 1;
      break;
    }
  }
  MsValue m = lookupMethodFromIndex(vm, instType->mro, startIdx, name);
  if (MS_IS_ERROR(m)) {
    return MS_ERROR_VALUE;  // AttributeError (T080 placeholder)
  }
  return bindMethod(m, su->instance);
}

static void superTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsSuperObj* su = (struct MsSuperObj*) obj;
  visit(&su->startType, ctx);
  visit(&su->instance, ctx);
}

struct MsType msSuperType = {
    .name = "super",
    .objSize = sizeof(struct MsSuperObj),
    .traverse = superTraverse,
    .tpGetattr = superGetAttr,
};

MsValue msNewSuper(MsValue startType, MsValue instance) {
  struct MsSuperObj* su = (struct MsSuperObj*) msGCAlloc(&msSuperType, sizeof(struct MsSuperObj));
  su->startType = startType;
  su->instance = instance;
  return MS_OBJ_VAL(su);
}

MsValue instanceGetAttr(struct MsVM* vm, MsValue obj, MsValue name) {
  struct MsInstanceObj* inst = (struct MsInstanceObj*) MS_AS_OBJ(obj);
  if (MS_AS_BOOL(msMapHas(vm, MS_OBJ_VAL(inst->attrs), name))) {
    return msMapGet(vm, MS_OBJ_VAL(inst->attrs), name);
  }
  struct MsType* tp = MS_AS_OBJ(obj)->type;
  MsValue m = msTypeLookupMethodMRO(vm, tp, name);
  if (MS_IS_ERROR(m)) {
    return MS_ERROR_VALUE;  // not found: OP_GET_ATTR propagates this as AttributeError
  }
  return bindMethod(m, obj);
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
