// ms_func.c -- MsFuncProto/MsClosure runtime objects + call frame setup (P5-T068)
#include "mslang/ms_func.h"

#include <string.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_list.h"
#include "mslang/ms_map.h"
#include "mslang/ms_object.h"
#include "mslang/ms_str.h"
#include "mslang/ms_vm.h"

// Frame pool (vm.md ss4): the eval loop is a single goto-dispatch function,
// so call frames cannot live on the C call stack -- MsFrame instead forms an
// explicit linked list (MsFrame::caller) allocated from this pool, falling
// back to msAlloc once exhausted (deep recursion).
#define FRAME_POOL_SIZE 256
static MsFrame gFramePool[FRAME_POOL_SIZE];
static int gFramePoolTop = 0;

static MsFrame* msNewFrame(void) {
  MsFrame* f;
  if (gFramePoolTop < FRAME_POOL_SIZE) {
    f = &gFramePool[gFramePoolTop++];
  } else {
    f = MS_ALLOC(MsFrame);
  }
  f->isCtor = false;  // T072: a pool-reused frame may carry a stale true from a prior ctor call
  return f;
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
  if (proto->paramNames) {
    for (uint32_t i = 0; i < proto->arityMax; i++) {
      msFree((void*) proto->paramNames[i]);
    }
    msFree(proto->paramNames);
  }
  msFree(proto->paramNameLens);
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
  // T072 fix: chunk->constants (e.g. attribute-name string constants used as
  // OP_SET_ATTR/OP_GET_ATTR hash keys) are never traversed by anything --
  // funcProtoTraverse only visits proto->defaults -- so a string constant
  // reachable ONLY through some future instance's attrs-map key would be
  // freed out from under this chunk's constant slot the moment every holder
  // of that key dies in one GC sweep. Root the whole pool permanently, once
  // per chunk, same fix class as gInitNameVal (ms_vm.c msVMInit); nested
  // chunks are never freed so this never needs an unroot.
  for (uint32_t i = 0; i < chunk->constLen; i++) {
    msGCPushRoot(chunk->constants[i]);
  }
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
  for (uint8_t i = 0; i < cl->upvalueCount; i++) {
    if (cl->upvalues[i]) {
      MsValue uvVal = MS_OBJ_VAL(cl->upvalues[i]);
      visit(&uvVal, ctx);
    }
  }
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

// Right-to-left default lookup shared by msClosureCall/msClosureCallKw
// (defaults[0] is the last parameter's default -- ms_func.h). Returns false
// (leaving *out untouched) when slot i is before the first defaulted
// parameter or otherwise has no default.
static bool paramDefaultAt(MsFuncProto* proto, uint32_t i, MsValue* out) {
  if (i < proto->arity) {
    return false;
  }
  uint32_t defIdx = proto->defaultCount - 1 - (i - proto->arity);
  if (defIdx >= proto->defaultCount) {
    return false;
  }
  *out = proto->defaults[defIdx];
  return true;
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

  // Missing trailing arguments: fill from proto->defaults.
  for (uint32_t i = argc; i < proto->arityMax; i++) {
    MsValue def;
    *t->sp++ = paramDefaultAt(proto, i, &def) ? def : MS_NIL_VAL;
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

  // **kwargs collector (T070): OP_CALL/OP_CALL_EX never carry a kwargsMap, so
  // the collector slot is always an empty map here (msClosureCallKw is the
  // only path that can populate it from actual keyword arguments). t->sp is
  // already positioned at slots[proto->kwargsSlot] (== arityMax, or
  // arityMax+1 when hasVararg also collected a slot above -- ms_parse_expr.c's
  // msParseParamList enforces positional, then ...args, then **kwargs order).
  if (proto->hasKwarg) {
    newFrame->slots[proto->kwargsSlot] = msNewMap(4);
    t->sp++;
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

int msFindParamSlot(MsFuncProto* proto, MsValue name) {
  if (!MS_IS_OBJ(name) || MS_AS_OBJ(name)->type != &msStrType) {
    return -1;
  }
  struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(name);
  for (uint32_t i = 0; i < proto->arityMax; i++) {
    if (proto->paramNameLens[i] == s->len && memcmp(proto->paramNames[i], s->data, s->len) == 0) {
      return (int) i;
    }
  }
  return -1;
}

// T070: binds posArgc positional args + a kwargsMap by parameter name.
// Everything is validated using a C-local scratch array (slotVals/filled)
// before any frame is allocated or the value stack is touched, so an error
// return never leaks a frame-pool slot and never leaves t->sp in a
// half-written state (unlike msClosureCall, whose only failure checks are
// pure argc comparisons that run before its own msNewFrame() call). The
// original stack region (callee, positional args, kwargsMap -- vm.md ss3.6)
// stays untouched and therefore GC-traced throughout validation, so
// kwargsMap's entries (and proto->defaults, reachable via cl) need no extra
// rooting; only the newly built kwargsExtra collector needs msGCPushRoot,
// same as msClosureCall's varargList.
struct MsFrame* msClosureCallKw(struct MsThread* t, MsClosure* cl, uint32_t posArgc, MsValue kwargsMap) {
  MsFuncProto* proto = cl->proto;
  if (proto->hasVararg) {
    // vararg + keyword-call mixing is out of scope (impl/P5-T070-kwargs.md
    // "风险与边界"): this function never allocates/collects a vararg slot, so
    // proceeding would leave it uninitialized (yet GC-traced) or overwrite
    // proto->kwargsSlot's binding. Reject instead of corrupting state.
    return NULL;  // TypeError: vararg functions do not support keyword calls (T080 placeholder)
  }
  if (posArgc > proto->arityMax) {
    return NULL;  // TypeError: too many positional arguments (T080 placeholder)
  }
  MsValue* posArgs = t->sp - posArgc - 1;  // below kwargsMap (top of stack), at pos_arg0

  MsValue slotVals[256];  // arityMax upper bound -- ms_scope.c's 256-local cap
  bool filled[256] = {false};
  for (uint32_t i = 0; i < posArgc; i++) {
    slotVals[i] = posArgs[i];
    filled[i] = true;
  }

  MsValue kwargsExtra = MS_NIL_VAL;
  if (proto->hasKwarg) {
    kwargsExtra = msNewMap(4);
    msGCPushRoot(kwargsExtra);
  }
  MsFrame* result = NULL;

  struct MsMapObj* km = (struct MsMapObj*) MS_AS_OBJ(kwargsMap);
  for (uint32_t i = 0; i < km->cap; i++) {
    struct MsMapEntry* e = &km->entries[i];
    if (!e->occupied) {
      continue;
    }
    int slot = msFindParamSlot(proto, e->key);
    if (slot < 0) {
      if (!proto->hasKwarg) {
        goto cleanup;  // TypeError: unexpected keyword argument (T080 placeholder)
      }
      msMapSet(&gVM, kwargsExtra, e->key, e->value);
      continue;
    }
    if (filled[slot]) {
      goto cleanup;  // TypeError: got multiple values for argument (T080 placeholder)
    }
    slotVals[slot] = e->value;
    filled[slot] = true;
  }

  for (uint32_t i = 0; i < proto->arityMax; i++) {
    if (filled[i]) {
      continue;
    }
    MsValue def;
    if (!paramDefaultAt(proto, i, &def)) {
      goto cleanup;  // TypeError: missing required argument (T080 placeholder)
    }
    slotVals[i] = def;
  }

  {
    MsFrame* newFrame = msNewFrame();
    newFrame->chunk = proto->chunk;
    newFrame->ip = proto->chunk->code;
    newFrame->closure = (struct MsObject*) cl;
    newFrame->caller = t->topFrame;
    newFrame->slots = posArgs;
    newFrame->slotCount = proto->localCount;

    t->sp = posArgs;
    for (uint32_t i = 0; i < proto->arityMax; i++) {
      *t->sp++ = slotVals[i];
    }
    if (proto->hasKwarg) {
      newFrame->slots[proto->kwargsSlot] = kwargsExtra;
      t->sp++;
    }
    while (t->sp < newFrame->slots + newFrame->slotCount) {
      *t->sp++ = MS_NIL_VAL;
    }

    t->topFrame = newFrame;
    result = newFrame;
  }

cleanup:
  if (proto->hasKwarg) {
    msGCPopRoot();
  }
  return result;
}
