# P12-T154 stdlib: re（编译 + 基础匹配）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `re` 模块的核心：正则表达式编译（Thompson NFA 构造）和基础匹配（`match`、`fullmatch`）。全自实现 NFA/DFA，零外部依赖。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 | MsStrObj（UTF-8 字符串） |

---

## API 清单（本任务范围）

```ms
// 编译
pat := re.compile(pattern, flags=0)   // → MsPatternObj
// flags: re.IGNORECASE(I), re.MULTILINE(M), re.DOTALL(S),
//        re.ASCII(A), re.VERBOSE(X)

// 基础匹配（本任务）
re.match(pattern, string, flags=0)       // 匹配字符串开头
re.fullmatch(pattern, string, flags=0)   // 全字符串匹配
pat.match(string, pos=0, endpos=-1)
pat.fullmatch(string, pos=0, endpos=-1)
// 返回 MsMatchObj 或 nil

// MsMatchObj
m.group(n=0) → str     // n=0=整体，n=1..k=分组
m.groups() → tuple     // 所有分组
m.start(n=0) → int     // 匹配起始字节位置
m.end(n=0) → int       // 结束字节位置
m.span(n=0) → (start, end)
m.string            // 原始字符串
m.re                // 编译的 pattern
m.pos, m.endpos     // 搜索范围
```

---

## 实现要点

```c
// === NFA 构造（Thompson 法）===
// 1. 解析正则 → 后缀 NFA 片段
// 字符集语法：a b [abc] [a-z] . \d \w \s \D \W \S
// 量词：* + ? {m} {m,} {m,n} 贪婪与非贪婪（?）
// 分组：(...) (?:...) (?P<name>...)（命名分组）
// 锚点：^ $ \b \B（多行模式调整 ^$）
// 反向引用：\1...\9（Match 阶段验证，不影响 NFA）

typedef struct NfaState {
  int  type;           // SPLIT/CHAR/CLASS/MATCH/etc.
  NfaState* out1;
  NfaState* out2;
  union {
    uint32_t ch;     // 字面字符（Unicode codepoint）
    uint8_t* class_bits;  // 字符类位图（ASCII），Unicode 额外处理
  };
} NfaState;

// NFA 片段：(start, out_list)
// Thompson 连接：Alt（|），Cat（拼接），Quest/Star/Plus

// === 匹配引擎（Thompson NFA 模拟）===
// 状态集合用位集（或排序列表）
// 每次步进：计算 ε-闭包，然后按字符转换
// 分组捕获：扩展到 TNFA（每条路径记录 submatch 信息）
// 使用 leftmost-longest（贪婪）语义

// IGNORECASE：Unicode 折叠（简化：ASCII 大小写映射）
// DOTALL：'.' 匹配 '\n'
// MULTILINE：^ 匹配行首，$ 匹配行尾

typedef struct MsMatchObj {
  MsObject  header;
  MsValue   string;     // 原始字符串
  MsValue   pattern;    // MsPatternObj
  int32_t*  groups;     // [start,end] 对，groups[0..ngroups-1]
  int       ngroups;
  int       pos, endpos;
} MsMatchObj;
```

---

## 验收标准（checklist）

- [ ] `re.match(r"\d+", "123abc")` → match，`m.group()="123"`。
- [ ] `re.match(r"\d+", "abc")` → `nil`。
- [ ] `re.fullmatch(r"\d+", "123")` → match。
- [ ] `re.fullmatch(r"\d+", "123abc")` → `nil`。
- [ ] 分组：`re.match(r"(\w+)@(\w+)", "user@host").groups()` → `("user","host")`。
- [ ] IGNORECASE：`re.match(r"abc", "ABC", re.I)` → match。

---

## 测试用例（.ms）

```ms
import re

m := re.match(r"(\d+)\.(\d+)", "3.14 is pi")
if m {
    print(m.group())    // "3.14"
    print(m.group(1))   // "3"
    print(m.group(2))   // "14"
    print(m.span())     // (0, 4)
}

// fullmatch
print(re.fullmatch(r"[a-z]+", "hello"))  // match
print(re.fullmatch(r"[a-z]+", "hello1")) // nil

// 字符类
m2 := re.match(r"[\w\s]+", "hello world 123")
print(m2.group())  // "hello world 123"

// 量词
m3 := re.match(r"a{2,4}", "aaab")
print(m3.group())  // "aaa"（贪婪）

// flags
m4 := re.match(r"hello", "HELLO", re.IGNORECASE)
print(bool(m4))    // true
```
