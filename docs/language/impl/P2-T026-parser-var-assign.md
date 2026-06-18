# P2-T026 var 声明 / `:=` 短声明 / 赋值（复合/`++`/`--`）

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现变量声明与赋值语句：
- `var x = expr` / `var x`（`MS_ND_VAR_DECL`）
- `x := expr`（短声明，`MS_ND_SHORT_DECL`）
- `x = expr`（普通赋值，`MS_ND_ASSIGN`）
- `x += expr` 等复合赋值（`MS_ND_COMPOUND_ASSIGN`）
- `x++` / `x--`（已在 T021 中产生 `MS_ND_INC_DEC`，此处在语句层包装为 `MS_ND_EXPR_STMT`）

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | Pratt 框架 |
| P2-T021 | 调用/属性/下标（左值表达式） |
| P2-T017 | `MS_ND_VAR_DECL`/`MS_ND_SHORT_DECL`/`MS_ND_ASSIGN`/`MS_ND_COMPOUND_ASSIGN` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.2 语句 — `VarDecl` / `ShortVarDecl` / `AssignStmt` / `LValue` |
| `syntax.md` | §3.1 短变量声明（重复声明即赋值） |
| `syntax.md` | §1.10 运算符与界符（复合赋值算子集合：`+= -= *= /= %= &= |= ^= <<= >>=`） |

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

`syntax.md §2.2`：`VarDecl = 'var' identifier [ '=' Expr ] ';'`——单标识符，可选初始值（省略时运行期为 `nil`）。

```c
// msParseStmt 中，msParserMatch(MS_TOK_VAR) 分支：
static MsNode* parseVarDecl(MsParser* p) {
  MsSrcPos pos = p->prev.pos;
  msParserExpect(p, MS_TOK_IDENT, "expected variable name");

  const char* name = p->prev.start;
  uint32_t nameLen = p->prev.len;

  MsNode* init = NULL;
  if (msParserMatch(p, MS_TOK_ASSIGN)) {
    init = msParseExpr(p);
  }

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind            = MS_ND_VAR_DECL;
  n->pos             = pos;
  n->varDecl.name    = name;
  n->varDecl.nameLen = nameLen;
  n->varDecl.init    = init;
  return n;
}
```

### 2. 短声明 `:=`

`syntax.md §2.2`：`ShortVarDecl = identifier ':=' Expr ';'`——单标识符。在 `msParseStmt` 中，先解析表达式得到 `MS_ND_IDENT`，再检测 `MS_TOK_COLON_ASSIGN`。

```c
static MsNode* parseShortDecl(MsParser* p, MsNode* target) {
  // 已消耗 ':='
  MsNode* value = msParseExpr(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind            = MS_ND_SHORT_DECL;
  n->pos             = target->pos;
  n->varDecl.name    = target->ident.name;
  n->varDecl.nameLen = target->ident.len;
  n->varDecl.init    = value;
  return n;
}
```

### 3. 普通赋值与复合赋值

`syntax.md §2.2`：`AssignStmt = LValue ('=' | CompoundAssign) Expr ';' | LValue ('++' | '--') ';'`，`LValue = identifier | Expr '.' identifier | Expr '[' Expr ']'`。

```c
// 在 msParseStmt 中，解析表达式后检查赋值运算符：
MsNode* expr = msParseExpr(p);

MsTokKind assignOp = p->cur.kind;
if (assignOp == MS_TOK_ASSIGN) {
  msParserAdvance(p);
  MsNode* value = msParseExpr(p);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind          = MS_ND_ASSIGN;
  n->assign.target = expr;
  n->assign.value  = value;
  return n;
} else if (isCompoundAssign(assignOp)) {
  msParserAdvance(p);
  MsNode* value = msParseExpr(p);
  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind         = MS_ND_COMPOUND_ASSIGN;
  n->binary.op    = assignOp;
  n->binary.left  = expr;
  n->binary.right = value;
  return n;
} else if (assignOp == MS_TOK_COLON_ASSIGN) {
  msParserAdvance(p);
  return parseShortDecl(p, expr);
}
// 否则为表达式语句
MsNode* stmt = MS_ARENA_NEW(p->arena, MsNode);
stmt->kind           = MS_ND_EXPR_STMT;
stmt->exprStmt.expr  = expr;
return stmt;
```

---

## 验收标准（checklist）

- [ ] `"var x = 1"` → `MS_ND_VAR_DECL(name="x", init=MS_ND_INT(1))`。
- [ ] `"var x"` → `MS_ND_VAR_DECL(name="x", init=NULL)`（nil 初始化，运行期）。
- [ ] `"x := 42"` → `MS_ND_SHORT_DECL(name="x", init=MS_ND_INT(42))`。
- [ ] `"x = 10"` → `MS_ND_ASSIGN`。
- [ ] `"x += 5"` → `MS_ND_COMPOUND_ASSIGN(op=MS_TOK_PLUS_ASSIGN, left=x, right=5)`。
- [ ] `"a.b = 1"` → `MS_ND_ASSIGN(target=MS_ND_ATTR(a,"b"), value=1)`。
- [ ] `"a[0] = 2"` → `MS_ND_ASSIGN(target=MS_ND_INDEX(a,0), value=2)`。
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
  MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "var x = 42");
  MS_ASSERT_EQ(n->kind, MS_ND_VAR_DECL, "var decl");
  MS_ASSERT_EQ(n->varDecl.init->kind, MS_ND_INT, "init kind");
  MS_ASSERT_EQ(n->varDecl.init->litInt.ival, 42, "init=42");
  msArenaFree(&a);
}

static void testShortDecl(void) {
  MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "x := 42");
  MS_ASSERT_EQ(n->kind, MS_ND_SHORT_DECL, "short decl");
  msArenaFree(&a);
}

static void testCompoundAssign(void) {
  MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "x += 5");
  MS_ASSERT_EQ(n->kind, MS_ND_COMPOUND_ASSIGN, "compound assign");
  MS_ASSERT_EQ(n->binary.op, MS_TOK_PLUS_ASSIGN, "+=");
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


// 复合赋值
x += 5
print(x)       // 15
x <<= 1
print(x)       // 30


// 属性赋值
class Point {
    func __init__(self, x, y) {
        self.x = x
        self.y = y
    }
}

p := Point(1, 2)
p.x = 10
print(p.x)    // 10


// 下标赋值
lst := [1, 2, 3]
lst[0] = 99
print(lst)     // [99, 2, 3]
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **左值检验（lvalue）**：`x++`、`x = …` 需要左侧是合法 lvalue（变量、属性、下标）；parser 暂不检查，编译器（T040）在 `store` 指令生成时验证。
- **链式赋值不支持**：`x = y = 1` 在 mslang 中不合法；parser 解析 `x = y` 后回头看到 `= 1` 时，已进入新语句解析，由语句分隔符自然截断。
- **`var` 无类型注解（初版）**：mslang 初版无静态类型注解（动态类型语言）；`var x int` 语法保留为扩展，初版仅支持 `var x = expr` 和 `var x`（省略初值运行期为 `nil`）。
