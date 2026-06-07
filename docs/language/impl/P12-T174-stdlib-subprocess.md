# P12-T174 stdlib: subprocess

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `subprocess` 模块（对齐 `stdlib/subprocess.md`）：创建子进程、管道通信、等待结束。跨平台（POSIX fork/exec + Windows CreateProcess）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T137 | os.process（fork/exec/pipe 低层 API） |
| P12-T134 | io 模块 |

---

## API 清单

```ms
// 便捷函数
subprocess.run(args, *, stdin=nil, input=nil, stdout=nil, stderr=nil,
               capture_output=false, timeout=nil, check=false,
               cwd=nil, env=nil, text=false, encoding=nil) → CompletedProcess

// capture_output=true 等同于 stdout=PIPE, stderr=PIPE
// check=true 时，returncode != 0 抛 CalledProcessError
// text=true 时，stdout/stderr 返回 str，否则 bytes
// timeout：秒数，超时抛 TimeoutExpired

// CompletedProcess
result.args          // 执行的参数
result.returncode    // 返回码
result.stdout        // 标准输出（bytes|str|nil）
result.stderr        // 标准错误（bytes|str|nil）
result.check_returncode()  // 若非零抛 CalledProcessError

// 便捷函数（基于 run）
subprocess.call(args, **kwargs) → int           // 返回 returncode
subprocess.check_call(args, **kwargs) → int     // 非零抛异常
subprocess.check_output(args, **kwargs) → bytes // 返回 stdout

// Popen（底层：持续交互进程）
p := subprocess.Popen(args, stdin=nil, stdout=nil, stderr=nil,
                      cwd=nil, env=nil, text=false)
p.stdin   // 可写文件（若 stdin=PIPE）
p.stdout  // 可读文件（若 stdout=PIPE）
p.stderr  // 可读文件（若 stderr=PIPE）
p.pid     // 子进程 PID
p.returncode  // 结束后填充

p.communicate(input=nil, timeout=nil) → (stdout, stderr)
p.wait(timeout=nil) → returncode
p.poll() → int|nil    // 非阻塞检查
p.send_signal(sig)    // 发送信号
p.terminate()         // SIGTERM / TerminateProcess
p.kill()              // SIGKILL / TerminateProcess(force)

subprocess.PIPE    // = -1（使用管道）
subprocess.STDOUT  // = -2（stderr 重定向到 stdout）
subprocess.DEVNULL // = -3（/dev/null）

// 异常
subprocess.CalledProcessError(returncode, cmd, output=nil, stderr=nil)
subprocess.TimeoutExpired(cmd, timeout, output=nil, stderr=nil)
```

---

## 实现要点

```c
// POSIX 实现：
// 1. pipe(fd_stdin), pipe(fd_stdout), pipe(fd_stderr)
// 2. fork()
//    子进程：dup2(read_end→0, write_end→1, write_end→2) → close → execvp()
//    父进程：关闭子进程端，保留父进程端作为 p.stdin/stdout/stderr
// 3. wait4(pid, &status, WNOHANG, nil) → poll/wait

// Windows 实现：
// CreateProcess + 管道（SECURITY_ATTRIBUTES + CreatePipe + DuplicateHandle）
// WaitForSingleObject / TerminateProcess
// GetExitCodeProcess

// communicate()：
// 若有 input：写到 stdin（协程化，避免阻塞）
// 读取 stdout/stderr（协程化，可并发）
// 避免死锁：不能同时阻塞写和读，需并发或非阻塞 I/O

// timeout：使用 time.after() channel + select 实现（集成调度器）
// 超时后 kill() 子进程

typedef struct MsPopenObj {
  MsObject  header;
  pid_t     pid;          // 子进程 PID（POSIX）
  int       returncode;   // -1 = 未结束
  MsValue   stdin_file;   // MsFileObj 或 nil
  MsValue   stdout_file;
  MsValue   stderr_file;
  bool      text_mode;
} MsPopenObj;
```

---

## 验收标准（checklist）

- [ ] `subprocess.run(["echo", "hello"], capture_output=true).stdout` → `b"hello\n"`（POSIX）。
- [ ] `subprocess.check_output(["echo", "hi"])` → `b"hi\n"`。
- [ ] `check=true` 时非零退出码抛 `CalledProcessError`。
- [ ] `Popen` + `communicate()` 正确写入 stdin 并读取 stdout。
- [ ] `timeout` 超时后进程被 kill，抛 `TimeoutExpired`。
- [ ] `env={"PATH":"..."}` 可传递自定义环境变量。

---

## 测试用例（.ms）

```ms
import subprocess

// 基础调用
r := subprocess.run(["echo", "hello world"], capture_output=true, text=true)
print(r.stdout)        // "hello world\n"
print(r.returncode)    // 0

// 管道传入
r2 := subprocess.run(["cat"], input=b"pipe data", capture_output=true)
print(r2.stdout)       // b"pipe data"

// 错误检查
try:
    subprocess.check_call(["false"])  // 返回 1
catch e as subprocess.CalledProcessError:
    print("Failed:", e.returncode)  // 1

// check_output
out := subprocess.check_output(["ls", "-1", "/tmp"], text=true)
print(type(out))  // str

// Popen 交互
p := subprocess.Popen(["sort"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
stdout, _ := p.communicate(input=b"banana\napple\ncherry\n")
print(stdout)  // b"apple\nbanana\ncherry\n"
```
