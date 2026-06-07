# P12-T190 stdlib: pathlib

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `pathlib` 模块（对齐 `stdlib/pathlib.md`）：面向对象的文件系统路径操作，封装 `os.path` 和文件系统操作。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T135 | os.path |
| P12-T136 | os.fs |

---

## API 清单

```ms
// 创建 Path
p := pathlib.Path("/home/user/docs/file.txt")
p := pathlib.Path("./relative/path")
p := pathlib.Path.cwd()      // 当前工作目录
p := pathlib.Path.home()     // 用户主目录

// 路径拼接（/ 运算符重载）
p2 := p / "subdir" / "file.txt"
p3 := pathlib.Path("/root") / "data"

// 路径属性（只读）
p.name        // "file.txt"（最后一个组件）
p.stem        // "file"（无扩展名）
p.suffix      // ".txt"（扩展名）
p.suffixes    // [".tar", ".gz"]（所有扩展名）
p.parent      // Path("/home/user/docs")
p.parents     // [Path("/home/user/docs"), Path("/home/user"), ...]
p.parts       // ("/", "home", "user", "docs", "file.txt")
p.root        // "/"
p.drive       // ""（POSIX）或 "C:"（Windows）
p.anchor      // "/" 或 "C:\"

// 路径测试
p.exists()    → bool
p.is_file()   → bool
p.is_dir()    → bool
p.is_symlink() → bool
p.is_absolute() → bool
p.is_relative_to(other) → bool

// 路径解析
p.resolve() → Path   // 绝对路径（解析 symlinks）
p.relative_to(other) → Path  // 相对路径
p.with_name("other.txt") → Path  // 替换文件名
p.with_stem("other") → Path      // 替换 stem（保留 suffix）
p.with_suffix(".py") → Path      // 替换扩展名

// 文件系统操作
p.stat() → os.StatResult
p.mkdir(mode=0o777, parents=false, exist_ok=false)
p.touch(mode=0o666, exist_ok=true)
p.unlink(missing_ok=false)
p.rmdir()              // 仅删除空目录
p.rename(target) → Path
p.replace(target) → Path   // 覆盖目标

// 目录遍历
p.iterdir() → iterator[Path]        // 直接子项
p.glob(pattern) → iterator[Path]    // 通配符匹配
p.rglob(pattern) → iterator[Path]   // 递归通配符

// 文件读写（快捷方式）
p.read_text(encoding="utf-8") → str
p.write_text(text, encoding="utf-8")
p.read_bytes() → bytes
p.write_bytes(data)
p.open(mode="r", **kwargs) → file

// 字符串转换
str(p)     // 路径字符串（POSIX 用 /，Windows 用 \）
repr(p)    // "PosixPath('/home/user/...')"
p.as_posix() → str   // 强制 POSIX 格式（/ 分隔）
```

---

## 实现要点

```c
// MsPathObj：内部存储路径字符串（规范化格式）
// 平台抽象：PosixPath / WindowsPath（由 pathlib.Path 根据平台选择）

typedef struct MsPathObj {
  MsObject  header;
  MsStrObj* path;    // 规范化路径字符串（不含尾部 /）
  bool      absolute;
} MsPathObj;

// / 运算符：tpDiv 方法
// Path / "str"：拼接路径（os.path.join）
// Path / Path：同上

// glob 实现：
// 模式匹配：使用 fnmatch 语义（* 匹配非路径分隔符，** 递归）
// ** 匹配：递归 os.walk 然后 fnmatch 过滤

// iterdir()：os.listdir() → 排序 → 转 Path 对象

// read_text/write_text：
// with open(str(self), mode, encoding=encoding) as f: return f.read()

// resolve()：
// os.path.abspath + 解析 symlinks（os.path.realpath）
```

---

## 验收标准（checklist）

- [ ] `Path("/a/b") / "c"` → `Path("/a/b/c")`。
- [ ] `Path("/a/b/file.tar.gz").suffixes` → `[".tar", ".gz"]`。
- [ ] `Path(".").iterdir()` 列出当前目录。
- [ ] `p.glob("*.txt")` 返回匹配 .txt 文件。
- [ ] `p.read_text()` / `p.write_text()` round-trip。
- [ ] `p.mkdir(parents=true, exist_ok=true)` 创建中间目录不报错。

---

## 测试用例（.ms）

```ms
import pathlib, tempfile, os

// 基础操作
p := pathlib.Path("/usr/local/bin/python3")
print(p.name)      // "python3"
print(p.parent)    // /usr/local/bin
print(p.parts)     // ("/", "usr", "local", "bin", "python3")
print(p.suffix)    // ""（无扩展名）

p2 := pathlib.Path("archive.tar.gz")
print(p2.stem)     // "archive.tar"
print(p2.suffix)   // ".gz"
print(p2.suffixes) // [".tar", ".gz"]

// 路径拼接
base := pathlib.Path("/home/user")
full := base / "docs" / "readme.md"
print(str(full))   // /home/user/docs/readme.md
print(full.relative_to(base))  // docs/readme.md

// 文件操作（使用临时目录）
with tempfile.TemporaryDirectory() as tmp:
    d := pathlib.Path(tmp)
    (d / "sub").mkdir()
    (d / "sub" / "file.txt").write_text("hello")
    print((d / "sub" / "file.txt").read_text())  // "hello"
    files := list(d.rglob("*.txt"))
    print(len(files))   // 1

// glob
cwd := pathlib.Path(".")
py_files := list(cwd.glob("*.md"))
print(len(py_files) > 0)   // true（有 .md 文件）
```
