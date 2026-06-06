# P4-T050 简易 GC：单线程 STW 标记-清除 + 基础分配器

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现最简可运行的垃圾回收器：**单线程 Stop-the-World（STW）标记-清除**。这是 GC 演进路线的第一步（基线版本），供 P4 VM 核心运行。后续 P10 阶段渐进演进为三代并发 GC，但代码结构上已预留分代位与写屏障占位。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T049 | `MsObject`/`MsType` 完整定义（gcFlags/gcNext/tp_mark） |
| P0-T002 | 内存分配封装（`msAlloc`/`msRealloc`/`msFree`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `gc.md` | §2 演进基线（STW 标记-清除） |
| `gc.md` | §1 GC 标记位布局 |
| `gc.md` | §3 根枚举（栈帧 / 全局 / 常量池） |

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

```c
typedef struct MsGC {
    MsObject*   allObjects;   // 所有存活对象的链表（通过 gcNext 串联）
    size_t      bytesAlloc;   // 当前已分配字节数
    size_t      nextGC;       // 触发 GC 的阈值（初始 1MB，每次 GC 后调整）
    uint32_t    numCollects;  // 已执行 GC 次数（统计）
    uint32_t    numObjects;   // 当前存活对象数
} MsGC;

// 全局 GC 状态（单例）
extern MsGC gGC;
```

### 2. 对象分配

```c
// 分配一个 size 字节的堆对象，并将其加入 GC 链表
MsObject* msGCAlloc(MsType* type, size_t size);

// 实现
MsObject* msGCAlloc(MsType* type, size_t size) {
    // 阈值触发 GC
    if (gGC.bytesAlloc + size > gGC.nextGC) {
        msGCCollect();
    }

    MsObject* obj = msAlloc(size);  // T002 封装的 malloc
    memset(obj, 0, size);
    obj->type    = type;
    obj->gcFlags = MS_GC_WHITE;
    obj->gcNext  = gGC.allObjects;
    gGC.allObjects = obj;

    gGC.bytesAlloc += size;
    gGC.numObjects++;
    return obj;
}
```

### 3. 标记阶段

```c
// 标记单个对象及其所有可达子对象（DFS，递归，简单实现）
static void markObject(MsObject* obj) {
    if (!obj || (obj->gcFlags & 0x03) == MS_GC_BLACK) return;
    obj->gcFlags = (obj->gcFlags & ~0x03) | MS_GC_BLACK;
    // 调用类型的 mark 回调，递归标记子对象
    if (obj->type && obj->type->tp_mark) {
        obj->type->tp_mark(obj);
    }
}

static void markValue(MsValue v) {
    if (MS_IS_OBJ(v)) markObject(MS_AS_OBJ(v));
}

// 从根集合（VM 栈 + 全局变量）开始标记
static void markRoots(MsVM* vm);  // T051 提供 vm 结构后实现
```

### 4. 清除阶段

```c
static void sweep(void) {
    MsObject** prev = &gGC.allObjects;
    MsObject*  cur  = gGC.allObjects;

    while (cur) {
        if ((cur->gcFlags & 0x03) == MS_GC_BLACK) {
            // 存活：重置为白色，继续
            cur->gcFlags = (cur->gcFlags & ~0x03) | MS_GC_WHITE;
            prev = &cur->gcNext;
            cur  = cur->gcNext;
        } else {
            // 垃圾：从链表移除，调用 tp_free，释放内存
            MsObject* dead = cur;
            *prev = dead->gcNext;
            cur   = dead->gcNext;
            if (dead->type && dead->type->tp_free) {
                dead->type->tp_free(dead);
            }
            gGC.bytesAlloc -= dead->type ? dead->type->instanceSize : sizeof(MsObject);
            gGC.numObjects--;
            msFree(dead);
        }
    }
}
```

### 5. 触发收集

```c
void msGCCollect(void) {
    markRoots(&gVM);     // 标记根
    sweep();             // 清除垃圾
    gGC.numCollects++;
    // 动态调整阈值（两倍已分配量，上限 64MB）
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
// T126 中用完整 handle/root API 替换，初版用简单全局根数组
void msGCPushRoot(MsValue v);
void msGCPopRoot(void);
```

---

## 验收标准（checklist）

- [ ] `msGCAlloc` 正确分配并将对象加入 `allObjects` 链表。
- [ ] 无根引用的对象在 `msGCCollect()` 后被释放（`tp_free` 被调用）。
- [ ] 有根引用的对象在 GC 后仍然存活。
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

// 带计数器的 tp_free stub
static int gFreeCount = 0;
static void stubFree(MsObject* obj) { (void)obj; gFreeCount++; }
static MsType stubType = {
    .name = "stub", .instanceSize = sizeof(MsObject), .tp_free = stubFree
};

static void testAllocFree(void) {
    msGCInit();
    MsObject* o = msGCAlloc(&stubType, sizeof(MsObject));
    MS_ASSERT_TRUE(o != NULL,           "alloc ok");
    MS_ASSERT_TRUE(gGC.numObjects == 1, "1 object");
    // 无根引用 → GC 后被回收
    msGCCollect();
    MS_ASSERT_TRUE(gFreeCount == 1,     "free called");
    MS_ASSERT_TRUE(gGC.numObjects == 0, "0 objects");
    msGCShutdown();
}

int main(void) {
    MS_RUN(testAllocFree);
    return msTestSummary();
}
```

---

## Benchmark

N/A（GC 吞吐量 bench 在 P10-T125 完成后提供，需要生代 GC 对比基线才有意义）。

---

## 风险与边界

- **栈帧根枚举**：`markRoots` 需要能遍历当前 VM 栈上所有活跃 `MsValue`；在 T051（`MsVM`/`MsFrame`）完成前，`markRoots` 为 stub（空实现），分配量低时不触发 GC，初版可接受。
- **递归标记栈溢出**：深嵌套数据结构（如深链表）可能导致 C 栈溢出；P10 演进时改为迭代（灰色对象队列）。初版接受此限制（文档注明深度限制约 10000）。
- **`tp_free` vs GC free**：`tp_free` 用于析构（如关闭文件句柄、释放非 GC 堆内存），不负责 `msFree(obj)` 本身（GC sweep 负责 `msFree`）。
- **线程安全**：初版无锁（单线程）；P9/P10 演进时添加 GC 锁与安全点。
