# P12-T136 stdlib: os（文件系统 / 环境变量）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `os` 模块的文件系统和环境变量 API：目录创建/删除/列举、文件操作、环境变量访问、进程 ID、工作目录等。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T135 | os.path |
| P11-T130 | 扩展模块 API |

---

## API 清单

```ms
// 文件系统（对齐 stdlib/os.md）
os.getcwd()                   // → str  当前工作目录
os.chdir(path)                // 切换工作目录
os.listdir(path=".")          // → list[str]  目录内容
os.makedirs(path, exist_ok=false)  // 递归创建目录
os.mkdir(path)                // 创建单级目录
os.remove(path)               // 删除文件
os.rmdir(path)                // 删除空目录
os.removedirs(path)           // 递归删除空目录
os.rename(src, dst)           // 重命名/移动
os.replace(src, dst)          // 原子替换
os.stat(path)                 // → StatResult
os.lstat(path)                // → StatResult（不跟随符号链接）
os.link(src, dst)             // 硬链接（POSIX）
os.symlink(src, dst)          // 符号链接（POSIX）
os.readlink(path)             // 读符号链接目标
os.walk(top, topdown=true)    // 目录树遍历（生成器）
os.scandir(path=".")          // 目录迭代器（含 DirEntry）

// 环境变量
os.environ        // 类 map 对象（读写环境变量）
os.getenv(key, default=nil) → str|nil
os.putenv(key, val)
os.unsetenv(key)

// 进程
os.getpid() → int
os.getppid() → int（POSIX）
os.getuid() → int（POSIX）
os.getgid() → int（POSIX）
os.cpu_count() → int
os.urandom(n) → bytes       // 密码学安全随机字节

// 文件描述符
os.open(path, flags, mode=0o666) → int
os.close(fd)
os.read(fd, n) → bytes
os.write(fd, data) → int
```

---

## 实现要点

```c
// StatResult 结构
typedef struct MsStatResult {
    MsInstanceObj base;
    int64_t  st_mode;
    int64_t  st_ino;
    int64_t  st_dev;
    int64_t  st_nlink;
    int64_t  st_uid;
    int64_t  st_gid;
    int64_t  st_size;
    double   st_atime;
    double   st_mtime;
    double   st_ctime;
} MsStatResult;

// os.walk 是生成器（惰性），返回 (dirpath, subdirs, files) 三元组
// 使用递归或显式栈实现

// os.environ：类 map 对象，访问 getenv/setenv/unsetenv
// 使用 MsType 的 tp_getitem/tp_setitem 委托到系统调用
```

---

## 验收标准（checklist）

- [ ] `os.getcwd()` 返回正确路径。
- [ ] `os.makedirs("/tmp/a/b/c", exist_ok=true)` 成功。
- [ ] `os.listdir(".")` 返回当前目录内容列表。
- [ ] `os.stat(".")` 返回正确的 StatResult。
- [ ] `os.getenv("HOME")` 返回环境变量值。
- [ ] `os.walk(".")` 生成 (dirpath, dirs, files) 元组。
- [ ] `os.urandom(16)` 返回 16 字节的随机数据。

---

## 测试用例（.ms）

```ms
import os

print(type(os.getcwd()))  // str
print(os.getpid() > 0)    // true
print(os.cpu_count() >= 1)  // true

// 目录操作
os.makedirs("/tmp/ms_test_dir/sub", exist_ok=true)
print(os.path.isdir("/tmp/ms_test_dir"))  // true
os.removedirs("/tmp/ms_test_dir/sub")

// 环境变量
os.environ["MS_TEST_VAR"] = "hello"
print(os.getenv("MS_TEST_VAR"))  // hello
del os.environ["MS_TEST_VAR"]

// walk
for root, dirs, files in os.walk(".") {
    print(root, len(dirs), len(files))
    break  // 只打印第一个
}
```

---

## Benchmark

N/A（I/O 性能由 OS 决定）。

---

## 风险与边界

- **Windows `os.symlink`**：在 Windows 10 之前需要管理员权限；初版在 Windows 下若无权限则抛 `OSError`。
- **`os.walk` 的目录修改**：如果 `topdown=true`，可在回调中修改 `dirs` 列表以控制递归深度（与 Python 一致）；初版直接迭代，不支持修改。
