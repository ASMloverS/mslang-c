# P1-T013 运算符与界符完整集

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `syntax.md §1.10` 中全部运算符与界符的扫描：算术/位/比较/赋值/复合赋值/channel/`<-`/`...`/`++`/`--` 以及各括号、分隔符。这些 token 是语法分析的基础，每个都需要正确的双字符/三字符前向探测。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T006 | Lexer 框架 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.10 运算符与界符（完整列表） |
| `syntax.md` | §2.3 表达式（运算符在文法中的使用） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c   # 添加 scanOperator() 内部函数
```

---

## 实现要点

运算符扫描采用**switch-on-first-char + 前向探测**策略：

```c
static MsToken scanOperator(MsLexer* lex) {
  uint32_t start = lex->pos;
  char c = advance(lex);  // 消耗当前字节
  char n = peek(lex);     // 探测下一字节（不消耗）
  switch (c) {
  case '+':
    if (n == '+') { advance(lex); return tok(TOK_INC, start, lex); }
    if (n == '=') { advance(lex); return tok(TOK_PLUS_ASSIGN, start, lex); }
    return tok(TOK_PLUS, start, lex);
  case '-':
    if (n == '-') { advance(lex); return tok(TOK_DEC, start, lex); }
    if (n == '=') { advance(lex); return tok(TOK_MINUS_ASSIGN, start, lex); }
    return tok(TOK_MINUS, start, lex);
  case '*':
    if (n == '*') {
      advance(lex);
      char n2 = peek(lex);
      if (n2 == '=') { advance(lex); }  // **= 不在规范中，留作错误
      return tok(TOK_STARSTAR, start, lex);
    }
    if (n == '=') { advance(lex); return tok(TOK_STAR_ASSIGN, start, lex); }
    return tok(TOK_STAR, start, lex);
  case '/':
    if (n == '=') { advance(lex); return tok(TOK_SLASH_ASSIGN, start, lex); }
    return tok(TOK_SLASH, start, lex);
  case '%':
    if (n == '=') { advance(lex); return tok(TOK_PERCENT_ASSIGN, start, lex); }
    return tok(TOK_PERCENT, start, lex);
  case '<':
    if (n == '<') {
      advance(lex);
      if (peek(lex) == '=') { advance(lex); return tok(TOK_SHL_ASSIGN, start, lex); }
      return tok(TOK_SHL, start, lex);
    }
    if (n == '-') { advance(lex); return tok(TOK_ARROW_LEFT, start, lex); }
    if (n == '=') { advance(lex); return tok(TOK_LE, start, lex); }
    return tok(TOK_LT, start, lex);
  case '>':
    if (n == '>') {
      advance(lex);
      if (peek(lex) == '=') { advance(lex); return tok(TOK_SHR_ASSIGN, start, lex); }
      return tok(TOK_SHR, start, lex);
    }
    if (n == '=') { advance(lex); return tok(TOK_GE, start, lex); }
    return tok(TOK_GT, start, lex);
  case '=':
    if (n == '=') { advance(lex); return tok(TOK_EQ, start, lex); }
    return tok(TOK_ASSIGN, start, lex);
  case '!':
    if (n == '=') { advance(lex); return tok(TOK_NEQ, start, lex); }
    return tok(TOK_ERROR, start, lex);  // 单独 '!' 不合法
  case ':':
    if (n == '=') { advance(lex); return tok(TOK_COLON_ASSIGN, start, lex); }
    return tok(TOK_COLON, start, lex);
  case '&':
    if (n == '=') { advance(lex); return tok(TOK_AMP_ASSIGN, start, lex); }
    return tok(TOK_AMP, start, lex);
  case '|':
    if (n == '=') { advance(lex); return tok(TOK_PIPE_ASSIGN, start, lex); }
    return tok(TOK_PIPE, start, lex);
  case '^':
    if (n == '=') { advance(lex); return tok(TOK_CARET_ASSIGN, start, lex); }
    return tok(TOK_CARET, start, lex);
  case '~': return tok(TOK_TILDE, start, lex);
  case '.':
    if (n == '.') {
      advance(lex);
      if (peek(lex) == '.') { advance(lex); return tok(TOK_DOTDOTDOT, start, lex); }
      return tok(TOK_ERROR, start, lex);  // '..' 不合法
    }
    // '.' 后接数字→浮点（T009 负责）; 此处若 n 为数字应不走到这里
    return tok(TOK_DOT, start, lex);
  case ',': return tok(TOK_COMMA, start, lex);
  case ';': return tok(TOK_SEMICOLON, start, lex);
  case '(': return tok(TOK_LPAREN, start, lex);
  case ')': return tok(TOK_RPAREN, start, lex);
  case '[': return tok(TOK_LBRACKET, start, lex);
  case ']': return tok(TOK_RBRACKET, start, lex);
  case '{': return tok(TOK_LBRACE, start, lex);
  case '}': return tok(TOK_RBRACE, start, lex);
  default:  return tok(TOK_ERROR, start, lex);
  }
}
```

---

## 验收标准（checklist）

- [ ] `+` → `TOK_PLUS`，`++` → `TOK_INC`，`+=` → `TOK_PLUS_ASSIGN`。
- [ ] `**` → `TOK_STARSTAR`，`*` → `TOK_STAR`，`*=` → `TOK_STAR_ASSIGN`。
- [ ] `<-` → `TOK_ARROW_LEFT`，`<=` → `TOK_LE`，`<<` → `TOK_SHL`，`<<=` → `TOK_SHL_ASSIGN`。
- [ ] `>>` → `TOK_SHR`，`>>=` → `TOK_SHR_ASSIGN`，`>=` → `TOK_GE`。
- [ ] `:=` → `TOK_COLON_ASSIGN`，`:` → `TOK_COLON`。
- [ ] `...` → `TOK_DOTDOTDOT`，`.` → `TOK_DOT`（后无数字时）。
- [ ] `..` → `TOK_ERROR`（非法，`..` 仅在 import 的相对路径中使用，但那是标识符层面的解析，不是单独 token）。
- [ ] `!` → `TOK_ERROR`（单独 `!` 不合法，mslang 无 C 风格 `!`，逻辑非为 `not`）。
- [ ] `!=` → `TOK_NEQ`。
- [ ] 所有括号、逗号、分号单字符 token 各自正确。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_operators.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"

static void lexTok(const char* src, MsTokKind expected) {
  MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  MsToken t = msLexNext(&lex);
  MS_ASSERT_EQ(t.kind, expected, src);
}

static void testArithmetic(void) {
  lexTok("+",   TOK_PLUS);
  lexTok("++",  TOK_INC);
  lexTok("+=",  TOK_PLUS_ASSIGN);
  lexTok("-",   TOK_MINUS);
  lexTok("--",  TOK_DEC);
  lexTok("-=",  TOK_MINUS_ASSIGN);
  lexTok("**",  TOK_STARSTAR);
  lexTok("*=",  TOK_STAR_ASSIGN);
  lexTok("/=",  TOK_SLASH_ASSIGN);
  lexTok("%=",  TOK_PERCENT_ASSIGN);
}

static void testBitwise(void) {
  lexTok("&=",  TOK_AMP_ASSIGN);
  lexTok("|=",  TOK_PIPE_ASSIGN);
  lexTok("^=",  TOK_CARET_ASSIGN);
  lexTok("<<=", TOK_SHL_ASSIGN);
  lexTok(">>=", TOK_SHR_ASSIGN);
}

static void testArrows(void) {
  lexTok("<-",  TOK_ARROW_LEFT);
  lexTok("<=",  TOK_LE);
  lexTok("<<",  TOK_SHL);
  lexTok("...", TOK_DOTDOTDOT);
  lexTok(":=",  TOK_COLON_ASSIGN);
  lexTok("!=",  TOK_NEQ);
}

int main(void) {
  MS_RUN(testArithmetic);
  MS_RUN(testBitwise);
  MS_RUN(testArrows);
  return msTestSummary();
}
```

---

## .ms 使用示例

N/A（词法层，用 `mslang tokens` 验证）。

---

## Benchmark

N/A（归入 T016 词法整体 bench）。

---

## 风险与边界

- **`//` 注释**：`/` 后跟 `/` 进入注释扫描（T014），不产生 `TOK_SLASH`。`scanOperator` 在 `/` 情况下需先检查 `n == '/'`（跳过注释），再检查 `n == '='`。
- **`$` 字符**：`$` 在 `$"` 情况由 T011 处理；其他位置（如 `$x`）产生 `TOK_ERROR`（`syntax.md §1.8.1`）。
- **`!` 不合法**：mslang 无 `!`（逻辑非为关键字 `not`，不等于 `!=` 已处理）；单独 `!` 应产生 `TOK_ERROR` 以便给出友好错误消息（"use 'not' for logical negation"）。
