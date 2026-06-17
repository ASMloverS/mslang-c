# P2-T024 函数字面量 / 匿名函数（闭包）

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现 `func(params) { body }` 作为**表达式**（函数字面量/匿名函数/闭包）的前缀解析，产生 `MS_ND_FUNC_DECL`（`name=NULL` 表示匿名）。

与 T034 中的**命名函数声明**（语句级）不同，本任务专注于函数作为值（`func` 关键字出现在表达式上下文中，如赋值右侧、调用参数等）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T017 | `MS_ND_FUNC_DECL`/`MS_ND_PARAM` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3 表达式 — `FuncLiteral` / `PrimaryExpr` |
| `syntax.md` | §2.1 顶层结构 — `ParamList` / `Param`（默认值/`...args`/`**kwargs`） |
| `syntax.md` | §3.3 函数字面量与闭包（upvalue 语义） |
| `syntax.md` | §3.4 可变参数 |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 注册 MS_TOK_FUNC 前缀 parseFuncLit（static）
src/parser/ms_parser.c       # 共享 msParseParamList（T034 命名函数复用）
include/mslang/ms_parser.h   # 声明 msParseParamList
```

---

## 实现要点

### 1. 参数列表解析（`msParseParamList`，供 T024 与 T034 共用）

复用 `ms_ast.h` 已有的 `param` union 成员（`name`/`nameLen`/`defaultVal`/`isVararg`/`isKwarg`）。

参数顺序约束（对应 `syntax.md §2.1 ParamList`）：普通参数（可带默认值）→ `...args` → `**kwargs`。§3.4 规定可变参数须有前导位置参数。

```c
// 在 ms_parser.h 中声明（跨文件共享 API）
MsNodeList* msParseParamList(MsParser* p);

// 解析 '(' 之后的参数列表，直到 ')'
MsNodeList* msParseParamList(MsParser* p) {
  MsNodeList* params = NULL;
  MsNodeList** tail  = &params;
  bool sawVararg = false;
  bool sawKwarg  = false;
  bool sawDefault = false;

  while (!check(p, MS_TOK_RPAREN) && !check(p, MS_TOK_EOF)) {
    MsNode* param = MS_ARENA_NEW(p->arena, MsNode);
    param->kind = MS_ND_PARAM;
    param->pos  = p->cur.pos;

    if (match(p, MS_TOK_STARSTAR)) {
      // **kwargs
      if (sawKwarg) parserError(p, "only one **kwargs allowed");
      expect(p, MS_TOK_IDENT, "expected parameter name after '**'");
      param->param.name    = p->prev.start;
      param->param.nameLen = p->prev.len;
      param->param.isKwarg = true;
      sawKwarg = true;
    } else if (match(p, MS_TOK_DOTDOTDOT)) {
      // ...args
      if (sawVararg) parserError(p, "only one ...args allowed");
      if (!params) parserError(p, "variadic parameter requires a leading positional parameter");
      expect(p, MS_TOK_IDENT, "expected parameter name after '...'");
      param->param.name     = p->prev.start;
      param->param.nameLen  = p->prev.len;
      param->param.isVararg = true;
      sawVararg = true;
    } else {
      // 普通参数（可选默认值）
      if (sawVararg) parserError(p, "positional parameter after ...args");
      expect(p, MS_TOK_IDENT, "expected parameter name");
      param->param.name    = p->prev.start;
      param->param.nameLen = p->prev.len;
      if (match(p, MS_TOK_ASSIGN)) {
        param->param.defaultVal = msParseExpr(p);
        sawDefault = true;
      } else if (sawDefault) {
        parserError(p, "non-default parameter after default parameter");
      }
    }

    MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
    item->node = param; item->next = NULL;
    *tail = item; tail = &item->next;

    if (!match(p, MS_TOK_COMMA)) break;
  }
  return params;
}
```

### 2. 函数字面量前缀

`FuncLiteral = [ 'async' ] 'func' '(' ParamList ')' Block`——表达式上下文的 `func` 后直接是 `(`，**不允许**带名字（带名字的 `FuncDecl` 属语句级，由 T034 负责）。

```c
// gParseRules[MS_TOK_FUNC] = { parseFuncLit, NULL, PREC_NONE };
static MsNode* parseFuncLit(MsParser* p) {
  MsSrcPos pos = p->prev.pos;  // 'func'

  expect(p, MS_TOK_LPAREN, "expected '(' after 'func'");
  MsNodeList* params = msParseParamList(p);
  expect(p, MS_TOK_RPAREN, "expected ')' after parameters");
  MsNode* body = parseBlock(p);  // { stmts }（T027 实现 parseBlock）

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind            = MS_ND_FUNC_DECL;
  n->pos             = pos;
  n->funcDecl.name   = NULL;
  n->funcDecl.params = params;
  n->funcDecl.body   = body;
  n->funcDecl.isAsync = false;
  return n;
}
```

### 3. `async func` 字面量

`ms_ast.h` 中 `MS_ND_FUNC_DECL` / `MS_ND_ASYNC_FUNC` 共用 `funcDecl` union 成员，含 `bool isAsync` 字段。本任务统一使用 `kind = MS_ND_FUNC_DECL` 且置 `isAsync = true` 表示异步函数字面量（与 union 注释语义一致，避免 kind/isAsync 双重标记冲突）。

```c
// gParseRules[MS_TOK_ASYNC] = { parseAsyncFuncLit, NULL, PREC_NONE };
static MsNode* parseAsyncFuncLit(MsParser* p) {
  MsSrcPos pos = p->prev.pos;
  expect(p, MS_TOK_FUNC, "expected 'func' after 'async'");
  MsNode* fn = parseFuncLit(p);
  fn->pos             = pos;
  fn->funcDecl.isAsync = true;
  return fn;
}
```

### 4. 参数顺序约束

参数顺序（`syntax.md §2.1 ParamList`）：普通参数（可带默认值）→ `...args` → `**kwargs`：

```
func f(a, b=1, ...args, **kw) { … }
```

Parser 需检查：
- `**kwargs` 必须最后。
- `...args` 后不允许普通位置参数。
- `...args` 须有前导位置参数（§3.4 约束，`func(...args) {}` 为语法错误）。
- 默认值只能在无默认值参数之后（`a, b=1, c` 中 `c` 是语法错误）。

---

## 验收标准（checklist）

- [ ] `"func() {}"` → `MS_ND_FUNC_DECL(name=NULL, params=[], body=MS_ND_BLOCK([]))`。
- [ ] `"func(a, b) { return a + b }"` → params=[a, b]，body 含 `MS_ND_RETURN`。
- [ ] `"func(a, b=1) {}"` → b 有 `defaultVal = MS_ND_INT(1)`。
- [ ] `"func(first, ...args) {}"` → params=[first, `MS_ND_PARAM(isVararg=true)`]。
- [ ] `"func(first, **kw) {}"` → params=[first, `MS_ND_PARAM(isKwarg=true)`]。
- [ ] `"func(a, ...args, **kw) {}"` → 三参数，顺序正确。
- [ ] `"var f = func() {}"` → 赋值右侧的函数字面量合法。
- [ ] `"async func() {}"` → `MS_ND_FUNC_DECL(isAsync=true)`。
- [ ] `"func(b=1, a) {}"` → 语法错误（无默认值参数在默认值参数之后）。
- [ ] `"func(...args) {}"` → 语法错误（可变参数须有前导位置参数）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_func_literal.c`）

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

static void testEmptyFuncLit(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "func() {}");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func decl");
  MS_ASSERT_TRUE(n->funcDecl.name == NULL, "anonymous");
  MS_ASSERT_TRUE(n->funcDecl.params == NULL, "no params");
  msArenaFree(&a);
}

