# P4-T052 栈操作 / 常量 / 局部变量指令

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

在 T051 eval loop 骨架中，新增尚未实现的基础指令：栈操作 `OP_ROT2`、upvalue 读写骨架（`OP_GET_UPVALUE`/`OP_SET_UPVALUE`/`OP_CLOSE_UPVALUE`）。

> **范围说明**：`OP_POP`/`OP_DUP`/`OP_CONST`/`OP_CONST_NIL`/`OP_CONST_TRUE`/`OP_CONST_FALSE`/`OP_GET_LOCAL`/`OP_SET_LOCAL` 已在 T051（`src/vm/ms_vm.c`）实现，本任务不重复。upvalue 三条指令因依赖尚未定义的 `MsClosureObj`（P5-T068 才引入），本任务仅实现操作数消费骨架（stub），真正的闭包/open-upvalue 语义推迟到 P5-T071（闭包 upvalue open/close 运行期语义）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T051 | 求值循环骨架（`PUSH`/`POP`/`PEEK`/`READ_BYTE`/`READ_AX` 宏） |
| P4-T049 | `MsValue` 定义 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3.0 栈操作 |
| `vm.md` | §3.1 常量与字面量加载（已由 T051 实现，仅供交叉核对） |
| `vm.md` | §3.2 变量操作 |
| `vm.md` | §5 闭包与 Upvalue |

---

## 待实现（C 文件 / 函数）

### 修改文件

```
src/vm/ms_vm.c   # eval() switch 中增加对应 case
```

---

## 实现要点

### 1. 栈操作指令

`OP_POP`/`OP_DUP` 已在 T051 实现（`src/vm/ms_vm.c`），本任务新增 `OP_ROT2`：

| 指令 | 操作 | 说明 |
|---|---|---|
| `OP_ROT2` | 交换栈顶两个值（s[0] 与 s[1]） | `a=POP(); b=POP(); PUSH(a); PUSH(b);` |

```c
case OP_ROT2: {
  MsValue a = POP(), b = POP();
  PUSH(a);
  PUSH(b);
  DISPATCH();
}
```

### 2. 常量指令（已在 T051 实现，不重复）

`OP_CONST`/`OP_CONST_NIL`/`OP_CONST_TRUE`/`OP_CONST_FALSE` 已在 `src/vm/ms_vm.c` 完整实现（`frame->chunk->constants[idx]` + `READ_AX()`），本任务无需改动。

### 3. 局部变量指令（已在 T051 实现，不重复）

`OP_GET_LOCAL`/`OP_SET_LOCAL` 已在 `src/vm/ms_vm.c` 完整实现，本任务无需改动。局部变量存储在 `frame->slots[0..slotCount-1]`（参数 + 其余局部变量共用该窗口）。

### 4. Upvalue 读写（T052 内为占位 stub）

Upvalue 的完整 open/close 语义（`struct MsUpvalue`，见 `vm.md §5`）依赖闭包运行时对象 `MsClosureObj`，该类型由 P5-T068（调用约定）引入、P5-T071（闭包 upvalue open/close 运行期语义）实现真正的读写与 close 逻辑，均晚于本任务。T052 中 `frame->closure` 恒为 `NULL`（T051 `msVMRun` 已如此初始化），三条 upvalue 指令仅消费操作数、维持栈平衡，不做真正的 upvalue 访问。

```c
case OP_GET_UPVALUE: {
  (void) READ_BYTE();  // upvalue 索引，占位阶段暂不使用
  PUSH(MS_NIL_VAL);    // stub：真正实现见 T071
  DISPATCH();
}
case OP_SET_UPVALUE: {
  (void) READ_BYTE();  // upvalue 索引，占位阶段暂不使用
  DISPATCH();          // stub：不弹出、不写入，真正实现见 T071
}
case OP_CLOSE_UPVALUE: {
  (void) READ_BYTE();  // 本地槽号（vm.md §3.2），占位阶段暂不使用
  DISPATCH();          // stub：不操作操作数栈，真正实现见 T071
}
```

---

## 验收标准（checklist）

- [ ] `OP_ROT2` 正确交换栈顶两个值。
- [ ] `OP_DUP`（T051 已实现）后栈顶两个值相同——仅回归确认，非本任务交付物。
- [ ] `OP_GET_LOCAL(0)`/`OP_SET_LOCAL(1)`（T051 已实现）行为不变——仅回归确认，非本任务交付物。
- [ ] `OP_GET_UPVALUE`/`OP_SET_UPVALUE`/`OP_CLOSE_UPVALUE` 正确消费各自操作数并维持栈平衡（stub 占位，真正的闭包访问见 T071）。

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

- **Upvalue 为占位 stub**：真正的 `struct MsUpvalue`（`vm.md §5`：`head`/`location`/`closedVal`）、open-upvalue 链表、`MsClosureObj` 均由 T068/T071 引入并实现；T052 仅保证三条指令的操作数解码与栈平衡正确，不做闭包语义验证。
- **`OP_SET_LOCAL` 不弹出**（T051 已实现，此处仅说明前提）：编译器在赋值语句后总 emit `OP_POP`（T040 设计），VM 不需要在 SET_LOCAL 内弹出；与 Python 不同，mslang 赋值返回 nil（不是右值）。
