# shutil — 高级文件与目录操作

```ms
import shutil
```

## 概述

基于 `os` 模块提供高层文件操作：复制（保留或丢弃元数据）、移动、
递归删除目录树、归档（压缩/解压）、磁盘用量查询以及可执行文件查找。
参考 Python shutil 模块设计。

## 常量与类型

**DiskUsage** — `shutil.diskUsage()` 返回值

| 属性 | 类型 | 说明 |
|---|---|---|
| `.total` | `int` | 磁盘总容量（字节） |
| `.used` | `int` | 已用空间（字节） |
| `.free` | `int` | 可用空间（字节） |

## 函数签名速查

**复制操作**

| 函数 | 签名 | 说明 |
|---|---|---|
| `copy` | `shutil.copy(src, dst) → str` | 复制文件内容+权限位；`dst` 可为目录 |
| `copy2` | `shutil.copy2(src, dst) → str` | 复制内容+全部元数据（时间戳等） |
| `copyfile` | `shutil.copyfile(src, dst, followSymlinks=true) → str` | 仅复制文件内容 |
| `copymode` | `shutil.copymode(src, dst)` | 仅复制权限位 |
| `copystat` | `shutil.copystat(src, dst)` | 复制权限+时间戳等元数据 |
| `copyfileobj` | `shutil.copyfileobj(fsrc, fdst, length=16384)` | 在文件对象间复制数据 |

**移动与删除**

| 函数 | 签名 | 说明 |
|---|---|---|
| `move` | `shutil.move(src, dst) → str` | 移动/重命名（跨设备感知） |
| `rmtree` | `shutil.rmtree(path, ignoreErrors=false, onerror=nil)` | 递归删除目录树 |

**目录树复制**

| 函数 | 签名 | 说明 |
|---|---|---|
| `copytree` | `shutil.copytree(src, dst, symlinks=false, ignore=nil, dirsExistOk=false) → str` | 递归复制目录树 |
| `ignorePatterns` | `shutil.ignorePatterns(*patterns) → callable` | 生成 `copytree` 的 `ignore` 参数 |

**归档操作**

| 函数 | 签名 | 说明 |
|---|---|---|
| `makeArchive` | `shutil.makeArchive(baseName, format, rootDir=nil, baseDir=nil) → str` | 创建归档文件 |
| `unpackArchive` | `shutil.unpackArchive(filename, extractDir=nil, format=nil)` | 解压归档文件 |
| `getArchiveFormats` | `shutil.getArchiveFormats() → list` | 列出支持的归档格式 |

**其他**

| 函数 | 签名 | 说明 |
|---|---|---|
| `diskUsage` | `shutil.diskUsage(path) → DiskUsage` | 查询磁盘用量 |
| `which` | `shutil.which(name, mode=nil, path=nil) → str\|nil` | 在 PATH 中查找可执行文件 |

## 详细语义

### 复制函数对比

| 函数 | 文件内容 | 权限位 | 时间戳/元数据 |
|---|---|---|---|
| `copyfile` | 是 | 否 | 否 |
| `copy` | 是 | 是 | 否 |
| `copy2` | 是 | 是 | 是 |
| `copymode` | 否 | 是 | 否 |
| `copystat` | 否 | 是 | 是 |

---

### shutil.copy / shutil.copy2

```
shutil.copy(src, dst) → str
shutil.copy2(src, dst) → str
```

将 `src` 文件复制到 `dst`。若 `dst` 是目录，则在该目录下创建同名文件。
返回最终目标文件路径。

`copy` 复制文件内容和权限位（`copyfile` + `copymode`）。
`copy2` 额外保留时间戳等元数据（`copyfile` + `copystat`），
适用于备份场景。

符号链接：`followSymlinks=true`（默认）时复制链接目标的内容；
`copyfile(followSymlinks=false)` 时创建符号链接的副本。

---

### shutil.move

```
shutil.move(src, dst) → str
```

将 `src` 移动到 `dst`。优先尝试 `os.rename`；
若源和目标不在同一文件系统（`os.rename` 失败），则先 `copy2` 后删除源。
`dst` 是目录时，移动到该目录下同名路径。返回最终目标路径。

---