static void testFuncWithParams(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "func(a, b) { return a }");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func");
  int cnt = 0;
  for (MsNodeList* l = n->funcDecl.params; l; l = l->next) cnt++;
  MS_ASSERT_EQ(cnt, 2, "2 params");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testEmptyFuncLit);
  MS_RUN(testFuncWithParams);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// 匿名函数赋值
doubleIt := func(x) { return x * 2 }
print(doubleIt(5))   // 10

// 高阶函数
apply := func(f, x) { return f(x) }
print(apply(doubleIt, 3))   // 6

// 闭包捕获（命名函数声明 makeAdder 属 T034，此处展示匿名闭包）
makeAdder := func(n) {
    return func(x) { return x + n }
}
add5 := makeAdder(5)
print(add5(3))    // 8
print(add5(10))   // 15

// vararg（§3.4：可变参数须有前导位置参数）
sum := func(first, ...rest) {
    s := first
    for v in rest { s += v }
    return s
}
print(sum(1, 2, 3, 4))  // 10
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`parseBlock` 依赖**：`parseFuncLit` 调用 `parseBlock`（预期签名：`MsNode* parseBlock(MsParser* p)`，返回 `MS_ND_BLOCK`），后者在 T027（if/else）中实现。若严格按任务顺序，T024 需在 T027 后实现；但 `parseBlock` 实现极简（消耗 `{` 循环解析语句到 `}`），可在 T024 中预先实现骨架并与 T027 约定接口。
- **参数默认值与闭包**：默认值在定义时求值（与 Python 相同，非运行时），这是语义问题，compiler（T043）负责处理。
- **函数字面量始终匿名**：`FuncLiteral` 文法不允许带名字（`func` 后直接是 `(`）。带名字的 `func name() {}` 属语句级 `FuncDecl`，由 T034 负责。
