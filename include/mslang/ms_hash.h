#pragma once

#include <stddef.h>
#include <stdint.h>

#define MS_FNV1A32_INIT  UINT32_C(2166136261)
#define MS_FNV1A64_INIT  UINT64_C(14695981039346656037)

// One-shot FNV-1a hash.
uint32_t msFnv1a32(const void* data, size_t len);
uint64_t msFnv1a64(const void* data, size_t len);

// Incremental (streaming) variants.
uint32_t msFnv1a32Update(uint32_t hash, const void* data, size_t len);
uint64_t msFnv1a64Update(uint64_t hash, const void* data, size_t len);
