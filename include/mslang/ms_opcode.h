#pragma once

#include <stdint.h>

typedef enum MsOpCode {
  // stack ops
  OP_POP,
  OP_DUP,
  OP_ROT2,

  // constant loading
  OP_CONST,
  OP_CONST_INT,
  OP_CONST_TRUE,
  OP_CONST_FALSE,
  OP_CONST_NIL,

  // variable ops
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL,
  OP_DEL_GLOBAL,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_CLOSE_UPVALUE,

  // arithmetic
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD,
  OP_POW,
  OP_NEG,
  OP_BAND,
  OP_BOR,
  OP_BXOR,
  OP_SHL,
  OP_SHR,
  OP_BNOT,

  // comparison and logic
  OP_EQ,
  OP_NE,
  OP_LT,
  OP_LE,
  OP_GT,
  OP_GE,
  OP_IN,
  OP_IS,
  OP_NOT,
  OP_IS_NOT,
  OP_NOT_IN,
  OP_ISINSTANCE,
  OP_AND_JMP,
  OP_OR_JMP,

  // jumps (3-byte signed AX, relative to next instruction)
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_JUMP_IF_TRUE,
  OP_JUMP_IF_FALSE_PEEK,

  // function calls and return
  OP_CALL,
  OP_CALL_ASYNC,
  OP_CALL_EX,
  OP_CALL_KW,
  OP_RETURN,
  OP_RETURN_NIL,

  // attributes and subscripts
  OP_GET_ATTR,
  OP_SET_ATTR,
  OP_DEL_ATTR,
  OP_GET_ITEM,
  OP_SET_ITEM,
  OP_DEL_ITEM,

  // container building
  OP_BUILD_LIST,
  OP_BUILD_MAP,
  OP_BUILD_TUPLE,
  OP_BUILD_SET,
  OP_BUILD_SLICE,
  OP_BUILD_STR,

  // closures and classes
  OP_MAKE_FUNC,
  OP_MAKE_CLASS,
  OP_LOAD_SUPER,

  // iteration
  OP_GET_ITER,
  OP_FOR_ITER,

  // exception handling
  OP_PUSH_EXCEPT,
  OP_POP_EXCEPT,
  OP_RAISE,
  OP_RAISE_ASSERT,
  OP_RERAISE,
  OP_LOAD_EXCEPTION,
  OP_CLEAR_EXCEPTION,
  OP_WITH_ENTER,
  OP_WITH_EXIT,

  // concurrency
  OP_GO,
  OP_MAKE_CHAN,
  OP_CHAN_SEND,
  OP_CHAN_RECV,
  OP_CHAN_CLOSE,
  OP_AWAIT,
  OP_SELECT_BEGIN,
  OP_SELECT_CASE_SEND,
  OP_SELECT_CASE_RECV,
  OP_SELECT_END,

  // modules
  OP_IMPORT,
  OP_IMPORT_FROM,

  // string conversion
  OP_TO_STR,

  OP_COUNT,  // sentinel
} MsOpCode;

// OP_BUILD_SLICE operand A: bitmask of present slice components
#define MS_SLICE_HAS_LO 0x1
#define MS_SLICE_HAS_HI 0x2
#define MS_SLICE_HAS_STEP 0x4
