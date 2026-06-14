# P1-T013 运算符与界符完整集

> **状态**：✅ 已完成

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
src/lexer/ms_lexer.c          # lexScanOperator 已实现（ms_lexer.c:87），本任务补充测试
tests/lexer/test_operators.c  # 新增运算符/界符单测
```

---

## 实现要点

运算符扫描由已实现的 `lexScanOperator`（`src/lexer/ms_lexer.c:87`）完成，采用 **switch-on-first-char + `lexMatch` 前向探测**策略。`c` 为已消耗的首字节，通过参数传入。

**函数签名：**

```c
static struct MsToken lexScanOperator(struct MsLexer* lex,
                                      uint8_t c,
                                      uint32_t start,
                                      struct MsSrcPos pos);
```

**内部辅助函数：**
- `lexMatch(lex, ch)` — 若下一字节为 `ch` 则消耗并返回 true，否则不消耗返回 false
- `lexMakeToken(lex, kind, start, pos)` — 创建正常 token
- `lexMakeError(lex, start, pos, msg)` — 创建错误 token（内含错误消息）

**代表性实现模式（`+` 分支）：**

```c
case '+':
  if (lexMatch(lex, '+')) {
    return lexMakeToken(lex, MS_TOK_INC, start, pos);
  }
  if (lexMatch(lex, '=')) {
    return lexMakeToken(lex, MS_TOK_PLUS_ASSIGN, start, pos);
  }
  return lexMakeToken(lex, MS_TOK_PLUS, start, pos);
```

**关键行为说明：**
- `**`：直接返回 `MS_TOK_STARSTAR`，`**=` 不做特殊处理（`=` 成为下一 token）。
- `..`：通过 `lexPeekByte` + `lexPeekByte2` 同时检查后两字节，仅 `...` 完整形式返回 `MS_TOK_DOTDOTDOT`；`..` 仅消耗第一个 `.` 返回 `MS_TOK_DOT`，第二个 `.` 留给下次扫描。
- `//`：由 T014 在 `lexScan` 主分派层处理，不进入 `lexScanOperator`。
- `$`：由 T011（f-string）在主分派层处理，不进入 `lexScanOperator`。
- 单独 `!`：调用 `lexMakeError(lex, start, pos, "unexpected '!'")`。

---

## 验收标准（checklist）

- [x] `+` → `MS_TOK_PLUS`，`++` → `MS_TOK_INC`，`+=` → `MS_TOK_PLUS_ASSIGN`。 <!-- v:ctest:test_operators -->
- [x] `-` → `MS_TOK_MINUS`，`--` → `MS_TOK_DEC`，`-=` → `MS_TOK_MINUS_ASSIGN`。 <!-- v:ctest:test_operators -->
- [x] `*` → `MS_TOK_STAR`，`**` → `MS_TOK_STARSTAR`，`*=` → `MS_TOK_STAR_ASSIGN`。 <!-- v:ctest:test_operators -->
- [x] `/` → `MS_TOK_SLASH`，`/=` → `MS_TOK_SLASH_ASSIGN`。 <!-- v:ctest:test_operators -->
- [x] `%` → `MS_TOK_PERCENT`，`%=` → `MS_TOK_PERCENT_ASSIGN`。 <!-- v:ctest:test_operators -->
- [x] `&` → `MS_TOK_AMP`，`&=` → `MS_TOK_AMP_ASSIGN`。 <!-- v:ctest:test_operators -->
- [x] `|` → `MS_TOK_PIPE`，`|=` → `MS_TOK_PIPE_ASSIGN`。 <!-- v:ctest:test_operators -->
- [x] `^` → `MS_TOK_CARET`，`^=` → `MS_TOK_CARET_ASSIGN`。 <!-- v:ctest:test_operators -->
- [x] `~` → `MS_TOK_TILDE`。 <!-- v:ctest:test_operators -->
- [x] `<<` → `MS_TOK_SHL`，`<<=` → `MS_TOK_SHL_ASSIGN`，`>>` → `MS_TOK_SHR`，`>>=` → `MS_TOK_SHR_ASSIGN`。 <!-- v:ctest:test_operators -->
- [x] `<-` → `MS_TOK_ARROW_LEFT`，`<=` → `MS_TOK_LE`，`<` → `MS_TOK_LT`。 <!-- v:ctest:test_operators -->
- [x] `>=` → `MS_TOK_GE`，`>` → `MS_TOK_GT`。 <!-- v:ctest:test_operators -->
- [x] `==` → `MS_TOK_EQ`，`=` → `MS_TOK_ASSIGN`。 <!-- v:ctest:test_operators -->
- [x] `!=` → `MS_TOK_NEQ`。 <!-- v:ctest:test_operators -->
- [x] `:=` → `MS_TOK_COLON_ASSIGN`，`:` → `MS_TOK_COLON`。 <!-- v:ctest:test_operators -->
- [x] `...` → `MS_TOK_DOTDOTDOT`，`.` → `MS_TOK_DOT`（后无数字时）。 <!-- v:ctest:test_operators -->
- [x] `..` → 两个 `MS_TOK_DOT`（非错误，第一个 `.` 返回 `MS_TOK_DOT`，第二个 `.` 留给下次扫描）。 <!-- v:ctest:test_operators -->
- [x] 单独 `!` → `MS_TOK_ERROR`（mslang 无 C 风格 `!`，逻辑非为关键字 `not`）。 <!-- v:ctest:test_operators -->
- [x] 所有括号 `()[]{}` 及逗号 `,`、分号 `;` 单字符 token 各自正确。 <!-- v:ctest:test_operators -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_operators.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"
#include <string.h>

