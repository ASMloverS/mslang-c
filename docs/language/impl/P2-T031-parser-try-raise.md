# P2-T031 try / catch / finally / raise

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现异常处理语句的解析：`try { } catch (name [: Type {, Type}]) { } finally { }` 与 `raise [expr]`，产生 `ND_TRY`/`ND_CATCH_CLAUSE`/`ND_RAISE` 节点。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T027 | `parseBlock` |
| P2-T017 | `ND_TRY`/`ND_CATCH_CLAUSE`/`ND_RAISE` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.2 TryStmt / CatchClause / RaiseStmt |
| `errors.md` | §3 try/catch/finally 语义 |
| `errors.md` | §4 raise |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c   # parseTryStmt / parseRaiseStmt
```

---

## 实现要点

### 1. try/catch/finally 语法

```
try {
    stmts
} catch (e) {
    stmts
} catch (e: ExcType) {
    stmts
} catch (e: ExcType1, ExcType2) {
    stmts
} finally {
    stmts
}
```

- 可有 0 个或多个 `catch` 子句（至少有一个 `catch` 或 `finally`）。
- `catch (e)` 无类型过滤，捕获所有异常。
- `catch (e: Type1, Type2)` 支持多类型匹配（逗号分隔）。
- `finally` 可选，最多一个。
- 绑定名 `e` 作用域仅限 catch 块内。

### 2. 实现

```c
static MsNode* parseTryStmt(MsParser* p) {
  MsSrcPos pos = p->prev.pos;
  expect(p, TOK_LBRACE, "expected '{' after 'try'");
  MsNode* body = parseBlock(p);

  MsNodeList* handlers = NULL;
  MsNodeList** hTail   = &handlers;
  MsNode* finally_block = NULL;

  // 允许换行
  while (match(p, TOK_NEWLINE) || match(p, TOK_SEMICOLON)) {}

  // catch 子句（0+个）
  while (match(p, TOK_CATCH)) {
    MsNode* clause = MS_ARENA_NEW(p->arena, MsNode);
    clause->kind = ND_CATCH_CLAUSE;
    clause->pos  = p->prev.pos;

    const char* bindName = NULL;
    uint32_t    bindLen  = 0;
    MsNodeList* excTypes = NULL;
    MsNodeList** etTail  = &excTypes;

    // catch ( identifier [ : TypeName {, TypeName} ] )
    expect(p, TOK_LPAREN, "expected '(' after 'catch'");
    expect(p, TOK_IDENT, "expected binding name");
    bindName = p->prev.start;
    bindLen  = p->prev.len;

    if (match(p, TOK_COLON)) {
      // 一或多个类型名
      do {
        MsNode* t = msParseExpr(p);   // ND_IDENT 或 ND_ATTR
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = t; item->next = NULL;
        *etTail = item; etTail = &item->next;
      } while (match(p, TOK_COMMA));
    }
    expect(p, TOK_RPAREN, "expected ')' to close catch clause");
    expect(p, TOK_LBRACE, "expected '{' after catch clause");
    MsNode* clauseBody = parseBlock(p);

    clause->catch_clause.exc_types = excTypes;  // list of type nodes (NULL → catch all)
    clause->catch_clause.bind_name = bindName;
    clause->catch_clause.bind_len  = bindLen;
    clause->catch_clause.body      = clauseBody;

    MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
    item->node = clause; item->next = NULL;
    *hTail = item; hTail = &item->next;

    while (match(p, TOK_NEWLINE) || match(p, TOK_SEMICOLON)) {}
  }

  // finally（可选）
  if (match(p, TOK_FINALLY)) {
    expect(p, TOK_LBRACE, "expected '{' after 'finally'");
    finally_block = parseBlock(p);
  }

  if (handlers == NULL && finally_block == NULL) {
    parserError(p, "try requires at least one 'catch' or 'finally'");
  }

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind                   = ND_TRY;
  n->pos                    = pos;
  n->try_stmt.body          = body;
  n->try_stmt.handlers      = handlers;
  n->try_stmt.finally_block = finally_block;
  return n;
}
```

**`ND_CATCH_CLAUSE` 节点字段**（追加到 T017 中）：

```c
// ND_CATCH_CLAUSE
struct {
  MsNodeList* exc_types;  // 异常类型列表（NULL → 捕获所有；逗号分隔多类型）
  const char* bind_name;  // 绑定名（NULL → 不绑定）
  uint32_t    bind_len;
  MsNode*     body;       // ND_BLOCK
} catch_clause;
```

### 3. `raise` 语句

```c
// msParseStmt 中，match(TOK_RAISE) 分支：
static MsNode* parseRaiseStmt(MsParser* p) {
  MsSrcPos pos = p->prev.pos;
  MsNode* exc  = NULL;

  if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF)) {
    exc = msParseExpr(p);
  }
  // raise 无参数 → 重抛当前异常（在 catch 块内合法）

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind             = ND_RAISE;
  n->pos              = pos;
  n->single_expr.expr = exc;    // 异常对象（NULL → reraise）
  return n;
}
```

---

## 验收标准（checklist）

- [ ] `"try { } catch (e) { }"` → `ND_TRY(body, handlers=[CATCH(exc_types=NULL,bind="e")], finally=NULL)`。
- [ ] `"try { } catch (e: ValueError) { }"` → `CATCH(exc_types=[ND_IDENT("ValueError")], bind="e")`。
- [ ] `"try { } catch (e: ValueError, TypeError) { }"` → `CATCH(exc_types=[ValueError,TypeError], bind="e")`。
- [ ] `"try { } finally { }"` → handlers=NULL, finally=block。
- [ ] `"try { } catch (e) { } catch (e: E2) { } finally { }"` → 2 handlers + finally。
- [ ] `"try { }"` 无 catch/finally → 语法错误。
- [ ] `"raise"` → `ND_RAISE(exc=NULL)`（reraise）。
- [ ] `"raise ValueError(\"msg\")"` → `ND_RAISE(exc=ND_CALL(ValueError, ["msg"]))`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_try_raise.c`）

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

