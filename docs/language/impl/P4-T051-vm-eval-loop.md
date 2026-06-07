# P4-T051 MsFrame / MsThread + 求值循环（eval loop）

> **状态**：⬜ 未开始

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
| `vm.md` | §1 MsFrame / MsThread 结构 |
| `vm.md` | §2 求值循环主循环（dispatch loop） |
| `vm.md` | §3 调用帧栈布局 |

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
  int         slotCount;   // 本帧分配的槽数（0 = locals + upvalues）
  MsObject*   closure;     // 当前函数闭包对象（MsClosure*），用于 upvalue 访问
  struct MsFrame* caller;  // 调用者帧（链表）
} MsFrame;
```

### 2. MsThread（线程状态）

```c
#define MS_STACK_MAX   (1024 * 256)   // 最大栈深度（256K 个 MsValue）

typedef struct MsThread {
  MsValue   stack[MS_STACK_MAX];   // 值栈（静态分配，简化版）
  MsValue*  sp;                    // 栈顶指针（指向下一个空槽）
  MsFrame*  frame;                 // 当前帧（调用帧链的顶端）
  MsValue   globals;               // 全局命名空间（MsMap*）
  // 异常状态
  uint32_t  exceptSP;              // 异常时恢复的栈深度
  bool      hasException;
  MsValue   currentException;      // 当前传播中的异常
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
#define READ_U16()    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

static MsValue eval(MsThread* t) {
  MsFrame* frame = t->frame;

#define DISPATCH() goto dispatch
dispatch:;
  uint8_t op = READ_BYTE();
  switch (op) {

  case OP_CONST: {
    uint16_t idx = READ_U16();
    PUSH(frame->chunk->consts[idx]);
    DISPATCH();
  }
  case OP_NIL:   PUSH(MS_NIL_VAL);           DISPATCH();
  case OP_TRUE:  PUSH(MS_BOOL_VAL(true));     DISPATCH();
  case OP_FALSE: PUSH(MS_BOOL_VAL(false));    DISPATCH();
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
    t->sp    = frame->slots - 1;  // 弹出帧（含 callee slot）
    t->frame = frame->caller;
    if (!t->frame) return result;  // 顶层返回
    PUSH(result);
    frame = t->frame;
    DISPATCH();
  }

  default:
    fprintf(stderr, "unknown opcode: %02X\n", op);
    return MS_ERROR_VALUE;
  }
}
```

### 5. 全局变量访问

```c
case OP_GET_GLOBAL: {
  uint16_t nameIdx = READ_U16();
  MsValue name = frame->chunk->consts[nameIdx];
  MsValue val  = msMapGet(t->globals, name);
  if (MS_IS_NIL(val)) {
    // 全局未定义 → NameError（T080 前使用 MS_ERROR_VALUE）
    return msRaiseNameError(t, name);
  }
  PUSH(val);
  DISPATCH();
}
case OP_SET_GLOBAL: {
  uint16_t nameIdx = READ_U16();
  MsValue name = frame->chunk->consts[nameIdx];
  msMapSet(t->globals, name, PEEK(0));
  DISPATCH();
}
```

---

## 验收标准（checklist）

- [ ] `msVMInit()` 初始化线程栈指针、globals 为空 map、gc 为初始状态。
- [ ] 顶层 chunk 执行：`OP_CONST + OP_RETURN` 正确返回常量值。
- [ ] `OP_POP`/`OP_DUP` 正确操作栈顶。
- [ ] `OP_GET_LOCAL(0)` 返回帧的第 0 槽。
- [ ] `OP_SET_GLOBAL` + `OP_GET_GLOBAL` 读写全局变量。
- [ ] `OP_RETURN` 正确恢复调用者帧并将返回值压栈。
- [ ] 顶层返回时 `msVMRun` 返回对应 `MsValue`。

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
- **globals 为 MsMap**：T060 实现 MsMap 后才能真正使用；T051 中 globals 先用 `MsValue globals = MS_NIL_VAL` 占位，`OP_GET_GLOBAL` 暂为 stub（总返回 NIL）。
