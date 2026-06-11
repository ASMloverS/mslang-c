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
| `type-system.md` | §1.1 MsObject 头部、§1.2 MsValue、§1.3 MsType（含类型槽函数指针签名） |
| `c-api.md` | §2 公开头文件、§3.1 MsHandle 数据类型、§6.1 MsCFunction 签名 |
| `errors.md` | §5.2 异常触发（`MS_ERROR_VALUE` 哨兵） |
| `c-style.md` | §命名规范（ms 前缀、PascalCase 类型） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
include/mslang/mslang.h        # umbrella header（唯一公开头）
include/mslang/ms_common.h     # 工具（既有文件，T002 已建立，本任务仅纳入 umbrella）
include/mslang/ms_value.h      # MsValue / MsTag（前置版本，T049 补充完整）
include/mslang/ms_object.h     # MsObject / MsType 前置声明（T049 补充完整）
include/mslang/ms_vm.h         # MsVM 不透明指针前置声明（T051 补充完整）
include/mslang/ms_handle.h     # MsHandle 前置（T126 补充完整）
include/mslang/ms_error.h      # MsErrCode / MS_ASSERT（既有文件，T002 已建立，本任务仅纳入 umbrella）
```

### `include/mslang/mslang.h`

```c
// 嵌入者与扩展模块唯一需要包含的头文件
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 版本
#define MSLANG_VERSION_MAJOR 0
#define MSLANG_VERSION_MINOR 1
#define MSLANG_VERSION_PATCH 0
#define MSLANG_VERSION_STR   "0.1.0"

// 项目内部头与后续 C API 声明统一包入 extern "C"，保证 C++ 嵌入者获得 C 链接名
#ifdef __cplusplus
extern "C" {
#endif

// 工具
#include "mslang/ms_common.h"
#include "mslang/ms_error.h"

// 核心类型（前置版本，完整定义在各子系统头文件中；各头自包含，组内按字母序）
#include "mslang/ms_handle.h"
#include "mslang/ms_object.h"
#include "mslang/ms_value.h"
#include "mslang/ms_vm.h"

// C API 函数声明（T127–T131 中逐步 #include 各 capi_*.h）

#ifdef __cplusplus
}
#endif

```

### `include/mslang/ms_value.h`（前置版本）

```c
// ms_value.h — MsTag 枚举与 MsValue 前置声明（完整定义见 T049）
#pragma once

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

// 错误哨兵宏（errors.md §5.2）；MsValue 为前置声明，宏展开实例化须待 T049 补全定义
#define MS_ERROR_VALUE ((MsValue){.tag = MS_TAG_ERROR})
#define MS_NIL         ((MsValue){.tag = MS_TAG_NIL})

// msIsError 检查函数不在本任务提供：
// T049 在 MsValue 完整定义后以 static inline int msIsError(MsValue v) 实现（c-api.md §4.4）

```

### `include/mslang/ms_object.h`（前置版本）

```c
// ms_object.h — MsObject/MsType 前置声明与类型槽函数指针别名（完整定义见 T049）
#pragma once

#include <stddef.h>

// 前置声明（自包含：不依赖 ms_value.h / ms_vm.h 的包含顺序）
struct MsObject;
struct MsType;
struct MsValue;
struct MsVM;

// 函数指针类型别名（权威签名见 type-system.md §1.3）
// GC 子引用访问者：traverse 对对象内每个持有堆引用的 MsValue 槽位调用一次，
// slot 为槽位地址，GC 可就地更新（半区复制时改写为 to-space 新地址，见 gc.md §6/§9）
typedef void     (*MsVisitFn)   (struct MsValue* slot, void* ctx);
typedef void     (*MsTraverseFn)(struct MsObject* obj, MsVisitFn visit, void* ctx);
typedef void     (*MsDestroyFn) (struct MsObject* obj);
// MsCallFn 与 c-api.md §6.1 的 MsCFunction 同构
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
// ms_vm.h — MsVM/MsModule 不透明指针前置声明（完整定义见 T051/T086）
#pragma once

// 不透明 VM 指针（完整定义 T051）
typedef struct MsVM MsVM;

// 不透明模块指针（完整定义 T086）
typedef struct MsModule MsModule;

```

### `include/mslang/ms_handle.h`（前置版本）

```c
// ms_handle.h — MsHandle 前置声明（完整定义见 T126）
#pragma once

// 前置声明（完整定义 T126）
typedef struct MsHandleSlot* MsHandle;

```

---

## 实现要点

1. **头文件自包含**：每个头自行 `#include` 或前置声明所需类型（如 `ms_object.h` 包含 `<stddef.h>` 并前置声明 `struct MsValue` / `struct MsVM`），不依赖 umbrella 的包含顺序（`c-style.md` §2.3）；umbrella 内 `#include` 组内按字母序排列，与 clang-format 的 `SortIncludes` 兼容（`c-style.md` §2.4）。
2. **头文件保护**：使用 `#pragma once`（所有主流编译器均已支持，见 `c-style.md` §2.2）。T002 既有头（`ms_common.h`、`ms_error.h`）若仍为 `#ifndef` 宏守卫，本任务纳入 umbrella 时一并迁移为 `#pragma once`。
3. **`MS_ERROR_VALUE` 哨兵**：定义为宏 `((MsValue){.tag = MS_TAG_ERROR})`（语义见 `errors.md` §5.2），不引入 extern 常量，无需占位 `.c` 文件；T049 补全 `struct MsValue` 后宏即可实例化。`msIsError` 不在本任务提供，T049 以 `static inline int msIsError(MsValue v)` 实现（与 `c-api.md` §4.4 签名一致；宏形式会破坏后续同名函数声明，且 camelCase 宏违反 `c-style.md` §3.6）。
4. **`MsValue` 的完整定义**：本任务只提供 `typedef struct MsValue MsValue`（前置）；T049 在 `ms_value.h` 中补充 `struct MsValue { MsTag tag; union { … } as; }` 完整结构（带 struct 标签，见 type-system.md §1.2），现有 `#include` 路径不变。
5. **`extern "C"` 块**：在 `mslang.h` 中将项目内部头的 `#include` 与后续 C API 声明一并包入 `extern "C"` 块（标准库头留在块外），保证 C++ 嵌入者获得 C 链接名；内部头文件不重复添加（避免嵌套）。

---

## 验收标准（checklist）

- [ ] `#include <mslang/mslang.h>` 在一个空 C 文件中编译通过，无警告。
- [ ] `#include <mslang/mslang.h>` 多次包含（多个 .c 文件）无重复定义错误。
- [ ] `MsVM*`、`MsValue`（前置）、`MsHandle` 可作为参数类型在函数声明中使用。
- [ ] `MSLANG_VERSION_STR` 宏展开为 `"0.1.0"`。
- [ ] `MS_NIL`、`MS_ERROR_VALUE` 宏已定义（`MsValue` 本任务为前置声明，宏实例化与 `.tag` 字段验证迁移至 T049 验收）。
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

- **前置声明 vs 完整定义的时序**：`MsValue` 为前置时，任何代码不能使用 `sizeof(MsValue)`、访问其字段或实例化 `MS_NIL`/`MS_ERROR_VALUE` 宏——T006（lexer）不需要访问字段，仅用于函数签名，前置声明足够。T049 补充完整定义后所有代码重新编译。
- **未覆盖**：`mslang/exceptions.h`（内置异常类型指针）在 T079 添加；`mslang/capi_*.h` 在 P11 逐步添加。