static void testTryCatch(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "try { } catch (e: ValueError) { }");
  MS_ASSERT_EQ(n->kind, ND_TRY, "try");
  MS_ASSERT_TRUE(n->try_stmt.handlers != NULL, "has catch");
  msArenaFree(&a);
}

static void testRaise(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "raise");
  MS_ASSERT_EQ(n->kind, ND_RAISE, "raise");
  MS_ASSERT_TRUE(n->single_expr.expr == NULL, "reraise");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testTryCatch);
  MS_RUN(testRaise);
  return msTestSummary();
}
```

### .ms 使用示例（T067 + T079–T084 后验证）

```ms
// try/catch/finally
try {
    x := 1 / 0
} catch (e: ZeroDivisionError) {
    print("caught:", e)   // caught: ZeroDivisionError
} finally {
    print("cleanup")      // cleanup
}

// 多 catch（单 catch 子句，多类型）
try {
    raise ValueError("bad input")
} catch (e: TypeError, ValueError) {
    print("type or value error:", e)  // type or value error: bad input
}

// 多 catch 子句
try {
    raise ValueError("bad input")
} catch (e: TypeError) {
    print("type error")
} catch (e: ValueError) {
    print("value error:", e)  // value error: bad input
}

// reraise
func handle(action) {
    try {
        action()
    } catch (e: Exception) {
        print("logging:", e)
        raise    // 重抛
    }
}
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`TOK_CATCH`/`TOK_FINALLY`**：均需在 T007 关键字表中添加（核对 `syntax.md §1.4`）。
- **无类型 catch 的语义**：`catch (e) { }` 无类型过滤，捕获所有异常；绑定名不可省略（`e` 始终必须给出）。
- **reraise 上下文**：`raise` 无参数只在 `catch` 块内合法；语义检查在编译器（T046）阶段。
