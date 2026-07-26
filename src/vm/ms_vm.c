// ms_vm.c -- MsVM global state + main eval loop (switch dispatch)
#include "mslang/ms_vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_bool.h"
#include "mslang/ms_bytes.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_float.h"
#include "mslang/ms_func.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_int.h"
#include "mslang/ms_list.h"
#include "mslang/ms_map.h"
#include "mslang/ms_object.h"
#include "mslang/ms_opcode.h"
#include "mslang/ms_range.h"
#include "mslang/ms_set.h"
#include "mslang/ms_slice.h"
#include "mslang/ms_str.h"
#include "mslang/ms_tuple.h"

MsVM gVM;

// Native (C) function object (M1 minimal calling convention -- OP_CALL
// detects this type by identity and calls fn directly, bypassing the
// generic tpCall slot reserved for future __call__ instances; see the
// "OP_CALL 分派" risk note in impl/P4-T067-vm-e2e-m1.md). No traverse/destroy
// needed: fn is a process-lifetime C pointer, name a static string literal.
struct MsNativeFnObj {
  struct MsObject head;
  MsCallFn fn;
  const char* name;
};

struct MsType msNativeFnType = {
    .name = "native_function",
    .objSize = sizeof(struct MsNativeFnObj),
};

static MsValue msNewNativeFn(const char* name, MsCallFn fn) {
  struct MsObject* obj = msGCAlloc(&msNativeFnType, sizeof(struct MsNativeFnObj));
  struct MsNativeFnObj* nf = (struct MsNativeFnObj*) obj;
  nf->fn = fn;
  nf->name = name;
  return MS_OBJ_VAL(nf);
}

// Registers a C function under `name` in the global namespace. msVMInit-only:
// there is no bytecode instruction exposing this to script code.
static void msRegisterBuiltin(const char* name, MsCallFn fn) {
  MsValue key = msNewStr(name, (uint32_t) strlen(name));
  MsValue val = msNewNativeFn(name, fn);
  msMapSet(&gVM, gVM.mainThread.globals, key, val);
}

// Stack operation helper macros (t: MsThread*).
#define PUSH(v) (*t->sp++ = (v))
#define POP() (*--t->sp)
#define PEEK(n) (*(t->sp - 1 - (n)))
#define POKE(n, v) (*(t->sp - 1 - (n)) = (v))

// Read operands.
#define READ_BYTE() (*frame->ip++)
// AX: 3-byte big-endian operand (vm.md ss3); must not be shrunk to uint16.
#define READ_AX() (frame->ip += 3, ((uint32_t) frame->ip[-3] << 16) | ((uint32_t) frame->ip[-2] << 8) | frame->ip[-1])
// Jump operands are a 3-byte signed 24-bit AX (vm.md ss3); sign-extend via a
// left-then-right shift through int32_t.
#define READ_JUMP_OFFSET() ((int32_t) (READ_AX() << 8) >> 8)

struct MsType* msTypeOf(MsValue v) {
  switch (v.tag) {
    case MS_TAG_INT:
      return gVM.intType;
    case MS_TAG_FLOAT:
      return gVM.floatType;
    case MS_TAG_BOOL:
      return gVM.boolType;
    case MS_TAG_NIL:
      return gVM.nilType;
    case MS_TAG_OBJ:
      return MS_AS_OBJ(v)->type;
    default:
      return NULL;
  }
}

// Shared tpIter slot for every iterator type (list/map/set/tuple/str/range
// iterator): an iterator is its own iterator, so iter(iter(x)) == iter(x).
MsValue msIterSelf(struct MsVM* vm, MsValue v) {
  (void) vm;
  return v;
}

// Fallback when a's type has no matching slot (TypeError placeholder pre-T080).
static MsValue msNotImplemented(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  (void) a;
  (void) b;
  return MS_ERROR_VALUE;
}

// a < b via a's tpLt slot (TypeError placeholder pre-T080 if absent).
static MsValue msValueLt(MsValue a, MsValue b) {
  struct MsType* ta = msTypeOf(a);
  if (!ta->tpLt) {
    return MS_ERROR_VALUE;
  }
  return ta->tpLt(&gVM, a, b);
}

// a <= b: prefer a's tpLe slot (rich comparison, e.g. subset); otherwise fall
// back to not (b < a) (total-order types, unchanged behavior).
static MsValue msValueLe(MsValue a, MsValue b) {
  struct MsType* ta = msTypeOf(a);
  if (ta->tpLe) {
    return ta->tpLe(&gVM, a, b);
  }
  MsValue lt = msValueLt(b, a);
  if (MS_IS_ERROR(lt)) {
    return lt;
  }
  return MS_BOOL_VAL(!msValueTruthy(lt));
}

// a >= b: prefer a's tpGe slot; otherwise fall back to not (a < b).
static MsValue msValueGe(MsValue a, MsValue b) {
  struct MsType* ta = msTypeOf(a);
  if (ta->tpGe) {
    return ta->tpGe(&gVM, a, b);
  }
  MsValue lt = msValueLt(a, b);
  if (MS_IS_ERROR(lt)) {
    return lt;
  }
  return MS_BOOL_VAL(!msValueTruthy(lt));
}

// a > b: prefer a's tpGt slot; otherwise fall back to b < a.
static MsValue msValueGt(MsValue a, MsValue b) {
  struct MsType* ta = msTypeOf(a);
  if (ta->tpGt) {
    return ta->tpGt(&gVM, a, b);
  }
  return msValueLt(b, a);
}

// is: object identity, not __eq__ (type-system.md ss3.4).
static bool msValueIs(MsValue a, MsValue b) {
  if (a.tag != b.tag) {
    return false;
  }
  switch (a.tag) {
    case MS_TAG_NIL:
      return true;
    case MS_TAG_BOOL:
      return MS_AS_BOOL(a) == MS_AS_BOOL(b);
    case MS_TAG_INT:
      return MS_AS_INT(a) == MS_AS_INT(b);
    case MS_TAG_FLOAT:
      return MS_AS_FLOAT(a) == MS_AS_FLOAT(b);
    case MS_TAG_OBJ:
      return MS_AS_OBJ(a) == MS_AS_OBJ(b);
    default:
      return false;
  }
}

