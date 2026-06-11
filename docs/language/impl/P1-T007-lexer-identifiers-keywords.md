# P1-T007 标识符与关键字识别

> **状态**：✅ 已完成

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
| `syntax.md` | §1.1 字符集与编码（按字节读取 UTF-8） |
| `syntax.md` | §1.3 自动分号插入（golden 中 NEWLINE 期望的依据） |
| `syntax.md` | §1.4 关键字（完整保留字列表） |
| `syntax.md` | §1.5 标识符（letter/digit BNF） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c   # 添加 lexScanIdent() 内部函数
```

### 内部函数

```c
// 扫描标识符（假设当前字节已确认为首字母）
// 返回 MS_TOK_IDENT 或对应关键字 token
static struct MsToken lexScanIdent(struct MsLexer* lex);

// 查找关键字（排序表二分查找）
// 返回对应 MsTokKind，或 MS_TOK_IDENT 表示非关键字
static MsTokKind lexLookupKeyword(const char* start, uint32_t len);
```

### 关键字表

```c
// syntax.md §1.4 全部 38 个关键字（按字母序，供二分查找）
static const struct { const char* word; MsTokKind kind; } kKeywords[] = {
  {"and",         MS_TOK_AND},
  {"as",          MS_TOK_AS},
  {"assert",      MS_TOK_ASSERT},
  {"async",       MS_TOK_ASYNC},
  {"await",       MS_TOK_AWAIT},
  {"break",       MS_TOK_BREAK},
  {"case",        MS_TOK_CASE},
  {"catch",       MS_TOK_CATCH},
  {"chan",        MS_TOK_CHAN},
  {"class",       MS_TOK_CLASS},
  {"continue",    MS_TOK_CONTINUE},
  {"default",     MS_TOK_DEFAULT},
  {"del",         MS_TOK_DEL},
  {"else",        MS_TOK_ELSE},
  {"extends",     MS_TOK_EXTENDS},
  {"fallthrough", MS_TOK_FALLTHROUGH},
  {"false",       MS_TOK_FALSE},
  {"finally",     MS_TOK_FINALLY},
  {"for",         MS_TOK_FOR},
  {"func",        MS_TOK_FUNC},
  {"go",          MS_TOK_GO},
  {"if",          MS_TOK_IF},
  {"import",      MS_TOK_IMPORT},
  {"in",          MS_TOK_IN},
  {"is",          MS_TOK_IS},
  {"make",        MS_TOK_MAKE},
  {"nil",         MS_TOK_NIL},
  {"not",         MS_TOK_NOT},
  {"or",          MS_TOK_OR},
  {"pass",        MS_TOK_PASS},
  {"raise",       MS_TOK_RAISE},
  {"return",      MS_TOK_RETURN},
  {"select",      MS_TOK_SELECT},
  {"switch",      MS_TOK_SWITCH},
  {"true",        MS_TOK_TRUE},
  {"try",         MS_TOK_TRY},
  {"var",         MS_TOK_VAR},
  {"with",        MS_TOK_WITH},
};

#define KEYWORD_COUNT (sizeof(kKeywords) / sizeof(kKeywords[0]))
_Static_assert(KEYWORD_COUNT == 38, "syntax.md §1.4 keyword count");
```

---

## 实现要点

1. **首字节判断**：`isalpha((unsigned char)c) || c == '_'` 进入标识符扫描；非 ASCII 字节（`>= 0x80`）也视为有效 Unicode 字母起始字节（`syntax.md §1.5 unicode_letter`），直接纳入 token 直到下一个非字母数字字节。
2. **后续字节**：`isalnum((unsigned char)c) || c == '_' || c >= 0x80`。
3. **关键字查找**：将扫描得到的字节段与关键字表对比；初版用**排序表+二分查找**（38 个关键字，O(log38) ≈ 6 次比较），足够快。后续如有性能需求可换完美哈希（gperf/手写 switch-on-first-char）。
4. **`true`/`false`/`nil`**：在关键字表中，产生 `MS_TOK_TRUE`/`MS_TOK_FALSE`/`MS_TOK_NIL`（非 `MS_TOK_IDENT`）。
5. **`len`/`type`**：是内置全局函数（非保留字，`syntax.md §1.4`），产生 `MS_TOK_IDENT`，不在关键字表中。

---

## 验收标准（checklist）

- [x] `"if"` → `MS_TOK_IF`，`"ifx"` → `MS_TOK_IDENT`，`"x"` → `MS_TOK_IDENT`。
- [x] 全部 38 个关键字各产生对应 token 种类（表驱动覆盖测试，遍历 `kKeywords[]`）。
- [x] `"len"` / `"type"` → `MS_TOK_IDENT`（非关键字）。
- [x] `"_private"` / `"__init__"` → `MS_TOK_IDENT`。
- [x] 含 Unicode 字母的标识符 `"名前"` → `MS_TOK_IDENT`（UTF-8 字节透明；初版暂不拒绝非字母 Unicode 码点，见「风险与边界」）。
- [x] 数字不可作为标识符首字符：`"1x"` 的首字节 `1` 不进入 `lexScanIdent`；完整行为（`MS_TOK_INT(1)` + `MS_TOK_IDENT("x")`）在 P1-T008（整数字面量）的验收中验证。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_ident_keywords.c`）

