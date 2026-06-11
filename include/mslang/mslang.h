// mslang.h -- umbrella header; the single header embedders and extension modules need to include
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Version
#define MSLANG_VERSION_MAJOR 0
#define MSLANG_VERSION_MINOR 1
#define MSLANG_VERSION_PATCH 0
#define MSLANG_VERSION_STR   "0.1.0"

// Internal headers and future C API declarations are wrapped in extern "C"
// so C++ embedders get C linkage names.
#ifdef __cplusplus
extern "C" {
#endif

// Utilities
#include "mslang/ms_common.h"
#include "mslang/ms_error.h"

// Core types (forward-declaration versions; complete definitions in sub-system headers;
// each header is self-contained; within each group, ordered alphabetically)
#include "mslang/ms_handle.h"
#include "mslang/ms_object.h"
#include "mslang/ms_value.h"
#include "mslang/ms_vm.h"

// C API function declarations (added incrementally in T127-T131 via capi_*.h includes)

#ifdef __cplusplus
}
#endif
