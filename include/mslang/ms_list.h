// ms_list.h -- msListType public declarations: MsListObj + constructors + methods (type-system.md ss2.7)
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsVM;

// Mutable dynamic array of MsValue (type-system.md ss2.7). head must be the
// first member (msGCAlloc/GC treat every heap object as struct MsObject*).
// items is an extra, non-GC-managed allocation (msAlloc/msRealloc); destroy
// frees it.
struct MsListObj {
  struct MsObject head;
  uint32_t len;
  uint32_t cap;
  MsValue* items;
};

extern struct MsType msListType;

// Create a list with initCap pre-allocated slots (len starts at 0).
MsValue msNewList(uint32_t initCap);

// append/insert return MS_NIL_VAL on success, MS_ERROR_VALUE (OverflowError)
// if growth would exceed UINT32_MAX capacity.
MsValue msListAppend(struct MsListObj* l, MsValue v);
// get/set support negative indices (Python-style wraparound); out-of-range
// returns MS_ERROR_VALUE (IndexError).
MsValue msListGet(struct MsListObj* l, int64_t i);
MsValue msListSet(struct MsListObj* l, int64_t i, MsValue v);
// insert clamps i into [0, len] (Python list.insert semantics; never IndexError).
MsValue msListInsert(struct MsListObj* l, int64_t i, MsValue v);
// pop removes and returns items[i] (negative i wraps); out-of-range returns
// MS_ERROR_VALUE (IndexError).
MsValue msListPop(struct MsListObj* l, int64_t i);

// list methods (type-system.md ss2.7); not yet wired to `.method()` call
// syntax (attribute/method dispatch lands in T073), exposed here as plain C
// functions for direct use and testing (same pattern as ms_str.h/ms_bytes.h).
MsValue msListRemove(MsValue v, MsValue item);  // ValueError if item is not found
// Returns MS_INT_VAL(-1) if item is not found (not an error; same convention
// as msStrIndex).
MsValue msListIndex(MsValue v, MsValue item, int64_t start);
MsValue msListCount(MsValue v, MsValue item);
// key function support deferred to T099 (function-call machinery not ready).
MsValue msListSort(MsValue v, bool reverse);
MsValue msListReverse(MsValue v);
// other must be a list object: generic iterable support (any object with
// tpIter) is deferred to T065, same narrowing as msBytesExtend.
MsValue msListExtend(MsValue v, MsValue other);
MsValue msListClear(MsValue v);
MsValue msListCopy(MsValue v);
