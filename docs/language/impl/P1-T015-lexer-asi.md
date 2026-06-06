# P1-T015 自动分号插入（ASI）规则

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 Go 风格的自动分号插入（Automatic Semicolon Insertion, ASI）：在特定 token 之后出现换行时，词法器虚拟插入 `TOK_NEWLINE`（相当于分号）。Parser 使用 `TOK_NEWLINE` 作为语句分隔符，从而支持省略行尾分号。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T014 | 注释与空白处理（换行行号更新） |
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
src/lexer/ms_lexer.c   # 修改换行处理逻辑，添加 asiShouldInsert()
include/mslang/ms_lexer.h # 确认 TOK_NEWLINE 已在枚举中
```

### 关键函数

```c
// 判断当前最后产生的 token 是否触发 ASI
// 遵循 syntax.md §1.3：若前一 token 是 ASI 触发 token，则在换行处插入 TOK_NEWLINE
static bool asiShouldInsert(MsTokKind prev);
```

---

## 实现要点

### ASI 触发 token（`syntax.md §1.3`）

换行前最后一个 token 属于下列类型时，词法器插入 `TOK_NEWLINE`：

| 分类 | Token 种类 |
|---|---|
| 标识符 | `TOK_IDENT` |
| 字面量 | `TOK_INT`, `TOK_FLOAT`, `TOK_STRING`, `TOK_BYTES`, `TOK_FSTRING_END`, `TOK_TRUE`, `TOK_FALSE`, `TOK_NIL` |
| 关键字 | `TOK_RETURN`, `TOK_BREAK`, `TOK_CONTINUE`, `TOK_PASS`, `TOK_FALLTHROUGH` |
| 关键字（值型） | `TOK_TRUE`, `TOK_FALSE`, `TOK_NIL` |
| 后缀运算符 | `TOK_INC`（`++`）, `TOK_DEC`（`--`） |
| 右括号/右界符 | `TOK_RPAREN`（`)`）, `TOK_RBRACKET`（`]`）, `TOK_RBRACE`（`}`）, `TOK_DOTDOTDOT`（`...`） |

```c
static bool asiShouldInsert(MsTokKind prev) {
    switch (prev) {
    case TOK_IDENT:
    case TOK_INT:    case TOK_FLOAT:
    case TOK_STRING: case TOK_BYTES: case TOK_FSTRING_END:
    case TOK_TRUE:   case TOK_FALSE: case TOK_NIL:
    case TOK_RETURN: case TOK_BREAK: case TOK_CONTINUE:
    case TOK_PASS:   case TOK_FALLTHROUGH:
    case TOK_INC:    case TOK_DEC:
    case TOK_RPAREN: case TOK_RBRACKET: case TOK_RBRACE:
    case TOK_DOTDOTDOT:
        return true;
    default:
        return false;
    }
}
```

### 集成到 `msLexNext`

```c
MsToken msLexNext(MsLexer* lex) {
top:
    while (lex->pos < lex->srcLen) {
        char c = lex->src[lex->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            lex->pos++; continue;
        }
        if (c == '\n') {
            lex->pos++;
            lex->line++;
            lex->lineStart = lex->pos;
            if (asiShouldInsert(lex->prevKind)) {
                // 返回虚拟分号 token
                MsToken t = { .kind = TOK_NEWLINE, .pos = {lex->fileName, lex->line - 1, 0} };
                lex->prevKind = TOK_NEWLINE;
                return t;
            }
            continue;
        }
        // ... 注释处理（T014）
        break;
    }
    // ... 正常扫描
    // 每次产生 token 后：lex->prevKind = t.kind;
    return t;
}
```

**关键点**：`MsLexer` 需要 `MsTokKind prevKind` 字段，记录上一次返回的 token 种类；初始化为 `TOK_EOF`（不触发 ASI）。

### 多个连续换行

连续多行空行（或仅含注释的行）只插入最多一个 `TOK_NEWLINE`（ASI 触发后，`prevKind` 置为 `TOK_NEWLINE`，后续换行不再触发）。

### `msLexNextSkipNewline`

API `msLexNextSkipNewline` 跳过所有 `TOK_NEWLINE`，供 parser 在已知不需要 ASI 的位置使用（如 `(` 内部的换行应忽略）：

```c
MsToken msLexNextSkipNewline(MsLexer* lex) {
    MsToken t;
    do { t = msLexNext(lex); } while (t.kind == TOK_NEWLINE);
    return t;
}
```

---

## 验收标准（checklist）

- [ ] `"x\ny"` → `TOK_IDENT("x")`, `TOK_NEWLINE`, `TOK_IDENT("y")`。
- [ ] `"x\n\n\ny"` → `TOK_IDENT("x")`, `TOK_NEWLINE`, `TOK_IDENT("y")`（仅一个 NEWLINE）。
- [ ] `"return\nx"` → `TOK_RETURN`, `TOK_NEWLINE`, `TOK_IDENT("x")`。
- [ ] `"(\nx\n)"` → `TOK_LPAREN`, `TOK_IDENT("x")`, `TOK_RPAREN`（`(` 后换行不触发，因为 `(` 不在 ASI 列表中）。
- [ ] `"x // comment\ny"` → `TOK_IDENT("x")`, `TOK_NEWLINE`, `TOK_IDENT("y")`（注释后换行触发 ASI）。
- [ ] `"+\nx"` → `TOK_PLUS`, `TOK_IDENT("x")`（`+` 后换行不触发 ASI）。
- [ ] `"}\nx"` → `TOK_RBRACE`, `TOK_NEWLINE`, `TOK_IDENT("x")`（`}` 触发 ASI）。
- [ ] `"...\nx"` → `TOK_DOTDOTDOT`, `TOK_NEWLINE`, `TOK_IDENT("x")`。
- [ ] 文件末尾 `TOK_EOF` 前，若最后 token 在 ASI 列表中，不自动插入 NEWLINE（EOF 已隐含语句结束）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/lexer/test_asi.c`）

```c
#include "ms_test.h"
#include "mslang/ms_lexer.h"

// 辅助：收集 token 种类序列
static void collectKinds(const char* src, MsTokKind* out, int maxOut) {
    MsLexer lex;
    msLexerInit(&lex, src, (uint32_t)strlen(src), "<t>");
    int i = 0;
    for (; i < maxOut; i++) {
        MsToken t = msLexNext(&lex);
        out[i] = t.kind;
        if (t.kind == TOK_EOF) break;
    }
}

static void testSimpleASI(void) {
    MsTokKind kinds[8];
    collectKinds("x\ny", kinds, 8);
    MS_ASSERT_EQ(kinds[0], TOK_IDENT,    "x");
    MS_ASSERT_EQ(kinds[1], TOK_NEWLINE,  "ASI");
    MS_ASSERT_EQ(kinds[2], TOK_IDENT,    "y");
    MS_ASSERT_EQ(kinds[3], TOK_EOF,      "eof");
}

static void testReturnASI(void) {
    MsTokKind kinds[8];
    collectKinds("return\nx", kinds, 8);
    MS_ASSERT_EQ(kinds[0], TOK_RETURN,  "return");
    MS_ASSERT_EQ(kinds[1], TOK_NEWLINE, "ASI after return");
}

static void testNoASIInsideParen(void) {
    // '(' 不在 ASI 触发列表，换行被忽略
    MsTokKind kinds[8];
    collectKinds("(\nx\n)", kinds, 8);
    MS_ASSERT_EQ(kinds[0], TOK_LPAREN,  "lparen");
    MS_ASSERT_EQ(kinds[1], TOK_IDENT,   "x (no ASI before x)");
    MS_ASSERT_EQ(kinds[2], TOK_NEWLINE, "ASI after x (x is in list)");
    MS_ASSERT_EQ(kinds[3], TOK_RPAREN,  "rparen");
}

static void testMultipleNewlines(void) {
    MsTokKind kinds[8];
    collectKinds("x\n\n\ny", kinds, 8);
    MS_ASSERT_EQ(kinds[0], TOK_IDENT,   "x");
    MS_ASSERT_EQ(kinds[1], TOK_NEWLINE, "single ASI");
    MS_ASSERT_EQ(kinds[2], TOK_IDENT,   "y");
}

int main(void) {
    MS_RUN(testSimpleASI);
    MS_RUN(testReturnASI);
    MS_RUN(testNoASIInsideParen);
    MS_RUN(testMultipleNewlines);
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

- **`{` 开头的块**：`if cond\n{` → `cond` 触发 ASI，但 parser 在 `if` 语句右侧看到 `TOK_NEWLINE` 再看到 `{`，需 parser 能处理这一序列（`syntax.md §1.3` 明确 `{` 必须与 `if`/`func`/`for` 等在同一行，如果源码换行写 `{`，会导致语法错误，与 Go 完全相同）。词法层无需特殊处理，行为自然正确。
- **`msLexPeek` 与 ASI**：`msLexPeek` 不推进 `prevKind`，调用后再调 `msLexNext` 时仍以旧 `prevKind` 判断 ASI，行为正确。
- **显式 `;`**：`TOK_SEMICOLON` 也作语句分隔符；parser 应将 `TOK_NEWLINE` 与 `TOK_SEMICOLON` 视为等价。
