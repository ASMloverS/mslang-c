// ms_func.h -- MsFuncProto / MsClosure runtime objects + call convention (P5-T068)
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

struct MsChunk;
struct MsThread;
struct MsFrame;
struct MsUpvalue;

// Function prototype: one per source-level function definition (compile-time
// constant, referenced by OP_MAKE_FUNC's funcIdx operand into the enclosing
// chunk's constant pool -- vm.md ss3.9/ss4).
typedef struct MsFuncProto {
  struct MsObject header;
  struct MsChunk* chunk;
  const char* name;   // NUL-terminated, owned copy; NULL for anonymous functions
  uint32_t arity;     // required argument count
  uint32_t arityMax;  // max positional argument count (excludes *args, T069)
  uint32_t defaultCount;
  MsValue* defaults;    // right-to-left: defaults[0] is the last parameter's default
  uint32_t localCount;  // local slot count reserved by the call frame (params today)
  uint8_t upvalueCount;
  bool isAsync;
  bool hasVararg;           // T069
  bool hasKwarg;            // T070
  uint32_t kwOnlyCount;     // T070 (reserved; kw-only params not implemented yet)
  const char** paramNames;  // T070: owned copies, length arityMax (excludes vararg/kwarg slot names)
  uint32_t* paramNameLens;  // T070: byte length per paramNames[i] (not NUL-terminated)
  uint32_t kwargsSlot;      // T070: local slot index of the **kwargs collector (valid when hasKwarg)
} MsFuncProto;

// Runtime closure object: wraps a MsFuncProto with its captured upvalues
// (vm.md ss5). upvalues[] entries are filled in by OP_MAKE_FUNC (P5-T071).
typedef struct MsClosure {
  struct MsObject header;
  MsFuncProto* proto;
  uint8_t upvalueCount;
  // P5-T075: class this closure was defined in, backfilled by OP_MAKE_CLASS
  // for method closures (MS_OBJ_VAL(tp)); MS_NIL_VAL for OP_MAKE_FUNC
  // closures (not a method). Read by OP_LOAD_SUPER as the MRO scan's start.
  MsValue definingClass;
  struct MsUpvalue* upvalues[];
} MsClosure;

extern struct MsType msFuncProtoType;
extern struct MsType msClosureType;

// Allocates a MsFuncProto. `name` is copied (NUL-terminated) if non-NULL --
// the AST node it comes from does not outlive compilation. `defaults` is
// allocated with defaultCount slots, initially nil; OP_MAKE_FUNC fills them
// in from the default-value expressions compiled just before it in the
// enclosing chunk (evaluated once per OP_MAKE_FUNC execution -- "def time").
MsValue msNewFuncProto(
    struct MsChunk* chunk,
    const char* name,
    uint32_t arity,
    uint32_t arityMax,
    uint32_t defaultCount,
    uint32_t localCount);

// Allocates a MsClosure wrapping proto, with upvalueCount upvalue slots
// (zero-initialized; OP_MAKE_FUNC's caller fills them in).
MsValue msNewClosure(MsFuncProto* proto, uint8_t upvalueCount);

// Sets up a new call frame for calling closure cl with argc arguments already
// on the value stack (t->sp - argc .. t->sp). Binds arguments to local slots,
// fills missing trailing arguments from proto->defaults, checks arity, and
// installs the new frame as t->topFrame. Returns the new frame, or NULL on an
// arity mismatch (TypeError, T080 placeholder -- caller returns MS_ERROR_VALUE).
struct MsFrame* msClosureCall(struct MsThread* t, MsClosure* cl, uint32_t argc);

// Sets up a new call frame binding posArgc positional arguments (already on
// the value stack, same convention as msClosureCall) plus a kwargsMap (still
// on the stack above the positional args -- vm.md ss3.6's OP_CALL_KW stack
// layout) by parameter name. Excess keys are collected into proto->kwargsSlot
// when proto->hasKwarg; otherwise an unmatched key is a TypeError. Returns
// NULL on: too many positional arguments, an unknown keyword (no **kwargs
// collector), a keyword duplicating an already-bound positional argument, or
// a still-missing required argument (T080 placeholders -- caller returns
// MS_ERROR_VALUE).
struct MsFrame* msClosureCallKw(struct MsThread* t, MsClosure* cl, uint32_t posArgc, MsValue kwargsMap);

// Finds the local slot index for parameter `name` among proto->paramNames[0..
// arityMax). Returns -1 if name is not a string or does not match any
// parameter (O(arityMax) linear scan -- impl/P5-T070-kwargs.md "风险与边界":
// acceptable given typical arities).
int msFindParamSlot(MsFuncProto* proto, MsValue name);

// Releases a call frame obtained from msClosureCall's internal frame pool
// back to the pool (or frees it, if it was a malloc fallback). Called by
// ms_vm.c's OP_RETURN/OP_RETURN_NIL when popping a non-top-level frame.
void msFreeFrame(struct MsFrame* f);
