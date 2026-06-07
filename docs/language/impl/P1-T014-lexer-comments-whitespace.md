# P1-T014 注释（`//`）与空白处理

> **状态**：⬜ 未开始

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
| `c-style.md` | §注释规范（仅 `//`） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c   # 在 msLexNext 顶部添加跳过空白/注释的循环
```

---

## 实现要点

1. **`msLexNext` 主循环前置跳过**：

```c
MsToken msLexNext(MsLexer* lex) {
  // 跳过空白与注释（循环，因注释后可能仍有空白）
top:
  while (lex->pos < lex->srcLen) {
    char c = lex->src[lex->pos];
    if (c == ' ' || c == '\t' || c == '\r') {
      lex->pos++;
      continue;
    }
    if (c == '\n') {
      // 换行由 T015 的 ASI 逻辑处理，此处仅更新行号
      lex->pos++;
      lex->line++;
      lex->lineStart = lex->pos;
      // 若 ASI 应插入分号，由 T015 在此处返回 TOK_NEWLINE
      continue; // 或 T015：return tokNewline();
    }
    if (c == '/' && lex->pos + 1 < lex->srcLen && lex->src[lex->pos + 1] == '/') {
      // 跳过注释：到行尾（不消耗 '\n'，留给行号追踪与 ASI）
      lex->pos += 2;
      while (lex->pos < lex->srcLen && lex->src[lex->pos] != '\n') {
        lex->pos++;
      }
      goto top;
    }
    break;  // 到达有效字节
  }
  // … 继续正常 token 扫描
}
```

2. **`\r\n` 处理**：`\r` 单独跳过（视为空白）；`\r\n` 中 `\r` 被跳过，`\n` 触发行号递增。这样在 Windows 源文件下行号仍然正确。
3. **块注释不支持**：`/* … */` 不是合法注释；`/` 后跟 `*` 产生 `TOK_SLASH` + `TOK_STAR`（或 `TOK_ERROR` 提示"use // for comments"）。初版按正常运算符处理（`/*` 会被 parser 报语法错误），不需要词法层专门检测。

---

## 验收标准（checklist）

- [ ] `"// comment\nx"` → 跳过注释，返回 `TOK_IDENT("x")`（行 2）。
- [ ] `"  \t  x"` → 跳过空白，返回 `TOK_IDENT("x")`。
- [ ] `"// comment"` 无换行结尾 → 仅返回 `TOK_EOF`。
- [ ] 注释后紧跟换行，行号正确递增。
- [ ] `"x // comment\ny"` → `TOK_IDENT("x")`（行 1）、`TOK_IDENT("y")`（行 2）（中间可能有 ASI，T015 负责）。
- [ ] `"\r\n"` 只触发一次行号递增。
- [ ] `"/* block */"` 不报词法错误，而是产生 `TOK_SLASH`/`TOK_STAR`/…（由 parser 报错）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_comments_whitespace.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"

static void testCommentSkip(void) {
  const char* src = "// comment\nx";
  MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  // 跳过注释（T015 会在换行处插入 ASI，但此处 T014 层面仅验证注释跳过）
  MsToken t;
  do { t = msLexNext(&lex); } while (t.kind == TOK_NEWLINE);
  MS_ASSERT_EQ(t.kind, TOK_IDENT, "ident after comment");
  MS_ASSERT_EQ(t.pos.line, 2, "on line 2");
}

static void testWhitespaceSkip(void) {
  const char* src = "   \t  x";
  MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  MsToken t = msLexNext(&lex);
  MS_ASSERT_EQ(t.kind, TOK_IDENT, "ident after whitespace");
}

static void testMultiLineComment(void) {
  const char* src = "a\n// c1\n// c2\nb";
  MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  MsToken t1 = msLexNext(&lex);
  MS_ASSERT_EQ(t1.kind, TOK_IDENT, "a");
  MS_ASSERT_EQ(t1.pos.line, 1, "line 1");
  // 跳过可能的 TOK_NEWLINE
  MsToken t2;
  do { t2 = msLexNext(&lex); } while (t2.kind == TOK_NEWLINE);
  MS_ASSERT_EQ(t2.kind, TOK_IDENT, "b");
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

- **注释与 BOM**：源文件 "UTF-8 无 BOM"（`syntax.md §1.1`）；若源文件以 BOM（`0xEF 0xBB 0xBF`）开头，词法器报 `TOK_ERROR` 或静默跳过（初版静默跳过，与主流实现一致）。
- **末行无换行**：文件最后一行可能无 `\n`；词法器通过 `pos >= srcLen` 检测 EOF，不需要 `\n` 终止。
