# P4-T051 MsFrame / MsThread + 求值循环（eval loop）

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现 VM 的调用帧（`MsFrame`）、线程状态（`MsThread`）以及主求值循环（`eval loop`）。初版使用 `switch` 分派（后续 T112 演进为 computed-goto）；每个 opcode 对应一个 `case`，形成可运行 VM 的主体骨架。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T049 | `MsValue`/`MsObject`/`MsType` |
| P4-T050 | `msGCAlloc`/`msGCCollect` |
| P3-T037 | `MsChunk`/`MsOpCode` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3 指令集（含 §3.1 常量/字面量加载、§3.2 变量操作） |
| `vm.md` | §4 调用帧（MsFrame）/ MsThread 结构 |
| `vm.md` | §6 求值循环（dispatch loop） |
| `vm.md` | §9 实现层 opcode 命名映射（`OP_` 前缀对照表，3 字节 AX 约定） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
src/vm/ms_vm.c           # MsVM 全局状态 + msVMRun
include/mslang/ms_vm.h   # MsFrame / MsThread / MsVM 声明
```

---

## 实现要点

### 1. MsFrame（调用帧）

```c
typedef struct MsFrame {
  MsChunk*    chunk;       // 当前函数的字节码块
  uint8_t*    ip;          // 指令指针（指向 chunk->code 中的当前字节）
  MsValue*    slots;       // 帧的基地址（指向栈中此帧第 0 个槽）
  uint32_t    slotCount;   // 本帧分配的槽数 = locals 数 + upvalue 数
  MsObject*   closure;     // 当前函数闭包对象（MsClosure*），用于 upvalue 访问
  struct MsFrame* caller;  // 调用者帧（链表）
} MsFrame;
```

### 2. MsThread（线程状态）

```c
#define MS_STACK_MAX   (1024 * 256)   // 最大栈深度（256K 个 MsValue）

typedef struct MsThread {
  // ⚠ 栈模型：本任务暂沿用「MsThread 共享值栈 + frame->slots 指入」方案，
  // 与 vm.md §4 的「每帧内联 MsValue stack[] 柔性数组」设计不一致，
  // 需人工决策后二选一并同步更新 vm.md（详见任务末尾"风险与边界"）。
  MsValue   stack[MS_STACK_MAX];   // 值栈（静态分配，简化版）
  MsValue*  sp;                    // 栈顶指针（指向下一个空槽）
  MsFrame*  topFrame;              // 当前帧（调用帧链的顶端，对应 vm.md §4）
  MsValue   globals;               // 全局命名空间（MsMap*）
  // 异常状态（字段对齐 vm.md §4 / errors.md §5.1）
  MsValue   exception;              // 当前传播中的异常（MS_NIL_VAL 表示无异常）
  struct ExceptEntry* exceptStack;  // 异常处理器栈（errors.md §5.1，T079–T083 前恒为 NULL）
  struct MsCoroutine*  coro;        // 所属协程（P9 并发演进前恒为 NULL，占位对齐 vm.md §4）
} MsThread;
```

### 3. VM 全局状态

```c
typedef struct MsVM {
  MsThread   mainThread;
  MsGC       gc;
  // 内置类型（T053–T066 填充）
  MsType*    intType;
  MsType*    floatType;
  MsType*    boolType;
  MsType*    nilType;
  MsType*    strType;
  MsType*    bytesType;
  MsType*    listType;
  MsType*    mapType;
  MsType*    tupleType;
  MsType*    setType;
  // ... 更多类型在后续任务填充
} MsVM;

extern MsVM gVM;

void msVMInit(void);
void msVMShutdown(void);
MsValue msVMRun(MsChunk* chunk);          // 顶层执行
MsValue msVMRunFile(const char* path);    // run 子命令入口
```

### 4. 求值循环骨架

```c
// 栈操作辅助宏
#define PUSH(v)  (*t->sp++ = (v))
#define POP()    (*--t->sp)
#define PEEK(n)  (*(t->sp - 1 - (n)))
#define POKE(n,v) (*(t->sp - 1 - (n)) = (v))

// 读取操作数
#define READ_BYTE()   (*frame->ip++)
// AX：3 字节大端序操作数（见 vm.md §3 约定），不得缩减为 2 字节 uint16
#define READ_AX()     (frame->ip += 3, \
                        ((uint32_t) frame->ip[-3] << 16) | ((uint32_t) frame->ip[-2] << 8) | frame->ip[-1])

