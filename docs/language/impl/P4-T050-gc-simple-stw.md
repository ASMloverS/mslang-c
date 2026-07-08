# P4-T050 简易 GC：单线程 STW 标记-清除 + 基础分配器

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现最简可运行的垃圾回收器：**单线程 Stop-the-World（STW）标记-清除**。这是 GC 演进路线的第一步（基线版本），供 P4 VM 核心运行。后续 P10 阶段渐进演进为三代并发 GC，但代码结构上已预留分代位与写屏障占位。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T049 | `MsObject`/`MsType` 完整定义（`gcFlags`/`traverse`/`destroy`/`objSize`/`varSize`） |
| P0-T002 | 内存分配封装（`msAlloc`/`msRealloc`/`msFree`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `gc.md` | §2 内存空间布局（`gcFlags` 分代位编码，本任务仅使用 bit0 标记位） |
| `gc.md` | §8 精确根枚举（VM 栈帧 / 全局 / C API 根，本任务仅实现全局根数组一项） |
| `P4-T049` | §3 `MsGcFlags` 位布局、`MsObject`/`MsType` 权威字段定义 |
| `type-system.md` | §2 MsObject 堆对象头 / §3 MsType 类型描述符 |

> 说明：本任务是 `gc.md` 三代并发 GC 之外的**简化 STW 基线**（单线程标记-清除），
> 供 P4 里程碑（M1）尽早跑通端到端；P10 阶段将替换为 `gc.md` 描述的年轻代 bump+Cheney 复制、
> 分代写屏障、增量并发标记等完整机制。

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
src/gc/ms_gc.c           # GC 实现（标记 / 清除 / 分配）
include/mslang/ms_gc.h   # 公共 GC API
```

---

## 实现要点

### 1. GC 状态结构

`MsObject`（T049）不含 `gcNext` 字段，本任务不侵入修改其定义，改用外部侧表
（`MsVec`，见 P0-T002）追踪所有存活对象：

```c
typedef struct MsGC {
  MsVec(MsObject*) allObjects;  // 侧表：所有存活对象指针（非侵入，无需 MsObject.gcNext）
  MsVec(MsValue)   roots;       // 全局 GC 根数组（msGCPushRoot/msGCPopRoot 操作）
  size_t           bytesAlloc;  // 当前已分配字节数
  size_t           nextGC;      // 触发 GC 的阈值（初始 1MB，基线自选值，非 gc.md §11 参数）
  uint32_t         numCollects; // 已执行 GC 次数（统计）
  uint32_t         numObjects;  // 当前存活对象数
} MsGC;

// 全局 GC 状态（单例）
extern MsGC gGC;
```

### 2. 对象分配

```c
// 分配一个 size 字节的堆对象，并将其加入 GC 侧表
MsObject* msGCAlloc(MsType* type, size_t size);

// 实现
MsObject* msGCAlloc(MsType* type, size_t size) {
  // 阈值触发 GC
  if (gGC.bytesAlloc + size > gGC.nextGC) {
    msGCCollect();
  }

  // T002 封装的 malloc（OOM→abort，见 P0-T002，故此处无需 NULL 检查）。
  // 注意：与 gc.md §3.1 最终版年轻代 bump 分配器 msAlloc(vm, type, size) 同名不同签名，
  // 属基线期临时复用；P10 演进为真正的 GC 分配器时需重命名/合并二者（见「风险与边界」）。
  MsObject* obj = msAlloc(size);
  memset(obj, 0, size);
  obj->type    = type;
  obj->gcFlags = 0;  // 未标记（MS_GC_MARK 位为 0）
  MsVecPush(&gGC.allObjects, obj);

  gGC.bytesAlloc += size;
  gGC.numObjects++;
  return obj;
}
```

### 3. 标记阶段

复用 T049 已定义的 `MS_GC_MARK`（bit0），标记/清除全程只操作该位，不触碰
`MS_GC_GEN_MASK`（bit1-2 分代位）与 `MS_GC_FORWARDED`（bit3），避免破坏分代编码：

```c
static void markObject(MsObject* obj);

static void markValue(MsValue v) {
  if (MS_IS_OBJ(v)) markObject(MS_AS_OBJ(v));
}

// GC 访问者回调：由 type->traverse 对对象内每个持有堆引用的 MsValue 槽调用一次
static void markVisit(MsValue* slot, void* ctx) {
  (void)ctx;
  markValue(*slot);
}

// 标记单个对象及其所有可达子对象（DFS，递归，简单实现）
static void markObject(MsObject* obj) {
  if (!obj || (obj->gcFlags & MS_GC_MARK)) return;
  obj->gcFlags |= MS_GC_MARK;
  // 调用类型的 traverse 回调，递归标记子对象
  if (obj->type && obj->type->traverse) {
    obj->type->traverse(obj, markVisit, NULL);
  }
}

