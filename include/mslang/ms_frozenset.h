// ms_frozenset.h -- msFrozensetType public declarations: reuses MsSetObj/MsSetEntry (ms_set.h), immutable
// (type-system.md ss2.11)
#pragma once

#include "mslang/ms_set.h"
#include "mslang/ms_value.h"

struct MsVM;

extern struct MsType msFrozensetType;

// Builds a frozenset from a list or set/frozenset source (generic iterable
// support deferred to T065, same narrowing as msSetUpdate). Reuses msSetAdd's
// hash/dedup logic for insertion. Element hashing errors return
// MS_ERROR_VALUE (TypeError, T080 placeholder), same as an unsupported
// source type.
MsValue msNewFrozensetFromIter(struct MsVM* vm, MsValue iterable);

// Set operations (type-system.md ss2.11); other must be a frozenset (mixed
// set/frozenset operands deferred, same narrowing as ms_set.h). Also serve as
// the .union()/.intersection()/.difference()/.symmetricDifference() methods.
MsValue msFrozensetUnion(struct MsVM* vm, MsValue a, MsValue b);      // a | b
MsValue msFrozensetIntersect(struct MsVM* vm, MsValue a, MsValue b);  // a & b
MsValue msFrozensetDiff(struct MsVM* vm, MsValue a, MsValue b);       // a - b
MsValue msFrozensetSymDiff(struct MsVM* vm, MsValue a, MsValue b);    // a ^ b

// frozenset methods (type-system.md ss2.11); not yet wired to `.method()`
// call syntax (attribute/method dispatch lands in T073), exposed here as
// plain C functions for direct use and testing (same pattern as ms_set.h).
MsValue msFrozensetIsSubset(MsValue a, MsValue b);                     // .isSubset(other)
MsValue msFrozensetIsSuperset(MsValue a, MsValue b);                   // .isSuperset(other)
MsValue msFrozensetIsDisjoint(struct MsVM* vm, MsValue a, MsValue b);  // .isDisjoint(other)
MsValue msFrozensetCopy(MsValue v);                                    // .copy() returns self (immutable)
