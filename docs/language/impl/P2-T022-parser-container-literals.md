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
| P2-T017 | `ND_LIST`/`ND_MAP`/`ND_SET` 节点 |
| P2-T021 | `TOK_LBRACKET` 中缀（parseIndex）已注册 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.3.9 list 字面量（`[a, b, c]`） |
| `syntax.md` | §2.3.10 map 字面量（`{k: v}`） |
| `syntax.md` | §2.3.11 set 字面量（`{a, b}`）与消歧规则 |
| `type-system.md` | §2.8 set / §2.9 map |

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
// gParseRules[TOK_LBRACKET] = { parseListLit, parseIndex, PREC_CALL };
static MsNode* parseListLit(MsParser* p) {
    MsSrcPos pos = p->prev.pos;  // '['
    MsNodeList* elems = NULL;
    MsNodeList** tail = &elems;

    while (!check(p, TOK_RBRACKET) && !check(p, TOK_EOF)) {
        MsNode* elem;
        if (match(p, TOK_STAR)) {
            // *expr 展开（Python 风格 [*a, *b]）
            MsNode* inner = parsePrecedence(p, PREC_OR);
            elem = MS_ARENA_NEW(p->arena, MsNode);
            elem->kind = ND_STAR_EXPR;
            elem->unary.operand = inner;
        } else {
            elem = parsePrecedence(p, PREC_OR);
        }
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = elem; item->next = NULL;
        *tail = item; tail = &item->next;

        if (!match(p, TOK_COMMA)) break;
        if (check(p, TOK_RBRACKET)) break;  // 尾随逗号
    }
    expect(p, TOK_RBRACKET, "expected ']' after list");

    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind            = ND_LIST;
    n->pos             = pos;
    n->container.elems = elems;
    return n;
}
```

### 2. map/set 消歧

```c
// gParseRules[TOK_LBRACE] = { parseMapOrSetLit, NULL, PREC_NONE };
static MsNode* parseMapOrSetLit(MsParser* p) {
    MsSrcPos pos = p->prev.pos;  // '{'

    // 空 {} → 空 map
    if (match(p, TOK_RBRACE)) {
        MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
        n->kind      = ND_MAP;
        n->pos       = pos;
        n->map.pairs = NULL;
        return n;
    }

    // 解析第一个元素，向前探测 ':'
    MsNode* first = parsePrecedence(p, PREC_OR);

    if (match(p, TOK_COLON)) {
        // map 模式：{k: v, k2: v2, …}
        MsNodeList* pairs = NULL;
        MsNodeList** tail = &pairs;

        MsNode* val = parsePrecedence(p, PREC_OR);
        // 构造键值对节点（ND_BINARY(TOK_COLON, key, val)）
        MsNode* pair = MS_ARENA_NEW(p->arena, MsNode);
        pair->kind       = ND_BINARY;
        pair->binary.op  = TOK_COLON;
        pair->binary.left  = first;
        pair->binary.right = val;
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = pair; item->next = NULL;
        *tail = item; tail = &item->next;

        while (match(p, TOK_COMMA) && !check(p, TOK_RBRACE)) {
            if (match(p, TOK_STARSTAR)) {
                // **d 展开
                MsNode* inner = parsePrecedence(p, PREC_OR);
                MsNode* star2 = MS_ARENA_NEW(p->arena, MsNode);
                star2->kind = ND_DOUBLESTAR_EXPR;
                star2->unary.operand = inner;
                MsNodeList* it = MS_ARENA_NEW(p->arena, MsNodeList);
                it->node = star2; it->next = NULL;
                *tail = it; tail = &it->next;
            } else {
                MsNode* k = parsePrecedence(p, PREC_OR);
                expect(p, TOK_COLON, "expected ':' after map key");
                MsNode* v = parsePrecedence(p, PREC_OR);
                MsNode* pr = MS_ARENA_NEW(p->arena, MsNode);
                pr->kind = ND_BINARY; pr->binary.op = TOK_COLON;
                pr->binary.left = k; pr->binary.right = v;
                MsNodeList* it = MS_ARENA_NEW(p->arena, MsNodeList);
                it->node = pr; it->next = NULL;
                *tail = it; tail = &it->next;
            }
        }
        expect(p, TOK_RBRACE, "expected '}' after map");
        MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
        n->kind      = ND_MAP;
        n->pos       = pos;
        n->map.pairs = pairs;
        return n;

    } else {
        // set 模式：{a, b, c, …}
        MsNodeList* elems = NULL;
        MsNodeList** tail = &elems;
        MsNodeList* item0 = MS_ARENA_NEW(p->arena, MsNodeList);
        item0->node = first; item0->next = NULL;
        elems = item0; tail = &item0->next;

        while (match(p, TOK_COMMA) && !check(p, TOK_RBRACE)) {
            MsNode* elem = parsePrecedence(p, PREC_OR);
            MsNodeList* it = MS_ARENA_NEW(p->arena, MsNodeList);
            it->node = elem; it->next = NULL;
            *tail = it; tail = &it->next;
        }
        expect(p, TOK_RBRACE, "expected '}' after set");
        MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
        n->kind            = ND_SET;
        n->pos             = pos;
        n->container.elems = elems;
        return n;
    }
}
```

---

## 验收标准（checklist）

- [ ] `"[]"` → `ND_LIST(elems=NULL)`。
- [ ] `"[1, 2, 3]"` → `ND_LIST(elems=[1,2,3])`。
- [ ] `"[1,]"` 尾随逗号合法。
- [ ] `"[*a, 1]"` → `ND_LIST(elems=[ND_STAR_EXPR(a), ND_INT(1)])`。
- [ ] `"{}"` → `ND_MAP(pairs=NULL)`（空 map，非空 set）。
- [ ] `"{1: 2, 3: 4}"` → `ND_MAP`，2 个键值对。
- [ ] `"{1, 2, 3}"` → `ND_SET`，3 个元素。
- [ ] `"{1}"` → `ND_SET`（单元素 set，非 map）。
- [ ] `"{**d, 'k': v}"` → `ND_MAP`，含 `ND_DOUBLESTAR_EXPR`。
- [ ] map 与 set 消歧：第一个 `:` 决定是 map；无 `:` 是 set。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_container_literals.c`）

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

