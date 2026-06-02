# mslang 异常系统

## 1. 异常层次结构

```
BaseException
├── SystemExit               // sys.exit() 触发，不被普通 catch 捕获
├── KeyboardInterrupt        // Ctrl+C
└── Exception
    ├── RuntimeError         // 一般运行时错误
    ├── TypeError            // 类型不符
    ├── ValueError           // 值不合法（类型正确但值错误）
    ├── AttributeError       // 属性不存在
    ├── NameError            // 变量名未定义
    ├── IndexError           // 下标越界
    ├── KeyError             // map 键不存在
    ├── ZeroDivisionError    // 除零
    ├── OverflowError        // 整数转换越界 / 显式溢出（如 int() 越界、math.factorial 超限等）；浮点常规运算溢出产生 inf，不抛此异常
    ├── IOError              // IO 操作失败
    ├── OSError              // 操作系统错误（IOError 别名）
    ├── FileNotFoundError    // 文件不存在（IOError 子类）
    ├── PermissionError      // 权限不足
    ├── TimeoutError         // 超时（并发场景）
    ├── StopIteration        // 迭代结束（迭代器协议内部使用）
    ├── NotImplementedError  // 未实现的方法
    ├── AssertionError       // assert 失败
    ├── ImportError          // 模块导入失败
    ├── RecursionError       // 调用栈溢出
    └── PanicError           // 内部不可恢复错误（msPanic）
```

内置异常均为 `class`，继承自对应父类，可被用户子类化：

```ms
class MyError extends RuntimeError {
    func __init__(self, msg, code) {
        super().__init__(msg)
        self.code = code
    }
}
```

---

## 2. 异常对象结构

```c
struct MsException {
  struct MsObject head;      // type 指向异常类描述符
  struct MsStr*   message;   // 错误消息（__str__ 返回）
  struct MsMap*   attrs;     // 实例属性（如 .code, .filename 等）
  struct MsObject* traceback; // MsTraceback（回溯信息）
};

struct MsTraceback {
  struct MsObject head;
  struct MsList*  frames;    // list of (filename, lineno, funcname) tuple
};
```

---

### BaseException.__init__

`BaseException.__init__(self, message="")` 由 C 层提供：将 `message` 写入 `MsException.message` 字段。子类在 `__init__` 中调用 `super().__init__(msg)` 即通过此 C 函数初始化 `self.message`；无需在脚本层重新赋值。

```ms
class MyError extends RuntimeError {
    func __init__(self, msg, code) {
        super().__init__(msg)   // 写入 MsException.message（C 层）
        self.code = code
    }
}
```

---

## 3. try/catch/finally 语义

```ms
try {
    riskyOp()
} catch (e: TypeError) {
    // 捕获 TypeError 及其子类
    print("type error:", e)
} catch (e: ValueError, KeyError) {
    // 捕获多种类型（逗号分隔）
    print("val or key error:", e)
} catch (e) {
    // 捕获所有 Exception（不含 SystemExit 等）
    print("other:", e)
} finally {
    // 无论是否抛出，始终执行
    cleanup()
}
```

规则：
- 多个 `catch` 从上到下按顺序匹配，**第一个匹配的执行**，其余跳过。
- 类型检查使用 `isinstance(e, ExcClass)`。
- `finally` 在 `catch` 之后执行，即使 `catch` 内又抛出异常（此时 `finally` 先执行，再传播新异常）。
- `catch (e: SomeClass)` 中 `e` 的作用域仅限 catch 块内（离开后不可访问）。

---

## 4. raise

```ms
raise ValueError("invalid input")   // 抛出新异常
raise                                // 在 catch 块内重新抛出当前异常
raise MyError("oops", 42)
```

- `raise expr`：`expr` 必须是 `BaseException` 的实例或子类（调用其 `__init__` 的方式也可：`raise ValueError` 等价于 `raise ValueError()`）。
- 裸 `raise`：重新抛出（reraise）`catch` 捕获到的当前异常，保留原始回溯。