// in: dispatch to container's tpContains slot (list/map/set fill this in
// T059-T062); no generic tpIter/tpNext fallback (StopIteration sentinel is
// not settled until T065).
static MsValue msContains(MsValue container, MsValue item) {
  struct MsType* tc = msTypeOf(container);
  if (tc->tpContains) {
    return tc->tpContains(&gVM, container, item);
  }
  return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
}

// obj.name fallback when tp has no tpGetattr: look up name in tp's methods
// dict (MsMap*, T073 populates it for user classes; NULL for every built-in
// type today, so this always returns nil pre-T073).
static MsValue msTypeLookupMethod(struct MsType* tp, MsValue name) {
  if (!tp->methods) {
    return MS_NIL_VAL;
  }
  return msMapGet(&gVM, MS_OBJ_VAL(tp->methods), name);
}

// Binary arithmetic dispatch: pop b, a; call a's type slot (or bail via
// msNotImplemented); push the result. Returns (propagates) on error.
#define BINARY_OP(slot)                                                         \
  do {                                                                          \
    MsValue b = POP(), a = POP();                                               \
    struct MsType* ta = msTypeOf(a);                                            \
    MsValue r = ta->slot ? ta->slot(&gVM, a, b) : msNotImplemented(&gVM, a, b); \
    if (MS_IS_ERROR(r)) {                                                       \
      return r;                                                                 \
    }                                                                           \
    PUSH(r);                                                                    \
  } while (0)

// Unary arithmetic dispatch: mirrors BINARY_OP for one-operand ops.
#define UNARY_OP(slot)                                         \
  do {                                                         \
    MsValue a = POP();                                         \
    struct MsType* ta = msTypeOf(a);                           \
    MsValue r = ta->slot ? ta->slot(&gVM, a) : MS_ERROR_VALUE; \
    if (MS_IS_ERROR(r)) {                                      \
      return r;                                                \
    }                                                          \
    PUSH(r);                                                   \
  } while (0)

// BAND/BOR/BXOR: int operands compute inline; other operands fall back to the
// matching tpBitor/tpBitand/tpBitxor slot (same pattern as BINARY_OP), which
// set (T062) fills in for union/intersection/symmetric-difference.
#define BITWISE_OP(op, slot)                                                      \
  do {                                                                            \
    MsValue b = POP(), a = POP();                                                 \
    if (MS_IS_INT(a) && MS_IS_INT(b)) {                                           \
      PUSH(MS_INT_VAL(MS_AS_INT(a) op MS_AS_INT(b)));                             \
    } else {                                                                      \
      struct MsType* ta = msTypeOf(a);                                            \
      MsValue r = ta->slot ? ta->slot(&gVM, a, b) : msNotImplemented(&gVM, a, b); \
      if (MS_IS_ERROR(r)) {                                                       \
        return r;                                                                 \
      }                                                                           \
      PUSH(r);                                                                    \
    }                                                                             \
  } while (0)

// SHL/SHR: same int-only rule as BITWISE_OP, plus a shift-count bounds check;
// resultExpr differs per op (SHL casts through uint64_t to avoid UB on a
// negative left operand; SHR is a plain signed/arithmetic shift).
#define SHIFT_OP(resultExpr)              \
  do {                                    \
    MsValue b = POP(), a = POP();         \
    if (!MS_IS_INT(a) || !MS_IS_INT(b)) { \
      return MS_ERROR_VALUE;              \
    }                                     \
    int64_t shift = MS_AS_INT(b);         \
    if (shift < 0 || shift >= 64) {       \
      return MS_ERROR_VALUE;              \
    }                                     \
    PUSH(MS_INT_VAL(resultExpr));         \
  } while (0)

// Ordering/membership dispatch: evaluate rExpr (an MsValue-returning call
// using already-popped operands), propagate error, push the result or its
// truthy-negation (LE/GE/NOT_IN reduce via negation of LT/IN's result).
#define COMPARE_OP(rExpr, negate)           \
  do {                                      \
    MsValue r = (rExpr);                    \
    if (MS_IS_ERROR(r)) {                   \
      return r;                             \
    }                                       \
    if (negate) {                           \
      PUSH(MS_BOOL_VAL(!msValueTruthy(r))); \
    } else {                                \
      PUSH(r);                              \
    }                                       \
  } while (0)

// Shared callee dispatch for OP_CALL and OP_CALL_EX (T069), extracted from
// OP_CALL's former inline body (T068): native fast path calls fn and pushes
// the result directly (returns NULL, *ok == true); closure path returns the
// new MsFrame to switch to (*ok == true); *ok == false means the caller must
// `return MS_ERROR_VALUE` (not callable, or arity mismatch -- T080 placeholder).
static MsFrame* dispatchCall(MsThread* t, uint8_t argc, bool* ok) {
  MsValue* argv = t->sp - argc;
  MsValue callee = *(t->sp - argc - 1);
  if (MS_IS_OBJ(callee) && MS_AS_OBJ(callee)->type == &msNativeFnType) {
    struct MsNativeFnObj* nf = (struct MsNativeFnObj*) MS_AS_OBJ(callee);
    MsValue result = nf->fn(&gVM, argv, argc);
    if (MS_IS_ERROR(result)) {
      *ok = false;
      return NULL;
    }
    t->sp = argv - 1;  // pop callee + args
    PUSH(result);
    *ok = true;
    return NULL;
  }
  if (!MS_IS_OBJ(callee) || MS_AS_OBJ(callee)->type != &msClosureType) {
    *ok = false;  // TypeError: not callable (T080 placeholder)
    return NULL;
  }
  MsClosure* cl = (MsClosure*) MS_AS_OBJ(callee);
  MsFrame* newFrame = msClosureCall(t, cl, argc);
  if (!newFrame) {
    *ok = false;  // TypeError: arity mismatch (T080 placeholder)
    return NULL;
  }
  *ok = true;
  return newFrame;
}

