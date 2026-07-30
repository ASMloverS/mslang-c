# mslang 字节码与虚拟机

## 1. 总体架构

```
源码 (.ms)
   │  词法分析 (lexer)
   ▼
Token 流
   │  递归下降 + Pratt 解析 (parser)
   ▼
AST
   │  单遍编译 (compiler)
   ▼
MsChunk (字节码 + 常量池 + 行号表)
   │  求值循环 (vm eval loop)
   ▼
执行结果
```

VM 为**基于栈的虚拟机**，每条指令从操作数栈弹入操作数、将结果压回栈。

---

## 2. MsChunk（字节码块）

```c
struct MsChunk {
  uint8_t*         code;        // 字节码字节流
  uint32_t         codeLen;
  uint32_t         codeCap;

  MsValue*         constants;   // 常量池（字符串、函数、类字面量等）
  uint32_t         constLen;
  uint32_t         constCap;

  uint32_t*        lines;       // lines[i] = code[i] 所在源码行（用于调试/回溯）
  uint32_t         linesLen;

  struct MsStr*    sourceName;  // 文件名（调试用）
};
```

常量池中存放：字符串字面量、编译期已知的函数对象、类描述符、浮点数（int64 直接内联在指令中或常量池均可）。

---

## 3. 指令集

指令宽度：**1 字节操作码 + 可变操作数**。大多数指令跟 1 或 3 字节操作数。

约定：
- `A` = 单字节操作数（0~255）
- `AX` = 3 字节操作数（无符号 0~16777215，大端序）；跳转指令（`JMP`/`JMP_IF_*`/`FOR_ITER`）中 AX 以**有符号 24 位补码**解读（-8388608~8388607），表示相对**下一条指令**的字节偏移，正值前跳，负值回跳
- `s[0]` = 栈顶，`s[1]` = 栈顶下一个，依此类推

### 3.0 栈操作

| 操作码 | 语义 |
|---|---|
| `POP` | 弹出并丢弃栈顶 |
| `DUP` | 复制栈顶（压入 s[0] 的副本） |
| `ROT2` | 交换 s[0] 与 s[1] |

使用场景：
- `POP`：`ExprStmt`（表达式语句）末尾丢弃结果；`finally` 内联路径清理临时值。
- `DUP`：三目表达式保留中间值；`finally` 多路径内联拷贝时复制异常对象；链式比较保留操作数。
- `ROT2`：多赋值与参数重排。

### 3.1 常量与字面量加载

| 操作码 | 操作数 | 语义 |
|---|---|---|
| `CONST` | AX: 常量池索引 | `push(constants[AX])` |
| `CONST_INT` | AX: 内联 int24（扩展有符号） | push 小整数（优化路径） |
| `CONST_TRUE` | — | `push(true)` |
| `CONST_FALSE` | — | `push(false)` |
| `CONST_NIL` | — | `push(nil)` |

### 3.2 变量操作

| 操作码 | 操作数 | 语义 |
|---|---|---|
| `LOAD_LOCAL` | A: 槽号 | `push(frame.locals[A])` |
| `STORE_LOCAL` | A: 槽号 | `frame.locals[A] = s[0]`（不弹出；赋值语句的求值结果由编译器单独 emit `POP` 清理，见 impl P3-T040） |
| `LOAD_GLOBAL` | AX: 名字常量索引 | 从全局表查找 |
| `STORE_GLOBAL` | AX: 名字常量索引 | 写入全局表 |
| `LOAD_UPVALUE` | A: upvalue 索引 | 从闭包 upvalue 数组读取 |
| `STORE_UPVALUE` | A: upvalue 索引 | 写入闭包 upvalue |
| `CLOSE_UPVALUE` | 无 | 关闭并弹出栈顶槽对应的 open upvalue（提升到堆），栈效应 -1（P5-T071 对本节的修正：字节码不携带操作数，语义固定作用于 `t->sp - 1`） |

### 3.3 算术与位运算

