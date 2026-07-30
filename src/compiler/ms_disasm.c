#include "mslang/ms_disasm.h"

#include <assert.h>

#include "mslang/ms_chunk.h"
#include "mslang/ms_opcode.h"
#include "mslang/ms_value.h"

// Operand encoding format for each opcode (widths documented in vm.md §3,
// deviations reflect the actual bytes emitted by ms_compiler.c).
typedef enum {
  FMT_NONE,        // no operand, 1 byte total
  FMT_A,           // 1-byte unsigned operand (slot/count/argc), 2 bytes total
  FMT_AX_CONST,    // 3-byte unsigned operand = constant pool index, 4 bytes
  FMT_AX_JUMP,     // 3-byte signed relative jump offset, 4 bytes
  FMT_AX_PLAIN,    // 3-byte unsigned operand (count), 4 bytes
  FMT_CONST_INT,   // 3-byte signed inline int24 value, 4 bytes
  FMT_MAKE_FUNC,   // AX funcIdx + upvalueCount:u8 + upvalueCount*2 bytes
  FMT_MAKE_CLASS,  // nameIdx:u16 + methodCount:u8 + baseCount:u8 + methods*4
} MsOpFmt;

typedef struct {
  const char* name;
  MsOpFmt fmt;
} MsOpInfo;

