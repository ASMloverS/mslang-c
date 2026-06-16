# P2-T022 list / set / map 字面量消歧

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `[…]` list 字面量、`{…}` map/set 字面量的前缀解析，以及两者的消歧逻辑：
- `{}` → 空 map（`ND_MAP`）
- `{k: v, …}` → map（键值对，`:` 分隔）
- `{a, b, …}` → set（无 `:` 分隔符）
- `[a, b, …]` → list

这是 Pratt 框架中 `TOK_LBRACKET` 前缀与 `TOK_LBRACE` 前缀的实现。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T017 | `MS_ND_LIST`/`MS_ND_MAP`/`MS_ND_SET` 节点 |
| P2-T021 | `MS_TOK_LBRACKET` 中缀（parseIndex）已注册 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3 表达式文法（PrimaryExpr / ListLiteral / SetLiteral / MapLiteral 产生式及消歧注释） |
| `type-system.md` | §2.8 map |
| `type-system.md` | §2.10 set |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parse_expr.c   # 新增 parseListLit / parseMapOrSetLit
```

---

## 实现要点

### 1. list 字面量

```c
// 在 msParseExprRegisterRules 中更新 MS_TOK_LBRACKET 的前缀规则：
// parserRegisterRule(MS_TOK_LBRACKET, parseListLit, parseIndex, PREC_CALL);
static MsNode* parseListLit(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;  // '['
  MsNodeList* elems = NULL;
  MsNodeList** tail = &elems;

  while (!msParserCheck(p, MS_TOK_RBRACKET) && !msParserCheck(p, MS_TOK_EOF)) {
    MsNode* elem = parsePrecedence(p, PREC_OR);
    MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
    item->node = elem; item->next = NULL;
    *tail = item; tail = &item->next;

    if (!msParserMatch(p, MS_TOK_COMMA)) { break; }
    if (msParserCheck(p, MS_TOK_RBRACKET)) { break; }  // 尾随逗号
  }
  msParserExpect(p, MS_TOK_RBRACKET, "expected ']' after list");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind            = MS_ND_LIST;
  n->pos             = pos;
  n->container.elems = elems;
  return n;
}
```

### 2. map/set 消歧

```c
// 在 msParseExprRegisterRules 中新增 MS_TOK_LBRACE 前缀规则：
// parserRegisterRule(MS_TOK_LBRACE, parseMapOrSetLit, NULL, PREC_NONE);
static MsNode* parseMapOrSetLit(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;  // '{'

  // 空 {} → 空 map
  if (msParserMatch(p, MS_TOK_RBRACE)) {
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind      = MS_ND_MAP;
    n->pos       = pos;
    n->map.pairs = NULL;
    return n;
  }

  // 解析第一个元素，向前探测 ':'
  MsNode* first = parsePrecedence(p, PREC_OR);

  if (msParserMatch(p, MS_TOK_COLON)) {
    // map 模式：{k: v, k2: v2, …}
    MsNodeList* pairs = NULL;
    MsNodeList** tail = &pairs;

    MsNode* val = parsePrecedence(p, PREC_OR);
    // 构造键值对节点（MS_ND_BINARY(MS_TOK_COLON, key, val)）
    MsNode* pair = MS_ARENA_NEW(p->arena, MsNode);
    pair->kind         = MS_ND_BINARY;
    pair->binary.op    = MS_TOK_COLON;
    pair->binary.left  = first;
    pair->binary.right = val;
    MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
    item->node = pair; item->next = NULL;
    *tail = item; tail = &item->next;

    while (msParserMatch(p, MS_TOK_COMMA) && !msParserCheck(p, MS_TOK_RBRACE)) {
      MsNode* k = parsePrecedence(p, PREC_OR);
      msParserExpect(p, MS_TOK_COLON, "expected ':' after map key");
      MsNode* v = parsePrecedence(p, PREC_OR);
      MsNode* pr = MS_ARENA_NEW(p->arena, MsNode);
      pr->kind           = MS_ND_BINARY;
      pr->binary.op      = MS_TOK_COLON;
      pr->binary.left    = k;
      pr->binary.right   = v;
      MsNodeList* it = MS_ARENA_NEW(p->arena, MsNodeList);
      it->node = pr; it->next = NULL;
      *tail = it; tail = &it->next;
    }
    msParserExpect(p, MS_TOK_RBRACE, "expected '}' after map");
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind      = MS_ND_MAP;
    n->pos       = pos;
    n->map.pairs = pairs;
    return n;
  }

  // set 模式：{a, b, c, …}
  MsNodeList* elems = NULL;
  MsNodeList** tail = &elems;
  MsNodeList* item0 = MS_ARENA_NEW(p->arena, MsNodeList);
  item0->node = first; item0->next = NULL;
  elems = item0; tail = &item0->next;

  while (msParserMatch(p, MS_TOK_COMMA) && !msParserCheck(p, MS_TOK_RBRACE)) {
    MsNode* elem = parsePrecedence(p, PREC_OR);
    MsNodeList* it = MS_ARENA_NEW(p->arena, MsNodeList);
    it->node = elem; it->next = NULL;
    *tail = it; tail = &it->next;
  }
  msParserExpect(p, MS_TOK_RBRACE, "expected '}' after set");
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind            = MS_ND_SET;
  n->pos             = pos;
  n->container.elems = elems;
  return n;
}
```

---

## 验收标准（checklist）

- [ ] `"[]"` → `MS_ND_LIST(elems=NULL)`。
- [ ] `"[1, 2, 3]"` → `MS_ND_LIST(elems=[1,2,3])`。
- [ ] `"[1,]"` 尾随逗号合法。
- [ ] `"{}"` → `MS_ND_MAP(pairs=NULL)`（空 map，非空 set）。
- [ ] `"{1: 2, 3: 4}"` → `MS_ND_MAP`，2 个键值对。
- [ ] `"{1, 2, 3}"` → `MS_ND_SET`，3 个元素。
- [ ] `"{1}"` → `MS_ND_SET`（单元素 set，非 map）。
- [ ] map 与 set 消歧：第一个 `:` 决定是 map；无 `:` 是 set。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_container_literals.c`）