| 操作码 | 语义（弹出 s[1] op s[0]，压入结果） |
|---|---|
| `ADD` | + |
| `SUB` | - |
| `MUL` | * |
| `DIV` | / （int/int→int 整除；有 float→float） |
| `MOD` | % |
| `POW` | ** |
| `BAND` | & |
| `BOR` | \| |
| `BXOR` | ^ |
| `SHL` | << |
| `SHR` | >> （算术右移） |
| `NEG` | 一元 - |
| `BNOT` | ~ |

### 3.4 比较与逻辑

| 操作码 | 语义 |
|---|---|
| `EQ` | `==`（调 `__eq__`） |
| `NE` | `!=` |
| `LT` | `<` |
| `LE` | `<=` |
| `GT` | `>` |
| `GE` | `>=` |
| `IN` | `x in y`（调 `__contains__`） |
| `IS` | 对象同一性（指针比较） |
| `ISINSTANCE` | `isinstance(s[1], s[0])` → bool（含继承；用于 catch 类型匹配，见 errors.md §5.4） |
| `NOT` | 逻辑非（`__bool__`） |
| `AND_JMP` | A: offset — 短路 and：栈顶为假则跳（保留栈顶值） |
| `OR_JMP` | A: offset — 短路 or：栈顶为真则跳 |

### 3.5 跳转

| 操作码 | 操作数 | 语义 |
|---|---|---|
| `JMP` | AX: 有符号 24 位偏移 | 无条件相对跳转，ip += AX（正数前跳，负数回跳） |
| `JMP_IF_FALSE` | AX: 有符号 24 位偏移 | 弹出，为假则跳 |
| `JMP_IF_TRUE` | AX: 有符号 24 位偏移 | 弹出，为真则跳 |
| `JMP_IF_FALSE_PEEK` | AX: 有符号 24 位偏移 | 不弹出，为假则跳（用于短路） |

### 3.6 函数调用与返回

| 操作码 | 操作数 | 语义 |
|---|---|---|
| `CALL` | A: argc | `s[argc]` 为同步函数，`s[0..argc-1]` 为参数（逆序），新建帧并立即执行 |
| `CALL_ASYNC` | A: argc | `s[argc]` 为 `async func`，创建 `MsCoroutine`（状态 `CORO_RUNNABLE`）+ 返回 `MsFuture` 压栈；函数体不立即执行，由调度器派发 |
| `CALL_EX` | — | 展开最后参数 list（`f(...args)`） |
| `CALL_KW` | A: argc | 栈顶为 map 类型的 kwargs，`s[1..argc]` 为位置参数；若 `hasKwarg=1` 则装配为末尾 map 参数传入（见 `type-system.md §2.12`） |
| `RETURN` | — | 弹出返回值，销毁当前帧，返回调用方 |
| `RETURN_NIL` | — | 等价 `CONST_NIL; RETURN` |

### 3.7 属性与下标

| 操作码 | 操作数 | 语义 |
|---|---|---|
| `LOAD_ATTR` | AX: 名字常量索引 | `s[0] = s[0].name` |
| `STORE_ATTR` | AX: 名字常量索引 | `obj=pop(); val=pop(); obj.name=val` |
| `DEL_ATTR` | AX: 名字常量索引 | |
| `LOAD_ITEM` | — | `s[1][s[0]]`（调 `__getitem__`） |
| `STORE_ITEM` | — | `s[2][s[1]] = s[0]`（调 `__setitem__`） |
| `DEL_ITEM` | — | |

### 3.8 容器构建

| 操作码 | 操作数 | 语义 |
|---|---|---|
| `BUILD_LIST` | A: n | 弹出 n 个元素（栈底先入）构建 list |
| `BUILD_MAP` | A: n | 弹出 2n 个（key,val 交替）构建 map |
| `BUILD_TUPLE` | A: n | 构建 tuple |
| `BUILD_SLICE` | A: flags | 构建切片对象（start/stop/step） |

### 3.9 闭包与类

| 操作码 | 操作数 | 语义 |
|---|---|---|
| `MAKE_CLOSURE` | AX: 函数常量索引, A: upvalue数 | 创建闭包，upvalue 描述符跟在指令后 |
| `MAKE_CLASS` | AX: 类常量索引 | 创建类对象（从栈上弹出基类，若有） |
| `LOAD_SUPER` | — | 压入 super proxy |

