# P2-T032 go 语句 / select 语句

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现并发控制语句的解析：
- `go func_call(args)` → `ND_GO`（启动 goroutine）
- `select { case <-ch: … case ch <- v: … default: … }` → `ND_SELECT`（channel 多路复用）

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T027 | `parseBlock` |
| P2-T025 | `ND_RECV`/`ND_SEND` 节点 |
| P2-T017 | `ND_GO`/`ND_SELECT` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.6.3 go 语句 |
| `syntax.md` | §2.6.4 select 语句（case 含 send/recv/default） |
| `concurrency.md` | §1 goroutine / §3 select |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c   # parseGoStmt / parseSelectStmt
```

---

## 实现要点

### 1. `go` 语句

```c
// match(TOK_GO) 分支：
static MsNode* parseGoStmt(MsParser* p) {
  MsSrcPos pos = p->prev.pos;
  // go 后必须跟函数调用表达式
  MsNode* callExpr = msParseExpr(p);
  // 语义检查：callExpr 应为 ND_CALL（编译器验证）

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind        = ND_GO;
  n->pos         = pos;
  n->go_stmt.call = callExpr;
  return n;
}
```

### 2. `select` 语句

select 的 case 有三种形式：
- `case <-ch:` → 接收操作（可选赋值 `case v := <-ch:`）
- `case ch <- val:` → 发送操作
- `default:` → 无阻塞分支（最多一个）

```c
static MsNode* parseSelectStmt(MsParser* p) {
  MsSrcPos pos = p->prev.pos;
  expect(p, TOK_LBRACE, "expected '{' after 'select'");

  MsNodeList* cases  = NULL;
  MsNodeList** cTail = &cases;

  while (match(p, TOK_NEWLINE) || match(p, TOK_SEMICOLON)) {}

  while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
    MsNode* selectCase = MS_ARENA_NEW(p->arena, MsNode);
    selectCase->kind = ND_SWITCH_CASE;   // 复用，或使用专用 ND_SELECT_CASE
    selectCase->pos  = p->cur.pos;

    MsNode* commStmt = NULL;  // 通信语句（send/recv/assign-recv）

    if (match(p, TOK_DEFAULT)) {
      // default: 无通信语句
      expect(p, TOK_COLON, "expected ':' after 'default'");
    } else if (match(p, TOK_CASE)) {
      // 解析通信操作
      // 可能形式：
      //   case <-ch:
      //   case v := <-ch:
      //   case v, ok := <-ch:
      //   case ch <- val:

      // 先解析 case 后的表达式/赋值
      if (check(p, TOK_ARROW_LEFT)) {
        // case <-ch:
        advance(p);
        MsNode* chanExpr = msParseExpr(p);
        commStmt = MS_ARENA_NEW(p->arena, MsNode);
        commStmt->kind = ND_RECV;
        commStmt->recv.chan_expr = chanExpr;
      } else {
        MsNode* lhs = msParseExpr(p);
        lhs = parseMaybeTuple(p, lhs);

        if (match(p, TOK_COLON_ASSIGN)) {
          // case v := <-ch:
          MsNode* rhs = msParseExpr(p);  // 应为 ND_RECV
          commStmt = MS_ARENA_NEW(p->arena, MsNode);
          commStmt->kind = ND_SHORT_DECL;
          commStmt->var_decl.init = rhs;
          // 存 lhs（名称）
        } else if (match(p, TOK_ARROW_LEFT)) {
          // case ch <- val:
          MsNode* val = msParseExpr(p);
          commStmt = MS_ARENA_NEW(p->arena, MsNode);
          commStmt->kind = ND_SEND;
          commStmt->send.chan_expr = lhs;
          commStmt->send.val = val;
        } else {
          parserError(p, "invalid select case");
        }
      }
      expect(p, TOK_COLON, "expected ':' after select case");
    } else {
      parserError(p, "expected 'case' or 'default' in select");
      break;
    }

    // 解析 case 体
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
    // selectCase 存 commStmt + stmts（需 ND_SELECT_CASE 专用字段）

    MsNodeList* citem = MS_ARENA_NEW(p->arena, MsNodeList);
    citem->node = selectCase; citem->next = NULL;
    *cTail = citem; cTail = &citem->next;
  }
  expect(p, TOK_RBRACE, "expected '}' to close select");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind               = ND_SELECT;
  n->pos                = pos;
  n->select_stmt.cases  = cases;
  return n;
}
```

**`ND_SELECT_CASE` 专用字段**（在 T017 中追加）：

```c
// ND_SELECT_CASE（不同于 ND_SWITCH_CASE）
struct {
  MsNode*     comm;    // 通信语句（ND_RECV/ND_SEND/ND_SHORT_DECL，NULL → default）
  MsNode*     body;    // ND_BLOCK
} select_case;
```

---

## 验收标准（checklist）

- [ ] `"go f()"` → `ND_GO(call=ND_CALL(f,[]))`。
- [ ] `"go func() { print(1) }()"` → `ND_GO(call=ND_CALL(ND_FUNC_DECL,…))`（匿名函数即时调用）。
- [ ] `"select { case <-ch: a }"` → `ND_SELECT`，case 含 `ND_RECV`。
- [ ] `"select { case v := <-ch: a }"` → case 含 `ND_SHORT_DECL(init=ND_RECV)`。
- [ ] `"select { case ch <- 1: a }"` → case 含 `ND_SEND`。
- [ ] `"select { default: a }"` → default case（comm=NULL）。
- [ ] `"select { case <-c1: a case <-c2: b default: c }"` → 3 个 case。
- [ ] `select {}` → 空 select（永久阻塞，合法）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_go_select.c`）

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

static void testGoStmt(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "go f()");
  MS_ASSERT_EQ(n->kind, ND_GO, "go");
  MS_ASSERT_EQ(n->go_stmt.call->kind, ND_CALL, "call");
  msArenaFree(&a);
}

static void testSelectBasic(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "select { case <-ch: pass }");
  MS_ASSERT_EQ(n->kind, ND_SELECT, "select");
  MS_ASSERT_TRUE(n->select_stmt.cases != NULL, "has cases");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testGoStmt);
  MS_RUN(testSelectBasic);
  return msTestSummary();
}
```

### .ms 使用示例（T107/T110 后验证）

```ms
// go 语句
ch := make(chan int, 1)
go func() {
    ch <- 42
}()
print(<-ch)   // 42

// select
c1 := make(chan string, 1)
c2 := make(chan string, 1)
c1 <- "one"

select {
case v := <-c1:
    print("from c1:", v)   // from c1: one
case v := <-c2:
    print("from c2:", v)
default:
    print("no data ready")
}

// 超时模式（需 time 模块）
// select {
// case v := <-ch:
//     print("got:", v)
// case <-time.after(1.0):
//     print("timeout")
// }
```

---

## Benchmark

N/A（并发语义在 T106–T114 实现；归入 T114 并发整体 bench）。

---

## 风险与边界

- **`go` 后必须是函数调用**：`go expr` 语法，`expr` 应为 `ND_CALL`；parser 不强制，编译器（T045）验证。
- **`select` 空 case**：`select {}` 合法（阻塞 goroutine，类似 `time.sleep(∞)`）。
- **select case 行为**：select 随机选择已就绪的 case，无就绪则阻塞（有 default 时不阻塞）。运行期语义在 T110 实现。
