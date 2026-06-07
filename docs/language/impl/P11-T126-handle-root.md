# P11-T126 句柄 / 根表 / 本地帧

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 C API 的**句柄机制**（Handle）：C 扩展代码持有的 `MsValue` 需要向 GC 注册为根，防止 GC 期间对象被错误回收。提供本地帧（`MsLocalFrame`）作为批量管理句柄的生命周期单元。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T050 | GC 根枚举 |
| P10-T117 | 精确根枚举（`msEnumerateHandles`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `c-api.md` | §3 句柄与根表（MsHandle / MsHandleSlot） |
| `c-api.md` | §3.3 本地根栈帧（LocalFrame） |

---

## 实现要点

### 1. MsHandle 与 MsHandleSlot

```c
// 句柄槽：GC 通过更新 ptr 字段来重定向移动后的对象
struct MsHandleSlot {
  struct MsObject* ptr;      // GC 在对象移动后更新此字段
  int32_t          refCount; // 0 = 空闲槽
};

typedef struct MsHandleSlot* MsHandle;  // C 代码持有的是槽指针

// 解引用句柄
#define MS_HANDLE_GET(h)  ((h)->ptr)
```

### 2. 本地根栈帧（LocalFrame）

```c
// 进入函数时创建本地帧（c-api.md §3.3）
void msPushLocalFrame(MsVM* vm, int initialCap);

// 离开函数时批量释放所有本地句柄
void msPopLocalFrame(MsVM* vm);

// 在当前本地帧中创建句柄（自动被 msPopLocalFrame 追踪）
MsHandle msLocalHandle(MsVM* vm, struct MsObject* obj);
MsHandle msLocalHandleV(MsVM* vm, MsValue v);  // 若 v 非 OBJ 返回 NULL
```

典型用法：
```c
MsValue myCFunc(MsVM* vm, int argc, MsValue* argv) {
  msPushLocalFrame(vm, 4);        // 最多 4 个本地句柄

  MsHandle lst = msLocalHandle(vm, MS_AS_OBJ(msNewList(vm, 0)));
  msListAppend(vm, msHandleToValue(lst), argv[0]);

  MsValue result = msHandleToValue(lst);
  msPopLocalFrame(vm);
  return result;
}
```

### 3. 根枚举（与 GC 集成）

```c
// GC 枚举所有活跃本地帧句柄（T117 调用）
void msEnumerateHandles(MsVM* vm, MsRootVisitor visit, void* data) {
  // 遍历句柄表中所有 refCount > 0 的槽
  for (uint32_t i = 0; i < vm->handleTable.count; i++) {
    struct MsHandleSlot* slot = &vm->handleTable.slots[i];
    if (slot->refCount > 0 && slot->ptr != NULL)
      visit(&slot->ptr, data);
  }
}
```

### 4. 全局持久句柄

```c
// 全局持久句柄：跨多次调用保护对象（不随 LocalFrame 释放）
MsHandle msNewHandle(MsVM* vm, struct MsObject* obj);
void     msFreeHandle(MsVM* vm, MsHandle h);
MsValue  msHandleToValue(MsHandle h);
```

---

## 验收标准（checklist）

- [ ] `msPushLocalFrame` + `msLocalHandle` 保护的对象在 GC 后仍存活。
- [ ] `msPopLocalFrame` 后，其帧内句柄不再作为 GC 根。
- [ ] `msNewHandle`（全局持久句柄）保护对象跨多次 GC 调用；`msFreeHandle` 后失效。
- [ ] GC 期间枚举所有活跃句柄（无漏枚举）。

---

## 测试用例（C 单测）

```c
// tests/test_handle.c
void testLocalFrameProtectsObject(void) {
  msPushLocalFrame(&gVM, 4);
  MsHandle h = msLocalHandle(&gVM, MS_AS_OBJ(msStr(&gVM, "hello", 5)));

  msGCCollect();  // GC 期间 h 被保护

  MS_ASSERT(strcmp(msStrData(msHandleToValue(h)), "hello") == 0);
  msPopLocalFrame(&gVM);
}
```

---

## Benchmark

N/A（句柄操作在 C 函数调用层，不在热路径）。

---

## 风险与边界

- **帧深度限制**：每个本地帧最多 16 个句柄；超出时动态分配溢出帧（开销较大）；典型 C 扩展函数不超过 8 个临时对象。
