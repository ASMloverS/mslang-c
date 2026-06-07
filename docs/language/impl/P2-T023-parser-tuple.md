# P2-T023 tuple 字面量 / 分组括号消歧

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `(…)` 前缀的解析，区分两种情况：
1. **分组括号**：`(expr)` → 返回 `expr` 本身（无 `ND_TUPLE` 包装）
2. **tuple 字面量**：`(a, b, c)` → `ND_TUPLE`；`(a,)` 单元素 tuple（有尾随逗号）

同时实现**裸 tuple**（无括号逗号分隔，仅在赋值/return 右侧等特定上下文中使用）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T017 | `ND_TUPLE` 节点 |
| P2-T021 | `TOK_LPAREN` 中缀（parseCall）已注册 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3.12 tuple 字面量（括号与逗号规则） |
| `type-system.md` | §2.10 tuple（不可变、固定长度） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 实现 parseGroupOrTuple（前缀）
src/parser/ms_parser.c       # 实现 parseTupleExpr（无括号裸 tuple，供 stmt 调用）
```

---

## 实现要点

### 1. `()` 分组与 tuple 消歧

```c
// gParseRules[TOK_LPAREN] = { parseGroupOrTuple, parseCall, PREC_CALL };
static MsNode* parseGroupOrTuple(MsParser* p) {
  MsSrcPos pos = p->prev.pos;  // '('

  // 空 tuple：()
  if (match(p, TOK_RPAREN)) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind            = ND_TUPLE;
    n->pos             = pos;
    n->container.elems = NULL;
    return n;
  }

  MsNode* first = parsePrecedence(p, PREC_OR);

  // 是否有逗号 → tuple
  if (match(p, TOK_COMMA)) {
    // tuple 模式
    MsNodeList* elems = NULL;
    MsNodeList** tail = &elems;
    MsNodeList* item0 = MS_ARENA_NEW(p->arena, MsNodeList);
    item0->node = first; item0->next = NULL;
    elems = item0; tail = &item0->next;

    while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
      MsNode* elem = parsePrecedence(p, PREC_OR);
      MsNodeList* it = MS_ARENA_NEW(p->arena, MsNodeList);
      it->node = elem; it->next = NULL;
      *tail = it; tail = &it->next;
      if (!match(p, TOK_COMMA)) break;
    }
    expect(p, TOK_RPAREN, "expected ')' after tuple");

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind            = ND_TUPLE;
    n->pos             = pos;
    n->container.elems = elems;
    return n;
  }

  // 分组括号：(expr) 无逗号 → 直接返回 first
  expect(p, TOK_RPAREN, "expected ')'");
  // 不包装：first 上附加 pos 信息（可选）
  return first;
}
```

### 2. 裸 tuple（`a, b, c` 无括号）

裸 tuple 仅在明确上下文中出现（`return a, b`、`var x, y = …` 赋值解包等）。由 `msParseStmt` 或赋值解析检测第一个表达式后接 `,` 决定是否构造 `ND_TUPLE`。

```c
// 在 parsePrecedence 之后、语句层调用
MsNode* parseMaybeTuple(MsParser* p, MsNode* first) {
  if (!check(p, TOK_COMMA)) return first;
  // 有逗号 → 裸 tuple
  MsNodeList* elems = NULL;
  MsNodeList** tail = &elems;
  MsNodeList* item0 = MS_ARENA_NEW(p->arena, MsNodeList);
  item0->node = first; item0->next = NULL;
  elems = item0; tail = &item0->next;

  while (match(p, TOK_COMMA) && !check(p, TOK_NEWLINE)
           && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF)) {
    MsNode* elem = parsePrecedence(p, PREC_OR);
    MsNodeList* it = MS_ARENA_NEW(p->arena, MsNodeList);
    it->node = elem; it->next = NULL;
    *tail = it; tail = &it->next;
  }
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind            = ND_TUPLE;
  n->pos             = first->pos;
  n->container.elems = elems;
  return n;
}
```

### 3. `(a,)` 单元素 tuple

通过尾随逗号识别：

```c
// 在 parseGroupOrTuple 中，match(TOK_COMMA) 成功后立即 check TOK_RPAREN：
if (match(p, TOK_COMMA)) {
  if (check(p, TOK_RPAREN)) {
    // 单元素 tuple (a,)
    advance(p);
    // elems = [first]
    ...
    return tupleNode;
  }
  // 多元素 tuple …
}
```

---

## 验收标准（checklist）

- [ ] `"()"` → `ND_TUPLE(elems=NULL)`（空 tuple）。
- [ ] `"(1,)"` → `ND_TUPLE(elems=[1])`（单元素 tuple）。
- [ ] `"(1, 2, 3)"` → `ND_TUPLE(elems=[1,2,3])`。
- [ ] `"(1 + 2)"` → `ND_BINARY(+, 1, 2)`（分组，无 tuple 包装）。
- [ ] `"(x)"` → `ND_IDENT("x")`（分组）。
- [ ] `"(1, 2,)"` 尾随逗号合法（`elems=[1,2]`）。
- [ ] `"f((1, 2))"` → `ND_CALL(args=[ND_TUPLE([1,2])])`（调用参数中的 tuple）。
- [ ] 裸 tuple `"return 1, 2"` → `ND_RETURN(ND_TUPLE([1,2]))`（由 T030 的 return 解析调用 `parseMaybeTuple`）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_tuple.c`）

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

