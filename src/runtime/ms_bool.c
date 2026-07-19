// ms_bool.c -- msBoolType/msNilType definitions: bool/nil runtime type slots (type-system.md ss2.3/ss2.4)
#include "mslang/ms_bool.h"

#include "mslang/ms_object.h"
#include "mslang/ms_str.h"
#include "mslang/ms_value.h"

// tpRepr/tpStr were deferred pending msNewStr (T057); wired up here since M1
// print()/str()/repr() (impl/P4-T067-vm-e2e-m1.md) are the first callers.
static MsValue boolStr(struct MsVM* vm, MsValue v) {
  (void) vm;
  return MS_AS_BOOL(v) ? msNewStr("true", 4) : msNewStr("false", 5);
}

static MsValue boolHash(struct MsVM* vm, MsValue v) {
  (void) vm;
  return MS_INT_VAL(MS_AS_BOOL(v) ? 1 : 0);
}

static MsValue boolEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  return MS_BOOL_VAL(msValueEqual(a, b));
}

struct MsType msBoolType = {
    .name = "bool",
    .objSize = 0,  // scalar, no heap object
    .tpStr = boolStr,
    .tpRepr = boolStr,
    .tpHash = boolHash,
    .tpEq = boolEq,
};

static MsValue nilStr(struct MsVM* vm, MsValue v) {
  (void) vm;
  (void) v;
  return msNewStr("nil", 3);
}

static MsValue nilHash(struct MsVM* vm, MsValue v) {
  (void) vm;
  (void) v;
  return MS_INT_VAL(0);
}

static MsValue nilEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  (void) a;
  return MS_BOOL_VAL(MS_IS_NIL(b));
}

struct MsType msNilType = {
    .name = "nil",
    .objSize = 0,  // singleton, no heap object
    .tpStr = nilStr,
    .tpRepr = nilStr,
    .tpHash = nilHash,
    .tpEq = nilEq,
};
