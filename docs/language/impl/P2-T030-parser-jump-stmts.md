# P2-T030 return / break / continue / pass / del / assert

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现所有"跳转/控制"类简单语句的解析：`return`、`break`、`continue`、`pass`、`del`、`assert`，产生对应 AST 节点。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 / `msParseStmt` 骨架 |
| P2-T017 | `ND_RETURN`/`ND_BREAK`/`ND_CONTINUE`/`ND_PASS`/`ND_DEL`/`ND_ASSERT` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.4.11 return 语句（单值/多值/空 return） |
| `syntax.md` | §2.4.12 break / continue |
| `syntax.md` | §2.4.13 pass |
| `syntax.md` | §2.4.14 del 语句 |
| `syntax.md` | §2.4.15 assert 语句 |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c   # 各语句分支（在 msParseStmt switch 中）
```

---

## 实现要点

### 1. `return [expr[, expr2, …]]`

```c
case TOK_RETURN: {
  MsSrcPos pos = p->prev.pos;
  MsNode* expr = NULL;
  // 若当前 token 不是语句分隔符，解析返回值（支持裸 tuple）
  if (!check(p, TOK_NEWLINE) && !check(p, TOK_SEMICOLON) && !check(p, TOK_EOF)) {
    expr = msParseExpr(p);
    expr = parseMaybeTuple(p, expr);  // return a, b → ND_TUPLE
  }
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind            = ND_RETURN;
  n->pos             = pos;
  n->single_expr.expr = expr;
  return n;
}
```

### 2. `break` / `continue`

```c
case TOK_BREAK: {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind       = ND_BREAK;
  n->pos        = p->prev.pos;
  n->jump.label = NULL;  // 初版不支持 label
  return n;
}
case TOK_CONTINUE: {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind       = ND_CONTINUE;
  n->pos        = p->prev.pos;
  n->jump.label = NULL;
  return n;
}
```

### 3. `pass`

```c
case TOK_PASS: {
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = ND_PASS;
  n->pos  = p->prev.pos;
  return n;
}
```

### 4. `del expr`

```c
case TOK_DEL: {
  MsSrcPos pos = p->prev.pos;
  MsNode* target = msParseExpr(p);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind             = ND_DEL;
  n->pos              = pos;
  n->single_expr.expr = target;
  return n;
}
```

`del` 的合法目标：变量名（从局部/全局作用域删除）、属性 `del obj.x`、下标 `del lst[i]`。非 lvalue 在编译期报错。

### 5. `assert expr [, msg]`

```c
case TOK_ASSERT: {  // 注意：assert 是关键字（TOK_ASSERT 需在 T007 中添加）
  MsSrcPos pos = p->prev.pos;
  MsNode* cond = msParseExpr(p);
  MsNode* msg  = NULL;
  if (match(p, TOK_COMMA)) {
    msg = msParseExpr(p);
  }
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind              = ND_ASSERT;
  n->pos               = pos;
  n->single_expr.expr  = cond;
  n->single_expr.expr2 = msg;
  return n;
}
```

**注**：`assert` 需要在 T007（关键字表）中添加 `TOK_ASSERT`。

---

## 验收标准（checklist）

- [ ] `"return"` → `ND_RETURN(expr=NULL)`（空 return）。
- [ ] `"return 42"` → `ND_RETURN(expr=ND_INT(42))`。
- [ ] `"return a, b"` → `ND_RETURN(expr=ND_TUPLE([a,b]))`（裸 tuple）。
- [ ] `"break"` → `ND_BREAK(label=NULL)`。
- [ ] `"continue"` → `ND_CONTINUE(label=NULL)`。
- [ ] `"pass"` → `ND_PASS`。
- [ ] `"del x"` → `ND_DEL(target=ND_IDENT(x))`。
- [ ] `"del a[0]"` → `ND_DEL(target=ND_INDEX(a,0))`。
- [ ] `"del obj.x"` → `ND_DEL(target=ND_ATTR(obj,x))`。
- [ ] `"assert cond"` → `ND_ASSERT(cond=cond, msg=NULL)`。
- [ ] `"assert x > 0, \"must be positive\""` → `ND_ASSERT(cond=…, msg=ND_STRING(…))`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_jump_stmts.c`）

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

static void testReturn(void) {
  MsArena a; msArenaInit(&a);
  MS_ASSERT_EQ(pStmt(&a, "return")->kind,       ND_RETURN,   "return");
  MS_ASSERT_EQ(pStmt(&a, "return 1")->kind,     ND_RETURN,   "return 1");
  MS_ASSERT_EQ(pStmt(&a, "break")->kind,        ND_BREAK,    "break");
  MS_ASSERT_EQ(pStmt(&a, "continue")->kind,     ND_CONTINUE, "continue");
  MS_ASSERT_EQ(pStmt(&a, "pass")->kind,         ND_PASS,     "pass");
  MS_ASSERT_EQ(pStmt(&a, "del x")->kind,        ND_DEL,      "del");
  MS_ASSERT_EQ(pStmt(&a, "assert x")->kind,     ND_ASSERT,   "assert");
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
func check_pos(n) {
    assert n > 0, $"n must be positive, got {n}"
}
check_pos(5)   // OK
// check_pos(-1)  // AssertionError: n must be positive, got -1
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`assert` 关键字**：需在 T007 关键字表中添加（`assert` 可能在第一版设计文档中未明确列出，需核对 `syntax.md §1.4`）。
- **`del` 作用域**：`del x` 从当前作用域删除变量（类 Python）；`del obj.x` 删除属性；`del lst[i]` 删除索引。运行期行为在 VM（T040/T066）实现。
- **`return` 在函数外**：`return` 出现在函数体外是语义错误，由编译器（T043）检测。
