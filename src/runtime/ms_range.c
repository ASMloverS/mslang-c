// ms_range.c -- msRangeType/msRangeIterType definitions: lazy integer range + iterator (type-system.md ss2.12)
#include "mslang/ms_range.h"

#include <stdio.h>

#include "mslang/ms_gc.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

MsValue msNewRange(int64_t start, int64_t stop, int64_t step) {
  struct MsObject* obj = msGCAlloc(&msRangeType, sizeof(struct MsRangeObj));
  struct MsRangeObj* r = (struct MsRangeObj*) obj;
  r->start = start;
  r->stop = stop;
  r->step = step;
  return MS_OBJ_VAL(r);
}

MsValue msBuiltinRange(struct MsVM* vm, MsValue* args, int argc) {
  (void) vm;  // 异常类型区分依赖 T080，当前统一返回 MS_ERROR_VALUE 哨兵
  int64_t start = 0, stop, step = 1;
  if (argc == 1) {
    if (!MS_IS_INT(args[0])) {
      return MS_ERROR_VALUE;
    }
    stop = MS_AS_INT(args[0]);
  } else if (argc == 2) {
    if (!MS_IS_INT(args[0]) || !MS_IS_INT(args[1])) {
      return MS_ERROR_VALUE;
    }
    start = MS_AS_INT(args[0]);
    stop = MS_AS_INT(args[1]);
  } else if (argc == 3) {
    if (!MS_IS_INT(args[0]) || !MS_IS_INT(args[1]) || !MS_IS_INT(args[2])) {
      return MS_ERROR_VALUE;
    }
    start = MS_AS_INT(args[0]);
    stop = MS_AS_INT(args[1]);
    step = MS_AS_INT(args[2]);
    if (step == 0) {
      return MS_ERROR_VALUE;  // ValueError
    }
  } else {
    return MS_ERROR_VALUE;  // TypeError
  }
  return msNewRange(start, stop, step);
}

static bool isRange(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msRangeType;
}

// True once cur has passed stop for the given step direction; shared by
// rangeLen (cur = start) and rangeIterNext (cur = current position).
static bool rangeExhausted(int64_t cur, int64_t stop, int64_t step) {
  return (step > 0 && cur >= stop) || (step < 0 && cur <= stop);
}

static MsValue rangeLen(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsRangeObj* r = (struct MsRangeObj*) MS_AS_OBJ(v);
  if (rangeExhausted(r->start, r->stop, r->step)) {
    return MS_INT_VAL(0);
  }
  int64_t n = (r->stop - r->start + r->step - (r->step > 0 ? 1 : -1)) / r->step;
  return MS_INT_VAL(n > 0 ? n : 0);
}

static MsValue rangeGetItem(struct MsVM* vm, MsValue v, MsValue idx) {
  struct MsRangeObj* r = (struct MsRangeObj*) MS_AS_OBJ(v);
  if (!MS_IS_INT(idx)) {
    return MS_ERROR_VALUE;
  }
  int64_t len = MS_AS_INT(rangeLen(vm, v));
  int64_t i = MS_AS_INT(idx);
  if (i < 0) {
    i += len;
  }
  if (i < 0 || i >= len) {
    return MS_ERROR_VALUE;  // IndexError
  }
  return MS_INT_VAL(r->start + i * r->step);
}

static MsValue rangeContains(struct MsVM* vm, MsValue v, MsValue item) {
  (void) vm;
  struct MsRangeObj* r = (struct MsRangeObj*) MS_AS_OBJ(v);
  if (!MS_IS_INT(item)) {
    return MS_BOOL_VAL(false);
  }
  int64_t x = MS_AS_INT(item);
  if (r->step > 0) {
    if (x < r->start || x >= r->stop) {
      return MS_BOOL_VAL(false);
    }
    return MS_BOOL_VAL((x - r->start) % r->step == 0);
  }
  if (x > r->start || x <= r->stop) {
    return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL((r->start - x) % (-r->step) == 0);
}

// range == range: 归一化后比较（空 range 恒等；否则比较 start 与 step，长度已在前面比对）
static MsValue rangeEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isRange(b)) {
    return MS_BOOL_VAL(false);
  }
  struct MsRangeObj* ra = (struct MsRangeObj*) MS_AS_OBJ(a);
  struct MsRangeObj* rb = (struct MsRangeObj*) MS_AS_OBJ(b);
  int64_t lenA = MS_AS_INT(rangeLen(vm, a));
  int64_t lenB = MS_AS_INT(rangeLen(vm, b));
  if (lenA != lenB) {
    return MS_BOOL_VAL(false);
  }
  if (lenA == 0) {
    return MS_BOOL_VAL(true);
  }
  if (ra->start != rb->start) {
    return MS_BOOL_VAL(false);
  }
  if (lenA == 1) {
    return MS_BOOL_VAL(true);  // 单元素时 step 不影响可见序列
  }
  return MS_BOOL_VAL(ra->step == rb->step);
}

static MsValue rangeRepr(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsRangeObj* r = (struct MsRangeObj*) MS_AS_OBJ(v);
  char buf[96];
  int n = snprintf(
      buf, sizeof(buf), "range(%lld, %lld, %lld)", (long long) r->start, (long long) r->stop, (long long) r->step);
  return msNewStrNoIntern(buf, (uint32_t) n);
}

static MsValue rangeIterSelf(struct MsVM* vm, MsValue v) {
  (void) vm;
  return v;
}

static MsValue rangeIterNext(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsRangeIterObj* it = (struct MsRangeIterObj*) MS_AS_OBJ(v);
  if (rangeExhausted(it->cur, it->stop, it->step)) {
    return MS_NIL_VAL;  // StopIteration marker via nil (T065 protocol)
  }
  MsValue result = MS_INT_VAL(it->cur);
  it->cur += it->step;
  return result;
}

struct MsType msRangeIterType = {
    .name = "range_iterator",
    .objSize = sizeof(struct MsRangeIterObj),
    .tpIter = rangeIterSelf,
    .tpNext = rangeIterNext,
};

static MsValue rangeIter(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsRangeObj* r = (struct MsRangeObj*) MS_AS_OBJ(v);
  struct MsObject* obj = msGCAlloc(&msRangeIterType, sizeof(struct MsRangeIterObj));
  struct MsRangeIterObj* it = (struct MsRangeIterObj*) obj;
  it->cur = r->start;
  it->stop = r->stop;
  it->step = r->step;
  return MS_OBJ_VAL(it);
}

struct MsType msRangeType = {
    .name = "range",
    .objSize = sizeof(struct MsRangeObj),
    .tpLen = rangeLen,
    .tpGetitem = rangeGetItem,
    .tpContains = rangeContains,
    .tpIter = rangeIter,
    .tpEq = rangeEq,
    .tpRepr = rangeRepr,
    .traverse = NULL,  // only int fields, no GC sub-objects
};
