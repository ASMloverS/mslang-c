#pragma once

#include <stddef.h>

void* msAlloc(size_t size);
void* msRealloc(void* ptr, size_t newSize);
void  msFree(void* ptr);

#define MS_ALLOC(T)             ((T*)msAlloc(sizeof(T)))
#define MS_ALLOC_N(T, n)        ((T*)msAlloc(sizeof(T) * (size_t)(n)))
#define MS_REALLOC_N(ptr, T, n) ((T*)msRealloc(ptr, sizeof(T) * (size_t)(n)))
#define MS_FREE(ptr)            (msFree(ptr), (ptr) = NULL)

#define MS_ALIGN8(n)  (((size_t)(n) + 7u) & ~7u)
