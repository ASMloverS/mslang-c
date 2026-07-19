// ms_list.c -- msListType definition: mutable dynamic-array runtime type (type-system.md ss2.7)
#include "mslang/ms_list.h"

#include <stdlib.h>
#include <string.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_object.h"
#include "mslang/ms_slice.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

static bool isList(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msListType;
}

// Ensures l->cap >= needed, growing by doubling (amortized), starting at 4
// (cap < 4 -> 4 -> 8 -> 16 -> ..., per P4-T059-vm-list.md ss2). Callers must
// guarantee 'needed' itself does not overflow uint32_t; this only guards the
// doubling loop against overflowing past UINT32_MAX.
static bool listEnsureCap(struct MsListObj* l, uint32_t needed) {
  if (needed <= l->cap) {
    return true;
  }
  uint64_t newCap = l->cap < 4 ? 4 : (uint64_t) l->cap * 2;
  while (newCap < needed) {
    newCap *= 2;
  }
  if (newCap > UINT32_MAX) {
    return false;  // OverflowError (T080 placeholder)
  }
  l->items = (MsValue*) msRealloc(l->items, (size_t) newCap * sizeof(MsValue));
  l->cap = (uint32_t) newCap;
  return true;
}

static void listDestroy(struct MsObject* obj) {
  msFree(((struct MsListObj*) obj)->items);
}

MsValue msNewList(uint32_t initCap) {
  struct MsObject* obj = msGCAlloc(&msListType, sizeof(struct MsListObj));
  struct MsListObj* l = (struct MsListObj*) obj;
  l->len = 0;
  l->cap = initCap;
  l->items = initCap > 0 ? MS_ALLOC_N(MsValue, initCap) : NULL;
  return MS_OBJ_VAL(l);
}

MsValue msListAppend(struct MsListObj* l, MsValue v) {
  if ((uint64_t) l->len + 1 > UINT32_MAX) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  if (!listEnsureCap(l, l->len + 1)) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  l->items[l->len++] = v;
  return MS_NIL_VAL;
}

// Normalizes a possibly-negative index against l->len (Python-style
// wraparound); returns -1 if out of range after normalization. Shared by
// msListGet/msListSet/msListPop/listGetItem.
static int64_t normalizeIndex(struct MsListObj* l, int64_t i) {
  if (i < 0) {
    i += (int64_t) l->len;
  }
  return (i < 0 || i >= (int64_t) l->len) ? -1 : i;
}

MsValue msListGet(struct MsListObj* l, int64_t i) {
  int64_t idx = normalizeIndex(l, i);
  if (idx < 0) {
    return MS_ERROR_VALUE;  // IndexError (T080 placeholder)
  }
  return l->items[idx];
}

MsValue msListSet(struct MsListObj* l, int64_t i, MsValue v) {
  int64_t idx = normalizeIndex(l, i);
  if (idx < 0) {
    return MS_ERROR_VALUE;  // IndexError (T080 placeholder)
  }
  l->items[idx] = v;
  return MS_NIL_VAL;
}

MsValue msListInsert(struct MsListObj* l, int64_t i, MsValue v) {
  if (i < 0) {
    i += (int64_t) l->len;
    if (i < 0) {
      i = 0;
    }
  }
  if (i > (int64_t) l->len) {
    i = (int64_t) l->len;
  }
  if ((uint64_t) l->len + 1 > UINT32_MAX) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  if (!listEnsureCap(l, l->len + 1)) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  memmove(l->items + i + 1, l->items + i, (l->len - (uint32_t) i) * sizeof(MsValue));
  l->items[i] = v;
  l->len++;
  return MS_NIL_VAL;
}

MsValue msListPop(struct MsListObj* l, int64_t i) {
  int64_t idx = normalizeIndex(l, i);
  if (idx < 0) {
    return MS_ERROR_VALUE;  // IndexError (T080 placeholder)
  }
  MsValue v = l->items[idx];
  memmove(l->items + idx, l->items + idx + 1, (l->len - (uint32_t) idx - 1) * sizeof(MsValue));
  l->len--;
  return v;
}

static MsValue listLen(struct MsVM* vm, MsValue v) {
  (void) vm;
  return MS_INT_VAL((int64_t) ((struct MsListObj*) MS_AS_OBJ(v))->len);
}

static MsValue listEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isList(b)) {
    return MS_BOOL_VAL(false);
  }
  struct MsListObj* la = (struct MsListObj*) MS_AS_OBJ(a);
  struct MsListObj* lb = (struct MsListObj*) MS_AS_OBJ(b);
  if (la->len != lb->len) {
    return MS_BOOL_VAL(false);
  }
  for (uint32_t i = 0; i < la->len; i++) {
    if (!msValueEqual(la->items[i], lb->items[i])) {
      return MS_BOOL_VAL(false);
    }
  }
  return MS_BOOL_VAL(true);
}

