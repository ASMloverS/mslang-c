// ms_str.h -- msStrType public declarations: MsStrObj + constructors + methods (type-system.md ss2.5)
#pragma once

#include <stdint.h>

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsVM;

// UTF-8 immutable string (type-system.md ss2.5). head must be the first
// member (msGCAlloc/GC treat every heap object as struct MsObject*).
struct MsStrObj {
  struct MsObject head;
  uint32_t len;    // UTF-8 byte count
  uint32_t cpLen;  // cached Unicode codepoint count (UINT32_MAX = not computed)
  uint32_t hash;   // cached FNV-1a hash (0 = not computed)
  char data[];     // inline storage (flexible array member)
};

extern struct MsType msStrType;

// Create a string, interning it (sharing a single MsStrObj*) when
// len <= 64 bytes and an equal-content string is already interned.
MsValue msNewStr(const char* data, uint32_t len);

// Create a string without attempting to intern it (large strings).
MsValue msNewStrNoIntern(const char* data, uint32_t len);

// Concatenate two strings into a new string (O(n)).
MsValue msStrConcat(MsValue a, MsValue b);

// Format an int64_t as a string (fast path, no intermediate heap buffer).
MsValue msStrFromInt(int64_t i);

// Reset the intern table (called once per VM lifecycle by msVMInit(); the
// intern table is process-wide static state and must not leak stale
// pointers across msVMShutdown()/msVMInit() cycles).
void msStrInternInit(void);

// str methods (type-system.md ss2.5 / stdlib/strings.md); not yet wired to
// `.method()` call syntax (attribute/method dispatch lands in T073), exposed
// here as plain C functions for direct use and testing.
MsValue msStrCodepointCount(MsValue v);
MsValue msStrUpper(MsValue v);
MsValue msStrLower(MsValue v);
// chars == NULL means the default whitespace set " \t\n\r".
MsValue msStrStrip(MsValue v, const char* chars, uint32_t charsLen);
MsValue msStrLStrip(MsValue v, const char* chars, uint32_t charsLen);
MsValue msStrRStrip(MsValue v, const char* chars, uint32_t charsLen);
MsValue msStrHasPrefix(MsValue v, MsValue prefix);
MsValue msStrHasSuffix(MsValue v, MsValue suffix);
// start/end are byte offsets; end < 0 means "search to end of string".
// Returns MS_INT_VAL(-1) if sub is not found (not an error).
MsValue msStrIndex(MsValue v, MsValue sub, int64_t start, int64_t end);
// count < 0 means "replace all occurrences".
MsValue msStrReplace(MsValue v, MsValue oldSub, MsValue newSub, int64_t count);
