# P2-T026 var 声明 / `:=` 短声明 / 赋值（复合/`++`/`--`）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现变量声明与赋值语句：
- `var x = expr` / `var x, y = expr, expr`（`ND_VAR_DECL`）
- `x := expr`（短声明，`ND_SHORT_DECL`）
- `x = expr`（普通赋值，`ND_ASSIGN`）
- `x += expr` 等复合赋值（`ND_COMPOUND_ASSIGN`）
- `x++` / `x--`（已在 T021 中产生 `ND_INC_DEC`，此处在语句层包装为 `ND_EXPR_STMT`）
- 多目标赋值解包：`a, b = expr`（tuple/list 解包）

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T021 | 调用/属性/下标（左值表达式） |
| P2-T017 | `ND_VAR_DECL`/`ND_SHORT_DECL`/`ND_ASSIGN`/`ND_COMPOUND_ASSIGN` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.4.2 var 声明（`var x = expr`） |
| `syntax.md` | §2.4.3 短声明（`x := expr`） |
| `syntax.md` | §2.4.4 赋值语句（`x = expr`，`x, y = …`） |
| `syntax.md` | §2.4.5 复合赋值（`+= -= *= /= %= &= |= ^= <<= >>= **=`） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c       # 在 msParseStmt 中处理 var/assign/short-decl
src/parser/ms_parse_expr.c   # parseAssignTarget（左值检验辅助）
```

---

## 实现要点

### 1. `var` 声明

```c
// msParseStmt 中，match(TOK_VAR) 分支：
static MsNode* parseVarDecl(MsParser* p) {
  MsSrcPos pos = p->prev.pos;

  // 收集变量名列表（支持解包：var a, b = …）
  MsNodeList* names = NULL;
  MsNodeList** nameTail = &names;
  do {
    expect(p, TOK_IDENT, "expected variable name");
    MsNode* nameNode = MS_ARENA_NEW(p->arena, MsNode);
    nameNode->kind      = ND_IDENT;
    nameNode->pos       = p->prev.pos;
    nameNode->ident.name = p->prev.start;
    nameNode->ident.len  = p->prev.len;
    MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
    item->node = nameNode; item->next = NULL;
    *nameTail = item; nameTail = &item->next;
  } while (match(p, TOK_COMMA));

  // 可选初始值
  MsNodeList* inits = NULL;
  if (match(p, TOK_ASSIGN)) {
    inits = NULL;
    MsNodeList** initTail = &inits;
    do {
      MsNode* val = parsePrecedence(p, PREC_OR);
      MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
      item->node = val; item->next = NULL;
      *initTail = item; initTail = &item->next;
    } while (match(p, TOK_COMMA));
  }

  // 单变量：ND_VAR_DECL；多变量：包装为 ND_BLOCK([ND_VAR_DECL, …]) 或 ND_ASSIGN 解包
  // 简化：单变量直接 ND_VAR_DECL，多变量 ND_ASSIGN(target=ND_TUPLE(names), value=ND_TUPLE(inits))
  if (names->next == NULL) {
    // 单变量
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind               = ND_VAR_DECL;
    n->pos                = pos;
    n->var_decl.name      = names->node->ident.name;
    n->var_decl.nameLen   = names->node->ident.len;
    n->var_decl.init      = inits ? inits->node : NULL;
    return n;
  } else {
    // 多变量解包：var a, b = expr1, expr2
    MsNode* target = MS_ARENA_NEW(p->arena, MsNode);
    target->kind = ND_TUPLE; target->container.elems = names;
    MsNode* value  = MS_ARENA_NEW(p->arena, MsNode);
    value->kind  = ND_TUPLE;
    value->container.elems = inits;
    MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
    n->kind           = ND_ASSIGN;
    n->pos            = pos;
    n->assign.target  = target;
    n->assign.value   = value;
    return n;
  }
}
```

### 2. 短声明 `:=`

```c
// 检测：在 parseExprStmt 中，解析到 ND_IDENT 后，check TOK_COLON_ASSIGN
// 在 msParseStmt 中：
static MsNode* parseShortDecl(MsParser* p, MsNode* target) {
  // 已消耗 ':='（p->prev.kind == TOK_COLON_ASSIGN）
  MsNode* value = msParseExpr(p);  // 支持裸 tuple：a, b := 1, 2
  value = parseMaybeTuple(p, value);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind             = ND_SHORT_DECL;
  n->pos              = target->pos;
  n->var_decl.name    = target->ident.name;  // 单目标
  n->var_decl.nameLen = target->ident.len;
  n->var_decl.init    = value;
  // 多目标（a, b := …）：target 为 ND_TUPLE，init 为 ND_TUPLE
  return n;
}
```

### 3. 普通赋值与复合赋值

```c
// 在 msParseStmt 中，解析表达式后：
MsNode* exprStmt = msParseExpr(p);
exprStmt = parseMaybeTuple(p, exprStmt);  // 裸 tuple 目标

