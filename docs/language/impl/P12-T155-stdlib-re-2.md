# P12-T155 stdlib: re（search / findall / 分组 / flags）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

在 T154 NFA 引擎基础上实现 `re.search`、`re.findall`、`re.finditer`、命名分组、lookahead/lookbehind 断言。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T154 | re 编译 + 基础匹配（NFA 引擎） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-re-2.md` | §1 模块 API |

---

## API 清单（本任务范围）

```ms
// 搜索
re.search(pattern, string, flags=0) → Match|nil
// 在 string 任意位置匹配

pat.search(string, pos=0, endpos=-1)

// 查找所有
re.findall(pattern, string, flags=0) → list[str]
// 无分组：返回所有匹配子串
// 有分组：返回分组元组列表

re.finditer(pattern, string, flags=0) → iterator[Match]
// 返回所有非重叠匹配的迭代器

// 命名分组（本任务扩展 NFA）
// (?P<name>pattern) 匹配命名分组
m.group("name")         // 按名访问
m.groupdict() → dict    // 所有命名分组
m.groupdict(default="") // 未匹配分组填 default

// 断言（零宽）
// (?=...)  正向前瞻
// (?!...)  负向前瞻
// (?<=...) 正向后顾（固定宽度）
// (?<!...) 负向后顾（固定宽度）

// 编译后的 pattern 对象属性
pat.pattern   // 原始 pattern 字符串
pat.flags     // 编译标志
pat.groups    // 分组数量
pat.groupindex  // 命名分组 name→index 映射
```

---

## 实现要点

```c
// search：在 pos=0..len(s) 处依次尝试 match，找到即返回
// 优化：若 pattern 以字面字符开头，跳过不含该字符的位置

// findall：循环调用 search，非重叠，每次从上次结束位置继续
// 空匹配处理：空匹配后前进一个字节（避免无限循环）

// 命名分组：解析 (?P<name>...) 时记录 name→group_idx 映射
// 存在 MsPatternObj.groupindex MsMapObj

// 零宽断言实现：
// 正向前瞻 (?=...): 当前位置尝试匹配子模式，成功但不推进位置
// 负向前瞻 (?!...): 子模式失败才继续
// 后顾：固定宽度，退 n 字节尝试匹配
// 变宽后顾不支持（抛 error.NotImplemented）

// NFA 状态扩展：
// LOOKAHEAD_POS / LOOKAHEAD_NEG / LOOKBEHIND_POS / LOOKBEHIND_NEG
// 匹配时进入特殊模式（不捕获，不推进主指针）
```

---

## 验收标准（checklist）

- [ ] `re.search(r"\d+", "foo123bar")` → match `"123"`。
- [ ] `re.findall(r"\d+", "1 and 2 and 3")` → `["1","2","3"]`。
- [ ] `re.findall(r"(\w+)=(\w+)", "a=1 b=2")` → `[("a","1"),("b","2")]`。
- [ ] 命名分组 `(?P<year>\d{4})` 通过 `m.group("year")` 访问。
- [ ] 正向前瞻 `\d+(?= dollars)` 只匹配"dollars"前的数字。
- [ ] `re.finditer` 返回可迭代的 Match 对象序列。

---

## 测试用例（.ms）

```ms
import re

// search
m := re.search(r"\b\d{3}-\d{4}\b", "call 555-1234 today")
print(m.group())    // "555-1234"

// findall
emails := re.findall(r"[\w.]+@[\w.]+", "a@b.com x@y.org")
print(emails)       // ["a@b.com","x@y.org"]

// 命名分组
pat := re.compile(r"(?P<year>\d{4})-(?P<month>\d{2})-(?P<day>\d{2})")
m2 := pat.search("date: 2024-01-15")
print(m2.groupdict())  // {"year":"2024","month":"01","day":"15"}

// 前瞻断言
prices := re.findall(r"\d+(?= USD)", "100 USD and 200 EUR")
print(prices)       // ["100"]（只匹配 USD 前的数字）

// finditer
for m in re.finditer(r"\w+", "one two three") {
    print(m.group(), m.start())
}
```
