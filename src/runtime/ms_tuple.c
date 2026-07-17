// ms_tuple.c -- msTupleType definition: immutable inline-array runtime type (type-system.md ss2.9)
#include "mslang/ms_tuple.h"

#include <string.h>

#include "mslang/ms_gc.h"
#include "mslang/ms_hash.h"
#include "mslang/ms_object.h"
#include "mslang/ms_slice.h"
#include "mslang/ms_vm.h"

static size_t tupleVarSize(const struct MsObject* obj) {
  return sizeof(struct MsTupleObj) + ((const struct MsTupleObj*) obj)->len * sizeof(MsValue);
}

// msGCAlloc zero-fills the whole allocation (items[] included), so items[]
// is already GC-safe here without an explicit memset.
MsValue msNewTuple(uint32_t len) {
  size_t size = sizeof(struct MsTupleObj) + (size_t) len * sizeof(MsValue);
  struct MsTupleObj* t = (struct MsTupleObj*) msGCAlloc(&msTupleType, size);
  t->len = len;
  t->hash = 0;
  return MS_OBJ_VAL(t);
}

static bool isTuple(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msTupleType;
}

static MsValue tupleLen(struct MsVM* vm, MsValue v) {
  (void) vm;
  return MS_INT_VAL((int64_t) ((struct MsTupleObj*) MS_AS_OBJ(v))->len);
}

static MsValue tupleEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isTuple(b)) {
    return MS_BOOL_VAL(false);
  }
  struct MsTupleObj* ta = (struct MsTupleObj*) MS_AS_OBJ(a);
  struct MsTupleObj* tb = (struct MsTupleObj*) MS_AS_OBJ(b);
  if (ta->len != tb->len) {
    return MS_BOOL_VAL(false);
  }
  for (uint32_t i = 0; i < ta->len; i++) {
    if (!msValueEqual(ta->items[i], tb->items[i])) {
      return MS_BOOL_VAL(false);
    }
  }
  return MS_BOOL_VAL(true);
}

