# P4-T053 int 类型 + 整数算术指令

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `int` 运行时类型（`MsIntType`）及所有整数算术指令。`int` 是 mslang 的核心值类型，使用 64 位有符号整数（`int64_t`），除法截断为零（C 语义），位操作与 Python 一致（右移算术移位，`~x = -(x+1)`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T051 | 求值循环与 PUSH/POP 宏 |
| P4-T049 | `MsValue`/`MsType` 定义（`MS_INT_VAL`/`MS_AS_INT`） |
| P4-T050 | `msGCAlloc`（int 为标量值，不需 GC，但 repr 等需分配 str） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §4 int 类型语义 |
| `syntax.md` | §1.6 整数字面量 |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
src/runtime/ms_int.c          # msIntType 定义 + 类型槽实现
include/mslang/ms_int.h       # 公共声明
```

### 修改文件

```
src/vm/ms_vm.c                # OP_ADD/SUB/MUL/DIV/MOD/POW/NEG/BITNOT/
                               #   BITAND/BITOR/BITXOR/SHL/SHR case 实现
```

---

## 实现要点

### 1. 类型槽实现

```c
// repr：返回 MsStr "$i"
static MsValue intRepr(MsValue v) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%" PRId64, MS_AS_INT(v));
  return msNewStr(buf, strlen(buf));
}

// hash：简单取模
static MsValue intHash(MsValue v) {
  return MS_INT_VAL(MS_AS_INT(v));  // 与 float hash 对齐（3 == 3.0 必须 hash 相同）
}

// eq
static MsValue intEq(MsValue a, MsValue b) {
  if (MS_IS_INT(b))   return MS_BOOL_VAL(MS_AS_INT(a) == MS_AS_INT(b));
  if (MS_IS_FLOAT(b)) return MS_BOOL_VAL((double)MS_AS_INT(a) == MS_AS_FLOAT(b));
  return MS_BOOL_VAL(false);
}

// lt
static MsValue intLt(MsValue a, MsValue b) {
  if (MS_IS_INT(b))   return MS_BOOL_VAL(MS_AS_INT(a) < MS_AS_INT(b));
  if (MS_IS_FLOAT(b)) return MS_BOOL_VAL((double)MS_AS_INT(a) < MS_AS_FLOAT(b));
  return MS_ERROR_VALUE;  // TypeError
}

MsType msIntType = {
  .name = "int", .instanceSize = 0,  // 标量，不分配堆对象
  .tpRepr  = intRepr,
  .tpStr   = intRepr,
  .tpHash  = intHash,
  .tpEq    = intEq,
  .tpLt    = intLt,
  .tpAdd   = intAdd,
  .tpSub   = intSub,
  .tpMul   = intMul,
  .tpDiv   = intDiv,
  .tpMod   = intMod,
  .tpPow   = intPow,
  .tpNeg   = intNeg,
  .tpBitnot = intBitnot,
  .tpBitand = intBitand,
  .tpBitor  = intBitor,
  .tpBitxor = intBitxor,
  .tpShl    = intShl,
  .tpShr    = intShr,
};
```

### 2. VM 算术指令

```c
// 通用二元算术分派宏
#define BINARY_OP(slot) do {                         \
  MsValue b = POP(), a = POP();                    \
  MsType* ta = msTypeOf(a);                        \
  MsValue r = ta->slot ? ta->slot(a, b)            \
                          : msNotImplemented(a, b);  \
  if (MS_IS_ERROR(r)) return r;                    \
  PUSH(r);                                         \
} while(0)

case OP_ADD:    BINARY_OP(tpAdd);    DISPATCH();
case OP_SUB:    BINARY_OP(tpSub);    DISPATCH();
case OP_MUL:    BINARY_OP(tpMul);    DISPATCH();
case OP_DIV:    BINARY_OP(tpDiv);    DISPATCH();
case OP_MOD:    BINARY_OP(tpMod);    DISPATCH();
case OP_POW:    BINARY_OP(tpPow);    DISPATCH();
case OP_BITAND: BINARY_OP(tpBitand); DISPATCH();
case OP_BITOR:  BINARY_OP(tpBitor);  DISPATCH();
case OP_BITXOR: BINARY_OP(tpBitxor); DISPATCH();
case OP_SHL:    BINARY_OP(tpShl);    DISPATCH();
case OP_SHR:    BINARY_OP(tpShr);    DISPATCH();

case OP_NEG: {
  MsValue a = POP();
  MsType* ta = msTypeOf(a);
  MsValue r = ta->tpNeg ? ta->tpNeg(a) : MS_ERROR_VALUE;
  if (MS_IS_ERROR(r)) return r;
  PUSH(r);
  DISPATCH();
}
case OP_BITNOT: {
  // ~a = -(a+1) for int
  MsValue a = POP();
  if (!MS_IS_INT(a)) { return msTypeError(t, "bitnot requires int"); }
  PUSH(MS_INT_VAL(~MS_AS_INT(a)));
  DISPATCH();
}
```

### 3. 整数运算语义

| 运算 | 语义 |
|---|---|
| `a / b` | 截断除法（C 语义，向零取整）；`b==0` → ZeroDivisionError |
| `a % b` | 余数（与 a 同号，C 语义）；`b==0` → ZeroDivisionError |
| `a ** b` | 幂运算；`b < 0` → 浮点结果（转为 float）；`b >= 0` → int |
| `a << b` | 逻辑左移；`b < 0` 或 `b >= 64` → 报错 |
| `a >> b` | 算术右移（有符号）；`b < 0` 或 `b >= 64` → 报错 |
| `~a` | 按位取反，等价 `-(a+1)` |

```c
static MsValue intDiv(MsValue a, MsValue b) {
  if (MS_IS_INT(b)) {
    if (MS_AS_INT(b) == 0) return MS_ERROR_VALUE;  // ZeroDivisionError（T080）
    return MS_INT_VAL(MS_AS_INT(a) / MS_AS_INT(b));
  }
  if (MS_IS_FLOAT(b)) {
    if (MS_AS_FLOAT(b) == 0.0) return MS_ERROR_VALUE;
    return MS_FLOAT_VAL((double)MS_AS_INT(a) / MS_AS_FLOAT(b));
  }
  return MS_ERROR_VALUE;  // TypeError
}