```c
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_lexer.h"

// 与 src/lexer/ms_lexer.c 的 kKeywords[] 同步维护的关键字-枚举映射
// （syntax.md §1.4 全部 38 个，按字母序）
static const struct { const char* word; MsTokKind kind; } kKeywordCases[] = {
  {"and", MS_TOK_AND},     {"as", MS_TOK_AS},       {"assert", MS_TOK_ASSERT},
  {"async", MS_TOK_ASYNC}, {"await", MS_TOK_AWAIT}, {"break", MS_TOK_BREAK},
  {"case", MS_TOK_CASE},   {"catch", MS_TOK_CATCH}, {"chan", MS_TOK_CHAN},
  {"class", MS_TOK_CLASS}, {"continue", MS_TOK_CONTINUE},
  {"default", MS_TOK_DEFAULT},         {"del", MS_TOK_DEL},
  {"else", MS_TOK_ELSE},   {"extends", MS_TOK_EXTENDS},
  {"fallthrough", MS_TOK_FALLTHROUGH}, {"false", MS_TOK_FALSE},
  {"finally", MS_TOK_FINALLY},         {"for", MS_TOK_FOR},
  {"func", MS_TOK_FUNC},   {"go", MS_TOK_GO},       {"if", MS_TOK_IF},
  {"import", MS_TOK_IMPORT}, {"in", MS_TOK_IN},     {"is", MS_TOK_IS},
  {"make", MS_TOK_MAKE},   {"nil", MS_TOK_NIL},     {"not", MS_TOK_NOT},
  {"or", MS_TOK_OR},       {"pass", MS_TOK_PASS},   {"raise", MS_TOK_RAISE},
  {"return", MS_TOK_RETURN}, {"select", MS_TOK_SELECT},
  {"switch", MS_TOK_SWITCH}, {"true", MS_TOK_TRUE}, {"try", MS_TOK_TRY},
  {"var", MS_TOK_VAR},     {"with", MS_TOK_WITH},
};

#define KEYWORD_CASE_COUNT (sizeof(kKeywordCases) / sizeof(kKeywordCases[0]))
_Static_assert(KEYWORD_CASE_COUNT == 38, "keyword coverage must stay 38/38");

static void lexOne(const char* src, MsTokKind expected) {
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<test>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, expected, src);
}

// 表驱动：38/38 全覆盖，关键字表演化时只需同步映射表
static void testKeywords(void) {
  for (size_t i = 0; i < KEYWORD_CASE_COUNT; i++) {
    lexOne(kKeywordCases[i].word, kKeywordCases[i].kind);
  }
}

// 二分查找前提：映射表（与实现表同序）须按字母序排好
static void testKeywordTableSorted(void) {
  for (size_t i = 1; i < KEYWORD_CASE_COUNT; i++) {
    MS_ASSERT_EQ(strcmp(kKeywordCases[i - 1].word, kKeywordCases[i].word) < 0,
                 true, kKeywordCases[i].word);
  }
}

static void testNonKeywordIdents(void) {
  lexOne("len", MS_TOK_IDENT);
  lexOne("type", MS_TOK_IDENT);
  lexOne("self", MS_TOK_IDENT);
  lexOne("x", MS_TOK_IDENT);
  lexOne("_priv", MS_TOK_IDENT);
  lexOne("__init__", MS_TOK_IDENT);
}

static void testIdentPrefixOfKeyword(void) {
  lexOne("ifx", MS_TOK_IDENT);    // "if" 前缀但不是关键字
  lexOne("fore", MS_TOK_IDENT);   // "for" 前缀
  lexOne("trueish", MS_TOK_IDENT);
}

int main(void) {
  MS_RUN(testKeywords);
  MS_RUN(testKeywordTableSorted);
  MS_RUN(testNonKeywordIdents);
  MS_RUN(testIdentPrefixOfKeyword);
  return msTestSummary();
}
```

### golden 测试（`tests/golden/ident_kw/`）

> **延迟验收**：本 golden 依赖 ASI（T015）与 `tokens` 子命令（T016），在 T015/T016
> 完成后启用；本任务的即时验收以上方 C 单测为准。

```
// input.ms
if else for func class
x _x len type self
```

```
// expected（mslang tokens 输出格式，T016 确定后对齐）
// 注：第 1 行末 token 为关键字 class，不在 syntax.md §1.3 的 ASI 触发列表中，
// 不产生 NEWLINE；第 2 行末 token 为标识符 self，触发 ASI。
1:1  IF         "if"
1:4  ELSE       "else"
1:9  FOR        "for"
1:13 FUNC       "func"
1:18 CLASS      "class"
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
- **大小写敏感**：标识符区分大小写（`syntax.md §1.5`）；`If` / `IF` 均为 `MS_TOK_IDENT`，不是关键字。
- **关键字表变更**：若后续版本新增保留字，只需在 `kKeywords` 表添加一行；二分查找要求数组已按字母序排好（运行时会验证，或编译期断言）。
