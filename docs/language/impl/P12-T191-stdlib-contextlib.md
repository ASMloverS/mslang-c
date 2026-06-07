# P12-T191 stdlib: contextlib

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `contextlib` 模块（对齐 `stdlib/contextlib.md`）：上下文管理器工具，简化 with 语句的自定义实现。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T072 | class（__enter__/__exit__ 协议） |
| P6-T080 | 异常传播 |

---

## API 清单

```ms
// contextmanager 装饰器（将生成器函数变为上下文管理器）
@contextlib.contextmanager
func managed_resource(name) {
    print("setup", name)
    yield resource_value    // with 块在此暂停
    print("teardown", name) // 即使异常也执行（通过 finally）
}

with managed_resource("db") as res:
    // ... 使用 res ...

// 关闭上下文管理器
with contextlib.closing(socket.socket()) as sock:
    // sock 用完后自动 sock.close()

// 抑制特定异常
with contextlib.suppress(FileNotFoundError):
    os.unlink("/tmp/maybe_exists.txt")
// FileNotFoundError 被忽略，程序继续

// 重定向输出
import io
buf := io.StringIO()
with contextlib.redirect_stdout(buf):
    print("captured")
print(buf.getvalue())   // "captured\n"

// redirect_stderr
with contextlib.redirect_stderr(io.StringIO()):
    logging.warning("suppressed warning")

// nullcontext（不做任何事的上下文管理器）
with contextlib.nullcontext(enter_result="placeholder") as v:
    print(v)   // "placeholder"

// ExitStack（动态管理多个上下文管理器）
with contextlib.ExitStack() as stack:
    f1 := stack.enter_context(open("file1.txt"))
    f2 := stack.enter_context(open("file2.txt"))
    stack.callback(print, "all done")  // 注册回调
    stack.callback(lambda: cleanup())
// 退出时：回调 LIFO 顺序执行，then __exit__ LIFO

// AbstractContextManager（抽象基类）
class MyCtx(contextlib.AbstractContextManager):
    func __enter__(self) { return self }
    func __exit__(self, exc_type, exc_val, exc_tb) { return false }

// asynccontextmanager（协程版）
@contextlib.asynccontextmanager
async func amanaged() {
    yield setup_resource()
    // teardown
}

async with amanaged() as r:
    // ...
```

---

## 实现要点

```c
// contextmanager 装饰器：
// 1. 包裹生成器函数 g，返回 _GeneratorContextManager 类实例
// 2. __enter__：调用 g(args)，推进到 yield，返回 yield 值
// 3. __exit__(exc_type, exc_val, exc_tb)：
//    - 无异常：向生成器 send(nil)，期待 StopIteration
//    - 有异常：向生成器 throw(exc_type, exc_val)
//      若生成器处理了（无新异常）：返回 true（抑制）
//      若生成器重新抛出不同异常：传播新异常

typedef struct MsGenCtxMgrObj {
  MsObject header;
  MsCoroutineObj* gen;   // 内部生成器
  bool     used;
} MsGenCtxMgrObj;

// closing：__exit__ 调用 obj.close()
// suppress(*exc_types)：__exit__ 检查 exc_type in exc_types → 返回 true

// redirect_stdout/stderr：
// 临时替换 sys.stdout/sys.stderr
// 在 MsThread 中存储 current stdout/stderr

// ExitStack：
// 维护 callbacks 列表（LIFO）
// __exit__：逆序执行，收集所有异常，最后可能 reraise

// asynccontextmanager：
// 类似 contextmanager，但使用 await 而非 send
// __aenter__/__aexit__ 协议（T111 async/await）
```

---

## 验收标准（checklist）

- [ ] `@contextmanager` 装饰的生成器函数正常 enter/exit。
- [ ] 异常情况：with 块中的异常传播到 finally 之后（或被抑制）。
- [ ] `suppress(ValueError)` 抑制 ValueError，不抑制其他异常。
- [ ] `redirect_stdout` 捕获 print 输出。
- [ ] ExitStack 逆序执行所有清理回调。
- [ ] 嵌套 ExitStack 上下文：多次 `enter_context` 全部正确清理。

---

## 测试用例（.ms）

```ms
import contextlib, io

// contextmanager
@contextlib.contextmanager
func temp_list() {
    lst := []
    yield lst
    print("cleanup, final len:", len(lst))
}

with temp_list() as lst:
    lst.append(1)
    lst.append(2)
// 输出: cleanup, final len: 2

// suppress
with contextlib.suppress(ValueError):
    int("not a number")  // 不抛出，静默忽略
print("after suppress")  // 正常执行

// 异常不匹配时传播
try:
    with contextlib.suppress(ValueError):
        raise TypeError("not suppressed")
catch e as TypeError:
    print("TypeError propagated")  // 正确传播

// redirect_stdout
buf := io.StringIO()
with contextlib.redirect_stdout(buf):
    print("hello captured")
    print("another line")
print(buf.getvalue())  // "hello captured\nanother line\n"

// ExitStack
results := []
with contextlib.ExitStack() as stack:
    stack.callback(lambda: results.append("cb3"))
    stack.callback(lambda: results.append("cb2"))
    stack.callback(lambda: results.append("cb1"))
print(results)  // ["cb1","cb2","cb3"]（LIFO 顺序）
```
