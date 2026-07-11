// ms_bytes.h -- msBytesType public declarations: MsBytes + constructors + methods (type-system.md ss2.6)
#pragma once

#include <stdint.h>

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsVM;

// Mutable byte sequence (type-system.md ss2.6). head must be the first
// member (msGCAlloc/GC treat every heap object as struct MsObject*). data is
// an extra, non-GC-managed allocation (msAlloc/msRealloc); destroy frees it.
struct MsBytes {
  struct MsObject head;
  uint32_t len;   // current byte count
  uint32_t cap;   // allocated capacity of data
  uint8_t* data;  // byte array
};

extern struct MsType msBytesType;

// Create a bytes object, copying len bytes from data.
MsValue msNewBytes(const uint8_t* data, uint32_t len);

// bytes methods (type-system.md ss2.6); not yet wired to `.method()` call
// syntax (attribute/method dispatch lands in T073), exposed here as plain C
// functions for direct use and testing (same pattern as ms_str.h).
MsValue msBytesDecode(MsValue v);
MsValue msBytesHex(MsValue v);
// bytes.fromHex(s) is a type-level static constructor (Python: bytes.fromHex);
// s must be a str. .ms-level `bytes.fromHex(...)` builtin-type-constructor
// call syntax lands in T097; exposed here as a plain C function meanwhile.
MsValue msBytesFromHex(MsValue s);
MsValue msBytesAppend(MsValue v, MsValue b);
// other must be a bytes object: list/iterator protocol (a generic
// "iterable of int") is not implemented until T059+, so extend() is
// narrowed to bytes-with-bytes for now.
MsValue msBytesExtend(MsValue v, MsValue other);
MsValue msBytesCopy(MsValue v);
// start is a byte offset. Returns MS_INT_VAL(-1) if sub is not found (not an error).
MsValue msBytesFind(MsValue v, MsValue sub, int64_t start);
MsValue msBytesStartsWith(MsValue v, MsValue prefix);
MsValue msBytesEndsWith(MsValue v, MsValue suffix);
