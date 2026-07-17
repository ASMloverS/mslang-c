// ms_slice.c -- msSliceType definition: OP_BUILD_SLICE's slice object + normalization (T065)
#include "mslang/ms_slice.h"

#include <string.h>

#include "mslang/ms_gc.h"
#include "mslang/ms_object.h"

MsValue msNewSlice(MsValue start, MsValue stop, MsValue step) {
  struct MsObject* obj = msGCAlloc(&msSliceType, sizeof(struct MsSliceObj));
  struct MsSliceObj* s = (struct MsSliceObj*) obj;
  s->start = start;
  s->stop = stop;
  s->step = step;
  return MS_OBJ_VAL(s);
}

bool msIsSlice(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msSliceType;
}

// Clamps a resolved (already length-adjusted-if-negative) index into
// [lower, upper], the bound pair matching Python's slice.indices() for the
// step's direction (positive step: [0, len]; negative step: [-1, len-1]).
static int64_t clampBound(int64_t idx, int64_t lower, int64_t upper) {
  if (idx < lower) {
    return lower;
  }
  if (idx > upper) {
    return upper;
  }
  return idx;
}

// Resolves one raw component (already known non-nil) against len/lower/upper:
// negative values wrap (idx + len) before clamping; non-negative values
// clamp directly.
static int64_t resolveIndex(int64_t idx, int64_t len, int64_t lower, int64_t upper) {
  if (idx < 0) {
    idx += len;
  }
  return clampBound(idx, lower, upper);
}

void msSliceNormalize(struct MsSliceObj* s, int64_t len, int64_t* outStart, int64_t* outStop, int64_t* outStep) {
  int64_t step = MS_IS_NIL(s->step) ? 1 : MS_AS_INT(s->step);
  bool posStep = step > 0;
  int64_t lower = posStep ? 0 : -1;
  int64_t upper = posStep ? len : len - 1;

  int64_t start =
      MS_IS_NIL(s->start) ? (posStep ? lower : upper) : resolveIndex(MS_AS_INT(s->start), len, lower, upper);
  int64_t stop = MS_IS_NIL(s->stop) ? (posStep ? upper : lower) : resolveIndex(MS_AS_INT(s->stop), len, lower, upper);

  *outStart = start;
  *outStop = stop;
  *outStep = step;
}

int64_t msSliceCount(int64_t start, int64_t stop, int64_t step) {
  if (step > 0) {
    return start < stop ? (stop - start + step - 1) / step : 0;
  }
  return start > stop ? (start - stop - step - 1) / (-step) : 0;
}

bool msSliceStepIsZero(struct MsSliceObj* s) {
  return !MS_IS_NIL(s->step) && MS_AS_INT(s->step) == 0;
}

void msSliceFillValues(MsValue* dst, const MsValue* src, int64_t start, int64_t stop, int64_t step) {
  if (step == 1 && start < stop) {
    memcpy(dst, src + start, (size_t) (stop - start) * sizeof(MsValue));
    return;
  }
  int64_t w = 0;
  for (int64_t i = start; step > 0 ? i < stop : i > stop; i += step) {
    dst[w++] = src[i];
  }
}

// traverse: visit start/stop/step (address, not value) so a future moving GC
// (Cheney copying, T116) can rewrite them in place, same rationale as
// listTraverse/tupleTraverse.
static void sliceTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsSliceObj* s = (struct MsSliceObj*) obj;
  visit(&s->start, ctx);
  visit(&s->stop, ctx);
  visit(&s->step, ctx);
}

struct MsType msSliceType = {
    .name = "slice",
    .objSize = sizeof(struct MsSliceObj),
    .traverse = sliceTraverse,
};
