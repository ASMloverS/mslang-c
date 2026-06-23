# P2-T030 return / break / continue / pass / del / assert

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现所有"跳转/控制"类简单语句的解析：`return`、`break`、`continue`、`pass`、`del`、`assert`，产生对应 AST 节点。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 / `msParseStmt` 骨架 |
| P2-T017 | `MS_ND_RETURN`/`MS_ND_BREAK`/`MS_ND_CONTINUE`/`MS_ND_PASS`/`MS_ND_DEL`/`MS_ND_ASSERT` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.2 ReturnStmt / BreakStmt / ContinueStmt / DelStmt / AssertStmt / PassStmt 文法 |
| `syntax.md` | §1.4 关键字语义（assert/del/pass 等） |
| `ms_ast.h` | `MS_ND_RETURN`/`MS_ND_BREAK`/`MS_ND_CONTINUE`/`MS_ND_PASS`/`MS_ND_DEL`/`MS_ND_ASSERT`、`singleExpr`/`jump` 字段 |
| `ms_lexer.h` | `MS_TOK_RETURN`/`MS_TOK_BREAK`/`MS_TOK_CONTINUE`/`MS_TOK_PASS`/`MS_TOK_DEL`/`MS_TOK_ASSERT` |
| `ms_parser.h` | `msParserCheck`/`msParserMatch`/`msParseExpr` API |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c   # 各语句分支（在 msParseStmt switch 中）
```

---

## 实现要点

### 1. `return [expr]`

```c
case MS_TOK_RETURN: {
  struct MsSrcPos pos = p->prev.pos;
  MsNode* expr = NULL;
  // 若当前 token 不是语句分隔符，解析返回值（单个 Expr，§2.2 ReturnStmt）
  if (!msParserCheck(p, MS_TOK_NEWLINE) && !msParserCheck(p, MS_TOK_SEMICOLON) && !msParserCheck(p, MS_TOK_EOF)) {
    expr = msParseExpr(p);
  }
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind            = MS_ND_RETURN;
  n->pos             = pos;
  n->singleExpr.expr = expr;
  return n;
}
```

### 2. `break` / `continue`

```c
case MS_TOK_BREAK: {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind       = MS_ND_BREAK;
  n->pos        = p->prev.pos;
  n->jump.label = NULL;  // 初版不支持 label
  return n;
}
case MS_TOK_CONTINUE: {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind       = MS_ND_CONTINUE;
  n->pos        = p->prev.pos;
  n->jump.label = NULL;
  return n;
}
```

### 3. `pass`

```c
case MS_TOK_PASS: {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_PASS;
  n->pos  = p->prev.pos;
  return n;
}
```

### 4. `del expr`

```c
case MS_TOK_DEL: {
  struct MsSrcPos pos = p->prev.pos;
  MsNode* target = msParseExpr(p);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind             = MS_ND_DEL;
  n->pos              = pos;
  n->singleExpr.expr = target;
  return n;
}
```

`del` 的合法目标：变量名（从局部/全局作用域删除）、属性 `del obj.x`、下标 `del lst[i]`。Parser 接受任意 Expr 作为 del 目标（不在文法层强制 LValue），lvalue 合法性校验延后至编译器 T047（`with`/`del`/`assert` 编译）；此为有意偏离 `syntax.md §2.2` 文法 `DelStmt = 'del' LValue ';'` 的实现策略。`del` 后无表达式（如裸 `del` 后跟换行）时，`msParseExpr` 将产生 parse error。

### 5. `assert expr [, msg]`

```c
case MS_TOK_ASSERT: {  // assert cond [, msg]; msg 可选
  struct MsSrcPos pos = p->prev.pos;
  MsNode* cond = msParseExpr(p);
  MsNode* msg  = NULL;
  if (msParserMatch(p, MS_TOK_COMMA)) {
    msg = msParseExpr(p);
  }
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind              = MS_ND_ASSERT;
  n->pos               = pos;
  n->singleExpr.expr  = cond;
  n->singleExpr.expr2 = msg;
  return n;
}
```

`MS_TOK_ASSERT` 已由 lexer 提供（`ms_lexer.h`），`assert` 已是 `syntax.md §1.4` 保留字。

---

## 验收标准（checklist）

- [x] `"return"` → `MS_ND_RETURN(expr=NULL)`（空 return）。 <!-- v:ctest:test_jump_stmts -->
- [x] `"return 42"` → `MS_ND_RETURN(expr=MS_ND_INT(42))`。 <!-- v:ctest:test_jump_stmts -->
- [x] `"break"` → `MS_ND_BREAK(label=NULL)`。 <!-- v:ctest:test_jump_stmts -->
- [x] `"continue"` → `MS_ND_CONTINUE(label=NULL)`。 <!-- v:ctest:test_jump_stmts -->
- [x] `"pass"` → `MS_ND_PASS`。 <!-- v:ctest:test_jump_stmts -->
- [x] `"del x"` → `MS_ND_DEL(target=MS_ND_IDENT(x))`。 <!-- v:ctest:test_jump_stmts -->
- [x] `"del a[0]"` → `MS_ND_DEL(target=MS_ND_INDEX(a,0))`。 <!-- v:ctest:test_jump_stmts -->
- [x] `"del obj.x"` → `MS_ND_DEL(target=MS_ND_ATTR(obj,x))`。 <!-- v:ctest:test_jump_stmts -->
- [x] `"assert cond"` → `MS_ND_ASSERT(cond=cond, msg=NULL)`。 <!-- v:ctest:test_jump_stmts -->
- [x] `"assert x > 0, \"must be positive\""` → `MS_ND_ASSERT(cond=…, msg=MS_ND_STRING(…))`。 <!-- v:ctest:test_jump_stmts -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_jump_stmts.c`）

