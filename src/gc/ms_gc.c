// ms_gc.c -- simple single-threaded stop-the-world mark-sweep GC (P4 baseline)
#include "mslang/ms_gc.h"

#include <string.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_object.h"

#define MS_GC_INITIAL_THRESHOLD (1024 * 1024)
#define MS_GC_MAX_THRESHOLD (64 * 1024 * 1024)

MsGC gGC;

void msGCInit(void) {
  MsVecInit(&gGC.allObjects);
  MsVecInit(&gGC.roots);
  gGC.bytesAlloc = 0;
  gGC.nextGC = MS_GC_INITIAL_THRESHOLD;
  gGC.numCollects = 0;
  gGC.numObjects = 0;
}

void msGCShutdown(void) {
  for (uint32_t i = 0; i < MsVecLen(&gGC.allObjects); i++) {
    struct MsObject* obj = MsVecAt(&gGC.allObjects, i);
    if (obj->type && obj->type->destroy) {
      obj->type->destroy(obj);
    }
    msFree(obj);
  }
  MsVecFree(&gGC.allObjects);
  MsVecFree(&gGC.roots);
}

struct MsObject* msGCAlloc(struct MsType* type, size_t size) {
  if (gGC.bytesAlloc + size > gGC.nextGC) {
    msGCCollect();
  }

  struct MsObject* obj = msAlloc(size);
  memset(obj, 0, size);
  obj->type = type;
  MsVecPush(&gGC.allObjects, obj);

  gGC.bytesAlloc += size;
  gGC.numObjects++;
  return obj;
}

static void markObject(struct MsObject* obj);

static void markValue(MsValue v) {
  if (MS_IS_OBJ(v)) {
    markObject(MS_AS_OBJ(v));
  }
}

static void markVisit(MsValue* slot, void* ctx) {
  (void) ctx;
  markValue(*slot);
}

static void markObject(struct MsObject* obj) {
  if (!obj || (obj->gcFlags & MS_GC_MARK)) {
    return;
  }
  obj->gcFlags |= MS_GC_MARK;
  if (obj->type && obj->type->traverse) {
    obj->type->traverse(obj, markVisit, NULL);
  }
}

static void markRoots(void) {
  for (uint32_t i = 0; i < MsVecLen(&gGC.roots); i++) {
    markValue(MsVecAt(&gGC.roots, i));
  }
}

static void sweep(void) {
  uint32_t writeIdx = 0;
  for (uint32_t i = 0; i < MsVecLen(&gGC.allObjects); i++) {
    struct MsObject* obj = MsVecAt(&gGC.allObjects, i);
    if (obj->gcFlags & MS_GC_MARK) {
      obj->gcFlags &= ~(uint32_t) MS_GC_MARK;
      MsVecAt(&gGC.allObjects, writeIdx++) = obj;
    } else {
      size_t size = (obj->type && obj->type->varSize) ? obj->type->varSize(obj)
                    : obj->type                       ? obj->type->objSize
                                                      : sizeof(struct MsObject);
      if (obj->type && obj->type->destroy) {
        obj->type->destroy(obj);
      }
      gGC.bytesAlloc -= size;
      gGC.numObjects--;
      msFree(obj);
    }
  }
  gGC.allObjects.len = writeIdx;
}

void msGCCollect(void) {
  markRoots();
  sweep();
  gGC.numCollects++;
  gGC.nextGC = (gGC.bytesAlloc * 2 < MS_GC_MAX_THRESHOLD) ? gGC.bytesAlloc * 2 : MS_GC_MAX_THRESHOLD;
}

void msGCPushRoot(MsValue v) {
  MsVecPush(&gGC.roots, v);
}

void msGCPopRoot(void) {
  gGC.roots.len--;
}