### shutil.rmtree

```
shutil.rmtree(path, ignoreErrors=false, onerror=nil)
```

递归删除 `path` 目录及其所有内容（文件和子目录）。

- `ignoreErrors=true`：忽略所有错误，静默完成。
- `onerror`：错误回调 `func(func, path, exc)`；`func` 为引发错误的操作函数，
  `path` 为出错路径，`exc` 为异常信息。可在回调中尝试修复（如 `chmod` 后重试）。

删除只读文件时（Windows 常见），可用 `onerror` 先修改权限：

```ms
func handleError(fn, path, exc) {
    os.chmod(path, 0o600)
    fn(path)  // 重试删除
}
shutil.rmtree("build", onerror=handleError)
```

---

### shutil.copytree

```
shutil.copytree(src, dst, symlinks=false, ignore=nil, dirsExistOk=false) → str
```

递归复制 `src` 目录树到 `dst`。

- `symlinks=true`：保留符号链接（复制为链接而非内容）；`false` 则跟随链接复制内容。
- `ignore`：接受 `func(srcDir, names) → set[str]` 的可调用对象，
  返回要跳过的文件/目录名集合。通常由 `shutil.ignorePatterns` 生成。
- `dirsExistOk=true`：`dst` 已存在时不报错，合并目录内容；
  `false` 时 `dst` 已存在则抛 `FileExistsError`。

---

### shutil.makeArchive

```
shutil.makeArchive(baseName, format, rootDir=nil, baseDir=nil) → str
```

创建归档文件并返回其路径。

- `baseName`：归档文件路径（不含扩展名）。
- `format`：归档格式，支持 `"zip"`、`"tar"`、`"gztar"`（.tar.gz）、
  `"bztar"`（.tar.bz2）、`"xztar"`（.tar.xz）。
- `rootDir`：创建归档时切换到该目录（归档内路径相对于此）。
- `baseDir`：归档内包含的起始目录（相对于 `rootDir`）；`nil` 时归档整个 `rootDir`。

---

### shutil.which

```
shutil.which(name, mode=nil, path=nil) → str|nil
```

在 `path`（默认为 `PATH` 环境变量）中查找名为 `name` 的可执行文件。
`mode` 为文件访问位掩码（默认 `os.F_OK | os.X_OK`，即存在且可执行）。
找到返回完整路径字符串；未找到返回 `nil`。

## 示例

```ms
import shutil
import os

// 1. 复制文件（保留元数据）
shutil.copy2("config.yaml", "backup/config.yaml")

// 2. 移动目录
shutil.move("old_build", "archive/build_20240101")

// 3. 递归删除目录
shutil.rmtree("tmp", ignoreErrors=true)

// 4. 复制目录树，排除 __pycache__ 和 .git
shutil.copytree(
    "src",
    "dist/src",
    ignore=shutil.ignorePatterns("__pycache__", "*.pyc", ".git"),
)

// 5. 创建 .tar.gz 归档
archive := shutil.makeArchive("release-1.0", "gztar", rootDir=".", baseDir="src")
fmt.println($"归档创建于：{archive}")

// 6. 解压归档
shutil.unpackArchive("release-1.0.tar.gz", extractDir="unpacked")

// 7. 查询磁盘用量
usage := shutil.diskUsage("/")
fmt.println($"总量：{usage.total}, 已用：{usage.used}, 可用：{usage.free}")

// 8. 查找可执行文件
git := shutil.which("git")
if git != nil {
    fmt.println($"git 路径：{git}")
} else {
    fmt.println("git 未安装")
}

// 9. 在文件对象间复制
with open("src.bin", "rb") as fsrc {
    with open("dst.bin", "wb") as fdst {
        shutil.copyfileobj(fsrc, fdst)
    }
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `FileNotFoundError` | 源路径不存在 |
| `FileExistsError` | `copytree(dirsExistOk=false)` 时目标目录已存在 |
| `IsADirectoryError` | 对目录执行了仅适用于文件的操作 |
| `PermissionError` | 无权读取源或写入目标 |
| `OSError` | 磁盘满、跨设备移动等底层错误 |
| `ValueError` | `makeArchive` 传入不支持的 `format` |