// 从根集合开始标记：初版仅遍历全局根数组（gGC.roots）；
// VM 栈帧根枚举在 T051（MsVM/MsFrame）完成后补充
static void markRoots(void) {
  for (uint32_t i = 0; i < MsVecLen(&gGC.roots); i++) {
    markValue(MsVecAt(&gGC.roots, i));
  }
}
```

### 4. 清除阶段

```c
static void sweep(void) {
  uint32_t writeIdx = 0;
  for (uint32_t i = 0; i < MsVecLen(&gGC.allObjects); i++) {
    MsObject* obj = MsVecAt(&gGC.allObjects, i);
    if (obj->gcFlags & MS_GC_MARK) {
      // 存活：清除标记位，保留在侧表中
      obj->gcFlags &= ~(uint32_t)MS_GC_MARK;
      MsVecAt(&gGC.allObjects, writeIdx++) = obj;
    } else {
      // 垃圾：调用 destroy 析构，释放内存，不写回侧表（即从表中移除）
      size_t size = (obj->type && obj->type->varSize) ? obj->type->varSize(obj)
                    : obj->type ? obj->type->objSize
                                : sizeof(MsObject);
      if (obj->type && obj->type->destroy) {
        obj->type->destroy(obj);
      }
      gGC.bytesAlloc -= size;
      gGC.numObjects--;
      msFree(obj);
    }
  }
  gGC.allObjects.len = writeIdx;
}
```

### 5. 触发收集

```c
void msGCCollect(void) {
  markRoots();         // 标记根（初版：全局根数组）
  sweep();             // 清除垃圾
  gGC.numCollects++;
  // 动态调整阈值（两倍已分配量，上限 64MB；基线自选值，非 gc.md §11 参数）
  gGC.nextGC = (gGC.bytesAlloc * 2 < 64 * 1024 * 1024)
                 ? gGC.bytesAlloc * 2
                 : 64 * 1024 * 1024;
}
```

### 6. 公共 API

```c
// 初始化 GC（在 VM 初始化时调用）
void msGCInit(void);

// 关闭 GC（释放所有剩余对象，在 VM 销毁时调用）
void msGCShutdown(void);

// 手动触发一次完整收集
void msGCCollect(void);

// 分配堆对象（供各内置类型使用）
MsObject* msGCAlloc(MsType* type, size_t size);

// 将一个值注册为 GC 根（临时保护，防止被回收）
// T126 中用完整 handle/root API 替换，初版用 gGC.roots（MsVec）实现
void msGCPushRoot(MsValue v);   // MsVecPush(&gGC.roots, v)
void msGCPopRoot(void);         // gGC.roots.len--（弹出最近一个根）
```

---

## 验收标准（checklist）

- [ ] `msGCAlloc` 正确分配并将对象加入 `allObjects` 侧表。
- [ ] 无根引用的对象在 `msGCCollect()` 后被释放（`destroy` 被调用）。
- [ ] `msGCPushRoot` 注册的对象在 GC 后仍然存活，`msGCPopRoot` 后可被正常回收。
- [ ] `gGC.bytesAlloc` 在分配时增加、在 sweep 后减少，保持一致。
- [ ] `msGCShutdown()` 释放所有剩余对象，无内存泄漏（valgrind 无报告）。
- [ ] 阈值触发：分配量超过 `nextGC` 时自动调用 `msGCCollect()`。

---

## 测试用例（C 单测）

### `tests/gc/test_gc_basic.c`

```c
#include "ms_test.h"
#include "mslang/ms_gc.h"
#include "mslang/ms_value.h"

// 带计数器的 destroy stub
static int gFreeCount = 0;
static void stubDestroy(MsObject* obj) { (void)obj; gFreeCount++; }
static MsType stubType = {
  .name = "stub", .objSize = sizeof(MsObject), .destroy = stubDestroy
};

static void testAllocFree(void) {
  msGCInit();
  MsObject* o = msGCAlloc(&stubType, sizeof(*o));
  MS_ASSERT_TRUE(o != NULL,           "alloc ok");
  MS_ASSERT_TRUE(gGC.numObjects == 1, "1 object");
  // 无根引用 → GC 后被回收
  msGCCollect();
  MS_ASSERT_TRUE(gFreeCount == 1,     "free called");
  MS_ASSERT_TRUE(gGC.numObjects == 0, "0 objects");
  msGCShutdown();
}

static void testRootKeepsAlive(void) {
  msGCInit();
  gFreeCount = 0;
  MsObject* o = msGCAlloc(&stubType, sizeof(*o));
  msGCPushRoot(MS_OBJ_VAL(o));
  // 有根引用 → GC 后仍然存活
  msGCCollect();
  MS_ASSERT_TRUE(gFreeCount == 0,     "not freed while rooted");
  MS_ASSERT_TRUE(gGC.numObjects == 1, "still 1 object");
  msGCPopRoot();
  // 弹出根后 → GC 后被回收
  msGCCollect();
  MS_ASSERT_TRUE(gFreeCount == 1,     "freed after root popped");
  msGCShutdown();
}

int main(void) {
  MS_RUN(testAllocFree);
  MS_RUN(testRootKeepsAlive);
  return msTestSummary();
}
```

---

## Benchmark

N/A（GC 吞吐量 bench 在 P10-T125 完成后提供，需要生代 GC 对比基线才有意义）。

---

## 风险与边界

- **栈帧根枚举**：`markRoots` 初版仅遍历 `gGC.roots`（`msGCPushRoot`/`msGCPopRoot` 注册的全局根）；
  VM 栈上活跃 `MsValue` 的枚举需等 T051（`MsVM`/`MsFrame`）完成后补充，VM 尚未接入前不影响本任务验收。
- **递归标记栈溢出**：深嵌套数据结构（如深链表）可能导致 C 栈溢出；P10 演进时改为迭代（灰色对象队列）。初版接受此限制（文档注明深度限制约 10000）。
- **`destroy` vs GC free**：`destroy` 用于析构（如关闭文件句柄、释放非 GC 堆内存），不负责 `msFree(obj)` 本身（GC sweep 负责 `msFree`）。
- **线程安全**：初版无锁（单线程）；P9/P10 演进时添加 GC 锁与安全点。
- **`msAlloc` 命名冲突**：本任务复用 T002 的 `msAlloc(size)`（OOM→abort）做底层分配；
  `gc.md §3.1` 最终版年轻代 bump 分配器同名但签名为 `msAlloc(vm, type, size)`。
  P10 演进到真正的分代分配器时，二者需重命名或合并，避免符号冲突。
