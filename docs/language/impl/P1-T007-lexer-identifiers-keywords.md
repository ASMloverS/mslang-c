# P1-T007 标识符与关键字识别

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

在 T006 的 lexer 框架上实现标识符扫描与关键字识别。标识符为 `[A-Za-z_][A-Za-z0-9_]*` 加任意 Unicode 字母（UTF-8）；关键字为固定字符串集合，通过最小完美哈希表或 trie 加速查找。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T006 | Lexer 框架与 Token 定义 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.4 关键字（完整保留字列表） |
| `syntax.md` | §1.5 标识符（letter/digit BNF） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c   # 添加 scanIdent() 内部函数
```

### 内部函数

```c
// 扫描标识符（假设当前字节已确认为首字母）
// 返回 TOK_IDENT 或对应关键字 token
static MsToken scanIdent(MsLexer* lex);

// 查找关键字（完美哈希或二分查找）
// 返回对应 MsTokKind，或 TOK_IDENT 表示非关键字
static MsTokKind lookupKeyword(const char* start, uint32_t len);
```

### 关键字表

```c
// syntax.md §1.4 全部 37 个关键字
static const struct { const char* word; MsTokKind kind; } kKeywords[] = {
  {"and",         TOK_AND},
  {"as",          TOK_AS},
  {"async",       TOK_ASYNC},
  {"await",       TOK_AWAIT},
  {"break",       TOK_BREAK},
  {"case",        TOK_CASE},
  {"catch",       TOK_CATCH},
  {"chan",         TOK_CHAN},
  {"class",       TOK_CLASS},
  {"continue",    TOK_CONTINUE},
  {"default",     TOK_DEFAULT},
  {"del",         TOK_DEL},
  {"else",        TOK_ELSE},
  {"extends",     TOK_EXTENDS},
  {"fallthrough", TOK_FALLTHROUGH},
  {"false",       TOK_FALSE},
  {"finally",     TOK_FINALLY},
  {"for",         TOK_FOR},
  {"func",        TOK_FUNC},
  {"go",          TOK_GO},
  {"if",          TOK_IF},
  {"import",      TOK_IMPORT},
  {"in",          TOK_IN},
  {"is",          TOK_IS},
  {"make",        TOK_MAKE},
  {"nil",         TOK_NIL},
  {"not",         TOK_NOT},
  {"or",          TOK_OR},
  {"pass",        TOK_PASS},
  {"raise",       TOK_RAISE},
  {"return",      TOK_RETURN},
  {"select",      TOK_SELECT},
  {"switch",      TOK_SWITCH},
  {"true",        TOK_TRUE},
  {"try",         TOK_TRY},
  {"var",         TOK_VAR},
  {"with",        TOK_WITH},
  // 末尾哨兵
  {NULL, TOK_IDENT},
};
```

---

## 实现要点

1. **首字节判断**：`isalpha((unsigned char)c) || c == '_'` 进入标识符扫描；非 ASCII 字节（`>= 0x80`）也视为有效 Unicode 字母起始字节（`syntax.md §1.5 unicode_letter`），直接纳入 token 直到下一个非字母数字字节。
2. **后续字节**：`isalnum((unsigned char)c) || c == '_' || c >= 0x80`。
3. **关键字查找**：将扫描得到的字节段与关键字表对比；初版用**排序表+二分查找**（37 个关键字，O(log37) ≈ 6 次比较），足够快。后续如有性能需求可换完美哈希（gperf/手写 switch-on-first-char）。
4. **`true`/`false`/`nil`**：在关键字表中，产生 `TOK_TRUE`/`TOK_FALSE`/`TOK_NIL`（非 `TOK_IDENT`）。
5. **`len`/`type`**：是内置全局函数（非保留字，`syntax.md §1.4`），产生 `TOK_IDENT`，不在关键字表中。

---

## 验收标准（checklist）

- [ ] `"if"` → `TOK_IF`，`"ifx"` → `TOK_IDENT`，`"x"` → `TOK_IDENT`。
- [ ] 全部 37 个关键字各产生对应 token 种类（覆盖测试）。
- [ ] `"len"` / `"type"` → `TOK_IDENT`（非关键字）。
- [ ] `"_private"` / `"__init__"` → `TOK_IDENT`。
- [ ] 含 Unicode 字母的标识符 `"名前"` → `TOK_IDENT`（UTF-8 字节透明）。
- [ ] 数字不可作为标识符首字符：`"1x"` 先产生 `TOK_INT(1)`，再产生 `TOK_IDENT("x")`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_ident_keywords.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"

static void lexOne(const char* src, MsTokKind expected) {
  MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<test>");
  MsToken t = msLexNext(&lex);
  MS_ASSERT_EQ(t.kind, expected, src);
}

static void testKeywords(void) {
  lexOne("if",          TOK_IF);
  lexOne("else",        TOK_ELSE);
  lexOne("for",         TOK_FOR);
  lexOne("func",        TOK_FUNC);
  lexOne("class",       TOK_CLASS);
  lexOne("return",      TOK_RETURN);
  lexOne("async",       TOK_ASYNC);
  lexOne("await",       TOK_AWAIT);
  lexOne("go",          TOK_GO);
  lexOne("fallthrough", TOK_FALLTHROUGH);
  lexOne("with",        TOK_WITH);
  lexOne("del",         TOK_DEL);
  lexOne("true",        TOK_TRUE);
  lexOne("false",       TOK_FALSE);
  lexOne("nil",         TOK_NIL);
}

static void testNonKeywordIdents(void) {
  lexOne("len",    TOK_IDENT);
  lexOne("type",   TOK_IDENT);
  lexOne("self",   TOK_IDENT);
  lexOne("x",      TOK_IDENT);
  lexOne("_priv",  TOK_IDENT);
  lexOne("__init__", TOK_IDENT);
}

static void testIdentPrefixOfKeyword(void) {
  lexOne("ifx",    TOK_IDENT);   // "if" 前缀但不是关键字
  lexOne("fore",   TOK_IDENT);   // "for" 前缀
  lexOne("trueish",TOK_IDENT);
}

int main(void) {
  MS_RUN(testKeywords);
  MS_RUN(testNonKeywordIdents);
  MS_RUN(testIdentPrefixOfKeyword);
  return msTestSummary();
}
```

