// ms_int.c -- msIntType definition: int (int64_t) runtime type slots (type-system.md ss2.1)
#include "mslang/ms_int.h"

#include <math.h>
#include <stdint.h>

#include "mslang/ms_object.h"
#include "mslang/ms_value.h"

// tpRepr/tpStr are deferred to T057 (no MsStr constructor exists yet).

static MsValue intHash(struct MsVM* vm, MsValue v) {
  (void) vm;
  return MS_INT_VAL(MS_AS_INT(v));  // aligned with float hash: 3 == 3.0 must hash equal
}

static MsValue intEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  return MS_BOOL_VAL(msValueEqual(a, b));
}

static MsValue intLt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (MS_IS_INT(b)) {
    return MS_BOOL_VAL(MS_AS_INT(a) < MS_AS_INT(b));
  }
  if (MS_IS_FLOAT(b)) {
    return MS_BOOL_VAL((double) MS_AS_INT(a) < MS_AS_FLOAT(b));
  }
  return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
}

// +/-/* compute in uint64_t then cast back to int64_t: defined wraparound
// semantics, avoids signed-overflow UB (c-style.md ss12.3).
static MsValue intAdd(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (MS_IS_INT(b)) {
    return MS_INT_VAL((int64_t) ((uint64_t) MS_AS_INT(a) + (uint64_t) MS_AS_INT(b)));
  }
  if (MS_IS_FLOAT(b)) {
    return MS_FLOAT_VAL((double) MS_AS_INT(a) + MS_AS_FLOAT(b));
  }
  return MS_ERROR_VALUE;
}

static MsValue intSub(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (MS_IS_INT(b)) {
    return MS_INT_VAL((int64_t) ((uint64_t) MS_AS_INT(a) - (uint64_t) MS_AS_INT(b)));
  }
  if (MS_IS_FLOAT(b)) {
    return MS_FLOAT_VAL((double) MS_AS_INT(a) - MS_AS_FLOAT(b));
  }
  return MS_ERROR_VALUE;
}

static MsValue intMul(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (MS_IS_INT(b)) {
    return MS_INT_VAL((int64_t) ((uint64_t) MS_AS_INT(a) * (uint64_t) MS_AS_INT(b)));
  }
  if (MS_IS_FLOAT(b)) {
    return MS_FLOAT_VAL((double) MS_AS_INT(a) * MS_AS_FLOAT(b));
  }
  return MS_ERROR_VALUE;
}

static MsValue intDiv(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (MS_IS_INT(b)) {
    if (MS_AS_INT(b) == 0) {
      return MS_ERROR_VALUE;  // ZeroDivisionError (T080 placeholder)
    }
    if (MS_AS_INT(b) == -1) {
      // INT64_MIN / -1 overflows signed division (UB/SIGFPE); wrap instead.
      return MS_INT_VAL((int64_t) (0 - (uint64_t) MS_AS_INT(a)));
    }
    return MS_INT_VAL(MS_AS_INT(a) / MS_AS_INT(b));  // truncates toward zero
  }
  if (MS_IS_FLOAT(b)) {
    if (MS_AS_FLOAT(b) == 0.0) {
      return MS_ERROR_VALUE;
    }
    return MS_FLOAT_VAL((double) MS_AS_INT(a) / MS_AS_FLOAT(b));
  }
  return MS_ERROR_VALUE;
}

static MsValue intMod(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (MS_IS_INT(b)) {
    if (MS_AS_INT(b) == 0) {
      return MS_ERROR_VALUE;
    }
    if (MS_AS_INT(b) == -1) {
      // INT64_MIN % -1 overflows signed division (UB/SIGFPE); result is always 0.
      return MS_INT_VAL(0);
    }
    return MS_INT_VAL(MS_AS_INT(a) % MS_AS_INT(b));  // sign follows a (C semantics)
  }
  if (MS_IS_FLOAT(b)) {
    if (MS_AS_FLOAT(b) == 0.0) {
      return MS_ERROR_VALUE;
    }
    return MS_FLOAT_VAL(fmod((double) MS_AS_INT(a), MS_AS_FLOAT(b)));
  }
  return MS_ERROR_VALUE;
}

static MsValue intPow(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (MS_IS_INT(b)) {
    int64_t exp = MS_AS_INT(b);
    if (exp < 0) {
      return MS_FLOAT_VAL(pow((double) MS_AS_INT(a), (double) exp));
    }
    // square-and-multiply in uint64_t: same wraparound rationale as intAdd.
    uint64_t base = (uint64_t) MS_AS_INT(a);
    uint64_t result = 1;
    for (; exp > 0; exp >>= 1) {
      if (exp & 1) {
        result *= base;
      }
      base *= base;
    }
    return MS_INT_VAL((int64_t) result);
  }
  if (MS_IS_FLOAT(b)) {
    return MS_FLOAT_VAL(pow((double) MS_AS_INT(a), MS_AS_FLOAT(b)));
  }
  return MS_ERROR_VALUE;
}

static MsValue intNeg(struct MsVM* vm, MsValue a) {
  (void) vm;
  return MS_INT_VAL((int64_t) (0 - (uint64_t) MS_AS_INT(a)));
}

static MsValue intInvert(struct MsVM* vm, MsValue a) {
  (void) vm;
  return MS_INT_VAL(~MS_AS_INT(a));  // ~a == -(a+1)
}

struct MsType msIntType = {
    .name = "int",
    .objSize = 0,  // scalar, no heap object
    .tpHash = intHash,
    .tpEq = intEq,
    .tpLt = intLt,
    .tpAdd = intAdd,
    .tpSub = intSub,
    .tpMul = intMul,
    .tpDiv = intDiv,
    .tpMod = intMod,
    .tpPow = intPow,
    .tpNeg = intNeg,
    .tpInvert = intInvert,
};
