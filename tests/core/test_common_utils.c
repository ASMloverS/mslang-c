#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mslang/ms_alloc.h"
#include "mslang/ms_vec.h"
#include "mslang/ms_hash.h"

static void testVecPushAndGrow(void) {
  MsVec(int) v;
  MsVecInit(&v);
  for (int i = 0; i < 1024; i++) {
    MsVecPush(&v, i * 2);
  }
  assert(MsVecLen(&v) == 1024);
  assert(MsVecAt(&v, 0) == 0);
  assert(MsVecAt(&v, 1023) == 2046);
  MsVecFree(&v);
  assert(MsVecLen(&v) == 0);
  printf("PASS testVecPushAndGrow\n");
}

static void testFnv1a32(void) {
  assert(msFnv1a32("", 0)       == UINT32_C(2166136261));
  assert(msFnv1a32("hello", 5)  == UINT32_C(0x4f9f2cab));
  assert(msFnv1a32("foobar", 6) == UINT32_C(0xbf9cf968));
  printf("PASS testFnv1a32\n");
}

static void testFnv1a64(void) {
  assert(msFnv1a64("hello", 5) == UINT64_C(0xa430d84680aabd0b));
  printf("PASS testFnv1a64\n");
}

static void testAllocRealloc(void) {
  // msRealloc(NULL, size) must work like malloc.
  void* p = msRealloc(NULL, 64);
  assert(p != NULL);
  msFree(p);

  // MS_ALLOC_N + MS_REALLOC_N preserve data.
  int* arr = MS_ALLOC_N(int, 4);
  for (int i = 0; i < 4; i++) arr[i] = i;
  arr = MS_REALLOC_N(arr, int, 8);
  assert(arr[0] == 0);
  assert(arr[3] == 3);
  msFree(arr);

  // MS_FREE sets pointer to NULL.
  void* q = msAlloc(8);
  MS_FREE(q);
  assert(q == NULL);

  printf("PASS testAllocRealloc\n");
}

int main(void) {
  testVecPushAndGrow();
  testFnv1a32();
  testFnv1a64();
  testAllocRealloc();
  printf("All tests passed.\n");
  return 0;
}
