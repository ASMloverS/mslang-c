# P2-T020 if 表达式（条件表达式）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 mslang 的条件表达式（"三目"语法）。mslang **无** C 风格 `?:` 三目运算符；根据 `syntax.md §2.3.8`，条件表达式采用 Python 风格的 `expr if cond else expr`，产生 `ND_IF_EXPR` 节点。本任务在 Pratt 框架上注册 `TOK_IF` 的中缀解析函数。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T017 | `ND_IF_EXPR` 节点 |
| P2-T019 | 基础表达式前缀/中缀已注册 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3.8 条件表达式（`val if cond else alt`） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 注册 TOK_IF 中缀解析函数 parseIfExpr
```

### `MsNode` 字段（`ND_IF_EXPR`，已在 T017 定义）

```c
// 使用 if_stmt union（复用）：
// .if_stmt.cond       → 条件
// .if_stmt.then_block → 结果（真值侧，ND_IF_EXPR 中存 expr 而非 block）
// .if_stmt.else_block → 备选（假值侧）
// 更清晰做法：在 T017 的 ND_IF_EXPR 分支中单独定义字段：
// struct { MsNode* value; MsNode* cond; MsNode* alt; } if_expr;
```

---

## 实现要点

### 语法形式

```
expr if cond else alt
```

- `expr`：主值（条件为真时返回）
- `cond`：条件（优先级 `PREC_OR` 以上）
- `alt`：备选（条件为假时返回；优先级 `PREC_OR` 以上，右结合允许链式 `a if c1 else b if c2 else d`）

### 优先级

`if` 条件表达式的优先级低于 `or`（`PREC_IFEXPR = PREC_OR - 1` = 1，插入在 `PREC_ASSIGN` 与 `PREC_OR` 之间）。

调整优先级枚举（在 T018 的枚举中插入）：

```c
typedef enum Precedence {
  PREC_NONE      = 0,
  PREC_ASSIGN    = 1,
  PREC_IFEXPR    = 2,   // 新增：x if c else y
  PREC_OR        = 3,   // 原来 = 2，现在 = 3
  PREC_AND       = 4,
  PREC_NOT       = 5,
  PREC_COMPARE   = 6,
  PREC_BITOR     = 7,
  PREC_BITXOR    = 8,
  PREC_BITAND    = 9,
  PREC_SHIFT     = 10,
  PREC_TERM      = 11,
  PREC_FACTOR    = 12,
  PREC_UNARY     = 13,
  PREC_POWER     = 14,
  PREC_CALL      = 15,
  PREC_PRIMARY   = 16,
} Precedence;
```

### 注册与实现

```c
// gParseRules[TOK_IF] = { NULL /*不做前缀*/, parseIfExpr, PREC_IFEXPR };

static MsNode* parseIfExpr(MsParser* p, MsNode* value) {
  // 已经消耗 'if' token（p->prev == TOK_IF）
  MsSrcPos pos = p->prev.pos;

  // 解析条件（优先级 > PREC_IFEXPR 防止链式 if 冲突）
  MsNode* cond = parsePrecedence(p, PREC_OR);

  expect(p, TOK_ELSE, "expected 'else' after condition in if-expression");

  // 备选（右结合：允许 a if c1 else b if c2 else d）
  MsNode* alt = parsePrecedence(p, PREC_IFEXPR);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind            = ND_IF_EXPR;
  n->pos             = pos;
  n->if_expr.value   = value;
  n->if_expr.cond    = cond;
  n->if_expr.alt     = alt;
  return n;
}
```

### 链式条件表达式（右结合）

```
a if c1 else b if c2 else d
```

解析为：`a if c1 else (b if c2 else d)`，通过 `alt = parsePrecedence(p, PREC_IFEXPR)` 实现。

### 与 `if` 语句的区分

`if` 关键字在 Pratt 框架中注册为**中缀**（`infix`），用于条件表达式。`if` 关键字作为**语句开头**（前缀）时由 `msParseStmt` 单独处理（T027），不走 Pratt。

---

## 验收标准（checklist）

- [ ] `"a if cond else b"` → `ND_IF_EXPR(value=ND_IDENT(a), cond=ND_IDENT(cond), alt=ND_IDENT(b))`。
- [ ] `"1 + 2 if x > 0 else 3"` → 根为 `ND_IF_EXPR`，`value=ND_BINARY(+,1,2)`。
- [ ] `"a if c1 else b if c2 else d"` → 右结合：`alt` 为另一 `ND_IF_EXPR`。
- [ ] `"if cond { }"` 在语句上下文中不触发 if-expr（`if` 作为语句，非中缀）。
- [ ] 缺 `else` 时产生语法错误（"expected 'else'"）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_if_expr.c`）

```c
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "ms_arena.h"

static MsNode* px(MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t)strlen(s), "<t>", a);
  return msParseExpr(&p);
}

static void testIfExpr(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "x if cond else y");
  MS_ASSERT_EQ(n->kind, ND_IF_EXPR, "if expr");
  MS_ASSERT_EQ(n->if_expr.value->kind, ND_IDENT, "value=ident");
  MS_ASSERT_EQ(n->if_expr.cond->kind,  ND_IDENT, "cond=ident");
  MS_ASSERT_EQ(n->if_expr.alt->kind,   ND_IDENT, "alt=ident");
  msArenaFree(&a);
}

static void testIfExprChained(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "a if c1 else b if c2 else d");
  MS_ASSERT_EQ(n->kind, ND_IF_EXPR, "root if_expr");
  MS_ASSERT_EQ(n->if_expr.alt->kind, ND_IF_EXPR, "alt is also if_expr (right-assoc)");
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
abs_val := x if x >= 0 else -x
print(abs_val)  // 10
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **优先级调整影响 T019**：在 T018 的 `Precedence` 枚举中插入 `PREC_IFEXPR = 2` 会使原来 `PREC_OR = 2` 变为 `PREC_OR = 3`，其余依次 +1。T019 的 `parsePrecedence` 调用需同步更新（T018/T019 实现时一并处理）。
- **无三目 `?:`**：mslang 明确不支持 C 风格三目；遇到 `?` 应产生 `TOK_ERROR`（词法层，T013 已覆盖）。
- **if-expr vs if-stmt 消歧**：`if` 在表达式上下文（Pratt 中缀）触发 if-expr；在语句上下文（`msParseStmt` 首 token 为 `if`）触发 if-stmt。两者互不干扰。
