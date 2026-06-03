# subprocess — 子进程管理

```ms
import subprocess
```

## 概述

提供创建和管理子进程的接口，风格参考 Python subprocess 模块。
高层 API（`run`、`check_output`、`check_call`）覆盖绝大多数使用场景；
`Popen` 提供完整的底层控制，适合需要流式处理子进程 I/O 或异步交互的场景。

## 常量与类型

| 名称 | 类型 | 说明 |
|---|---|---|
| `subprocess.PIPE` | `int` | 重定向至管道（用于 `stdin`/`stdout`/`stderr`） |
| `subprocess.DEVNULL` | `int` | 重定向至空设备（丢弃输出） |
| `subprocess.STDOUT` | `int` | 将 `stderr` 合并到 `stdout`（仅用于 `stderr` 参数） |

**CompletedProcess** — `run()` 返回值

| 属性 | 类型 | 说明 |
|---|---|---|
| `.args` | `list[str]\|str` | 传入的命令参数 |
| `.returncode` | `int` | 进程退出码 |
| `.stdout` | `bytes\|str\|nil` | 捕获的标准输出（未捕获时为 `nil`） |
| `.stderr` | `bytes\|str\|nil` | 捕获的标准错误（未捕获时为 `nil`） |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `run` | `run(args, *, ...) → CompletedProcess` | 运行命令并等待完成 |
| `check_output` | `check_output(args, *, ...) → bytes` | 运行命令并返回标准输出 |
| `check_call` | `check_call(args, **kw) → int` | 运行命令并检查返回码 |

**Popen 方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `communicate` | `.communicate(input=nil, timeout=nil) → (stdout, stderr)` | 发送输入并等待完成 |
| `wait` | `.wait(timeout=nil) → int` | 等待进程结束，返回退出码 |
| `poll` | `.poll() → int\|nil` | 非阻塞检查进程是否结束 |
| `send_signal` | `.send_signal(sig)` | 向进程发送信号 |
| `terminate` | `.terminate()` | 发送 SIGTERM |
| `kill` | `.kill()` | 发送 SIGKILL（POSIX）或 TerminateProcess（Windows） |

## 详细语义

### subprocess.run

```
subprocess.run(
    args,
    *,
    stdin=nil,
    input=nil,
    capture_output=false,
    stdout=nil,
    stderr=nil,
    cwd=nil,
    env=nil,
    timeout=nil,
    check=false,
    shell=false,
    encoding=nil,
) → CompletedProcess
```

运行 `args` 指定的命令，阻塞直至子进程完成，返回 `CompletedProcess`。

**关键参数说明：**

- `args`：命令及参数，通常为字符串列表（如 `["ls", "-l"]`）。
  `shell=true` 时可传单个字符串，由 shell 解析。
- `input`：`bytes` 或 `str`，作为子进程的标准输入内容。
  不可与 `stdin` 同时使用；指定 `input` 时自动将 `stdin` 设为 `PIPE`。
- `capture_output=true`：等效于 `stdout=PIPE, stderr=PIPE`，捕获输出到 `.stdout` 和 `.stderr`。
- `check=true`：若退出码非零，抛 `CalledProcessError`。
- `timeout`：超时秒数（浮点），超时后抛 `TimeoutExpired` 并终止子进程。
- `encoding`：非 `nil` 时，`.stdout`/`.stderr` 以该编码解码为 `str`；
  否则为 `bytes`。
- `cwd`：子进程工作目录，默认继承父进程。
- `env`：子进程环境变量映射，`nil` 时继承父进程环境。
- `shell=true`：通过系统 shell 执行，支持管道、重定向等 shell 特性，
  但存在注入风险，避免对不可信输入使用。

---

### subprocess.check_output

```
subprocess.check_output(args, *, stderr=nil, **kw) → bytes
```

运行命令并返回标准输出内容。若退出码非零，抛 `CalledProcessError`，
异常对象的 `.output` 属性包含已捕获的输出。

等效于 `subprocess.run(args, stdout=PIPE, check=true, **kw).stdout`。