// Lexicographic comparison (same approach as strLt): compares element-wise
// up to the first mismatch; incomparable elements -> TypeError; equal common
// prefix -> shorter tuple is less.
static MsValue tupleLt(struct MsVM* vm, MsValue a, MsValue b) {
  if (!isTuple(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsTupleObj* ta = (struct MsTupleObj*) MS_AS_OBJ(a);
  struct MsTupleObj* tb = (struct MsTupleObj*) MS_AS_OBJ(b);
  uint32_t n = ta->len < tb->len ? ta->len : tb->len;
  for (uint32_t i = 0; i < n; i++) {
    if (msValueEqual(ta->items[i], tb->items[i])) {
      continue;
    }
    struct MsType* tp = msTypeOf(ta->items[i]);
    if (!tp->tpLt) {
      return MS_ERROR_VALUE;  // TypeError: element not comparable
    }
    return tp->tpLt(vm, ta->items[i], tb->items[i]);
  }
  return MS_BOOL_VAL(ta->len < tb->len);
}

static MsValue tupleHash(struct MsVM* vm, MsValue v) {
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
  if (t->hash) {
    return MS_INT_VAL((int64_t) (uint32_t) t->hash);
  }

  // FNV-1a aggregating every element's hash
  uint32_t h = MS_FNV1A32_INIT;
  for (uint32_t i = 0; i < t->len; i++) {
    struct MsType* tp = msTypeOf(t->items[i]);
    if (!tp->tpHash) {
      return MS_ERROR_VALUE;  // TypeError: element not hashable
    }
    MsValue eh = tp->tpHash(vm, t->items[i]);
    if (MS_IS_ERROR(eh)) {
      return MS_ERROR_VALUE;
    }
    uint32_t ev = (uint32_t) (uint64_t) MS_AS_INT(eh);
    h = msFnv1a32Update(h, &ev, sizeof(ev));
  }
  if (!h) {
    h = 1;
  }
  t->hash = h;
  return MS_INT_VAL((int64_t) (uint32_t) h);
}

// tuple[a:b:step] -> new tuple (T065); msSliceCount sizes the result tuple
// exactly, so elements are only visited once (fill pass), same approach as
// ms_list.c's listGetSlice.
static MsValue tupleGetSlice(struct MsTupleObj* t, MsValue idx) {
  struct MsSliceObj* sl = (struct MsSliceObj*) MS_AS_OBJ(idx);
  if (msSliceStepIsZero(sl)) {
    return MS_ERROR_VALUE;  // ValueError: slice step cannot be zero (T080 placeholder)
  }
  int64_t start, stop, step;
  msSliceNormalize(sl, t->len, &start, &stop, &step);
  uint32_t n = (uint32_t) msSliceCount(start, stop, step);
  MsValue r = msNewTuple(n);
  struct MsTupleObj* tr = (struct MsTupleObj*) MS_AS_OBJ(r);
  msSliceFillValues(tr->items, t->items, start, stop, step);
  return r;
}

static MsValue tupleGetItem(struct MsVM* vm, MsValue v, MsValue idx) {
  (void) vm;
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
  if (msIsSlice(idx)) {
    return tupleGetSlice(t, idx);
  }
  if (!MS_IS_INT(idx)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  int64_t i = MS_AS_INT(idx);
  if (i < 0) {
    i += (int64_t) t->len;
  }
  if (i < 0 || i >= (int64_t) t->len) {
    return MS_ERROR_VALUE;  // IndexError (T080 placeholder)
  }
  return t->items[i];
}

static MsValue tupleContains(struct MsVM* vm, MsValue v, MsValue item) {
  (void) vm;
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
  for (uint32_t i = 0; i < t->len; i++) {
    if (msValueEqual(t->items[i], item)) {
      return MS_BOOL_VAL(true);
    }
  }
  return MS_BOOL_VAL(false);
}

// (1,2) + (3,) -> (1,2,3); same type-check/overflow-check approach as
// listConcat.
static MsValue tupleConcat(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isTuple(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsTupleObj* ta = (struct MsTupleObj*) MS_AS_OBJ(a);
  struct MsTupleObj* tb = (struct MsTupleObj*) MS_AS_OBJ(b);
  if ((uint64_t) ta->len + tb->len > UINT32_MAX) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  uint32_t newLen = ta->len + tb->len;
  MsValue r = msNewTuple(newLen);
  struct MsTupleObj* tr = (struct MsTupleObj*) MS_AS_OBJ(r);
  if (ta->len) {
    memcpy(tr->items, ta->items, ta->len * sizeof(MsValue));
  }
  if (tb->len) {
    memcpy(tr->items + ta->len, tb->items, tb->len * sizeof(MsValue));
  }
  return r;
}

// traverse: visit every slot's item (address, not value) so a future moving
// GC (Cheney copying, T116) can rewrite it in place. Same rationale as
// listTraverse.
static void tupleTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsTupleObj* t = (struct MsTupleObj*) obj;
  for (uint32_t i = 0; i < t->len; i++) {
    visit(&t->items[i], ctx);
  }
}

// Iterator over a tuple (T065); tuple field is stored as MsValue so traverse
// can visit the slot in place, same convention as ms_list.c's MsListIterObj.
struct MsTupleIterObj {
  struct MsObject head;
  MsValue tuple;
  uint32_t idx;
};

static MsValue tupleIterNext(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsTupleIterObj* it = (struct MsTupleIterObj*) MS_AS_OBJ(v);
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(it->tuple);
  if (it->idx >= t->len) {
    return MS_NIL_VAL;  // StopIteration marker via nil (T065 protocol)
  }
  return t->items[it->idx++];
}

// traverse: visit the tuple reference slot, supporting a future moving GC
// (Cheney copying, T116) in place, same rationale as tupleTraverse.
static void tupleIterTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsTupleIterObj* it = (struct MsTupleIterObj*) obj;
  visit(&it->tuple, ctx);
}

static struct MsType msTupleIterType = {
    .name = "tuple_iterator",
    .objSize = sizeof(struct MsTupleIterObj),
    .traverse = tupleIterTraverse,
    .tpIter = msIterSelf,
    .tpNext = tupleIterNext,
};

static MsValue tupleIter(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsObject* obj = msGCAlloc(&msTupleIterType, sizeof(struct MsTupleIterObj));
  struct MsTupleIterObj* it = (struct MsTupleIterObj*) obj;
  it->tuple = v;
  it->idx = 0;
  return MS_OBJ_VAL(it);
}

struct MsType msTupleType = {
    .name = "tuple",
    .objSize = sizeof(struct MsTupleObj),
    .varSize = tupleVarSize,
    .traverse = tupleTraverse,
    .destroy = NULL,  // items[] is inline; msGCAlloc's block is freed as a whole
    .tpLen = tupleLen,
    .tpEq = tupleEq,
    .tpLt = tupleLt,
    .tpHash = tupleHash,
    .tpGetitem = tupleGetItem,
    .tpIter = tupleIter,
    .tpContains = tupleContains,
    .tpAdd = tupleConcat,
};

MsValue msTupleIndex(MsValue v, MsValue item, int64_t start) {
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
  uint32_t lo = start < 0 ? 0 : (uint32_t) start;
  for (uint32_t i = lo; i < t->len; i++) {
    if (msValueEqual(t->items[i], item)) {
      return MS_INT_VAL((int64_t) i);
    }
  }
  return MS_INT_VAL(-1);
}

MsValue msTupleCount(MsValue v, MsValue item) {
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
  int64_t count = 0;
  for (uint32_t i = 0; i < t->len; i++) {
    if (msValueEqual(t->items[i], item)) {
      count++;
    }
  }
  return MS_INT_VAL(count);
}
