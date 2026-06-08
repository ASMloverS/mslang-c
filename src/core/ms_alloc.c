#include "mslang/ms_alloc.h"
#include "mslang/ms_error.h"

#include <stdio.h>
#include <stdlib.h>

void* msAlloc(size_t size) {
  void* p = malloc(size);
  if (p == NULL) {
    fprintf(stderr, "mslang: out of memory (requested %zu bytes)\n", size);
    abort();
  }
  return p;
}

void* msRealloc(void* ptr, size_t newSize) {
  void* p = realloc(ptr, newSize);
  if (p == NULL) {
    fprintf(stderr, "mslang: out of memory (realloc %zu bytes)\n", newSize);
    abort();
  }
  return p;
}

void msFree(void* ptr) {
  free(ptr);
}

#ifndef NDEBUG
void msInternalPanic(const char* file, int line, const char* expr) {
  fprintf(stderr, "PANIC %s:%d: %s\n", file, line, expr);
  abort();
}
#endif
