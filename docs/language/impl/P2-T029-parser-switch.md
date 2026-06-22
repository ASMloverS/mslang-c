# P2-T029 switch / case / fallthrough / default

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现 `switch` 语句的解析，支持：
- `switch expr { case v1, v2: … case v3: … default: … }` 形式
- `fallthrough` 语句（跳到下一 case 的头部）
- 无表达式 `switch { case cond: … }`（类 if-else 链）

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T027 | `msParseBlock` |
| P2-T017 | `MS_ND_SWITCH`/`MS_ND_SWITCH_CASE`/`MS_ND_FALLTHROUGH` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.2 SwitchStmt / SwitchCase / FallthroughStmt |
| `ms_ast.h` | `MS_ND_SWITCH`、`MS_ND_SWITCH_CASE`、`MS_ND_FALLTHROUGH`、`switchStmt`/`switchCase` 字段 |
| `ms_lexer.h` | `MS_TOK_SWITCH`、`MS_TOK_CASE`、`MS_TOK_DEFAULT`、`MS_TOK_FALLTHROUGH` |
| `ms_parser.h` | `msParserCheck`/`msParserMatch`/`msParserExpect`/`msParserError` API |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c   # parseSwitchStmt + msParseStmt 增加 MS_TOK_SWITCH/MS_TOK_FALLTHROUGH 分支
```

---

## 实现要点

### Switch 结构

```
switch [expr] {
case val1, val2:
    stmts
    [fallthrough]
case val3:
    stmts
default:
    stmts
}
```

- `case` 后可有多个值（逗号分隔）。
- `default` 最多一个，可以在任意位置（但约定最后）。
- `fallthrough` 只能是 case 块最后一条语句，作用：执行完当前 case 后跳到下一 case 的**首条语句**（不再做条件检查），类似 Go。

### 实现

```c
static MsNode* parseSwitchStmt(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;

  // 可选 switch 表达式（无表达式 → expr=NULL，语义等价 switch true 由 compiler 处理）
  MsNode* expr = NULL;
  if (!msParserCheck(p, MS_TOK_LBRACE)) {
    expr = msParseExpr(p);
  }
  msParserExpect(p, MS_TOK_LBRACE, "expected '{' after switch expression");

  MsNodeList* cases = NULL;
  MsNodeList** casesTail = &cases;

  // 跳过换行
  while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {}

  while (!msParserCheck(p, MS_TOK_RBRACE) && !msParserCheck(p, MS_TOK_EOF)) {
    MsNode* caseNode = MS_ARENA_NEW(p->arena, MsNode);
    caseNode->kind = MS_ND_SWITCH_CASE;
    caseNode->pos  = p->cur.pos;

    bool isDefault = false;
    MsNodeList* values = NULL;

    if (msParserMatch(p, MS_TOK_CASE)) {
      // 解析 case 值列表（逗号分隔）
      MsNodeList** vt = &values;
      do {
        MsNode* val = msParseExpr(p);
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = val; item->next = NULL;
        *vt = item; vt = &item->next;
      } while (msParserMatch(p, MS_TOK_COMMA));
      msParserExpect(p, MS_TOK_COLON, "expected ':' after case value");
    } else if (msParserMatch(p, MS_TOK_DEFAULT)) {
      isDefault = true;
      msParserExpect(p, MS_TOK_COLON, "expected ':' after 'default'");
    } else {
      msParserError(p, "expected 'case' or 'default' in switch");
      break;
    }

    // 解析 case 体语句
    while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {}
    MsNodeList* stmts = NULL;
    MsNodeList** st = &stmts;
    while (!msParserCheck(p, MS_TOK_CASE) && !msParserCheck(p, MS_TOK_DEFAULT)
               && !msParserCheck(p, MS_TOK_RBRACE) && !msParserCheck(p, MS_TOK_EOF)) {
      MsNode* stmt = msParseStmt(p);
      if (stmt) {
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = stmt; item->next = NULL;
        *st = item; st = &item->next;
      }
      while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {}
    }

    // 组装 switchCase 字段
    caseNode->switchCase.values = values;
    caseNode->switchCase.isDefault = isDefault;
    MsNode* bodyBlock = MS_ARENA_NEW(p->arena, MsNode);
    bodyBlock->kind = MS_ND_BLOCK;
    bodyBlock->pos = caseNode->pos;
    bodyBlock->block.stmts = stmts;
    caseNode->switchCase.body = bodyBlock;

    MsNodeList* citem = MS_ARENA_NEW(p->arena, MsNodeList);
    citem->node = caseNode; citem->next = NULL;
    *casesTail = citem; casesTail = &citem->next;
  }
  msParserExpect(p, MS_TOK_RBRACE, "expected '}' to close switch");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind                = MS_ND_SWITCH;
  n->pos                 = pos;
  n->switchStmt.expr    = expr;
  n->switchStmt.cases   = cases;
  return n;
}
```

### `fallthrough` 语句

```c
// msParseStmt 中，match(MS_TOK_FALLTHROUGH) 分支：
MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
n->kind = MS_ND_FALLTHROUGH;
n->pos  = p->prev.pos;
return n;
```

---

## 验收标准（checklist）

- [ ] `"switch x { case 1: a case 2: b }"` → `MS_ND_SWITCH(expr=x, cases=[CASE([1],[a]), CASE([2],[b])])`。
- [ ] `"switch x { case 1, 2: a }"` → 单 case 有两个值 `[1, 2]`。
- [ ] `"switch x { default: a }"` → `MS_ND_SWITCH_CASE(values=NULL)`（default）。
- [ ] `"switch { case x > 0: a }"` → 无 switch 表达式（expr=NULL）。
- [ ] `"switch x { case 1: a\nfallthrough\ncase 2: b }"` → case 1 body 含 `MS_ND_FALLTHROUGH`。
- [ ] `"switch x {}"` → cases 为空，合法（空 switch）。
- [ ] case 体可跨多行（每行一语句，换行为分隔符）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_switch_stmt.c`）

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

static void testSwitchBasic(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "switch x { case 1: a }");
  MS_ASSERT_EQ(n->kind, MS_ND_SWITCH, "switch");
  MS_ASSERT_TRUE(n->switchStmt.cases != NULL, "has cases");
  msArenaFree(&a);
}

static void testSwitchNoExpr(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "switch { case x > 0: a }");
  MS_ASSERT_EQ(n->kind, MS_ND_SWITCH, "switch");
  MS_ASSERT_TRUE(n->switchStmt.expr == NULL, "no expr");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testSwitchBasic);
  MS_RUN(testSwitchNoExpr);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
x := 2
switch x {
case 1:
    print("one")
case 2, 3:
    print("two or three")
    fallthrough
case 4:
    print("four (or fell through from two/three)")
default:
    print("other")
}
// two or three
// four (or fell through from two/three)

// 无表达式 switch（类 if-else）
y := -5
switch {
case y > 0:
    print("positive")
case y == 0:
    print("zero")
default:
    print("negative")
}
// negative
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`fallthrough` 语义**：Go 风格，`fallthrough` 跳到下一 case 的第一条语句（不重新检查条件）。mslang 编译器（T046）在字节码层实现跳转；parser 只需产生 `MS_ND_FALLTHROUGH` 节点。
- **`fallthrough` 位置限制**：`fallthrough` 只能是 case 体最后一条语句；语义检查在 compiler 阶段（parser 不强制，允许在任意位置，运行期/编译期报错）。
- **default 位置**：约定 `default` 在最后，但不强制；编译器处理顺序与位置。