```c
#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "ms_arena.h"

static MsNode* px(struct MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t)strlen(s), "<t>", a);
  return msParseExpr(&p);
}

static void testEmptyList(void) {
  struct MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "[]");
  MS_ASSERT_EQ(n->kind, MS_ND_LIST, "list");
  MS_ASSERT_TRUE(n->container.elems == NULL, "empty");
  msArenaFree(&a);
}

static void testEmptyMap(void) {
  struct MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "{}");
  MS_ASSERT_EQ(n->kind, MS_ND_MAP, "empty map (not set)");
  msArenaFree(&a);
}

static void testSet(void) {
  struct MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "{1, 2, 3}");
  MS_ASSERT_EQ(n->kind, MS_ND_SET, "set");
  msArenaFree(&a);
}

static void testMap(void) {
  struct MsArena a; msArenaInit(&a);
  MsNode* n = px(&a, "{\"a\": 1, \"b\": 2}");
  MS_ASSERT_EQ(n->kind, MS_ND_MAP, "map");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testEmptyList);
  MS_RUN(testEmptyMap);
  MS_RUN(testSet);
  MS_RUN(testMap);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// list
nums := [1, 2, 3]
print(nums)     // [1, 2, 3]

// map
d := {"a": 1, "b": 2}
print(d["a"])   // 1

// set
s := {1, 2, 3, 2, 1}
print(len(s))   // 3（去重）

// 空值消歧
empty := {}          // 空 map
print(type(empty))   // map
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`{}` 歧义**：`{}` 约定为空 map，与某些语言（如 Python）一致。空 set 只能通过 `set()` 内置函数创建（`type-system.md §2.10`）。
- **`{expr}` 单元素**：`{expr}` 为单元素 set（无 `:`），`{expr: val}` 为单键 map。
- **`TOK_LBRACE` 与语句块**：`{` 在表达式上下文（Pratt 前缀）产生 map/set；在语句上下文（`if`/`func` 后）产生语句块。两者不冲突，因为语句解析器（`msParseStmt`）在知道上下文后直接调用 `parseBlock`，不走 Pratt 前缀。
- **map 键限制**：map 键必须可哈希（编译器不检查，运行时检查，VM 层 T060 实现）。
