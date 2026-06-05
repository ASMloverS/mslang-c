# os — 操作系统接口（文件系统、环境、进程）

```ms
import os
```

## 概述

提供操作系统接口，涵盖文件系统操作、环境变量读写、进程控制，
以及路径字符串处理子模块 `os.path`。

独立的 `path` 模块已合并入 `os.path`，所有路径操作均通过 `os.path` 访问。

## 常量与类型

| 名称 | 类型 | 说明 |
|---|---|---|
| `os.args` | `list[str]` | 命令行参数列表，含脚本名（`os.args[0]`） |
| `os.env` | `map[str,str]` | 环境变量映射，可读写 |
| `os.environ` | `map[str,str]` | 同 `os.env`，别名 |
| `os.sep` | `str` | 路径分隔符（POSIX: `"/"`, Windows: `"\\"`) |
| `os.linesep` | `str` | 行分隔符（POSIX: `"\n"`, Windows: `"\r\n"`) |
| `os.devnull` | `str` | 空设备路径（POSIX: `"/dev/null"`, Windows: `"NUL"`) |
| `os.curdir` | `str` | 当前目录符号 `"."` |
| `os.pardir` | `str` | 上级目录符号 `".."` |
| `os.name` | `str` | 操作系统标识：`"posix"`、`"nt"` 或 `"java"` |

**StatResult** — `os.stat()` / `os.lstat()` 返回值

| 属性 | 类型 | 说明 |
|---|---|---|
| `size` | `int` | 文件字节数 |
| `mTime` | `float` | 最后修改时间（Unix 时间戳） |
| `aTime` | `float` | 最后访问时间 |
| `cTime` | `float` | 状态变更时间（POSIX）或创建时间（Windows） |
| `mode` | `int` | 权限位（同 `stMode`） |
| `stMode` | `int` | 完整文件模式位 |
| `stUid` | `int` | 所有者用户 ID（POSIX） |
| `stGid` | `int` | 所有者组 ID（POSIX） |
| `isDir` | `bool` | 是否目录 |
| `isFile` | `bool` | 是否普通文件 |
| `isSymlink` | `bool` | 是否符号链接 |

**DirEntry** — `os.scandir()` 迭代器元素

| 成员 | 类型 | 说明 |
|---|---|---|
| `.name` | `str` | 文件名（不含目录） |
| `.path` | `str` | 完整路径 |
| `.isDir()` | `bool` | 是否目录（跟随符号链接） |
| `.isFile()` | `bool` | 是否普通文件 |
| `.isSymlink()` | `bool` | 是否符号链接 |
| `.stat()` | `StatResult` | 获取文件状态信息 |

## 函数签名速查

**环境变量**

| 函数 | 签名 | 说明 |
|---|---|---|
| `getenv` | `getenv(key, default=nil) → str\|nil` | 读取环境变量 |
| `setenv` | `setenv(key, value)` | 设置环境变量 |
| `unsetenv` | `unsetenv(key)` | 删除环境变量 |
| `putenv` | `putenv(key, value)` | `setenv` 的别名 |

**进程控制**

| 函数 | 签名 | 说明 |
|---|---|---|
| `getpid` | `getpid() → int` | 当前进程 PID |
| `getppid` | `getppid() → int` | 父进程 PID |
| `exit` | `exit(code=0)` | 终止进程 |
| `abort` | `abort()` | 发送 SIGABRT，强制终止 |
| `exec` | `exec(path, args, env=nil)` | 替换当前进程镜像（execv 风格） |

**文件系统**

| 函数 | 签名 | 说明 |
|---|---|---|
| `getcwd` | `getcwd() → str` | 当前工作目录 |
| `chdir` | `chdir(path)` | 切换工作目录 |
| `listdir` | `listdir(path=".") → list[str]` | 列出目录下文件名（不含 `.` 和 `..`） |
| `scandir` | `scandir(path=".") → iterator[DirEntry]` | 迭代目录，返回 DirEntry 对象 |
| `stat` | `stat(path) → StatResult` | 获取文件状态（跟随符号链接） |
| `lstat` | `lstat(path) → StatResult` | 获取文件状态（不跟随符号链接） |
| `mkdir` | `mkdir(path, mode=0o755)` | 创建单级目录 |
| `makedirs` | `makedirs(path, mode=0o755, existOk=false)` | 递归创建目录 |
| `rmdir` | `rmdir(path)` | 删除空目录 |
| `removedirs` | `removedirs(path)` | 递归删除空目录链 |
| `remove` | `remove(path)` | 删除文件 |
| `unlink` | `unlink(path)` | `remove` 的别名 |
| `rename` | `rename(src, dst)` | 重命名文件或目录 |
| `renames` | `renames(old, new)` | 重命名并自动创建/删除中间目录 |
| `replace` | `replace(src, dst)` | 原子重命名（目标存在则覆盖） |
| `link` | `link(src, dst)` | 创建硬链接 |
| `symlink` | `symlink(src, dst)` | 创建符号链接 |
| `readlink` | `readlink(path) → str` | 读取符号链接目标 |
| `chmod` | `chmod(path, mode)` | 修改权限位 |
| `walk` | `walk(top, topdown=true, followlinks=false) → iterator` | 递归遍历目录树 |
| `glob` | `glob(pattern) → list[str]` | glob 模式匹配，返回路径列表 |

