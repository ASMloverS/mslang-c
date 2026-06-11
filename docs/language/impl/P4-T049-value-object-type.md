# P4-T049 MsValue / MsObject / MsType 完整定义

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

在 P0-T005 骨架基础上，完整定义运行时的三大基础结构：
- `MsValue`：栈值（tagged union，NaN-boxing 或 explicit tag 两选一，初版选显式 tag）
- `MsObject`：堆对象头（类型指针 + GC 状态位）
- `MsType`：类型描述符（方法表 / 类型槽 / MRO 等）

这是整个运行时的基础，P4 所有后续任务均依赖本任务。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P0-T005 | 骨架头文件（`mslang.h`）已存在 |
| P3-T037 | `MsChunk`/`MsOpCode` 定义（函数 proto 引用） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §1 值模型（MsValue tagged union） |
| `type-system.md` | §2 MsObject 堆对象头 |
| `type-system.md` | §3 MsType 类型描述符 / 类型槽 |
| `gc.md` | §1 GC 标记位布局（gcFlags 在 MsObject 中） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增 / 修改文件

```
include/mslang/ms_value.h     # MsValue / MsTag / MsObject / MsType / 类型槽
src/runtime/ms_value.c        # 辅助函数（msValueRepr / msValueEqual / msValueTruthy）
```

---

## 实现要点

### 1. MsTag 枚举

```c
typedef enum MsTag {
  MS_TAG_INT    = 0,
  MS_TAG_FLOAT  = 1,
  MS_TAG_BOOL   = 2,
  MS_TAG_NIL    = 3,
  MS_TAG_OBJ    = 4,    // 堆对象（MsObject*）
  MS_TAG_ERROR  = 5,    // 异常传播哨兵（MS_ERROR_VALUE）
} MsTag;
```

### 2. MsValue

```c
typedef struct MsValue {
  MsTag tag;
  union {
    int64_t    i;
    double     f;
    int        b;
    MsObject*  obj;
  } as;
} MsValue;

// 构造宏
#define MS_NIL_VAL          ((MsValue){MS_TAG_NIL,   {.i = 0}})
#define MS_BOOL_VAL(b_)     ((MsValue){MS_TAG_BOOL,  {.b = (int)(b_)}})
#define MS_INT_VAL(i_)      ((MsValue){MS_TAG_INT,   {.i = (i_)}})
#define MS_FLOAT_VAL(f_)    ((MsValue){MS_TAG_FLOAT, {.f = (f_)}})
#define MS_OBJ_VAL(o_)      ((MsValue){MS_TAG_OBJ,   {.obj = (MsObject*)(o_)}})
#define MS_ERROR_VALUE      ((MsValue){MS_TAG_ERROR,  {.i = 0}})

// 检查宏
#define MS_IS_NIL(v)    ((v).tag == MS_TAG_NIL)
#define MS_IS_BOOL(v)   ((v).tag == MS_TAG_BOOL)
#define MS_IS_INT(v)    ((v).tag == MS_TAG_INT)
#define MS_IS_FLOAT(v)  ((v).tag == MS_TAG_FLOAT)
#define MS_IS_OBJ(v)    ((v).tag == MS_TAG_OBJ)
#define MS_IS_ERROR(v)  ((v).tag == MS_TAG_ERROR)

// 公开 API 检查函数（与 c-api.md §4.4 签名一致；T005 推迟至此实现）
static inline int msIsError(MsValue v) { return v.tag == MS_TAG_ERROR; }

// 提取宏
#define MS_AS_BOOL(v)   ((v).as.b)
#define MS_AS_INT(v)    ((v).as.i)
#define MS_AS_FLOAT(v)  ((v).as.f)
#define MS_AS_OBJ(v)    ((v).as.obj)
```

### 3. MsObject（堆对象头）

