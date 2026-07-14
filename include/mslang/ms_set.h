// ms_set.h -- msSetType public declarations: MsSetObj + constructors + methods (type-system.md ss2.10)
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsVM;

// One hash-table slot. occupied == false + hash == MS_SET_TOMBSTONE_HASH
// marks a deleted slot (tombstone); occupied == false + hash == 0 marks a
// never-used slot (findSlot's probe-stop condition, ms_set.c). Same
// open-addressing/tombstone convention as struct MsMapEntry (ms_map.h), minus
// the value field.
struct MsSetEntry {
  MsValue item;
  uint32_t hash;
  bool occupied;
};

// Mutable open-addressing hash set (type-system.md ss2.10). head must be the
// first member (msGCAlloc/GC treat every heap object as struct MsObject*).
// entries is an extra, non-GC-managed allocation (msAlloc/msRealloc); destroy
// frees it.
struct MsSetObj {
  struct MsObject head;
  uint32_t len;         // live element count
  uint32_t cap;         // total slot count (always a power of 2)
  uint32_t tombstones;  // deleted-slot count (feeds the load-factor check)
  struct MsSetEntry* entries;
};

extern struct MsType msSetType;

// Create a set with capacity rounded up to a power of 2 (minimum 8) that can
// hold at least initCap elements before the first resize.
MsValue msNewSet(uint32_t initCap);

// Element hashing errors return MS_ERROR_VALUE (TypeError, T080 placeholder,
// same convention as msMapSet/msMapDel).
MsValue msSetAdd(struct MsVM* vm, MsValue setVal, MsValue v);      // add (idempotent)
MsValue msSetHas(struct MsVM* vm, MsValue setVal, MsValue v);      // membership test
MsValue msSetRemove(struct MsVM* vm, MsValue setVal, MsValue v);   // remove; absent -> KeyError
MsValue msSetDiscard(struct MsVM* vm, MsValue setVal, MsValue v);  // remove; absent -> silently ignored

// Set operations (type-system.md ss2.10); other must be a set object (generic
// iterable support deferred to T065, same narrowing as msMapUpdate).
MsValue msSetUnion(struct MsVM* vm, MsValue a, MsValue b);      // a | b
MsValue msSetIntersect(struct MsVM* vm, MsValue a, MsValue b);  // a & b
MsValue msSetDiff(struct MsVM* vm, MsValue a, MsValue b);       // a - b
MsValue msSetSymDiff(struct MsVM* vm, MsValue a, MsValue b);    // a ^ b

// Relational operations (type-system.md ss2.10).
bool msSetIsSubset(struct MsSetObj* a, struct MsSetObj* b);          // a <= b
bool msSetIsProperSubset(struct MsSetObj* a, struct MsSetObj* b);    // a < b
bool msSetIsSuperset(struct MsSetObj* a, struct MsSetObj* b);        // a >= b
bool msSetIsProperSuperset(struct MsSetObj* a, struct MsSetObj* b);  // a > b

// set methods (type-system.md ss2.10); not yet wired to `.method()` call
// syntax (attribute/method dispatch lands in T073), exposed here as plain C
// functions for direct use and testing (same pattern as ms_map.h).
MsValue msSetPop(struct MsVM* vm, MsValue v);   // .pop(); empty set -> KeyError
MsValue msSetClear(MsValue v);                  // .clear()
MsValue msSetCopy(MsValue v);                   // .copy()
MsValue msSetIsDisjoint(MsValue a, MsValue b);  // .isDisjoint(other); other not a set -> TypeError

// In-place variants (|= / &= / -= / ^=); other must be a set object.
MsValue msSetUpdate(struct MsVM* vm, MsValue v, MsValue other);
MsValue msSetIntersectionUpdate(struct MsVM* vm, MsValue v, MsValue other);
MsValue msSetDifferenceUpdate(struct MsVM* vm, MsValue v, MsValue other);
MsValue msSetSymmetricDifferenceUpdate(struct MsVM* vm, MsValue v, MsValue other);