// Expands the iterable seq's elements onto the value stack (pushed in
// iteration order), for OP_CALL_EX's `*iterable` unpacking. *outCount
// receives the number of pushed elements. Mirrors collectIntoList's
// tpIter/tpNext/msGCPushRoot convention below, but pushes onto the operand
// stack instead of collecting into a list. Returns false (leaving *outCount
// untouched) if seq is not iterable (T080 placeholder: TypeError).
static bool msExpandToStack(MsThread* t, MsValue seq, uint32_t* outCount) {
  struct MsType* seqType = msTypeOf(seq);
  if (!seqType->tpIter) {
    return false;
  }
  MsValue iter = seqType->tpIter(&gVM, seq);
  if (MS_IS_ERROR(iter)) {
    return false;
  }
  msGCPushRoot(iter);
  struct MsType* iterType = msTypeOf(iter);
  uint32_t count = 0;
  for (;;) {
    MsValue v = iterType->tpNext(&gVM, iter);
    if (MS_IS_NIL(v)) {
      break;  // StopIteration sentinel (T065)
    }
    PUSH(v);
    count++;
  }
  msGCPopRoot();
  *outCount = count;
  return true;
}

static MsValue eval(MsThread* t) {
  MsFrame* frame = t->topFrame;

#define DISPATCH() goto dispatch
dispatch:;
  uint8_t op = READ_BYTE();
  switch (op) {
    case OP_CONST: {
      uint32_t idx = READ_AX();
      PUSH(frame->chunk->constants[idx]);
      DISPATCH();
    }
    case OP_CONST_NIL:
      PUSH(MS_NIL_VAL);
      DISPATCH();
    case OP_CONST_TRUE:
      PUSH(MS_BOOL_VAL(true));
      DISPATCH();
    case OP_CONST_FALSE:
      PUSH(MS_BOOL_VAL(false));
      DISPATCH();
    case OP_POP:
      POP();
      DISPATCH();
    case OP_DUP:
      PUSH(PEEK(0));
      DISPATCH();

    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      PUSH(frame->slots[slot]);
      DISPATCH();
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = PEEK(0);  // does not pop
      DISPATCH();
    }

    // name is a str constant (compiler interns identifiers as str constants);
    // an absent name is a NameError (T080 placeholder).
    case OP_GET_GLOBAL: {
      uint32_t nameIdx = READ_AX();
      MsValue name = frame->chunk->constants[nameIdx];
      if (!MS_AS_BOOL(msMapHas(&gVM, t->globals, name))) {
        return MS_ERROR_VALUE;  // NameError (T080 placeholder)
      }
      PUSH(msMapGet(&gVM, t->globals, name));
      DISPATCH();
    }
    // Same STORE_LOCAL/STORE_GLOBAL rule (vm.md ss3.2): does not pop the
    // stored value; ms_compiler.c's compileVarDecl/compileAssign emit a
    // trailing OP_POP to balance the stack.
    case OP_SET_GLOBAL: {
      uint32_t nameIdx = READ_AX();
      MsValue name = frame->chunk->constants[nameIdx];
      MsValue val = PEEK(0);
      MsValue r = msMapSet(&gVM, t->globals, name, val);
      if (MS_IS_ERROR(r)) {
        return r;
      }
      DISPATCH();
    }

    case OP_ROT2: {
      MsValue a = POP(), b = POP();
      PUSH(a);
      PUSH(b);
      DISPATCH();
    }

    // funcIdx (3-byte AX) indexes proto in the constant pool; upvalCount (1
    // byte) is followed by upvalCount [isLocal, index] descriptor pairs
    // (ms_disasm.c's FMT_MAKE_FUNC, unchanged from T043). Default-value
    // expressions for this proto were compiled into THIS chunk immediately
    // before this instruction (ms_compiler.c's compileFuncToConst), so they
    // are popped here into proto->defaults -- evaluated once per execution of
    // this OP_MAKE_FUNC ("def time", P5-T068-call-convention.md's risk note),
    // not once per call.
    case OP_MAKE_FUNC: {
      uint32_t funcIdx = READ_AX();
      uint8_t upvalCount = READ_BYTE();
      MsFuncProto* proto = (MsFuncProto*) MS_AS_OBJ(frame->chunk->constants[funcIdx]);

      msGCPushRoot(MS_OBJ_VAL(proto));  // protect proto across msNewClosure's msGCAlloc
      for (uint32_t i = 0; i < proto->defaultCount; i++) {
        proto->defaults[i] = POP();
      }

      MsValue clVal = msNewClosure(proto, upvalCount);
      MsClosure* cl = (MsClosure*) MS_AS_OBJ(clVal);
      for (uint8_t i = 0; i < upvalCount; i++) {
        (void) READ_BYTE();      // isLocal
        (void) READ_BYTE();      // index
        cl->upvalues[i] = NULL;  // T071 stub: real upvalue capture lands there
      }
      msGCPopRoot();
      PUSH(clVal);
      DISPATCH();
    }

    case OP_GET_UPVALUE: {
      (void) READ_BYTE();  // upvalue index; unused in this stub phase
      PUSH(MS_NIL_VAL);    // stub: real implementation lands in T071
      DISPATCH();
    }
    case OP_SET_UPVALUE: {
      (void) READ_BYTE();  // upvalue index; unused in this stub phase
      DISPATCH();          // stub: does not pop or write; real implementation lands in T071
    }
    case OP_CLOSE_UPVALUE: {
      (void) READ_BYTE();  // local slot index; unused in this stub phase
      DISPATCH();          // stub: does not touch the value stack; real implementation lands in T071
    }

    case OP_ADD:
      BINARY_OP(tpAdd);
      DISPATCH();
    case OP_SUB:
      BINARY_OP(tpSub);
      DISPATCH();
    case OP_MUL:
      BINARY_OP(tpMul);
      DISPATCH();
    case OP_DIV:
      BINARY_OP(tpDiv);
      DISPATCH();
    case OP_MOD:
      BINARY_OP(tpMod);
      DISPATCH();
    case OP_POW:
      BINARY_OP(tpPow);
      DISPATCH();

    case OP_NEG:
      UNARY_OP(tpNeg);
      DISPATCH();
    case OP_BNOT:
      // ~a = -(a+1), routed through tpInvert (same pattern as OP_NEG).
      UNARY_OP(tpInvert);
      DISPATCH();

    case OP_BAND:
      BITWISE_OP(&, tpBitand);
      DISPATCH();
    case OP_BOR:
      BITWISE_OP(|, tpBitor);
      DISPATCH();
    case OP_BXOR:
      BITWISE_OP(^, tpBitxor);
      DISPATCH();
    case OP_SHL:
      SHIFT_OP((int64_t) ((uint64_t) MS_AS_INT(a) << shift));
      DISPATCH();
    case OP_SHR:
      SHIFT_OP(MS_AS_INT(a) >> shift);  // arithmetic (signed) shift
      DISPATCH();

    case OP_NOT: {
      MsValue v = POP();
      PUSH(MS_BOOL_VAL(!msValueTruthy(v)));
      DISPATCH();
    }

    case OP_JUMP_IF_FALSE: {
      int32_t offset = READ_JUMP_OFFSET();
      MsValue v = POP();
      if (!msValueTruthy(v)) {
        frame->ip += offset;
      }
      DISPATCH();
    }
    case OP_JUMP_IF_TRUE: {
      int32_t offset = READ_JUMP_OFFSET();
      MsValue v = POP();
      if (msValueTruthy(v)) {
        frame->ip += offset;
      }
      DISPATCH();
    }
    // Short-circuit and/or: does not pop, leaves the last-evaluated operand
    // as the result (Python semantics, not coerced to bool).
    case OP_AND_JMP: {
      int32_t offset = READ_JUMP_OFFSET();
      if (!msValueTruthy(PEEK(0))) {
        frame->ip += offset;
      }
      DISPATCH();
    }
    case OP_OR_JMP: {
      int32_t offset = READ_JUMP_OFFSET();
      if (msValueTruthy(PEEK(0))) {
        frame->ip += offset;
      }
      DISPATCH();
    }
    case OP_JUMP: {
      int32_t offset = READ_JUMP_OFFSET();
      frame->ip += offset;
      DISPATCH();
    }

    case OP_EQ: {
      MsValue b = POP(), a = POP();
      PUSH(MS_BOOL_VAL(msValueEqual(a, b)));
      DISPATCH();
    }
    case OP_NE: {
      MsValue b = POP(), a = POP();
      PUSH(MS_BOOL_VAL(!msValueEqual(a, b)));
      DISPATCH();
    }
    case OP_LT: {
      MsValue b = POP(), a = POP();
      COMPARE_OP(msValueLt(a, b), false);
      DISPATCH();
    }
    case OP_GT: {
      MsValue b = POP(), a = POP();
      COMPARE_OP(msValueGt(a, b), false);
      DISPATCH();
    }
    case OP_LE: {
      MsValue b = POP(), a = POP();
      COMPARE_OP(msValueLe(a, b), false);
      DISPATCH();
    }
    case OP_GE: {
      MsValue b = POP(), a = POP();
      COMPARE_OP(msValueGe(a, b), false);
      DISPATCH();
    }
    case OP_IS: {
      MsValue b = POP(), a = POP();
      PUSH(MS_BOOL_VAL(msValueIs(a, b)));
      DISPATCH();
    }
    case OP_IS_NOT: {
      MsValue b = POP(), a = POP();
      PUSH(MS_BOOL_VAL(!msValueIs(a, b)));
      DISPATCH();
    }
    // Stack bottom-to-top: s[1]=item, s[0]=container (compiler pushes
    // "item in container" in that order), so container pops first.
    case OP_IN: {
      MsValue container = POP(), item = POP();
      COMPARE_OP(msContains(container, item), false);
      DISPATCH();
    }
    case OP_NOT_IN: {
      MsValue container = POP(), item = POP();
      COMPARE_OP(msContains(container, item), true);
      DISPATCH();
    }

    // container[key]: dispatch to container's tpGetitem slot (compiler
    // pushes container then key, so key pops first -- same order as
    // BINARY_OP's a/b).
    case OP_GET_ITEM:
      BINARY_OP(tpGetitem);
      DISPATCH();

    // container[key] = value: dispatch to container's tpSetitem slot
    // (compiler pushes value, then container, then key, so key pops first,
    // then container, then value -- ms_compiler.c's compileAssign).
    case OP_SET_ITEM: {
      MsValue key = POP(), obj = POP(), val = POP();
      struct MsType* to = msTypeOf(obj);
      MsValue r = to->tpSetitem ? to->tpSetitem(&gVM, obj, key, val) : MS_ERROR_VALUE;
      if (MS_IS_ERROR(r)) {
        return r;
      }
      PUSH(r);
      DISPATCH();
    }

    // count elements are on the stack, bottom-most first (compiler pushes
    // items left-to-right); OP_BUILD_LIST uses the 1-byte FMT_A operand
    // (ms_disasm.c), so count is read via READ_BYTE(), not READ_AX().
    case OP_BUILD_LIST: {
      uint8_t count = READ_BYTE();
      MsValue list = msNewList(count);
      struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(list);
      t->sp -= count;
      if (count) {
        memcpy(l->items, t->sp, count * sizeof(MsValue));
      }
      l->len = count;
      PUSH(list);
      DISPATCH();
    }

    // count key/value pairs are on the stack, key then val alternating,
    // bottom-most pair first (ms_compiler.c's compileMap); OP_BUILD_MAP uses
    // the 1-byte FMT_A operand (ms_disasm.c), so count is read via
    // READ_BYTE(). Pairs stay on the stack (not popped via t->sp -=) while
    // msMapSet inserts them, so a GC triggered by msMapResize's msAlloc still
    // sees them via the stack scan; the map itself is protected separately
    // with msGCPushRoot since it is not yet reachable from the stack.
    case OP_BUILD_MAP: {
      uint8_t count = READ_BYTE();
      MsValue* pairs = t->sp - (size_t) count * 2;
      MsValue map = msNewMap(count);
      msGCPushRoot(map);
      for (uint8_t i = 0; i < count; i++) {
        MsValue r = msMapSet(&gVM, map, pairs[(size_t) i * 2], pairs[(size_t) i * 2 + 1]);
        if (MS_IS_ERROR(r)) {
          msGCPopRoot();
          return r;  // TypeError: key not hashable
        }
      }
      msGCPopRoot();
      t->sp = pairs;
      PUSH(map);
      DISPATCH();
    }

    // count elements are on the stack, bottom-most first (compiler pushes
    // items left-to-right); OP_BUILD_TUPLE uses the 1-byte FMT_A operand
    // (ms_disasm.c), so count is read via READ_BYTE(), same as OP_BUILD_LIST.
    case OP_BUILD_TUPLE: {
      uint8_t count = READ_BYTE();
      MsValue val = msNewTuple(count);
      struct MsTupleObj* tup = (struct MsTupleObj*) MS_AS_OBJ(val);
      t->sp -= count;
      if (count) {
        memcpy(tup->items, t->sp, count * sizeof(MsValue));
      }
      PUSH(val);
      DISPATCH();
    }

    // count elements are on the stack, bottom-most first (compiler pushes
    // items left-to-right); OP_BUILD_SET uses the 1-byte FMT_A operand
    // (ms_disasm.c), so count is read via READ_BYTE(). Elements stay on the
    // stack (not popped via t->sp -=) while msSetAdd inserts them, so a GC
    // triggered by msSetAdd's msAlloc-based resize still sees them via the
    // stack scan; the set itself is protected separately with msGCPushRoot
    // since it is not yet reachable from the stack (same pattern as
    // OP_BUILD_MAP).
    case OP_BUILD_SET: {
      uint8_t count = READ_BYTE();
      MsValue* items = t->sp - count;
      MsValue setVal = msNewSet(count);
      msGCPushRoot(setVal);
      for (uint8_t i = 0; i < count; i++) {
        MsValue r = msSetAdd(&gVM, setVal, items[i]);
        if (MS_IS_ERROR(r)) {
          msGCPopRoot();
          return r;  // TypeError: element not hashable
        }
      }
      msGCPopRoot();
      t->sp = items;
      PUSH(setVal);
      DISPATCH();
    }

    // A = flags bitmask (MS_SLICE_HAS_LO/HI/STEP); pops present components in
    // reverse push order (step, then hi, then lo), missing ones default to
    // nil (ms_compiler.c's compileSliceExpr).
    case OP_BUILD_SLICE: {
      uint8_t flags = READ_BYTE();
      MsValue step = (flags & MS_SLICE_HAS_STEP) ? POP() : MS_NIL_VAL;
      MsValue hi = (flags & MS_SLICE_HAS_HI) ? POP() : MS_NIL_VAL;
      MsValue lo = (flags & MS_SLICE_HAS_LO) ? POP() : MS_NIL_VAL;
      PUSH(msNewSlice(lo, hi, step));
      DISPATCH();
    }

    // s[0] = s[0].__iter__(); TypeError (T080 placeholder) if the type has no
    // tpIter slot.
    case OP_GET_ITER: {
      MsValue iterable = POP();
      struct MsType* tp = msTypeOf(iterable);
      if (!tp->tpIter) {
        return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
      }
      MsValue iter = tp->tpIter(&gVM, iterable);
      if (MS_IS_ERROR(iter)) {
        return iter;
      }
      PUSH(iter);
      DISPATCH();
    }

    // AX: signed jump offset. Stack top is the iterator (not popped on the
    // has-next path -- FOR_ITER runs again next iteration). tpNext returning
    // nil is the StopIteration sentinel (T065 protocol, ms_range.c's
    // rangeIterNext establishes the same convention): pop the iterator and
    // jump past the loop body; otherwise push the produced value and fall
    // through into the body.
    case OP_FOR_ITER: {
      int32_t offset = READ_JUMP_OFFSET();
      MsValue iter = PEEK(0);
      struct MsType* tp = msTypeOf(iter);
      if (!tp->tpNext) {
        return MS_ERROR_VALUE;  // TypeError (T080 placeholder): not an iterator
      }
      MsValue val = tp->tpNext(&gVM, iter);
      if (MS_IS_NIL(val)) {
        (void) POP();
        frame->ip += offset;
      } else {
        PUSH(val);
      }
      DISPATCH();
    }

    // obj.name: dispatch to obj's tpGetattr slot; falls back to a methods
    // dict lookup when tp has no tpGetattr (T072/T073 populate methods dicts
    // for user classes; bound-method wrapping of the lookup result lands in
    // T073). Neither a slot nor a matching method name is an AttributeError
    // (MS_ERROR_VALUE, T080 placeholder).
    case OP_GET_ATTR: {
      uint32_t nameIdx = READ_AX();
      MsValue obj = POP();
      MsValue name = frame->chunk->constants[nameIdx];
      struct MsType* tp = msTypeOf(obj);
      MsValue result = tp->tpGetattr ? tp->tpGetattr(&gVM, obj, name) : msTypeLookupMethod(tp, name);
      if (MS_IS_NIL(result)) {
        return MS_ERROR_VALUE;  // AttributeError (T080 placeholder)
      }
      if (MS_IS_ERROR(result)) {
        return result;
      }
      PUSH(result);
      DISPATCH();
    }

    // obj.name = val: dispatch to obj's tpSetattr slot (ms_compiler.c's
    // compileAssign pushes val then obj, so obj pops first). Pushes the
    // tpSetattr result back (same convention as OP_SET_ITEM) so the trailing
    // OP_POP compileAssign always emits has a value to consume.
    case OP_SET_ATTR: {
      uint32_t nameIdx = READ_AX();
      MsValue obj = POP();
      MsValue val = POP();
      MsValue name = frame->chunk->constants[nameIdx];
      struct MsType* tp = msTypeOf(obj);
      if (!tp->tpSetattr) {
        return MS_ERROR_VALUE;  // AttributeError: readonly (T080 placeholder)
      }
      MsValue r = tp->tpSetattr(&gVM, obj, name, val);
      if (MS_IS_ERROR(r)) {
        return r;
      }
      PUSH(r);
      DISPATCH();
    }

    // del obj.name: dispatch to obj's tpDelattr slot (statement form --
    // ms_compiler.c's compileDel emits no trailing OP_POP, so this leaves the
    // stack exactly balanced: pops obj, pushes nothing).
    case OP_DEL_ATTR: {
      uint32_t nameIdx = READ_AX();
      MsValue obj = POP();
      MsValue name = frame->chunk->constants[nameIdx];
      struct MsType* tp = msTypeOf(obj);
      if (!tp->tpDelattr) {
        return MS_ERROR_VALUE;  // AttributeError: cannot delete (T080 placeholder)
      }
      MsValue r = tp->tpDelattr(&gVM, obj, name);
      if (MS_IS_ERROR(r)) {
        return r;
      }
      DISPATCH();
    }

    // del obj[key]: dispatch to obj's tpDelitem slot (compiler pushes obj
    // then key, so key pops first). Statement form, same balanced-stack
    // convention as OP_DEL_ATTR.
    case OP_DEL_ITEM: {
      MsValue key = POP(), obj = POP();
      struct MsType* tp = msTypeOf(obj);
      if (!tp->tpDelitem) {
        return MS_ERROR_VALUE;  // TypeError: does not support item deletion (T080 placeholder)
      }
      MsValue r = tp->tpDelitem(&gVM, obj, key);
      if (MS_IS_ERROR(r)) {
        return r;
      }
      DISPATCH();
    }

    case OP_RETURN: {
      MsValue result = POP();
      // pop this frame (including its callee slot) and restore the caller's frame
      t->sp = frame->slots - 1;
      t->topFrame = frame->caller;
      if (!t->topFrame) {
        return result;  // top-level return: frame is msVMRun's C local, not pool-allocated
      }
      msFreeFrame(frame);  // return this call frame to T068's frame pool
      PUSH(result);
      frame = t->topFrame;
      DISPATCH();
    }

    // Same frame-pop as OP_RETURN, but the result is always nil (no explicit
    // return value on the stack -- ms_compiler.c emits this for a function
    // body's implicit fall-through return and the top-level program's end).
    case OP_RETURN_NIL: {
      MsValue result = MS_NIL_VAL;
      t->sp = frame->slots - 1;
      t->topFrame = frame->caller;
      if (!t->topFrame) {
        return result;  // top-level return: frame is msVMRun's C local, not pool-allocated
      }
      msFreeFrame(frame);
      PUSH(result);
      frame = t->topFrame;
      DISPATCH();
    }

    // callee, then argc args are on the stack (ms_compiler.c's compileCall);
    // AX operand is argc (1 byte, read via READ_BYTE, matching OP_CALL's
    // FMT_A disasm format). Native (C-function) callees are dispatched
    // directly here -- detected by object identity against msNativeFnType,
    // not the generic tpCall slot (that slot is reserved for future
    // user-callable instances, T077). User-defined closures (MsClosure) go
    // through msClosureCall (ms_func.c, P5-T068): arity check + a new call
    // frame from the frame pool, bound as t->topFrame.
    case OP_CALL: {
      uint8_t argc = READ_BYTE();
      bool ok;
      MsFrame* newFrame = dispatchCall(t, argc, &ok);
      if (!ok) {
        return MS_ERROR_VALUE;
      }
      if (newFrame) {
        frame = newFrame;
      }
      DISPATCH();
    }

    // Stack: [callee, argsSeq] (ms_compiler.c's compileCallEx merges plain
    // args and any `*rest` into a single sequence beforehand -- this
    // instruction only unpacks that one sequence onto the stack, T069).
    case OP_CALL_EX: {
      MsValue argsSeq = POP();
      uint32_t argc;
      if (!msExpandToStack(t, argsSeq, &argc)) {
        return MS_ERROR_VALUE;  // TypeError (T080 placeholder): not iterable
      }
      bool ok;
      MsFrame* newFrame = dispatchCall(t, (uint8_t) argc, &ok);
      if (!ok) {
        return MS_ERROR_VALUE;
      }
      if (newFrame) {
        frame = newFrame;
      }
      DISPATCH();
    }

    // Stack: [callee, pos_arg0..posArgc-1, kwargsMap] (ms_compiler.c's
    // compileCallKw, vm.md ss3.6). kwargsMap is only PEEK'd here, not
    // popped -- it stays part of the traced value stack until
    // msClosureCallKw claims/reuses that stack region itself. Native
    // functions do not support keyword arguments (T080 placeholder, same as
    // dispatchCall's not-callable case above).
    case OP_CALL_KW: {
      uint8_t posArgc = READ_BYTE();
      MsValue kwargsMap = PEEK(0);
      MsValue callee = PEEK(posArgc + 1);
      if (!MS_IS_OBJ(callee) || MS_AS_OBJ(callee)->type != &msClosureType) {
        return MS_ERROR_VALUE;  // TypeError: not callable / kwargs unsupported for native fns (T080 placeholder)
      }
      if (!MS_IS_OBJ(kwargsMap) || MS_AS_OBJ(kwargsMap)->type != &msMapType) {
        return MS_ERROR_VALUE;  // TypeError: kwargs must be a map (T080 placeholder)
      }
      MsClosure* cl = (MsClosure*) MS_AS_OBJ(callee);
      MsFrame* newFrame = msClosureCallKw(t, cl, (uint32_t) posArgc, kwargsMap);
      if (!newFrame) {
        return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
      }
      frame = newFrame;
      DISPATCH();
    }

    default:
      fprintf(stderr, "unknown opcode: %02X\n", op);
      return MS_ERROR_VALUE;
  }
}

