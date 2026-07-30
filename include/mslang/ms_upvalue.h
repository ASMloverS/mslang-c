// ms_upvalue.h -- MsUpvalue runtime object: open (stack-pointing) and closed
// (heap-copied) closure variable capture (vm.md ss5, P5-T071)
#pragma once

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsThread;

// head must be the first member (msGCAlloc/GC treat every heap object as
// struct MsObject*, matching MsFuncProto/MsClosure's "header" convention).
typedef struct MsUpvalue {
  struct MsObject header;
  MsValue* location;           // open: points into a MsThread stack slot; closed: &closedVal
  MsValue closedVal;           // close-time copy of the stack slot's latest value
  struct MsUpvalue* nextOpen;  // t->openUpvalues singly-linked list, sorted by location descending
} MsUpvalue;

extern struct MsType msUpvalueType;

// Finds or creates the open upvalue for `location` in t->openUpvalues.
// Capturing the same location twice (from two different closures) returns
// the same MsUpvalue object, so closures sharing a variable observe each
// other's writes.
MsUpvalue* msCaptureUpvalue(struct MsThread* t, MsValue* location);

// Closes every open upvalue in t->openUpvalues with location >= slot: copies
// the current stack value into closedVal and redirects location to
// &closedVal. slot is the base address of the stack region about to become
// invalid (a frame's slots on OP_RETURN/OP_RETURN_NIL, or a block scope's
// first local slot on OP_CLOSE_UPVALUE).
void msCloseUpvalues(struct MsThread* t, MsValue* slot);
