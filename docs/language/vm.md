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
| `STORE_LOCAL` | A: 槽号 | `frame.locals[A] = pop()` |
| `LOAD_GLOBAL` | AX: 名字常量索引 | 从全局表查找 |
| `STORE_GLOBAL` | AX: 名字常量索引 | 写入全局表 |
| `LOAD_UPVALUE` | A: upvalue 索引 | 从闭包 upvalue 数组读取 |
| `STORE_UPVALUE` | A: upvalue 索引 | 写入闭包 upvalue |
| `CLOSE_UPVALUE` | A: 本地槽号 | 将 open upvalue 关闭（提升到堆） |

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
  struct MsFunction* fn;        // 当前函数
  uint8_t*           ip;        // 指令指针
  MsValue*           slots;     // 局部变量槽（栈分配或帧池分配）
  uint32_t           slotCount;
  MsValue*           stackTop;  // 操作数栈顶
  MsValue            stack[];   // 内联操作数栈（柔性数组）
};
```

每个 goroutine 有独立帧链：

```c
struct MsThread {
  struct MsFrame*    topFrame;    // 当前帧
  MsValue            exception;   // 当前传播中的异常（MS_NIL 表示无异常，GC 按栈槽追踪）
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
  MsValue*        stack = frame->stack;
  uint8_t*        ip    = frame->ip;
  for (;;) {
    uint8_t op = *ip++;
    switch (op) {
      case OP_CONST: { ... break; }
      case OP_ADD: {
        MsValue b = POP(); MsValue a = POP();
        PUSH(msAdd(vm, a, b));
        break;
      }
      // ...
      case OP_CALL: {
        uint8_t argc = *ip++;
        MsValue fn   = stack[sp - argc - 1];
        msCall(vm, thread, fn, argc);  // 新建帧，更新 ip/frame
        frame = thread->topFrame;
        ip    = frame->ip;
        stack = frame->stack;
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