// M1 built-in functions (impl/P4-T067-vm-e2e-m1.md "实现要点 2/3"), registered
// into t->globals by msVMInit below. Signatures match MsCallFn/MsCFunction
// (c-api.md ss6.1).

// str(v): prefer tpStr; Python's default str() falls back to repr() when a
// type defines no __str__ (list/map/set/tuple only have tpRepr in M1), so
// mirror that here rather than requiring every container to duplicate it.
static MsValue stringify(MsValue v) {
  struct MsType* tp = msTypeOf(v);
  if (tp->tpStr) {
    return tp->tpStr(&gVM, v);
  }
  if (tp->tpRepr) {
    return tp->tpRepr(&gVM, v);
  }
  return MS_ERROR_VALUE;
}

// print(...): stringify each arg, space-separated, trailing newline
// (type-system.md ss3.4: print(x) triggers __str__).
static MsValue msBuiltinPrint(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  for (int i = 0; i < argc; i++) {
    if (i > 0) {
      fputc(' ', stdout);
    }
    MsValue sv = stringify(argv[i]);
    if (MS_IS_ERROR(sv)) {
      fputc('?', stdout);
      continue;
    }
    struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(sv);
    fwrite(s->data, 1, s->len, stdout);
  }
  fputc('\n', stdout);
  return MS_NIL_VAL;
}

