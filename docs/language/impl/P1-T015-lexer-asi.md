# P1-T015 自动分号插入（ASI）规则

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现 Go 风格的自动分号插入（Automatic Semicolon Insertion, ASI）：在特定 token 之后出现换行时，词法器虚拟插入 `MS_TOK_NEWLINE`（相当于分号）。Parser 使用 `MS_TOK_NEWLINE` 作为语句分隔符，从而支持省略行尾分号。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T014 | 注释与空白处理（换行行号更新）⚠ 本任务将改写 `lexScan` 的 `\n` 分支：T014 建立的"每个 `\n` 无条件产 `MS_TOK_NEWLINE`"行为将替换为"仅 ASI 触发时产 `MS_TOK_NEWLINE`"，T014 的相关测试需同步复核 |
| P1-T006 | Lexer 框架（`hasPeek`/`peek` 机制） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.3 自动分号插入规则 |
| `syntax.md` | §2.1 语句（语句以换行或 `;` 结束） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/lexer/ms_lexer.c      # 改造 lexScan 的 \n 分支，添加 asiShouldInsert()；
                           # 在 msLexerNext 出口更新 lex->prevKind
include/mslang/ms_lexer.h # struct MsLexer 新增 MsTokKind prevKind 字段；
                           # msLexerInit 初始化 lex->prevKind = MS_TOK_EOF
