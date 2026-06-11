// ms_value.h -- MsTag enumeration and MsValue forward declaration (complete definition in T049)
#pragma once

// Tag enumeration (type-system.md ss1.2)
// Complete MsValue struct is defined in T049.
typedef enum MsTag {
  MS_TAG_INT   = 0,
  MS_TAG_FLOAT = 1,
  MS_TAG_BOOL  = 2,
  MS_TAG_NIL   = 3,
  MS_TAG_OBJ   = 4,
  MS_TAG_ERROR = 5,  // error sentinel
} MsTag;

// Forward declaration (complete definition T049)
typedef struct MsValue MsValue;

// Error sentinel macro (errors.md ss5.2).  MsValue is a forward declaration here;
// macro instantiation and .tag field access are deferred until T049 completes the struct.
#define MS_ERROR_VALUE ((MsValue){.tag = MS_TAG_ERROR})
#define MS_NIL         ((MsValue){.tag = MS_TAG_NIL})

// msIsError is not provided in this task.
// T049 implements: static inline int msIsError(MsValue v)  (c-api.md ss4.4)
