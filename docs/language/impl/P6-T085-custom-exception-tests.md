# P6-T085 自定义异常 .ms 测试套件（P6 里程碑）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

通过完整的 `.ms` 测试套件验证 P6 异常系统（T079–T084），包括：自定义异常类、多层 catch、finally 语义、traceback 输出、with 语句异常、assert 等。此任务是 P6 阶段的**里程碑收口**：所有测试通过后，mslang 具有生产级别的异常处理能力。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P6-T079 ~ T084 | P6 所有任务 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `errors.md` | §1 异常层次 / §2 自定义异常 / §3 catch 语义 |

---

## M2.5 测试套件（`tests/ms/p6/`）

### `tests/ms/p6/custom_exception.ms`

```ms
// 自定义异常类
class AppError extends Exception {
    func __init__(self, code, msg) {
        super().__init__(msg)
        self.code = code
    }
    func __repr__(self) {
        return $"AppError({self.code}, {self.message})"
    }
}
class NetworkError extends AppError {}
class TimeoutError extends NetworkError {
    func __init__(self, url) {
        super().__init__(408, $"Timeout: {url}")
        self.url = url
    }
}

// 抛出和捕获
try {
    raise TimeoutError("https://example.com")
} catch (e: TimeoutError) {
    print(e.code)       // 408
    print(e.url)        // https://example.com
    print(e.message)    // Timeout: https://example.com
}

// isinstance 检查
e := TimeoutError("x.com")
print(isinstance(e, TimeoutError))   // true
print(isinstance(e, NetworkError))   // true
print(isinstance(e, AppError))       // true
print(isinstance(e, Exception))      // true
print(isinstance(e, TypeError))      // false
```

**期望输出**：
```
408
https://example.com
Timeout: https://example.com
true
true
true
true
false
```

### `tests/ms/p6/try_catch_finally.ms`

```ms
func danger(doRaise) {
    try {
        if doRaise {
            raise ValueError("danger!")
        }
        return "ok"
    } catch (e: ValueError) {
        print("caught:", e.message)
        return "caught"
    } finally {
        print("finally!")
    }
}

print(danger(false))   // finally!\nok
print(danger(true))    // caught: danger!\nfinally!\ncaught
```

**期望输出**：
```
finally!
ok
caught: danger!
finally!
caught
```

### `tests/ms/p6/exception_chain.ms`

```ms
try {
    try {
        x := int("abc")
    } catch (e: ValueError) {
        raise RuntimeError("parse failed")
    }
} catch (e: RuntimeError) {
    print(e.message)    // parse failed
}
```

### `tests/ms/p6/with_context_manager.ms`

```ms
class Resource {
    func __init__(self, name) { self.name = name; self.closed = false }
    func __enter__(self) { print($"open {self.name}"); return self }
    func __exit__(self, *args) {
        self.closed = true
        print($"close {self.name}")
        return false
    }
}

// 正常
with Resource("file.txt") as r {
    print($"using {r.name}")
}
print(r.closed)

// 异常
try {
    with Resource("conn") as r {
        raise IOError("broken")
    }
} catch (e: Exception) {
    print($"caught: {e.message}")
    print(r.closed)
}
```

**期望输出**：
```
open file.txt
using file.txt
close file.txt
true
open conn
close conn
caught: broken
true
```

### `tests/ms/p6/assert_usage.ms`

```ms
func divide(a, b) {
    assert b != 0, "divisor must not be zero"
    return a / b
}

print(divide(10, 2))   // 5
try {
    divide(10, 0)
} catch (e: AssertionError) {
    print(e.message)   // divisor must not be zero
}
```

### `tests/ms/p6/reraise.ms`

```ms
func log_and_reraise() {
    try {
        raise ValueError("original")
    } catch (e: ValueError) {
        print("logging:", e.message)
        raise   // 重抛
    }
}

try {
    log_and_reraise()
} catch (e: ValueError) {
    print("outer caught:", e.message)
}
```

**期望输出**：
```
logging: original
outer caught: original
```

---

## 验收标准（checklist）

- [ ] 所有 `tests/ms/p6/*.ms` golden 测试通过。
- [ ] 自定义异常类可继承、isinstance 按 MRO 匹配。
- [ ] finally 在所有路径（正常/catch/return/reraise）都执行。
- [ ] with 语句 `__exit__` 被调用（正常和异常路径）。
- [ ] `raise X from Y` 设置 `__cause__` 属性。
- [ ] traceback 格式正确（文件名/行号/函数名）。
- [ ] 未处理异常打印到 stderr 并退出 1。

---

## Benchmark

```ms
// benchmarks/bench_exception.ms（非热路径，只测试 happy path 开销）
// try/catch 无异常的开销（POP_EXCEPT 频率）
n := 1_000_000
for i in range(n) {
    try { pass } catch Exception { }
}
// 目标：与无 try/catch 的循环相差 < 10%
```

---

## 风险与边界

- **`IOError`**：`tests/ms/p6/with_context_manager.ms` 中使用 `IOError`（`OSError` 的别名）；若 T079 未注册此别名，测试可改为 `OSError`。
- **`e.__cause__`**：`raise X from Y` 设置 `__cause__` 属性；T083 才完整附加 traceback；T085 的 chain 测试只检查 `e.message`，不检查 `__cause__`（可在 T083 后补全）。