```c
// gcFlags 位布局（uint32_t）
#define MS_GC_MARK        0x01   // bit 0：标记位（mark-sweep）
#define MS_GC_GEN_MASK    0x06   // bits 1-2：分代（0=年轻 1=中 2=老）
#define MS_GC_GEN_SHIFT   1
#define MS_GC_FORWARDED   0x08   // bit 3：已被复制（半区复制 forwarded）
#define MS_GC_FINALIZABLE 0x10   // bit 4：可终结（有 __del__）

struct MsObject {
  union {
    struct MsType*   type;  // 正常态：指向类型描述符（必须是第一个成员）
    struct MsObject* fwd;   // GC_FORWARDED 置位时复用为 to-space 目标地址
  };
  uint32_t gcFlags;  // GC 标记位、分代位、转发标志
};
```

### 4. MsType（类型描述符）

```c
// 类型槽（函数指针）
typedef MsValue (*MsUnaryFn) (MsValue a);
typedef MsValue (*MsBinaryFn)(MsValue a, MsValue b);
typedef MsValue (*MsCallFn)  (MsValue self, MsValue* args, int argc);
typedef void    (*MsMarkFn)  (MsObject* obj);  // GC mark 回调
typedef void    (*MsFreeFn)  (MsObject* obj);  // 析构回调

struct MsType {
  const char*  name;         // 类型名称（C 字符串）
  uint32_t     instanceSize; // 实例字节大小（用于 GC 分配）
  MsType**     mro;          // MRO 数组（以 NULL 结尾）
  uint32_t     mroLen;

  // 核心类型槽
  MsUnaryFn    tpRepr;      // repr(obj)
  MsUnaryFn    tpStr;       // str(obj)
  MsUnaryFn    tpHash;      // hash(obj) → MS_INT_VAL
  MsBinaryFn   tpEq;        // obj == other
  MsBinaryFn   tpLt;        // obj < other
  MsCallFn     tpCall;      // obj(...)
  MsMarkFn     tpMark;      // GC mark children
  MsFreeFn     tpFree;      // 析构（非 GC 释放用）

  // 容器槽
  MsBinaryFn   tpGetitem;   // obj[key]
  MsCallFn     tpSetitem;   // obj[key] = val（args=[key,val]）
  MsBinaryFn   tpDelitem;   // del obj[key]
  MsBinaryFn   tpGetattr;   // obj.name（name 为 MsStr）
  MsCallFn     tpSetattr;   // obj.name = val
  MsBinaryFn   tpDelattr;   // del obj.name
  MsUnaryFn    tpIter;      // iter(obj)
  MsUnaryFn    tpNext;      // next(iter)
  MsUnaryFn    tpLen;       // len(obj)

  // 算术槽
  MsBinaryFn   tpAdd, tpSub, tpMul, tpDiv, tpMod, tpPow;
  MsBinaryFn   tpBitand, tpBitor, tpBitxor, tpShl, tpShr;
  MsUnaryFn    tpNeg, tpBitnot;

  // 方法字典（MsMap*，存储 MsStr → MsFunc）
  MsObject*    methods;      // 初始为 NULL（T073 前留空）
};
```

### 5. 辅助函数

```c
// 值相等性检查（结构相等，非身份）
bool msValueEqual(MsValue a, MsValue b);

// 真值测试（Python 语义）
bool msValueTruthy(MsValue v);

// 调试打印（不分配新字符串，直接 fprintf）
void msValuePrint(MsValue v, FILE* fp);

// repr（分配新 MsStr）
MsValue msValueRepr(MsValue v);
```

实现：
- `msValueTruthy`：nil→false，bool→值本身，int→!= 0，float→!= 0.0，str/bytes→len > 0，list/tuple→len > 0，其他→true。
- `msValueEqual`：nil==nil，bool/int/float 跨类型比较（int↔float 提升），str 按 UTF-8 内容，obj 调用 `type->tpEq`。

---

## 验收标准（checklist）

