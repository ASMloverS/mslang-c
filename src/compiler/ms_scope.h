#pragma once

#include <stdbool.h>
#include <stdint.h>

struct MsChunk;
struct MsCompileResult;
struct MsLoopCtx;

struct MsLocal {
  const char* name;
  uint32_t nameLen;
  int depth;      // -1 = declared but uninitialized sentinel; 0 = function top
  bool captured;  // captured by inner closure as upvalue
  bool isConst;   // reserved, not used in v1
};

struct MsUpvalue {
  bool isLocal;   // true: from outer local; false: from outer upvalue
  uint8_t index;  // outer local slot or outer upvalue index
};

typedef struct MsCompiler {
  struct MsCompiler* enclosing;  // outer compiler (NULL = top-level)
  struct MsChunk* chunk;
  struct MsLocal locals[256];
  int localCount;
  int scopeDepth;
  struct MsUpvalue upvalues[256];
  int upvalueCount;
  bool isFunction;
  struct MsCompileResult* result;
  struct MsLoopCtx* currentLoop;
} MsCompiler;

void msCompilerInit(MsCompiler* c, MsCompiler* enclosing, struct MsChunk* chunk, bool isFunc);
void msCompilerFree(MsCompiler* c);

void msScopeBegin(MsCompiler* c);
void msScopeEnd(MsCompiler* c);

int msScopeDeclareLocal(MsCompiler* c, const char* name, uint32_t len);
int msScopeResolveLocal(MsCompiler* c, const char* name, uint32_t len);
void msScopeMarkInitialized(MsCompiler* c);

int msScopeResolveUpvalue(MsCompiler* c, const char* name, uint32_t len);
int msScopeAddUpvalue(MsCompiler* c, uint8_t index, bool isLocal);
