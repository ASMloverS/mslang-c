// ms_frozenset.c -- msFrozensetType definition: immutable hashable set, reuses MsSetObj/MsSetEntry (type-system.md
// ss2.11)
#include "mslang/ms_frozenset.h"

#include "mslang/ms_gc.h"
#include "mslang/ms_list.h"
#include "mslang/ms_object.h"
#include "mslang/ms_vm.h"

static bool isFrozenset(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msFrozensetType;
}

// Same layout as msNewSet (ms_set.c); retag as msFrozensetType (objSize
// matches, no allocation happens between the two calls, so no GC can
// observe the object mid-retag).
static MsValue newFrozenset(uint32_t initCap) {
  MsValue v = msNewSet(initCap);
  MS_AS_OBJ(v)->type = &msFrozensetType;
  return v;
}

// Inserts every live element of src into r, reusing each entry's already-known
// hash (msSetAddHashed) instead of re-hashing via msSetAdd (spec ss1).
static void frozensetInsertAll(MsValue r, struct MsSetObj* src) {
  struct MsSetObj* sr = (struct MsSetObj*) MS_AS_OBJ(r);
  for (uint32_t i = 0; i < src->cap; i++) {
    struct MsSetEntry* e = &src->entries[i];
    if (e->occupied) {
      msSetAddHashed(sr, e->item, e->hash);
    }
  }
}

// Inserts src's elements into r, keeping only those whose membership in
// other equals wantPresent (true -> intersect, false -> diff/symdiff). Reuses
// each entry's already-known hash for both the membership probe
// (msSetFindInSet) and the insert (msSetAddHashed), instead of re-hashing via
// msSetHas/msSetAdd.
static void frozensetFilterInto(MsValue r, struct MsSetObj* src, MsValue other, bool wantPresent) {
  struct MsSetObj* sr = (struct MsSetObj*) MS_AS_OBJ(r);
  struct MsSetObj* so = (struct MsSetObj*) MS_AS_OBJ(other);
  for (uint32_t i = 0; i < src->cap; i++) {
    struct MsSetEntry* e = &src->entries[i];
    if (!e->occupied) {
      continue;
    }
    struct MsSetEntry* oe = msSetFindInSet(so, e->item, e->hash);
    if ((oe && oe->occupied) == wantPresent) {
      msSetAddHashed(sr, e->item, e->hash);
    }
  }
}

