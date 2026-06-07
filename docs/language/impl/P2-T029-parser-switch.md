# P2-T029 switch / case / fallthrough / default

> **状态**：⬜ 未开始

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
| P2-T027 | `parseBlock` |
| P2-T017 | `ND_SWITCH`/`ND_SWITCH_CASE`/`ND_FALLTHROUGH` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.4.10 switch 语句（含 fallthrough） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c   # parseSwitchStmt
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
  MsSrcPos pos = p->prev.pos;

  // 可选 switch 表达式（无表达式 → 用 ND_BOOL(true) 作为 dummy）
  MsNode* expr = NULL;
  if (!check(p, TOK_LBRACE)) {
    expr = msParseExpr(p);
  }
  expect(p, TOK_LBRACE, "expected '{' after switch expression");

  MsNodeList* cases = NULL;
  MsNodeList** casesTail = &cases;

  // 跳过换行
  while (match(p, TOK_NEWLINE) || match(p, TOK_SEMICOLON)) {}

  while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
    MsNode* caseNode = MS_ARENA_NEW(p->arena, MsNode);
    caseNode->kind = ND_SWITCH_CASE;
    caseNode->pos  = p->cur.pos;

    bool isDefault = false;
    MsNodeList* values = NULL;

    if (match(p, TOK_CASE)) {
      // 解析 case 值列表（逗号分隔）
      MsNodeList** vt = &values;
      do {
        MsNode* val = parsePrecedence(p, PREC_OR);
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = val; item->next = NULL;
        *vt = item; vt = &item->next;
      } while (match(p, TOK_COMMA));
      expect(p, TOK_COLON, "expected ':' after case value");
    } else if (match(p, TOK_DEFAULT)) {
      isDefault = true;
      expect(p, TOK_COLON, "expected ':' after 'default'");
    } else {
      parserError(p, "expected 'case' or 'default' in switch");
      break;
    }

    // 解析 case 体语句
    while (match(p, TOK_NEWLINE) || match(p, TOK_SEMICOLON)) {}
    MsNodeList* stmts = NULL;
    MsNodeList** st = &stmts;
    while (!check(p, TOK_CASE) && !check(p, TOK_DEFAULT)
               && !check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
      MsNode* stmt = msParseStmt(p);
      if (stmt) {
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = stmt; item->next = NULL;
        *st = item; st = &item->next;
      }
      while (match(p, TOK_NEWLINE) || match(p, TOK_SEMICOLON)) {}
    }

    // 组装 switch_case 节点（使用 switch_stmt 字段）
    // 由于 ND_SWITCH_CASE 是扩展节点，复用 switch_stmt union：
    caseNode->switch_stmt.expr  = isDefault ? NULL : (MsNode*)(uintptr_t)1;  // 标记 is_default
    caseNode->switch_stmt.cases = values;  // case 值列表
    // body 存入 block 字段：
    MsNode* bodyBlock = MS_ARENA_NEW(p->arena, MsNode);
    bodyBlock->kind = ND_BLOCK;
    bodyBlock->block.stmts = stmts;
    // 存储 body：使用独立指针——在 T017 中 ND_SWITCH_CASE 应有 .values + .body 字段

    MsNodeList* citem = MS_ARENA_NEW(p->arena, MsNodeList);
    citem->node = caseNode; citem->next = NULL;
    *casesTail = citem; casesTail = &citem->next;
  }
  expect(p, TOK_RBRACE, "expected '}' to close switch");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind                = ND_SWITCH;
  n->pos                 = pos;
  n->switch_stmt.expr    = expr;
  n->switch_stmt.cases   = cases;
  return n;
}
```

**注**：`ND_SWITCH_CASE` 节点需在 T017 中为其 union 追加专用字段：

```c
// 在 MsNode union 中追加（ND_SWITCH_CASE）：
struct {
  MsNodeList* values;   // case 值列表（NULL → default）
  MsNode*     body;     // ND_BLOCK
} switch_case;
```

### `fallthrough` 语句

```c
// msParseStmt 中，match(TOK_FALLTHROUGH) 分支：
MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
n->kind = ND_FALLTHROUGH;
n->pos  = p->prev.pos;
return n;
```

---

## 验收标准（checklist）

- [ ] `"switch x { case 1: a case 2: b }"` → `ND_SWITCH(expr=x, cases=[CASE([1],[a]), CASE([2],[b])])`。
- [ ] `"switch x { case 1, 2: a }"` → 单 case 有两个值 `[1, 2]`。
- [ ] `"switch x { default: a }"` → `ND_SWITCH_CASE(values=NULL)`（default）。
- [ ] `"switch { case x > 0: a }"` → 无 switch 表达式（expr=NULL）。
- [ ] `"switch x { case 1: a\nfallthrough\ncase 2: b }"` → case 1 body 含 `ND_FALLTHROUGH`。
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
  MS_ASSERT_EQ(n->kind, ND_SWITCH, "switch");
  MS_ASSERT_TRUE(n->switch_stmt.cases != NULL, "has cases");
  msArenaFree(&a);
}

static void testSwitchNoExpr(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "switch { case x > 0: a }");
  MS_ASSERT_EQ(n->kind, ND_SWITCH, "switch");
  MS_ASSERT_TRUE(n->switch_stmt.expr == NULL, "no expr");
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
case y > 0:  print("positive")
case y == 0: print("zero")
default:     print("negative")
}
// negative
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`fallthrough` 语义**：Go 风格，`fallthrough` 跳到下一 case 的第一条语句（不重新检查条件）。mslang 编译器（T046）在字节码层实现跳转；parser 只需产生 `ND_FALLTHROUGH` 节点。
- **`fallthrough` 位置限制**：`fallthrough` 只能是 case 体最后一条语句；语义检查在 compiler 阶段（parser 不强制，允许在任意位置，运行期/编译期报错）。
- **default 位置**：约定 `default` 在最后，但不强制；编译器处理顺序与位置。
