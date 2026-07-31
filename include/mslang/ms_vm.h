// ms_vm.h -- MsFrame/MsThread/MsVM complete definitions and eval loop entry points (T051)
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mslang/ms_chunk.h"
#include "mslang/ms_value.h"

struct MsObject;
struct MsType;
struct ExceptEntry;
struct MsCoroutine;
struct MsUpvalue;

// Opaque module pointer (complete definition T086)
typedef struct MsModule MsModule;

#define MS_STACK_MAX (1024 * 256)  // max stack depth (256K MsValue slots)

// Call frame: one activation record per function call (vm.md ss4).
typedef struct MsFrame {
  struct MsChunk* chunk;     // bytecode chunk of the current function
  uint8_t* ip;               // instruction pointer into chunk->code
  MsValue* slots;            // base of this frame's stack window
  uint32_t slotCount;        // slots allocated for this frame (locals + upvalues)
  struct MsObject* closure;  // current function's closure object (MsClosure*)
  struct MsFrame* caller;    // caller frame (linked list)
  bool isCtor;               // T072: true => OP_RETURN/OP_RETURN_NIL substitute slots[0] (self) for the return value
  // T073: true for a bound-method call frame (dispatchBoundMethodCall
  // overwrote the callee slot in place with self, same stack-shape trick as
  // a ctor frame) -- OP_RETURN/OP_RETURN_NIL must rewind sp to frame->slots
  // (not frame->slots - 1) same as isCtor, but WITHOUT substituting self for
  // the return value (unlike isCtor, a bound method returns its own value).
  bool boundCall;
} MsFrame;

// Per-thread VM state: shared value stack + call frame chain (vm.md ss4).
typedef struct MsThread {
  MsValue stack[MS_STACK_MAX];      // shared value stack (statically sized)
  MsValue* sp;                      // stack top pointer (next free slot)
  MsFrame* topFrame;                // current frame (top of the frame chain)
  MsValue globals;                  // global namespace (MsMap*; MS_NIL_VAL stub until T060)
  MsValue exception;                // exception currently propagating (MS_NIL_VAL = none)
  struct ExceptEntry* exceptStack;  // exception handler stack (errors.md ss5.1; NULL before T079-T083)
  struct MsCoroutine* coro;         // owning coroutine (NULL before P9 concurrency)
  struct MsUpvalue* openUpvalues;   // open upvalue chain, sorted by location descending (vm.md ss5)
} MsThread;

// Global VM state (single instance, gVM).
// Note: GC state is not duplicated here -- msGCInit/msGCAlloc/msGCCollect (ms_gc.h)
// already operate on the process-wide singleton gGC, so MsVM holds no MsGC member.
typedef struct MsVM {
  MsThread mainThread;
  // Built-in types (filled in by T053-T066)
  struct MsType* intType;
  struct MsType* floatType;
  struct MsType* boolType;
  struct MsType* nilType;
  struct MsType* strType;
  struct MsType* bytesType;
  struct MsType* listType;
  struct MsType* mapType;
  struct MsType* tupleType;
  struct MsType* setType;
} MsVM;

extern MsVM gVM;

// Dispatches a value to its type descriptor (vm.md ss6); gVM.xxxType slots
// are filled in incrementally by T053-T066.
struct MsType* msTypeOf(MsValue v);

// Shared tpIter slot for every iterator type: an iterator is its own
// iterator, so iter(iter(x)) == iter(x) (T065).
MsValue msIterSelf(struct MsVM* vm, MsValue v);

void msVMInit(void);
void msVMShutdown(void);
MsValue msVMRun(struct MsChunk* chunk);  // top-level execution
MsValue msVMRunFile(const char* path);   // `run` sub-command entry point