static MsValue intPow(MsValue a, MsValue b) {
  if (MS_IS_INT(b)) {
    int64_t exp = MS_AS_INT(b);
    if (exp < 0) {
      // 负指数 → float
      return MS_FLOAT_VAL(pow((double)MS_AS_INT(a), (double)exp));
    }
    int64_t base = MS_AS_INT(a), result = 1;
    for (; exp > 0; exp >>= 1) {
      if (exp & 1) result *= base;
      base *= base;
    }
    return MS_INT_VAL(result);
  }
  if (MS_IS_FLOAT(b)) {
    return MS_FLOAT_VAL(pow((double)MS_AS_INT(a), MS_AS_FLOAT(b)));
  }
  return MS_ERROR_VALUE;
}
```

### 4. `msTypeOf` 辅助

```c
static inline MsType* msTypeOf(MsValue v) {
  switch (v.tag) {
  case MS_TAG_INT:   return &msIntType;
  case MS_TAG_FLOAT: return &msFloatType;
  case MS_TAG_BOOL:  return &msBoolType;
  case MS_TAG_NIL:   return &msNilType;
  case MS_TAG_OBJ:   return MS_AS_OBJ(v)->type;
  default:           return NULL;
  }
}
```

---

## 验收标准（checklist）

- [ ] `1 + 2` → 3（MS_INT_VAL）。
- [ ] `10 / 3` → 3（截断除法）。
- [ ] `10 % 3` → 1。
- [ ] `2 ** 10` → 1024（整数幂）。
- [ ] `2 ** -1` → 0.5（负指数 → float）。
- [ ] `5 / 0` → ZeroDivisionError（MS_ERROR_VALUE，T080 后完整报错）。
- [ ] `-42` → -42（取负）。
- [ ] `~5` → -6（按位取反）。
- [ ] `3 << 2` → 12；`16 >> 2` → 4。
- [ ] `int + float` → float（跨类型提升）。

---

## 测试用例（C 单测）

### `tests/vm/test_int_arith.c`

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

static void testBasicArith(void) {
  MS_ASSERT_TRUE(MS_AS_INT(run("1 + 2"))   == 3,   "+ ok");
  MS_ASSERT_TRUE(MS_AS_INT(run("10 - 4"))  == 6,   "- ok");
  MS_ASSERT_TRUE(MS_AS_INT(run("3 * 7"))   == 21,  "* ok");
  MS_ASSERT_TRUE(MS_AS_INT(run("10 / 3"))  == 3,   "/ trunc ok");
  MS_ASSERT_TRUE(MS_AS_INT(run("10 % 3"))  == 1,   "% ok");
  MS_ASSERT_TRUE(MS_AS_INT(run("2 ** 10")) == 1024,"** ok");
}

static void testBitOps(void) {
  MS_ASSERT_TRUE(MS_AS_INT(run("5 & 3"))  == 1,  "& ok");
  MS_ASSERT_TRUE(MS_AS_INT(run("5 | 3"))  == 7,  "| ok");
  MS_ASSERT_TRUE(MS_AS_INT(run("5 ^ 3"))  == 6,  "^ ok");
  MS_ASSERT_TRUE(MS_AS_INT(run("3 << 2")) == 12, "<< ok");
  MS_ASSERT_TRUE(MS_AS_INT(run("16 >> 2"))== 4,  ">> ok");
  MS_ASSERT_TRUE(MS_AS_INT(run("~5"))     == -6, "~ ok");
}

int main(void) {
  MS_RUN(testBasicArith);
  MS_RUN(testBitOps);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// 基本算术
print(2 ** 32)          // 4294967296
print(1_000_000 * 1_000_000) // 1000000000000

// 截断除法
print(7 / 2)   // 3
print(-7 / 2)  // -3（向零，非向负无穷）

// 位操作
mask := 0xFF
val  := 0xABCD
print((val >> 8) & mask)  // 0xAB = 171
print(val & mask)         // 0xCD = 205
```

---

## Benchmark

```ms
// benchmarks/bench_int_arith.ms（T067 后可运行）
n := 10_000_000
sum := 0
for i in range(n) {
    sum += i
}
print(sum)  // 49999995000000
// 目标：> 100M iterations/sec
```

---

## 风险与边界

- **溢出行为**：`int64_t` 在 C 中有符号溢出是未定义行为；`a * b` 等运算不做溢出检查（初版接受回绕语义；后续可添加 `__int128` 溢出检测或大整数切换）。
- **`a ** b` 整数溢出**：大幂运算（如 `2 ** 100`）会溢出；初版不检查，后续可引入大整数或报错。
- **跨类型算术**：`int op float` → float，通过 `intAdd` 检查 `b.tag == MS_TAG_FLOAT` 并提升。`float op int` → 同理在 `floatAdd`（T054）处理。