static const MsOpInfo kOpInfo[OP_COUNT] = {
    [OP_POP] = {"OP_POP", FMT_NONE},
    [OP_DUP] = {"OP_DUP", FMT_NONE},
    [OP_ROT2] = {"OP_ROT2", FMT_NONE},

    [OP_CONST] = {"OP_CONST", FMT_AX_CONST},
    [OP_CONST_INT] = {"OP_CONST_INT", FMT_CONST_INT},
    [OP_CONST_TRUE] = {"OP_CONST_TRUE", FMT_NONE},
    [OP_CONST_FALSE] = {"OP_CONST_FALSE", FMT_NONE},
    [OP_CONST_NIL] = {"OP_CONST_NIL", FMT_NONE},

    [OP_GET_LOCAL] = {"OP_GET_LOCAL", FMT_A},
    [OP_SET_LOCAL] = {"OP_SET_LOCAL", FMT_A},
    [OP_GET_GLOBAL] = {"OP_GET_GLOBAL", FMT_AX_CONST},
    [OP_SET_GLOBAL] = {"OP_SET_GLOBAL", FMT_AX_CONST},
    [OP_DEL_GLOBAL] = {"OP_DEL_GLOBAL", FMT_AX_CONST},
    [OP_GET_UPVALUE] = {"OP_GET_UPVALUE", FMT_A},
    [OP_SET_UPVALUE] = {"OP_SET_UPVALUE", FMT_A},
    [OP_CLOSE_UPVALUE] = {"OP_CLOSE_UPVALUE", FMT_NONE},

    [OP_ADD] = {"OP_ADD", FMT_NONE},
    [OP_SUB] = {"OP_SUB", FMT_NONE},
    [OP_MUL] = {"OP_MUL", FMT_NONE},
    [OP_DIV] = {"OP_DIV", FMT_NONE},
    [OP_MOD] = {"OP_MOD", FMT_NONE},
    [OP_POW] = {"OP_POW", FMT_NONE},
    [OP_NEG] = {"OP_NEG", FMT_NONE},
    [OP_BAND] = {"OP_BAND", FMT_NONE},
    [OP_BOR] = {"OP_BOR", FMT_NONE},
    [OP_BXOR] = {"OP_BXOR", FMT_NONE},
    [OP_SHL] = {"OP_SHL", FMT_NONE},
    [OP_SHR] = {"OP_SHR", FMT_NONE},
    [OP_BNOT] = {"OP_BNOT", FMT_NONE},

    [OP_EQ] = {"OP_EQ", FMT_NONE},
    [OP_NE] = {"OP_NE", FMT_NONE},
    [OP_LT] = {"OP_LT", FMT_NONE},
    [OP_LE] = {"OP_LE", FMT_NONE},
    [OP_GT] = {"OP_GT", FMT_NONE},
    [OP_GE] = {"OP_GE", FMT_NONE},
    [OP_IN] = {"OP_IN", FMT_NONE},
    [OP_IS] = {"OP_IS", FMT_NONE},
    [OP_NOT] = {"OP_NOT", FMT_NONE},
    [OP_IS_NOT] = {"OP_IS_NOT", FMT_NONE},
    [OP_NOT_IN] = {"OP_NOT_IN", FMT_NONE},
    [OP_ISINSTANCE] = {"OP_ISINSTANCE", FMT_NONE},
    [OP_AND_JMP] = {"OP_AND_JMP", FMT_AX_JUMP},
    [OP_OR_JMP] = {"OP_OR_JMP", FMT_AX_JUMP},

    [OP_JUMP] = {"OP_JUMP", FMT_AX_JUMP},
    [OP_JUMP_IF_FALSE] = {"OP_JUMP_IF_FALSE", FMT_AX_JUMP},
    [OP_JUMP_IF_TRUE] = {"OP_JUMP_IF_TRUE", FMT_AX_JUMP},
    [OP_JUMP_IF_FALSE_PEEK] = {"OP_JUMP_IF_FALSE_PEEK", FMT_AX_JUMP},

    [OP_CALL] = {"OP_CALL", FMT_A},
    [OP_CALL_ASYNC] = {"OP_CALL_ASYNC", FMT_A},
    [OP_CALL_EX] = {"OP_CALL_EX", FMT_NONE},
    [OP_CALL_KW] = {"OP_CALL_KW", FMT_A},
    [OP_RETURN] = {"OP_RETURN", FMT_NONE},
    [OP_RETURN_NIL] = {"OP_RETURN_NIL", FMT_NONE},

    [OP_GET_ATTR] = {"OP_GET_ATTR", FMT_AX_CONST},
    [OP_SET_ATTR] = {"OP_SET_ATTR", FMT_AX_CONST},
    [OP_DEL_ATTR] = {"OP_DEL_ATTR", FMT_AX_CONST},
    [OP_GET_ITEM] = {"OP_GET_ITEM", FMT_NONE},
    [OP_SET_ITEM] = {"OP_SET_ITEM", FMT_NONE},
    [OP_DEL_ITEM] = {"OP_DEL_ITEM", FMT_NONE},

    [OP_BUILD_LIST] = {"OP_BUILD_LIST", FMT_A},
    [OP_BUILD_MAP] = {"OP_BUILD_MAP", FMT_A},
    [OP_BUILD_TUPLE] = {"OP_BUILD_TUPLE", FMT_A},
    [OP_BUILD_SET] = {"OP_BUILD_SET", FMT_A},
    [OP_BUILD_SLICE] = {"OP_BUILD_SLICE", FMT_A},
    [OP_BUILD_STR] = {"OP_BUILD_STR", FMT_A},

    [OP_MAKE_FUNC] = {"OP_MAKE_FUNC", FMT_MAKE_FUNC},
    [OP_MAKE_CLASS] = {"OP_MAKE_CLASS", FMT_MAKE_CLASS},
    [OP_LOAD_SUPER] = {"OP_LOAD_SUPER", FMT_NONE},

    [OP_GET_ITER] = {"OP_GET_ITER", FMT_NONE},
    [OP_FOR_ITER] = {"OP_FOR_ITER", FMT_AX_JUMP},

    [OP_PUSH_EXCEPT] = {"OP_PUSH_EXCEPT", FMT_AX_JUMP},
    [OP_POP_EXCEPT] = {"OP_POP_EXCEPT", FMT_NONE},
    [OP_RAISE] = {"OP_RAISE", FMT_A},
    [OP_RAISE_ASSERT] = {"OP_RAISE_ASSERT", FMT_A},
    [OP_RERAISE] = {"OP_RERAISE", FMT_NONE},
    [OP_LOAD_EXCEPTION] = {"OP_LOAD_EXCEPTION", FMT_NONE},
    [OP_CLEAR_EXCEPTION] = {"OP_CLEAR_EXCEPTION", FMT_NONE},
    [OP_WITH_ENTER] = {"OP_WITH_ENTER", FMT_NONE},
    [OP_WITH_EXIT] = {"OP_WITH_EXIT", FMT_A},

    [OP_GO] = {"OP_GO", FMT_A},
    [OP_MAKE_CHAN] = {"OP_MAKE_CHAN", FMT_A},
    [OP_CHAN_SEND] = {"OP_CHAN_SEND", FMT_NONE},
    [OP_CHAN_RECV] = {"OP_CHAN_RECV", FMT_NONE},
    [OP_CHAN_CLOSE] = {"OP_CHAN_CLOSE", FMT_NONE},
    [OP_AWAIT] = {"OP_AWAIT", FMT_NONE},
    [OP_SELECT_BEGIN] = {"OP_SELECT_BEGIN", FMT_AX_PLAIN},
    [OP_SELECT_CASE_SEND] = {"OP_SELECT_CASE_SEND", FMT_NONE},
    [OP_SELECT_CASE_RECV] = {"OP_SELECT_CASE_RECV", FMT_NONE},
    [OP_SELECT_END] = {"OP_SELECT_END", FMT_NONE},

    [OP_IMPORT] = {"OP_IMPORT", FMT_AX_CONST},
    [OP_IMPORT_FROM] = {"OP_IMPORT_FROM", FMT_AX_CONST},

    [OP_TO_STR] = {"OP_TO_STR", FMT_NONE},
};

