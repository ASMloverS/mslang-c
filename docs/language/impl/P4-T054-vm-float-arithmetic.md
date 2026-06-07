# P4-T054 float 类型 + 浮点算术指令

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `float` 运行时类型（`MsFloatType`）及浮点算术指令。`float` 使用 IEEE 754 双精度（`double`），与 `int` 的跨类型算术通过类型提升处理。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T053 | `msIntType`/`msTypeOf`/`BINARY_OP` 宏 |
| P4-T051 | 求值循环 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §5 float 类型语义 |
| `syntax.md` | §1.7 浮点字面量 |

---

## 待实现（C 文件）

```
src/runtime/ms_float.c    # msFloatType 定义
include/mslang/ms_float.h
```

---

## 实现要点

### 1. 类型槽实现

```c
static MsValue floatRepr(MsValue v) {
  char buf[64];
  double d = MS_AS_FLOAT(v);
  if (isinf(d))  return msNewStr(d > 0 ? "inf" : "-inf", d > 0 ? 3 : 4);
  if (isnan(d))  return msNewStr("nan", 3);
  // Python 风格：去掉尾随零但保留小数点
  int n = snprintf(buf, sizeof(buf), "%.17g", d);
  // 若无小数点和 e，追加 ".0"
  bool hasDot = false;
  for (int i = 0; i < n; i++) if (buf[i] == '.' || buf[i] == 'e') { hasDot = true; break; }
  if (!hasDot) { buf[n++] = '.'; buf[n++] = '0'; buf[n] = '\0'; }
  return msNewStr(buf, (uint32_t)n);
}

static MsValue floatHash(MsValue v) {
  double d = MS_AS_FLOAT(v);
  // 若 d 是整数值，hash 与对应 int 一致（保证 3 == 3.0 → hash 相同）
  if (d == (double)(int64_t)d) return MS_INT_VAL((int64_t)d);
  // 否则按 double 位模式
  uint64_t bits; memcpy(&bits, &d, 8);
  return MS_INT_VAL((int64_t)(bits ^ (bits >> 32)));
}

static MsValue floatEq(MsValue a, MsValue b) {
  double da = MS_AS_FLOAT(a);
  double db = MS_IS_FLOAT(b) ? MS_AS_FLOAT(b)
               : MS_IS_INT(b)  ? (double)MS_AS_INT(b) : 0;
  if (!MS_IS_FLOAT(b) && !MS_IS_INT(b)) return MS_BOOL_VAL(false);
  return MS_BOOL_VAL(da == db);
}

static MsValue floatAdd(MsValue a, MsValue b) {
  double da = MS_AS_FLOAT(a);
  if (MS_IS_FLOAT(b)) return MS_FLOAT_VAL(da + MS_AS_FLOAT(b));
  if (MS_IS_INT(b))   return MS_FLOAT_VAL(da + (double)MS_AS_INT(b));
  return MS_ERROR_VALUE;  // TypeError
}

// 类似地实现 floatSub/floatMul/floatDiv/floatMod/floatPow/floatNeg/floatLt

MsType msFloatType = {
  .name = "float", .instanceSize = 0,
  .tpRepr   = floatRepr,
  .tpStr    = floatRepr,
  .tpHash   = floatHash,
  .tpEq     = floatEq,
  .tpLt     = floatLt,
  .tpAdd    = floatAdd,
  .tpSub    = floatSub,
  .tpMul    = floatMul,
  .tpDiv    = floatDiv,
  .tpMod    = floatMod,
  .tpPow    = floatPow,
  .tpNeg    = floatNeg,
};
```

### 2. 浮点运算语义

| 运算 | 语义 |
|---|---|
| `a / b`（float） | IEEE 754 除法；`b == 0.0` → `inf` 或 `nan`（不报错） |
| `a % b`（float） | Python 风格：`a - floor(a/b)*b`（与 Python `fmod` 一致） |
| `a ** b`（float） | `pow(a, b)` |
| `float + int` | 提升 int → double，返回 float |
| `-inf`/`nan` | 作为合法浮点值，`repr` 为 `"inf"`/`"-inf"`/`"nan"` |

