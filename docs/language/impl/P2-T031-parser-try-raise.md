# P2-T031 try / catch / finally / raise

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现异常处理语句的解析：`try { } catch (name [: Type {, Type}]) { } finally { }` 与 `raise [expr]`，产生 `MS_ND_TRY`/`MS_ND_CATCH_CLAUSE`/`MS_ND_RAISE` 节点。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T027 | `msParseBlock` |
| P2-T017 | `MS_ND_TRY`/`MS_ND_CATCH_CLAUSE`/`MS_ND_RAISE` 节点（`ms_ast.h`） |

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

`msParseStmt` 中 `if (msParserMatch(p, MS_TOK_TRY))` 分支调用 `parseTryStmt`：

```c
static MsNode* parseTryStmt(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;
  msParserExpect(p, MS_TOK_LBRACE, "expected '{' after 'try'");
  MsNode* body = msParseBlock(p);

  MsNodeList* handlers = NULL;
  MsNodeList** hTail   = &handlers;
  MsNode* finallyBlock = NULL;

  while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {}

  // catch 子句（0+个）
  // MS_ND_CATCH_CLAUSE 为 auxiliary 节点（枚举值在 MS_ND_COUNT 之后），
  // 仅作为 tryStmt.handlers 列表项，不出现在顶层语句派发中。
  while (msParserMatch(p, MS_TOK_CATCH)) {
    MsNode* clause = MS_ARENA_NEW(p->arena, MsNode);
    clause->kind = MS_ND_CATCH_CLAUSE;
    clause->pos  = p->prev.pos;

    const char* asName   = NULL;
    MsNodeList* typeFilters = NULL;
    MsNodeList** etTail  = &typeFilters;

    // catch ( identifier [ : TypeExpr {, TypeExpr} ] )
    msParserExpect(p, MS_TOK_LPAREN, "expected '(' after 'catch'");
    msParserExpect(p, MS_TOK_IDENT, "expected binding name");
    asName = p->prev.start;

    if (msParserMatch(p, MS_TOK_COLON)) {
      // 类型位使用 msParseExpr 以支持 errors.md §1 的 module.ExcClass 限定名
      // （如 csv.Error），有意放宽 syntax.md §2.2 的 identifier 文法；
      // 非 MS_ND_IDENT/MS_ND_ATTR 形态的合法性校验延后至编译器 T046。
      do {
        MsNode* t = msParseExpr(p);
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = t; item->next = NULL;
        *etTail = item; etTail = &item->next;
      } while (msParserMatch(p, MS_TOK_COMMA));
    }
    msParserExpect(p, MS_TOK_RPAREN, "expected ')' to close catch clause");
    msParserExpect(p, MS_TOK_LBRACE, "expected '{' after catch clause");
    MsNode* clauseBody = msParseBlock(p);

    clause->catchClause.typeFilter = typeFilters;
    clause->catchClause.asName     = asName;
    clause->catchClause.body       = clauseBody;

    MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
    item->node = clause; item->next = NULL;
    *hTail = item; hTail = &item->next;

    while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {}
  }

  // finally（可选）
  if (msParserMatch(p, MS_TOK_FINALLY)) {
    msParserExpect(p, MS_TOK_LBRACE, "expected '{' after 'finally'");
    finallyBlock = msParseBlock(p);
  }

  if (handlers == NULL && finallyBlock == NULL) {
    msParserError(p, "try requires at least one 'catch' or 'finally'");
  }

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind              = MS_ND_TRY;
  n->pos               = pos;
  n->tryStmt.body      = body;
  n->tryStmt.handlers  = handlers;
  n->tryStmt.finallyBlock = finallyBlock;
  return n;
}
```

**`MS_ND_CATCH_CLAUSE` 节点字段**（`ms_ast.h` 已有定义，需将 `typeFilter` 升级为 `MsNodeList*` 以支持多类型捕获）：

现有 `ms_ast.h` 定义：