// Read the 3-byte big-endian AX operand starting right after the opcode.
static uint32_t readAX(const struct MsChunk* chunk, uint32_t offset) {
  return ((uint32_t) chunk->code[offset + 1] << 16) | ((uint32_t) chunk->code[offset + 2] << 8) |
         (uint32_t) chunk->code[offset + 3];
}

// Read the AX operand as a signed 24-bit two's-complement value.
static int32_t readAXSigned(const struct MsChunk* chunk, uint32_t offset) {
  uint32_t u = readAX(chunk, offset);
  if (u & 0x800000) {
    u |= 0xFF000000u;
  }
  return (int32_t) u;
}

int msDisasmInstr(const struct MsChunk* chunk, uint32_t offset, FILE* fp) {
  uint32_t line = msChunkGetLine(chunk, offset);
  uint32_t prevLine = (offset > 0) ? msChunkGetLine(chunk, offset - 1) : 0;
  char lineNumStr[16];
  snprintf(lineNumStr, sizeof(lineNumStr), "%u", line);
  const char* lineStr = (offset > 0 && line == prevLine) ? "|" : lineNumStr;
  fprintf(fp, "%04X %4s ", offset, lineStr);

  uint8_t op = chunk->code[offset];
  if (op >= OP_COUNT || kOpInfo[op].name == NULL) {
    fprintf(fp, "UNKNOWN(%02X)\n", op);
    return 1;
  }
  const char* name = kOpInfo[op].name;

  switch (kOpInfo[op].fmt) {
    case FMT_NONE:
      fprintf(fp, "%s\n", name);
      return 1;
    case FMT_A:
      fprintf(fp, "%-20s %5u\n", name, chunk->code[offset + 1]);
      return 2;
    case FMT_AX_CONST: {
      uint32_t idx = readAX(chunk, offset);
      fprintf(fp, "%-20s %5u  ", name, idx);
      msValuePrint(chunk->constants[idx], fp);
      fprintf(fp, "\n");
      return 4;
    }
    case FMT_AX_PLAIN:
      fprintf(fp, "%-20s %5u\n", name, readAX(chunk, offset));
      return 4;
    case FMT_AX_JUMP: {
      int32_t rel = readAXSigned(chunk, offset);
      uint32_t target = (uint32_t) ((int32_t) (offset + 4) + rel);
      fprintf(fp, "%-20s %5d -> %04X\n", name, rel, target);
      return 4;
    }
    case FMT_CONST_INT:
      fprintf(fp, "%-20s %5d\n", name, readAXSigned(chunk, offset));
      return 4;
    case FMT_MAKE_FUNC: {
      uint32_t funcIdx = readAX(chunk, offset);
      uint8_t upCount = chunk->code[offset + 4];
      fprintf(fp, "%-20s %5u  upvalues=%u\n", name, funcIdx, upCount);
      return 5 + 2 * (int) upCount;
    }
    case FMT_MAKE_CLASS: {
      uint32_t nameIdx = ((uint32_t) chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
      uint8_t methodCount = chunk->code[offset + 3];
      uint8_t baseCount = chunk->code[offset + 4];
      fprintf(fp, "%-20s %5u  methods=%u bases=%u\n", name, nameIdx, methodCount, baseCount);
      return 5 + 4 * (int) methodCount;
    }
    default:
      assert(0 && "unreachable: unhandled MsOpFmt");
      return 1;
  }
}

void msChunkDisasm(const struct MsChunk* chunk, const char* name, FILE* fp) {
  fprintf(fp, "== %s ==\n", name);
  uint32_t offset = 0;
  while (offset < chunk->codeLen) {
    offset += (uint32_t) msDisasmInstr(chunk, offset, fp);
  }
}
