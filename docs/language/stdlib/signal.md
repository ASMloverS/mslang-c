# signal — 信号处理

```ms
import signal
```

## 概述

提供 POSIX 风格的进程信号处理接口：注册信号处理函数、向进程发送信号、
查询当前处理函数，以及定时器信号支持。

信号处理函数在主 goroutine 中异步调用。
长耗时操作应在处理函数内通过 channel 或 goroutine 委托，以避免阻塞信号分发。
`SIGKILL` 和 `SIGSTOP` 在所有平台上均无法被捕获或忽略。

## 常量与类型

**信号编号常量**

| 常量 | 说明 |
|---|---|
| `signal.SIGABRT` | 异常终止（通常由 `abort()` 触发） |
| `signal.SIGFPE` | 浮点/算术异常 |
| `signal.SIGILL` | 非法指令 |
| `signal.SIGINT` | 键盘中断（Ctrl+C） |
| `signal.SIGSEGV` | 段错误（非法内存访问） |
| `signal.SIGTERM` | 终止请求（可捕获，`kill` 命令默认发送） |
| `signal.SIGKILL` | 强制终止（POSIX；不可捕获） |
| `signal.SIGUSR1` | 用户自定义信号 1（POSIX） |
| `signal.SIGUSR2` | 用户自定义信号 2（POSIX） |
| `signal.SIGPIPE` | 写入已关闭管道（POSIX） |
| `signal.SIGHUP` | 终端挂起或控制进程退出（POSIX） |
| `signal.SIGCHLD` | 子进程状态变更（POSIX） |
| `signal.SIGALRM` | 定时器到期（POSIX，见 `alarm`） |

**特殊处理函数值**

| 常量 | 说明 |
|---|---|
| `signal.SIG_DFL` | 恢复默认处理行为 |
| `signal.SIG_IGN` | 忽略该信号 |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `signal` | `signal.signal(signum, handler)` | 注册信号处理函数，返回旧处理函数 |
| `getsignal` | `signal.getsignal(signum) → handler` | 查询当前处理函数 |
| `raiseSignal` | `signal.raiseSignal(signum)` | 向当前进程发送信号 |
| `kill` | `signal.kill(pid, signum)` | 向指定 PID 发送信号 |
| `alarm` | `signal.alarm(seconds) → int` | 设置 SIGALRM 定时器（POSIX） |
| `pause` | `signal.pause()` | 暂停直至收到信号（POSIX） |
| `pthreadKill` | `signal.pthreadKill(threadId, signum)` | 向指定线程发送信号（POSIX） |

## 详细语义

### signal.signal

```
signal.signal(signum, handler) → previousHandler
```

为信号 `signum` 注册处理函数 `handler`，返回替换前的旧处理函数。

`handler` 的签名：

```ms
func myHandler(signum, frame) {
    // signum: int — 收到的信号编号
    // frame: Frame — 被中断时的调用帧（可为 nil）
}
```

`handler` 也可以是 `signal.SIG_DFL`（恢复默认行为）或 `signal.SIG_IGN`（忽略）。

**限制：**
- 信号处理函数只能在主 goroutine 注册。
- `SIGKILL`、`SIGSTOP` 不可被捕获或忽略，传入这些信号会抛 `OSError`。
- Windows 平台仅支持 `SIGABRT`、`SIGFPE`、`SIGILL`、`SIGINT`、`SIGSEGV`、`SIGTERM`。

---

### signal.getsignal

```
signal.getsignal(signum) → handler
```

返回 `signum` 当前注册的处理函数。返回值可能为 `SIG_DFL`、`SIG_IGN`，
或一个用户注册的函数对象。

---

### signal.raiseSignal

```
signal.raiseSignal(signum)
```

向当前进程发送信号 `signum`，等效于 C 标准库的 `raise()`。
信号处理函数（若已注册）会在调用返回前被调用。

---

### signal.kill

```
signal.kill(pid, signum)
```

向 PID 为 `pid` 的进程发送信号 `signum`。
`pid=0` 时向当前进程组发送；`pid=-1` 时向所有进程发送（需足够权限）。
仅 POSIX 系统完全支持；Windows 下仅支持部分信号。

---

### signal.alarm

```
signal.alarm(seconds) → int
```

在 `seconds` 秒后向当前进程发送 `SIGALRM`。返回上一次定时器剩余秒数。
`seconds=0` 取消待定定时器。每个进程只有一个 alarm 定时器。
仅 POSIX 系统支持；Windows 上调用会抛 `OSError`。

---

### signal.pause

```
signal.pause()
```

挂起当前进程，直至收到任意信号。信号处理函数执行完毕后，`pause` 返回。
仅 POSIX 系统支持。

---

### 信号与 goroutine 的交互

mslang 中信号处理函数在主 goroutine 执行。若需在信号处理中触发并发操作，
推荐通过 channel 通知其他 goroutine，保持处理函数简短：

```ms
sigCh := make(chan int, 1)

signal.signal(signal.SIGTERM, func(signum, frame) {
    sigCh <- signum  // 非阻塞发送，处理函数立即返回
})

go func() {
    signum := <-sigCh
    fmt.println($"收到信号 {signum}，开始清理...")
    // 执行清理逻辑
}()
```

## 示例

```ms
import signal
import fmt

// 1. 注册 SIGINT 处理（优雅退出）
signal.signal(signal.SIGINT, func(signum, frame) {
    fmt.println("\n收到 Ctrl+C，正在退出...")
    os.exit(0)
})

// 2. 忽略 SIGPIPE（网络编程中常用）
signal.signal(signal.SIGPIPE, signal.SIG_IGN)

// 3. 查询当前处理函数
old := signal.getsignal(signal.SIGTERM)
fmt.println($"SIGTERM 当前处理：{old}")

// 4. 临时覆盖信号处理，之后还原
prev := signal.signal(signal.SIGUSR1, func(signum, frame) {
    fmt.println("收到 SIGUSR1")
})
// ... 执行某段逻辑 ...
signal.signal(signal.SIGUSR1, prev)  // 还原

// 5. alarm 定时（POSIX）
signal.signal(signal.SIGALRM, func(signum, frame) {
    fmt.println("定时到期！")
})
signal.alarm(5)  // 5 秒后触发 SIGALRM
signal.pause()   // 等待信号

// 6. 向另一个进程发送信号
childPid := 12345
signal.kill(childPid, signal.SIGTERM)
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `OSError` | 尝试捕获 `SIGKILL`/`SIGSTOP`；`kill` 目标进程不存在或无权限；`alarm`/`pause` 在 Windows 上调用 |
| `ValueError` | `signum` 不是合法信号编号 |
| `TypeError` | `handler` 不是可调用对象、`SIG_DFL` 或 `SIG_IGN` |