MsValue msNewFrozensetFromIter(struct MsVM* vm, MsValue iterable) {
  if (!MS_IS_OBJ(iterable)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsType* tp = MS_AS_OBJ(iterable)->type;
  if (tp == &msListType) {
    struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(iterable);
    MsValue r = newFrozenset(l->len);
    msGCPushRoot(r);
    for (uint32_t i = 0; i < l->len; i++) {
      MsValue err = msSetAdd(vm, r, l->items[i]);
      if (MS_IS_ERROR(err)) {
        msGCPopRoot();
        return err;
      }
    }
    msGCPopRoot();
    return r;
  }
  if (tp == &msSetType || tp == &msFrozensetType) {
    struct MsSetObj* src = (struct MsSetObj*) MS_AS_OBJ(iterable);
    MsValue r = newFrozenset(src->len);
    msGCPushRoot(r);
    frozensetInsertAll(r, src);
    msGCPopRoot();
    return r;
  }
  return MS_ERROR_VALUE;  // TypeError: unsupported source (generic tpIter lands T065)
}

MsValue msFrozensetUnion(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  struct MsSetObj* sb = (struct MsSetObj*) MS_AS_OBJ(b);
  MsValue r = newFrozenset(sa->len + sb->len);
  msGCPushRoot(r);
  frozensetInsertAll(r, sa);
  frozensetInsertAll(r, sb);
  msGCPopRoot();
  return r;
}

MsValue msFrozensetIntersect(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  struct MsSetObj* sb = (struct MsSetObj*) MS_AS_OBJ(b);
  MsValue r = newFrozenset(sa->len < sb->len ? sa->len : sb->len);
  msGCPushRoot(r);
  frozensetFilterInto(r, sa, b, true);
  msGCPopRoot();
  return r;
}

MsValue msFrozensetDiff(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  MsValue r = newFrozenset(sa->len);
  msGCPushRoot(r);
  frozensetFilterInto(r, sa, b, false);
  msGCPopRoot();
  return r;
}

MsValue msFrozensetSymDiff(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  struct MsSetObj* sb = (struct MsSetObj*) MS_AS_OBJ(b);
  MsValue r = newFrozenset(sa->len + sb->len);
  msGCPushRoot(r);
  frozensetFilterInto(r, sa, b, false);
  frozensetFilterInto(r, sb, a, false);
  msGCPopRoot();
  return r;
}

static MsValue frozensetEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isFrozenset(b)) {
    return MS_BOOL_VAL(false);
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  struct MsSetObj* sb = (struct MsSetObj*) MS_AS_OBJ(b);
  if (sa->len != sb->len) {
    return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL(msSetIsSubset(sa, sb));
}

// frozenset <= frozenset -> subset; frozenset < frozenset -> proper subset
static MsValue frozensetLe(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;
  }
  return MS_BOOL_VAL(msSetIsSubset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

static MsValue frozensetLt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;
  }
  return MS_BOOL_VAL(msSetIsProperSubset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

// frozenset >= frozenset -> superset; frozenset > frozenset -> proper superset
// (without these, msValueGe/msValueGt would fall back to a total-order
// !(a<b)/!(a<=b), which is wrong for incomparable sets, e.g.
// frozenset({1}) >= frozenset({2}) would wrongly report true)
static MsValue frozensetGe(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;
  }
  return MS_BOOL_VAL(msSetIsSuperset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

static MsValue frozensetGt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;
  }
  return MS_BOOL_VAL(msSetIsProperSuperset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

// XOR aggregate over entry hashes (order-independent, fits set semantics);
// no cache field on struct MsSetObj (spec ss2), recomputed on every call.
static MsValue frozensetHash(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsSetObj* fs = (struct MsSetObj*) MS_AS_OBJ(v);
  uint32_t h = 0;
  for (uint32_t i = 0; i < fs->cap; i++) {
    if (fs->entries[i].occupied) {
      h ^= fs->entries[i].hash * 0x9e3779b9u;
    }
  }
  if (!h) {
    h = 1;
  }
  return MS_INT_VAL((int64_t) (uint32_t) h);
}

// tpIter deferred to T065 (same policy as msSetType). No add/remove slots:
// frozenset is immutable. traverse/destroy/tpLen have no set-vs-frozenset
// predicate to diverge on, so they're shared with msSetType (ms_set.h).
struct MsType msFrozensetType = {
    .name = "frozenset",
    .objSize = sizeof(struct MsSetObj),
    .traverse = msSetTraverse,
    .destroy = msSetDestroy,
    .tpLen = msSetLen,
    .tpEq = frozensetEq,
    .tpContains = msSetHas,
    .tpBitor = msFrozensetUnion,
    .tpSub = msFrozensetDiff,
    .tpBitand = msFrozensetIntersect,
    .tpBitxor = msFrozensetSymDiff,
    .tpLe = frozensetLe,
    .tpLt = frozensetLt,
    .tpGe = frozensetGe,
    .tpGt = frozensetGt,
    .tpHash = frozensetHash,
};

MsValue msFrozensetIsSubset(MsValue a, MsValue b) {
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  return MS_BOOL_VAL(msSetIsSubset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

MsValue msFrozensetIsSuperset(MsValue a, MsValue b) {
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  return MS_BOOL_VAL(msSetIsSuperset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

MsValue msFrozensetIsDisjoint(struct MsVM* vm, MsValue a, MsValue b) {
  if (!isFrozenset(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  for (uint32_t i = 0; i < sa->cap; i++) {
    struct MsSetEntry* e = &sa->entries[i];
    if (!e->occupied) {
      continue;
    }
    MsValue has = msSetHas(vm, b, e->item);
    if (MS_IS_ERROR(has)) {
      return has;
    }
    if (MS_AS_BOOL(has)) {
      return MS_BOOL_VAL(false);
    }
  }
  return MS_BOOL_VAL(true);
}

MsValue msFrozensetCopy(MsValue v) {
  return v;
}