static MsValue msBuiltinLen(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc != 1) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsType* tp = msTypeOf(argv[0]);
  return tp->tpLen ? tp->tpLen(&gVM, argv[0]) : MS_ERROR_VALUE;
}

// type(x): M1 downgrade -- returns the type NAME as a str, not a type object
// (full type-object semantics land in P5-T078, type-system.md ss5).
static MsValue msBuiltinType(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc != 1) {
    return MS_ERROR_VALUE;
  }
  const char* name = msTypeOf(argv[0])->name;
  return msNewStr(name, (uint32_t) strlen(name));
}

static MsValue msBuiltinRepr(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc != 1) {
    return MS_ERROR_VALUE;
  }
  struct MsType* tp = msTypeOf(argv[0]);
  return tp->tpRepr ? tp->tpRepr(&gVM, argv[0]) : MS_ERROR_VALUE;
}

static MsValue msBuiltinStr(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc != 1) {
    return MS_ERROR_VALUE;
  }
  return stringify(argv[0]);
}

// v's C string data if v is a str, else NULL. MsStrObj::data is always
// NUL-terminated (ms_str.c's allocStr), so callers can hand it straight to
// strtoll/strtod without an intermediate copy. Shared by msBuiltinInt/
// msBuiltinFloat's str->number parsing.
static const char* strObjData(MsValue v) {
  if (!MS_IS_OBJ(v) || MS_AS_OBJ(v)->type != &msStrType) {
    return NULL;
  }
  return ((struct MsStrObj*) MS_AS_OBJ(v))->data;
}

