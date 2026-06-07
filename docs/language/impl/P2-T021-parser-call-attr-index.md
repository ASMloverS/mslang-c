# P2-T021 调用 / 属性访问 / 下标 / 后缀运算符

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

在 Pratt 框架中注册 `()` 调用、`.` 属性访问、`[]` 下标/切片的中缀解析函数，以及后缀 `++`/`--` 运算符。这些运算符优先级最高（`PREC_CALL`），在所有二元运算符之上。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T017 | `ND_CALL`/`ND_ATTR`/`ND_INDEX`/`ND_SLICE` 节点 |
| P2-T020 | if-expr（优先级枚举最终版本） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3.6 后缀表达式（调用/属性/下标） |
| `syntax.md` | §2.4.1 函数调用参数列表（位置/关键字/`*args`/`**kwargs`） |
| `syntax.md` | §2.3.7 切片语法 `a[lo:hi:step]` |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 新增 parseCall / parseAttr / parseIndex / parsePostfixIncDec
```

---

## 实现要点

### 1. 属性访问 `.name`

```c
// gParseRules[TOK_DOT] = { NULL, parseAttr, PREC_CALL };
static MsNode* parseAttr(MsParser* p, MsNode* left) {
  expect(p, TOK_IDENT, "expected attribute name after '.'");
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind         = ND_ATTR;
  n->pos          = p->prev.pos;
  n->attr.obj     = left;
  n->attr.name    = p->prev.start;
  n->attr.nameLen = p->prev.len;
  return n;
}
```

### 2. 下标/切片 `a[key]` / `a[lo:hi:step]`

```c
// gParseRules[TOK_LBRACKET] infix = parseIndex（前缀由 T022 list literal 注册）
static MsNode* parseIndex(MsParser* p, MsNode* obj) {
  MsSrcPos pos = p->prev.pos;  // '['

  MsNode* lo   = NULL;
  MsNode* hi   = NULL;
  MsNode* step = NULL;

  // lo: 可选（遇 ':' 或 ']' 则省略）
  if (!check(p, TOK_COLON) && !check(p, TOK_RBRACKET)) {
    lo = msParseExpr(p);
  }

  if (match(p, TOK_COLON)) {
    // 切片模式
    if (!check(p, TOK_COLON) && !check(p, TOK_RBRACKET)) {
      hi = msParseExpr(p);
    }
    if (match(p, TOK_COLON)) {
      if (!check(p, TOK_RBRACKET)) {
        step = msParseExpr(p);
      }
    }
    expect(p, TOK_RBRACKET, "expected ']' after slice");
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind       = ND_SLICE;
    n->pos        = pos;
    n->slice.obj  = obj;
    n->slice.lo   = lo;
    n->slice.hi   = hi;
    n->slice.step = step;
    return n;
  } else {
    if (lo == NULL) parserError(p, "empty index not allowed");
    expect(p, TOK_RBRACKET, "expected ']' after index");
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind      = ND_INDEX;
    n->pos       = pos;
    n->index.obj = obj;
    n->index.key = lo;
    return n;
  }
}
```

### 3. 函数调用 `f(args…)`

```c
// gParseRules[TOK_LPAREN] infix = parseCall（前缀由 T023 group/tuple 注册）
static MsNode* parseCall(MsParser* p, MsNode* callee) {
  MsSrcPos pos = p->prev.pos;  // '('

  MsNodeList* args      = NULL;
  MsNodeList* kwargs    = NULL;
  MsNodeList** argTail  = &args;
  MsNodeList** kwTail   = &kwargs;

  while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
    MsNode* expr = NULL;

    if (match(p, TOK_STARSTAR)) {
      // **kwargs 展开
      MsNode* inner = parsePrecedence(p, PREC_OR);
      expr = MS_ARENA_NEW(p->arena, MsNode);
      expr->kind = ND_DOUBLESTAR_EXPR;
      expr->unary.operand = inner;
      MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
      item->node = expr; item->next = NULL;
      *kwTail = item; kwTail = &item->next;
    } else if (match(p, TOK_STAR)) {
      // *args 展开
      MsNode* inner = parsePrecedence(p, PREC_OR);
      expr = MS_ARENA_NEW(p->arena, MsNode);
      expr->kind = ND_STAR_EXPR;
      expr->unary.operand = inner;
      MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
      item->node = expr; item->next = NULL;
      *argTail = item; argTail = &item->next;
    } else {
      // 检查是否为 kwarg（ident 后跟 '='，而非 '=='）
      MsToken peek = msLexPeek(&p->lex);
      if (p->cur.kind == TOK_IDENT && peek.kind == TOK_ASSIGN) {
        advance(p);  // 消耗 ident
        const char* kname = p->prev.start;
        uint32_t    klen  = p->prev.len;
        advance(p);  // 消耗 '='
        MsNode* val = parsePrecedence(p, PREC_OR);
        MsNode* kw = MS_ARENA_NEW(p->arena, MsNode);
        kw->kind          = ND_KWARG_PAIR;
        kw->attr.name     = kname;
        kw->attr.nameLen  = klen;
        kw->attr.obj      = val;
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = kw; item->next = NULL;
        *kwTail = item; kwTail = &item->next;
      } else {
        // 普通位置参数
        MsNode* val = parsePrecedence(p, PREC_OR);
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = val; item->next = NULL;
        *argTail = item; argTail = &item->next;
      }
    }

    if (!match(p, TOK_COMMA)) break;
    if (check(p, TOK_RPAREN)) break;  // 允许尾随逗号
  }
  expect(p, TOK_RPAREN, "expected ')' after arguments");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind        = ND_CALL;
  n->pos         = pos;
  n->call.callee = callee;
  n->call.args   = args;
  n->call.kwargs = kwargs;
  return n;
}
```

### 4. 后缀 `++` / `--`

```c
// gParseRules[TOK_INC] = { NULL, parsePostfix, PREC_CALL };
// gParseRules[TOK_DEC] = { NULL, parsePostfix, PREC_CALL };
static MsNode* parsePostfix(MsParser* p, MsNode* left) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind             = ND_INC_DEC;
  n->pos              = p->prev.pos;
  n->inc_dec.target   = left;
  n->inc_dec.isInc    = (p->prev.kind == TOK_INC);
  return n;
}
```

---

## 验收标准（checklist）

- [ ] `"f()"` → `ND_CALL(callee=ND_IDENT("f"), args=[], kwargs=[])`。
- [ ] `"f(1, 2)"` → `ND_CALL(args=[ND_INT(1), ND_INT(2)])`。
- [ ] `"f(x=1)"` → `ND_CALL(kwargs=[ND_KWARG_PAIR("x", ND_INT(1))])`。
- [ ] `"f(*a, **b)"` → `args=[ND_STAR_EXPR(a)]`, `kwargs=[ND_DOUBLESTAR_EXPR(b)]`。
- [ ] `"f(1,)"` 尾随逗号合法。
- [ ] `"obj.x"` → `ND_ATTR(obj=ND_IDENT("obj"), name="x")`。
- [ ] `"a.b.c"` → 链式属性，根为 `ND_ATTR("c")`，子为 `ND_ATTR("b")`。
- [ ] `"a[0]"` → `ND_INDEX(obj=a, key=0)`。
- [ ] `"a[1:3]"` → `ND_SLICE(lo=1, hi=3, step=NULL)`。
- [ ] `"a[::2]"` → `ND_SLICE(lo=NULL, hi=NULL, step=2)`。
- [ ] `"x++"` → `ND_INC_DEC(isInc=true)`。
- [ ] `"f().g[0]++"` → 链式，最外层 `ND_INC_DEC`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_call_attr.c`）

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

