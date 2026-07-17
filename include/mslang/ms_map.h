// ms_map.h -- msMapType public declarations: MsMapObj + constructors + methods (type-system.md ss2.8)
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsVM;

// One hash-table slot. occupied == false + hash == MS_MAP_TOMBSTONE_HASH
// marks a deleted slot (tombstone); occupied == false + hash == 0 marks a
// never-used slot (findSlot's probe-stop condition, ms_map.c).
struct MsMapEntry {
  MsValue key;
  MsValue value;
  uint32_t hash;  // occupied 时为缓存 hash；!occupied 时复用为 tombstone 标记
  bool occupied;
};

// Mutable open-addressing hash table (type-system.md ss2.8). head must be
// the first member (msGCAlloc/GC treat every heap object as struct
// MsObject*). entries is an extra, non-GC-managed allocation (msAlloc/
// msRealloc); destroy frees it.
struct MsMapObj {
  struct MsObject head;
  uint32_t len;         // live key/value pair count
  uint32_t cap;         // total slot count (always a power of 2)
  uint32_t tombstones;  // deleted-slot count (feeds the load-factor check)
  struct MsMapEntry* entries;
};

extern struct MsType msMapType;

// Iterator over a map's keys (T065). map is stored as MsValue (not a raw
// MsMapObj*) so traverse can visit the slot in place, same convention as
// struct MsListIterObj's list field.
struct MsMapIterObj {
  struct MsObject head;
  MsValue map;
  uint32_t slotIdx;
};

extern struct MsType msMapIterType;

// Create a map with capacity rounded up to a power of 2 (minimum 8) that can
// hold at least initCap pairs before the first resize.
MsValue msNewMap(uint32_t initCap);

// key/val hashing/comparison errors return MS_ERROR_VALUE (TypeError, T080
// placeholder). msMapGet returns MS_NIL_VAL when key is absent (the `.get()`
// default-nil case); msMapDel returns MS_ERROR_VALUE (KeyError, T080
// placeholder) when key is absent.
MsValue msMapGet(struct MsVM* vm, MsValue mapVal, MsValue key);
MsValue msMapSet(struct MsVM* vm, MsValue mapVal, MsValue key, MsValue val);
MsValue msMapDel(struct MsVM* vm, MsValue mapVal, MsValue key);

// map methods (type-system.md ss2.8); not yet wired to `.method()` call
// syntax (attribute/method dispatch lands in T073), exposed here as plain C
// functions for direct use and testing (same pattern as ms_list.h).
MsValue msMapGetDefault(struct MsVM* vm, MsValue v, MsValue key, MsValue def);  // .get(key, default=nil)
// keys/values/items return a plain list (tuple is T061, after this task;
// items() uses a 2-element [k, v] list as the pair representation until
// tuple lands).
MsValue msMapKeys(struct MsVM* vm, MsValue v);
MsValue msMapValues(struct MsVM* vm, MsValue v);
MsValue msMapItems(struct MsVM* vm, MsValue v);
MsValue msMapHas(struct MsVM* vm, MsValue v, MsValue key);  // equivalent to `key in map`
// pop removes and returns the value for key, or def if key is absent (never
// an error; same default-driven semantics as msMapGetDefault).
MsValue msMapPop(struct MsVM* vm, MsValue v, MsValue key, MsValue def);
// other must be a map object (generic iterable-of-pairs support deferred to
// T065, same narrowing as msListExtend).
MsValue msMapUpdate(struct MsVM* vm, MsValue v, MsValue other);
MsValue msMapClear(MsValue v);
MsValue msMapCopy(MsValue v);
MsValue msMapSetDefault(struct MsVM* vm, MsValue v, MsValue key, MsValue val);
