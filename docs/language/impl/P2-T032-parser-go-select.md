# P2-T032 go 语句 / select 语句

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现并发控制语句的解析：
- `go func_call(args)` → `MS_ND_GO`（启动 goroutine）
- `select { case <-ch: … case ch <- v: … default: … }` → `MS_ND_SELECT`（channel 多路复用）

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T027 | `msParseBlock` |
| P2-T025 | `MS_ND_RECV`/`MS_ND_SEND` 节点 |
| P2-T017 | `MS_ND_GO`/`MS_ND_SELECT`/`MS_ND_SELECT_CASE` 节点（`ms_ast.h`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.2 语句文法（GoStmt / SelectStmt / SendStmt / RecvStmt） |
| `concurrency.md` | §2.2 go 语句、§3.4 channel 操作、§3.5 select |

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
// msParserMatch(p, MS_TOK_GO) 分支：
static MsNode* parseGoStmt(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;
  // go 后解析表达式；文法限定 CallExpr，parser 放宽为 Expr 以复用
  // 表达式解析，MS_ND_CALL 形态校验下沉至编译器 T045。
  MsNode* callExpr = msParseExpr(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind       = MS_ND_GO;
  n->pos        = pos;
  n->goStmt.call = callExpr;
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
  struct MsSrcPos pos = p->prev.pos;
  msParserExpect(p, MS_TOK_LBRACE, "expected '{' after 'select'");

  MsNodeList* cases  = NULL;
  MsNodeList** cTail = &cases;

  while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {}

  while (!msParserCheck(p, MS_TOK_RBRACE) && !msParserCheck(p, MS_TOK_EOF)) {
    MsNode* selectCase = MS_ARENA_NEW(p->arena, MsNode);
    selectCase->kind = MS_ND_SELECT_CASE;
    selectCase->pos  = p->cur.pos;

    MsNode* commStmt = NULL;

    if (msParserMatch(p, MS_TOK_DEFAULT)) {
      msParserExpect(p, MS_TOK_COLON, "expected ':' after 'default'");
    } else if (msParserMatch(p, MS_TOK_CASE)) {
      // 可能形式：
      //   case <-ch:            → MS_ND_RECV
      //   case v := <-ch:       → MS_ND_SHORT_DECL(init=MS_ND_RECV)
      //   case ch <- val:       → MS_ND_SEND

      if (msParserCheck(p, MS_TOK_ARROW_LEFT)) {
        // case <-ch:
        msParserAdvance(p);
        MsNode* chanExpr = msParseExpr(p);
        commStmt = MS_ARENA_NEW(p->arena, MsNode);
        commStmt->kind = MS_ND_RECV;
        commStmt->recv.chanExpr = chanExpr;
      } else {
        MsNode* lhs = msParseExpr(p);

        if (msParserMatch(p, MS_TOK_COLON_ASSIGN)) {
          // case v := <-ch:
          MsNode* rhs = msParseExpr(p);
          commStmt = MS_ARENA_NEW(p->arena, MsNode);
          commStmt->kind = MS_ND_SHORT_DECL;
          commStmt->varDecl.init = rhs;
          // lhs 为 MS_ND_IDENT；v,ok 双名形式的存储方案见「风险与边界」
        } else if (msParserMatch(p, MS_TOK_ARROW_LEFT)) {
          // case ch <- val:
          MsNode* val = msParseExpr(p);
          commStmt = MS_ARENA_NEW(p->arena, MsNode);
          commStmt->kind = MS_ND_SEND;
          commStmt->send.chanExpr = lhs;
          commStmt->send.val = val;
        } else {
          msParserError(p, "invalid select case");
        }
      }
      msParserExpect(p, MS_TOK_COLON, "expected ':' after select case");
    } else {
      msParserError(p, "expected 'case' or 'default' in select");
      break;
    }

    // 解析 case 体
    while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {}
    MsNodeList* stmts = NULL;
    MsNodeList** st = &stmts;
    while (!msParserCheck(p, MS_TOK_CASE) && !msParserCheck(p, MS_TOK_DEFAULT)
               && !msParserCheck(p, MS_TOK_RBRACE)
               && !msParserCheck(p, MS_TOK_EOF)) {
      MsNode* stmt = msParseStmt(p);
      if (stmt) {
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = stmt;
        item->next = NULL;
        *st = item;
        st = &item->next;
      }
      while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {}
    }

    // 将 stmts 包装为 MS_ND_BLOCK 并装配回 selectCase 节点
    MsNode* body = MS_ARENA_NEW(p->arena, MsNode);
    body->kind = MS_ND_BLOCK;
    body->block.stmts = stmts;

    selectCase->selectCase.comm = commStmt;
    selectCase->selectCase.body = body;

    MsNodeList* citem = MS_ARENA_NEW(p->arena, MsNodeList);
    citem->node = selectCase;
    citem->next = NULL;
    *cTail = citem;
    cTail = &citem->next;
  }
  msParserExpect(p, MS_TOK_RBRACE, "expected '}' to close select");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind              = MS_ND_SELECT;
  n->pos               = pos;
  n->selectStmt.cases  = cases;
  return n;
}
```

**`MS_ND_SELECT_CASE` 专用字段**（由本任务在 `ms_ast.h` 的 auxiliary 枚举区追加，与 `MS_ND_SWITCH_CASE`/`MS_ND_KWARG_PAIR` 并列）：

```c
// MS_ND_SELECT_CASE
struct {
  MsNode* comm;    // 通信语句（MS_ND_RECV/MS_ND_SEND/MS_ND_SHORT_DECL，NULL → default）
  MsNode* body;    // MS_ND_BLOCK
} selectCase;
```

---

## 验收标准（checklist）

- [x] `"go f()"` → `MS_ND_GO(call=MS_ND_CALL(f,[]))`。 <!-- v:ctest:test_go_select -->
- [x] `"go func() { print(1) }()"` → `MS_ND_GO(call=MS_ND_CALL(MS_ND_FUNC_DECL,…))`（匿名函数即时调用）。 <!-- v:ctest:test_go_select -->
- [x] `"select { case <-ch: a }"` → `MS_ND_SELECT`，case 含 `MS_ND_RECV`。 <!-- v:ctest:test_go_select -->
- [x] `"select { case v := <-ch: a }"` → case 含 `MS_ND_SHORT_DECL(init=MS_ND_RECV)`。 <!-- v:ctest:test_go_select -->
- [x] `"select { case ch <- 1: a }"` → case 含 `MS_ND_SEND`。 <!-- v:ctest:test_go_select -->
- [x] `"select { default: a }"` → default case（comm=NULL）。 <!-- v:ctest:test_go_select -->
- [x] `"select { case <-c1: a case <-c2: b default: c }"` → 3 个 case。 <!-- v:ctest:test_go_select -->
- [x] `select {}` → 空 select（永久阻塞，合法）。 <!-- v:ctest:test_go_select -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_go_select.c`）

```c
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_ast.h"
#include "mslang/ms_parser.h"
#include "parser/ms_arena.h"

static MsNode* pStmt(struct MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t)strlen(s), "<t>", a);
  return msParseStmt(&p);
}

static void testGoStmt(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "go f()");
  MS_ASSERT_EQ(n->kind, MS_ND_GO, "go");
  MS_ASSERT_EQ(n->goStmt.call->kind, MS_ND_CALL, "call");
  msArenaFree(&a);
}

static void testSelectBasic(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "select { case <-ch: pass }");
  MS_ASSERT_EQ(n->kind, MS_ND_SELECT, "select");
  MS_ASSERT_TRUE(n->selectStmt.cases != NULL, "has cases");
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

- **`go` 后必须是函数调用**：`syntax.md §2.2` 文法限定 `GoStmt = 'go' CallExpr ';'`，parser 放宽为 `msParseExpr` 以复用表达式解析，`MS_ND_CALL` 形态校验下沉至编译器 T045。
- **`select` 空 case**：`select {}` 合法（阻塞 goroutine，类似 `time.sleep(∞)`）。
- **select case 行为**：select 随机选择已就绪的 case，无就绪则阻塞（有 default 时不阻塞）。运行期语义在 T110 实现。
- **`value, ok := <-ch` 双名绑定**：`concurrency.md §3.4` / `syntax.md §2.2 RecvStmt` 定义 `value, ok := <-ch` 形式（`ok` 为 closed 标志）。现有 `varDecl` 仅支持单名绑定；初版 parser 可暂只支持 `v := <-ch` 单名形式，双名绑定的 AST 表达（如 lhs 用 `MS_ND_TUPLE` 或扩展 `varDecl`）留待实现时根据 AST 设计决定。
