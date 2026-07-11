// ms_bytes.c -- msBytesType definition: mutable byte-sequence runtime type (type-system.md ss2.6)
#include "mslang/ms_bytes.h"

#include <stdbool.h>
#include <string.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_object.h"
#include "mslang/ms_str.h"

static void bytesDestroy(struct MsObject* obj) {
  struct MsBytes* b = (struct MsBytes*) obj;
  msFree(b->data);
}

MsValue msNewBytes(const uint8_t* data, uint32_t len) {
  struct MsObject* obj = msGCAlloc(&msBytesType, sizeof(struct MsBytes));
  struct MsBytes* b = (struct MsBytes*) obj;
  b->len = len;
  b->cap = len;
  b->data = len > 0 ? (uint8_t*) msAlloc(len) : NULL;
  if (len > 0) {
    memcpy(b->data, data, len);
  }
  return MS_OBJ_VAL(b);
}

static bool isBytes(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msBytesType;
}

static MsValue bytesLen(struct MsVM* vm, MsValue v) {
  (void) vm;
  return MS_INT_VAL(((struct MsBytes*) MS_AS_OBJ(v))->len);
}

// Normalizes a possibly-negative index against b->len (Python-style
// wraparound); returns -1 if out of range after normalization. Shared by
// bytesGetItem/bytesSetItem.
static int64_t normalizeIndex(struct MsBytes* b, int64_t i) {
  if (i < 0) {
    i += (int64_t) b->len;
  }
  return (i < 0 || i >= (int64_t) b->len) ? -1 : i;
}

