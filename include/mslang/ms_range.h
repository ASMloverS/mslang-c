// ms_range.h -- msRangeType/msRangeIterType public declarations: lazy integer range + iterator (type-system.md ss2.12)
#pragma once

#include <stdint.h>

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsVM;

// Lazy integer range (type-system.md ss2.12); no element list is materialized.
// head must be the first member (msGCAlloc/GC treat every heap object as
// struct MsObject*).
struct MsRangeObj {
  struct MsObject head;
  int64_t start;
  int64_t stop;
  int64_t step;  // non-zero, positive or negative
};

// Independent iterator object over a range; does not mutate the MsRangeObj it
// was built from (range objects are immutable, iterable more than once).
struct MsRangeIterObj {
  struct MsObject head;
  int64_t cur;  // current value
  int64_t stop;
  int64_t step;
};

extern struct MsType msRangeType;
extern struct MsType msRangeIterType;

// Builds a range object directly (no argc dispatch); step must be non-zero.
MsValue msNewRange(int64_t start, int64_t stop, int64_t step);

// range(stop) / range(start, stop) / range(start, stop, step). Non-int
// arguments or a zero step return MS_ERROR_VALUE (TypeError/ValueError, T080
// placeholder).
MsValue msBuiltinRange(struct MsVM* vm, MsValue* args, int argc);