**os.path 子模块**

| 函数 | 签名 | 说明 |
|---|---|---|
| `join` | `os.path.join(*parts) → str` | 拼接路径分量 |
| `split` | `os.path.split(path) → (head, tail)` | 分割为目录与文件名 |
| `splitext` | `os.path.splitext(path) → (root, ext)` | 分割扩展名（含点） |
| `basename` | `os.path.basename(path) → str` | 文件名部分 |
| `dirname` | `os.path.dirname(path) → str` | 目录部分 |
| `abspath` | `os.path.abspath(path) → str` | 转为绝对路径 |
| `realpath` | `os.path.realpath(path) → str` | 解析所有符号链接后的绝对路径 |
| `expanduser` | `os.path.expanduser(path) → str` | 展开 `~` 和 `~user` |
| `expandvars` | `os.path.expandvars(path) → str` | 展开 `$VAR` 和 `${VAR}` |
| `normpath` | `os.path.normpath(path) → str` | 规范化路径（合并 `//`、解析 `..`） |
| `commonpath` | `os.path.commonpath(paths) → str` | 最长公共子路径 |
| `commonprefix` | `os.path.commonprefix(list) → str` | 字符级公共前缀 |
| `exists` | `os.path.exists(path) → bool` | 路径是否存在 |
| `isfile` | `os.path.isfile(path) → bool` | 是否普通文件 |
| `isdir` | `os.path.isdir(path) → bool` | 是否目录 |
| `isabs` | `os.path.isabs(path) → bool` | 是否绝对路径 |
| `islink` | `os.path.islink(path) → bool` | 是否符号链接 |
| `getsize` | `os.path.getsize(path) → int` | 文件字节数 |
| `getATime` | `os.path.getATime(path) → float` | 最后访问时间戳 |
| `getMTime` | `os.path.getMTime(path) → float` | 最后修改时间戳 |
| `getCTime` | `os.path.getCTime(path) → float` | 状态变更/创建时间戳 |
| `samefile` | `os.path.samefile(p1, p2) → bool` | 是否指向同一文件 |
| `relpath` | `os.path.relpath(path, start=".") → str` | 相对路径 |
| `match` | `os.path.match(pattern, path) → bool` | glob 模式匹配判断 |
| `glob` | `os.path.glob(pattern) → list[str]` | glob 模式匹配，返回路径列表 |

## 详细语义

### os.listdir / os.scandir

```
os.listdir(path=".") → list[str]
os.scandir(path=".") → iterator[DirEntry]
```

`listdir` 返回目录下所有条目的文件名字符串列表，不含 `.` 和 `..`，顺序不保证。

`scandir` 返回 DirEntry 迭代器，比 `listdir` + `os.stat` 更高效，
因为底层系统调用（如 `readdir`）通常已携带类型信息，无需额外 stat。
迭代器需用 `with` 语句或手动关闭以释放系统资源。

---

### os.walk

```
os.walk(top, topdown=true, followlinks=false) → iterator[(dirpath, dirnames, filenames)]
```

递归遍历以 `top` 为根的目录树，每次迭代产出三元组：
- `dirpath`：当前目录路径字符串
- `dirnames`：当前目录下子目录名列表（可就地修改以剪枝）
- `filenames`：当前目录下普通文件名列表

`topdown=true` 时自顶向下遍历（父目录在子目录之前产出），
`topdown=false` 时自底向上遍历。

修剪示例：在 `topdown=true` 时，对 `dirnames` 就地删除不需要进入的子目录，
`os.walk` 将跳过对应子树。

