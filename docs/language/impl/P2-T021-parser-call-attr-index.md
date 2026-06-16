# P2-T021 调用 / 属性访问 / 下标 / 后缀运算符

> **状态**：✅ 已完成

---

## 任务目标 / 背景

在 Pratt 框架中注册 `()` 调用、`.` 属性访问、`[]` 下标/切片的中缀解析函数，以及后缀 `++`/`--` 运算符。这些运算符优先级最高（`PREC_CALL`），在所有二元运算符之上。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T017 | `MS_ND_CALL`/`MS_ND_ATTR`/`MS_ND_INDEX`/`MS_ND_SLICE` 节点 |
| P2-T020 | if-expr（优先级枚举最终版本） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §1.10 运算符与界符 |
| `syntax.md` | §2.3 表达式（PostfixExpr / CallArgs / ArgList EBNF） |
| `syntax.md` | §2.3 表达式（PostfixExpr，注：切片 EBNF 待补充，见「风险与边界」） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 新增 parseCall / parseAttr / parseIndex / parsePostfix
```

---

## 实现要点

### 1. 属性访问 `.name`

```c
// gParseRules[MS_TOK_DOT] = { NULL, parseAttr, PREC_CALL };
static MsNode* parseAttr(MsParser* p, MsNode* left) {
  msParserExpect(p, MS_TOK_IDENT, "expected attribute name after '.'");
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind         = MS_ND_ATTR;
  n->pos          = p->prev.pos;
  n->attr.obj     = left;
  n->attr.name    = p->prev.start;
  n->attr.nameLen = p->prev.len;
  return n;
}
```

### 2. 下标/切片 `a[key]` / `a[lo:hi:step]`

```c
// gParseRules[MS_TOK_LBRACKET] infix = parseIndex（前缀由 T022 list literal 注册）
static MsNode* parseIndex(MsParser* p, MsNode* obj) {
  struct MsSrcPos pos = p->prev.pos;  // '['

  MsNode* lo   = NULL;
  MsNode* hi   = NULL;
  MsNode* step = NULL;

  // lo: 可选（遇 ':' 或 ']' 则省略）
  if (!msParserCheck(p, MS_TOK_COLON) && !msParserCheck(p, MS_TOK_RBRACKET)) {
    lo = msParseExpr(p);
  }

  if (msParserMatch(p, MS_TOK_COLON)) {
    // 切片模式
    if (!msParserCheck(p, MS_TOK_COLON) && !msParserCheck(p, MS_TOK_RBRACKET)) {
      hi = msParseExpr(p);
    }
    if (msParserMatch(p, MS_TOK_COLON)) {
      if (!msParserCheck(p, MS_TOK_RBRACKET)) {
        step = msParseExpr(p);
      }
    }
    msParserExpect(p, MS_TOK_RBRACKET, "expected ']' after slice");
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind       = MS_ND_SLICE;
    n->pos        = pos;
    n->slice.obj  = obj;
    n->slice.lo   = lo;
    n->slice.hi   = hi;
    n->slice.step = step;
    return n;
  } else {
    if (lo == NULL) {
      msParserError(p, "empty index not allowed");
    }
    msParserExpect(p, MS_TOK_RBRACKET, "expected ']' after index");
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind      = MS_ND_INDEX;
    n->pos       = pos;
    n->index.obj = obj;
    n->index.key = lo;
    return n;
  }
}
```

### 3. 函数调用 `f(args…)`

```c
// gParseRules[MS_TOK_LPAREN] infix = parseCall（前缀由 T023 group/tuple 注册）
static MsNode* parseCall(MsParser* p, MsNode* callee) {
  struct MsSrcPos pos = p->prev.pos;  // '('

  MsNodeList* args      = NULL;
  MsNodeList* kwargs    = NULL;
  MsNodeList** argTail  = &args;
  MsNodeList** kwTail   = &kwargs;

  while (!msParserCheck(p, MS_TOK_RPAREN) && !msParserCheck(p, MS_TOK_EOF)) {
    MsNode* expr = NULL;

    if (msParserMatch(p, MS_TOK_STARSTAR)) {
      // **kwargs 展开
      MsNode* inner = msParseExpr(p);
      expr = MS_ARENA_NEW(p->arena, MsNode);
      expr->kind = MS_ND_DOUBLESTAR_EXPR;
      expr->starExpr.expr = inner;
      MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
      item->node = expr;
      item->next = NULL;
      *kwTail = item;
      kwTail = &item->next;
    } else if (msParserMatch(p, MS_TOK_STAR)) {
      // *args 展开
      MsNode* inner = msParseExpr(p);
      expr = MS_ARENA_NEW(p->arena, MsNode);
      expr->kind = MS_ND_STAR_EXPR;
      expr->starExpr.expr = inner;
      MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
      item->node = expr;
      item->next = NULL;
      *argTail = item;
      argTail = &item->next;
    } else {
      // 检查是否为 kwarg（ident 后跟 '='，而非 '=='）
      // 使用 msParserPeekNext 取 cur 之后一个 token，避免与 lexer peek 缓冲层耦合
      struct MsToken peek = msParserPeekNext(p);
      if (p->cur.kind == MS_TOK_IDENT && peek.kind == MS_TOK_ASSIGN) {
        msParserAdvance(p);  // 消耗 ident
        const char* kname = p->prev.start;
        msParserAdvance(p);  // 消耗 '='
        MsNode* val = msParseExpr(p);
        MsNode* kw = MS_ARENA_NEW(p->arena, MsNode);
        kw->kind            = MS_ND_KWARG_PAIR;
        kw->kwargPair.name  = kname;
        kw->kwargPair.value = val;
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = kw;
        item->next = NULL;
        *kwTail = item;
        kwTail = &item->next;
      } else {
        // 普通位置参数
        MsNode* val = msParseExpr(p);
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = val;
        item->next = NULL;
        *argTail = item;
        argTail = &item->next;
      }
    }

    if (!msParserMatch(p, MS_TOK_COMMA)) break;
    if (msParserCheck(p, MS_TOK_RPAREN)) break;  // 允许尾随逗号
  }
  msParserExpect(p, MS_TOK_RPAREN, "expected ')' after arguments");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind        = MS_ND_CALL;
  n->pos         = pos;
  n->call.callee = callee;
  n->call.args   = args;
  n->call.kwargs = kwargs;
  return n;
}
```

### 4. 后缀 `++` / `--`

```c
// gParseRules[MS_TOK_INC] = { NULL, parsePostfix, PREC_CALL };
// gParseRules[MS_TOK_DEC] = { NULL, parsePostfix, PREC_CALL };
// 四类后缀算子（.  ()  []  ++/--）的 ParseRule 均为 { NULL, fn, PREC_CALL }，
// 左结合由 Pratt 主循环驱动；parsePostfix 不向右递归，结合性正确。
static MsNode* parsePostfix(MsParser* p, MsNode* left) {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind           = MS_ND_INC_DEC;
  n->pos            = p->prev.pos;
  n->incDec.target  = left;
  n->incDec.isInc   = (p->prev.kind == MS_TOK_INC);
  return n;
}
```

---

## 验收标准（checklist）

- [ ] `"f()"` → `MS_ND_CALL(callee=MS_ND_IDENT("f"), args=[], kwargs=[])`。
- [ ] `"f(1, 2)"` → `MS_ND_CALL(args=[MS_ND_INT(1), MS_ND_INT(2)])`。
- [ ] `"f(x=1)"` → `MS_ND_CALL(kwargs=[MS_ND_KWARG_PAIR("x", MS_ND_INT(1))])`。
- [ ] `"f(*a, **b)"` → `args=[MS_ND_STAR_EXPR(a)]`, `kwargs=[MS_ND_DOUBLESTAR_EXPR(b)]`。
- [ ] `"f(1,)"` 尾随逗号合法。
- [ ] `"obj.x"` → `MS_ND_ATTR(obj=MS_ND_IDENT("obj"), name="x")`。
- [ ] `"a.b.c"` → 链式属性，根为 `MS_ND_ATTR("c")`，子为 `MS_ND_ATTR("b")`。
- [ ] `"a[0]"` → `MS_ND_INDEX(obj=a, key=0)`。
- [ ] `"a[1:3]"` → `MS_ND_SLICE(lo=1, hi=3, step=NULL)`。
- [ ] `"a[::2]"` → `MS_ND_SLICE(lo=NULL, hi=NULL, step=2)`。
- [ ] `"x++"` → `MS_ND_INC_DEC(isInc=true)`。
- [ ] `"f().g[0]++"` → 链式，最外层 `MS_ND_INC_DEC`；四类后缀算子同为 `PREC_CALL`，左结合正确。

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
  MS_ASSERT_EQ(n->kind, MS_ND_CALL, "call");
  MS_ASSERT_TRUE(n->call.args == NULL, "no args");
  msArenaFree(&a);
}

static void testKwarg(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "f(x=1)");
  MS_ASSERT_EQ(n->kind, MS_ND_CALL, "call");
  MS_ASSERT_TRUE(n->call.kwargs != NULL, "has kwargs");
  MS_ASSERT_EQ(n->call.kwargs->node->kind, MS_ND_KWARG_PAIR, "kwarg");
  msArenaFree(&a);
}

static void testSlice(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "a[1:3:2]");
  MS_ASSERT_EQ(n->kind, MS_ND_SLICE, "slice");
  MS_ASSERT_EQ(n->slice.lo->lit_int.ival,   1, "lo=1");
  MS_ASSERT_EQ(n->slice.hi->lit_int.ival,   3, "hi=3");
  MS_ASSERT_EQ(n->slice.step->lit_int.ival, 2, "step=2");
  msArenaFree(&a);
}

static void testAttrChain(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "a.b.c");
  MS_ASSERT_EQ(n->kind,              MS_ND_ATTR, "outer attr c");
  MS_ASSERT_EQ(n->attr.obj->kind,    MS_ND_ATTR, "inner attr b");
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
func add(a, b) {
  return a + b
}

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

- **kwarg vs 赋值消歧**：`f(x=1)` 是 kwarg，`f(x==1)` 是布尔比较（普通位置参数）。实现须通过 `msParserPeekNext`（parser 层统一前瞻接口）向前看一个 token 判断 `=` vs `==`，不可直接调用 `msLexerPeek` 绕过 parser 的 token 缓冲层，否则可能取到 cur 之后第二个 token 导致消歧错位。`msParserPeekNext` 接口需在 T018/T026 中约定并实现。
- **切片 EBNF 缺失**：`a[lo:hi:step]` 切片语法尚未写入 `syntax.md §2.3 PostfixExpr`（当前仅有 `'[' Expr ']'`）。本任务实现先于规范，需联动在 `syntax.md` §2.3 补充切片产生式（如 `Subscript = '[' [Expr] [ ':' [Expr] [ ':' [Expr] ] ] ']'`）。
- **`a[expr]` vs `a[:]`**：切片与下标通过 `:` 的存在区分；空下标 `a[]` 语法错误。
- **`++`/`--` lvalue 检查**：parser 不验证 lvalue；编译器（T040）在生成 store 指令时检查并报错。
- **换行与调用**：`f\n(args)` 因 `f` 触发 ASI，parser 收到 `NEWLINE` 后不进入 `parseCall`；与 Go 行为一致（`f` 和 `(args)` 是独立语句，`(args)` 是语法错误）。