static void testEmptyList(void) {
    MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "[]");
    MS_ASSERT_EQ(n->kind, ND_LIST, "list");
    MS_ASSERT_TRUE(n->container.elems == NULL, "empty");
    msArenaFree(&a);
}

static void testEmptyMap(void) {
    MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "{}");
    MS_ASSERT_EQ(n->kind, ND_MAP, "empty map (not set)");
    msArenaFree(&a);
}

static void testSet(void) {
    MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "{1, 2, 3}");
    MS_ASSERT_EQ(n->kind, ND_SET, "set");
    msArenaFree(&a);
}

static void testMap(void) {
    MsArena a; msArenaInit(&a);
    MsNode* n = px(&a, "{\"a\": 1, \"b\": 2}");
    MS_ASSERT_EQ(n->kind, ND_MAP, "map");
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

// 展开
a := [1, 2]
b := [*a, 3, 4]
print(b)    // [1, 2, 3, 4]

d2 := {**d, "c": 3}
print(d2)   // {"a": 1, "b": 2, "c": 3}
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`{}` 歧义**：`{}` 约定为空 map，与某些语言（如 Python）一致。空 set 只能通过 `set()` 内置函数创建（`type-system.md §2.8`）。
- **`{expr}` 单元素**：`{expr}` 为单元素 set（无 `:`），`{expr: val}` 为单键 map。
- **`TOK_LBRACE` 与语句块**：`{` 在表达式上下文（Pratt 前缀）产生 map/set；在语句上下文（`if`/`func` 后）产生语句块。两者不冲突，因为语句解析器（`msParseStmt`）在知道上下文后直接调用 `parseBlock`，不走 Pratt 前缀。
- **map 键限制**：map 键必须可哈希（编译器不检查，运行时检查，VM 层 T060 实现）。
