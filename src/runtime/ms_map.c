// ms_map.c -- msMapType definition: open-addressing hash table runtime type (type-system.md ss2.8)
#include "mslang/ms_map.h"

#include <math.h>
#include <string.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_list.h"
#include "mslang/ms_object.h"
#include "mslang/ms_vm.h"

#define MS_MAP_TOMBSTONE_HASH UINT32_MAX  // 仅在 !occupied 时用作 tombstone 标记

static bool isMap(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msMapType;
}

// tpHash dispatch: *ok is false when v's type has no tpHash (unhashable, e.g.
// list/map) or v is a NaN float (nan is not a legal map key, type-system.md
// ss2.8).
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

// Finds the slot for key/hash: a live match, the first reusable tombstone
// probed along the way, or the first never-used slot (probe terminator).
// cap must be > 0 (msNewMap guarantees cap >= 8 for every live map).
static struct MsMapEntry* findSlot(struct MsMapEntry* entries, uint32_t cap, MsValue key, uint32_t hash) {
  uint32_t idx = hash & (cap - 1);
  struct MsMapEntry* tombstone = NULL;
  for (;;) {
    struct MsMapEntry* e = &entries[idx];
    if (!e->occupied) {
      if (e->hash == MS_MAP_TOMBSTONE_HASH) {
        if (!tombstone) {
          tombstone = e;
        }
      } else {
        return tombstone ? tombstone : e;
      }
    } else if (e->hash == hash && msValueEqual(e->key, key)) {
      return e;
    }
    idx = (idx + 1) & (cap - 1);
  }
}