// int(x): simplified str->int (M1 downgrade, impl/P4-T067-vm-e2e-m1.md);
// int/bool passthrough, float truncates toward zero (Python int() semantics).
static MsValue msBuiltinInt(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc != 1) {
    return MS_ERROR_VALUE;
  }
  MsValue v = argv[0];
  if (MS_IS_INT(v)) {
    return v;
  }
  if (MS_IS_FLOAT(v)) {
    return MS_INT_VAL((int64_t) MS_AS_FLOAT(v));
  }
  if (MS_IS_BOOL(v)) {
    return MS_INT_VAL(MS_AS_BOOL(v) ? 1 : 0);
  }
  const char* s = strObjData(v);
  if (s) {
    char* end;
    long long parsed = strtoll(s, &end, 10);
    if (end == s) {
      return MS_ERROR_VALUE;  // ValueError (T080 placeholder)
    }
    return MS_INT_VAL((int64_t) parsed);
  }
  return MS_ERROR_VALUE;
}

// float(x): simplified str->float (M1 downgrade); int/bool widen.
static MsValue msBuiltinFloat(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc != 1) {
    return MS_ERROR_VALUE;
  }
  MsValue v = argv[0];
  if (MS_IS_FLOAT(v)) {
    return v;
  }
  if (MS_IS_INT(v)) {
    return MS_FLOAT_VAL((double) MS_AS_INT(v));
  }
  if (MS_IS_BOOL(v)) {
    return MS_FLOAT_VAL(MS_AS_BOOL(v) ? 1.0 : 0.0);
  }
  const char* s = strObjData(v);
  if (s) {
    char* end;
    double parsed = strtod(s, &end);
    if (end == s) {
      return MS_ERROR_VALUE;  // ValueError (T080 placeholder)
    }
    return MS_FLOAT_VAL(parsed);
  }
  return MS_ERROR_VALUE;
}