// bytes[i] -> int (0-255, byte-indexed); slice keys are deferred to T065
// slicing semantics (same rationale as ms_str.c's strGetItem).
static MsValue bytesGetItem(struct MsVM* vm, MsValue v, MsValue idx) {
  (void) vm;
  if (!MS_IS_INT(idx)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  int64_t i = normalizeIndex(b, MS_AS_INT(idx));
  if (i < 0) {
    return MS_ERROR_VALUE;  // IndexError (T080 placeholder)
  }
  return MS_INT_VAL((int64_t) b->data[i]);
}

static MsValue bytesSetItem(struct MsVM* vm, MsValue v, MsValue key, MsValue val) {
  (void) vm;
  if (!MS_IS_INT(key) || !MS_IS_INT(val)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  int64_t i = normalizeIndex(b, MS_AS_INT(key));
  if (i < 0) {
    return MS_ERROR_VALUE;  // IndexError (T080 placeholder)
  }
  int64_t n = MS_AS_INT(val);
  if (n < 0 || n > 255) {
    return MS_ERROR_VALUE;  // ValueError (T080 placeholder)
  }
  b->data[i] = (uint8_t) n;
  return MS_NIL_VAL;
}

static MsValue bytesEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isBytes(b)) {
    return MS_BOOL_VAL(false);
  }
  struct MsBytes* ba = (struct MsBytes*) MS_AS_OBJ(a);
  struct MsBytes* bb = (struct MsBytes*) MS_AS_OBJ(b);
  if (ba->len != bb->len) {
    return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL(memcmp(ba->data, bb->data, ba->len) == 0);
}

// Lexicographic comparison (same pattern as ms_str.c's strLt).
static MsValue bytesLt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isBytes(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsBytes* ba = (struct MsBytes*) MS_AS_OBJ(a);
  struct MsBytes* bb = (struct MsBytes*) MS_AS_OBJ(b);
  int cmp = memcmp(ba->data, bb->data, ba->len < bb->len ? ba->len : bb->len);
  return MS_BOOL_VAL(cmp < 0 || (cmp == 0 && ba->len < bb->len));
}

static MsValue bytesAdd(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isBytes(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsBytes* ba = (struct MsBytes*) MS_AS_OBJ(a);
  struct MsBytes* bb = (struct MsBytes*) MS_AS_OBJ(b);
  if ((uint64_t) ba->len + bb->len > UINT32_MAX) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  uint32_t newLen = ba->len + bb->len;
  uint8_t* buf = msAlloc(newLen);
  memcpy(buf, ba->data, ba->len);
  memcpy(buf + ba->len, bb->data, bb->len);
  MsValue r = msNewBytes(buf, newLen);
  msFree(buf);
  return r;
}

// tpIter/tpNext deferred: the StopIteration sentinel/iterator protocol is
// not settled until T065 (same rationale as ms_str.c's msStrType).
struct MsType msBytesType = {
    .name = "bytes",
    .objSize = sizeof(struct MsBytes),
    .traverse = NULL,  // data holds no MsValue sub-references
    .destroy = bytesDestroy,
    .tpLen = bytesLen,
    .tpEq = bytesEq,
    .tpLt = bytesLt,
    .tpAdd = bytesAdd,
    .tpGetitem = bytesGetItem,
    .tpSetitem = bytesSetItem,
};

MsValue msBytesDecode(MsValue v) {
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  return msNewStr((const char*) b->data, b->len);
}

static const char kHexDigits[] = "0123456789abcdef";

MsValue msBytesHex(MsValue v) {
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  if (b->len == 0) {
    return msNewStr("", 0);
  }
  char* buf = msAlloc((size_t) b->len * 2);
  for (uint32_t i = 0; i < b->len; i++) {
    buf[(size_t) i * 2] = kHexDigits[b->data[i] >> 4];
    buf[(size_t) i * 2 + 1] = kHexDigits[b->data[i] & 0xF];
  }
  MsValue r = msNewStrNoIntern(buf, b->len * 2);
  msFree(buf);
  return r;
}

static int hexDigitVal(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

MsValue msBytesFromHex(MsValue s) {
  struct MsStrObj* str = (struct MsStrObj*) MS_AS_OBJ(s);
  if (str->len % 2 != 0) {
    return MS_ERROR_VALUE;  // ValueError (T080 placeholder)
  }
  uint32_t outLen = str->len / 2;
  if (outLen == 0) {
    return msNewBytes(NULL, 0);
  }
  uint8_t* buf = (uint8_t*) msAlloc(outLen);
  for (uint32_t i = 0; i < outLen; i++) {
    int hi = hexDigitVal(str->data[(size_t) i * 2]);
    int lo = hexDigitVal(str->data[(size_t) i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      msFree(buf);
      return MS_ERROR_VALUE;  // ValueError (T080 placeholder)
    }
    buf[i] = (uint8_t) ((hi << 4) | lo);
  }
  MsValue r = msNewBytes(buf, outLen);
  msFree(buf);
  return r;
}

// Ensures b->cap >= needed, growing by doubling (amortized) so repeated
// small appends/extends don't realloc on every call. Shared by
// msBytesAppend (needed = len + 1) and msBytesExtend (needed = len + o->len).
// Callers must guarantee 'needed' itself does not overflow uint32_t (see the
// (uint64_t) length-sum guards in msBytesAppend/msBytesExtend); this only
// guards the doubling loop against overflowing past UINT32_MAX, returning
// false instead of wrapping/looping forever.
static bool bytesEnsureCap(struct MsBytes* b, uint32_t needed) {
  if (needed <= b->cap) {
    return true;
  }
  uint64_t newCap = b->cap == 0 ? 8 : (uint64_t) b->cap * 2;
  while (newCap < needed) {
    newCap *= 2;
  }
  if (newCap > UINT32_MAX) {
    return false;  // OverflowError (T080 placeholder)
  }
  b->data = (uint8_t*) msRealloc(b->data, (uint32_t) newCap);
  b->cap = (uint32_t) newCap;
  return true;
}

MsValue msBytesAppend(MsValue v, MsValue val) {
  if (!MS_IS_INT(val)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  int64_t n = MS_AS_INT(val);
  if (n < 0 || n > 255) {
    return MS_ERROR_VALUE;  // ValueError (T080 placeholder)
  }
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  if ((uint64_t) b->len + 1 > UINT32_MAX) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  if (!bytesEnsureCap(b, b->len + 1)) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  b->data[b->len++] = (uint8_t) n;
  return MS_NIL_VAL;
}

MsValue msBytesExtend(MsValue v, MsValue other) {
  if (!isBytes(other)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  struct MsBytes* o = (struct MsBytes*) MS_AS_OBJ(other);
  if ((uint64_t) b->len + o->len > UINT32_MAX) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  if (!bytesEnsureCap(b, b->len + o->len)) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  memcpy(b->data + b->len, o->data, o->len);
  b->len += o->len;
  return MS_NIL_VAL;
}

MsValue msBytesCopy(MsValue v) {
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  return msNewBytes(b->data, b->len);
}

// Shared substring search, mirrors ms_str.c's findSub but over uint8_t*
// (byte, not codepoint, data). First index >= lo where needle occurs in
// hay, or -1 if not found. An empty needle matches at lo.
static int64_t findSub(const uint8_t* hay, const uint8_t* needle, uint32_t needleLen, uint32_t lo, uint32_t hi) {
  if (needleLen == 0) {
    return (int64_t) lo;
  }
  for (uint32_t i = lo; i + needleLen <= hi; i++) {
    if (memcmp(hay + i, needle, needleLen) == 0) {
      return (int64_t) i;
    }
  }
  return -1;
}

MsValue msBytesFind(MsValue v, MsValue sub, int64_t start) {
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  struct MsBytes* s = (struct MsBytes*) MS_AS_OBJ(sub);
  uint32_t lo = start < 0 ? 0 : (uint32_t) start;
  if (lo > b->len) {
    return MS_INT_VAL(-1);
  }
  return MS_INT_VAL(findSub(b->data, s->data, s->len, lo, b->len));
}

MsValue msBytesStartsWith(MsValue v, MsValue prefix) {
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  struct MsBytes* p = (struct MsBytes*) MS_AS_OBJ(prefix);
  if (p->len > b->len) {
    return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL(memcmp(b->data, p->data, p->len) == 0);
}

MsValue msBytesEndsWith(MsValue v, MsValue suffix) {
  struct MsBytes* b = (struct MsBytes*) MS_AS_OBJ(v);
  struct MsBytes* p = (struct MsBytes*) MS_AS_OBJ(suffix);
  if (p->len > b->len) {
    return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL(memcmp(b->data + (b->len - p->len), p->data, p->len) == 0);
}
