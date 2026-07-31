// ms_gc.c -- simple single-threaded stop-the-world mark-sweep GC (P4 baseline)
#include "mslang/ms_gc.h"

#include <string.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_object.h"
#include "mslang/ms_upvalue.h"
#include "mslang/ms_vm.h"

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
  // Two passes, same reason as sweep()'s below: a user-defined class object
  // (MsTypeObj, T072) is itself GC-managed, and its instances' head.type
  // points directly into that object's memory -- destroying/freeing objects
  // in allocation order (as a single combined pass would) frees the class
  // before its instances are ever reached, leaving their head.type dangling.
  for (uint32_t i = 0; i < MsVecLen(&gGC.allObjects); i++) {
    struct MsObject* obj = MsVecAt(&gGC.allObjects, i);
    if (obj->type && obj->type->destroy) {
      obj->type->destroy(obj);
    }
  }
  for (uint32_t i = 0; i < MsVecLen(&gGC.allObjects); i++) {
    msFree(MsVecAt(&gGC.allObjects, i));
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

  // main thread's live value stack + global namespace (T067 introduced
  // t->globals; both were never enumerated as roots before, so a collection
  // firing while values were only reachable through them would free live
  // data -- full multi-thread/coroutine root enumeration is P10-T117).
  MsThread* t = &gVM.mainThread;
  for (MsValue* slot = t->stack; slot < t->sp; slot++) {
    markValue(*slot);
  }
  markValue(t->globals);

  // Open upvalues (T071): a captured local not yet reachable through any
  // closure's upvalues[] (e.g. mid-OP_MAKE_FUNC) must still survive collection.
  for (struct MsUpvalue* uv = t->openUpvalues; uv; uv = uv->nextOpen) {
    markValue(MS_OBJ_VAL(uv));
  }
}

static void sweep(void) {
  // Two passes: a class object and its last instance can both die in the
  // same sweep (T072's MsTypeObj is itself GC-managed, and an instance's
  // head.type points directly into it), and the class is always earlier in
  // allObjects (defined before it can be instantiated) -- destroying/sizing
  // and freeing interleaved in one pass would free the class before the
  // instance's own turn, leaving its head.type/varSize/objSize lookup
  // dangling. Tally sizes and destroy everything dead first, while every
  // dead object's memory is still intact; free only in the second pass.
  size_t freedBytes = 0;
  uint32_t freedCount = 0;
  for (uint32_t i = 0; i < MsVecLen(&gGC.allObjects); i++) {
    struct MsObject* obj = MsVecAt(&gGC.allObjects, i);
    if (obj->gcFlags & MS_GC_MARK) {
      continue;
    }
    freedBytes += (obj->type && obj->type->varSize) ? obj->type->varSize(obj)
                  : obj->type                       ? obj->type->objSize
                                                    : sizeof(struct MsObject);
    freedCount++;
    if (obj->type && obj->type->destroy) {
      obj->type->destroy(obj);
    }
  }

  uint32_t writeIdx = 0;
  for (uint32_t i = 0; i < MsVecLen(&gGC.allObjects); i++) {
    struct MsObject* obj = MsVecAt(&gGC.allObjects, i);
    if (obj->gcFlags & MS_GC_MARK) {
      obj->gcFlags &= ~(uint32_t) MS_GC_MARK;
      MsVecAt(&gGC.allObjects, writeIdx++) = obj;
    } else {
      msFree(obj);
    }
  }
  gGC.allObjects.len = writeIdx;
  gGC.bytesAlloc -= freedBytes;
  gGC.numObjects -= freedCount;
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
