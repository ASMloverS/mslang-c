// ms_class.h -- user-defined class runtime objects: MsTypeObj/MsInstanceObj,
// msMetaType, instance attribute get/set, __init__ lookup (P5-T072)
#pragma once

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsVM;

// type-system.md ss3.2: "class is type" -- a user-defined class's runtime
// object embeds a struct MsType (mstype); every instance's head.type points
// directly at &tp->mstype, so msTypeOf(instance)->name is the class's own
// name with no type()-builtin special-casing needed.
struct MsTypeObj {
  struct MsObject head;  // head.type == &msMetaType
  struct MsType mstype;  // instances' head.type points here
};

// type-system.md ss3.2: instance object. No `klass` field -- head.type
// (== &tp->mstype) already identifies the owning class.
struct MsInstanceObj {
  struct MsObject head;
  struct MsObject* attrs;  // MsMapObj*, instance attribute dict
};

// Metatype shared by every user-defined class object (type(Foo) == "type").
extern struct MsType msMetaType;

// "__init__" string constant, created once by msVMInit() and cached here so
// msFindInit does not msNewStr a fresh key on every instantiation.
extern MsValue gInitNameVal;

// Magic-method (dunder) name string constants (P5-T074), interned once by
// msVMInit() (same gInitNameVal convention) so ms_vm.c's opcode dispatch
// never msNewStr's a fresh key per operation.
extern MsValue gDunderAdd, gDunderRadd;
extern MsValue gDunderSub, gDunderRsub;
extern MsValue gDunderMul, gDunderRmul;
extern MsValue gDunderDiv, gDunderRdiv;
extern MsValue gDunderMod, gDunderRmod;
extern MsValue gDunderPow, gDunderRpow;
extern MsValue gDunderOr, gDunderAnd, gDunderXor;
extern MsValue gDunderNeg, gDunderInvert;
extern MsValue gDunderEq, gDunderNe;
extern MsValue gDunderLt, gDunderLe, gDunderGt, gDunderGe;
extern MsValue gDunderContains;
extern MsValue gDunderGetitem, gDunderSetitem, gDunderDelitem;
extern MsValue gDunderIter, gDunderNext;

// GC traverse for MsInstanceObj (assigned as tp->mstype.traverse by
// ms_vm.c's OP_MAKE_CLASS).
void instanceTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx);

// True iff v is a user-defined class instance (P5-T074 impl doc ss0): every
// user class's mstype.traverse is instanceTraverse (assigned by OP_MAKE_CLASS),
// while each built-in type uses its own distinct traverse function, so
// comparing the traverse pointer's identity safely tells instances apart from
// built-ins without dereferencing a possibly-static (non-heap) MsType via
// offsetof.
static inline bool msIsInstance(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type->traverse == instanceTraverse;
}

// tpGetattr/tpSetattr slots for every user-defined class (ms_object.h;
// impl/P5-T072-class-instantiation.md ss7): instance attrs dict first, then
// (get only) the class's own methods dict -- the returned method is
// unbound (no self), self-binding via MsBoundMethodObj lands in T073.
MsValue instanceGetAttr(struct MsVM* vm, MsValue obj, MsValue name);
MsValue instanceSetAttr(struct MsVM* vm, MsValue obj, MsValue name, MsValue val);

// Finds __init__ along tp's baseClass chain (single inheritance; T072 does
// not depend on T073's mstype.mro). Returns MS_NIL_VAL if none.
MsValue msFindInit(struct MsVM* vm, struct MsType* tp);

// Bound method object (P5-T073): obj.method returns a MsBoundMethodObj with
// self already bound; calling it injects self as the first argument.
struct MsBoundMethodObj {
  struct MsObject head;  // head.type == &msBoundMethodType
  MsValue func;          // MsClosure*
  MsValue self;          // bound instance
};

extern struct MsType msBoundMethodType;

// func/self must be GC-rooted by the caller before calling (msNewBoundMethod
// allocates and may trigger GC).
MsValue msNewBoundMethod(MsValue func, MsValue self);

// Precomputes cls->mstype.mro (single-inheritance linear chain along
// baseClass, terminated at NULL -- no `object` root class). Must be called
// after cls->mstype.baseClass is assigned.
void msBuildMRO(struct MsTypeObj* cls);

// Looks up name along tp->mro's classes' own methods dicts. Returns
// MS_ERROR_VALUE (not MS_NIL_VAL, which is a legal attribute value) if not found.
MsValue msTypeLookupMethodMRO(struct MsVM* vm, struct MsType* tp, MsValue name);

// super() proxy object (P5-T075): attribute/method lookup scans instance's
// MRO starting right after startType (the class that defined the currently
// executing method -- MsClosure.definingClass, not type(instance)).
struct MsSuperObj {
  struct MsObject head;  // head.type == &msSuperType
  MsValue startType;     // MsTypeObj*: MRO scan starts at the class after this one
  MsValue instance;      // bound instance (self)
};

extern struct MsType msSuperType;

// startType/instance are already reachable via MsClosure.definingClass
// (frame->closure) and frame->slots[0] (value stack) respectively, so no
// extra msGCPushRoot is needed by the caller before calling this.
MsValue msNewSuper(MsValue startType, MsValue instance);
