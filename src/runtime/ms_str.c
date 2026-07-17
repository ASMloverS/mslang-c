// ms_str.c -- msStrType definition: UTF-8 immutable string runtime type (type-system.md ss2.5)
#include "mslang/ms_str.h"

#include <stdbool.h>
#include <string.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_hash.h"
#include "mslang/ms_object.h"
#include "mslang/ms_slice.h"
#include "mslang/ms_vm.h"

#define MS_STR_INTERN_MAX 64
#define MS_STR_INTERN_CAP 4096

// Process-wide intern table for short strings (<= MS_STR_INTERN_MAX bytes),
// open addressing with linear probing. Entries are weak references (not GC
// roots): a mid-run GC that collects an unreachable interned string would
// leave a stale slot here (gc.md's documented v1 risk; sweep-time intern
// cleanup is deferred). Reset once per VM lifecycle by msStrInternInit() so
// repeated msVMInit()/msVMShutdown() cycles in one process never read a
// pointer freed by the previous cycle's msGCShutdown().
static struct MsStrObj* gInternTable[MS_STR_INTERN_CAP];

void msStrInternInit(void) {
  memset(gInternTable, 0, sizeof(gInternTable));
}

// Finds data/len in the intern table. On a hit, *outFound is true and the
// returned slot holds the existing MsStrObj*. On a miss, *outFound is false
// and the returned slot is the empty slot to insert into (NULL if the table
// is full -- caller should just skip interning in that case).
static struct MsStrObj** internFind(uint32_t hash, const char* data, uint32_t len, bool* outFound) {
  uint32_t idx = hash % MS_STR_INTERN_CAP;
  for (uint32_t probe = 0; probe < MS_STR_INTERN_CAP; probe++) {
    struct MsStrObj** slot = &gInternTable[idx];
    if (*slot == NULL) {
      *outFound = false;
      return slot;
    }
    if ((*slot)->len == len && memcmp((*slot)->data, data, len) == 0) {
      *outFound = true;
      return slot;
    }
    idx = (idx + 1) % MS_STR_INTERN_CAP;
  }
  *outFound = false;
  return NULL;
}

static MsValue allocStr(const char* data, uint32_t len, uint32_t hash) {
  struct MsObject* obj = msGCAlloc(&msStrType, sizeof(struct MsStrObj) + len + 1);
  struct MsStrObj* s = (struct MsStrObj*) obj;
  s->len = len;
  s->cpLen = UINT32_MAX;
  s->hash = hash;
  memcpy(s->data, data, len);
  s->data[len] = '\0';
  return MS_OBJ_VAL(s);
}

MsValue msNewStrNoIntern(const char* data, uint32_t len) {
  return allocStr(data, len, 0);
}

MsValue msNewStr(const char* data, uint32_t len) {
  if (len > MS_STR_INTERN_MAX) {
    return allocStr(data, len, 0);
  }
  uint32_t hash = msFnv1a32(data, len);
  if (!hash) {
    hash = 1;  // 0 marks "not computed" (strHash)
  }
  bool found = false;
  struct MsStrObj** slot = internFind(hash, data, len, &found);
  if (found) {
    return MS_OBJ_VAL(*slot);
  }
  MsValue v = allocStr(data, len, hash);
  if (slot) {
    *slot = (struct MsStrObj*) MS_AS_OBJ(v);
  }
  return v;
}

MsValue msStrConcat(MsValue a, MsValue b) {
  struct MsStrObj* sa = (struct MsStrObj*) MS_AS_OBJ(a);
  struct MsStrObj* sb = (struct MsStrObj*) MS_AS_OBJ(b);
  if ((uint64_t) sa->len + sb->len > UINT32_MAX) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  uint32_t newLen = sa->len + sb->len;
  char* buf = msAlloc(newLen);
  memcpy(buf, sa->data, sa->len);
  memcpy(buf + sa->len, sb->data, sb->len);
  MsValue r = msNewStr(buf, newLen);
  msFree(buf);
  return r;
}

MsValue msStrFromInt(int64_t i) {
  char tmp[20];  // INT64_MIN magnitude (19 digits) + sign, no NUL needed
  int idx = 20;
  bool neg = i < 0;
  uint64_t u = neg ? (uint64_t) (0 - (uint64_t) i) : (uint64_t) i;
  if (u == 0) {
    tmp[--idx] = '0';
  }
  while (u) {
    tmp[--idx] = (char) ('0' + u % 10);
    u /= 10;
  }
  if (neg) {
    tmp[--idx] = '-';
  }
  return msNewStr(tmp + idx, (uint32_t) (20 - idx));
}

