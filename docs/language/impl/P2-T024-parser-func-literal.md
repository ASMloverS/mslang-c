# P2-T024 函数字面量 / 匿名函数（闭包）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `func(params) { body }` 作为**表达式**（函数字面量/匿名函数/闭包）的前缀解析，产生 `ND_FUNC_DECL`（`name=NULL` 表示匿名）。

与 T034 中的**命名函数声明**（语句级）不同，本任务专注于函数作为值（`func` 关键字出现在表达式上下文中，如赋值右侧、调用参数等）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T017 | `ND_FUNC_DECL`/`ND_PARAM` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.5.2 函数字面量（匿名 func） |
| `syntax.md` | §2.5.3 参数列表（默认值/`*args`/`**kwargs`） |
| `syntax.md` | §2.5.4 闭包捕获（自由变量，运行期 upvalue） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 注册 TOK_FUNC 前缀 parseFuncLit
src/parser/ms_parser.c       # 共享 parseParamList（T034 命名函数复用）
```

---

## 实现要点

### 1. 参数列表解析（`parseParamList`，供 T024 与 T034 共用）

```c
// 解析 '(' 之后的参数列表，直到 ')'
// 参数节点：ND_PARAM（在 T017 节点中追加）
MsNodeList* parseParamList(MsParser* p) {
    MsNodeList* params = NULL;
    MsNodeList** tail  = &params;
    bool sawVararg  = false;
    bool sawKwarg   = false;

    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
        MsNode* param = MS_ARENA_NEW(p->arena, MsNode);
        param->kind  = ND_PARAM;
        param->pos   = p->cur.pos;

        if (match(p, TOK_STARSTAR)) {
            // **kwargs 参数
            if (sawKwarg) parserError(p, "only one **kwargs allowed");
            expect(p, TOK_IDENT, "expected parameter name after '**'");
            param->func_decl.name    = p->prev.start;  // 复用 func_decl 字段
            // 单独 param 结构（T017 中 ND_PARAM 有独立字段）：
            // param->param.name    = p->prev.start;
            // param->param.namelen = p->prev.len;
            // param->param.is_kwarg   = true;
            // param->param.default_   = NULL;
            sawKwarg = true;
        } else if (match(p, TOK_STAR)) {
            // *args 参数
            if (sawVararg) parserError(p, "only one *args allowed");
            expect(p, TOK_IDENT, "expected parameter name after '*'");
            // param->param.is_vararg = true;
            sawVararg = true;
        } else {
            // 普通参数（可选默认值）
            expect(p, TOK_IDENT, "expected parameter name");
            // param->param.name    = p->prev.start;
            // param->param.namelen = p->prev.len;
            if (match(p, TOK_ASSIGN)) {
                // 默认值
                // param->param.default_ = parsePrecedence(p, PREC_OR);
            }
        }

        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = param; item->next = NULL;
        *tail = item; tail = &item->next;

        if (!match(p, TOK_COMMA)) break;
    }
    return params;
}
```

**注**：`ND_PARAM` 节点需要独立的 union 字段（`param.name`/`param.namelen`/`param.default_`/`param.is_vararg`/`param.is_kwarg`），在 T017 的 `MsNode` union 中追加。

### 2. 函数字面量前缀

```c
// gParseRules[TOK_FUNC] = { parseFuncLit, NULL, PREC_NONE };
static MsNode* parseFuncLit(MsParser* p) {
    MsSrcPos pos = p->prev.pos;  // 'func'
    const char* name    = NULL;
    uint32_t    nameLen = 0;

    // 可选名称（命名函数字面量，如 `var f = func myFunc() {}`）
    if (check(p, TOK_IDENT)) {
        advance(p);
        name    = p->prev.start;
        nameLen = p->prev.len;
    }

    expect(p, TOK_LPAREN, "expected '(' after 'func'");
    MsNodeList* params = parseParamList(p);
    expect(p, TOK_RPAREN, "expected ')' after parameters");
    MsNode* body = parseBlock(p);   // { stmts }（T027 实现 parseBlock）

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind                 = ND_FUNC_DECL;
    n->pos                  = pos;
    n->func_decl.name       = name;
    n->func_decl.params     = params;
    n->func_decl.body       = body;
    n->func_decl.is_async   = false;
    return n;
}
```

### 3. `async func` 字面量

```c
// gParseRules[TOK_ASYNC] = { parseAsyncFuncLit, NULL, PREC_NONE };
static MsNode* parseAsyncFuncLit(MsParser* p) {
    MsSrcPos pos = p->prev.pos;
    expect(p, TOK_FUNC, "expected 'func' after 'async'");
    MsNode* fn = parseFuncLit(p);   // 复用，fn->kind = ND_FUNC_DECL
    fn->func_decl.is_async = true;
    fn->kind = ND_ASYNC_FUNC;
    return fn;
}
```

### 4. 参数顺序约束

参数顺序：普通参数 → `*args` → 关键字专用参数 → `**kwargs`：

```
func f(a, b=1, *args, key=2, **kw) { … }
```

Parser 需检查：
- `**kwargs` 必须最后。
- `*args` 后只能有关键字专用参数（无位置参数）。
- 默认值只能在无默认值参数之后（`a, b=1, c` 中 `c` 是语法错误）。

---

## 验收标准（checklist）

- [ ] `"func() {}"` → `ND_FUNC_DECL(name=NULL, params=[], body=ND_BLOCK([]))`。
- [ ] `"func(a, b) { return a + b }"` → params=[a, b]，body 含 `ND_RETURN`。
- [ ] `"func(a, b=1) {}"` → b 有默认值 `ND_INT(1)`。
- [ ] `"func(*args) {}"` → params=[`ND_PARAM(is_vararg=true)`]。
- [ ] `"func(**kw) {}"` → params=[`ND_PARAM(is_kwarg=true)`]。
- [ ] `"func(a, *args, **kw) {}"` → 三参数，顺序正确。
- [ ] `"var f = func() {}"` → 赋值右侧的函数字面量合法。
- [ ] `"async func() {}"` → `ND_ASYNC_FUNC`。
- [ ] `"func(b=1, a) {}"` → 语法错误（无默认值参数在默认值参数之后）。

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
    MS_ASSERT_EQ(n->kind, ND_FUNC_DECL, "func decl");
    MS_ASSERT_TRUE(n->func_decl.name == NULL, "anonymous");
    MS_ASSERT_TRUE(n->func_decl.params == NULL, "no params");
    msArenaFree(&a);
}

static void testFuncWithParams(void) {
    MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "func(a, b) { return a }");
    MS_ASSERT_EQ(n->kind, ND_FUNC_DECL, "func");
    int cnt = 0;
    for (MsNodeList* l = n->func_decl.params; l; l = l->next) cnt++;
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
double := func(x) { return x * 2 }
print(double(5))   // 10

// 高阶函数
apply := func(f, x) { return f(x) }
print(apply(double, 3))   // 6

// 闭包捕获
func makeAdder(n) {
    return func(x) { return x + n }
}
add5 := makeAdder(5)
print(add5(3))    // 8
print(add5(10))   // 15

// vararg
sum := func(*args) {
    s := 0
    for v in args { s += v }
    return s
}
print(sum(1, 2, 3, 4))  // 10
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`parseBlock` 依赖**：`parseFuncLit` 调用 `parseBlock`，后者在 T027（if/else）中实现。若严格按任务顺序，T024 需在 T027 后实现；但 `parseBlock` 实现极简（消耗 `{` 循环解析语句到 `}`），可在 T024 中预先实现 `parseBlock` 骨架。
- **参数默认值与闭包**：默认值在定义时求值（与 Python 相同，非运行时），这是语义问题，compiler（T043）负责处理。
- **命名函数字面量 vs 命名函数声明**：`func name() {}` 在**表达式**上下文产生 `ND_FUNC_DECL(name!=NULL)`（函数字面量有名字，便于递归）；在**语句**上下文产生同样的节点，但由 `msParseStmt` 识别并处理（T034）。
