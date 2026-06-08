#include "mslang/ms_vec.h"

void msVecGrow_(void** data, uint32_t* cap, size_t elemSize) {
  uint32_t newCap = (*cap < 8) ? 8 : (*cap * 2);
  *data = msRealloc(*data, (size_t)newCap * elemSize);
  *cap  = newCap;
}
