# P2-T027 if / else if / else 语句

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `if` 语句（包括 `else if` 链与 `else` 分支）的语句级解析，产生 `MS_ND_IF` 节点。同时实现 `parseBlock`（`{ stmt… }`）辅助函数，因为所有控制流语句都依赖块解析。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架（`msParseStmt` 骨架） |
| P2-T026 | 表达式语句与赋值 |
| P2-T017 | `MS_ND_IF`/`MS_ND_BLOCK` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.2 语句 — `IfStmt` / `Block` |
| `syntax.md` | §1.3 ASI（`{` 必须与 `if` 在同一行） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c     # parseBlock + parseIfStmt（在 msParseStmt 分支中注册）
```

---

## 实现要点

### 1. `parseBlock`（共用辅助函数）

```c
// 解析 { stmt… }（已消耗 '{'）
MsNode* parseBlock(MsParser* p) {
  MsSrcPos pos = p->prev.pos;

  MsNodeList* stmts = NULL;
  MsNodeList** tail = &stmts;

  // 跳过起始换行（块内首行换行无语义）
  while (match(p, MS_TOK_NEWLINE) || match(p, MS_TOK_SEMICOLON)) {}

  while (!check(p, MS_TOK_RBRACE) && !check(p, MS_TOK_EOF)) {
    MsNode* stmt = msParseStmt(p);
    if (stmt != NULL) {
      MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
      item->node = stmt; item->next = NULL;
      *tail = item; tail = &item->next;
    }
    // 跳过语句分隔符
    while (match(p, MS_TOK_NEWLINE) || match(p, MS_TOK_SEMICOLON)) {}
  }
  expect(p, MS_TOK_RBRACE, "expected '}' to close block");

  MsNode* block = MS_ARENA_NEW(p->arena, MsNode);
  block->kind = MS_ND_BLOCK;
  block->pos = pos;
  block->block.stmts = stmts;
  return block;
}
```

### 2. `parseIfStmt`

```c
// msParseStmt 中，match(MS_TOK_IF) 分支：
static MsNode* parseIfStmt(MsParser* p) {
  MsSrcPos pos = p->prev.pos;

  // 条件（不使用 '{' 分隔符，直到遇 '{'）
  // 注意：条件不能换行（ASI 规则），因此不跳过换行
  MsNode* cond = parsePrecedence(p, PREC_OR);

  // '{' 必须在同一行
  expect(p, MS_TOK_LBRACE, "expected '{' after if condition");
  MsNode* thenBlock = parseBlock(p);

  MsNode* elseBlock = NULL;
  // 消耗可能的换行，检查 else
  while (match(p, MS_TOK_NEWLINE) || match(p, MS_TOK_SEMICOLON)) {}
  if (match(p, MS_TOK_ELSE)) {
    if (match(p, MS_TOK_IF)) {
      elseBlock = parseIfStmt(p);  // else if 递归
    } else {
      expect(p, MS_TOK_LBRACE, "expected '{' after 'else'");
      elseBlock = parseBlock(p);
    }
  }

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_IF;
  n->pos = pos;
  n->ifStmt.cond = cond;
  n->ifStmt.thenBlock = thenBlock;
  n->ifStmt.elseBlock = elseBlock;
  return n;
}
```

### 3. else 跨行问题

mslang ASI 规则：`}` 触发分号插入（T015）。因此 `} else` 需要在 `}` 后跳过 `MS_TOK_NEWLINE` 再匹配 `else`：

```c
// 在 parseIfStmt 中，thenBlock 解析完成后：
// parseBlock 已消耗 '}'，此后 prevKind = MS_TOK_RBRACE → ASI 触发
// msParseStmt 循环会看到 MS_TOK_NEWLINE 然后停止
// 因此 else 必须与 } 在同一行，OR 在下一语句处理中检查

// mslang 设计（参考 Go）：else 必须与 } 在同一行
// 词法层：} 后遇换行触发 ASI，else 视为新语句开头 → 语法错误
// 这是 Go 风格：if cond { \n} else { ... } 是合法的
// 但 if cond { \n}\nelse { ... } 不合法
```

**实际实现**：parseBlock 消耗 `}` 后，调用方立即 check `else`（不跳过换行），若没有 `else` 则正常结束。若 `}` 和 `else` 在不同行，ASI 插入了 `MS_TOK_NEWLINE`，`else` 变成独立语句，产生"unexpected 'else'"错误。这是正确的 Go 风格行为。

---

## 验收标准（checklist）

- [ ] `"if x { }"` → `MS_ND_IF(cond=MS_ND_IDENT(x), thenBlock=MS_ND_BLOCK([]), elseBlock=NULL)`。
- [ ] `"if x { a } else { b }"` → `elseBlock` 为 `MS_ND_BLOCK([MS_ND_EXPR_STMT(b)])`。
- [ ] `"if x { a } else if y { b } else { c }"` → `elseBlock` 为 `MS_ND_IF`（递归）。
- [ ] `"if x {\n  a\n} else {\n  b\n}"` → 多行块合法（`}` 和 `else` 同行）。
- [ ] `"if x {\n}\nelse { }"` → 语法错误（`}` 后换行，ASI，`else` 成新语句）。
- [ ] `"if x > 0 && y < 10 { }"` → 条件为 `MS_ND_BINARY(AND, …)`。
- [ ] `parseBlock` 为空块 `{}` 返回 `MS_ND_BLOCK(stmts=NULL)`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_if_stmt.c`）

```c
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "ms_arena.h"

static MsNode* pStmt(MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t)strlen(s), "<t>", a);
  return msParseStmt(&p);
}

static void testIfSimple(void) {
  MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "if x { }");
  MS_ASSERT_EQ(n->kind, MS_ND_IF, "if stmt");
  MS_ASSERT_TRUE(n->ifStmt.elseBlock == NULL, "no else");
  msArenaFree(&a);
}

static void testIfElse(void) {
  MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "if x { a } else { b }");
  MS_ASSERT_EQ(n->kind, MS_ND_IF, "if");
  MS_ASSERT_TRUE(n->ifStmt.elseBlock != NULL, "has else");
  MS_ASSERT_EQ(n->ifStmt.elseBlock->kind, MS_ND_BLOCK, "else is block");
  msArenaFree(&a);
}

static void testIfElseIf(void) {
  MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "if x { a } else if y { b } else { c }");
  MS_ASSERT_EQ(n->kind, MS_ND_IF, "outer if");
  MS_ASSERT_EQ(n->ifStmt.elseBlock->kind, MS_ND_IF, "else if");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testIfSimple);
  MS_RUN(testIfElse);
  MS_RUN(testIfElseIf);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
x := 5

if x > 0 {
    print("positive")
} else if x == 0 {
    print("zero")
} else {
    print("negative")
}
// positive

// 嵌套 if
if x > 0 {
    if x < 100 {
        print("between 1 and 99")
    }
}
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`{` 必须在同一行**：mslang 与 Go 完全一致，`if cond` 后必须立刻跟 `{`；换行后的 `{` 会因 ASI 先看到 `cond` 的 ASI，导致 `{` 变成 map/set 字面量前缀（语法错误）。
- **`else` 跨行**：`} else {` 在同一行合法；`}\nelse` 不合法（Go 同样如此）。
- **`if` 表达式 vs `if` 语句**：`if` 在 Pratt 中缀（T020）是表达式；在 `msParseStmt` 首 token 检测时是语句。两者通过调用上下文区分，不冲突。