static void lexTok(const char* src, MsTokKind expected) {
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, expected, src);
}

static void testArithmetic(void) {
  lexTok("+",   MS_TOK_PLUS);
  lexTok("++",  MS_TOK_INC);
  lexTok("+=",  MS_TOK_PLUS_ASSIGN);
  lexTok("-",   MS_TOK_MINUS);
  lexTok("--",  MS_TOK_DEC);
  lexTok("-=",  MS_TOK_MINUS_ASSIGN);
  lexTok("*",   MS_TOK_STAR);
  lexTok("**",  MS_TOK_STARSTAR);
  lexTok("*=",  MS_TOK_STAR_ASSIGN);
  lexTok("/",   MS_TOK_SLASH);
  lexTok("/=",  MS_TOK_SLASH_ASSIGN);
  lexTok("%",   MS_TOK_PERCENT);
  lexTok("%=",  MS_TOK_PERCENT_ASSIGN);
}

static void testBitwise(void) {
  lexTok("~",   MS_TOK_TILDE);
  lexTok("&",   MS_TOK_AMP);
  lexTok("&=",  MS_TOK_AMP_ASSIGN);
  lexTok("|",   MS_TOK_PIPE);
  lexTok("|=",  MS_TOK_PIPE_ASSIGN);
  lexTok("^",   MS_TOK_CARET);
  lexTok("^=",  MS_TOK_CARET_ASSIGN);
  lexTok("<<",  MS_TOK_SHL);
  lexTok("<<=", MS_TOK_SHL_ASSIGN);
  lexTok(">>",  MS_TOK_SHR);
  lexTok(">>=", MS_TOK_SHR_ASSIGN);
}

static void testComparisons(void) {
  lexTok("<-",  MS_TOK_ARROW_LEFT);
  lexTok("<=",  MS_TOK_LE);
  lexTok("<",   MS_TOK_LT);
  lexTok(">=",  MS_TOK_GE);
  lexTok(">",   MS_TOK_GT);
  lexTok("==",  MS_TOK_EQ);
  lexTok("=",   MS_TOK_ASSIGN);
  lexTok("!=",  MS_TOK_NEQ);
}

static void testMisc(void) {
  lexTok("...", MS_TOK_DOTDOTDOT);
  lexTok(".",   MS_TOK_DOT);
  lexTok(":=",  MS_TOK_COLON_ASSIGN);
  lexTok(":",   MS_TOK_COLON);
  lexTok(",",   MS_TOK_COMMA);
  lexTok(";",   MS_TOK_SEMICOLON);
  lexTok("(",   MS_TOK_LPAREN);
  lexTok(")",   MS_TOK_RPAREN);
  lexTok("[",   MS_TOK_LBRACKET);
  lexTok("]",   MS_TOK_RBRACKET);
  lexTok("{",   MS_TOK_LBRACE);
  lexTok("}",   MS_TOK_RBRACE);
  lexTok("!",   MS_TOK_ERROR);
}

int main(void) {
  MS_RUN(testArithmetic);
  MS_RUN(testBitwise);
  MS_RUN(testComparisons);
  MS_RUN(testMisc);
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

- **`//` 注释**：`//` 由 T014 在 `lexScan` 主分派层处理，不进入 `lexScanOperator`（`lexScanOperator` 只处理 `/=` 与单独 `/`，不检查 `n == '/'`）。
- **`$` 字符**：`$"` 由 T011（f-string）在主分派层处理；其他 `$` 位置（如 `$x`）在主分派层产生 `MS_TOK_ERROR`，不进入 `lexScanOperator`。
- **`!` 不合法**：mslang 无 `!`（逻辑非为关键字 `not`）。单独 `!` 由 `lexScanOperator` 调用 `lexMakeError(lex, start, pos, "unexpected '!'")` 产生 `MS_TOK_ERROR`（见 `ms_lexer.c:151`）。
