// ms_set.c -- msSetType definition: open-addressing hash set runtime type (type-system.md ss2.10)
#include "mslang/ms_set.h"

#include <math.h>
#include <string.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_object.h"
#include "mslang/ms_vm.h"

#define MS_SET_TOMBSTONE_HASH UINT32_MAX  // 仅在 !occupied 时用作 tombstone 标记

static bool isSet(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msSetType;
}

// tpHash dispatch: *ok is false when v's type has no tpHash (unhashable, e.g.
// list/map) or v is a NaN float (nan is not a legal set element, same rule as
// map keys, type-system.md ss2.8/ss2.10). Same logic as ms_map.c's hashValue.
static uint32_t hashValue(struct MsVM* vm, MsValue v, bool* ok) {
  struct MsType* tp = msTypeOf(v);
  if (!tp || !tp->tpHash) {
    *ok = false;
    return 0;
  }
  if (MS_IS_FLOAT(v) && isnan(MS_AS_FLOAT(v))) {
    *ok = false;
    return 0;
  }
  MsValue h = tp->tpHash(vm, v);
  *ok = true;
  return MS_IS_INT(h) ? (uint32_t) (uint64_t) MS_AS_INT(h) : 0;
}

// Finds the slot for item/hash: a live match, the first reusable tombstone
// probed along the way, or the first never-used slot (probe terminator).
// cap must be > 0 (msNewSet guarantees cap >= 8 for every live set).
static struct MsSetEntry* findSlot(struct MsSetEntry* entries, uint32_t cap, MsValue item, uint32_t hash) {
  uint32_t idx = hash & (cap - 1);
  struct MsSetEntry* tombstone = NULL;
  for (;;) {
    struct MsSetEntry* e = &entries[idx];
    if (!e->occupied) {
      if (e->hash == MS_SET_TOMBSTONE_HASH) {
        if (!tombstone) {
          tombstone = e;
        }
      } else {
        return tombstone ? tombstone : e;
      }
    } else if (e->hash == hash && msValueEqual(e->item, item)) {
      return e;
    }
    idx = (idx + 1) & (cap - 1);
  }
}

// Hashes item and probes for its slot; NULL means either item is absent from
// a logically-empty table (s->len == 0, skips the probe entirely) or item was
// not found in a non-empty one. *ok is false (TypeError, T080 placeholder)
// when item is unhashable.
static struct MsSetEntry* lookupEntry(struct MsVM* vm, struct MsSetObj* s, MsValue item, bool* ok) {
  uint32_t hash = hashValue(vm, item, ok);
  if (!*ok) {
    return NULL;
  }
  return s->len ? findSlot(s->entries, s->cap, item, hash) : NULL;
}

// Probes s for item/hash without hashing (caller already has hash from its
// own entry); used by the union/diff/intersect/subset/disjoint/update family
// to test membership in the *other* set. NULL means s is empty or item is
// absent.
static struct MsSetEntry* findInSet(struct MsSetObj* s, MsValue item, uint32_t hash) {
  return s->len ? findSlot(s->entries, s->cap, item, hash) : NULL;
}

static uint32_t roundUpPow2(uint32_t n) {
  uint32_t p = 8;
  while (p < n) {
    p *= 2;
  }
  return p;
}

// msAlloc does not zero memory (unlike msGCAlloc); findSlot's "never-used
// slot" probe terminator relies on occupied == false && hash == 0. Shared by
// msNewSet and msSetResize.
static struct MsSetEntry* allocZeroedEntries(uint32_t cap) {
  struct MsSetEntry* entries = MS_ALLOC_N(struct MsSetEntry, cap);
  memset(entries, 0, (size_t) cap * sizeof(struct MsSetEntry));
  return entries;
}

MsValue msNewSet(uint32_t initCap) {
  struct MsObject* obj = msGCAlloc(&msSetType, sizeof(struct MsSetObj));
  struct MsSetObj* s = (struct MsSetObj*) obj;
  s->len = 0;
  s->cap = roundUpPow2(initCap);
  s->tombstones = 0;
  s->entries = allocZeroedEntries(s->cap);
  return MS_OBJ_VAL(s);
}

// Rebuilds entries at newCap, rehashing every live element (tombstones are
// dropped in the process, same as ms_map.c's msMapResize).
static void msSetResize(struct MsSetObj* s, uint32_t newCap) {
  struct MsSetEntry* oldEntries = s->entries;
  uint32_t oldCap = s->cap;
  struct MsSetEntry* newEntries = allocZeroedEntries(newCap);
  for (uint32_t i = 0; i < oldCap; i++) {
    struct MsSetEntry* e = &oldEntries[i];
    if (e->occupied) {
      struct MsSetEntry* slot = findSlot(newEntries, newCap, e->item, e->hash);
      slot->item = e->item;
      slot->hash = e->hash;
      slot->occupied = true;
    }
  }
  msFree(oldEntries);
  s->entries = newEntries;
  s->cap = newCap;
  s->tombstones = 0;
}

