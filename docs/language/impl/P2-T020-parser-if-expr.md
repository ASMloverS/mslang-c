# P2-T020 if 表达式（条件表达式）

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现 mslang 的条件表达式（"三目"语法）。mslang **无** C 风格 `?:` 三目运算符；根据 `syntax.md §2.3`，条件表达式采用 Python 风格的 `expr if cond else expr`，产生 `MS_ND_IF_EXPR` 节点。本任务在 Pratt 框架上注册 `MS_TOK_IF` 的中缀解析函数。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T017 | `MS_ND_IF_EXPR` 节点 |
| P2-T019 | 基础表达式前缀/中缀已注册 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3 表达式优先级表（`TernaryExpr = OrExpr [ 'if' Expr 'else' TernaryExpr ]`） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 注册 MS_TOK_IF 中缀解析函数 parseIfExpr
```

### `MsNode` 字段（`MS_ND_IF_EXPR`，已在 T017 定义）

T017 已在 `ms_ast.h` 中定义：

```c
// MS_ND_IF_EXPR — 已完成，直接使用：
// n->ifExpr.thenExpr  → 主值（条件为真时返回）
// n->ifExpr.cond      → 条件
// n->ifExpr.elseExpr  → 备选（条件为假时返回）
```

---

## 实现要点

### 语法形式

```
expr if cond else alt
```

- `expr`：主值（条件为真时返回）
- `cond`：条件（完整 Expr，`PREC_IF_EXPR` 级；syntax.md EBNF 规定为 `Expr`）
- `alt`：备选（条件为假时返回；`PREC_IF_EXPR` 级，右结合允许链式 `a if c1 else b if c2 else d`）

### 优先级

`PREC_IF_EXPR = 1` 已在 T018 的 `ms_parser.h` 枚举中预留（低于 `PREC_OR = 2`），**本任务无需改动枚举**。`msParseExpr` 已从 `PREC_IF_EXPR` 级别起始，条件表达式自然成为最低优先级。

### 注册与实现

```c
// 注册（放入 msParseExprRegisterRules()）：
// parserRegisterRule(MS_TOK_IF, NULL, parseIfExpr, PREC_IF_EXPR);

static MsNode* parseIfExpr(MsParser* p, MsNode* value) {
    // 已消耗 'if' token（p->prev.kind == MS_TOK_IF）
    MsSrcPos pos = p->prev.pos;

    // 条件：syntax.md EBNF 中为完整 Expr（TernaryExpr，PREC_IF_EXPR 级）
    MsNode* cond = parsePrecedence(p, PREC_IF_EXPR);

    msParserExpect(p, MS_TOK_ELSE, "expected 'else' after condition in if-expression");

    // 备选（右结合：允许 a if c1 else b if c2 else d）
    MsNode* alt = parsePrecedence(p, PREC_IF_EXPR);

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind              = MS_ND_IF_EXPR;
    n->pos               = pos;
    n->ifExpr.thenExpr   = value;
    n->ifExpr.cond       = cond;
    n->ifExpr.elseExpr   = alt;
    return n;
}
```

### 链式条件表达式（右结合）

```
a if c1 else b if c2 else d
```

解析为：`a if c1 else (b if c2 else d)`，通过 `alt = parsePrecedence(p, PREC_IF_EXPR)` 右结合实现（`alt` 侧从 `PREC_IF_EXPR` 起始，允许 `else` 后再接条件表达式）。

### 与 `if` 语句的区分

`if` 关键字在 Pratt 框架中注册为**中缀**（`infix`），用于条件表达式。`if` 关键字作为**语句开头**（前缀）时由 `msParseStmt` 单独处理（T027），不走 Pratt。

---

## 验收标准（checklist）

- [x] `"a if cond else b"` → `MS_ND_IF_EXPR(thenExpr=MS_ND_IDENT(a), cond=MS_ND_IDENT(cond), elseExpr=MS_ND_IDENT(b))`。
- [x] `"1 + 2 if x > 0 else 3"` → 根为 `MS_ND_IF_EXPR`，`thenExpr=MS_ND_BINARY(+,1,2)`。
- [x] `"a if c1 else b if c2 else d"` → 右结合：`elseExpr` 为另一 `MS_ND_IF_EXPR`。
- [x] `"if cond { }"` 在语句上下文中不触发 if-expr（`if` 作为语句，非中缀）。
- [x] 缺 `else` 时产生语法错误（"expected 'else'"）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_if_expr.c`）

```c
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "parser/ms_arena.h"

static MsNode* px(MsArena* a, const char* s) {
    MsParser p;
    msParserInit(&p, s, (uint32_t)strlen(s), "<t>", a);
    return msParseExpr(&p);
}

static void testIfExpr(void) {
    MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "x if cond else y");
    MS_ASSERT_EQ(n->kind,                   MS_ND_IF_EXPR, "if expr");
    MS_ASSERT_EQ(n->ifExpr.thenExpr->kind,  MS_ND_IDENT,   "thenExpr=ident");
    MS_ASSERT_EQ(n->ifExpr.cond->kind,      MS_ND_IDENT,   "cond=ident");
    MS_ASSERT_EQ(n->ifExpr.elseExpr->kind,  MS_ND_IDENT,   "elseExpr=ident");
    msArenaFree(&a);
}

static void testIfExprChained(void) {
    MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "a if c1 else b if c2 else d");
    MS_ASSERT_EQ(n->kind,                    MS_ND_IF_EXPR, "root if_expr");
    MS_ASSERT_EQ(n->ifExpr.elseExpr->kind,   MS_ND_IF_EXPR, "elseExpr is also if_expr (right-assoc)");
    msArenaFree(&a);
}

int main(void) {
    MS_RUN(testIfExpr);
    MS_RUN(testIfExprChained);
    return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
x := 10
sign := "positive" if x > 0 else "non-positive"
print(sign)    // positive

// 链式
grade := "A" if x >= 90 else "B" if x >= 80 else "C"
print(grade)   // C（x=10，均不满足）

// 嵌套使用
absVal := x if x >= 0 else -x
print(absVal)   // 10
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **无需调整枚举**：`PREC_IF_EXPR = 1` 已在 T018 中预留，T019 无需同步，直接使用即可。
- **无三目 `?:`**：mslang 明确不支持 C 风格三目；遇到 `?` 应产生 `MS_TOK_ERROR`（词法层，T013 已覆盖）。
- **if-expr vs if-stmt 消歧**：`MS_TOK_IF` 在表达式上下文（Pratt 中缀）触发 if-expr；在语句上下文（`msParseStmt` 首 token 为 `if`）触发 if-stmt。两者互不干扰。