// Hashes key and probes for its slot; NULL means either key is absent from a
// logically-empty table (m->len == 0, skips the probe entirely) or key was
// not found in a non-empty one. *ok is false (TypeError, T080 placeholder)
// when key is unhashable; hashOut, if non-NULL, still receives the computed
// hash so callers that go on to insert (msMapSetDefault) don't re-hash.
// Shared by every read-oriented lookup (get/del/getitem/has/getDefault/pop);
// msMapSet/msMapSetDefault's write path needs its own findSlot call after a
// possible resize, so they use mapSetHashed instead.
static struct MsMapEntry* lookupEntry(struct MsVM* vm, struct MsMapObj* m, MsValue key, uint32_t* hashOut, bool* ok) {
  uint32_t hash = hashValue(vm, key, ok);
  if (!*ok) {
    return NULL;
  }
  if (hashOut) {
    *hashOut = hash;
  }
  return m->len ? findSlot(m->entries, m->cap, key, hash) : NULL;
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
// msNewMap and msMapResize.
static struct MsMapEntry* allocZeroedEntries(uint32_t cap) {
  struct MsMapEntry* entries = MS_ALLOC_N(struct MsMapEntry, cap);
  memset(entries, 0, (size_t) cap * sizeof(struct MsMapEntry));
  return entries;
}

MsValue msNewMap(uint32_t initCap) {
  struct MsObject* obj = msGCAlloc(&msMapType, sizeof(struct MsMapObj));
  struct MsMapObj* m = (struct MsMapObj*) obj;
  m->len = 0;
  m->cap = roundUpPow2(initCap);
  m->tombstones = 0;
  m->entries = allocZeroedEntries(m->cap);
  return MS_OBJ_VAL(m);
}

// Rebuilds entries at newCap, rehashing every live pair (tombstones are
// dropped in the process, per the "rehash 时 tombstone" risk note).
static void msMapResize(struct MsMapObj* m, uint32_t newCap) {
  struct MsMapEntry* oldEntries = m->entries;
  uint32_t oldCap = m->cap;
  struct MsMapEntry* newEntries = allocZeroedEntries(newCap);
  for (uint32_t i = 0; i < oldCap; i++) {
    struct MsMapEntry* e = &oldEntries[i];
    if (e->occupied) {
      struct MsMapEntry* slot = findSlot(newEntries, newCap, e->key, e->hash);
      slot->key = e->key;
      slot->value = e->value;
      slot->hash = e->hash;
      slot->occupied = true;
    }
  }
  msFree(oldEntries);
  m->entries = newEntries;
  m->cap = newCap;
  m->tombstones = 0;
}

MsValue msMapGet(struct MsVM* vm, MsValue mapVal, MsValue key) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(mapVal);
  bool ok;
  struct MsMapEntry* e = lookupEntry(vm, m, key, NULL, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  return (e && e->occupied) ? e->value : MS_NIL_VAL;
}

// Inserts key/val using a hash the caller already computed (msMapSet right
// after hashValue; msMapSetDefault reuses the hash from its lookupEntry
// probe instead of re-hashing key on the write path).
static void mapSetHashed(struct MsMapObj* m, MsValue key, MsValue val, uint32_t hash) {
  if ((m->len + m->tombstones + 1) * 4 > m->cap * 3) {
    msMapResize(m, m->cap < 8 ? 8 : m->cap * 2);
  }
  struct MsMapEntry* e = findSlot(m->entries, m->cap, key, hash);
  bool isNew = !e->occupied;
  if (isNew && e->hash == MS_MAP_TOMBSTONE_HASH) {
    m->tombstones--;
  }
  e->key = key;
  e->value = val;
  e->hash = hash;
  e->occupied = true;
  if (isNew) {
    m->len++;
  }
}

MsValue msMapSet(struct MsVM* vm, MsValue mapVal, MsValue key, MsValue val) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(mapVal);
  bool ok;
  uint32_t hash = hashValue(vm, key, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  mapSetHashed(m, key, val, hash);
  return MS_NIL_VAL;
}

MsValue msMapDel(struct MsVM* vm, MsValue mapVal, MsValue key) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(mapVal);
  bool ok;
  struct MsMapEntry* e = lookupEntry(vm, m, key, NULL, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  if (!e || !e->occupied) {
    return MS_ERROR_VALUE;  // KeyError (T080 placeholder)
  }
  e->occupied = false;
  e->hash = MS_MAP_TOMBSTONE_HASH;
  m->len--;
  m->tombstones++;
  return MS_NIL_VAL;
}

static MsValue mapLen(struct MsVM* vm, MsValue v) {
  (void) vm;
  return MS_INT_VAL((int64_t) ((struct MsMapObj*) MS_AS_OBJ(v))->len);
}

static MsValue mapEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isMap(b)) {
    return MS_BOOL_VAL(false);
  }
  struct MsMapObj* ma = (struct MsMapObj*) MS_AS_OBJ(a);
  struct MsMapObj* mb = (struct MsMapObj*) MS_AS_OBJ(b);
  if (ma->len != mb->len) {
    return MS_BOOL_VAL(false);
  }
  for (uint32_t i = 0; i < ma->cap; i++) {
    struct MsMapEntry* e = &ma->entries[i];
    if (!e->occupied) {
      continue;
    }
    struct MsMapEntry* be = mb->len ? findSlot(mb->entries, mb->cap, e->key, e->hash) : NULL;
    if (!be || !be->occupied || !msValueEqual(be->value, e->value)) {
      return MS_BOOL_VAL(false);
    }
  }
  return MS_BOOL_VAL(true);
}

// m[key]: unlike msMapGet/.get(), a missing key is a KeyError, not nil.
static MsValue mapGetItem(struct MsVM* vm, MsValue mapVal, MsValue key) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(mapVal);
  bool ok;
  struct MsMapEntry* e = lookupEntry(vm, m, key, NULL, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  if (!e || !e->occupied) {
    return MS_ERROR_VALUE;  // KeyError (T080 placeholder)
  }
  return e->value;
}

// key in map / .has(key): unhashable key -> TypeError, same as mapGetItem.
MsValue msMapHas(struct MsVM* vm, MsValue v, MsValue key) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  bool ok;
  struct MsMapEntry* e = lookupEntry(vm, m, key, NULL, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  return MS_BOOL_VAL(e && e->occupied);
}

// traverse: visit every live slot's key/value (address, not value) so a
// future moving GC (Cheney copying, T116) can rewrite it in place.
static void mapTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsMapObj* m = (struct MsMapObj*) obj;
  for (uint32_t i = 0; i < m->cap; i++) {
    struct MsMapEntry* e = &m->entries[i];
    if (e->occupied) {
      visit(&e->key, ctx);
      visit(&e->value, ctx);
    }
  }
}

