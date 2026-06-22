# P2-T028 for 语句四种形式（range 无需特殊消歧）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `for` 语句的四种形式：
1. **无限循环**：`for { body }`
2. **条件循环**：`for cond { body }`（类 while）
3. **三段式**：`for init; cond; post { body }`（§2.2 ForHeader 三段式分支）
4. **for-in 迭代**：`for x in iterable { body }`（或 `for k, v in map { body }`）

`range` 不是关键字（§1.4），`for i in range(n) { }` 中的 `range(n)` 是普通函数调用，parser 无需特殊处理。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T027 | `msParseBlock` |
| P2-T017 | `MS_ND_FOR` 节点 |
| P2-T026 | 赋值/短声明（for-in 左侧目标） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.2 ForStmt / ForHeader（三种形式定义） |
| `syntax.md` | §3.2 for 的三种形式（语义示例） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c   # parseForStmt（在 msParseStmt 分支中）
```

---

## 实现要点

### 消歧逻辑

`for` 语句通过向前探测消歧：

```
for {              → 无限循环（msParserCheck MS_TOK_LBRACE）
for cond {         → 条件循环（cond 是表达式，后跟 MS_TOK_LBRACE）
for init; c; p {   → 三段式（first 后跟 ';'）
for x in e {       → for-in 单变量（解析 target，msParserMatch MS_TOK_IN，解析 iter）
for k, v in e {    → for-in 双变量解包（target 为 MS_ND_TUPLE）
```

关键难点：`for x in e { }` 中，`x` 是 `MS_ND_IDENT`，但 `for x { }` 是条件循环（`x` 是条件表达式）。消歧方法：解析完"第一个表达式"后，检查是否跟 `in` 或 `;`。

```c
static MsNode* parseForStmt(MsParser* p) {
  MsSrcPos pos = p->prev.pos;

  // 1. 无限循环
  if (msParserCheck(p, MS_TOK_LBRACE)) {
    msParserAdvance(p);
    MsNode* body = msParseBlock(p);
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_FOR;
    n->pos = pos;
    n->forStmt.init = NULL;
    n->forStmt.cond = NULL;
    n->forStmt.post = NULL;
    n->forStmt.body = body;
    n->forStmt.forTarget = NULL;
    n->forStmt.forIter = NULL;
    return n;
  }

  // 2. 解析第一个表达式/目标（若为 ident 且后跟 ':=' 则走短声明路径）
  MsNode* first = msParseExpr(p);
  first = parseMaybeTuple(p, first);  // 支持 for a, b in …

  // 3. for-in 消歧
  if (msParserMatch(p, MS_TOK_IN)) {
    MsNode* iter = msParseExpr(p);
    msParserExpect(p, MS_TOK_LBRACE, "expected '{' after for-in iterable");
    MsNode* body = msParseBlock(p);

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_FOR;
    n->pos = pos;
    n->forStmt.init = NULL;
    n->forStmt.cond = NULL;
    n->forStmt.post = NULL;
    n->forStmt.body = body;
    n->forStmt.forTarget = first;
    n->forStmt.forIter = iter;
    return n;
  }

  // 4. 三段式：for init; cond; post { }
  // init 可为 MS_ND_SHORT_DECL（`:=`）或普通表达式/赋值语句
  // post 可为 MS_ND_INC_DEC（`i++`）或赋值语句
  // ForHeader 内部的 ';' 为真实分号，不受 ASI 影响
  if (msParserMatch(p, MS_TOK_SEMICOLON)) {
    MsNode* init = first;
    MsNode* cond = NULL;
    if (!msParserCheck(p, MS_TOK_SEMICOLON)) {
      cond = msParseExpr(p);
    }
    msParserExpect(p, MS_TOK_SEMICOLON, "expected ';' after for condition");
    MsNode* post = NULL;
    if (!msParserCheck(p, MS_TOK_LBRACE)) {
      post = msParseExpr(p);
    }
    msParserExpect(p, MS_TOK_LBRACE, "expected '{' after for post");
    MsNode* body = msParseBlock(p);

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind = MS_ND_FOR;
    n->pos = pos;
    n->forStmt.init = init;
    n->forStmt.cond = cond;
    n->forStmt.post = post;
    n->forStmt.body = body;
    n->forStmt.forTarget = NULL;
    n->forStmt.forIter = NULL;
    return n;
  }

  // 5. 条件循环（first 是条件）
  msParserExpect(p, MS_TOK_LBRACE, "expected '{' after for condition");
  MsNode* body = msParseBlock(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_FOR;
  n->pos = pos;
  n->forStmt.init = NULL;
  n->forStmt.cond = first;
  n->forStmt.post = NULL;
  n->forStmt.body = body;
  n->forStmt.forTarget = NULL;
  n->forStmt.forIter = NULL;
  return n;
}
```

### for-in 解包

`for k, v in map { }` 中，`k, v` 解析为裸 tuple（`MS_ND_TUPLE`），元素列表在 `forTarget->container.elems`，由 VM 在迭代时展开（T065）。

### `range` 在 for 中的使用

```ms
for i in range(10) { }
```

`range(10)` 是普通函数调用（`MS_ND_CALL(MS_ND_IDENT("range"), [10])`），无特殊语法。`range` 不是关键字，不需要消歧。

---

## 验收标准（checklist）

- [ ] `"for i := 0; i < 10; i++ { }"` → `MS_ND_FOR(init=MS_ND_SHORT_DECL(i,0), cond=MS_ND_BINARY(<,i,10), post=MS_ND_INC_DEC(i, isInc=true))`（三段式）。
- [ ] `"for { }"` → `MS_ND_FOR(cond=NULL, forTarget=NULL, forIter=NULL)`（无限循环）。
- [ ] `"for x < 10 { }"` → `MS_ND_FOR(cond=MS_ND_BINARY(<, x, 10))`（条件循环）。
- [ ] `"for i in range(10) { }"` → `MS_ND_FOR(forTarget=MS_ND_IDENT(i), forIter=MS_ND_CALL(range,[10]))`。
- [ ] `"for k, v in d { }"` → `forTarget=MS_ND_TUPLE([k,v])`。
- [ ] `"for x in [1,2,3] { }"` → `forIter=MS_ND_LIST([1,2,3])`。
- [ ] `"for { break }"` → body 含 `MS_ND_BREAK`（T030 后验证，非本任务门禁）。
- [ ] for 内允许 `continue`（T030 后验证，非本任务门禁）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_for_stmt.c`）

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

static void testForInfinite(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "for { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_TRUE(n->forStmt.cond == NULL, "no cond");
  MS_ASSERT_TRUE(n->forStmt.forTarget == NULL, "no target");
  msArenaFree(&a);
}

static void testForIn(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "for i in lst { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_TRUE(n->forStmt.forTarget != NULL, "has target");
  MS_ASSERT_TRUE(n->forStmt.forIter != NULL, "has iter");
  MS_ASSERT_EQ(n->forStmt.forTarget->kind, MS_ND_IDENT, "target=ident");
  msArenaFree(&a);
}

static void testForCond(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "for x > 0 { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_TRUE(n->forStmt.cond != NULL, "has cond");
  MS_ASSERT_EQ(n->forStmt.cond->kind, MS_ND_BINARY, "cond is binary");
  msArenaFree(&a);
}

static void testForThreePart(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "for i := 0; i < 10; i++ { }");
  MS_ASSERT_EQ(n->kind, MS_ND_FOR, "for");
  MS_ASSERT_TRUE(n->forStmt.init != NULL, "has init");
  MS_ASSERT_TRUE(n->forStmt.cond != NULL, "has cond");
  MS_ASSERT_TRUE(n->forStmt.post != NULL, "has post");
  MS_ASSERT_TRUE(n->forStmt.forTarget == NULL, "no target");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testForInfinite);
  MS_RUN(testForIn);
  MS_RUN(testForCond);
  MS_RUN(testForThreePart);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// 无限循环（配合 break）
x := 0
for {
    x++
    if x >= 5 { break }
}
print(x)   // 5


// 条件循环（while）
y := 10
for y > 0 {
    y -= 3
}
print(y)   // -2（10-3-3-3-3 = -2）


// for-in：range
for i in range(3) {
    print(i)   // 0, 1, 2
}


// for-in：列表
for v in [10, 20, 30] {
    print(v)   // 10 20 30
}


// for-in：map（解包 k, v）
d := {"a": 1, "b": 2}
for k, v in d {
    print(k, v)   // a 1 / b 2（顺序不定）
}


// 嵌套 continue
for i in range(5) {
    if i == 2 { continue }
    print(i)   // 0 1 3 4
}
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`for x { }` 歧义**：`x` 既可是条件（`for cond { }`），也可是 for-in 目标（`for x in iter { }`）。通过解析完 `x` 后检查是否跟 `in` 消歧；若后跟 `{` 则是条件循环。
- **三段式 init 解析**：init 位置若当前 token 为 identifier 且后跟 `:=`，走短声明路径产生 `MS_ND_SHORT_DECL`；否则为普通表达式/赋值语句。后续 `cond`/`post` 可见该变量（作用域限 for 块内）。post 位可产生 `MS_ND_INC_DEC`（如 `i++`）或赋值语句。ForHeader 内 `;` 为真实分号，不受 ASI 影响。
- **`for` 与 label**：mslang 无 Go 风格的带标签 break/continue（`break label`）；初版不实现，T030 的 `MS_ND_BREAK.label` 字段为 NULL。
