#include "ms_scope.h"

#include <stdio.h>
#include <string.h>

#include "mslang/ms_chunk.h"
#include "mslang/ms_error.h"
#include "mslang/ms_opcode.h"

void msCompilerInit(MsCompiler* c, MsCompiler* enclosing, struct MsChunk* chunk, bool isFunc) {
  c->enclosing = enclosing;
  c->chunk = chunk;
  c->localCount = 0;
  c->scopeDepth = 0;
  c->upvalueCount = 0;
  c->isFunction = isFunc;
  memset(c->locals, 0, sizeof(c->locals));
  memset(c->upvalues, 0, sizeof(c->upvalues));
}

void msCompilerFree(MsCompiler* c) {
  c->enclosing = NULL;
  c->chunk = NULL;
  c->localCount = 0;
  c->scopeDepth = 0;
  c->upvalueCount = 0;
  c->isFunction = false;
}

void msScopeBegin(MsCompiler* c) {
  c->scopeDepth++;
}

void msScopeEnd(MsCompiler* c) {
  while (c->localCount > 0 && c->locals[c->localCount - 1].depth == c->scopeDepth) {
    if (c->locals[c->localCount - 1].captured) {
      msChunkEmitOp(c->chunk, OP_CLOSE_UPVALUE, 0);
    } else {
      msChunkEmitOp(c->chunk, OP_POP, 0);
    }
    c->localCount--;
  }
  c->scopeDepth--;
}

int msScopeDeclareLocal(MsCompiler* c, const char* name, uint32_t len) {
  if (c->localCount >= 256) {
    fprintf(stderr, "error: too many local variables (max 256)\n");
    return -1;
  }

  // check for duplicate at same depth
  for (int i = c->localCount - 1; i >= 0; i--) {
    if (c->locals[i].depth != -1 && c->locals[i].depth < c->scopeDepth) {
      break;
    }
    if (c->locals[i].nameLen == len && memcmp(c->locals[i].name, name, len) == 0) {
      fprintf(stderr, "error: variable '%.*s' already declared in this scope\n", (int) len, name);
      return -1;
    }
  }

  int slot = c->localCount;
  c->locals[slot].name = name;
  c->locals[slot].nameLen = len;
  c->locals[slot].depth = -1;
  c->locals[slot].captured = false;
  c->locals[slot].isConst = false;
  c->localCount++;
  return slot;
}

int msScopeResolveLocal(MsCompiler* c, const char* name, uint32_t len) {
  for (int i = c->localCount - 1; i >= 0; i--) {
    if (c->locals[i].nameLen == len && memcmp(c->locals[i].name, name, len) == 0) {
      if (c->locals[i].depth == -1) {
        fprintf(stderr, "error: cannot read local variable in its own initializer\n");
        return -1;
      }
      return i;
    }
  }
  return -1;
}

void msScopeMarkInitialized(MsCompiler* c) {
  MS_ASSERT(c->localCount > 0);
  c->locals[c->localCount - 1].depth = c->scopeDepth;
}

int msScopeResolveUpvalue(MsCompiler* c, const char* name, uint32_t len) {
  if (c->enclosing == NULL) {
    return -1;
  }

  int slot = msScopeResolveLocal(c->enclosing, name, len);
  if (slot != -1) {
    c->enclosing->locals[slot].captured = true;
    return msScopeAddUpvalue(c, (uint8_t) slot, true);
  }

  int uvIdx = msScopeResolveUpvalue(c->enclosing, name, len);
  if (uvIdx != -1) {
    return msScopeAddUpvalue(c, (uint8_t) uvIdx, false);
  }

  return -1;
}

int msScopeAddUpvalue(MsCompiler* c, uint8_t index, bool isLocal) {
  for (int i = 0; i < c->upvalueCount; i++) {
    if (c->upvalues[i].index == index && c->upvalues[i].isLocal == isLocal) {
      return i;
    }
  }

  if (c->upvalueCount >= 256) {
    fprintf(stderr, "error: too many upvalues (max 256)\n");
    return -1;
  }

  c->upvalues[c->upvalueCount].isLocal = isLocal;
  c->upvalues[c->upvalueCount].index = index;
  return c->upvalueCount++;
}
