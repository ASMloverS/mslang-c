# P0-T005 公共头与前置类型（`mslang.h` umbrella）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

建立 `include/mslang/mslang.h`（umbrella header）和所有下游任务需要的**前置类型声明**（forward declarations）。后续任务将在各自的头文件中补充完整定义；本任务只建立骨架，使 P1 开始的 lexer 能 `#include <mslang/mslang.h>` 拿到 `MsVM*`、`MsValue`、`MsObject*` 等不透明类型。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P0-T002 | `ms_common.h`（内存/工具）就绪 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §1.1 MsObject 头部、§1.2 MsValue、§1.3 MsType |
| `c-api.md` | §2 公开头文件、§3.1 MsHandle 数据类型 |
| `c-style.md` | §命名规范（ms 前缀、PascalCase 类型） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
include/mslang/mslang.h        # umbrella header（唯一公开头）
include/mslang/ms_common.h     # 工具（T002 已建立，本任务纳入 umbrella）
include/mslang/ms_value.h      # MsValue / MsTag（前置版本，T049 补充完整）
include/mslang/ms_object.h     # MsObject / MsType 前置声明（T049 补充完整）
include/mslang/ms_vm.h         # MsVM 不透明指针前置声明（T051 补充完整）
include/mslang/ms_handle.h     # MsHandle 前置（T126 补充完整）
include/mslang/ms_error.h      # MsErrCode / MS_ASSERT（T002 已建立）
```

### `include/mslang/mslang.h`

```c
// 嵌入者与扩展模块唯一需要包含的头文件
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// 工具
#include "mslang/ms_common.h"
#include "mslang/ms_error.h"

// 核心类型（前置版本，完整定义在各子系统头文件中）
#include "mslang/ms_value.h"
#include "mslang/ms_object.h"
#include "mslang/ms_vm.h"
#include "mslang/ms_handle.h"

// C API 函数声明（T127–T131 中逐步填充）
// 此处仅放最基础的，后续 #include 各 capi_*.h

#ifdef __cplusplus
extern "C" {
#endif

// 版本
#define MSLANG_VERSION_MAJOR 0
#define MSLANG_VERSION_MINOR 1
#define MSLANG_VERSION_PATCH 0
#define MSLANG_VERSION_STR   "0.1.0"

#ifdef __cplusplus
}
#endif

```

### `include/mslang/ms_value.h`（前置版本）

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

// tag 枚举（type-system.md §1.2）
// 完整 MsValue struct 在 T049 中定义
typedef enum MsTag {
  MS_TAG_INT   = 0,
  MS_TAG_FLOAT = 1,
  MS_TAG_BOOL  = 2,
  MS_TAG_NIL   = 3,
  MS_TAG_OBJ   = 4,
  MS_TAG_ERROR = 5,  // 错误哨兵
} MsTag;

// 前置声明（完整定义 T049）
typedef struct MsValue MsValue;

// 错误哨兵常量（errors.md §5.2）
extern const MsValue MS_ERROR_VALUE;
// 便捷检查宏（T049 补充完整内联实现）
#define msIsError(v) ((v).tag == MS_TAG_ERROR)
#define MS_NIL       ((MsValue){.tag = MS_TAG_NIL})

```

### `include/mslang/ms_object.h`（前置版本）

```c
#pragma once

// 前置声明（完整定义 T049）
struct MsObject;
struct MsType;

// 函数指针类型别名（type-system.md §1.3 MsType 中用到）
typedef void     (*MsTraverseFn)(struct MsObject* obj, void* ctx);
typedef void     (*MsDestroyFn) (struct MsObject* obj);
typedef struct MsValue (*MsCallFn)    (struct MsVM* vm,
                                      struct MsValue* argv, int argc);
typedef struct MsValue (*MsBinaryFn)  (struct MsVM* vm,
                                      struct MsValue a, struct MsValue b);
typedef struct MsValue (*MsUnaryFn)   (struct MsVM* vm, struct MsValue a);
typedef struct MsValue (*MsTernaryFn) (struct MsVM* vm,
                                      struct MsValue a, struct MsValue b,
                                      struct MsValue c);
typedef size_t   (*MsSizeFn)   (const struct MsObject* obj);

```

### `include/mslang/ms_vm.h`（前置版本）

```c
#pragma once

// 不透明 VM 指针（完整定义 T051）
typedef struct MsVM MsVM;

// 不透明模块指针（完整定义 T086）
typedef struct MsModule MsModule;

```

### `include/mslang/ms_handle.h`（前置版本）

