# P0-T002 通用工具：内存/动态数组/错误码/FNV-1a

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现贯穿整个运行时的底层工具模块：内存分配封装（带 OOM 检测）、泛型动态数组（`MsVec`）、全局错误码枚举、FNV-1a 哈希函数。这些基础设施被 lexer、parser、compiler、VM、GC 等所有上层模块共用，必须在 P1 开始前就绪。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P0-T001 | CMake 构建系统就绪 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §2.5 MsStr（FNV-1a hash 字段说明） |
| `vm.md` | §2 MsChunk（手写增长数组字段 codeLen/codeCap，动态数组宽度 uint32_t 一致性参照） |
| `execution.md` | §4 .msc 格式、§4.2 失效模式（source_hash = 源文件 FNV-1a 64） |
| `gc.md` | 分配对齐 ALIGN8（MS_ALIGN8 依据，gc.md size = ALIGN8(size)） |
| `c-style.md` | §命名规范（ms 前缀、驼峰） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
include/mslang/ms_common.h    # 统一工具头（umbrella）
include/mslang/ms_alloc.h     # 内存分配
include/mslang/ms_vec.h       # 泛型动态数组
include/mslang/ms_hash.h      # FNV-1a 哈希
include/mslang/ms_error.h     # 错误码枚举
src/core/ms_alloc.c
src/core/ms_vec.c
src/core/ms_hash.c
```

### 内存分配（`ms_alloc.h`）

```c
// 封装 malloc/realloc/free，OOM 时打印诊断后 abort()（T049 补充全局 VM 指针后可改为抛异常）
void* msAlloc(size_t size);                        // 等同 malloc，OOM→abort
void* msRealloc(void* ptr, size_t newSize);        // OOM→abort
void  msFree(void* ptr);                           // 等同 free(ptr)

// 类型安全宏（避免手写 sizeof）
#define MS_ALLOC(T)           ((T*)msAlloc(sizeof(T)))
#define MS_ALLOC_N(T, n)      ((T*)msAlloc(sizeof(T) * (size_t)(n)))
#define MS_REALLOC_N(ptr, T, n) ((T*)msRealloc(ptr, sizeof(T) * (size_t)(n)))
#define MS_FREE(ptr)          (msFree(ptr), (ptr) = NULL)

// 对齐分配（GC 分配器用，8 字节对齐）
#define MS_ALIGN8(n)  (((size_t)(n) + 7u) & ~7u)
```

### 泛型动态数组（`ms_vec.h`）

采用宏泛型方案（无 void* 擦除，保留类型安全）：

```c
// 使用示例：MsVec(uint8_t) code;  MsVecInit(&code);  MsVecPush(&code, 0xAB);
//
// 内部表示（所有实例共享布局，仅 T 不同）
#define MsVec(T) struct { T* data; uint32_t len; uint32_t cap; }

#define MsVecInit(v)      ((v)->data = NULL, (v)->len = 0, (v)->cap = 0)
#define MsVecFree(v)      (msFree((v)->data), MsVecInit(v))
#define MsVecLen(v)       ((v)->len)
#define MsVecAt(v, i)     ((v)->data[i])
#define MsVecLast(v)      ((v)->data[(v)->len - 1])

// Push：自动扩容（cap *= 2，初始 cap=8）
#define MsVecPush(v, val) do {                              \
  if ((v)->len >= (v)->cap) msVecGrow_((void**)&(v)->data, \
    &(v)->cap, sizeof(*(v)->data));                     \
  (v)->data[(v)->len++] = (val);                          \
} while(0)

// 内部扩容函数（仅供宏调用，不对外暴露；尾部 _ 为内部约定标记，不属于公开 API）
void msVecGrow_(void** data, uint32_t* cap, size_t elemSize);
```

### FNV-1a 哈希（`ms_hash.h`）

```c
// FNV-1a 32 位（用于字符串快速哈希，如 lexer 关键字查找）
uint32_t msFnv1a32(const void* data, size_t len);