```c
// MS_ND_CATCH_CLAUSE
struct {
  MsNode*     typeFilter;  // 当前为单类型；需升级为 MsNodeList* 以支持多类型
  const char* asName;      // 绑定名（source slice，非 NUL 结尾）
  MsNode*     body;        // MS_ND_BLOCK
} catchClause;
```

本任务实现时须将 `typeFilter` 类型改为 `MsNodeList*`（指向异常类型节点链表，NULL 表示捕获所有异常），以匹配 `syntax.md §2.2` 的 `CatchClause = 'catch' '(' identifier [ ':' identifier { ',' identifier } ] ')' Block` 多类型文法。`asName` 存储方式（source slice vs arena 复制 NUL 结尾串）需与项目其他绑定名保持一致。

### 3. `raise` 语句

`msParseStmt` 中 `if (msParserMatch(p, MS_TOK_RAISE))` 分支调用 `parseRaiseStmt`：

```c
static MsNode* parseRaiseStmt(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;
  MsNode* exc = NULL;

  if (!msParserCheck(p, MS_TOK_NEWLINE) && !msParserCheck(p, MS_TOK_SEMICOLON) && !msParserCheck(p, MS_TOK_EOF)) {
    exc = msParseExpr(p);
  }
  // raise 无参数 → 重抛当前异常（在 catch 块内合法）

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind            = MS_ND_RAISE;
  n->pos             = pos;
  n->singleExpr.expr = exc;
  return n;
}
```

---

## 验收标准（checklist）

- [x] `"try { } catch (e) { }"` → `MS_ND_TRY(body, handlers=[CATCH(typeFilter=NULL,asName="e")], finallyBlock=NULL)`。 <!-- v:ctest:test_try_raise -->
- [x] `"try { } catch (e: ValueError) { }"` → `CATCH(typeFilter=[MS_ND_IDENT("ValueError")], asName="e")`。 <!-- v:ctest:test_try_raise -->
- [x] `"try { } catch (e: ValueError, TypeError) { }"` → `CATCH(typeFilter=[ValueError,TypeError], asName="e")`。 <!-- v:ctest:test_try_raise -->
- [x] `"try { } finally { }"` → handlers=NULL, finallyBlock=block。 <!-- v:ctest:test_try_raise -->
- [x] `"try { } catch (e) { } catch (e: E2) { } finally { }"` → 2 handlers + finallyBlock。 <!-- v:ctest:test_try_raise -->
- [x] `"try { }"` 无 catch/finally → 语法错误。 <!-- v:ctest:test_try_raise -->
- [x] `"raise"` → `MS_ND_RAISE(expr=NULL)`（reraise）。 <!-- v:ctest:test_try_raise -->
- [x] `"raise ValueError(\"msg\")"` → `MS_ND_RAISE(expr=MS_ND_CALL(ValueError, ["msg"]))`。 <!-- v:ctest:test_try_raise -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_try_raise.c`）

```c
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_ast.h"
#include "mslang/ms_parser.h"
#include "parser/ms_arena.h"

static MsNode* pStmt(struct MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t)strlen(s), "<t>", a);
  return msParseStmt(&p);
}

static void testTryCatch(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "try { } catch (e: ValueError) { }");
  MS_ASSERT_EQ(n->kind, MS_ND_TRY, "try");
  MS_ASSERT_TRUE(n->tryStmt.handlers != NULL, "has catch");
  msArenaFree(&a);
}

static void testRaise(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "raise");
  MS_ASSERT_EQ(n->kind, MS_ND_RAISE, "raise");
  MS_ASSERT_TRUE(n->singleExpr.expr == NULL, "reraise");
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
    print("caught:", e)   // caught: division by zero
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

- **`MS_TOK_CATCH`/`MS_TOK_FINALLY`/`MS_TOK_RAISE`**：已由 lexer 提供（`ms_lexer.h`），均为 `syntax.md §1.4` 保留字。
- **无类型 catch 的语义**：`catch (e) { }` 无类型过滤，捕获所有异常；绑定名不可省略（`e` 始终必须给出）。
- **reraise 上下文**：`raise` 无参数只在 `catch` 块内合法；语义检查在编译器（T046）阶段。
