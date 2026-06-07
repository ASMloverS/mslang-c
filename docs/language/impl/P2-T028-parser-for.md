# P2-T028 for 语句三种形式 + range 消歧

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `for` 语句的三种形式：
1. **无限循环**：`for { body }`
2. **条件循环**：`for cond { body }`（类 while）
3. **for-in 迭代**：`for x in iterable { body }`（或 `for k, v in map { body }`）

同时处理 `for` 内的 `range` 表达式消歧（`range` 是伪关键字，在 for-in 右侧的 `range(n)` 是内置调用，不需要特殊处理）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T027 | `parseBlock` |
| P2-T017 | `ND_FOR` 节点 |
| P2-T026 | 赋值/短声明（for-in 左侧目标） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.4.7 for 语句（三形式） |
| `syntax.md` | §2.4.8 for-in 迭代与解包 |
| `syntax.md` | §2.4.9 break/continue（T030） |

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
for {          → 无限循环（match TOK_LBRACE）
for cond {     → 条件循环（cond 是表达式，后跟 TOK_LBRACE）
for x in e {  → for-in（解析 target，match TOK_IN，解析 iter）
```

关键难点：`for x in e { }` 中，`x` 是 `ND_IDENT`，但 `for x { }` 是条件循环（`x` 是条件表达式）。消歧方法：解析完"第一个表达式"后，检查是否跟 `in`。

```c
static MsNode* parseForStmt(MsParser* p) {
  MsSrcPos pos = p->prev.pos;

  // 1. 无限循环
  if (check(p, TOK_LBRACE)) {
    advance(p);
    MsNode* body = parseBlock(p);
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind            = ND_FOR;
    n->pos             = pos;
    n->for_stmt.init        = NULL;
    n->for_stmt.cond        = NULL;
    n->for_stmt.post        = NULL;
    n->for_stmt.body        = body;
    n->for_stmt.for_target  = NULL;
    n->for_stmt.for_iter    = NULL;
    return n;
  }

  // 2. 解析第一个表达式/目标
  MsNode* first = msParseExpr(p);
  first = parseMaybeTuple(p, first);  // 支持 for a, b in …

  // 3. for-in 消歧
  if (match(p, TOK_IN)) {
    MsNode* iter = msParseExpr(p);
    expect(p, TOK_LBRACE, "expected '{' after for-in iterable");
    MsNode* body = parseBlock(p);

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind                = ND_FOR;
    n->pos                 = pos;
    n->for_stmt.init       = NULL;
    n->for_stmt.cond       = NULL;
    n->for_stmt.post       = NULL;
    n->for_stmt.body       = body;
    n->for_stmt.for_target = first;
    n->for_stmt.for_iter   = iter;
    return n;
  }

  // 4. 条件循环（first 是条件）
  expect(p, TOK_LBRACE, "expected '{' after for condition");
  MsNode* body = parseBlock(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind              = ND_FOR;
  n->pos               = pos;
  n->for_stmt.init     = NULL;
  n->for_stmt.cond     = first;  // 条件
  n->for_stmt.post     = NULL;
  n->for_stmt.body     = body;
  n->for_stmt.for_target = NULL;
  n->for_stmt.for_iter   = NULL;
  return n;
}
```

### for-in 解包

`for k, v in map { }` 中，`k, v` 解析为裸 tuple（`ND_TUPLE([k, v])`），由 VM 在迭代时展开（T065）。

### `range` 在 for 中的使用

```ms
for i in range(10) { }
```

`range(10)` 是普通函数调用（`ND_CALL(ND_IDENT("range"), [10])`），无特殊语法。`range` 不是关键字，不需要消歧。

---

## 验收标准（checklist）

- [ ] `"for { }"` → `ND_FOR(cond=NULL, target=NULL, iter=NULL)`（无限循环）。
- [ ] `"for x < 10 { }"` → `ND_FOR(cond=ND_BINARY(<, x, 10))`（条件循环）。
- [ ] `"for i in range(10) { }"` → `ND_FOR(target=ND_IDENT(i), iter=ND_CALL(range,[10]))`。
- [ ] `"for k, v in d { }"` → `target=ND_TUPLE([k,v])`。
- [ ] `"for x in [1,2,3] { }"` → `iter=ND_LIST([1,2,3])`。
- [ ] `"for { break }"` → body 含 `ND_BREAK`（T030 实现）。
- [ ] for 内允许 `continue`（T030）。

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
  MS_ASSERT_EQ(n->kind, ND_FOR, "for");
  MS_ASSERT_TRUE(n->for_stmt.cond == NULL, "no cond");
  MS_ASSERT_TRUE(n->for_stmt.for_target == NULL, "no target");
  msArenaFree(&a);
}

static void testForIn(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "for i in lst { }");
  MS_ASSERT_EQ(n->kind, ND_FOR, "for");
  MS_ASSERT_TRUE(n->for_stmt.for_target != NULL, "has target");
  MS_ASSERT_TRUE(n->for_stmt.for_iter != NULL, "has iter");
  MS_ASSERT_EQ(n->for_stmt.for_target->kind, ND_IDENT, "target=ident");
  msArenaFree(&a);
}

static void testForCond(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "for x > 0 { }");
  MS_ASSERT_EQ(n->kind, ND_FOR, "for");
  MS_ASSERT_TRUE(n->for_stmt.cond != NULL, "has cond");
  MS_ASSERT_EQ(n->for_stmt.cond->kind, ND_BINARY, "cond is binary");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testForInfinite);
  MS_RUN(testForIn);
  MS_RUN(testForCond);
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
- **C 风格 for 不支持**：mslang 无 `for init; cond; post { }` 形式（Go 有，mslang 无）；开发者用 `var i = 0; for i < n { i++ }` 替代。
- **`for` 与 label**：mslang 无 Go 风格的带标签 break/continue（`break label`）；初版不实现，T030 的 `ND_BREAK.label` 字段为 NULL。