static MsValue msBuiltinBool(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc != 1) {
    return MS_ERROR_VALUE;
  }
  return MS_BOOL_VAL(msValueTruthy(argv[0]));
}

// Iterates v via its tpIter/tpNext protocol, appending each element into a
// freshly created list. Shared by msBuiltinList/msBuiltinTuple/msBuiltinSet
// so only one place implements the "materialize an iterable" loop; both the
// iterator and the growing list are GC-rooted for the duration (same
// defensive pattern as OP_BUILD_MAP/OP_BUILD_SET in eval()).
static MsValue collectIntoList(MsValue v) {
  struct MsType* tp = msTypeOf(v);
  if (!tp->tpIter) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder): not iterable
  }
  MsValue iter = tp->tpIter(&gVM, v);
  if (MS_IS_ERROR(iter)) {
    return iter;
  }
  msGCPushRoot(iter);
  MsValue list = msNewList(0);
  msGCPushRoot(list);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(list);
  struct MsType* itp = msTypeOf(iter);
  for (;;) {
    MsValue item = itp->tpNext(&gVM, iter);
    if (MS_IS_NIL(item)) {
      break;
    }
    msListAppend(l, item);
  }
  msGCPopRoot();  // list
  msGCPopRoot();  // iter
  return list;
}

static MsValue msBuiltinList(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc != 1) {
    return MS_ERROR_VALUE;
  }
  return collectIntoList(argv[0]);
}

