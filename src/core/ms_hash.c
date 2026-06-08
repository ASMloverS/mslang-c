#include "mslang/ms_hash.h"

uint32_t msFnv1a32Update(uint32_t hash, const void* data, size_t len) {
  const unsigned char* p = (const unsigned char*)data;
  for (size_t i = 0; i < len; i++) {
    hash ^= (uint32_t)p[i];
    hash *= UINT32_C(16777619);
  }
  return hash;
}

uint32_t msFnv1a32(const void* data, size_t len) {
  return msFnv1a32Update(MS_FNV1A32_INIT, data, len);
}

uint64_t msFnv1a64Update(uint64_t hash, const void* data, size_t len) {
  const unsigned char* p = (const unsigned char*)data;
  for (size_t i = 0; i < len; i++) {
    hash ^= (uint64_t)p[i];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

uint64_t msFnv1a64(const void* data, size_t len) {
  return msFnv1a64Update(MS_FNV1A64_INIT, data, len);
}