static MsValue strLen(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  return MS_INT_VAL((int64_t) s->len);
}

static bool isStr(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msStrType;
}

static MsValue strEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isStr(b)) {
    return MS_BOOL_VAL(false);
  }
  struct MsStrObj* sa = (struct MsStrObj*) MS_AS_OBJ(a);
  struct MsStrObj* sb = (struct MsStrObj*) MS_AS_OBJ(b);
  if (sa == sb) {
    return MS_BOOL_VAL(true);  // intern hit
  }
  if (sa->len != sb->len) {
    return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL(memcmp(sa->data, sb->data, sa->len) == 0);
}

static MsValue strHash(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  if (!s->hash) {
    s->hash = msFnv1a32(s->data, s->len);
    if (!s->hash) {
      s->hash = 1;  // avoid 0 (marks "not computed")
    }
  }
  return MS_INT_VAL((int64_t) (uint32_t) s->hash);
}

static MsValue strLt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isStr(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsStrObj* sa = (struct MsStrObj*) MS_AS_OBJ(a);
  struct MsStrObj* sb = (struct MsStrObj*) MS_AS_OBJ(b);
  int cmp = memcmp(sa->data, sb->data, sa->len < sb->len ? sa->len : sb->len);
  return MS_BOOL_VAL(cmp < 0 || (cmp == 0 && sa->len < sb->len));
}