static void mapDestroy(struct MsObject* obj) {
  msFree(((struct MsMapObj*) obj)->entries);
}

struct MsType msMapType = {
    .name = "map",
    .objSize = sizeof(struct MsMapObj),
    .traverse = mapTraverse,
    .destroy = mapDestroy,
    .tpLen = mapLen,
    .tpEq = mapEq,
    .tpGetitem = mapGetItem,
    .tpSetitem = msMapSet,
    .tpContains = msMapHas,
};

MsValue msMapGetDefault(struct MsVM* vm, MsValue v, MsValue key, MsValue def) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  bool ok;
  struct MsMapEntry* e = lookupEntry(vm, m, key, NULL, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  return (e && e->occupied) ? e->value : def;
}

MsValue msMapKeys(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MsValue r = msNewList(m->len);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(r);
  for (uint32_t i = 0; i < m->cap; i++) {
    if (m->entries[i].occupied) {
      l->items[l->len++] = m->entries[i].key;
    }
  }
  return r;
}

MsValue msMapValues(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MsValue r = msNewList(m->len);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(r);
  for (uint32_t i = 0; i < m->cap; i++) {
    if (m->entries[i].occupied) {
      l->items[l->len++] = m->entries[i].value;
    }
  }
  return r;
}

// tuple is T061 (after this task); each pair is a 2-element [k, v] list
// until tuple lands.
MsValue msMapItems(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MsValue r = msNewList(m->len);
  // r isn't reachable from any root yet (map itself is, via the VM stack, but
  // r is only held by this C local); msNewList(2) below can trigger GC on
  // later iterations, so r must be pushed as an explicit root for the loop.
  msGCPushRoot(r);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(r);
  for (uint32_t i = 0; i < m->cap; i++) {
    struct MsMapEntry* e = &m->entries[i];
    if (!e->occupied) {
      continue;
    }
    MsValue pair = msNewList(2);
    struct MsListObj* pl = (struct MsListObj*) MS_AS_OBJ(pair);
    pl->items[0] = e->key;
    pl->items[1] = e->value;
    pl->len = 2;
    l->items[l->len++] = pair;
  }
  msGCPopRoot();
  return r;
}

MsValue msMapPop(struct MsVM* vm, MsValue v, MsValue key, MsValue def) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  bool ok;
  struct MsMapEntry* e = lookupEntry(vm, m, key, NULL, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  if (!e || !e->occupied) {
    return def;
  }
  MsValue val = e->value;
  e->occupied = false;
  e->hash = MS_MAP_TOMBSTONE_HASH;
  m->len--;
  m->tombstones++;
  return val;
}

MsValue msMapUpdate(struct MsVM* vm, MsValue v, MsValue other) {
  if (!isMap(other)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsMapObj* o = (struct MsMapObj*) MS_AS_OBJ(other);
  for (uint32_t i = 0; i < o->cap; i++) {
    struct MsMapEntry* e = &o->entries[i];
    if (e->occupied) {
      MsValue r = msMapSet(vm, v, e->key, e->value);
      if (MS_IS_ERROR(r)) {
        return r;
      }
    }
  }
  return MS_NIL_VAL;
}

MsValue msMapClear(MsValue v) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  memset(m->entries, 0, (size_t) m->cap * sizeof(struct MsMapEntry));
  m->len = 0;
  m->tombstones = 0;
  return MS_NIL_VAL;
}

MsValue msMapCopy(MsValue v) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  MsValue r = msNewMap(m->cap);
  struct MsMapObj* mr = (struct MsMapObj*) MS_AS_OBJ(r);
  memcpy(mr->entries, m->entries, (size_t) m->cap * sizeof(struct MsMapEntry));
  mr->len = m->len;
  mr->tombstones = m->tombstones;
  return r;
}

MsValue msMapSetDefault(struct MsVM* vm, MsValue v, MsValue key, MsValue val) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(v);
  bool ok;
  uint32_t hash;
  struct MsMapEntry* e = lookupEntry(vm, m, key, &hash, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  if (e && e->occupied) {
    return e->value;
  }
  mapSetHashed(m, key, val, hash);
  return val;
}
