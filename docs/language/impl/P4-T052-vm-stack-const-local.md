# P4-T052 栈操作 / 常量 / 局部变量指令

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

在 T051 eval loop 骨架中，完整实现以下基础指令集：栈操作（POP/DUP/SWAP/ROT3）、常量加载（OP_CONST/OP_NIL/OP_TRUE/OP_FALSE）、局部变量读写（GET/SET_LOCAL）、upvalue 读写（GET/SET/CLOSE_UPVALUE）。这些指令是所有后续指令的基础。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T051 | 求值循环骨架（PUSH/POP/PEEK/READ_BYTE/READ_U16 宏） |
| P4-T049 | `MsValue` 定义 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §2.1 栈操作指令 |
| `vm.md` | §2.2 常量指令 |
| `vm.md` | §2.3 局部变量指令 |
| `vm.md` | §3.3 upvalue open/close |

---

## 待实现（C 文件 / 函数）

### 修改文件

```
src/vm/ms_vm.c   # eval() switch 中增加对应 case
```

---

## 实现要点

### 1. 栈操作指令

| 指令 | 操作 | 说明 |
|---|---|---|
| `OP_POP` | 弹出栈顶 | `POP()` |
| `OP_DUP` | 复制栈顶 | `PUSH(PEEK(0))` |
| `OP_SWAP` | 交换栈顶两个值 | `t1=POP(); t2=POP(); PUSH(t1); PUSH(t2)` |
| `OP_ROT3` | 旋转栈顶三个值（a,b,c→c,a,b） | 三元组旋转 |
| `OP_POP_N` | 弹出 N 个值 `[1B: N]` | 函数调用清理 |

```c
case OP_POP:   (void)POP();                DISPATCH();
case OP_DUP:   PUSH(PEEK(0));              DISPATCH();
case OP_SWAP: {
  MsValue a = POP(), b = POP();
  PUSH(a); PUSH(b);
  DISPATCH();
}
case OP_ROT3: {
  MsValue c = POP(), b = POP(), a = POP();
  PUSH(c); PUSH(a); PUSH(b);
  DISPATCH();
}
case OP_POP_N: {
  uint8_t n = READ_BYTE();
  t->sp -= n;
  DISPATCH();
}
```

### 2. 常量指令

```c
case OP_CONST: {
  uint16_t idx = READ_U16();
  PUSH(frame->chunk->consts[idx]);
  DISPATCH();
}
case OP_NIL:   PUSH(MS_NIL_VAL);        DISPATCH();
case OP_TRUE:  PUSH(MS_BOOL_VAL(true)); DISPATCH();
case OP_FALSE: PUSH(MS_BOOL_VAL(false));DISPATCH();
```

### 3. 局部变量指令

局部变量存储在 `frame->slots[0..slotCount-1]`：
- slot 0..paramCount-1：参数
- slot paramCount..slotCount-1：局部变量

```c
case OP_GET_LOCAL: {
  uint8_t slot = READ_BYTE();
  PUSH(frame->slots[slot]);
  DISPATCH();
}
case OP_SET_LOCAL: {
  uint8_t slot = READ_BYTE();
  frame->slots[slot] = PEEK(0);   // 注意：不弹出（赋值语句后由编译器 emit OP_POP）
  DISPATCH();
}
```

### 4. Upvalue 读写

Upvalue 实现采用 **open/close** 两阶段：
- **open upvalue**：指向被捕获局部变量在栈上的槽（`MsUpvalueObj.location = &stack[slot]`）。
- **close upvalue**（`OP_CLOSE_UPVALUE`）：被捕获变量离开作用域时，将其值从栈复制到堆（`MsUpvalueObj.closed = value`，`location = &closed`）。

```c
typedef struct MsUpvalueObj {
  MsObject    header;
  MsValue*    location;   // 指向栈槽（open 时）或 &closed（close 后）
  MsValue     closed;     // close 后存放的值
  MsUpvalueObj* nextOpen; // 所有 open upvalue 链表（GC 用）
} MsUpvalueObj;

// GET_UPVALUE / SET_UPVALUE
case OP_GET_UPVALUE: {
  uint8_t idx = READ_BYTE();
  MsUpvalueObj* uv = ((MsClosureObj*)frame->closure)->upvalues[idx];
  PUSH(*uv->location);
  DISPATCH();
}
case OP_SET_UPVALUE: {
  uint8_t idx = READ_BYTE();
  MsUpvalueObj* uv = ((MsClosureObj*)frame->closure)->upvalues[idx];
  *uv->location = PEEK(0);
  DISPATCH();
}

// CLOSE_UPVALUE：将栈顶变量转移到堆（关闭 open upvalue）
case OP_CLOSE_UPVALUE:
  msCloseUpvalues(t, t->sp - 1);  // 关闭指向此槽或更高的所有 upvalue
  (void)POP();
  DISPATCH();
```

```c
// 关闭所有 location >= slot 的 open upvalue
static void msCloseUpvalues(MsThread* t, MsValue* slot) {
  while (t->openUpvalues && t->openUpvalues->location >= slot) {
    MsUpvalueObj* uv = t->openUpvalues;
    uv->closed   = *uv->location;
    uv->location = &uv->closed;
    t->openUpvalues = uv->nextOpen;
  }
}
```

---

## 验收标准（checklist）

- [ ] `OP_DUP` 后栈顶两个值相同。
- [ ] `OP_SWAP` 正确交换两个值。
- [ ] `OP_GET_LOCAL(0)` 返回 slot 0 的值。
- [ ] `OP_SET_LOCAL(1)` 写入 slot 1，不影响栈深度（除非与 POP 配合）。
- [ ] `OP_GET_UPVALUE`/`OP_SET_UPVALUE` 通过 closure 的 upvalues 数组访问。
- [ ] `OP_CLOSE_UPVALUE` 正确将 open upvalue 转移到堆，location 指针更新。
- [ ] 编译器与 VM 约定一致：`SET_LOCAL` 不弹出值（peek），`POP` 单独 emit。

---

## 测试用例（C 单测）

### `tests/vm/test_locals.c`

```c
#include "ms_test.h"
#include "mslang/ms_vm.h"
#include "mslang/ms_compiler.h"

static void testLocal(void) {
  // "x := 42\nx" 期望返回 42
  MsCompileResult r = msCompile("x := 42\nx", 9, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "compile ok");
  msVMInit();
  MsValue v = msVMRun(r.chunk);
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 42, "got 42");
  msVMShutdown();
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testLocal);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// 局部变量
x := 10
y := 20
z := x + y
print(z)   // 30

// 闭包（upvalue）
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
```

---

## Benchmark

N/A（在 T067 整体 VM bench 中涵盖）。

---

## 风险与边界

- **Upvalue 对象 GC**：`MsUpvalueObj` 本身是 GC 管理的堆对象，其 `tpMark` 回调需标记 `location`（若已 close）或忽略（若 open 时 location 指向栈上存活帧，栈根枚举已覆盖）。
- **open upvalue 链表**：`MsThread.openUpvalues` 是按 `location` 降序排列的链表，`msCloseUpvalues` 只需遍历头部。
- **`OP_SET_LOCAL` 不弹出**：编译器在赋值语句后总 emit `OP_POP`（T040 设计），VM 不需要在 SET_LOCAL 内弹出；与 Python 不同，mslang 赋值返回 nil（不是右值）。
