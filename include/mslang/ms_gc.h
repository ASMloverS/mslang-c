// ms_gc.h -- simple single-threaded stop-the-world mark-sweep GC (P4 baseline)
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ms_value.h"
#include "ms_vec.h"

struct MsObject;
struct MsType;

// GC state (single global instance). allObjects is a non-intrusive side table
// tracking every live object (MsObject has no gcNext field); roots is the
// global GC root array managed by msGCPushRoot/msGCPopRoot.
typedef struct MsGC {
  MsVec(struct MsObject*) allObjects;
  MsVec(MsValue) roots;
  size_t bytesAlloc;
  size_t nextGC;
  uint32_t numCollects;
  uint32_t numObjects;
} MsGC;

extern MsGC gGC;

// Initialize the GC (call once at VM startup).
void msGCInit(void);

// Shut down the GC: frees every remaining object (call at VM teardown).
void msGCShutdown(void);

// Run one full mark-sweep collection.
void msGCCollect(void);

// Allocate a size-byte heap object and register it in the GC side table.
struct MsObject* msGCAlloc(struct MsType* type, size_t size);

// Register a value as a temporary GC root, protecting it from collection.
void msGCPushRoot(MsValue v);

// Pop the most recently pushed GC root.
void msGCPopRoot(void);