### golden 测试（`tests/golden/ident_kw/`）

```
// input.ms
if else for func class
x _x len type self
```

```
// expected（mslang tokens 输出格式，T016 确定后对齐）
1:1  IF         "if"
1:4  ELSE       "else"
1:9  FOR        "for"
1:13 FUNC       "func"
1:18 CLASS      "class"
1:24 NEWLINE    ";"
2:1  IDENT      "x"
2:3  IDENT      "_x"
2:6  IDENT      "len"
2:10 IDENT      "type"
2:15 IDENT      "self"
2:19 NEWLINE    ";"
3:1  EOF
```

---

## .ms 使用示例

N/A（词法层，用 `mslang tokens` CLI 验证）。

---

## Benchmark

C microbench（包含于 T016 的 bench_lexer 中）：目标 ≥ 10M 关键字查找/秒（二分查找）。

---

## 风险与边界

- **Unicode 字母检测**：初版按"字节 >= 0x80 即为 Unicode 字母"简化处理（不完全符合 Unicode ID_Start 规范，但对实际使用影响极小）；后续可引入 Unicode 属性表精确判断。
- **大小写敏感**：标识符区分大小写（`syntax.md §1.5`）；`If` / `IF` 均为 `TOK_IDENT`，不是关键字。
- **关键字表变更**：若后续版本新增保留字，只需在 `kKeywords` 表添加一行；二分查找要求数组已按字母序排好（运行时会验证，或编译期断言）。