---

### subprocess.check_call

```
subprocess.check_call(args, **kw) → int
```

运行命令并等待完成。退出码为零时返回 `0`；非零时抛 `CalledProcessError`。

---

### subprocess.Popen

```
subprocess.Popen(
    args,
    stdin=nil,
    stdout=nil,
    stderr=nil,
    cwd=nil,
    env=nil,
    shell=false,
    encoding=nil,
)
```

低层子进程对象，构造后立即启动子进程。

**属性：**

| 属性 | 说明 |
|---|---|
| `.pid` | 子进程 PID |
| `.returncode` | 进程退出码（`nil` 表示尚未结束） |
| `.stdin` | 子进程标准输入流（仅 `stdin=PIPE` 时非 `nil`） |
| `.stdout` | 子进程标准输出流（仅 `stdout=PIPE` 时非 `nil`） |
| `.stderr` | 子进程标准错误流（仅 `stderr=PIPE` 时非 `nil`） |

**communicate vs wait：**

- `communicate(input, timeout)` — 一次性发送 `input` 并读取全部 `stdout`/`stderr`，
  等待进程结束。适合输出量不超过内存的场景，避免死锁。
- `wait(timeout)` — 仅等待进程退出，不读取 I/O。若子进程 `stdout=PIPE` 而不读取，
  当管道缓冲区满时会发生死锁，应优先使用 `communicate`。

`Popen` 支持 `with` 语句；退出时若进程未结束，自动调用 `terminate()`，
然后等待进程退出以回收资源。

---

### 异常详情

**CalledProcessError**

```
subprocess.CalledProcessError(returncode, cmd, output=nil, stderr=nil)
```

当 `check=true` 且退出码非零时抛出。属性：`.returncode`、`.cmd`、`.output`、`.stderr`。

**TimeoutExpired**

```
subprocess.TimeoutExpired(cmd, timeout, output=nil, stderr=nil)
```

超时时抛出，`run` 会先调用 `kill()` 终止子进程，再抛此异常。

## 示例

```ms
import subprocess

// 1. 运行命令，检查退出码
result := subprocess.run(["echo", "hello"], capture_output=true, encoding="utf-8")
fmt.println(result.stdout)  // "hello\n"

// 2. check=true：失败自动抛异常
try {
    subprocess.run(["false"], check=true)
} except subprocess.CalledProcessError as e {
    fmt.println($"命令失败，退出码：{e.returncode}")
}

// 3. check_output：获取命令输出
out := subprocess.check_output(["git", "rev-parse", "HEAD"], encoding="utf-8")
fmt.println($"当前 commit: {out.strip()}")

// 4. 传入标准输入
res := subprocess.run(
    ["cat"],
    input=bytes("标准输入内容\n"),
    capture_output=true,
)
fmt.println(res.stdout)

// 5. Popen 流式处理
proc := subprocess.Popen(
    ["ls", "-la"],
    stdout=subprocess.PIPE,
    encoding="utf-8",
)
stdout, _ := proc.communicate()
fmt.println(stdout)

// 6. Popen with 语句，自动清理
with subprocess.Popen(["sleep", "10"]) as proc {
    fmt.println($"子进程 PID：{proc.pid}")
    // 退出 with 时自动 terminate
}

// 7. 超时控制
try {
    subprocess.run(["sleep", "60"], timeout=2.0)
} except subprocess.TimeoutExpired {
    fmt.println("命令超时")
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `subprocess.SubprocessError` | 所有子进程异常的基类 |
| `subprocess.CalledProcessError` | `check=true` 时退出码非零 |
| `subprocess.TimeoutExpired` | 超过 `timeout` 指定的秒数 |
| `FileNotFoundError` | 可执行文件不存在 |
| `PermissionError` | 无权执行目标程序 |
| `OSError` | 其他进程创建错误 |
| `ValueError` | `input` 与 `stdin` 同时指定；`STDOUT` 用于 `stdout` 参数等非法组合 |