```

### 关键函数

```c
// 判断上一个返回的 token 是否触发 ASI（syntax.md §1.3）
static bool asiShouldInsert(MsTokKind prev);
```

---

## 实现要点

### ASI 触发 token（`syntax.md §1.3`）

换行前最后一个 token 属于下列类型时，词法器插入 `MS_TOK_NEWLINE`（依据
`syntax.md §1.3`，`pass` 与 `...` 为本任务落地时同步扩展至 §1.3 的两项）：

| 分类 | Token 种类 |
|---|---|
| 标识符 | `MS_TOK_IDENT` |
| 字面量 | `MS_TOK_INT`, `MS_TOK_FLOAT`, `MS_TOK_STRING`, `MS_TOK_BYTES`, `MS_TOK_FSTRING_END`, `MS_TOK_TRUE`, `MS_TOK_FALSE`, `MS_TOK_NIL` |
| 关键字 | `MS_TOK_RETURN`, `MS_TOK_BREAK`, `MS_TOK_CONTINUE`, `MS_TOK_FALLTHROUGH`, `MS_TOK_PASS` |
| 后缀运算符 | `MS_TOK_INC`（`++`）, `MS_TOK_DEC`（`--`） |
| 右括号/右界符 | `MS_TOK_RPAREN`（`)`）, `MS_TOK_RBRACKET`（`]`）, `MS_TOK_RBRACE`（`}`）, `MS_TOK_DOTDOTDOT`（`...`） |

```c
static bool asiShouldInsert(MsTokKind prev) {
  switch (prev) {
    case MS_TOK_IDENT:
    case MS_TOK_INT:    case MS_TOK_FLOAT:
    case MS_TOK_STRING: case MS_TOK_BYTES: case MS_TOK_FSTRING_END:
    case MS_TOK_TRUE:   case MS_TOK_FALSE: case MS_TOK_NIL:
    case MS_TOK_RETURN: case MS_TOK_BREAK: case MS_TOK_CONTINUE:
    case MS_TOK_FALLTHROUGH: case MS_TOK_PASS:
    case MS_TOK_INC:    case MS_TOK_DEC:
    case MS_TOK_RPAREN: case MS_TOK_RBRACKET: case MS_TOK_RBRACE:
    case MS_TOK_DOTDOTDOT:
      return true;
    default:
      return false;
  }
}
```

### 改造 `lexScan` 的 `\n` 分支

ASI 实现**不新写** `msLexerNext` 循环，而是修改 `src/lexer/ms_lexer.c` 中已有的
`lexScan`（行 654）和 `msLexerNext`（行 831）两处：

**1. `lexScan` 的 `\n` 分支**（将现有无条件返回 NEWLINE 改为 ASI 条件判定）：

```c
// src/lexer/ms_lexer.c — lexScan 的 \n 分支（改造后）
// 注意：跳过空白和 // 注释的 while 循环在此处之前，不修改 prevKind
if (c == '\n') {
  uint32_t nlStart = start;
  struct MsSrcPos nlPos = pos;
  lexConsumeNewline(lex);           // 更新 line/lineStart
  if (asiShouldInsert(lex->prevKind)) {
    return lexMakeToken(lex, MS_TOK_NEWLINE, nlStart, nlPos);
  }
  // 非 ASI 换行：跳过继续扫描（与空白相同）
  goto rescan;
}
```

其中 `rescan:` 标签放在 `lexScan` 开头的跳过循环之前（向下跳，符合 c-style.md §8.2
"goto 仅向后跳/cleanup 模式"）；或者以 `while(1)` 包裹整个跳过+扫描逻辑，以
`continue` 替代 `goto rescan`。

**2. `msLexerNext` 出口更新 `prevKind`**：

```c
// src/lexer/ms_lexer.c — msLexerNext（改造后）
struct MsToken msLexerNext(struct MsLexer* lex) {
  struct MsToken t;
  if (lex->hasPeek) {
    lex->hasPeek = false;
    t = lex->peek;
  } else {
    t = lexScan(lex);
  }
  // prevKind 在 msLexerNext 出口（而非 lexScan 内部）更新，
  // 确保 msLexerPeek 调用 lexScan 时不会提前推进 prevKind。
  if (t.kind != MS_TOK_NEWLINE) {
    lex->prevKind = t.kind;
  }
  return t;
}
```

**关键点**：`struct MsLexer` 需新增 `MsTokKind prevKind` 字段（放在 `hasPeek` 附近），
`msLexerInit` 中初始化为 `lex->prevKind = MS_TOK_EOF`（不触发 ASI）。

### 多个连续换行

连续多行空行（或仅含注释的行）只插入最多一个 `MS_TOK_NEWLINE`。ASI 触发后
`prevKind` 不更新（NEWLINE 本身不修改 `prevKind`，见 `msLexerNext` 出口逻辑），
后续换行仍以前一有效 token 判断——但由于第一个 NEWLINE 已被 parser 消费，后续
连续换行的前一 token 仍为同一个 ASI 触发 token，会继续产 NEWLINE。因此应在
`asiShouldInsert` 增加判断：`prev == MS_TOK_NEWLINE` 时返回 `false`，实现"最多
一个连续 NEWLINE"的折叠。

### `msLexerNextSkipNewline`

`msLexerNextSkipNewline`（ms_lexer.c:847）**已在框架阶段实现**，供 parser 在已知
不需要 ASI 的位置使用（如 `(` 内部的换行应忽略）。本任务仅需确认其在 ASI 生效后
行为仍然正确——它循环调用 `msLexerNext` 直到得到非 `MS_TOK_NEWLINE` token，无需修改：

```c
// ms_lexer.c:847 — 已有，不需修改
struct MsToken msLexerNextSkipNewline(struct MsLexer* lex) {
  struct MsToken t;
  do {
    t = msLexerNext(lex);
  } while (t.kind == MS_TOK_NEWLINE);
  return t;
}
```

---

## 验收标准（checklist）

- [x] `"x\ny"` → `MS_TOK_IDENT("x")`, `MS_TOK_NEWLINE`, `MS_TOK_IDENT("y")`。 <!-- v:ctest:test_asi -->
- [x] `"x\n\n\ny"` → `MS_TOK_IDENT("x")`, `MS_TOK_NEWLINE`, `MS_TOK_IDENT("y")`（仅一个 NEWLINE）。 <!-- v:ctest:test_asi -->
- [x] `"return\nx"` → `MS_TOK_RETURN`, `MS_TOK_NEWLINE`, `MS_TOK_IDENT("x")`。 <!-- v:ctest:test_asi -->
- [x] `"(\nx\n)"` → `MS_TOK_LPAREN`, `MS_TOK_IDENT("x")`, `MS_TOK_NEWLINE`, `MS_TOK_RPAREN`（`(` 后换行不触发，`x` 后换行触发）。 <!-- v:ctest:test_asi -->
- [x] `"x // comment\ny"` → `MS_TOK_IDENT("x")`, `MS_TOK_NEWLINE`, `MS_TOK_IDENT("y")`（注释后换行触发 ASI；注释跳过在 `\n` 判定之前）。 <!-- v:ctest:test_asi -->
- [x] `"+\nx"` → `MS_TOK_PLUS`, `MS_TOK_IDENT("x")`（`+` 后换行不触发 ASI）。 <!-- v:ctest:test_asi -->
- [x] `"}\nx"` → `MS_TOK_RBRACE`, `MS_TOK_NEWLINE`, `MS_TOK_IDENT("x")`（`}` 触发 ASI）。 <!-- v:ctest:test_asi -->
- [x] `"...\nx"` → `MS_TOK_DOTDOTDOT`, `MS_TOK_NEWLINE`, `MS_TOK_IDENT("x")`（`...` 为 §1.3 扩展触发项）。 <!-- v:ctest:test_asi -->
- [x] `"x"` 无尾随换行 → `MS_TOK_IDENT("x")`, `MS_TOK_EOF`（无物理换行直接到 EOF，不插入 NEWLINE）。 <!-- v:ctest:test_asi -->
- [x] `"x\n"` 有尾随换行 → `MS_TOK_IDENT("x")`, `MS_TOK_NEWLINE`, `MS_TOK_EOF`（ASI 在 `\n` 处触发，再产 EOF）。 <!-- v:ctest:test_asi -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_asi.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"
#include <string.h>

// 辅助：收集 token 种类序列直到 MS_TOK_EOF（含）或 maxOut 满
static void collectKinds(const char* src, MsTokKind* out, int maxOut) {
  struct MsLexer lex;
  msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
  for (int i = 0; i < maxOut; i++) {
    struct MsToken t = msLexerNext(&lex);
    out[i] = t.kind;
    if (t.kind == MS_TOK_EOF) {
      break;
    }
  }
}

static void testSimpleASI(void) {
  MsTokKind kinds[8];
  collectKinds("x\ny", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_IDENT,    "x");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE,  "ASI");
  MS_ASSERT_EQ(kinds[2], MS_TOK_IDENT,    "y");
  MS_ASSERT_EQ(kinds[3], MS_TOK_EOF,      "eof");
}

static void testReturnASI(void) {
  MsTokKind kinds[8];
  collectKinds("return\nx", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_RETURN,  "return");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE, "ASI after return");
}

static void testNoASIInsideParen(void) {
  // '(' 不在 ASI 触发列表，换行被忽略；'x' 在列表中，其后换行触发 ASI
  MsTokKind kinds[8];
  collectKinds("(\nx\n)", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_LPAREN,  "lparen");
  MS_ASSERT_EQ(kinds[1], MS_TOK_IDENT,   "x (no ASI before x)");
  MS_ASSERT_EQ(kinds[2], MS_TOK_NEWLINE, "ASI after x");
  MS_ASSERT_EQ(kinds[3], MS_TOK_RPAREN,  "rparen");
}

static void testMultipleNewlines(void) {
  MsTokKind kinds[8];
  collectKinds("x\n\n\ny", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_IDENT,   "x");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE, "single ASI");
  MS_ASSERT_EQ(kinds[2], MS_TOK_IDENT,   "y");
}

static void testTrailingNewlineBeforeEof(void) {
  MsTokKind kinds[8];
  // 无尾随换行 — 不插入 NEWLINE
  collectKinds("x", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_IDENT, "x");
  MS_ASSERT_EQ(kinds[1], MS_TOK_EOF,   "eof directly");
  // 有尾随换行 — ASI 在 \n 处触发
  collectKinds("x\n", kinds, 8);
  MS_ASSERT_EQ(kinds[0], MS_TOK_IDENT,   "x");
  MS_ASSERT_EQ(kinds[1], MS_TOK_NEWLINE, "ASI from trailing newline");
  MS_ASSERT_EQ(kinds[2], MS_TOK_EOF,     "eof after ASI");
}

int main(void) {
  MS_RUN(testSimpleASI);
  MS_RUN(testReturnASI);
  MS_RUN(testNoASIInsideParen);
  MS_RUN(testMultipleNewlines);
  MS_RUN(testTrailingNewlineBeforeEof);
  return msTestSummary();
}
```

---

## .ms 使用示例

N/A（ASI 对脚本用户透明，体现在整体语法中）。

---

## Benchmark

N/A（归入 T016 词法整体 bench）。

---

## 风险与边界

- **`{` 开头的块**：`if cond\n{` → `cond` 触发 ASI，parser 在 `if` 右侧看到
  `MS_TOK_NEWLINE` 再看到 `{`，会导致语法错误（与 Go 完全相同；`syntax.md §1.3`
  明确 `{` 必须与 `if`/`func`/`for` 同行）。词法层无需特殊处理，行为自然正确。
- **`msLexerPeek` 与 ASI**：`msLexerPeek`（ms_lexer.c:839）内部调用 `lexScan`
  将下一 token 缓存到 `lex->peek`。由于 `prevKind` 在 `msLexerNext` 出口更新
  （而非在 `lexScan` 内），所以 peek 调用时 `lexScan` 读取的 `prevKind` 始终
  是上一次 `msLexerNext` 实际返回的 token——两条路径的 ASI 判定均正确：
  - 正常路径：`msLexerNext` → `lexScan` 遇 `\n`，读 `prevKind`=前一 token ✓
  - peek 路径：`msLexerPeek` → `lexScan` 遇 `\n`，读 `prevKind`=前一 token ✓
    → 随后 `msLexerNext` 从 `lex->peek` 返回 NEWLINE，更新 `prevKind` ✓
- **显式 `;`**：`MS_TOK_SEMICOLON` 也作语句分隔符；parser 应将 `MS_TOK_NEWLINE`
  与 `MS_TOK_SEMICOLON` 视为等价。
- **T014 测试复核**：T014 已有测试断言"任意 `\n` 都产 `MS_TOK_NEWLINE`"。本任务
  改变了该行为（非触发 token 后的 `\n` 被吞掉），须复核 T014 的 `testCommentSkip`
  / `testMultiLineComment` 等测试用例是否仍然有效，必要时调整断言。