### 3.10 迭代

| 操作码 | 操作数 | 语义 |
|---|---|---|
| `GET_ITER` | — | `s[0] = s[0].__iter__()` |
| `FOR_ITER` | AX: 跳出 offset | 调 `s[0].__next__()`；`StopIteration` 则跳出 |

### 3.11 异常处理

| 操作码 | 操作数 | 语义 |
|---|---|---|
| `PUSH_EXCEPT_HANDLER` | AX: catch 块 offset | 在当前帧注册异常处理器（压入处理器栈） |
| `POP_EXCEPT_HANDLER` | — | 弹出处理器（正常离开 try 块） |
| `RAISE` | A: 0=无参,1=有参 | 抛出异常（见 errors.md） |
| `RAISE_ASSERT` | A: 0=无消息,1=有消息 | A=1 时弹出消息，抛出 `AssertionError`（用于 assert 语句，见 errors.md §7） |
| `RERAISE` | — | 在 catch 块内重新抛出当前异常 |
| `LOAD_EXCEPTION` | — | 压入当前异常对象 |
| `CLEAR_EXCEPTION` | — | 清除当前异常 |

### 3.12 并发

| 操作码 | 语义 |
|---|---|
| `GO` | 弹出函数与参数，派发到调度器新建 goroutine |
| `CHAN_SEND` | `s[1] <- s[0]`，当前协程挂起直到接收方就绪 |
| `CHAN_RECV` | `push(<-s[0])`，挂起直到有值 |
| `CHAN_CLOSE` | 关闭 channel |
| `AWAIT` | 弹出 awaitable，挂起当前协程直到完成，压入结果 |
| `MAKE_CHAN` | A: 0=无缓冲,1=有缓冲 | 从栈弹容量创建 channel |
| `SELECT_BEGIN` | AX: case 数 | 开始 select 块 |
| `SELECT_CASE_SEND` / `SELECT_CASE_RECV` | — | 注册 select case |
| `SELECT_END` | — | 阻塞等待任一 case 就绪，跳到对应分支 |

---

## 4. 调用帧（MsFrame）

```c
struct MsFrame {
  struct MsFrame*    caller;    // 链式调用栈
  struct MsChunk*    chunk;     // 当前函数的字节码块
  uint8_t*           ip;        // 指令指针
  MsValue*           slots;     // 局部变量槽起始地址（指向 MsThread 共享操作数栈中的槽位）
  uint32_t           slotCount; // 本帧分配的槽数 = locals 数 + upvalue 数
  struct MsObject*   closure;   // 当前函数闭包对象（MsClosure*），用于 upvalue 访问
};
```

操作数栈**不**内联在帧中，而是每个 goroutine 拥有一个共享的定长值栈，帧仅通过 `slots` 指入其中：

```c
#define MS_STACK_MAX (1024 * 256)   // 最大栈深度（256K 个 MsValue）

struct MsThread {
  MsValue             stack[MS_STACK_MAX]; // 操作数栈（线程共享，静态分配）
  MsValue*            sp;          // 栈顶指针（指向下一个空槽），PUSH/POP 均操作此指针
  struct MsFrame*     topFrame;    // 当前帧
  MsValue             exception;   // 当前传播中的异常（MS_NIL 表示无异常，GC 按栈槽追踪）
  struct ExceptEntry* exceptStack; // 异常处理器栈
  // 调度器字段（scheduler.c 使用）
  struct MsCoroutine* coro;
};
```

---

## 5. 闭包与 Upvalue

闭包捕获外层局部变量的**引用**，通过 `MsUpvalue` 间接访问：

```c
struct MsUpvalue {
  struct MsObject head;
  MsValue*        location;  // 指向 open 时：栈槽；closed 后：&closedVal
  MsValue         closedVal; // 变量离开作用域后复制到此
};
```