static MsValue msBuiltinTuple(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc != 1) {
    return MS_ERROR_VALUE;
  }
  MsValue list = collectIntoList(argv[0]);
  if (MS_IS_ERROR(list)) {
    return list;
  }
  msGCPushRoot(list);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(list);
  MsValue tup = msNewTuple(l->len);
  struct MsTupleObj* to = (struct MsTupleObj*) MS_AS_OBJ(tup);
  if (l->len) {
    memcpy(to->items, l->items, l->len * sizeof(MsValue));
  }
  msGCPopRoot();  // list
  return tup;
}

static MsValue msBuiltinSet(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc != 1) {
    return MS_ERROR_VALUE;
  }
  MsValue list = collectIntoList(argv[0]);
  if (MS_IS_ERROR(list)) {
    return list;
  }
  msGCPushRoot(list);
  struct MsListObj* l = (struct MsListObj*) MS_AS_OBJ(list);
  MsValue setVal = msNewSet(l->len);
  msGCPushRoot(setVal);
  for (uint32_t i = 0; i < l->len; i++) {
    MsValue r = msSetAdd(&gVM, setVal, l->items[i]);
    if (MS_IS_ERROR(r)) {
      msGCPopRoot();  // setVal
      msGCPopRoot();  // list
      return r;       // TypeError: element not hashable
    }
  }
  msGCPopRoot();  // setVal
  msGCPopRoot();  // list
  return setVal;
}

// input(prompt): writes prompt (if given) to stdout, reads one stdin line
// (trailing \n/\r\n stripped), returns it as a str.
static MsValue msBuiltinInput(struct MsVM* vm, MsValue* argv, int argc) {
  (void) vm;
  if (argc == 1) {
    MsValue sv = stringify(argv[0]);
    if (!MS_IS_ERROR(sv)) {
      struct MsStrObj* s = (struct MsStrObj*) MS_AS_OBJ(sv);
      fwrite(s->data, 1, s->len, stdout);
    }
  }
  char buf[4096];
  if (!fgets(buf, sizeof(buf), stdin)) {
    return msNewStr("", 0);
  }
  size_t n = strlen(buf);
  if (n > 0 && buf[n - 1] == '\n') {
    n--;
    if (n > 0 && buf[n - 1] == '\r') {
      n--;
    }
  }
  return msNewStr(buf, (uint32_t) n);
}

void msVMInit(void) {
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;
  t->topFrame = NULL;
  // reset before msGCInit()/msGCAlloc below run: a stale globals map from a
  // prior msVMInit()/msVMShutdown() cycle would otherwise be a dangling
  // pointer that markRoots() (ms_gc.c) could dereference if init allocations
  // trigger a collection before t->globals is reassigned a few lines down.
  t->globals = MS_NIL_VAL;
  t->exception = MS_NIL_VAL;
  t->exceptStack = NULL;
  t->coro = NULL;

  gVM.intType = &msIntType;
  gVM.floatType = &msFloatType;
  gVM.boolType = &msBoolType;
  gVM.nilType = &msNilType;
  gVM.strType = &msStrType;
  gVM.bytesType = &msBytesType;
  gVM.listType = &msListType;
  gVM.mapType = &msMapType;
  gVM.tupleType = NULL;
  gVM.setType = NULL;

  // msNewMap/msNewStr/msRegisterBuiltin below allocate through msGCAlloc, so
  // GC + str interning must be up first (the reverse order of the doc's
  // illustrative snippet, which would deref an uninitialized gGC).
  msGCInit();
  msStrInternInit();

  t->globals = msNewMap(64);  // global namespace (this task: replaces the MS_NIL_VAL stub)

  msRegisterBuiltin("print", msBuiltinPrint);
  msRegisterBuiltin("len", msBuiltinLen);
  msRegisterBuiltin("type", msBuiltinType);
  msRegisterBuiltin("range", msBuiltinRange);
  msRegisterBuiltin("repr", msBuiltinRepr);
  msRegisterBuiltin("str", msBuiltinStr);
  msRegisterBuiltin("int", msBuiltinInt);
  msRegisterBuiltin("float", msBuiltinFloat);
  msRegisterBuiltin("bool", msBuiltinBool);
  msRegisterBuiltin("list", msBuiltinList);
  msRegisterBuiltin("tuple", msBuiltinTuple);
  msRegisterBuiltin("set", msBuiltinSet);
  msRegisterBuiltin("input", msBuiltinInput);
}

void msVMShutdown(void) {
  msGCShutdown();
}

MsValue msVMRun(struct MsChunk* chunk) {
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;
  PUSH(MS_NIL_VAL);  // reserve the top-level frame's callee slot

  MsFrame frame;
  frame.chunk = chunk;
  frame.ip = chunk->code;
  frame.slots = t->sp;
  frame.slotCount = 0;
  frame.closure = NULL;
  frame.caller = NULL;
  t->topFrame = &frame;

  return eval(t);
}

MsValue msVMRunFile(const char* path) {
  FILE* fp = fopen(path, "rb");
  if (!fp) {
    fprintf(stderr, "mslang: cannot open '%s'\n", path);
    return MS_ERROR_VALUE;
  }
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);
  char* src = msAlloc((size_t) size + 1);
  size_t n = fread(src, 1, (size_t) size, fp);
  fclose(fp);
  src[n] = '\0';

  MsCompileResult r = msCompile(src, (uint32_t) n, path);
  msFree(src);
  if (r.hadError) {
    fprintf(stderr, "compile error: %s\n", r.errBuf);
    msCompileResultFree(&r);
    return MS_ERROR_VALUE;
  }

  MsValue result = msVMRun(r.chunk);
  msCompileResultFree(&r);
  return result;
}
