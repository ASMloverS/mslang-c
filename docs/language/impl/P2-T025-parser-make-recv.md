# P2-T025 make / recv 表达式

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `make(chanType, size)` 和 `<-ch`（channel 接收）的前缀解析。这两个表达式是并发系统（P9）的词法前置，parser 在此仅产生对应的 AST 节点；运行期语义由 T108/T109 实现。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T017 | `ND_MAKE`/`ND_RECV` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3.13 make 表达式（`make(chan T, n)` / `make([]T, n)`） |
| `syntax.md` | §2.6.1 channel 操作（`<-ch` 接收，`ch <- val` 发送） |
| `concurrency.md` | §1 goroutine / §2 channel |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 注册 TOK_MAKE 前缀 parseMake
                              # 注册 TOK_ARROW_LEFT 前缀 parseRecv
```

---

## 实现要点

### 1. `make` 表达式

```c
// gParseRules[TOK_MAKE] = { parseMake, NULL, PREC_NONE };
// 注意：make 是关键字（T007 的关键字表），TOK_MAKE 已定义
static MsNode* parseMake(MsParser* p) {
    MsSrcPos pos = p->prev.pos;
    expect(p, TOK_LPAREN, "expected '(' after 'make'");

    // make 参数：第一个是类型表达式（chan T / []T 等），第二个是可选大小
    MsNode* typeExpr = parsePrecedence(p, PREC_OR);
    MsNode* sizeExpr = NULL;
    if (match(p, TOK_COMMA)) {
        sizeExpr = parsePrecedence(p, PREC_OR);
    }
    expect(p, TOK_RPAREN, "expected ')' after make arguments");

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind             = ND_MAKE;
    n->pos              = pos;
    n->make_expr.typeExpr = typeExpr;
    n->make_expr.sizeExpr = sizeExpr;
    return n;
}
```

### 2. `<-ch` channel 接收（前缀）

```c
// gParseRules[TOK_ARROW_LEFT] = { parseRecv, NULL, PREC_NONE };
// '<-' 在中缀位置不使用（发送 'ch <- val' 是语句，由 T026/stmt 处理）
static MsNode* parseRecv(MsParser* p) {
    MsSrcPos pos = p->prev.pos;
    // '<-' 后接 channel 表达式
    MsNode* chanExpr = parsePrecedence(p, PREC_UNARY);

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind           = ND_RECV;
    n->pos            = pos;
    n->recv.chan_expr  = chanExpr;
    return n;
}
```

### 3. `ch <- val` 发送（语句）

channel 发送不是表达式，而是语句（`syntax.md §2.6.2`）。由 `msParseStmt` 在解析表达式语句后检测是否紧跟 `<-` 运算符：

```c
// 在 msParseStmt 中：
MsNode* expr = msParseExpr(p);
if (match(p, TOK_ARROW_LEFT)) {
    // ch <- val：expr 是 channel 表达式，右侧是值
    MsNode* val = msParseExpr(p);
    MsNode* send = MS_ARENA_NEW(p->arena, MsNode);
    send->kind          = ND_SEND;
    send->send.chan_expr = expr;
    send->send.val      = val;
    return send;
}
// 否则继续赋值/短声明判断
```

### 4. 类型表达式解析（`chan T` / `[]T`）

初版 parser 将类型表达式视为普通表达式（标识符链 `chan.Int`、`[]int` 等）；在 `make(chan T, n)` 中，`chan` 是关键字（`TOK_CHAN`），`T` 是标识符，二者之间无运算符——需要特殊处理：

```c
// parseMake 中，typeExpr 通过 parseChanType 单独解析：
static MsNode* parseChanType(MsParser* p) {
    if (match(p, TOK_CHAN)) {
        // chan T → ND_BINARY(TOK_CHAN, nil, typeIdent)（临时表示）
        MsNode* elemType = parsePrecedence(p, PREC_PRIMARY);
        MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
        n->kind           = ND_BINARY;
        n->binary.op      = TOK_CHAN;
        n->binary.left    = NULL;
        n->binary.right   = elemType;
        return n;
    }
    // 其他类型（[]T、map[K]V 等）初版简化：当作标识符处理
    return parsePrecedence(p, PREC_OR);
}
```

**注**：mslang 类型系统是运行时动态类型，`make(chan int, 10)` 中的 `int` 和 `chan` 更多是语法提示，运行时按实际值类型处理（参见 `type-system.md §4.1`）。初版 parser 不需要完整类型表达式 AST，只需记录足够信息供 VM 创建 channel。

---

## 验收标准（checklist）

- [ ] `"make(chan int, 10)"` → `ND_MAKE(typeExpr=ND_BINARY(TOK_CHAN, nil, IDENT("int")), sizeExpr=10)`。
- [ ] `"make(chan int)"` → `ND_MAKE(sizeExpr=NULL)`（无缓冲 channel）。
- [ ] `"<-ch"` → `ND_RECV(chan_expr=ND_IDENT("ch"))`。
- [ ] `"x := <-ch"` → `ND_SHORT_DECL(name="x", init=ND_RECV(ch))`。
- [ ] `"ch <- 42"` 作为语句 → `ND_SEND(chan_expr=IDENT(ch), val=INT(42))`。
- [ ] `"v, ok := <-ch"` → `ND_SHORT_DECL(多目标, init=ND_RECV(ch))`（T026 解包赋值中处理）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_make_recv.c`）

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

static void testMakeChan(void) {
    MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "make(chan int, 10)");
    MS_ASSERT_EQ(n->kind, ND_MAKE, "make");
    MS_ASSERT_TRUE(n->make_expr.sizeExpr != NULL, "has size");
    MS_ASSERT_EQ(n->make_expr.sizeExpr->lit_int.ival, 10, "size=10");
    msArenaFree(&a);
}

static void testRecv(void) {
    MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "<-ch");
    MS_ASSERT_EQ(n->kind, ND_RECV, "recv");
    MS_ASSERT_EQ(n->recv.chan_expr->kind, ND_IDENT, "chan ident");
    msArenaFree(&a);
}

int main(void) {
    MS_RUN(testMakeChan);
    MS_RUN(testRecv);
    return msTestSummary();
}
```

### .ms 使用示例（T108/T109 后验证）

```ms
// 无缓冲 channel
ch := make(chan int)
go func() { ch <- 42 }()
v := <-ch
print(v)   // 42

// 有缓冲 channel
bch := make(chan string, 3)
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

- **`<-` 歧义**：`<-` 在表达式位置是接收（前缀）；在语句位置跟在 channel 表达式后是发送（中缀/语句）。Pratt 框架中仅注册前缀，发送由语句解析器单独处理。
- **`chan` 关键字与类型**：`chan` 作为类型关键字用于 `make`；`syntax.md §1.4` 已将 `chan` 列为关键字（`TOK_CHAN`）。初版 type 表达式解析不完整，留待 T034（func 类型注解）补充。
- **`make` vs 内置函数调用**：`make` 是关键字（非普通函数），在词法层产生 `TOK_MAKE`，parser 不走 `parseCall` 路径；这与 Go 语言行为一致。