---

## 5. VM 异常传播机制

### 5.1 异常处理器栈

每个 `MsThread` 维护一个异常处理器栈 `exceptStack`：

```c
struct ExceptEntry {
  uint8_t*            handlerIp;   // catch 块字节码入口
  uint32_t            stackDepth;  // 进入 try 时的操作数栈深度
  struct ExceptEntry* prev;
};
```

`PUSH_EXCEPT_HANDLER offset` 将新 entry 压入 `exceptStack`，`POP_EXCEPT_HANDLER` 弹出。

### 5.2 异常触发

C 层函数（内置、C 扩展）通过以下方式触发异常：

```c
msRaiseException(MsVM* vm, struct MsObject* exc);
msRaiseString(MsVM* vm, struct MsType* excType, const char* msg);
```

设置 `thread->exception = msObjValue(exc)`（将 `struct MsObject*` 包装为 `MsValue`），返回特殊哨兵值 `MS_ERROR_VALUE`（`{tag=MS_TAG_ERROR}`，`MS_TAG_ERROR` 定义见 type-system.md §1.2）给求值循环。

### 5.3 异常传播流程

```
求值循环检测到 MS_ERROR_VALUE 返回值
  │
  ▼
1. 是否有 exceptStack 条目？
   │  是：恢复操作数栈到 entry.stackDepth
   │       将当前异常对象压栈（供 LOAD_EXCEPTION 使用）
   │       跳转到 entry.handlerIp
   │  否：栈展开（弹出当前帧，继续外层帧）
   │       继续检查 exceptStack（在调用方帧）
   │       直到：
   │         - 找到处理器 → 同上处理
   │         - 帧链耗尽 → 未捕获异常
   ▼
2. 未捕获异常处理：
   - 打印回溯（Traceback）
   - goroutine 标记为 DEAD（异常结果存入 MsCoroutine.exception）
   - 若是主 goroutine，进程以非零退出码退出
   - 若是 go 启动的 goroutine，异常传播给 await 等待者（或被忽略若无人 await）
```

### 5.4 catch 类型匹配

```
LOAD_EXCEPTION       // 压入异常对象 e
CONST <ExcType>      // 压入目标类型
ISINSTANCE           // isinstance(e, ExcType)
JMP_IF_FALSE <next_catch_or_reraise>
```

### 5.5 finally 实现

`finally` 块在**所有可能路径**上执行（正常、异常、`return`、`break`、`continue`）。编译器在以下情况前插入 `finally` 块的内联拷贝：
- 正常离开 try 块
- `return` 语句（先执行 finally 再 return）
- `break`/`continue` 语句

若 finally 块本身抛出异常，新异常替换原异常传播。

---

## 6. 回溯打印

未捕获异常时输出格式（类 Python）：

```
Traceback (most recent call last):
  File "script.ms", line 12, in main
    result = divide(10, 0)
  File "script.ms", line 5, in divide
    return a / b
ZeroDivisionError: division by zero
```

`MsTraceback` 在 `CALL` 指令时自动记录每帧的 `(filename, lineno, funcname)`。

---

## 7. assert

```ms
assert condition
assert condition, "message"
```

条件为假时抛出 `AssertionError("message")`。编译为：

```
<evaluate condition>
JMP_IF_TRUE <skip>
CONST "message"  // 或 CONST_NIL
RAISE_ASSERT
<skip>:
```

---

## 8. 用户自定义异常最佳实践

```ms
class AppError extends Exception {
    func __init__(self, msg, code=0) {
        super().__init__(msg)
        self.code = code
    }
    func __str__(self) {
        return "AppError[" + str(self.code) + "]: " + super().__str__()
    }
}

try {
    raise AppError("something failed", 404)
} catch (e: AppError) {
    print(e.code, e)  // 404  AppError[404]: something failed
}
```
