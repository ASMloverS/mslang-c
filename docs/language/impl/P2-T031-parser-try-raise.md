# P2-T031 try / catch / finally / raise

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现异常处理语句的解析：`try { } catch ExcType as name { } finally { }` 与 `raise [expr] [from expr]`，产生 `ND_TRY`/`ND_CATCH_CLAUSE`/`ND_RAISE` 节点。

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
| `syntax.md` | §2.4.16 try/catch/finally |
| `syntax.md` | §2.4.17 raise 语句 |
| `errors.md` | §1 异常类层次（BaseException 等） |

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
} catch [ExcType [as name]] {
    stmts
} catch [ExcType2 [as name]] {
    stmts
} finally {
    stmts
}
```

- 可有 0 个或多个 `catch` 子句（至少有一个 `catch` 或 `finally`）。
- `catch` 可以不指定类型（捕获所有异常），等价于 `catch BaseException as e`。
- `finally` 可选，最多一个，在所有 `catch` 之后。
- `catch ExcType as name`：`ExcType` 是类型表达式（`ND_IDENT` 或 `ND_ATTR`），`name` 是绑定名（可选）。

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

    MsNode*     excType = NULL;
    const char* bindName = NULL;
    uint32_t    bindLen  = 0;

    // 可选：ExcType
    if (!check(p, TOK_LBRACE)) {
      excType = msParseExpr(p);   // 类型表达式（ND_IDENT / ND_ATTR）
      // 可选：as name
      if (match(p, TOK_AS)) {
        expect(p, TOK_IDENT, "expected binding name after 'as'");
        bindName = p->prev.start;
        bindLen  = p->prev.len;
      }
    }
    expect(p, TOK_LBRACE, "expected '{' after catch clause");
    MsNode* clauseBody = parseBlock(p);

    // 存入 ND_CATCH_CLAUSE（需 T017 中定义专用字段）
    // 临时：复用 try_stmt 字段存储单个 clause 信息
    // clause->catch_clause.exc_type  = excType;
    // clause->catch_clause.bind_name = bindName;
    // clause->catch_clause.body      = clauseBody;

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
  MsNode*     exc_type;   // 异常类型（NULL → 捕获所有）
  const char* bind_name;  // as name（NULL → 不绑定）
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
  MsNode* cause = NULL;

  if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF)) {
    exc = msParseExpr(p);
    if (match(p, TOK_FROM)) {   // raise ExcType from cause
      cause = msParseExpr(p);
    }
  }
  // raise 无参数 → 重抛当前异常（在 catch 块内合法）

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind              = ND_RAISE;
  n->pos               = pos;
  n->single_expr.expr  = exc;    // 异常对象（NULL → reraise）
  n->single_expr.expr2 = cause;  // from cause（NULL → 无）
  return n;
}
```

---

## 验收标准（checklist）

- [ ] `"try { } catch { }"` → `ND_TRY(body, handlers=[CATCH(exc=NULL,bind=NULL)], finally=NULL)`。
- [ ] `"try { } catch ValueError { }"` → `CATCH(exc=ND_IDENT("ValueError"), bind=NULL)`。
- [ ] `"try { } catch ValueError as e { }"` → `CATCH(exc=ValueError, bind="e")`。
- [ ] `"try { } finally { }"` → handlers=NULL, finally=block。
- [ ] `"try { } catch E1 { } catch E2 { } finally { }"` → 2 handlers + finally。
- [ ] `"try { }"` 无 catch/finally → 语法错误。
- [ ] `"raise"` → `ND_RAISE(exc=NULL, cause=NULL)`（reraise）。
- [ ] `"raise ValueError(\"msg\")"` → `ND_RAISE(exc=ND_CALL(ValueError, ["msg"]))`。
- [ ] `"raise E from cause"` → `ND_RAISE(exc=E, cause=cause)`。

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
  MsNode* n = pStmt(&a, "try { } catch ValueError { }");
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
} catch ZeroDivisionError as e {
    print("caught:", e)   // caught: ZeroDivisionError
} finally {
    print("cleanup")      // cleanup
}

// 多 catch
try {
    raise ValueError("bad input")
} catch TypeError as e {
    print("type error")
} catch ValueError as e {
    print("value error:", e)  // value error: bad input
}

// raise from
try {
    raise RuntimeError("wrapped") from ValueError("root cause")
} catch RuntimeError as e {
    print(e)    // RuntimeError: wrapped
    print(e.__cause__)  // ValueError: root cause
}

// reraise
func handle(action) {
    try {
        action()
    } catch Exception as e {
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

- **`TOK_CATCH`/`TOK_FINALLY`/`TOK_FROM`**：均需在 T007 关键字表中添加（核对 `syntax.md §1.4`）。
- **空 catch 的语义**：`catch { }` 无类型过滤，等价于 `catch BaseException { }`；编译器处理类型匹配。
- **reraise 上下文**：`raise` 无参数只在 `catch` 块内合法；语义检查在编译器（T046）阶段。
- **`from` 子句**：`from` 在 `raise … from …` 是语法关键字；在 `import … from …` 也是关键字（`TOK_FROM`，T035 使用）。