// Inserts item using a hash the caller already computed (the union/diff/
// symdiff/intersect/update family reuse a source entry's hash instead of
// re-hashing on insert); same pattern as ms_map.c's mapSetHashed.
static void setAddHashed(struct MsSetObj* s, MsValue item, uint32_t hash) {
  if ((s->len + s->tombstones + 1) * 4 > s->cap * 3) {
    msSetResize(s, s->cap < 8 ? 8 : s->cap * 2);
  }
  struct MsSetEntry* e = findSlot(s->entries, s->cap, item, hash);
  bool isNew = !e->occupied;
  if (isNew && e->hash == MS_SET_TOMBSTONE_HASH) {
    s->tombstones--;
  }
  e->item = item;
  e->hash = hash;
  e->occupied = true;
  if (isNew) {
    s->len++;
  }
}

MsValue msSetAdd(struct MsVM* vm, MsValue setVal, MsValue v) {
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(setVal);
  bool ok;
  uint32_t hash = hashValue(vm, v, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  setAddHashed(s, v, hash);
  return MS_NIL_VAL;
}

MsValue msSetHas(struct MsVM* vm, MsValue setVal, MsValue v) {
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(setVal);
  bool ok;
  struct MsSetEntry* e = lookupEntry(vm, s, v, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  return MS_BOOL_VAL(e && e->occupied);
}

MsValue msSetRemove(struct MsVM* vm, MsValue setVal, MsValue v) {
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(setVal);
  bool ok;
  struct MsSetEntry* e = lookupEntry(vm, s, v, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  if (!e || !e->occupied) {
    return MS_ERROR_VALUE;  // KeyError (T080 placeholder)
  }
  e->occupied = false;
  e->hash = MS_SET_TOMBSTONE_HASH;
  s->len--;
  s->tombstones++;
  return MS_NIL_VAL;
}

MsValue msSetDiscard(struct MsVM* vm, MsValue setVal, MsValue v) {
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(setVal);
  bool ok;
  struct MsSetEntry* e = lookupEntry(vm, s, v, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  if (e && e->occupied) {
    e->occupied = false;
    e->hash = MS_SET_TOMBSTONE_HASH;
    s->len--;
    s->tombstones++;
  }
  return MS_NIL_VAL;
}

MsValue msSetUnion(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isSet(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  struct MsSetObj* sb = (struct MsSetObj*) MS_AS_OBJ(b);
  MsValue r = msNewSet(sa->len + sb->len);
  msGCPushRoot(r);
  struct MsSetObj* sr = (struct MsSetObj*) MS_AS_OBJ(r);
  for (uint32_t i = 0; i < sa->cap; i++) {
    struct MsSetEntry* e = &sa->entries[i];
    if (e->occupied) {
      setAddHashed(sr, e->item, e->hash);
    }
  }
  for (uint32_t i = 0; i < sb->cap; i++) {
    struct MsSetEntry* e = &sb->entries[i];
    if (e->occupied) {
      setAddHashed(sr, e->item, e->hash);
    }
  }
  msGCPopRoot();
  return r;
}

MsValue msSetDiff(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isSet(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  struct MsSetObj* sb = (struct MsSetObj*) MS_AS_OBJ(b);
  MsValue r = msNewSet(sa->len);
  msGCPushRoot(r);
  struct MsSetObj* sr = (struct MsSetObj*) MS_AS_OBJ(r);
  for (uint32_t i = 0; i < sa->cap; i++) {
    struct MsSetEntry* e = &sa->entries[i];
    if (!e->occupied) {
      continue;
    }
    struct MsSetEntry* be = findInSet(sb, e->item, e->hash);
    if (!be || !be->occupied) {
      setAddHashed(sr, e->item, e->hash);
    }
  }
  msGCPopRoot();
  return r;
}

MsValue msSetSymDiff(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isSet(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  struct MsSetObj* sb = (struct MsSetObj*) MS_AS_OBJ(b);
  MsValue r = msNewSet(sa->len + sb->len);
  msGCPushRoot(r);
  struct MsSetObj* sr = (struct MsSetObj*) MS_AS_OBJ(r);
  for (uint32_t i = 0; i < sa->cap; i++) {
    struct MsSetEntry* e = &sa->entries[i];
    if (!e->occupied) {
      continue;
    }
    struct MsSetEntry* be = findInSet(sb, e->item, e->hash);
    if (!be || !be->occupied) {
      setAddHashed(sr, e->item, e->hash);
    }
  }
  for (uint32_t i = 0; i < sb->cap; i++) {
    struct MsSetEntry* e = &sb->entries[i];
    if (!e->occupied) {
      continue;
    }
    struct MsSetEntry* ae = findInSet(sa, e->item, e->hash);
    if (!ae || !ae->occupied) {
      setAddHashed(sr, e->item, e->hash);
    }
  }
  msGCPopRoot();
  return r;
}

MsValue msSetIntersect(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isSet(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  struct MsSetObj* sb = (struct MsSetObj*) MS_AS_OBJ(b);
  MsValue r = msNewSet(sa->len < sb->len ? sa->len : sb->len);
  msGCPushRoot(r);
  struct MsSetObj* sr = (struct MsSetObj*) MS_AS_OBJ(r);
  for (uint32_t i = 0; i < sa->cap; i++) {
    struct MsSetEntry* e = &sa->entries[i];
    if (!e->occupied) {
      continue;
    }
    struct MsSetEntry* be = findInSet(sb, e->item, e->hash);
    if (be && be->occupied) {
      setAddHashed(sr, e->item, e->hash);
    }
  }
  msGCPopRoot();
  return r;
}

bool msSetIsSubset(struct MsSetObj* a, struct MsSetObj* b) {
  if (a->len > b->len) {
    return false;
  }
  for (uint32_t i = 0; i < a->cap; i++) {
    struct MsSetEntry* e = &a->entries[i];
    if (!e->occupied) {
      continue;
    }
    struct MsSetEntry* be = findInSet(b, e->item, e->hash);
    if (!be || !be->occupied) {
      return false;
    }
  }
  return true;
}

bool msSetIsProperSubset(struct MsSetObj* a, struct MsSetObj* b) {
  return a->len < b->len && msSetIsSubset(a, b);
}

bool msSetIsSuperset(struct MsSetObj* a, struct MsSetObj* b) {
  return msSetIsSubset(b, a);
}

bool msSetIsProperSuperset(struct MsSetObj* a, struct MsSetObj* b) {
  return msSetIsProperSubset(b, a);
}

static MsValue setLen(struct MsVM* vm, MsValue v) {
  (void) vm;
  return MS_INT_VAL((int64_t) ((struct MsSetObj*) MS_AS_OBJ(v))->len);
}

static MsValue setEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isSet(b)) {
    return MS_BOOL_VAL(false);
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  struct MsSetObj* sb = (struct MsSetObj*) MS_AS_OBJ(b);
  if (sa->len != sb->len) {
    return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL(msSetIsSubset(sa, sb));
}

// set <= set -> subset; set < set -> proper subset
// set >= set -> superset; set > set -> proper superset
static MsValue setLe(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isSet(b)) {
    return MS_ERROR_VALUE;
  }
  return MS_BOOL_VAL(msSetIsSubset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

static MsValue setLt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isSet(b)) {
    return MS_ERROR_VALUE;
  }
  return MS_BOOL_VAL(msSetIsProperSubset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

static MsValue setGe(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isSet(b)) {
    return MS_ERROR_VALUE;
  }
  return MS_BOOL_VAL(msSetIsSuperset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

static MsValue setGt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isSet(b)) {
    return MS_ERROR_VALUE;
  }
  return MS_BOOL_VAL(msSetIsProperSuperset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

// traverse: visit every live slot's item (address, not value) so a future
// moving GC (Cheney copying, T116) can rewrite it in place. Same rationale as
// mapTraverse.
static void setTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsSetObj* s = (struct MsSetObj*) obj;
  for (uint32_t i = 0; i < s->cap; i++) {
    struct MsSetEntry* e = &s->entries[i];
    if (e->occupied) {
      visit(&e->item, ctx);
    }
  }
}

static void setDestroy(struct MsObject* obj) {
  msFree(((struct MsSetObj*) obj)->entries);
}

// tpIter deferred to T065 (same policy as ms_list.c's msListType).
struct MsType msSetType = {
    .name = "set",
    .objSize = sizeof(struct MsSetObj),
    .traverse = setTraverse,
    .destroy = setDestroy,
    .tpLen = setLen,
    .tpEq = setEq,
    .tpContains = msSetHas,
    .tpBitor = msSetUnion,
    .tpSub = msSetDiff,
    .tpBitand = msSetIntersect,
    .tpBitxor = msSetSymDiff,
    .tpLe = setLe,
    .tpLt = setLt,
    .tpGe = setGe,
    .tpGt = setGt,
};

MsValue msSetPop(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  for (uint32_t i = 0; i < s->cap; i++) {
    struct MsSetEntry* e = &s->entries[i];
    if (e->occupied) {
      MsValue item = e->item;
      e->occupied = false;
      e->hash = MS_SET_TOMBSTONE_HASH;
      s->len--;
      s->tombstones++;
      return item;
    }
  }
  return MS_ERROR_VALUE;  // KeyError: pop from empty set (T080 placeholder)
}

MsValue msSetClear(MsValue v) {
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  memset(s->entries, 0, (size_t) s->cap * sizeof(struct MsSetEntry));
  s->len = 0;
  s->tombstones = 0;
  return MS_NIL_VAL;
}

MsValue msSetCopy(MsValue v) {
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  MsValue r = msNewSet(s->cap);
  struct MsSetObj* sr = (struct MsSetObj*) MS_AS_OBJ(r);
  memcpy(sr->entries, s->entries, (size_t) s->cap * sizeof(struct MsSetEntry));
  sr->len = s->len;
  sr->tombstones = s->tombstones;
  return r;
}

MsValue msSetIsDisjoint(MsValue a, MsValue b) {
  if (!isSet(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* sa = (struct MsSetObj*) MS_AS_OBJ(a);
  struct MsSetObj* sb = (struct MsSetObj*) MS_AS_OBJ(b);
  for (uint32_t i = 0; i < sa->cap; i++) {
    struct MsSetEntry* e = &sa->entries[i];
    if (!e->occupied) {
      continue;
    }
    struct MsSetEntry* be = findInSet(sb, e->item, e->hash);
    if (be && be->occupied) {
      return MS_BOOL_VAL(false);
    }
  }
  return MS_BOOL_VAL(true);
}

MsValue msSetUpdate(struct MsVM* vm, MsValue v, MsValue other) {
  (void) vm;
  if (!isSet(other)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  struct MsSetObj* o = (struct MsSetObj*) MS_AS_OBJ(other);
  for (uint32_t i = 0; i < o->cap; i++) {
    struct MsSetEntry* e = &o->entries[i];
    if (e->occupied) {
      setAddHashed(s, e->item, e->hash);
    }
  }
  return MS_NIL_VAL;
}

MsValue msSetIntersectionUpdate(struct MsVM* vm, MsValue v, MsValue other) {
  (void) vm;
  if (!isSet(other)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  struct MsSetObj* o = (struct MsSetObj*) MS_AS_OBJ(other);
  for (uint32_t i = 0; i < s->cap; i++) {
    struct MsSetEntry* e = &s->entries[i];
    if (!e->occupied) {
      continue;
    }
    struct MsSetEntry* oe = findInSet(o, e->item, e->hash);
    if (!oe || !oe->occupied) {
      e->occupied = false;
      e->hash = MS_SET_TOMBSTONE_HASH;
      s->len--;
      s->tombstones++;
    }
  }
  return MS_NIL_VAL;
}

MsValue msSetDifferenceUpdate(struct MsVM* vm, MsValue v, MsValue other) {
  (void) vm;
  if (!isSet(other)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* s = (struct MsSetObj*) MS_AS_OBJ(v);
  struct MsSetObj* o = (struct MsSetObj*) MS_AS_OBJ(other);
  for (uint32_t i = 0; i < s->cap; i++) {
    struct MsSetEntry* e = &s->entries[i];
    if (!e->occupied) {
      continue;
    }
    struct MsSetEntry* oe = findInSet(o, e->item, e->hash);
    if (oe && oe->occupied) {
      e->occupied = false;
      e->hash = MS_SET_TOMBSTONE_HASH;
      s->len--;
      s->tombstones++;
    }
  }
  return MS_NIL_VAL;
}

MsValue msSetSymmetricDifferenceUpdate(struct MsVM* vm, MsValue v, MsValue other) {
  if (!isSet(other)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsSetObj* o = (struct MsSetObj*) MS_AS_OBJ(other);
  for (uint32_t i = 0; i < o->cap; i++) {
    struct MsSetEntry* e = &o->entries[i];
    if (!e->occupied) {
      continue;
    }
    MsValue hasR = msSetHas(vm, v, e->item);
    if (MS_IS_ERROR(hasR)) {
      return hasR;
    }
    MsValue r = MS_AS_BOOL(hasR) ? msSetDiscard(vm, v, e->item) : msSetAdd(vm, v, e->item);
    if (MS_IS_ERROR(r)) {
      return r;
    }
  }
  return MS_NIL_VAL;
}
