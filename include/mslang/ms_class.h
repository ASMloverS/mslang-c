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

// GC traverse for MsInstanceObj (assigned as tp->mstype.traverse by
// ms_vm.c's OP_MAKE_CLASS).
void instanceTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx);

// tpGetattr/tpSetattr slots for every user-defined class (ms_object.h;
// impl/P5-T072-class-instantiation.md ss7): instance attrs dict first, then
// (get only) the class's own methods dict -- the returned method is
// unbound (no self), self-binding via MsBoundMethodObj lands in T073.
MsValue instanceGetAttr(struct MsVM* vm, MsValue obj, MsValue name);
MsValue instanceSetAttr(struct MsVM* vm, MsValue obj, MsValue name, MsValue val);

// Finds __init__ along tp's baseClass chain (single inheritance; T072 does
// not depend on T073's mstype.mro). Returns MS_NIL_VAL if none.
MsValue msFindInit(struct MsVM* vm, struct MsType* tp);
