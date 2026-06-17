# P2-T025 make / recv 表达式

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `make(chan)` / `make(chan, cap)` 和 `<-ch`（channel 接收）的前缀解析。这两个表达式是并发系统（P9）的词法前置，parser 在此仅产生对应的 AST 节点；运行期语义由 T108/T109 实现。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T017 | `MS_ND_MAKE`/`MS_ND_RECV` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3 表达式 — `MakeExpr` / `RecvExpr` |
| `syntax.md` | §2.2 `SendStmt` / `RecvStmt`（`select` 内） |
| `syntax.md` | §3.5 channel 操作 |
| `concurrency.md` | §3 Channel |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 注册 MS_TOK_MAKE 前缀 parseMake
                              # 注册 MS_TOK_ARROW_LEFT 前缀 parseRecv
```

---

## 实现要点

### 1. `make` 表达式

`syntax.md §2.3`：`MakeExpr = 'make' '(' 'chan' [ ',' Expr ] ')'`——`make` 后只允许 `chan` 关键字 + 可选容量表达式，无元素类型。

```c
// gParseRules[MS_TOK_MAKE] = { parseMake, NULL, PREC_NONE };
static MsNode* parseMake(MsParser* p) {
  MsSrcPos pos = p->prev.pos;
  msParserExpect(p, MS_TOK_LPAREN, "expected '(' after 'make'");
  msParserExpect(p, MS_TOK_CHAN, "expected 'chan' in make expression");

  MsNode* capExpr = NULL;
  if (msParserMatch(p, MS_TOK_COMMA)) {
    capExpr = msParseExpr(p);
  }
  msParserExpect(p, MS_TOK_RPAREN, "expected ')' after make arguments");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind           = MS_ND_MAKE;
  n->pos            = pos;
  n->makeExpr.capExpr = capExpr;
  return n;
}
```

### 2. `<-ch` channel 接收（前缀）

`syntax.md §2.3`：`RecvExpr = '<-' Expr`

```c
// gParseRules[MS_TOK_ARROW_LEFT] = { parseRecv, NULL, PREC_NONE };
// '<-' 在中缀位置不使用（发送 'ch <- val' 是语句，由 T026/stmt 处理）
static MsNode* parseRecv(MsParser* p) {
  MsSrcPos pos = p->prev.pos;
  MsNode* chanExpr = parsePrecedence(p, PREC_UNARY);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind          = MS_ND_RECV;
  n->pos           = pos;
  n->recv.chanExpr = chanExpr;
  return n;
}
```

### 3. `ch <- val` 发送（语句）

channel 发送不是表达式，而是语句（`syntax.md §2.2 SendStmt = Expr '<-' Expr`）。`SendStmt` 在文法中仅出现于 `SelectCase` 内。由 `msParseStmt`（T026 语句解析）在解析表达式语句后检测是否紧跟 `<-` 运算符，**不属于本任务实现范围**（本任务仅修改 `ms_parse_expr.c`），此处仅记录接口供 T026 参考：

```c
// 在 msParseStmt 中（T026 实现）：
MsNode* expr = msParseExpr(p);
if (msParserMatch(p, MS_TOK_ARROW_LEFT)) {
  MsNode* val = msParseExpr(p);
  MsNode* send = MS_ARENA_NEW(p->arena, MsNode);
  send->kind         = MS_ND_SEND;
  send->send.chanExpr = expr;
  send->send.val     = val;
  return send;
}
```

**注**：`syntax.md §2.3` 的 `MakeExpr` 文法中 `make` 只接受 `chan` 关键字 + 可选容量，不涉及元素类型（mslang 是动态类型语言）。无需 `parseChanType` 或类型表达式解析。

---

## 验收标准（checklist）

- [ ] `"make(chan)"` → `MS_ND_MAKE(capExpr=NULL)`（无缓冲 channel）。
- [ ] `"make(chan, 16)"` → `MS_ND_MAKE(capExpr=MS_ND_INT(16))`。
- [ ] `"<-ch"` → `MS_ND_RECV(chanExpr=MS_ND_IDENT("ch"))`。
- [ ] `"make(chan,)"` → 语法错误（逗号后无表达式）。
- [ ] `"make()"` → 语法错误（缺少 `chan` 关键字）。
- [ ] （T026 验收，非本任务）`"x := <-ch"` → `MS_ND_SHORT_DECL(name="x", init=MS_ND_RECV(ch))`。
- [ ] （T026 验收，非本任务）`"ch <- 42"` 作为语句 → `MS_ND_SEND(chanExpr=IDENT(ch), val=INT(42))`。
- [ ] （T026 验收，非本任务）`"v, ok := <-ch"` → 多目标短声明。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_make_recv.c`）

```c
#include <string.h>
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "ms_arena.h"

static MsNode* px(MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t)strlen(s), "<t>", a);
  return msParseExpr(&p);
}

static void testMakeChanUnbuffered(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "make(chan)");
  MS_ASSERT_EQ(n->kind, MS_ND_MAKE, "make");
  MS_ASSERT_TRUE(n->makeExpr.capExpr == NULL, "unbuffered");
  msArenaFree(&a);
}

static void testMakeChanBuffered(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "make(chan, 16)");
  MS_ASSERT_EQ(n->kind, MS_ND_MAKE, "make");
  MS_ASSERT_TRUE(n->makeExpr.capExpr != NULL, "has cap");
  MS_ASSERT_EQ(n->makeExpr.capExpr->litInt.ival, 16, "cap=16");
  msArenaFree(&a);
}

static void testRecv(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "<-ch");
  MS_ASSERT_EQ(n->kind, MS_ND_RECV, "recv");
  MS_ASSERT_EQ(n->recv.chanExpr->kind, MS_ND_IDENT, "chan ident");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testMakeChanUnbuffered);
  MS_RUN(testMakeChanBuffered);
  MS_RUN(testRecv);
  return msTestSummary();
}
```

### .ms 使用示例（T108/T109 后验证）

```ms
// 无缓冲 channel
ch := make(chan)
go func() { ch <- 42 }()
v := <-ch
print(v)   // 42

// 有缓冲 channel
bch := make(chan, 3)
bch <- "hello"
bch <- "world"
print(<-bch)   // hello
print(<-bch)   // world

// ok 模式（channel 关闭检测）
v2, ok := <-ch
print(ok)  // false（channel 已关闭）
```

---

## Benchmark

N/A（并发语义未实现，不可 bench；归入 T114 并发整体 bench）。

---

## 风险与边界

- **`<-` 歧义**：`<-` 在表达式位置是接收（前缀 `RecvExpr`）；在语句位置跟在 channel 表达式后是发送（`SendStmt`，仅出现在 `SelectCase` 内，由 T026/T032 处理）。Pratt 框架中仅注册前缀，发送由语句解析器单独处理。
- **`chan` 关键字**：`syntax.md §1.4` 已将 `chan` 列为关键字（`MS_TOK_CHAN`）。`make(chan)` 中 `chan` 是固定语法成分，不涉及类型表达式。
- **`make` vs 内置函数调用**：`make` 是关键字（非普通函数），在词法层产生 `MS_TOK_MAKE`，parser 不走 `parseCall` 路径；这与 Go 语言行为一致。