`followlinks=false` 时，符号链接指向的目录不会被递归进入。

---

### os.makedirs

```
os.makedirs(path, mode=0o755, existOk=false)
```

递归创建 `path` 所指定的完整目录层级。
`existOk=true` 时，若目录已存在不抛异常；`existOk=false` 时，
若目标目录已存在则抛 `FileExistsError`。

---

### os.replace

```
os.replace(src, dst)
```

原子地将 `src` 重命名为 `dst`。若 `dst` 已存在（且不是目录），则静默覆盖。
相比 `os.rename`，`replace` 在 POSIX 系统上是原子操作，适合安全的文件替换。

---

### os.exec

```
os.exec(path, args, env=nil)
```

用新程序替换当前进程镜像（POSIX execv 风格）。
`args` 为字符串列表，`args[0]` 通常为程序名。
`env` 为环境变量映射；若为 `nil`，继承当前进程环境。
调用成功后不返回；失败抛 `OSError`。

---

### os.path.join

```
os.path.join(*parts) → str
```

将多个路径分量拼接为单个路径字符串。若某分量为绝对路径，
则舍弃其前所有分量，以该分量重新开始拼接。空字符串分量被忽略。

```ms
os.path.join("/usr", "local", "bin")   // → "/usr/local/bin"
os.path.join("/tmp", "/etc", "hosts")  // → "/etc/hosts"（/etc 为绝对路径）
```

---

### os.path.split

```
os.path.split(path) → (head, tail)
```

将路径拆分为 `(head, tail)` 元组，其中 `tail` 是最后一个分量（无路径分隔符），
`head` 是其余部分。末尾的分隔符会被去掉后再拆分。

```ms
os.path.split("/foo/bar.txt")  // → ("/foo", "bar.txt")
os.path.split("/foo/")         // → ("/foo", "")
```

---

### os.path.normpath

```
os.path.normpath(path) → str
```

规范化路径字符串：折叠多余的 `/`，解析 `.` 和 `..` 分量，但不访问文件系统、
不解析符号链接。若需同时解析符号链接，使用 `os.path.realpath`。

---

### os.path.commonpath vs commonprefix

- `commonpath(paths)` — 返回所有路径的最长公共 **路径前缀**（路径分量级别）；
  输入空列表抛 `ValueError`；混合绝对/相对路径抛 `ValueError`。
- `commonprefix(list)` — 返回所有字符串的最长公共 **字符前缀**，不感知路径结构，
  可能返回不完整的路径分量。

---

### os.glob / os.path.glob

支持标准 glob 通配符：`*`（任意字符序列，不跨目录）、`?`（单个字符）、
`[abc]`（字符集合）。`**` 匹配任意层级目录（仅当路径含 `**/` 时生效）。

## 示例

```ms
import os

// 1. 遍历目录树，收集所有 .ms 文件
func findMsFiles(root) {
    result := []
    for dirpath, _, files in os.walk(root) {
        for f in files {
            if os.path.splitext(f)[1] == ".ms" {
                result.append(os.path.join(dirpath, f))
            }
        }
    }
    return result
}

// 2. stat 一个文件
info := os.stat("/etc/hosts")
fmt.println($"size={info.size}, mTime={info.mTime}, isFile={info.isFile}")

// 3. 构建路径
p := os.path.join(os.getcwd(), "output", "report.txt")
fmt.println(os.path.abspath(p))

// 4. 读写环境变量
home := os.getenv("HOME", "/tmp")
os.setenv("APP_DEBUG", "1")

// 5. 递归创建目录
os.makedirs("build/obj/core", existOk=true)

// 6. scandir 高效遍历（比 listdir + stat 快）
with os.scandir(".") as it {
    for entry in it {
        if entry.isFile() {
            fmt.println(entry.name, entry.stat().size)
        }
    }
}

// 7. glob 匹配
for f in os.glob("src/**/*.ms") {
    fmt.println(f)
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `FileNotFoundError` | 路径不存在（`stat`、`remove`、`chdir` 等） |
| `FileExistsError` | 目标已存在（`mkdir`、`makedirs(existOk=false)` 等） |
| `IsADirectoryError` | 对目录执行了仅适用于文件的操作 |
| `NotADirectoryError` | 路径分量不是目录 |
| `PermissionError` | 无权限执行操作 |
| `OSError` | 其他 I/O 或系统调用错误 |
| `ValueError` | `os.path.commonpath` 传入空列表或混合绝对/相对路径 |