// list[a:b:step] -> new list (T065); msSliceCount sizes the result list
// exactly, so elements are only visited once (fill pass).
static MsValue listGetSlice(struct MsListObj* l, MsValue idx) {
  struct MsSliceObj* sl = (struct MsSliceObj*) MS_AS_OBJ(idx);
  if (msSliceStepIsZero(sl)) {
    return MS_ERROR_VALUE;  // ValueError: slice step cannot be zero (T080 placeholder)
  }
  int64_t start, stop, step;
  msSliceNormalize(sl, l->len, &start, &stop, &step);
  uint32_t n = (uint32_t) msSliceCount(start, stop, step);
  MsValue r = msNewList(n);
  struct MsListObj* lr = (struct MsListObj*) MS_AS_OBJ(r);
  msSliceFillValues(lr->items, l->items, start, stop, step);
  lr->len = n;
  return r;
}

// list[i] -> element; list[a:b:step] -> new list (T065).
static MsValue listGetItem(struct MsVM* vm, MsValue v, MsValue idx) {
  (void) vm;
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  if (msIsSlice(idx)) {
    return listGetSlice(l, idx);
  }
  if (!MS_IS_INT(idx)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  return msListGet(l, MS_AS_INT(idx));
}

static MsValue listSetItem(struct MsVM* vm, MsValue v, MsValue key, MsValue val) {
  (void) vm;
  if (!MS_IS_INT(key)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  return msListSet(l, MS_AS_INT(key), val);
}

static MsValue listContains(struct MsVM* vm, MsValue v, MsValue item) {
  (void) vm;
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  for (uint32_t i = 0; i < l->len; i++) {
    if (msValueEqual(l->items[i], item)) {
      return MS_BOOL_VAL(true);
    }
  }
  return MS_BOOL_VAL(false);
}

static MsValue listConcat(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isList(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsListObj* la = (struct MsListObj*) MS_AS_OBJ(a);
  struct MsListObj* lb = (struct MsListObj*) MS_AS_OBJ(b);
  if ((uint64_t) la->len + lb->len > UINT32_MAX) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  uint32_t newLen = la->len + lb->len;
  MsValue r = msNewList(newLen);
  struct MsListObj* lr = (struct MsListObj*) MS_AS_OBJ(r);
  if (la->len) {
    memcpy(lr->items, la->items, la->len * sizeof(MsValue));
  }
  if (lb->len) {
    memcpy(lr->items + la->len, lb->items, lb->len * sizeof(MsValue));
  }
  lr->len = newLen;
  return r;
}

static MsValue listRepeat(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_INT(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  int64_t n = MS_AS_INT(b);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(a);
  if (n <= 0 || l->len == 0) {
    return msNewList(0);
  }
  if ((uint64_t) n > UINT32_MAX / l->len) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  uint32_t newLen = (uint32_t) (l->len * (uint32_t) n);
  MsValue r = msNewList(newLen);
  struct MsListObj* lr = (struct MsListObj*) MS_AS_OBJ(r);
  for (int64_t i = 0; i < n; i++) {
    memcpy(lr->items + i * l->len, l->items, l->len * sizeof(MsValue));
  }
  lr->len = newLen;
  return r;
}

// traverse: visit every element slot (address, not value) so a future
// moving GC (Cheney copying, T116) can rewrite it in place.
static void listTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsListObj* l = (struct MsListObj*) obj;
  for (uint32_t i = 0; i < l->len; i++) {
    visit(&l->items[i], ctx);
  }
}

static MsValue listIterNext(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsListIterObj* it = (struct MsListIterObj*) MS_AS_OBJ(v);
  struct MsListObj* list = (struct MsListObj*) MS_AS_OBJ(it->list);
  if (it->idx >= list->len) {
    return MS_NIL_VAL;  // StopIteration marker via nil (T065 protocol)
  }
  return list->items[it->idx++];
}

// traverse: visit the list reference slot, supporting a future moving GC
// (Cheney copying, T116) in place, same rationale as listTraverse.
static void listIterTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsListIterObj* it = (struct MsListIterObj*) obj;
  visit(&it->list, ctx);
}

struct MsType msListIterType = {
    .name = "list_iterator",
    .objSize = sizeof(struct MsListIterObj),
    .traverse = listIterTraverse,
    .tpIter = msIterSelf,
    .tpNext = listIterNext,
};

static MsValue listIter(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsObject* obj = msGCAlloc(&msListIterType, sizeof(struct MsListIterObj));
  struct MsListIterObj* it = (struct MsListIterObj*) obj;
  it->list = v;
  it->idx = 0;
  return MS_OBJ_VAL(it);
}

// Grows buf (doubling) until it can hold `len + need` bytes; shared by
// listRepr's separator/repr-text/closing-bracket appends below.
static char* listReprGrow(char* buf, uint32_t* cap, uint32_t len, uint32_t need) {
  while (len + need > *cap) {
    *cap *= 2;
  }
  return msRealloc(buf, *cap);
}

// "[" + repr(item0) + ", " + repr(item1) + ... + "]" (Python list-literal
// syntax; impl/P4-T067-vm-e2e-m1.md needs this for print(list)). Recurses
// into each element's own tpRepr; an element with no tpRepr slot fails the
// whole repr (T080 placeholder: TypeError).
static MsValue listRepr(struct MsVM* vm, MsValue v) {
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  uint32_t cap = 64, len = 0;
  char* buf = msAlloc(cap);
  buf[len++] = '[';
  for (uint32_t i = 0; i < l->len; i++) {
    if (i > 0) {
      buf = listReprGrow(buf, &cap, len, 2);
      buf[len++] = ',';
      buf[len++] = ' ';
    }
    struct MsType* itp = msTypeOf(l->items[i]);
    if (!itp->tpRepr) {
      msFree(buf);
      return MS_ERROR_VALUE;
    }
    MsValue rv = itp->tpRepr(vm, l->items[i]);
    if (MS_IS_ERROR(rv)) {
      msFree(buf);
      return rv;
    }
    struct MsStrObj* rs = (struct MsStrObj*) MS_AS_OBJ(rv);
    buf = listReprGrow(buf, &cap, len, rs->len + 1);
    memcpy(buf + len, rs->data, rs->len);
    len += rs->len;
  }
  buf = listReprGrow(buf, &cap, len, 1);
  buf[len++] = ']';
  MsValue result = msNewStr(buf, len);
  msFree(buf);
  return result;
}

struct MsType msListType = {
    .name = "list",
    .objSize = sizeof(struct MsListObj),
    .traverse = listTraverse,
    .destroy = listDestroy,
    .tpRepr = listRepr,
    .tpLen = listLen,
    .tpEq = listEq,
    .tpGetitem = listGetItem,
    .tpSetitem = listSetItem,
    .tpIter = listIter,
    .tpContains = listContains,
    .tpAdd = listConcat,
    .tpMul = listRepeat,
};

MsValue msListRemove(MsValue v, MsValue item) {
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  for (uint32_t i = 0; i < l->len; i++) {
    if (msValueEqual(l->items[i], item)) {
      memmove(l->items + i, l->items + i + 1, (l->len - i - 1) * sizeof(MsValue));
      l->len--;
      return MS_NIL_VAL;
    }
  }
  return MS_ERROR_VALUE;  // ValueError (T080 placeholder)
}

MsValue msListIndex(MsValue v, MsValue item, int64_t start) {
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  uint32_t lo = start < 0 ? 0 : (uint32_t) start;
  for (uint32_t i = lo; i < l->len; i++) {
    if (msValueEqual(l->items[i], item)) {
      return MS_INT_VAL((int64_t) i);
    }
  }
  return MS_INT_VAL(-1);
}

MsValue msListCount(MsValue v, MsValue item) {
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  int64_t count = 0;
  for (uint32_t i = 0; i < l->len; i++) {
    if (msValueEqual(l->items[i], item)) {
      count++;
    }
  }
  return MS_INT_VAL(count);
}

// qsort()'s comparator takes no user-data parameter; a file-static flag is
// safe because the VM (gc.c's documented baseline) is single-threaded and
// sort() never recurses into another sort() mid-comparison.
static bool gListSortReverse = false;

static int listCompare(const void* pa, const void* pb) {
  MsValue a = *(const MsValue*) pa;
  MsValue b = *(const MsValue*) pb;
  if (gListSortReverse) {
    MsValue tmp = a;
    a = b;
    b = tmp;
  }
  struct MsType* ta = msTypeOf(a);
  if (!ta->tpLt) {
    return 0;  // TypeError placeholder; qsort has no error-propagation path
  }
  if (MS_AS_BOOL(ta->tpLt(&gVM, a, b))) {
    return -1;
  }
  if (MS_AS_BOOL(ta->tpLt(&gVM, b, a))) {
    return 1;
  }
  return 0;
}

// v1 uses qsort (C stdlib); key function support deferred to T099.
MsValue msListSort(MsValue v, bool reverse) {
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  gListSortReverse = reverse;
  qsort(l->items, l->len, sizeof(MsValue), listCompare);
  return MS_NIL_VAL;
}

MsValue msListReverse(MsValue v) {
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  for (uint32_t i = 0, j = l->len; i + 1 < j; i++) {
    j--;
    MsValue tmp = l->items[i];
    l->items[i] = l->items[j];
    l->items[j] = tmp;
  }
  return MS_NIL_VAL;
}

MsValue msListExtend(MsValue v, MsValue other) {
  if (!isList(other)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  struct MsListObj* o = (struct MsListObj*) MS_AS_OBJ(other);
  if ((uint64_t) l->len + o->len > UINT32_MAX) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  if (!listEnsureCap(l, l->len + o->len)) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  if (o->len) {
    memcpy(l->items + l->len, o->items, o->len * sizeof(MsValue));
  }
  l->len += o->len;
  return MS_NIL_VAL;
}

MsValue msListClear(MsValue v) {
  ((struct MsListObj*) MS_AS_OBJ(v))->len = 0;
  return MS_NIL_VAL;
}

MsValue msListCopy(MsValue v) {
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(v);
  MsValue r = msNewList(l->len);
  struct MsListObj* lr = (struct MsListObj*) MS_AS_OBJ(r);
  if (l->len) {
    memcpy(lr->items, l->items, l->len * sizeof(MsValue));
  }
  lr->len = l->len;
  return r;
}
