// ms_slice.h -- msSliceType public declarations: MsSliceObj + msNewSlice/msSliceNormalize (T065)
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsVM;

// Slice object built by OP_BUILD_SLICE (vm.md ss3.8) for obj[lo:hi:step].
// head must be the first member (msGCAlloc/GC treat every heap object as
// struct MsObject*). start/stop/step are MS_NIL_VAL for an omitted component
// (e.g. obj[:5] omits start).
struct MsSliceObj {
  struct MsObject head;
  MsValue start;
  MsValue stop;
  MsValue step;
};

extern struct MsType msSliceType;

// Builds a slice object from the raw (possibly nil) start/stop/step
// components; does not validate them (msSliceNormalize does, at use time).
MsValue msNewSlice(MsValue start, MsValue stop, MsValue step);

// True if v is a slice object (OP_GET_ITEM's is-slice-key test).
bool msIsSlice(MsValue v);

// Normalizes s's start/stop/step against a concrete sequence length len
// (Python slice.indices() semantics): nil step defaults to 1; nil start/stop
// default to the bound appropriate for the step's direction; negative
// indices wrap (index + len); out-of-range indices clamp into the valid
// bound. Iterating
//   for (i = *outStart; *outStep > 0 ? i < *outStop : i > *outStop; i += *outStep)
// yields exactly the sliced elements, in order.
void msSliceNormalize(struct MsSliceObj* s, int64_t len, int64_t* outStart, int64_t* outStop, int64_t* outStep);

// Number of elements a normalized (start, stop, step) triple yields (same
// "steps from start to stop" arithmetic as ms_range.c's rangeLen); lets
// sequence slicing (list/str/tuple) size its result in one pass instead of
// counting via a loop before filling.
int64_t msSliceCount(int64_t start, int64_t stop, int64_t step);

// True if s has an explicit step of 0 -- msSliceNormalize/msSliceCount would
// divide by it (SIGFPE). Callers (list/str/tuple getitem slice handling)
// must check this before calling msSliceNormalize/msSliceCount and return an
// error instead (Python: ValueError('slice step cannot be zero')).
bool msSliceStepIsZero(struct MsSliceObj* s);

// Copies the sliced elements from src into dst (dst must have room for
// msSliceCount(start, stop, step) elements); shared by list/tuple slicing,
// which both index an MsValue array (str slices raw bytes instead, see
// ms_str.c's strGetSlice). step == 1 takes a single memcpy instead of an
// element-by-element loop.
void msSliceFillValues(MsValue* dst, const MsValue* src, int64_t start, int64_t stop, int64_t step);
