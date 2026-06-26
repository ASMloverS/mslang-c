#pragma once

#include <stdbool.h>
#include <stdint.h>

struct MsChunk;

typedef struct MsCompileResult {
  struct MsChunk* chunk;
  bool hadError;
  char errBuf[256];
} MsCompileResult;

MsCompileResult msCompile(const char* src, uint32_t srcLen, const char* fileName);
void msCompileResultFree(MsCompileResult* r);
