// ms_float.c -- msFloatType definition: float (double) runtime type slots (type-system.md ss2.2)
#include "mslang/ms_float.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mslang/ms_object.h"
#include "mslang/ms_str.h"
#include "mslang/ms_value.h"

// tpRepr/tpStr were deferred pending msNewStr (T057); wired up here since M1
// print()/str()/repr() (impl/P4-T067-vm-e2e-m1.md) are the first callers.
// Python-style shortest round-trip repr: try increasing %g precision until
// the formatted text parses back to the exact same double.
static MsValue floatStr(struct MsVM* vm, MsValue v) {
  (void) vm;
  double d = MS_AS_FLOAT(v);
  char buf[32];
  int n = 0;
  for (int prec = 1; prec <= 17; prec++) {
    n = snprintf(buf, sizeof(buf), "%.*g", prec, d);
    if (strtod(buf, NULL) == d) {
      break;
    }
  }
  // Python floats always show a decimal point or exponent (3.0, not 3);
  // %g's plain-integer form (no '.'/'e') needs ".0" appended.
  if (!strpbrk(buf, ".eEnN")) {  // n/N covers "nan"/"inf" spellings, left as-is
    n += snprintf(buf + n, sizeof(buf) - (size_t) n, ".0");
  }
  return msNewStr(buf, (uint32_t) n);
}

static MsValue floatHash(struct MsVM* vm, MsValue v) {
  (void) vm;
  double d = MS_AS_FLOAT(v);
  // integer-valued floats hash equal to the corresponding int (3 == 3.0).
  // isfinite excludes inf/nan: (int64_t)inf/nan is undefined behavior.
  if (isfinite(d) && d >= -9223372036854775808.0 && d < 9223372036854775808.0 && d == (double) (int64_t) d) {
    return MS_INT_VAL((int64_t) d);
  }
  uint64_t bits;
  memcpy(&bits, &d, 8);
  return MS_INT_VAL((int64_t) (bits ^ (bits >> 32)));
}

static MsValue floatEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  return MS_BOOL_VAL(msValueEqual(a, b));
}

static MsValue floatLt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  double da = MS_AS_FLOAT(a);
  if (MS_IS_FLOAT(b)) {
    return MS_BOOL_VAL(da < MS_AS_FLOAT(b));
  }
  if (MS_IS_INT(b)) {
    return MS_BOOL_VAL(da < (double) MS_AS_INT(b));
  }
  return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
}

static MsValue floatAdd(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  double da = MS_AS_FLOAT(a);
  if (MS_IS_FLOAT(b)) {
    return MS_FLOAT_VAL(da + MS_AS_FLOAT(b));
  }
  if (MS_IS_INT(b)) {
    return MS_FLOAT_VAL(da + (double) MS_AS_INT(b));
  }
  return MS_ERROR_VALUE;
}

static MsValue floatSub(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  double da = MS_AS_FLOAT(a);
  if (MS_IS_FLOAT(b)) {
    return MS_FLOAT_VAL(da - MS_AS_FLOAT(b));
  }
  if (MS_IS_INT(b)) {
    return MS_FLOAT_VAL(da - (double) MS_AS_INT(b));
  }
  return MS_ERROR_VALUE;
}

static MsValue floatMul(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  double da = MS_AS_FLOAT(a);
  if (MS_IS_FLOAT(b)) {
    return MS_FLOAT_VAL(da * MS_AS_FLOAT(b));
  }
  if (MS_IS_INT(b)) {
    return MS_FLOAT_VAL(da * (double) MS_AS_INT(b));
  }
  return MS_ERROR_VALUE;
}

static MsValue floatDiv(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  double da = MS_AS_FLOAT(a);
  // IEEE 754: x / 0.0 -> +-inf/nan, no exception (unlike int / 0).
  if (MS_IS_FLOAT(b)) {
    return MS_FLOAT_VAL(da / MS_AS_FLOAT(b));
  }
  if (MS_IS_INT(b)) {
    return MS_FLOAT_VAL(da / (double) MS_AS_INT(b));
  }
  return MS_ERROR_VALUE;
}

static MsValue floatMod(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  double db;
  if (MS_IS_FLOAT(b)) {
    db = MS_AS_FLOAT(b);
  } else if (MS_IS_INT(b)) {
    db = (double) MS_AS_INT(b);
  } else {
    return MS_ERROR_VALUE;
  }
  // IEEE 754: x % 0.0 -> nan, no exception (same rationale as floatDiv).
  double r = fmod(MS_AS_FLOAT(a), db);
  // Python semantics: result takes the sign of the divisor (C fmod takes the
  // sign of the dividend).
  if (r != 0 && (r < 0) != (db < 0)) {
    r += db;
  }
  return MS_FLOAT_VAL(r);
}

static MsValue floatPow(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  double da = MS_AS_FLOAT(a);
  if (MS_IS_FLOAT(b)) {
    return MS_FLOAT_VAL(pow(da, MS_AS_FLOAT(b)));
  }
  if (MS_IS_INT(b)) {
    return MS_FLOAT_VAL(pow(da, (double) MS_AS_INT(b)));
  }
  return MS_ERROR_VALUE;
}

static MsValue floatNeg(struct MsVM* vm, MsValue a) {
  (void) vm;
  return MS_FLOAT_VAL(-MS_AS_FLOAT(a));
}

struct MsType msFloatType = {
    .name = "float",
    .objSize = 0,  // scalar, no heap object
    .tpStr = floatStr,
    .tpRepr = floatStr,  // Python: float.__repr__ == float.__str__
    .tpHash = floatHash,
    .tpEq = floatEq,
    .tpLt = floatLt,
    .tpAdd = floatAdd,
    .tpSub = floatSub,
    .tpMul = floatMul,
    .tpDiv = floatDiv,
    .tpMod = floatMod,
    .tpPow = floatPow,
    .tpNeg = floatNeg,
};