**Open upvalue**：函数仍在栈上时，`location` 直接指向栈槽，多个闭包共享同一 `struct MsUpvalue`。
**Closed upvalue**：函数返回时（`CLOSE_UPVALUE` 指令），值复制到 `closedVal`，`location` 重定向到 `&closedVal`。

---

## 6. 求值循环

```c
// 伪代码，实际用 computed-goto（GCC label-as-values）加速分发
void msRun(MsVM* vm, struct MsThread* thread) {
  struct MsFrame* frame = thread->topFrame;
  uint8_t*        ip    = frame->ip;
  for (;;) {
    uint8_t op = *ip++;
    switch (op) {
      case OP_CONST: { ... break; }
      case OP_ADD: {
        MsValue b = POP(); MsValue a = POP();  // POP()/PUSH() 操作 thread->sp（线程共享栈）
        PUSH(msAdd(vm, a, b));
        break;
      }
      // ...
      case OP_CALL: {
        uint8_t argc = *ip++;
        MsValue fn   = *(thread->sp - argc - 1);
        msCall(vm, thread, fn, argc);  // 新建帧，更新 ip/frame
        frame = thread->topFrame;
        ip    = frame->ip;
        break;
      }
    }
    // 安全点检查（GC / 调度器抢占）
    if (vm->safepointPending) { msEnterSafepoint(vm, thread); }
  }
}
```

**computed-goto 优化**（GCC/Clang）：将 `switch` 替换为 `goto *dispatch_table[op]`，消除分支预测失效的开销，提升约 15~30% 吞吐。

---

## 7. 安全点（Safepoint）

GC 与调度器需要暂停协程时，通过设置 `vm->safepointPending` 标志，
VM 在每次循环回边（`FOR_ITER`/`JMP` 回跳）和每次 `CALL` 后检查并协作进入暂停。

这避免了在任意指令中断 VM 带来的复杂 GC 根扫描问题——暂停点是精确已知的。

---

## 8. 反汇编器

`disasm` 是 `execution.md §2` 定义的 CLI 子命令之一，接受 `.ms` 或 `.msc`
文件作为输入，用于调试与测试（`mslang disasm script.ms`）：

```
== <function "add"> ==
0000  LOAD_LOCAL   0       ; a
0003  LOAD_LOCAL   1       ; b
0006  ADD
0007  RETURN
```

格式：`<偏移4位> <操作码名> <操作数> ; <注释>`

---

## 9. 实现层 opcode 命名映射

`MsOpCode` 枚举（`include/mslang/ms_opcode.h`）使用 `OP_` 前缀，与本规范中的助记名存在系统性对应。以下为完整映射表；跳转指令操作数均为 **3 字节有符号 AX**（见 §3 约定），实现层不得改为 2 字节 uint16。

