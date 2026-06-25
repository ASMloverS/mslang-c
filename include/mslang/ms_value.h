// ms_value.h -- MsTag enumeration and MsValue tagged union
#pragma once

#include <stdint.h>

// Tag enumeration (type-system.md ss1.2)
typedef enum MsTag {
  MS_TAG_INT = 0,
  MS_TAG_FLOAT = 1,
  MS_TAG_BOOL = 2,
  MS_TAG_NIL = 3,
  MS_TAG_OBJ = 4,
  MS_TAG_ERROR = 5,  // error sentinel
} MsTag;

struct MsObject;

// Tagged union value (type-system.md ss1)
typedef struct MsValue {
  MsTag tag;
  union {
    int64_t i;
    double f;
    int b;
    struct MsObject* obj;
  } as;
} MsValue;

// Construction macros
#define MS_NIL_VAL ((MsValue){MS_TAG_NIL, {.i = 0}})
#define MS_BOOL_VAL(b_) ((MsValue){MS_TAG_BOOL, {.b = (int) (b_)}})
#define MS_INT_VAL(i_) ((MsValue){MS_TAG_INT, {.i = (i_)}})
#define MS_FLOAT_VAL(f_) ((MsValue){MS_TAG_FLOAT, {.f = (f_)}})
#define MS_OBJ_VAL(o_) ((MsValue){MS_TAG_OBJ, {.obj = (struct MsObject*) (o_)}})

// Legacy compat aliases
#define MS_ERROR_VALUE ((MsValue){.tag = MS_TAG_ERROR})
#define MS_NIL MS_NIL_VAL