```c
#include <string.h>

#include "ms_test.h"
#include "mslang/ms_parser.h"
#include "mslang/ms_ast.h"
#include "ms_arena.h"

static MsNode* pStmt(MsArena* a, const char* s) {
  MsParser p;
  msParserInit(&p, s, (uint32_t)strlen(s), "<t>", a);
  return msParseStmt(&p);
}

static void testReturn(void) {
  MsArena a; msArenaInit(&a);
  MS_ASSERT_EQ(pStmt(&a, "return")->kind,       MS_ND_RETURN,   "return");
  MS_ASSERT_EQ(pStmt(&a, "return 1")->kind,     MS_ND_RETURN,   "return 1");
  MS_ASSERT_EQ(pStmt(&a, "break")->kind,        MS_ND_BREAK,    "break");
  MS_ASSERT_EQ(pStmt(&a, "continue")->kind,     MS_ND_CONTINUE, "continue");
  MS_ASSERT_EQ(pStmt(&a, "pass")->kind,         MS_ND_PASS,     "pass");
  MS_ASSERT_EQ(pStmt(&a, "del x")->kind,        MS_ND_DEL,      "del");
  MS_ASSERT_EQ(pStmt(&a, "assert x")->kind,     MS_ND_ASSERT,   "assert");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testReturn);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// return
func abs(x) {
    if x < 0 { return -x }
    return x
}
print(abs(-5))  // 5

// break / continue
for i in range(10) {
    if i == 3 { break }
    if i % 2 == 0 { continue }
    print(i)   // 1
}

// pass（空块占位）
func noop() {
    pass
}
noop()

// del
d := {"a": 1, "b": 2}
del d["a"]
print(d)   // {"b": 2}

x := 42
del x
// print(x)  // 运行时错误：x 未定义

// assert
func checkPos(n) {
    assert n > 0, $"n must be positive, got {n}"
}
checkPos(5)   // OK
// checkPos(-1)  // AssertionError: n must be positive, got -1
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`assert` 语义**：`assert` 已是 `syntax.md §1.4` 保留字，`MS_TOK_ASSERT` 已由 lexer 提供。`assert` 后无表达式时 `msParseExpr` 将产生 parse error。运行期 assert 失败抛出 `AssertionError`，由 T084 实现。
- **`del` 作用域**：`del x` 从当前作用域删除变量（类 Python）；`del obj.x` 删除属性；`del lst[i]` 删除索引。运行期行为在 VM（T040/T066）实现。
- **`return` 在函数外**：`return` 出现在函数体外是语义错误，由编译器（T043）检测。
