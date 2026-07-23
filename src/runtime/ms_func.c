// ms_func.c -- MsFuncProto/MsClosure runtime objects + call frame setup (P5-T068)
#include "mslang/ms_func.h"

#include <string.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_list.h"
#include "mslang/ms_object.h"
#include "mslang/ms_vm.h"

// Frame pool (vm.md ss4): the eval loop is a single goto-dispatch function,
// so call frames cannot live on the C call stack -- MsFrame instead forms an
// explicit linked list (MsFrame::caller) allocated from this pool, falling
// back to msAlloc once exhausted (deep recursion).
#define FRAME_POOL_SIZE 256
static MsFrame gFramePool[FRAME_POOL_SIZE];
static int gFramePoolTop = 0;

static MsFrame* msNewFrame(void) {
  if (gFramePoolTop < FRAME_POOL_SIZE) {
    return &gFramePool[gFramePoolTop++];
  }
  return MS_ALLOC(MsFrame);
}

void msFreeFrame(MsFrame* f) {
  if (f >= gFramePool && f < gFramePool + FRAME_POOL_SIZE) {
    gFramePoolTop--;
  } else {
    msFree(f);
  }
}

static void funcProtoTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  MsFuncProto* proto = (MsFuncProto*) obj;
  for (uint32_t i = 0; i < proto->defaultCount; i++) {
    visit(&proto->defaults[i], ctx);
  }
}

static void funcProtoDestroy(struct MsObject* obj) {
  MsFuncProto* proto = (MsFuncProto*) obj;
  msFree(proto->defaults);
  msFree((void*) proto->name);
}

struct MsType msFuncProtoType = {
    .name = "function_proto",
    .objSize = sizeof(MsFuncProto),
    .traverse = funcProtoTraverse,
    .destroy = funcProtoDestroy,
};

MsValue msNewFuncProto(
    struct MsChunk* chunk,
    const char* name,
    uint32_t arity,
    uint32_t arityMax,
    uint32_t defaultCount,
    uint32_t localCount) {
  struct MsObject* obj = msGCAlloc(&msFuncProtoType, sizeof(MsFuncProto));
  MsFuncProto* proto = (MsFuncProto*) obj;
  proto->chunk = chunk;
  if (name) {
    size_t len = strlen(name);
    char* copy = MS_ALLOC_N(char, len + 1);
    memcpy(copy, name, len + 1);
    proto->name = copy;
  }
  proto->arity = arity;
  proto->arityMax = arityMax;
  proto->defaultCount = defaultCount;
  if (defaultCount > 0) {
    proto->defaults = MS_ALLOC_N(MsValue, defaultCount);
    for (uint32_t i = 0; i < defaultCount; i++) {
      proto->defaults[i] = MS_NIL_VAL;
    }
  }
  proto->localCount = localCount;
  return MS_OBJ_VAL(proto);
}

// closure->proto is a raw MsFuncProto* (not MsValue) so ms_vm.c's OP_CALL/
// OP_MAKE_FUNC can dereference it directly; wrap it in a temporary MsValue
// just for this mark-sweep visit (current GC never moves objects, so no
// slot-rewrite needs to be observed back through cl->proto -- ms_gc.c's
// header comment: "P4 baseline").
static void closureTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  MsClosure* cl = (MsClosure*) obj;
  MsValue protoVal = MS_OBJ_VAL(cl->proto);
  visit(&protoVal, ctx);
  // upvalues[] entries are a T071 stub (always NULL) until upvalue open/close
  // lands, so there is nothing else to traverse yet.
}

static size_t closureVarSize(const struct MsObject* obj) {
  return sizeof(MsClosure) + ((const MsClosure*) obj)->upvalueCount * sizeof(void*);
}

struct MsType msClosureType = {
    .name = "function",
    .objSize = sizeof(MsClosure),
    .varSize = closureVarSize,
    .traverse = closureTraverse,
};

MsValue msNewClosure(MsFuncProto* proto, uint8_t upvalueCount) {
  size_t size = sizeof(MsClosure) + (size_t) upvalueCount * sizeof(void*);
  struct MsObject* obj = msGCAlloc(&msClosureType, size);
  MsClosure* cl = (MsClosure*) obj;
  cl->proto = proto;
  cl->upvalueCount = upvalueCount;
  return MS_OBJ_VAL(cl);
}

struct MsFrame* msClosureCall(struct MsThread* t, MsClosure* cl, uint32_t argc) {
  MsFuncProto* proto = cl->proto;
  if (argc < proto->arity) {
    return NULL;  // TypeError: missing required argument(s) (T080 placeholder)
  }
  if (!proto->hasVararg && argc > proto->arityMax) {
    return NULL;  // TypeError: too many arguments (T080 placeholder)
  }

  MsFrame* newFrame = msNewFrame();
  newFrame->chunk = proto->chunk;
  newFrame->ip = proto->chunk->code;
  newFrame->closure = (struct MsObject*) cl;
  newFrame->caller = t->topFrame;
  newFrame->slots = t->sp - argc;
  newFrame->slotCount = proto->localCount;

  // Missing trailing arguments: fill from proto->defaults, stored
  // right-to-left (defaults[0] is the last parameter's default -- ms_func.h).
  for (uint32_t i = argc; i < proto->arityMax; i++) {
    uint32_t defIdx = proto->defaultCount - 1 - (i - proto->arity);
    *t->sp++ = defIdx < proto->defaultCount ? proto->defaults[defIdx] : MS_NIL_VAL;
  }

  // vararg collection (T069): must run after default filling and before the
  // local-slot padding below, otherwise both would race to write slots[arityMax]
  // (impl/P5-T069-vararg.md "实现要点 1"/"风险与边界").
  if (proto->hasVararg) {
    uint32_t varargCount = argc > proto->arityMax ? argc - proto->arityMax : 0;
    MsValue varargList = msNewList(varargCount);
    msGCPushRoot(varargList);  // guard against GC during msListAppend below
    struct MsListObj* vl = (struct MsListObj*) MS_AS_OBJ(varargList);
    for (uint32_t i = 0; i < varargCount; i++) {
      msListAppend(vl, newFrame->slots[proto->arityMax + i]);
    }
    msGCPopRoot();
    newFrame->slots[proto->arityMax] = varargList;
    t->sp = newFrame->slots + proto->arityMax + 1;  // drop the extra raw args
  }

  // Pad up to the reserved slot count (a no-op today -- ms_scope.c's
  // msScopeEnd already pops body locals as their block scope exits, so
  // proto->localCount never exceeds arityMax yet; kept for forward
  // compatibility with a future flat local-slot allocation strategy).
  while (t->sp < newFrame->slots + newFrame->slotCount) {
    *t->sp++ = MS_NIL_VAL;
  }

  t->topFrame = newFrame;
  return newFrame;
}
