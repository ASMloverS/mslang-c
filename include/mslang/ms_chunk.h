#pragma once

#include <stdint.h>

#include "mslang/ms_opcode.h"
#include "mslang/ms_value.h"

struct MsStr;

struct MsChunk {
  uint8_t* code;
  uint32_t codeLen;
  uint32_t codeCap;

  MsValue* constants;
  uint32_t constLen;
  uint32_t constCap;

  uint32_t* lines;
  uint32_t linesLen;
  uint32_t linesCap;

  struct MsStr* sourceName;
};

void msChunkInit(struct MsChunk* ck, struct MsStr* sourceName);
void msChunkFree(struct MsChunk* ck);

void msChunkEmit(struct MsChunk* ck, uint8_t byte, uint32_t line);

static inline void msChunkEmitOp(struct MsChunk* ck, MsOpCode op, uint32_t line) {
  msChunkEmit(ck, (uint8_t) op, line);
}

void msChunkEmitOpA(struct MsChunk* ck, MsOpCode op, uint8_t a, uint32_t line);

void msChunkEmitOpAX(struct MsChunk* ck, MsOpCode op, uint32_t ax, uint32_t line);

uint32_t msChunkAddConst(struct MsChunk* ck, MsValue val);

uint32_t msChunkGetLine(const struct MsChunk* ck, uint32_t offset);

void msChunkPatchJump(struct MsChunk* ck, uint32_t patchOffset, uint32_t targetOffset);
