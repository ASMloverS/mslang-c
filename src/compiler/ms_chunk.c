#include "mslang/ms_chunk.h"

#include "mslang/ms_alloc.h"

void msChunkInit(struct MsChunk* ck, struct MsStr* sourceName) {
  ck->code = NULL;
  ck->codeLen = 0;
  ck->codeCap = 0;
  ck->constants = NULL;
  ck->constLen = 0;
  ck->constCap = 0;
  ck->lines = NULL;
  ck->linesLen = 0;
  ck->linesCap = 0;
  ck->sourceName = sourceName;
}

void msChunkFree(struct MsChunk* ck) {
  msFree(ck->code);
  msFree(ck->constants);
  msFree(ck->lines);
  msChunkInit(ck, NULL);
}

// grow helper for raw arrays (same strategy as MsVec: start 8, double)
static void chunkGrow(void** data, uint32_t* cap, size_t elemSize) {
  uint32_t newCap = (*cap < 8) ? 8 : (*cap * 2);
  *data = msRealloc(*data, elemSize * newCap);
  *cap = newCap;
}

void msChunkEmit(struct MsChunk* ck, uint8_t byte, uint32_t line) {
  if (ck->codeLen >= ck->codeCap) {
    chunkGrow((void**) &ck->code, &ck->codeCap, sizeof(uint8_t));
  }
  if (ck->linesLen >= ck->linesCap) {
    chunkGrow((void**) &ck->lines, &ck->linesCap, sizeof(uint32_t));
  }
  ck->code[ck->codeLen] = byte;
  ck->lines[ck->linesLen] = line;
  ck->codeLen++;
  ck->linesLen++;
}

void msChunkEmitOpA(struct MsChunk* ck, MsOpCode op, uint8_t a, uint32_t line) {
  msChunkEmit(ck, (uint8_t) op, line);
  msChunkEmit(ck, a, line);
}

void msChunkEmitOpAX(struct MsChunk* ck, MsOpCode op, uint32_t ax, uint32_t line) {
  msChunkEmit(ck, (uint8_t) op, line);
  // 3-byte big-endian
  msChunkEmit(ck, (uint8_t) ((ax >> 16) & 0xFF), line);
  msChunkEmit(ck, (uint8_t) ((ax >> 8) & 0xFF), line);
  msChunkEmit(ck, (uint8_t) (ax & 0xFF), line);
}

uint32_t msChunkAddConst(struct MsChunk* ck, MsValue val) {
  if (ck->constLen >= ck->constCap) {
    chunkGrow((void**) &ck->constants, &ck->constCap, sizeof(MsValue));
  }
  uint32_t idx = ck->constLen;
  ck->constants[ck->constLen++] = val;
  return idx;
}

uint32_t msChunkGetLine(const struct MsChunk* ck, uint32_t offset) {
  return ck->lines[offset];
}

void msChunkPatchJump(struct MsChunk* ck, uint32_t patchOffset, uint32_t targetOffset) {
  // relative offset = target - (patchOffset + 3)
  // patchOffset points to the first operand byte (right after the opcode)
  // the next instruction starts at patchOffset + 3
  int32_t rel = (int32_t) targetOffset - (int32_t) (patchOffset + 3);
  // encode as 3-byte big-endian (signed 24-bit two's complement)
  uint32_t u = (uint32_t) rel & 0x00FFFFFF;
  ck->code[patchOffset] = (uint8_t) ((u >> 16) & 0xFF);
  ck->code[patchOffset + 1] = (uint8_t) ((u >> 8) & 0xFF);
  ck->code[patchOffset + 2] = (uint8_t) (u & 0xFF);
}
