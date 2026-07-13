// ms_tuple.h -- msTupleType public declarations: MsTupleObj + constructor (type-system.md ss2.9)
#pragma once

#include <stdint.h>

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsVM;

// Immutable ordered sequence of MsValue (type-system.md ss2.9). head must be
// the first member (msGCAlloc/GC treat every heap object as struct
// MsObject*). items is an inline flexible array member -- unlike list, there
// is no separate items allocation, so there is no destroy callback.
struct MsTupleObj {
  struct MsObject head;
  uint32_t len;
  uint32_t hash;  // FNV-1a cache, 0 = not computed (same as struct MsStr)
  MsValue items[];
};

extern struct MsType msTupleType;

// Creates a tuple with len slots, zero-initialized. Caller must fill items[]
// completely before any allocation that could trigger GC; the zeroing
// guarantees tupleTraverse never reads a dangling pointer in the meantime.
MsValue msNewTuple(uint32_t len);

// tuple methods (type-system.md ss2.9); not yet wired to `.method()` call
// syntax (attribute/method dispatch lands in T073), exposed here as plain C
// functions for direct use and testing (same pattern as ms_list.h). len() is
// covered by tpLen directly, so only index/count are exposed here.
// Returns MS_INT_VAL(-1) if item is not found (not an error; same convention
// as msListIndex).
MsValue msTupleIndex(MsValue v, MsValue item, int64_t start);
MsValue msTupleCount(MsValue v, MsValue item);
