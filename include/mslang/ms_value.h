// ms_value.h -- MsTag enumeration and MsValue tagged union
#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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

static_assert(sizeof(MsValue) <= 16, "MsValue too large");

// Construction macros
#define MS_NIL_VAL ((MsValue){MS_TAG_NIL, {.i = 0}})
#define MS_BOOL_VAL(b_) ((MsValue){MS_TAG_BOOL, {.b = (int) (b_)}})
#define MS_INT_VAL(i_) ((MsValue){MS_TAG_INT, {.i = (i_)}})
#define MS_FLOAT_VAL(f_) ((MsValue){MS_TAG_FLOAT, {.f = (f_)}})
#define MS_OBJ_VAL(o_) ((MsValue){MS_TAG_OBJ, {.obj = (struct MsObject*) (o_)}})

// Legacy compat aliases
#define MS_ERROR_VALUE ((MsValue){.tag = MS_TAG_ERROR})
#define MS_NIL MS_NIL_VAL

// Type check macros
#define MS_IS_NIL(v) ((v).tag == MS_TAG_NIL)
#define MS_IS_BOOL(v) ((v).tag == MS_TAG_BOOL)
#define MS_IS_INT(v) ((v).tag == MS_TAG_INT)
#define MS_IS_FLOAT(v) ((v).tag == MS_TAG_FLOAT)
#define MS_IS_OBJ(v) ((v).tag == MS_TAG_OBJ)
#define MS_IS_ERROR(v) ((v).tag == MS_TAG_ERROR)

// Value extraction macros
#define MS_AS_BOOL(v) ((v).as.b)
#define MS_AS_INT(v) ((v).as.i)
#define MS_AS_FLOAT(v) ((v).as.f)
#define MS_AS_OBJ(v) ((v).as.obj)

// Public error-check API (signature matches c-api.md ss4.4; this is the sole
// definition point, not re-declared as an extern function elsewhere).
static inline int msIsError(MsValue v) {
  return v.tag == MS_TAG_ERROR;
}

// Structural equality (not identity).
bool msValueEqual(MsValue a, MsValue b);

// Truthiness test (Python semantics).
bool msValueTruthy(MsValue v);

// Debug print (writes directly, allocates no new string).
void msValuePrint(MsValue v, FILE* fp);

// repr (allocates a new MsStr).
MsValue msValueRepr(MsValue v);