// FNV-1a 64 位（.msc 缓存校验：哈希 .ms 源文件内容，存入 .msc 头 source_hash，见 execution.md §4.2）
uint64_t msFnv1a64(const void* data, size_t len);

// 增量版本（用于流式哈希）
#define MS_FNV1A32_INIT  UINT32_C(2166136261)
#define MS_FNV1A64_INIT  UINT64_C(14695981039346656037)
uint32_t msFnv1a32Update(uint32_t hash, const void* data, size_t len);
uint64_t msFnv1a64Update(uint64_t hash, const void* data, size_t len);
```

### 错误码（`ms_error.h`）

```c
typedef enum MsErrCode {
  MS_OK            = 0,
  MS_ERR_OOM       = 1,   // 内存不足
  MS_ERR_IO        = 2,   // I/O 错误
  MS_ERR_SYNTAX    = 3,   // 词法/语法错误
  MS_ERR_RUNTIME   = 4,   // 运行时异常（由 MS_ERROR_VALUE 携带）
  MS_ERR_IMPORT    = 5,   // 模块导入失败
  MS_ERR_INTERNAL  = 6,   // 内部断言失败（应 abort）
} MsErrCode;

// 断言宏（调试 build 验证内部不变量，release 下 NDEBUG 消除）
#ifdef NDEBUG
#  define MS_ASSERT(cond) ((void)0)
#else
#  define MS_ASSERT(cond) \
  ((cond) ? (void)0 : (msInternalPanic(__FILE__, __LINE__, #cond), (void)0))
#endif

#ifndef NDEBUG
void msInternalPanic(const char* file, int line, const char* expr);
#endif
```

---

## 实现要点

1. **`msVecGrow_` 增长策略**：初始 `cap=8`；每次 `cap < 8` 时置为 8，否则 `cap *= 2`。防止频繁 realloc。
2. **FNV-1a 正确性**：字节序无关（逐字节处理），无跨平台差异。32 位版用于运行时 map/set 哈希桶；64 位版用于文件缓存校验（`.msc` hash 模式）。
3. **`MS_ASSERT` 在 Debug 下输出文件名/行号后调用 `abort()`**；Release 下完全消除（`NDEBUG`）。`msInternalPanic` 仅在 Debug（`!NDEBUG`）下声明与定义，故 Release 下符号天然缺失。
4. **`msFree` 接受 `NULL`**（C 标准，`free(NULL)` 为 no-op）；`MS_FREE` 宏额外置 `NULL` 防止悬空指针使用。
5. **`MsVec` 宏**：不用 `_Generic` 或 void* 回调，保持 C17 兼容性与调试体验。类型安全由编译器在宏展开点检查。

---

## 验收标准（checklist）

- [ ] `cmake --build build`（含新源文件）编译通过，无警告。 <!-- v:build -->
- [ ] `msVecPush` 压入 1024 个 uint64_t 元素后，`len==1024`，`data[1023]` 值正确。 <!-- v:ctest:test_common_utils -->
- [ ] `msFnv1a32("hello", 5)` 返回 `0x4f9f2cab`（已知标准值）。 <!-- v:ctest:test_common_utils -->
- [ ] `msFnv1a64("hello", 5)` 返回 `0xa430d84680aabd0b`（已知标准值）。 <!-- v:ctest:test_common_utils -->
- [ ] `msRealloc(NULL, 64)` 等价 `malloc(64)` 正常返回。 <!-- v:ctest:test_common_utils -->
- [ ] `MS_FREE(ptr)` 执行后 `ptr == NULL`。 <!-- v:ctest:test_common_utils -->
- [ ] `MS_ASSERT(0)` 在 Debug build 打印文件名与行号后 abort（验证：运行后退出码非零且有输出）。 <!-- v:ctest:test_ms_assert_abort -->
- [ ] `MS_ASSERT(1)` 在 Debug build 无副作用。 <!-- v:ctest:test_common_utils -->
- [ ] Release build（`-DCMAKE_BUILD_TYPE=Release`）下 `MS_ASSERT(0)` 被完全消除（nm/objdump 检查无 `msInternalPanic` 符号，因 Debug-only 编译，符号必然缺失）。 <!-- v:ctest:test_symbol_absent_msInternalPanic -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/core/test_common_utils.c`）

```c
#include "ms_test.h"
#include "mslang/ms_alloc.h"
#include "mslang/ms_vec.h"
#include "mslang/ms_hash.h"

static void testVecPushAndGrow(void) {
  MsVec(int) v;
  MsVecInit(&v);
  for (int i = 0; i < 1024; i++) {
    MsVecPush(&v, i * 2);
  }
  MS_ASSERT_EQ(MsVecLen(&v), 1024, "len after 1024 pushes");
  MS_ASSERT_EQ(MsVecAt(&v, 0),    0,    "v[0]");
  MS_ASSERT_EQ(MsVecAt(&v, 1023), 2046, "v[1023]");
  MsVecFree(&v);
  MS_ASSERT_EQ(MsVecLen(&v), 0, "len after free");
}

static void testFnv1a32KnownValues(void) {
  // 已知测试向量（https://fnvhash.github.io/）
  MS_ASSERT_EQ(msFnv1a32("", 0),      UINT32_C(2166136261), "FNV32 empty");
  MS_ASSERT_EQ(msFnv1a32("hello", 5), UINT32_C(0x4f9f2cab), "FNV32 hello");
  MS_ASSERT_EQ(msFnv1a32("foobar", 6),UINT32_C(0xbf9cf968), "FNV32 foobar");
}

static void testFnv1a64KnownValues(void) {
  MS_ASSERT_EQ(msFnv1a64("hello", 5),
                 UINT64_C(0xa430d84680aabd0b), "FNV64 hello");
}

static void testAllocRealloc(void) {
  int* p = MS_ALLOC_N(int, 4);
  for (int i = 0; i < 4; i++) p[i] = i;
  p = MS_REALLOC_N(p, int, 8);
  // 前 4 个元素应保留
  MS_ASSERT_EQ(p[0], 0, "realloc preserves [0]");
  MS_ASSERT_EQ(p[3], 3, "realloc preserves [3]");
  msFree(p);
}

int main(void) {
  MS_RUN(testVecPushAndGrow);
  MS_RUN(testFnv1a32KnownValues);
  MS_RUN(testFnv1a64KnownValues);
  MS_RUN(testAllocRealloc);
  return msTestSummary();
}
```

---

## .ms 使用示例

N/A（内部工具模块，不暴露给脚本层）。

---

## Benchmark

```c
// benchmarks/core/bench_fnv.c
// 目标：FNV-1a 32 位在现代 CPU 上应 ≥ 1 GB/s 吞吐
// NOTE: 本 benchmark 依赖 POSIX clock_gettime，仅限 Linux/macOS；win32/MSVC 需改用 QueryPerformanceCounter
#include <time.h>
#include <stdio.h>
#include "mslang/ms_hash.h"

int main(void) {
  static const char data[1024] = {0};
  const int N = 1000000;
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  volatile uint32_t h = 0;
  for (int i = 0; i < N; i++) h ^= msFnv1a32(data, sizeof(data));
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
  printf("FNV32: %.0f MB/s (h=%u)\n",
           (double)(N * sizeof(data)) / secs / 1e6, h);
}
```

**参考指标**：>500 MB/s（现代桌面 CPU，`-O2`）。

---

## 风险与边界

- **`MsVec` 宏与 C++ 的冲突**：本项目纯 C17，不混 C++；如需 C++ 测试 harness，需重新评估。
- **`MsVecGrow_` 的 `void**` 参数**：GCC/Clang 允许通过 `void**` 修改任意指针类型（严格别名规则例外），但 MSVC `/W4` 可能警告；添加 `#pragma` 或用 `memcpy` 方案规避。
- **FNV-1a 哈希分布**：对于 map/set 哈希桶，FNV-1a 32 位在键分布均匀时表现良好；若出现严重碰撞可后续换 SipHash-1-3（不影响接口）。
- **未覆盖**：线程安全（多 Worker 分配）留待 T050（GC 分配器）和 T115（TLAB）；本任务只需单线程正确。
