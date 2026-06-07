# P12-T137 stdlib: os（进程 / spawn / fork）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `os` 模块的进程管理 API：`fork`（POSIX）、`exec*` 族、`waitpid`、`kill`、`pipe`。为 `subprocess` 模块（T170）提供底层支持。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T136 | os 基础 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-os-process.md` | §1 模块 API |

---

## API 清单

```ms
// 进程（POSIX，对齐 stdlib/os.md）
os.fork()                     // → int  子进程 pid / 0（子进程中）
os.execv(path, args)          // 替换当前进程镜像
os.execve(path, args, env)    // 带环境变量
os.waitpid(pid, options=0)    // → (pid, status)
os.kill(pid, sig)             // 发送信号
os.pipe()                     // → (read_fd, write_fd)
os.dup(fd)                    // → int  复制 fd
os.dup2(fd, fd2)              // 复制 fd 到 fd2
os.fdopen(fd, mode="r")       // fd → file 对象

// 退出
os.exit(code)                 // _exit()（不调用 atexit）

// Windows 特有
os.startfile(path)            // 关联打开文件
```

---

## 实现要点

```c
// POSIX: 使用 fork() + execve()
// Windows: 无 fork()，文档说明；subprocess(T170) 用 CreateProcess

// os.pipe() 返回 (readFd, writeFd)，可通过 os.fdopen 转为 file 对象

// os.waitpid status 解码：
// WIFEXITED(status) → 正常退出，WEXITSTATUS(status) → 退出码
// WIFSIGNALED(status) → 信号终止，WTERMSIG(status) → 信号号
```

---

## 验收标准（checklist）

- [ ] POSIX: `os.fork()` + `os.execv()` 可创建子进程。
- [ ] `os.pipe()` 返回 (readFd, writeFd)。
- [ ] `os.kill(pid, 0)` 检查进程是否存在（无实际信号）。
- [ ] `os.waitpid(pid)` 等待子进程退出并返回状态。

---

## 测试用例（.ms）

```ms
import os, sys

// pipe 通信
r, w := os.pipe()
pid := os.fork()
if pid == 0 {
    // 子进程
    os.close(r)
    f := os.fdopen(w, "w")
    f.write("hello from child")
    f.close()
    os.exit(0)
} else {
    // 父进程
    os.close(w)
    f := os.fdopen(r, "r")
    msg := f.read()
    f.close()
    print(msg)   // hello from child
    os.waitpid(pid)
}
```

---

## 风险与边界

- **Windows 无 `fork`**：`os.fork()` 在 Windows 抛 `NotImplementedError`；Windows 进程管理通过 `subprocess` 模块（T170）。
