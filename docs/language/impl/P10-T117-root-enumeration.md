# P10-T117 精确根枚举（栈帧 / 全局 / 句柄）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 GC 的**精确根枚举**（Exact Root Enumeration）：枚举所有 GC 根（调用栈上的 MsValue 槽位、全局变量、模块全局、GC 句柄、上值、协程等），为 Minor GC（T116）和增量 GC（T120）提供精确根集合，避免保守扫描的假正例。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T051 | MsFrame + MsThread（栈帧结构） |
| P7-T088 | 模块缓存（全局模块表） |
| P9-T107 | 协程（多协程的栈都是 GC 根） |

---

## 实现要点

### 1. 根枚举接口

```c
// 回调类型：访问一个 MsValue*（GC 可通过它更新引用）
typedef void (*MsRootVisitor)(MsValue* slot, void* data);

// 统一根枚举入口
void msEnumerateRoots(MsRootVisitor visit, void* data) {
  // 1. 所有 MsThread 的栈帧
  msEnumerateAllThreads(visit, data);

  // 2. 全局变量（主线程的 globals MsMapObj）
  visitValue(&gVM.mainThread.globals, visit, data);

  // 3. 模块缓存（所有已加载模块）
  visitValue(&gVM.moduleCache, visit, data);

  // 4. 内置类型对象（msIntType.typeObj 等）
  msEnumerateBuiltinTypes(visit, data);

  // 5. GC 临时保护根（msGCPushRoot / msGCPopRoot）
  msEnumeratePinnedRoots(visit, data);

  // 6. C API 根（T126，句柄表）
  msEnumerateHandles(visit, data);
}
```

### 2. 栈帧精确枚举

```c
// 每个 MsChunk 编译时生成"栈图"（stack map）：
// 记录每条指令执行后，哪些栈槽包含 GC 对象引用
// 初版简化：所有槽位都当成 MsValue 扫描（tagged union，tag 精确）
static void enumThread(MsThread* t, MsRootVisitor visit, void* data) {
  // 枚举每一帧的 slots[0..slotCount)
  MsFrame* f = t->frame;
  while (f) {
    for (uint16_t i = 0; i < f->slotCount; i++) {
      if (MS_IS_OBJ(f->slots[i]))
        visit(&f->slots[i], data);
    }
    // 帧的 closure
    if (MS_IS_OBJ(f->closure)) visit(&f->closure, data);
    f = f->caller;
  }

  // 枚举求值栈（sp 以下）
  for (MsValue* sp = t->stack; sp < t->sp; sp++) {
    if (MS_IS_OBJ(*sp)) visit(sp, data);
  }

  // 上值链
  MsUpvalueObj* uv = t->openUpvalues;
  while (uv) {
    if (MS_IS_OBJ(*uv->location)) visit(uv->location, data);
    uv = uv->nextOpen;
  }

  // 当前异常
  if (MS_IS_OBJ(t->currentException))
    visit(&t->currentException, data);
}
```

### 3. 协程栈枚举

```c
void msEnumerateAllThreads(MsRootVisitor visit, void* data) {
  // 主线程
  enumThread(&gVM.mainThread, visit, data);

  // 所有协程（包括挂起的）
  for each MsCoroutineObj* coro in gScheduler.allCoroutines {
    if (coro->state != CORO_DONE)
      enumThread(coro->thread, visit, data);
  }
}
```

### 4. 与 Minor GC 的集成

```c
// Minor GC 的根复制函数（作为 MsRootVisitor）：
static void copyRootRef(MsValue* slot, void* data) {
  uint8_t** freePtr = (uint8_t**)data;
  if (MS_IS_OBJ(*slot) && isInYoung(MS_AS_OBJ(*slot))) {
    MsObject* newObj = copyObj(MS_AS_OBJ(*slot), freePtr);
    *slot = MS_OBJ_VAL(newObj);  // 更新引用
  }
}

// Minor GC 时：
msEnumerateRoots(copyRootRef, &toFreePtr);
```

---

## 验收标准（checklist）

- [ ] 所有活对象经根枚举可达（无漏枚举 → 无过早回收）。
- [ ] 无假根（无已死对象被误标记为活）。
- [ ] 跨代引用（Old → Young）通过 remembered set 补充（T118）。
- [ ] 协程挂起时的栈正确枚举。
- [ ] GC 句柄（T126）被枚举。

---

## 测试用例（C 单测）

```c
// tests/test_root_enum.c
void testNoPrematureCollect(void) {
  // 创建对象，保留引用，GC 后对象应存活
  MsValue val = msNewStr("hello", 5);
  msGCPushRoot(val);
  msGCCollect();
  MS_ASSERT(!MS_IS_NIL(val));  // 未被回收
  MS_ASSERT(strcmp(((MsStrObj*)MS_AS_OBJ(val))->data, "hello") == 0);
  msGCPopRoot();
}
```

---

## Benchmark

N/A（根枚举是 GC 的一部分，在 T125 整体 benchmark）。

---

## 风险与边界

- **标量槽位**：MsValue tagged union 中，标量（INT/FLOAT/BOOL/NIL）无需 GC 枚举，只需枚举 `tag == OBJ` 的值；这是精确根的核心优势（vs 保守扫描的不确定性）。
- **返回地址等非引用数据**：eval 栈中只有 MsValue，无 C 返回地址混在其中（C 调用栈与 ms 求值栈分离），精确枚举无误判风险。
