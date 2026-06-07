# P12-T175 stdlib: signal / shutil / tempfile

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现三个小型系统工具模块：`signal`（Unix 信号处理）、`shutil`（高级文件操作）、`tempfile`（临时文件）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T135 | os.path |
| P12-T136 | os.fs（walk/urandom） |
| P12-T167 | gzip（shutil.make_archive 用） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-signal-shutil-tempfile.md` | §1 模块 API |

---

## API 清单

```ms
// signal（POSIX 信号，Windows 仅支持 SIGTERM/SIGINT/SIGABRT/SIGFPE/SIGILL/SIGSEGV）
signal.signal(signum, handler)  // handler = callable or SIG_DFL or SIG_IGN
signal.getsignal(signum) → handler
signal.raise_signal(signum)
signal.alarm(seconds) → old_time  // POSIX only（SIGALRM）
signal.pause()                 // POSIX only：等待信号
signal.SIG_DFL   signal.SIG_IGN
// 常量：SIGINT SIGTERM SIGKILL SIGQUIT SIGPIPE SIGALRM SIGCHLD SIGUSR1 SIGUSR2 ...

// shutil（高级文件/目录操作）
shutil.copy(src, dst) → str            // 复制文件+权限（dst 可为目录）
shutil.copy2(src, dst) → str           // 同 copy + 元数据（mtime等）
shutil.copyfile(src, dst) → str        // 仅复制内容
shutil.copyfileobj(fsrc, fdst, length=16384)
shutil.copytree(src, dst, symlinks=false, ignore=nil) → str  // 递归复制目录
shutil.rmtree(path, ignore_errors=false)  // 递归删除目录
shutil.move(src, dst) → str            // 移动（先 rename，失败则 copy+delete）
shutil.make_archive(base_name, format, root_dir=nil, base_dir=nil)
// format: "zip" "tar" "gztar"
shutil.unpack_archive(filename, extract_dir=nil, format=nil)
shutil.get_archive_formats() → list
shutil.disk_usage(path) → (total, used, free)
shutil.which(name) → str|nil  // 在 PATH 中查找可执行文件
shutil.chown(path, user=nil, group=nil)
shutil.ignore_patterns(*patterns)  // copytree ignore 参数

// tempfile
tempfile.mkstemp(suffix="", prefix="tmp", dir=nil, text=false) → (fd, path)
tempfile.mkdtemp(suffix="", prefix="tmp", dir=nil) → str
tempfile.NamedTemporaryFile(mode="w+b", suffix="", prefix="tmp",
                             dir=nil, delete=true) → file
tempfile.TemporaryDirectory(suffix="", prefix="tmp", dir=nil) → ctx_mgr
tempfile.TemporaryFile(mode="w+b", ...)  // 匿名临时文件
tempfile.gettempdir() → str   // 系统临时目录（/tmp 或 %TEMP%）
tempfile.tempdir           // 可设置覆盖默认临时目录
```

---

## 实现要点

```c
// signal：
// POSIX：sigaction() 注册处理器
// 注意：在 C 信号处理器中只能做 async-signal-safe 操作
// 实现：C handler 设置 atomic flag，调度器主循环检查 flag 后调用 .ms 处理器

// Windows：signal() 直接调用（支持有限信号集）

// shutil.copy：open+read+write（块 16KB 读写）
// copy2：追加 copystat()（os.utime + os.chmod）
// rmtree：os.walk bottom-up 删除文件再删目录
// move：先 os.rename，若跨设备（EXDEV）则 copy+remove

// tempfile：
// mkstemp：O_CREAT|O_EXCL 原子创建（避免竞态）
// 随机名称：secrets.token_hex(8) + suffix
// NamedTemporaryFile delete=true：close 时自动删除
// TemporaryDirectory：析构时调用 shutil.rmtree

// shutil.which：遍历 PATH 中的目录，检查 os.access(path, X_OK)
// Windows：自动追加 .exe/.cmd 扩展名

// shutil.disk_usage：statvfs() / GetDiskFreeSpaceEx()
```

---

## 验收标准（checklist）

- [ ] `signal.signal(SIGINT, handler)` 注册 Ctrl+C 处理器。
- [ ] `shutil.copy("/tmp/a.txt", "/tmp/b.txt")` 复制内容正确。
- [ ] `shutil.copytree("/tmp/src", "/tmp/dst")` 递归复制目录。
- [ ] `shutil.rmtree("/tmp/tmpdir")` 删除非空目录成功。
- [ ] `tempfile.mkstemp()` 创建唯一临时文件（多进程并发安全）。
- [ ] `TemporaryDirectory` 上下文结束后自动清理。

---

## 测试用例（.ms）

```ms
import signal, shutil, tempfile, os

// signal（SIGTERM 处理）
received := [false]
signal.signal(signal.SIGTERM, lambda sig, frame: received.__setitem__(0, true))
signal.raise_signal(signal.SIGTERM)
print(received[0])   // true

// shutil 文件操作
shutil.copy("/etc/hostname", "/tmp/hostname_copy")
print(os.path.exists("/tmp/hostname_copy"))  // true

// mkstemp
fd, path := tempfile.mkstemp(suffix=".txt")
os.write(fd, b"temp content")
os.close(fd)
print(os.path.exists(path))  // true
os.unlink(path)

// TemporaryDirectory
with tempfile.TemporaryDirectory() as tmpdir:
    print(os.path.isdir(tmpdir))  // true
    f := open(os.path.join(tmpdir, "test.txt"), "w")
    f.write("data")
    f.close()
// 上下文结束后自动删除
print(os.path.exists(tmpdir))  // false

// shutil.which
print(shutil.which("python3"))   // 如 /usr/bin/python3
print(shutil.which("nonexistent"))  // nil
```
