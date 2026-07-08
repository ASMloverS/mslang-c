// ms_vm.c -- MsVM global state + main eval loop (switch dispatch)
#include "mslang/ms_vm.h"

#include <stdio.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_gc.h"
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

      // ... remaining 60+ opcodes filled in incrementally by T052-T066

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

  gVM.intType = NULL;
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
