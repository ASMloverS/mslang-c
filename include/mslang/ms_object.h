// ms_object.h -- MsObject/MsType forward declarations and type-slot function-pointer aliases
//               (complete definitions in T049)
#pragma once

#include <stddef.h>

// Forward declarations (self-contained: does not depend on include order of other headers)
struct MsObject;
struct MsType;
struct MsValue;
struct MsVM;

// Function-pointer type aliases (authoritative signatures: type-system.md ss1.3)
// GC sub-reference visitor: traverse calls visit once for each MsValue slot in obj
// that holds a heap reference; GC may update the slot in-place (e.g. semi-space forwarding).
typedef void     (*MsVisitFn)   (struct MsValue* slot, void* ctx);
typedef void     (*MsTraverseFn)(struct MsObject* obj, MsVisitFn visit, void* ctx);
typedef void     (*MsDestroyFn) (struct MsObject* obj);
// MsCallFn is isomorphic to MsCFunction (c-api.md ss6.1)
typedef struct MsValue (*MsCallFn)    (struct MsVM* vm,
                                       struct MsValue* argv, int argc);
typedef struct MsValue (*MsBinaryFn)  (struct MsVM* vm,
                                       struct MsValue a, struct MsValue b);
typedef struct MsValue (*MsUnaryFn)   (struct MsVM* vm, struct MsValue a);
typedef struct MsValue (*MsTernaryFn) (struct MsVM* vm,
                                       struct MsValue a, struct MsValue b,
                                       struct MsValue c);
typedef size_t   (*MsSizeFn)   (const struct MsObject* obj);