| 规范助记名（vm.md §3） | 实现层 `MsOpCode` 枚举值 |
|---|---|
| `CONST` | `OP_CONST` |
| `CONST_INT` | `OP_CONST_INT` |
| `CONST_TRUE` / `CONST_FALSE` / `CONST_NIL` | `OP_CONST_TRUE` / `OP_CONST_FALSE` / `OP_CONST_NIL` |
| `LOAD_LOCAL` | `OP_GET_LOCAL` |
| `STORE_LOCAL` | `OP_SET_LOCAL` |
| `LOAD_GLOBAL` | `OP_GET_GLOBAL` |
| `STORE_GLOBAL` | `OP_SET_GLOBAL` |
| `LOAD_UPVALUE` | `OP_GET_UPVALUE` |
| `STORE_UPVALUE` | `OP_SET_UPVALUE` |
| `CLOSE_UPVALUE` | `OP_CLOSE_UPVALUE` |
| `POP` | `OP_POP` |
| `DUP` | `OP_DUP` |
| `ROT2` | `OP_ROT2` |
| `ADD` / `SUB` / `MUL` / `DIV` / `MOD` / `POW` | `OP_ADD` / `OP_SUB` / `OP_MUL` / `OP_DIV` / `OP_MOD` / `OP_POW` |
| `BAND` / `BOR` / `BXOR` / `SHL` / `SHR` | `OP_BAND` / `OP_BOR` / `OP_BXOR` / `OP_SHL` / `OP_SHR` |
| `NEG` / `BNOT` | `OP_NEG` / `OP_BNOT` |
| `EQ` / `NE` / `LT` / `LE` / `GT` / `GE` | `OP_EQ` / `OP_NE` / `OP_LT` / `OP_LE` / `OP_GT` / `OP_GE` |
| `IN` / `IS` / `ISINSTANCE` / `NOT` | `OP_IN` / `OP_IS` / `OP_ISINSTANCE` / `OP_NOT` |
| `AND_JMP` / `OR_JMP` | `OP_AND_JMP` / `OP_OR_JMP` |
| `JMP` | `OP_JUMP` |
| `JMP_IF_FALSE` | `OP_JUMP_IF_FALSE` |
| `JMP_IF_TRUE` | `OP_JUMP_IF_TRUE` |
| `JMP_IF_FALSE_PEEK` | `OP_JUMP_IF_FALSE_PEEK` |
| `CALL` / `CALL_ASYNC` / `CALL_EX` / `CALL_KW` | `OP_CALL` / `OP_CALL_ASYNC` / `OP_CALL_EX` / `OP_CALL_KW` |
| `RETURN` / `RETURN_NIL` | `OP_RETURN` / `OP_RETURN_NIL` |
| `LOAD_ATTR` / `STORE_ATTR` / `DEL_ATTR` | `OP_GET_ATTR` / `OP_SET_ATTR` / `OP_DEL_ATTR` |
| `LOAD_ITEM` / `STORE_ITEM` / `DEL_ITEM` | `OP_GET_ITEM` / `OP_SET_ITEM` / `OP_DEL_ITEM` |
| `BUILD_LIST` / `BUILD_MAP` / `BUILD_TUPLE` / `BUILD_SLICE` | `OP_BUILD_LIST` / `OP_BUILD_MAP` / `OP_BUILD_TUPLE` / `OP_BUILD_SLICE` |
| `BUILD_SET` | `OP_BUILD_SET` |
| `MAKE_CLOSURE` | `OP_MAKE_FUNC` |
| `MAKE_CLASS` | `OP_MAKE_CLASS` |
| `LOAD_SUPER` | `OP_LOAD_SUPER` |
| `GET_ITER` / `FOR_ITER` | `OP_GET_ITER` / `OP_FOR_ITER` |
| `PUSH_EXCEPT_HANDLER` / `POP_EXCEPT_HANDLER` | `OP_PUSH_EXCEPT` / `OP_POP_EXCEPT` |
| `RAISE` / `RAISE_ASSERT` / `RERAISE` | `OP_RAISE` / `OP_RAISE_ASSERT` / `OP_RERAISE` |
| `LOAD_EXCEPTION` / `CLEAR_EXCEPTION` | `OP_LOAD_EXCEPTION` / `OP_CLEAR_EXCEPTION` |
| `GO` / `CHAN_SEND` / `CHAN_RECV` / `CHAN_CLOSE` | `OP_GO` / `OP_CHAN_SEND` / `OP_CHAN_RECV` / `OP_CHAN_CLOSE` |
| `AWAIT` / `MAKE_CHAN` | `OP_AWAIT` / `OP_MAKE_CHAN` |
| `SELECT_BEGIN` / `SELECT_CASE_SEND` / `SELECT_CASE_RECV` / `SELECT_END` | `OP_SELECT_BEGIN` / `OP_SELECT_CASE_SEND` / `OP_SELECT_CASE_RECV` / `OP_SELECT_END` |
| *(string build, del global, compound tests — 扩展)* | `OP_BUILD_STR` / `OP_DEL_GLOBAL` / `OP_IS_NOT` / `OP_NOT_IN` |

> **注意**：跳转操作数一律为 **3 字节有符号 24 位 AX**（见 §3 开头约定），实现层枚举中对应 `OP_JUMP*` 系列指令的操作数编码必须遵循此约定，不得缩减为 2 字节 uint16。