static MsValue strAdd(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isStr(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  return msStrConcat(a, b);
}

static MsValue strMul(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_INT(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  int64_t n = MS_AS_INT(b);
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(a);
  if (n <= 0 || s->len == 0) {
    return msNewStr("", 0);
  }
  if ((uint64_t) n > UINT32_MAX / s->len) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  uint32_t newLen = (uint32_t) (s->len * (uint32_t) n);
  char* buf = msAlloc(newLen);
  for (int64_t i = 0; i < n; i++) {
    memcpy(buf + i * s->len, s->data, s->len);
  }
  MsValue r = msNewStrNoIntern(buf, newLen);
  msFree(buf);
  return r;
}

// s[a:b:step] -> new str (byte range, T065); msSliceCount sizes buf exactly,
// so bytes are only visited once (fill pass), same approach as
// ms_list.c's listGetSlice.
static MsValue strGetSlice(struct MsStrObj* s, MsValue idx) {
  struct MsSliceObj* sl = (struct MsSliceObj*) MS_AS_OBJ(idx);
  if (msSliceStepIsZero(sl)) {
    return MS_ERROR_VALUE;  // ValueError: slice step cannot be zero (T080 placeholder)
  }
  int64_t start, stop, step;
  msSliceNormalize(sl, s->len, &start, &stop, &step);
  uint32_t n = (uint32_t) msSliceCount(start, stop, step);
  char* buf = msAlloc(n == 0 ? 1 : n);
  if (step == 1) {
    memcpy(buf, s->data + start, n);
  } else {
    uint32_t w = 0;
    for (int64_t i = start; step > 0 ? i < stop : i > stop; i += step) {
      buf[w++] = s->data[i];
    }
  }
  MsValue r = msNewStrNoIntern(buf, n);
  msFree(buf);
  return r;
}

static MsValue strGetItem(struct MsVM* vm, MsValue v, MsValue idx) {
  (void) vm;
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  if (msIsSlice(idx)) {
    return strGetSlice(s, idx);
  }
  if (!MS_IS_INT(idx)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  int64_t i = MS_AS_INT(idx);
  if (i < 0) {
    i += s->len;
  }
  if (i < 0 || i >= s->len) {
    return MS_ERROR_VALUE;  // IndexError (T080 placeholder)
  }
  return MS_INT_VAL((uint8_t) s->data[i]);
}

static size_t strVarSize(const struct MsObject* obj) {
  const struct MsStrObj* s = (const struct MsStrObj*) obj;
  return sizeof(struct MsStrObj) + s->len + 1;
}

// Shared substring search: first index >= lo (and with the match fully
// inside [lo, hi)) where needle occurs in hay, or -1 if not found. An empty
// needle matches at lo. Backs strContains/msStrIndex/msStrReplace so all
// three agree on one search primitive instead of three hand-rolled loops.
static int64_t findSub(const char* hay, const char* needle, uint32_t needleLen, uint32_t lo, uint32_t hi) {
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

static MsValue strContains(struct MsVM* vm, MsValue v, MsValue item) {
  (void) vm;
  if (!isStr(item)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  struct MsStrObj* sub = (struct MsStrObj*) MS_AS_OBJ(item);
  return MS_BOOL_VAL(findSub(s->data, sub->data, sub->len, 0, s->len) != -1);
}

static MsValue strStr(struct MsVM* vm, MsValue v) {
  (void) vm;
  return v;  // str(s) is identity: no quotes, same content
}

static MsValue strRepr(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  uint32_t newLen = s->len + 2;
  char* buf = msAlloc(newLen);
  buf[0] = '"';
  memcpy(buf + 1, s->data, s->len);
  buf[newLen - 1] = '"';
  MsValue r = msNewStrNoIntern(buf, newLen);
  msFree(buf);
  return r;
}

// Number of UTF-8 bytes in the codepoint starting with leading byte b; an
// invalid leading byte (a stray continuation byte pattern) is treated as a
// single byte so the iterator always makes forward progress.
static uint32_t utf8CharLen(uint8_t b) {
  if ((b & 0x80) == 0x00) {
    return 1;
  }
  if ((b & 0xE0) == 0xC0) {
    return 2;
  }
  if ((b & 0xF0) == 0xE0) {
    return 3;
  }
  if ((b & 0xF8) == 0xF0) {
    return 4;
  }
  return 1;
}

// Iterator over a str, by Unicode codepoint (type-system.md ss2.5: "for ch in
// s 按 Unicode 码点迭代"). str field is stored as MsValue so traverse can
// visit the slot in place, same convention as ms_list.c's MsListIterObj.
struct MsStrIterObj {
  struct MsObject head;
  MsValue str;
  uint32_t idx;  // byte offset
};

static MsValue strIterNext(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsStrIterObj* it = (struct MsStrIterObj*) MS_AS_OBJ(v);
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(it->str);
  if (it->idx >= s->len) {
    return MS_NIL_VAL;  // StopIteration marker via nil (T065 protocol)
  }
  uint32_t clen = utf8CharLen((uint8_t) s->data[it->idx]);
  if (it->idx + clen > s->len) {
    clen = s->len - it->idx;  // truncated/invalid trailing bytes: don't overrun
  }
  MsValue ch = msNewStr(s->data + it->idx, clen);
  it->idx += clen;
  return ch;
}

// traverse: visit the str reference slot, supporting a future moving GC
// (Cheney copying, T116) in place, same rationale as ms_list.c's
// listIterTraverse.
static void strIterTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsStrIterObj* it = (struct MsStrIterObj*) obj;
  visit(&it->str, ctx);
}

static struct MsType msStrIterType = {
    .name = "str_iterator",
    .objSize = sizeof(struct MsStrIterObj),
    .traverse = strIterTraverse,
    .tpIter = msIterSelf,
    .tpNext = strIterNext,
};

static MsValue strIter(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsObject* obj = msGCAlloc(&msStrIterType, sizeof(struct MsStrIterObj));
  struct MsStrIterObj* it = (struct MsStrIterObj*) obj;
  it->str = v;
  it->idx = 0;
  return MS_OBJ_VAL(it);
}

struct MsType msStrType = {
    .name = "str",
    .objSize = sizeof(struct MsStrObj),
    .varSize = strVarSize,
    .traverse = NULL,  // no child objects (inline storage)
    .destroy = NULL,   // msGCAlloc-allocated, GC reclaims directly
    .tpRepr = strRepr,
    .tpStr = strStr,
    .tpHash = strHash,
    .tpEq = strEq,
    .tpLt = strLt,
    .tpLen = strLen,
    .tpAdd = strAdd,
    .tpMul = strMul,
    .tpGetitem = strGetItem,
    .tpIter = strIter,
    .tpContains = strContains,
};

MsValue msStrCodepointCount(MsValue v) {
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  if (s->cpLen == UINT32_MAX) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < s->len; i++) {
      if (((uint8_t) s->data[i] & 0xC0) != 0x80) {  // not a UTF-8 continuation byte
        count++;
      }
    }
    s->cpLen = count;
  }
  return MS_INT_VAL((int64_t) s->cpLen);
}

static char toUpperAscii(char c) {
  return (c >= 'a' && c <= 'z') ? (char) (c - ('a' - 'A')) : c;
}

static char toLowerAscii(char c) {
  return (c >= 'A' && c <= 'Z') ? (char) (c + ('a' - 'A')) : c;
}

static MsValue mapChars(MsValue v, char (*fn)(char)) {
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  char* buf = msAlloc(s->len);
  for (uint32_t i = 0; i < s->len; i++) {
    buf[i] = fn(s->data[i]);
  }
  MsValue r = msNewStrNoIntern(buf, s->len);
  msFree(buf);
  return r;
}

MsValue msStrUpper(MsValue v) {
  return mapChars(v, toUpperAscii);
}

MsValue msStrLower(MsValue v) {
  return mapChars(v, toLowerAscii);
}

static bool isStripChar(char c, const char* chars, uint32_t charsLen) {
  if (!chars) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  }
  for (uint32_t i = 0; i < charsLen; i++) {
    if (chars[i] == c) {
      return true;
    }
  }
  return false;
}

// Scans forward from 0, stopping at the first non-strip-char byte.
static uint32_t stripLoBound(const char* data, uint32_t len, const char* chars, uint32_t charsLen) {
  uint32_t lo = 0;
  while (lo < len && isStripChar(data[lo], chars, charsLen)) {
    lo++;
  }
  return lo;
}

// Scans backward from len, stopping at floor or the first non-strip-char byte.
static uint32_t stripHiBound(const char* data, uint32_t len, uint32_t floor, const char* chars, uint32_t charsLen) {
  uint32_t hi = len;
  while (hi > floor && isStripChar(data[hi - 1], chars, charsLen)) {
    hi--;
  }
  return hi;
}

MsValue msStrStrip(MsValue v, const char* chars, uint32_t charsLen) {
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  uint32_t lo = stripLoBound(s->data, s->len, chars, charsLen);
  uint32_t hi = stripHiBound(s->data, s->len, lo, chars, charsLen);
  return msNewStr(s->data + lo, hi - lo);
}

MsValue msStrLStrip(MsValue v, const char* chars, uint32_t charsLen) {
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  uint32_t lo = stripLoBound(s->data, s->len, chars, charsLen);
  return msNewStr(s->data + lo, s->len - lo);
}

MsValue msStrRStrip(MsValue v, const char* chars, uint32_t charsLen) {
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  uint32_t hi = stripHiBound(s->data, s->len, 0, chars, charsLen);
  return msNewStr(s->data, hi);
}

MsValue msStrHasPrefix(MsValue v, MsValue prefix) {
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  struct MsStrObj* p = (struct MsStrObj*) MS_AS_OBJ(prefix);
  if (p->len > s->len) {
    return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL(memcmp(s->data, p->data, p->len) == 0);
}

MsValue msStrHasSuffix(MsValue v, MsValue suffix) {
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  struct MsStrObj* p = (struct MsStrObj*) MS_AS_OBJ(suffix);
  if (p->len > s->len) {
    return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL(memcmp(s->data + (s->len - p->len), p->data, p->len) == 0);
}

MsValue msStrIndex(MsValue v, MsValue sub, int64_t start, int64_t end) {
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  struct MsStrObj* sub_ = (struct MsStrObj*) MS_AS_OBJ(sub);
  uint32_t lo = start < 0 ? 0 : (uint32_t) start;
  uint32_t hi = end < 0 || (uint32_t) end > s->len ? s->len : (uint32_t) end;
  if (lo > s->len) {
    return MS_INT_VAL(-1);
  }
  return MS_INT_VAL(findSub(s->data, sub_->data, sub_->len, lo, hi));
}

MsValue msStrReplace(MsValue v, MsValue oldSub, MsValue newSub, int64_t count) {
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(v);
  struct MsStrObj* o = (struct MsStrObj*) MS_AS_OBJ(oldSub);
  struct MsStrObj* n = (struct MsStrObj*) MS_AS_OBJ(newSub);
  if (o->len == 0) {
    return msNewStr(s->data, s->len);
  }
  // Two-pass: first count matched occurrences (bounded by count) via
  // findSub, then build the result buffer of the exact final size using the
  // same search so the two passes can never disagree on match positions.
  uint32_t matches = 0;
  for (uint32_t i = 0; count < 0 || (int64_t) matches < count;) {
    int64_t pos = findSub(s->data, o->data, o->len, i, s->len);
    if (pos < 0) {
      break;
    }
    matches++;
    i = (uint32_t) pos + o->len;
  }
  uint32_t newLen = s->len - matches * o->len + matches * n->len;
  char* buf = msAlloc(newLen == 0 ? 1 : newLen);
  uint32_t w = 0, i = 0;
  for (uint32_t replaced = 0; replaced < matches; replaced++) {
    uint32_t pos = (uint32_t) findSub(s->data, o->data, o->len, i, s->len);
    memcpy(buf + w, s->data + i, pos - i);
    w += pos - i;
    memcpy(buf + w, n->data, n->len);
    w += n->len;
    i = pos + o->len;
  }
  memcpy(buf + w, s->data + i, s->len - i);
  MsValue r = msNewStr(buf, newLen);
  msFree(buf);
  return r;
}
