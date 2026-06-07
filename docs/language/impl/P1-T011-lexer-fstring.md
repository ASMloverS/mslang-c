# P1-T011 f-string 词法 `$"…{expr}…"`

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 f-string（字符串内插）的词法处理。`$"…{expr}…"` 是语法糖，词法器需将其拆分为：字符串片段序列与嵌入表达式 token 流。词法器产生 `TOK_FSTRING_START`、`TOK_FSTRING_PART`、`TOK_FSTRING_EXPR_START`、`TOK_FSTRING_EXPR_END`、`TOK_FSTRING_END` 等特殊 token，parser 据此组装 AST 节点（`ConcatExpr`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T010 | 字符串字面量扫描（共用转义处理） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.8.1 f-string（字符串内插）字面量 |
| `stdlib.md` | §3 字符串内插 |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c    # 添加 scanFString() 及 f-string 状态
include/mslang/ms_lexer.h # 新增 f-string token 种类（追加到枚举）
```

### 新增 Token 种类（在 `MsTokKind` 枚举中追加）

```c
TOK_FSTRING_START,      // $" 的开始（携带第一个字符串片段，可能为空）
TOK_FSTRING_PART,       // } 之后到下一 { 或结束 " 之间的字符串片段
TOK_FSTRING_EXPR_START, // { 位置（触发 parser 开始解析内嵌表达式）
TOK_FSTRING_EXPR_END,   // } 位置（parser 停止解析内嵌表达式，回到 f-string 模式）
TOK_FSTRING_END,        // 闭合 " 位置
```

### 内部状态

```c
// MsLexer 中添加 f-string 嵌套状态
typedef enum FStrState { FSTR_NONE, FSTR_OUTER, FSTR_INNER } FStrState;

// 在 MsLexer 结构体中追加：
// FStrState  fstrState;
// int        fstrDepth;   // { } 嵌套深度（支持 $"{ {1:2}[k] }" 等）
```

---

## 实现要点

1. **`$"` 识别**：在 `msLexNext` 中，遇到 `$` 后检查下一字节；若为 `"`，进入 f-string 扫描模式（`FSTR_OUTER`），产生 `TOK_FSTRING_START`（含 `$"` 到第一个 `{` 或 `"` 之间的字符串片段）。`$` 在其他位置产生 `TOK_ERROR`。
2. **`FSTR_OUTER` 状态**：扫描字节直到：
   - 遇 `{`（且下一字节不是 `{`）→ 产生 `TOK_FSTRING_EXPR_START`，切换 `FSTR_INNER`，`fstrDepth=1`。
   - 遇 `{{` → 转义为单个 `{`，纳入当前字符串片段。
   - 遇 `}}` → 转义为单个 `}`，纳入当前字符串片段。
   - 遇 `}` 在 `FSTR_OUTER`（非 `FSTR_INNER`） → 语法错误（单独 `}` 不合法）。
   - 遇 `"` → 产生 `TOK_FSTRING_END`，退出 f-string 模式。
   - 遇 `\n` 或 `\0` → `TOK_ERROR`（未终止的 f-string）。
   - 转义序列同普通字符串处理。
3. **`FSTR_INNER` 状态**（在 `{…}` 内部）：词法器正常产生 token；遇 `{` 时 `fstrDepth++`（支持 `$"{{ {x} }}"` 中的字典字面量），遇 `}` 时 `fstrDepth--`；当 `fstrDepth == 0` 时产生 `TOK_FSTRING_EXPR_END`，切换回 `FSTR_OUTER`。
4. **嵌套 f-string**：`$"outer {$"inner"} !"` 合法（`FSTR_INNER` 内遇到 `$"` 进入另一层 f-string，栈式管理）；初版可不支持嵌套（产生 `TOK_ERROR`），文档明确标出。
5. **ASI 与 f-string**：f-string 的闭合 `"` 触发 ASI（与普通字符串同规则），`TOK_FSTRING_END` 同样作为 ASI 触发 token。

---

## 验收标准（checklist）

- [ ] `$"hello"` → `TOK_FSTRING_START("hello")`, `TOK_FSTRING_END`。
- [ ] `$"hello {name}!"` → `START("hello ")`, `EXPR_START`, `IDENT("name")`, `EXPR_END`, `PART("!")`, `END`。
- [ ] `$"{1 + 1}"` → `START("")`, `EXPR_START`, `INT(1)`, `PLUS`, `INT(1)`, `EXPR_END`, `PART("")`, `END`。
- [ ] `$"{{literal braces}}"` → `START("{literal braces}")`, `END`（`{{`/`}}` 转义）。
- [ ] `$"未终止` → `TOK_ERROR`（无闭合 `"`）。
- [ ] `$" {x` → `TOK_ERROR`（`{` 未匹配 `}`）。
- [ ] 在 `{…}` 内的 `{1: 2}` map 字面量正确处理 `fstrDepth`（`{` 使 depth=2，`}` 先减到 1，再遇第二个 `}` 减到 0 产生 `EXPR_END`）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_fstring.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"
#include <string.h>

static void testSimpleFString(void) {
  const char* src = "$\"hello\"";
  MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  MsToken t1 = msLexNext(&lex);
  MS_ASSERT_EQ(t1.kind, TOK_FSTRING_START, "fstring start");
  MsToken t2 = msLexNext(&lex);
  MS_ASSERT_EQ(t2.kind, TOK_FSTRING_END, "fstring end");
}

static void testFStringWithExpr(void) {
  const char* src = "$\"hi {x}!\"";
  MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  MsToken t;
  t = msLexNext(&lex); MS_ASSERT_EQ(t.kind, TOK_FSTRING_START, "start");
  t = msLexNext(&lex); MS_ASSERT_EQ(t.kind, TOK_FSTRING_EXPR_START, "expr_start");
  t = msLexNext(&lex); MS_ASSERT_EQ(t.kind, TOK_IDENT, "ident x");
  t = msLexNext(&lex); MS_ASSERT_EQ(t.kind, TOK_FSTRING_EXPR_END, "expr_end");
  t = msLexNext(&lex); MS_ASSERT_EQ(t.kind, TOK_FSTRING_PART, "part '!'");
  t = msLexNext(&lex); MS_ASSERT_EQ(t.kind, TOK_FSTRING_END, "end");
}

int main(void) {
  MS_RUN(testSimpleFString);
  MS_RUN(testFStringWithExpr);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
name := "world"
n    := 42
s    := $"hello {name}, n = {n}!"
print(s)
// hello world, n = 42!

// 嵌套表达式
items := [1, 2, 3]
print($"sum = {items[0] + items[1] + items[2]}")
// sum = 6

// {{ }} 字面大括号
print($"{{not an expr}}")
// {not an expr}
```

---

## Benchmark

N/A（f-string 扫描归入 T016 整体 bench）。

---

## 风险与边界

- **格式规范**：`syntax.md §1.8.1` 明确"初版不支持格式规范（如 `{x:.4f}`）"，词法器在 `{…}` 内遇 `:` 时不做特殊处理（仍视为 map 的 `:` 或普通 token），parser 层处理语义。
- **嵌套 f-string 初版**：建议初版直接禁止（在 `FSTR_INNER` 内遇 `$"` 时产生 `TOK_ERROR`），文档标明"嵌套 f-string 为保留扩展"。
- **`}}` 在 FSTR_OUTER**：`$"a}}b"` → `"a}b"`（转义）；`$"a}b"` → `TOK_ERROR`（单 `}` 不合法）。
