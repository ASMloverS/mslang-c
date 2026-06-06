# P12-T156 stdlib: re（sub / subn / split / 高级）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

完成 `re` 模块：替换（`sub`/`subn`）、拆分（`split`）、高级功能（反向引用、VERBOSE 模式）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T155 | re search/findall/分组 |

---

## API 清单（本任务范围）

```ms
// 替换
re.sub(pattern, repl, string, count=0, flags=0) → str
// repl 可为字符串或函数
// 字符串 repl 中：\1...\9 引用分组，\g<name> 命名分组，\0=整体

re.subn(pattern, repl, string, count=0, flags=0) → (str, n)
// 同 sub，但额外返回替换次数

pat.sub(repl, string, count=0)
pat.subn(repl, string, count=0)

// 拆分
re.split(pattern, string, maxsplit=0, flags=0) → list[str]
// 按 pattern 拆分，若 pattern 含分组则分组也包含在结果中

// 高级功能
// VERBOSE 模式（re.X / re.VERBOSE）：
// 忽略未转义空白，# 开头到行尾为注释
// 方便书写复杂正则

// 反向引用 \1...\9（模式内，引用前面的分组）
// 同一字符出现两次：r"(.)\1" 匹配重复字符

// 条件分组 (?(id)yes|no)（可选实现，标注为 P12 后期）

// 预编译 re 对象缓存（re.compile 内部）
// 全局 LRU 缓存最近 512 个 pattern+flags 组合
```

---

## 实现要点

```c
// sub 实现：
// 循环 finditer → 对每个 match 调用 expand(repl, match)
// expand：解析 repl 字符串，将 \1 替换为 match.group(1) 等
// 若 repl 是 callable：repl(match)

// split 实现：
// 循环 search → 截取前缀，若 pattern 含分组则追加各分组
// 结束后追加剩余后缀

// 反向引用（模式内）：
// NFA 不直接支持反向引用（理论上需要 NFA→回溯）
// 实现方案：匹配时在引用处检查已捕获分组内容（回溯引擎）
// 注：反向引用使正则成 NP-hard，加入步数限制（MATCH_STEP_LIMIT=1M）
// 超限抛 re.error("catastrophic backtracking detected")

// repl 字符串中 \g<name> 替换：
// 找到 groupindex[name] → 对应分组内容

// VERBOSE 模式预处理：
// re.compile 时先移除注释和未转义空白，再走正常编译路径
```

---

## 验收标准（checklist）

- [ ] `re.sub(r"\d+", "NUM", "a1 b22 c333")` → `"aNUM bNUM cNUM"`。
- [ ] `re.sub(r"(\w+) (\w+)", r"\2 \1", "hello world")` → `"world hello"`。
- [ ] `re.subn(r"a", "b", "aaa")` → `("bbb", 3)`。
- [ ] `re.split(r"\s+", "one  two   three")` → `["one","two","three"]`。
- [ ] 含分组的 split：`re.split(r"(\s+)", "a b")` → `["a"," ","b"]`。
- [ ] VERBOSE 模式：多行注释 pattern 编译正确。
- [ ] 反向引用：`re.search(r"(.)\1", "aabb")` 匹配 `"aa"`。

---

## 测试用例（.ms）

```ms
import re

// sub with backreference
result := re.sub(r"(\w+)@(\w+)", r"\2@\1", "user@host")
print(result)  // "host@user"

// sub with function
result2 := re.sub(r"\d+", lambda m: str(int(m.group())*2), "1 2 3")
print(result2)  // "2 4 6"

// subn
s, n := re.subn(r"cat", "dog", "cat cat cat")
print(s, n)   // "dog dog dog" 3

// split
print(re.split(r",\s*", "a, b,  c,d"))  // ["a","b","c","d"]
print(re.split(r"(,)", "a,b,c"))        // ["a",",","b",",","c"]

// VERBOSE
phone_re := re.compile(r"""
    (\d{3})   # area code
    [-. ]?    # separator
    (\d{3})   # prefix
    [-. ]?    # separator
    (\d{4})   # line number
""", re.VERBOSE)
m := phone_re.match("555-867-5309")
print(m.groups())  // ("555","867","5309")

// 反向引用
m2 := re.search(r"(\w+)\s+\1", "the the cat")
print(m2.group())  // "the the"（重复词）
```

---

## Benchmark

```ms
import re, time

// 编译和搜索性能
pat := re.compile(r"\b\w{5,10}\b")
text := "The quick brown fox jumps over the lazy dog " * 1000
t0 := time.now()
n := 0
for _ in re.finditer(pat, text) { n = n + 1 }
t1 := time.now()
print("finditer 44K words:", t1-t0, "ms, matched:", n)  // 目标 < 100ms
```