- [ ] `MsValue` 结构大小 ≤ 16 字节（`static_assert(sizeof(MsValue) <= 16)`）。
- [ ] `MsObject` 结构大小 ≤ 24 字节（`static_assert(sizeof(MsObject) <= 24)`）。
- [ ] `MS_INT_VAL(42)` → tag=INT，`MS_AS_INT(v) == 42`。
- [ ] `MS_OBJ_VAL(ptr)` → tag=OBJ，`MS_AS_OBJ(v) == ptr`。
- [ ] `msValueTruthy(MS_NIL_VAL) == false`。
- [ ] `msValueTruthy(MS_INT_VAL(0)) == false`。
- [ ] `msValueTruthy(MS_BOOL_VAL(true)) == true`。
- [ ] `msValueEqual(MS_INT_VAL(3), MS_FLOAT_VAL(3.0)) == true`（数值跨类型）。
- [ ] `msValueEqual(MS_NIL_VAL, MS_BOOL_VAL(false)) == false`（nil ≠ false）。
- [ ] nil 构造宏 `.tag == MS_TAG_NIL`、`MS_ERROR_VALUE.tag == MS_TAG_ERROR`（自 T005 迁入：宏须待 `MsValue` 完整定义后方可实例化验证）。
- [ ] `msIsError(MS_ERROR_VALUE) != 0` 且 `msIsError(MS_NIL_VAL) == 0`（`static inline`，与 c-api.md §4.4 签名一致）。

---

## 测试用例（C 单测）

### `tests/runtime/test_value.c`

```c
#include "ms_test.h"
#include "mslang/ms_value.h"

static void testTagging(void) {
  MsValue iv = MS_INT_VAL(42);
  MS_ASSERT_TRUE(MS_IS_INT(iv),       "is int");
  MS_ASSERT_TRUE(MS_AS_INT(iv) == 42, "value 42");
  MS_ASSERT_TRUE(sizeof(MsValue) <= 16, "size ok");
}

static void testTruthy(void) {
  MS_ASSERT_TRUE(!msValueTruthy(MS_NIL_VAL),        "nil false");
  MS_ASSERT_TRUE(!msValueTruthy(MS_INT_VAL(0)),     "0 false");
  MS_ASSERT_TRUE( msValueTruthy(MS_INT_VAL(1)),     "1 true");
  MS_ASSERT_TRUE(!msValueTruthy(MS_BOOL_VAL(false)),"false→false");
  MS_ASSERT_TRUE( msValueTruthy(MS_BOOL_VAL(true)), "true→true");
}

static void testEqual(void) {
  MS_ASSERT_TRUE( msValueEqual(MS_INT_VAL(3), MS_FLOAT_VAL(3.0)), "3==3.0");
  MS_ASSERT_TRUE(!msValueEqual(MS_NIL_VAL,    MS_BOOL_VAL(false)),"nil!=false");
  MS_ASSERT_TRUE( msValueEqual(MS_NIL_VAL,    MS_NIL_VAL),        "nil==nil");
}

int main(void) {
  MS_RUN(testTagging);
  MS_RUN(testTruthy);
  MS_RUN(testEqual);
  return msTestSummary();
}
```

---

## Benchmark

N/A（`MsValue` 操作成本由 VM 整体 bench 覆盖，T067 提供）。

---

## 风险与边界

- **NaN-boxing 演进**：当前选显式 tag 方案（代码清晰，易调试）；后续性能优化阶段可切换为 NaN-boxing（`double` 中嵌入指针）。切换点：P4 里程碑后（T067）。
- **`MsType.methods` 初为 NULL**：方法字典在 T073（MRO + 方法绑定）初始化，此任务只定义字段。
- **`MS_ERROR_VALUE` 传播**：`MS_TAG_ERROR` 作为异常在栈上的哨兵值；任何从操作返回 `MS_ERROR_VALUE` 的调用，VM 均应立刻停止传播并跳转到异常处理器（T080）。