static void testEmptyCall(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "f()");
  MS_ASSERT_EQ(n->kind, ND_CALL, "call");
  MS_ASSERT_TRUE(n->call.args == NULL, "no args");
  msArenaFree(&a);
}

static void testKwarg(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "f(x=1)");
  MS_ASSERT_EQ(n->kind, ND_CALL, "call");
  MS_ASSERT_TRUE(n->call.kwargs != NULL, "has kwargs");
  MS_ASSERT_EQ(n->call.kwargs->node->kind, ND_KWARG_PAIR, "kwarg");
  msArenaFree(&a);
}

static void testSlice(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "a[1:3:2]");
  MS_ASSERT_EQ(n->kind, ND_SLICE, "slice");
  MS_ASSERT_EQ(n->slice.lo->lit_int.ival,   1, "lo=1");
  MS_ASSERT_EQ(n->slice.hi->lit_int.ival,   3, "hi=3");
  MS_ASSERT_EQ(n->slice.step->lit_int.ival, 2, "step=2");
  msArenaFree(&a);
}

static void testAttrChain(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "a.b.c");
  MS_ASSERT_EQ(n->kind,              ND_ATTR, "outer attr c");
  MS_ASSERT_EQ(n->attr.obj->kind,    ND_ATTR, "inner attr b");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testEmptyCall);
  MS_RUN(testKwarg);
  MS_RUN(testSlice);
  MS_RUN(testAttrChain);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
func add(a, b) { return a + b }
print(add(1, 2))       // 3
print(add(a=1, b=2))   // 3

s := "hello"
print(s.upper())       // HELLO

lst := [10, 20, 30, 40, 50]
print(lst[0])          // 10
print(lst[1:3])        // [20, 30]
print(lst[::2])        // [10, 30, 50]

x := 0
x++
print(x)               // 1
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **kwarg vs 赋值消歧**：`f(x=1)` 是 kwarg，`f(x==1)` 是布尔比较（普通位置参数）。`msLexPeek` 向前看一个 token 判断 `=` vs `==`。
- **`a[expr]` vs `a[:]`**：切片与下标通过 `:` 的存在区分；空下标 `a[]` 语法错误。
- **`++`/`--` lvalue 检查**：parser 不验证 lvalue；编译器（T040）在生成 store 指令时检查并报错。
- **换行与调用**：`f\n(args)` 因 `f` 触发 ASI，parser 收到 `NEWLINE` 后不进入 `parseCall`；与 Go 行为一致（`f` 和 `(args)` 是独立语句，`(args)` 是语法错误）。
