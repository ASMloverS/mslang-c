# P5-T071 闭包 upvalue open/close 运行期

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

完整实现 upvalue 的运行期语义：open upvalue（指向栈上局部变量）和 close upvalue（被捕获变量离开作用域后转移到堆）。这使闭包能正确捕获并在函数返回后继续访问外层变量。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T068 | 调用约定（帧创建） |
| P4-T052 | `MsUpvalueObj`/`OP_CLOSE_UPVALUE` 骨架 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3.3 upvalue open/close 语义 |

---

## 实现要点

### 1. msCaptureUpvalue（创建或复用 open upvalue）

```c
// MsThread 维护按 location 降序排列的 open upvalue 链表
// 创建 upvalue 时，先查找是否已存在指向同一槽的 upvalue
MsUpvalueObj* msCaptureUpvalue(MsThread* t, MsValue* location) {
    MsUpvalueObj* prev = NULL;
    MsUpvalueObj* cur  = t->openUpvalues;

    // 按 location 排序（降序）查找
    while (cur && cur->location > location) {
        prev = cur;
        cur  = cur->nextOpen;
    }
    if (cur && cur->location == location) return cur;  // 复用

    // 创建新 open upvalue
    MsUpvalueObj* uv = (MsUpvalueObj*)msGCAlloc(&msUpvalueType, sizeof(MsUpvalueObj));
    uv->location   = location;
    uv->closed     = MS_NIL_VAL;
    uv->nextOpen   = cur;
    if (prev) prev->nextOpen = uv;
    else      t->openUpvalues = uv;
    return uv;
}
```

### 2. msCloseUpvalues（函数返回 / 作用域结束时关闭）

```c
// 关闭所有 location >= slot 的 open upvalue（slot 是即将失效的栈区域底部）
void msCloseUpvalues(MsThread* t, MsValue* slot) {
    while (t->openUpvalues && t->openUpvalues->location >= slot) {
        MsUpvalueObj* uv = t->openUpvalues;
        uv->closed   = *uv->location;   // 值从栈复制到堆
        uv->location = &uv->closed;     // 指针重定向到堆
        t->openUpvalues = uv->nextOpen;
        uv->nextOpen = NULL;
    }
}
```

### 3. 函数返回时关闭 upvalue

```c
case OP_RETURN: {
    MsValue result = POP();
    // 关闭此帧的所有 open upvalue
    msCloseUpvalues(t, frame->slots);
    // 恢复调用者帧
    msFreeFrame(frame);
    t->sp    = frame->slots - 1;
    t->frame = frame->caller;
    frame    = t->frame;
    if (!frame) return result;
    PUSH(result);
    DISPATCH();
}
```

### 4. GC mark for upvalue

```c
// MsUpvalueObj.tp_mark
static void upvalueMark(MsObject* obj) {
    MsUpvalueObj* uv = (MsUpvalueObj*)obj;
    if (uv->location == &uv->closed) {
        // close 状态：标记 closed 值
        if (MS_IS_OBJ(uv->closed)) markObject(MS_AS_OBJ(uv->closed));
    }
    // open 状态：location 指向栈，栈根枚举已覆盖
}
```

---

## 验收标准（checklist）

- [ ] `makeCounter()` 返回的函数每次调用递增 count（count 在堆上存活）。
- [ ] 多个闭包共享同一 upvalue：均看到最新值。
- [ ] 闭包在外层函数返回后仍可访问 upvalue。
- [ ] GC 不误回收 open upvalue（通过 openUpvalues 链作为 GC 根）。
- [ ] close upvalue 后 `closed` 字段正确（内容为捕获时的值）。
- [ ] 嵌套闭包（upvalue of upvalue）正确。

---

## 测试用例（.ms）

```ms
// 基础闭包
func makeCounter() {
    count := 0
    return func() {
        count += 1
        return count
    }
}
inc := makeCounter()
print(inc())  // 1
print(inc())  // 2
print(inc())  // 3

// 共享 upvalue
func makePair() {
    x := 0
    get := func() { return x }
    set := func(v) { x = v }
    return get, set
}
get, set := makePair()
set(42)
print(get())   // 42

// 深层嵌套
func outer() {
    x := 1
    func middle() {
        func inner() {
            return x   // upvalue of upvalue
        }
        return inner
    }
    return middle
}
print(outer()()())   // 1
```

---

## Benchmark

N/A（upvalue 成本归入 T068 call bench）。

---

## 风险与边界

- **GC 与 openUpvalues 链**：`t->openUpvalues` 链中的所有 open upvalue 必须是 GC 根（否则 GC 在标记阶段可能漏标 open upvalue 对象）。在 `markRoots` 中遍历 `t->openUpvalues` 链。
- **线程局部**：P9 多协程后，每个 `MsThread` 有独立的 `openUpvalues` 链，天然隔离。
