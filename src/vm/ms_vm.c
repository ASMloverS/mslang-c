// ms_vm.c -- MsVM global state + main eval loop (switch dispatch)
#include "mslang/ms_vm.h"

#include <stdio.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_int.h"
#include "mslang/ms_object.h"
#include "mslang/ms_opcode.h"

MsVM gVM;

// Stack operation helper macros (t: MsThread*).
#define PUSH(v) (*t->sp++ = (v))
#define POP() (*--t->sp)
#define PEEK(n) (*(t->sp - 1 - (n)))
#define POKE(n, v) (*(t->sp - 1 - (n)) = (v))

// Read operands.
#define READ_BYTE() (*frame->ip++)
// AX: 3-byte big-endian operand (vm.md ss3); must not be shrunk to uint16.
#define READ_AX() (frame->ip += 3, ((uint32_t) frame->ip[-3] << 16) | ((uint32_t) frame->ip[-2] << 8) | frame->ip[-1])

// Dispatches a value to its type descriptor (vm.md ss6); gVM.xxxType slots
// are filled in incrementally by T053-T066.
static struct MsType* msTypeOf(MsValue v) {
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

// Fallback when a's type has no matching slot (TypeError placeholder pre-T080).
static MsValue msNotImplemented(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  (void) a;
  (void) b;
  return MS_ERROR_VALUE;
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

// BAND/BOR/BXOR have no MsType slot (type-system.md ss1.3 opens no binary
// bitwise slots); inlined for int only, TypeError otherwise.
#define BITWISE_OP(op)                              \
  do {                                              \
    MsValue b = POP(), a = POP();                   \
    if (!MS_IS_INT(a) || !MS_IS_INT(b)) {           \
      return MS_ERROR_VALUE;                        \
    }                                               \
    PUSH(MS_INT_VAL(MS_AS_INT(a) op MS_AS_INT(b))); \
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

    case OP_GET_GLOBAL: {
      (void) READ_AX();  // name constant index; unused in this stub phase
      PUSH(MS_NIL_VAL);  // stub: real implementation lands after T060
      DISPATCH();
    }
    case OP_SET_GLOBAL: {
      (void) READ_AX();  // name constant index; unused in this stub phase
      DISPATCH();        // stub: does not pop or write; real implementation lands after T060
    }

    case OP_ROT2: {
      MsValue a = POP(), b = POP();
      PUSH(a);
      PUSH(b);
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
      BITWISE_OP(&);
      DISPATCH();
    case OP_BOR:
      BITWISE_OP(|);
      DISPATCH();
    case OP_BXOR:
      BITWISE_OP(^);
      DISPATCH();
    case OP_SHL:
      SHIFT_OP((int64_t) ((uint64_t) MS_AS_INT(a) << shift));
      DISPATCH();
    case OP_SHR:
      SHIFT_OP(MS_AS_INT(a) >> shift);  // arithmetic (signed) shift
      DISPATCH();

      // ... remaining opcodes filled in incrementally by T054-T066

    case OP_RETURN: {
      MsValue result = POP();
      // pop this frame (including its callee slot) and restore the caller's frame
      t->sp = frame->slots - 1;
      t->topFrame = frame->caller;
      if (!t->topFrame) {
        return result;  // top-level return
      }
      PUSH(result);
      frame = t->topFrame;
      DISPATCH();
    }

    default:
      fprintf(stderr, "unknown opcode: %02X\n", op);
      return MS_ERROR_VALUE;
  }
}

void msVMInit(void) {
  MsThread* t = &gVM.mainThread;
  t->sp = t->stack;
  t->topFrame = NULL;
  t->globals = MS_NIL_VAL;
  t->exception = MS_NIL_VAL;
  t->exceptStack = NULL;
  t->coro = NULL;

  gVM.intType = &msIntType;
  gVM.floatType = NULL;
  gVM.boolType = NULL;
  gVM.nilType = NULL;
  gVM.strType = NULL;
  gVM.bytesType = NULL;
  gVM.listType = NULL;
  gVM.mapType = NULL;
  gVM.tupleType = NULL;
  gVM.setType = NULL;

  msGCInit();
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
