# P12-T135 stdlib: os.path

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `os.path` 子模块：路径字符串操作（join/split/basename/dirname/splitext/normpath/abspath/expanduser）。纯字符串操作，不涉及文件系统 I/O，跨平台。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P7-T089 | 包（`os` 包含子模块 `os.path`） |
| P4-T057 | 字符串操作 |

---

## API 清单

```ms
// 对齐 stdlib/os.md（os.path 部分）
os.path.join(*parts)         // → str  连接路径片段
os.path.split(path)          // → (head, tail) tuple
os.path.basename(path)       // → str  最后一个分隔符后的部分
os.path.dirname(path)        // → str  最后一个分隔符前的部分
os.path.splitext(path)       // → (root, ext)
os.path.normpath(path)       // → str  规范化（消除 ./ ../ 双斜杠）
os.path.abspath(path)        // → str  转绝对路径（基于 cwd）
os.path.expanduser(path)     // → str  ~ 替换为用户主目录
os.path.isabs(path)          // → bool
os.path.exists(path)         // → bool（需要系统调用）
os.path.isfile(path)         // → bool
os.path.isdir(path)          // → bool
os.path.islink(path)         // → bool（仅 POSIX）
os.path.getsize(path)        // → int  文件大小
os.path.getmtime(path)       // → float Unix 时间戳
os.path.relpath(path, start=".")  // → str 相对路径
os.path.commonprefix(paths)  // → str
os.path.sep                  // str  路径分隔符 "/" 或 "\\"
```

---

## 实现要点

```c
// 路径操作全部为纯字符串处理，与平台路径分隔符无关（内部统一 '/'）
// Windows 下：sep = '\\'，但同时接受 '/'（兼容）

// join("a", "b", "c") → "a/b/c"
// 若中间某片段以 '/' 开头，丢弃其前的部分（绝对路径覆盖）
static MsValue pathJoin(MsThread* t, MsValue* args, int argc) {
  if (argc == 0) return msNewStr("", 0);
  // 从后往前找第一个绝对路径片段，然后 join
  int start = 0;
  for (int i = argc - 1; i >= 0; i--) {
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(args[i]);
    if (s->len > 0 && (s->data[0] == '/' || s->data[0] == '\\')) {
      start = i; break;
    }
  }
  // 连接 args[start..argc)
}
```

---

## 验收标准（checklist）

- [ ] `os.path.join("a","b","c")` → `"a/b/c"`（POSIX）。
- [ ] `os.path.join("a","/b")` → `"/b"`（绝对路径覆盖）。
- [ ] `os.path.basename("/foo/bar.txt")` → `"bar.txt"`。
- [ ] `os.path.splitext("foo.tar.gz")` → `("foo.tar", ".gz")`。
- [ ] `os.path.normpath("a/./b/../c")` → `"a/c"`。
- [ ] `os.path.expanduser("~/foo")` → `"/home/user/foo"` 等。

---

## 测试用例（.ms）

```ms
import os

p := os.path.join("home", "user", "docs")
print(p)   // home/user/docs

print(os.path.basename("/foo/bar.txt"))  // bar.txt
print(os.path.dirname("/foo/bar.txt"))   // /foo
print(os.path.splitext("file.tar.gz"))  // ("file.tar", ".gz")
print(os.path.normpath("a/./b/../c"))   // a/c
print(os.path.isabs("/tmp"))            // true
print(os.path.isabs("relative"))        // false
```

---

## 风险与边界

- **Windows 路径**：Windows 下需要支持 `C:\foo\bar` 形式（盘符 + 反斜杠）；`os.path.isabs` 要检测盘符前缀；路径 join 后在 Windows 显示 `\`，在 POSIX 显示 `/`。
