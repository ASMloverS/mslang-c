// ms_object.h -- MsObject/MsType complete definitions and type-slot function-pointer aliases
#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations (self-contained: does not depend on include order of other headers)
struct MsObject;
struct MsType;
struct MsValue;
struct MsVM;

// Function-pointer type aliases (authoritative signatures: type-system.md ss1.3)
// GC sub-reference visitor: traverse calls visit once for each MsValue slot in obj
// that holds a heap reference; GC may update the slot in-place (e.g. semi-space forwarding).
typedef void (*MsVisitFn)(struct MsValue* slot, void* ctx);
typedef void (*MsTraverseFn)(struct MsObject* obj, MsVisitFn visit, void* ctx);
typedef void (*MsDestroyFn)(struct MsObject* obj);
// MsCallFn is isomorphic to MsCFunction (c-api.md ss6.1)
typedef struct MsValue (*MsCallFn)(struct MsVM* vm, struct MsValue* argv, int argc);
typedef struct MsValue (*MsBinaryFn)(struct MsVM* vm, struct MsValue a, struct MsValue b);
typedef struct MsValue (*MsUnaryFn)(struct MsVM* vm, struct MsValue a);
typedef struct MsValue (*MsTernaryFn)(struct MsVM* vm, struct MsValue a, struct MsValue b, struct MsValue c);
typedef size_t (*MsSizeFn)(const struct MsObject* obj);

// gcFlags bit layout (gc.md ss1)
typedef enum MsGcFlags {
  MS_GC_MARK = 0x01,      // bit 0: mark bit (mark-sweep)
  MS_GC_GEN_MASK = 0x06,  // bits 1-2: generation (0=young 1=mid 2=old)
  MS_GC_GEN_SHIFT = 1,
  MS_GC_FORWARDED = 0x08,    // bit 3: already copied (semi-space forwarded)
  MS_GC_FINALIZABLE = 0x10,  // bit 4: finalizable (has __del__)
} MsGcFlags;

// Heap object header shared by all heap-allocated objects (type-system.md ss2).
struct MsObject {
  union {
    struct MsType* type;   // normal state: type descriptor pointer (must be first member)
    struct MsObject* fwd;  // reused as to-space target when MS_GC_FORWARDED is set
  };
  uint32_t gcFlags;  // GC mark bit, generation bits, forwarding flag
};

// Type descriptor: method table, type slots, MRO (type-system.md ss3).
struct MsType {
  const char* name;       // type name, e.g. "int"/"str"/"Dog"
  size_t objSize;         // fixed-size objects: sizeof(concrete object); var-size: header size
  MsSizeFn varSize;       // var-size object allocation-size callback (NULL = use objSize)
  MsTraverseFn traverse;  // GC traversal of child objects
  MsDestroyFn destroy;    // object destructor (called by GC)
  MsCallFn call;          // __call__ default implementation
  // Built-in type slots: tpAdd, tpStr, tpEq, tpHash, tpGetitem, tpSetitem ...
  MsBinaryFn tpAdd;
  MsBinaryFn tpSub;
  MsBinaryFn tpMul;
  MsBinaryFn tpDiv;
  MsBinaryFn tpMod;
  MsBinaryFn tpPow;
  MsBinaryFn tpEq;
  MsBinaryFn tpNe;
  MsBinaryFn tpLt;
  MsBinaryFn tpLe;
  MsBinaryFn tpGt;
  MsBinaryFn tpGe;
  MsUnaryFn tpNeg;
  MsUnaryFn tpNot;
  MsUnaryFn tpPos;     // __pos__
  MsUnaryFn tpInvert;  // __invert__
  MsUnaryFn tpHash;
  MsUnaryFn tpStr;
  MsUnaryFn tpRepr;
  MsUnaryFn tpBool;
  MsUnaryFn tpLen;
  MsBinaryFn tpGetitem;
  MsTernaryFn tpSetitem;
  MsUnaryFn tpIter;
  MsUnaryFn tpNext;
  MsBinaryFn tpContains;  // __contains__
  // User-class extra fields
  struct MsObject* baseClass;  // parent class (single inheritance)
  struct MsObject* mro;        // precomputed MRO list
  struct MsObject* methods;    // method/class-attr dict (struct MsMap; NULL until T073)
};

static_assert(sizeof(struct MsObject) <= 24, "MsObject too large");