static void testEmptyTuple(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "()");
  MS_ASSERT_EQ(n->kind, ND_TUPLE, "empty tuple");
  MS_ASSERT_TRUE(n->container.elems == NULL, "no elements");
  msArenaFree(&a);
}

static void testSingleElemTuple(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "(42,)");
  MS_ASSERT_EQ(n->kind, ND_TUPLE, "single-elem tuple");
  MS_ASSERT_TRUE(n->container.elems != NULL, "has one element");
  MS_ASSERT_TRUE(n->container.elems->next == NULL, "only one");
  msArenaFree(&a);
}

static void testGrouping(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "(1 + 2)");
  MS_ASSERT_EQ(n->kind, ND_BINARY, "grouping: returns binary, not tuple");
  msArenaFree(&a);
}

static void testTuple3(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "(1, 2, 3)");
  MS_ASSERT_EQ(n->kind, ND_TUPLE, "tuple 3");
  // count elems
  int cnt = 0;
  for (MsNodeList* l = n->container.elems; l; l = l->next) cnt++;
  MS_ASSERT_EQ(cnt, 3, "3 elements");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testEmptyTuple);
  MS_RUN(testSingleElemTuple);
  MS_RUN(testGrouping);
  MS_RUN(testTuple3);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// 空 tuple
t0 := ()
print(type(t0))  // tuple
print(len(t0))   // 0

// 单元素 tuple（注意尾随逗号）
t1 := (42,)
print(t1)        // (42,)

// 多元素 tuple
t2 := (1, 2, 3)
print(t2[0])     // 1
print(len(t2))   // 3

// 分组括号
x := (1 + 2) * 3
print(x)         // 9

// 裸 tuple（return 多值）
func minmax(lst) {
    return min(lst), max(lst)
}
lo, hi := minmax([3, 1, 4, 1, 5])
print(lo, hi)    // 1 5

// tuple 解包赋值
a, b, c := (10, 20, 30)
print(a, b, c)   // 10 20 30
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`(expr)` 无逗号**：不产生 tuple，直接返回内层表达式节点（分组）。这意味着 `(x)` 的类型与 `x` 完全相同，不引入额外包装。
- **`(expr,)` 与 trailing comma**：尾随逗号是区分单元素 tuple 与分组的唯一方式，必须支持。
- **裸 tuple 边界**：裸 tuple 在 `for x, y in …`、`a, b = …` 等上下文中使用；由具体语句解析器（T026/T028）负责调用 `parseMaybeTuple`，而非 Pratt 前缀。
- **tuple 不可变**：tuple 在 VM 层（T061）为不可变容器；parser 层无需区分可变性。