```c
static MsValue floatDiv(MsValue a, MsValue b) {
  double db = MS_IS_FLOAT(b) ? MS_AS_FLOAT(b)
               : MS_IS_INT(b)  ? (double)MS_AS_INT(b) : 0;
  if (!MS_IS_FLOAT(b) && !MS_IS_INT(b)) return MS_ERROR_VALUE;
  // IEEE 754：/ 0.0 → ±inf/nan，不抛异常
  return MS_FLOAT_VAL(MS_AS_FLOAT(a) / db);
}

static MsValue floatMod(MsValue a, MsValue b) {
  double da = MS_AS_FLOAT(a);
  double db = MS_IS_FLOAT(b) ? MS_AS_FLOAT(b) : (double)MS_AS_INT(b);
  if (!MS_IS_FLOAT(b) && !MS_IS_INT(b)) return MS_ERROR_VALUE;
  double r = fmod(da, db);
  // Python 风格取模：结果与 b 同号
  if (r != 0 && (r < 0) != (db < 0)) r += db;
  return MS_FLOAT_VAL(r);
}
```

---

## 验收标准（checklist）

- [ ] `1.5 + 2.5` → 4.0。
- [ ] `10.0 / 3.0` → 3.3333...（float）。
- [ ] `10.0 / 0.0` → `inf`（不报错）。
- [ ] `1 + 2.0` → 3.0（int 提升为 float）。
- [ ] `2.0 == 2` → true（跨类型相等）。
- [ ] `repr(3.0)` → `"3.0"`（有小数点）。
- [ ] `repr(1e100)` → `"1e+100"` 或等价表示。
- [ ] `hash(3) == hash(3.0)` → true（hash 一致性）。
- [ ] `float % 3.0` 结果与 b 同号（Python 语义）。

---

## 测试用例（C 单测）

### `tests/vm/test_float.c`

```c
#include "ms_test.h"
#include "mslang/ms_vm.h"
#include "mslang/ms_compiler.h"

static MsValue run(const char* src) {
  MsCompileResult r = msCompile(src, strlen(src), "<t>");
  msVMInit();
  MsValue v = msVMRun(r.chunk);
  msVMShutdown();
  msCompileResultFree(&r);
  return v;
}

static void testFloatArith(void) {
  MsValue v = run("1.5 + 2.5");
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && MS_AS_FLOAT(v) == 4.0, "1.5+2.5=4.0");

  v = run("1 + 2.0");
  MS_ASSERT_TRUE(MS_IS_FLOAT(v) && MS_AS_FLOAT(v) == 3.0, "1+2.0=3.0");

  v = run("2.0 == 2");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "2.0==2");
}

int main(void) {
  MS_RUN(testFloatArith);
  return msTestSummary();
}
```

### .ms 使用示例

```ms
print(3.14 * 2)           // 6.28
print(1 / 3)              // 0（整数除法）
print(1.0 / 3)            // 0.3333333333333333
print(0.1 + 0.2 == 0.3)   // false（IEEE 754 精度）
print(round(0.1 + 0.2, 1) == 0.3) // true（round 后）
print(float("inf"))       // inf
print(float("nan"))       // nan
print(1.0 / 0.0)          // inf
```

---

## Benchmark

```ms
// benchmarks/bench_float.ms
n := 10_000_000
x := 1.0
for i in range(n) { x = x * 1.0000001 }
print(x)
// 目标：> 100M float ops/sec
```

---

## 风险与边界

- **`repr` 精度**：使用 `%.17g` 保证往返精度（`float(repr(x)) == x`）；但部分值如 `1.0` 会输出 `"1"`（无小数点），需追加 `".0"`。
- **float `%` Python 语义 vs C `fmod`**：C `fmod` 结果与被除数同号，Python 取模结果与除数同号；需手动调整。
- **`-0.0`**：`repr(-0.0)` 应为 `"-0.0"`（与 Python 一致）；`-0.0 == 0.0` 为 true（IEEE 754）。
