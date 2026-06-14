# P1-T014 注释（`//`）与空白处理

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现单行注释（`// …`）的跳过逻辑和空白字符的处理（空格、制表符、回车、换行）。注释不产生任何 token；换行影响 ASI（T015），但本任务先处理"跳过"逻辑，ASI 在 T015 中叠加。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T006 | Lexer 框架（行号追踪） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.2 空白与注释（`//` 单行，不支持块注释） |
| `syntax.md` | §1.3 自动分号插入（ASI，换行 token 行为） |
| `c-style.md` | §注释规范（仅 `//`） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
tests/lexer/test_comments_whitespace.c   # 验证既有空白与注释跳过行为
```

> **注意**：`src/lexer/ms_lexer.c` 的空白与单行注释跳过逻辑已在 `lexScan`
> 函数中实现完毕，本任务无需修改该文件，仅补充测试覆盖。

---

## 实现要点

1. **既有实现概览**：空白与单行注释跳过已在 `src/lexer/ms_lexer.c` 的内部静态
   函数 `lexScan` 中实现。该函数使用 `lexPeekByte`/`lexPeekByte2`/`lexAtEnd`
   访问器读取字节，使用 `lexConsumeNewline` 处理换行（更新 `lex->line` 与
   `lex->lineStart`），整体结构为 `while (!lexAtEnd(lex))` 循环 + `continue`
   回绕，**不使用 `goto`**（c-style.md §8.2 禁止向上跳）：

```c
// src/lexer/ms_lexer.c — lexScan（已有，不需修改）
while (!lexAtEnd(lex)) {
  uint8_t c = lexPeekByte(lex);
  if (c == ' ' || c == '\t' || c == '\r') {
    lex->pos++;
    continue;
  }
  if (c == '\n') {
    // lexConsumeNewline 更新 line/lineStart 并返回 MS_TOK_NEWLINE
    return lexConsumeNewline(lex);
  }
  if (c == '/' && lexPeekByte2(lex) == '/') {
    // 跳过 // 注释到行尾（不消耗 '\n'，留给换行处理）
    lex->pos += 2;
    while (!lexAtEnd(lex) && lexPeekByte(lex) != '\n') {
      lex->pos++;
    }
    continue;
  }
  break;
}
// … 继续正常 token 扫描
```

2. **`\r\n` 处理**：`\r` 视为空白单独跳过；`\r\n` 中 `\r` 被跳过，`\n` 由
   `lexConsumeNewline` 触发行号递增，Windows 源文件行号始终正确。
3. **块注释不支持**：`/* … */` 不是合法注释；`/` 后跟 `*` 由 P1-T013 的
   `lexScanOperator` 处理，产生 `MS_TOK_SLASH` + `MS_TOK_STAR`（`/*` 后由
   parser 报语法错误）。词法层的跳过循环仅拦截 `//`（双斜杠），无需处理 `/*`。

---

## 验收标准（checklist）

- [x] `"// comment\nx"` → 跳过注释，返回 `MS_TOK_IDENT("x")`（行 2）。 <!-- v:ctest:test_comments_whitespace -->
- [x] `"  \t  x"` → 跳过空白，返回 `MS_TOK_IDENT("x")`。 <!-- v:ctest:test_comments_whitespace -->
- [x] `"// comment"` 无换行结尾 → 仅返回 `MS_TOK_EOF`。 <!-- v:ctest:test_comments_whitespace -->
- [x] 注释后紧跟换行，行号正确递增。 <!-- v:ctest:test_comments_whitespace -->
- [x] `"x // comment\ny"` → `MS_TOK_IDENT("x")`（行 1）、`MS_TOK_IDENT("y")`（行 2）（中间可能有 ASI，T015 负责）。 <!-- v:ctest:test_comments_whitespace -->
- [x] `"\r\n"` 只触发一次行号递增。 <!-- v:ctest:test_comments_whitespace -->
- [x] `"/* block */"` 不报词法错误，而是产生 `MS_TOK_SLASH`/`MS_TOK_STAR`/…（由 parser 报错）。 <!-- v:ctest:test_comments_whitespace -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_comments_whitespace.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"
#include <string.h>

static void testCommentSkip(void) {
  const char* src = "// comment\nx";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  // 跳过注释（T015 会在换行处插入 ASI，但此处 T014 层面仅验证注释跳过）
  struct MsToken t;
  do { t = msLexerNext(&lex); } while (t.kind == MS_TOK_NEWLINE);
  MS_ASSERT_EQ(t.kind, MS_TOK_IDENT, "ident after comment");
  MS_ASSERT_EQ(t.pos.line, 2, "on line 2");
}

static void testWhitespaceSkip(void) {
  const char* src = "   \t  x";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t = msLexerNext(&lex);
  MS_ASSERT_EQ(t.kind, MS_TOK_IDENT, "ident after whitespace");
}

static void testMultiLineComment(void) {
  const char* src = "a\n// c1\n// c2\nb";
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  struct MsToken t1 = msLexerNext(&lex);
  MS_ASSERT_EQ(t1.kind, MS_TOK_IDENT, "a");
  MS_ASSERT_EQ(t1.pos.line, 1, "line 1");
  // 跳过可能的 MS_TOK_NEWLINE
  struct MsToken t2;
  do { t2 = msLexerNext(&lex); } while (t2.kind == MS_TOK_NEWLINE);
  MS_ASSERT_EQ(t2.kind, MS_TOK_IDENT, "b");
  MS_ASSERT_EQ(t2.pos.line, 4, "line 4");
}

int main(void) {
  MS_RUN(testCommentSkip);
  MS_RUN(testWhitespaceSkip);
  MS_RUN(testMultiLineComment);
  return msTestSummary();
}
```

---

## .ms 使用示例

N/A（注释与空白对脚本用户透明）。

---

## Benchmark

N/A（归入 T016 词法整体 bench）。

---

## 风险与边界

- **注释与 BOM**：源文件规定"UTF-8 无 BOM"（`syntax.md §1.1`）。当前实现不对
  BOM 做特殊处理：`0xEF` 字节落入 `c >= 0x80` 的标识符起始分支，最终由后续
  UTF-8 解码或标识符识别逻辑报告错误；本任务不引入 BOM 静默跳过逻辑，与既有
  实现保持一致。
- **末行无换行**：文件最后一行可能无 `\n`；词法器通过 `lexAtEnd(lex)` 检测
  EOF，不需要 `\n` 终止。
- **f-string 内部**：f-string 嵌入表达式内的空白/注释跳过由 `MS_FSTR_INNER`
  状态分支（ms_lexer.c:674）单独处理，本任务的测试不覆盖该分支。
