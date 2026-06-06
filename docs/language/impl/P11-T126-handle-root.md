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
| `c-api.md` | §2 句柄 + 本地帧 |

---

## 实现要点

### 1. MsLocalFrame

```c
// C 扩展函数的调用帧：所有局部句柄在帧销毁时自动释放
#define MS_HANDLE_CAP 16

typedef struct MsLocalFrame {
    MsValue  slots[MS_HANDLE_CAP];  // 内嵌句柄槽（避免小帧的堆分配）
    uint32_t count;
    struct MsLocalFrame* prev;     // 链接到上层帧
} MsLocalFrame;

// 宏：在 C 函数入口声明本地帧
#define MS_LOCAL_FRAME(vm)                         \
    MsLocalFrame _lframe_ = {0};                   \
    _lframe_.prev = (vm)->handleStack;             \
    (vm)->handleStack = &_lframe_

// 宏：函数返回前弹出帧（并返回转义句柄）
#define MS_RETURN(vm, val) \
    do { \
        MsValue _ret_ = (val); \
        (vm)->handleStack = _lframe_.prev; \
        return _ret_; \
    } while (0)
```

### 2. 句柄分配

```c
// 在当前本地帧中分配一个句柄槽，返回指针
MsValue* msHandleAlloc(MsVM* vm, MsValue val) {
    MsLocalFrame* frame = vm->handleStack;
    if (!frame || frame->count >= MS_HANDLE_CAP) {
        // 帧已满：动态扩展（溢出到堆）
        frame = msAllocOverflowFrame(vm, frame);
    }
    MsValue* slot = &frame->slots[frame->count++];
    *slot = val;
    return slot;
}

// 典型用法：
// MsValue* obj = msHandleAlloc(vm, msNewList());
// ... 期间 GC 安全 ...
// MsValue result = *obj;
```

### 3. 根枚举（与 GC 集成）

```c
// GC 枚举所有本地帧句柄（T117 调用）
void msEnumerateHandles(MsRootVisitor visit, void* data) {
    MsLocalFrame* frame = gVM.handleStack;
    while (frame) {
        for (uint32_t i = 0; i < frame->count; i++) {
            if (MS_IS_OBJ(frame->slots[i]))
                visit(&frame->slots[i], data);
        }
        frame = frame->prev;
    }
}
```

### 4. 全局根（永久保护）

```c
// 全局根：用于跨调用持久保护（不随本地帧释放）
typedef struct MsGlobalHandle {
    MsValue val;
    struct MsGlobalHandle* next;
} MsGlobalHandle;

MsValue* msNewGlobalHandle(MsVM* vm, MsValue val) {
    MsGlobalHandle* h = msAlloc(sizeof(MsGlobalHandle));
    h->val = val;
    h->next = vm->globalHandles;
    vm->globalHandles = h;
    return &h->val;
}

void msFreeGlobalHandle(MsVM* vm, MsValue* handle) {
    // 从链表中移除
}
```

---

## 验收标准（checklist）

- [ ] `MS_LOCAL_FRAME` + `msHandleAlloc` 保护的对象在 GC 后仍存活。
- [ ] 本地帧弹出后，其句柄不再作为 GC 根。
- [ ] 全局句柄保护对象跨多次 GC 调用。
- [ ] GC 期间枚举所有本地帧和全局句柄（无漏枚举）。

---

## 测试用例（C 单测）

```c
// tests/test_handle.c
void test_local_frame_protects_object(void) {
    MS_LOCAL_FRAME(&gVM);
    MsValue* obj = msHandleAlloc(&gVM, msNewStr("hello", 5));

    msGCCollect();  // GC 期间 obj 被保护

    MS_ASSERT(strcmp(((MsStrObj*)MS_AS_OBJ(*obj))->data, "hello") == 0);
    MS_RETURN(&gVM, MS_NIL_VAL);
}
```

---

## Benchmark

N/A（句柄操作在 C 函数调用层，不在热路径）。

---

## 风险与边界

- **帧深度限制**：每个本地帧最多 16 个句柄；超出时动态分配溢出帧（开销较大）；典型 C 扩展函数不超过 8 个临时对象。