```c
#pragma once

// 前置声明（完整定义 T126）
typedef struct MsHandleSlot* MsHandle;

```

---

## 实现要点

1. **前置声明与循环包含**：`ms_value.h` 引用 `struct MsObject`（前置声明即可）；`ms_object.h` 函数指针里用到 `MsValue`，因此 `ms_object.h` 需在 `ms_value.h` 之后被包含——umbrella header 中按序排列。
2. **头文件保护**：使用 `#pragma once`（所有主流编译器均已支持，见 `c-style.md` §2.2）。
3. **`MS_ERROR_VALUE` 常量**：在 `ms_value.h` 中 `extern const MsValue MS_ERROR_VALUE`；在 T049 的 `ms_value.c` 中定义初始化值 `{.tag = MS_TAG_ERROR}`。
4. **`MsValue` 的完整定义**：本任务只提供 `typedef struct MsValue MsValue`（前置）；T049 在 `ms_value.h` 中补充 `struct MsValue { MsTag tag; union { … } as; }` 完整结构，现有 `#include` 路径不变。
5. **`extern "C"` 块**：仅在 `mslang.h` 顶层添加，内部头文件不重复添加（避免嵌套）。

---

## 验收标准（checklist）

- [ ] `#include <mslang/mslang.h>` 在一个空 C 文件中编译通过，无警告。
- [ ] `#include <mslang/mslang.h>` 多次包含（多个 .c 文件）无重复定义错误。
- [ ] `MsVM*`、`MsValue`（前置）、`MsHandle` 可作为参数类型在函数声明中使用。
- [ ] `MSLANG_VERSION_STR` 宏展开为 `"0.1.0"`。
- [ ] `MS_NIL.tag == MS_TAG_NIL`（编译期可验证的字面量）。
- [ ] `MS_ERROR_VALUE` 全局常量链接正确（需在某 .c 文件中定义初值，本任务可在 `src/core/ms_value.c` 占位定义，T049 替换）。
- [ ] `cmake --build build` 含本任务头文件后编译通过。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/core/test_public_headers.c`）

```c
#include "ms_test.h"
#include "mslang/mslang.h"

static void testVersionMacro(void) {
  MS_ASSERT_EQ(MSLANG_VERSION_MAJOR, 0, "major");
  MS_ASSERT_EQ(MSLANG_VERSION_MINOR, 1, "minor");
  MS_ASSERT_STR_EQ(MSLANG_VERSION_STR, "0.1.0", "version str");
}

static void testTagValues(void) {
  MS_ASSERT_EQ(MS_TAG_INT,   0, "INT tag");
  MS_ASSERT_EQ(MS_TAG_FLOAT, 1, "FLOAT tag");
  MS_ASSERT_EQ(MS_TAG_NIL,   3, "NIL tag");
  MS_ASSERT_EQ(MS_TAG_ERROR, 5, "ERROR tag");
}

static void testMsNil(void) {
  // MsValue 完整定义 T049 后才能用 .tag；本任务仅测试宏可编译
  // (void)MS_NIL;  // 仅验证宏不报编译错误（因 MsValue 为前置，暂不能访问字段）
  MS_ASSERT_TRUE(1, "headers compile ok");
}

int main(void) {
  MS_RUN(testVersionMacro);
  MS_RUN(testTagValues);
  MS_RUN(testMsNil);
  return msTestSummary();
}
```

> 注：`MsValue` 在本任务为前置声明，其字段在 T049 完整定义后才可访问。此处测试仅验证头文件可编译、宏值正确。

---

## .ms 使用示例

N/A（公共头文件为 C API 基础设施，不直接暴露给脚本层）。

---

## Benchmark

N/A。

---

## 风险与边界

- **前置声明 vs 完整定义的时序**：`MsValue` 为前置时，任何代码不能使用 `sizeof(MsValue)` 或访问其字段——T006（lexer）不需要访问字段，仅用于函数签名，前置声明足够。T049 补充完整定义后所有代码重新编译。
- **`MS_ERROR_VALUE` 的占位定义**：本任务在 `src/core/ms_value.c` 中写 `const MsValue MS_ERROR_VALUE = {.tag = MS_TAG_ERROR};`，但 `MsValue` 结构体尚未完整定义——因此在 T049 之前此文件无法编译。解决方法：T005 阶段先不编译 `ms_value.c`，只在 CMakeLists 中条件性添加，或在 T049 一并创建该文件。
- **未覆盖**：`mslang/exceptions.h`（内置异常类型指针）在 T079 添加；`mslang/capi_*.h` 在 P11 逐步添加。
