#pragma once

#include <stdint.h>
#include "ms_alloc.h"

// Macro-generic dynamic array. Usage: MsVec(uint8_t) code; MsVecInit(&code);
#define MsVec(T) struct { T* data; uint32_t len; uint32_t cap; }

#define MsVecInit(v)   ((v)->data = NULL, (v)->len = 0, (v)->cap = 0)
#define MsVecFree(v)   (msFree((v)->data), MsVecInit(v))
#define MsVecLen(v)    ((v)->len)
#define MsVecAt(v, i)  ((v)->data[i])
#define MsVecLast(v)   ((v)->data[(v)->len - 1])

// Push with auto-grow (cap starts at 8, then doubles).
#define MsVecPush(v, val) do {                                       \
  if ((v)->len >= (v)->cap)                                          \
    msVecGrow_((void**)&(v)->data, &(v)->cap, sizeof(*(v)->data));   \
  (v)->data[(v)->len++] = (val);                                     \
} while (0)

// Internal grow helper - do not call directly.
void msVecGrow_(void** data, uint32_t* cap, size_t elemSize);