// 检查赋值运算符
MsTokKind assignOp = p->cur.kind;
if (assignOp == TOK_ASSIGN) {
  advance(p);
  MsNode* value = msParseExpr(p);
  value = parseMaybeTuple(p, value);
  // ND_ASSIGN
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind          = ND_ASSIGN;
  n->assign.target = exprStmt;
  n->assign.value  = value;
  return n;
} else if (isCompoundAssign(assignOp)) {
  advance(p);
  MsNode* value = msParseExpr(p);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind         = ND_COMPOUND_ASSIGN;
  n->binary.op    = assignOp;
  n->binary.left  = exprStmt;
  n->binary.right = value;
  return n;
} else if (assignOp == TOK_COLON_ASSIGN) {
  advance(p);
  // 短声明
  return parseShortDecl(p, exprStmt);
}
// 否则为表达式语句
MsNode* stmt = MS_ARENA_NEW(p->arena, MsNode);
stmt->kind        = ND_EXPR_STMT;
stmt->expr_stmt.expr = exprStmt;
return stmt;
```

---

## 验收标准（checklist）

- [ ] `"var x = 1"` → `ND_VAR_DECL(name="x", init=ND_INT(1))`。
- [ ] `"var x"` → `ND_VAR_DECL(name="x", init=NULL)`（零值初始化，运行期）。
- [ ] `"var a, b = 1, 2"` → `ND_ASSIGN(target=ND_TUPLE([a,b]), value=ND_TUPLE([1,2]))`。
- [ ] `"x := 42"` → `ND_SHORT_DECL(name="x", init=ND_INT(42))`。
- [ ] `"a, b := 1, 2"` → 多目标短声明。
- [ ] `"x = 10"` → `ND_ASSIGN`。
- [ ] `"x += 5"` → `ND_COMPOUND_ASSIGN(op=TOK_PLUS_ASSIGN, left=x, right=5)`。
- [ ] `"a.b = 1"` → `ND_ASSIGN(target=ND_ATTR(a,"b"), value=1)`。
- [ ] `"a[0] = 2"` → `ND_ASSIGN(target=ND_INDEX(a,0), value=2)`。
- [ ] `"x = y = 1"` → 语法错误（mslang 不支持链式赋值，仅允许单赋值语句）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_var_assign.c`）

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

static void testVarDecl(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "var x = 42");
  MS_ASSERT_EQ(n->kind, ND_VAR_DECL, "var decl");
  MS_ASSERT_EQ(n->var_decl.init->lit_int.ival, 42, "init=42");
  msArenaFree(&a);
}

static void testShortDecl(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "x := 42");
  MS_ASSERT_EQ(n->kind, ND_SHORT_DECL, "short decl");
  msArenaFree(&a);
}

static void testCompoundAssign(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "x += 5");
  MS_ASSERT_EQ(n->kind, ND_COMPOUND_ASSIGN, "compound assign");
  MS_ASSERT_EQ(n->binary.op, TOK_PLUS_ASSIGN, "+=");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testVarDecl);
  MS_RUN(testShortDecl);
  MS_RUN(testCompoundAssign);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// var 声明
var x = 10
var y          // nil 初始化
print(x, y)    // 10 nil

// 短声明
z := 42
print(z)       // 42

// 多变量短声明
a, b := 1, 2
print(a, b)    // 1 2

// 复合赋值
x += 5
print(x)       // 15
x **= 2
print(x)       // 225

// 属性赋值
class Point { func __init__(self, x, y) { self.x = x; self.y = y } }
p := Point(1, 2)
p.x = 10
print(p.x)    // 10

// 解包赋值
lst := [1, 2, 3]
a, b, c = lst[0], lst[1], lst[2]
print(a, b, c)  // 1 2 3
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **左值检验（lvalue）**：`x++`、`x = …` 需要左侧是合法 lvalue（变量、属性、下标）；parser 暂不检查，编译器（T040）在 `store` 指令生成时验证。
- **链式赋值不支持**：`x = y = 1` 在 mslang 中不合法；parser 解析 `x = y` 后回头看到 `= 1` 时，已进入新语句解析，由语句分隔符自然截断。
- **多目标赋值 vs 函数返回**：`a, b = func()` 要求 `func()` 返回 tuple；运行期检查（VM/T068）。
- **`var` 无类型注解（初版）**：mslang 初版无静态类型注解（动态类型语言）；`var x int` 语法保留为扩展，初版仅支持 `var x = expr` 和 `var x`。
