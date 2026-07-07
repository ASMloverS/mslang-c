// ms_value.c -- MsValue helper functions: equality, truthiness, print, repr
#include "mslang/ms_value.h"

#include <inttypes.h>

#include "mslang/ms_object.h"

bool msValueEqual(MsValue a, MsValue b) {
  if (a.tag != b.tag) {
    // Cross-type: int vs float promotion.
    if (MS_IS_INT(a) && MS_IS_FLOAT(b)) {
      return (double) MS_AS_INT(a) == MS_AS_FLOAT(b);
    }
    if (MS_IS_FLOAT(a) && MS_IS_INT(b)) {
      return MS_AS_FLOAT(a) == (double) MS_AS_INT(b);
    }
    return false;
  }
  switch (a.tag) {
    case MS_TAG_NIL:
      return true;
    case MS_TAG_BOOL:
      return MS_AS_BOOL(a) == MS_AS_BOOL(b);
    case MS_TAG_INT:
      return MS_AS_INT(a) == MS_AS_INT(b);
    case MS_TAG_FLOAT:
      return MS_AS_FLOAT(a) == MS_AS_FLOAT(b);
    case MS_TAG_OBJ: {
      struct MsObject* oa = MS_AS_OBJ(a);
      struct MsObject* ob = MS_AS_OBJ(b);
      if (oa == ob) {
        return true;  // identity equality
      }
      if (oa->type->tpEq) {
        MsValue r = oa->type->tpEq(NULL, a, b);
        return msValueTruthy(r);
      }
      return false;  // default: identity comparison only
    }
    default:
      return false;
  }
}

bool msValueTruthy(MsValue v) {
  switch (v.tag) {
    case MS_TAG_NIL:
      return false;
    case MS_TAG_BOOL:
      return (bool) MS_AS_BOOL(v);
    case MS_TAG_INT:
      return MS_AS_INT(v) != 0;
    case MS_TAG_FLOAT:
      return MS_AS_FLOAT(v) != 0.0;
    case MS_TAG_OBJ: {
      struct MsType* type = MS_AS_OBJ(v)->type;
      if (type->tpBool) {
        return msValueTruthy(type->tpBool(NULL, v));
      }
      if (type->tpLen) {
        return MS_AS_INT(type->tpLen(NULL, v)) > 0;
      }
      return true;
    }
    default:
      return true;
  }
}

void msValuePrint(MsValue v, FILE* fp) {
  switch (v.tag) {
    case MS_TAG_NIL:
      fprintf(fp, "nil");
      break;
    case MS_TAG_BOOL:
      fprintf(fp, "%s", MS_AS_BOOL(v) ? "true" : "false");
      break;
    case MS_TAG_INT:
      fprintf(fp, "%" PRId64, MS_AS_INT(v));
      break;
    case MS_TAG_FLOAT:
      fprintf(fp, "%g", MS_AS_FLOAT(v));
      break;
    case MS_TAG_OBJ:
      fprintf(fp, "<obj>");
      break;
    default:
      fprintf(fp, "<error>");
      break;
  }
}

MsValue msValueRepr(MsValue v) {
  // No built-in MsType is registered yet (int/float/bool/nil land in
  // T053-T055, str in T057), so repr can only dispatch through tpRepr for
  // heap objects; other tags return MS_ERROR_VALUE until their types exist.
  if (MS_IS_OBJ(v)) {
    struct MsType* type = MS_AS_OBJ(v)->type;
    if (type->tpRepr) {
      return type->tpRepr(NULL, v);
    }
  }
  return MS_ERROR_VALUE;
}