static MsValue eval(MsThread* t) {
  MsFrame* frame = t->topFrame;

#define DISPATCH() goto dispatch
dispatch:;
  uint8_t op = READ_BYTE();
  switch (op) {

  case OP_CONST: {
    uint32_t idx = READ_AX();
    PUSH(frame->chunk->constants[idx]);
    DISPATCH();
  }
  case OP_CONST_NIL:   PUSH(MS_NIL_VAL);           DISPATCH();
  case OP_CONST_TRUE:  PUSH(MS_BOOL_VAL(true));     DISPATCH();
  case OP_CONST_FALSE: PUSH(MS_BOOL_VAL(false));    DISPATCH();
  case OP_POP:   POP();                       DISPATCH();
  case OP_DUP:   PUSH(PEEK(0));               DISPATCH();

  case OP_GET_LOCAL: {
    uint8_t slot = READ_BYTE();
    PUSH(frame->slots[slot]);
    DISPATCH();
  }
  case OP_SET_LOCAL: {
    uint8_t slot = READ_BYTE();
    frame->slots[slot] = PEEK(0);  // 不弹出
    DISPATCH();
  }

  // ... 全部 60+ 操作码在 T052–T066 中逐步填充

  case OP_RETURN: {
    MsValue result = POP();
    // 恢复调用者帧
    t->sp       = frame->slots - 1;  // 弹出帧（含 callee slot）
    t->topFrame = frame->caller;
    if (!t->topFrame) return result;  // 顶层返回
    PUSH(result);
    frame = t->topFrame;
    DISPATCH();
  }

  default:
    fprintf(stderr, "unknown opcode: %02X\n", op);
    return MS_ERROR_VALUE;
  }
}
```

### 5. 全局变量访问（T051 内为占位 stub）

`MsMap`（`msMapGet`/`msMapSet`）由 T060 实现，`msRaiseNameError` 由 T080 实现，均晚于本任务；
T051 中 `t->globals` 恒为 `MS_NIL_VAL` 占位，`OP_GET_GLOBAL`/`OP_SET_GLOBAL` 仅消费操作数、维持栈平衡，
不做真正的表读写。真正的 map 读写行为推迟到 T060 之后的任务中实现。

```c
case OP_GET_GLOBAL: {
  (void) READ_AX();     // 名字常量索引，占位阶段暂不使用
  PUSH(MS_NIL_VAL);     // stub：真正实现见 T060 之后
  DISPATCH();
}
case OP_SET_GLOBAL: {
  (void) READ_AX();     // 名字常量索引，占位阶段暂不使用
  DISPATCH();           // stub：不弹出、不写入，真正实现见 T060 之后
}
```

---

## 验收标准（checklist）

- [x] `msVMInit()` 初始化线程栈指针、globals 占位为 `MS_NIL_VAL`、gc 为初始状态。
- [x] 顶层 chunk 执行：`OP_CONST + OP_RETURN` 正确返回常量值。（`tests/vm/test_eval_basic.c`）
- [x] `OP_POP`/`OP_DUP` 正确操作栈顶。（review 代码走查确认，未有独立断言用例）
- [x] `OP_GET_LOCAL(0)` 返回帧的第 0 槽。（review 代码走查确认，未有独立断言用例）
- [x] `OP_SET_GLOBAL` + `OP_GET_GLOBAL` 消费操作数并维持栈平衡（stub 占位，真正读写见 T060 之后）。（review 代码走查确认，未有独立断言用例）
- [x] `OP_RETURN` 正确恢复调用者帧并将返回值压栈。（顶层路径由测试覆盖；嵌套调用路径待 T068 调用约定落地后补测）
- [x] 顶层返回时 `msVMRun` 返回对应 `MsValue`。（`tests/vm/test_eval_basic.c`）

---

## 测试用例（C 单测）

### `tests/vm/test_eval_basic.c`

```c
#include "ms_test.h"
#include "mslang/ms_vm.h"
#include "mslang/ms_compiler.h"

static void testConstReturn(void) {
  MsCompileResult r = msCompile("42", 2, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "compile ok");
  msVMInit();
  MsValue result = msVMRun(r.chunk);
  MS_ASSERT_TRUE(MS_IS_INT(result),      "is int");
  MS_ASSERT_TRUE(MS_AS_INT(result) == 42,"value 42");
  msVMShutdown();
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testConstReturn);
  return msTestSummary();
}
```

---

## Benchmark

N/A（单独 opcode 微基准在 T067 整体 bench 中覆盖）。

---

## 风险与边界

- **`switch` 分派性能**：每个 opcode 一次 `switch` + 一次 `goto dispatch`，现代编译器会优化为跳转表（接近 computed-goto 性能）。正式 computed-goto 版本在 T112 作为并发演进的一部分引入。
- **静态栈大小**：`MsThread.stack[256K]` = 4MB/线程；Go 语言用可增长栈，初版使用固定大小（简单可靠）；P9 并发演进时改为动态分配。
- **globals 为 MsMap**：T060 实现 MsMap 后才能真正使用；T051 中 globals 先用 `MsValue globals = MS_NIL_VAL` 占位，`OP_GET_GLOBAL`/`OP_SET_GLOBAL` 暂为 stub（仅消费操作数、不读写、不弹出）。
- **⚠ 求值栈模型与 `vm.md §4/§6` 不一致，待人工决策**：`vm.md` 设计为「每帧内联 `MsValue stack[]` 柔性数组 + `stackTop`」，本任务当前实现为「`MsThread` 共享定长栈 `stack[MS_STACK_MAX]` + `sp`，帧仅经 `slots` 指入」。二者是根本性架构分歧，需人工确认取舍并同步更新 `vm.md §4/§6`（若维持共享栈模型）或改回每帧内联栈（若维持 `vm.md` 原设计，需先在 `MsChunk` 补充每函数最大栈深度字段以支持按需分配帧大小）。本次审核未替用户做出该决策，暂保留共享栈模型不变。
