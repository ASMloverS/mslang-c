# P2-T034 func / class 声明 + ParamList（默认值/vararg/kwarg）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现语句级的命名函数声明（`func name(params) { body }`）与 class 声明（`class Name [extends Base] { body }`）。参数列表解析（`msParseParamList`）已在 T024 完整实现，本任务直接复用。这是 P2 最复杂的语句解析任务。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T024 | `parseFuncLit`/`msParseParamList`（完整参数列表解析） |
| P2-T027 | `parseBlock` |
| P2-T017 | `MS_ND_FUNC_DECL`/`MS_ND_CLASS_DECL`/`MS_ND_PARAM` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.1 FuncDecl（`[ 'async' ] 'func' identifier '(' ParamList ')' Block`） |
| `syntax.md` | §2.1 ParamList（`[ Param { ',' Param } [ ',' '...' identifier ] [ ',' '**' identifier ] ]`） |
| `syntax.md` | §2.1 ClassDecl（`'class' identifier [ 'extends' identifier ] '{' { MethodDecl } '}'`） |
| `type-system.md` | §3 class（MRO/方法解析/魔术方法）、§3.6 类属性 vs 实例属性 |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c       # parseFuncDecl / parseClassDecl + msParseStmt 分派
```

> **注意：** `msParseParamList` 已在 T024（`src/parser/ms_parse_expr.c:481`）完整实现，支持 `...args`/`**kwargs`/默认值/错误校验，本任务直接复用，无需修改。

---

## 实现要点

### 1. `MS_ND_PARAM` 节点字段（T017 已定义，无需追加）

T017 §3 已定义 `MS_ND_PARAM` 的完整字段，本任务直接使用：

```c
// MS_ND_PARAM（参数定义）— T017 权威定义
struct {
  const char* name;
  uint32_t    nameLen;
  MsNode*     defaultVal;   // 默认值（NULL → 无）
  bool        isVararg;     // ...args
  bool        isKwarg;      // **kwargs
} param;
```

### 2. `msParseParamList`（T024 已完整实现，本任务复用）

T024 在 `src/parser/ms_parse_expr.c:481` 已导出 `msParseParamList`，完整支持：

- `...args`（`MS_TOK_DOTDOTDOT`）— 须有前导位置参数
- `**kwargs`（`MS_TOK_STARSTAR`）— 至多一个，须在末尾
- 默认值（`MS_TOK_ASSIGN` → `msParseExpr`）
- 错误校验：非默认参数在默认参数之后、重复 vararg/kwarg 等

本任务 `parseFuncDecl` 直接调用 `msParseParamList(p)` 即可，无需修改或重写。

### 3. 命名函数声明

**`msParseStmt` 中 `MS_TOK_ASYNC` 分派逻辑：** 遇到 `MS_TOK_ASYNC` 时，前瞻下一 token——若为 `MS_TOK_FUNC` 且其后为 `MS_TOK_IDENT`，则走 `parseFuncDecl`（语句级声明）；否则交回表达式路径（T024 `parseFuncLit` 处理匿名 `async func(){}` 字面量）。

```c
// msParseStmt 中，MS_TOK_FUNC 分支：
static MsNode* parseFuncDecl(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;
  bool isAsync = false;

  if (p->prev.kind == MS_TOK_ASYNC) {
    msParserExpect(p, MS_TOK_FUNC, "expected 'func' after 'async'");
    isAsync = true;
  }

  msParserExpect(p, MS_TOK_IDENT, "expected function name");
  const char* name = p->prev.start;
  uint32_t nameLen = p->prev.len;

  msParserExpect(p, MS_TOK_LPAREN, "expected '(' after function name");
  MsNodeList* params = msParseParamList(p);
  msParserExpect(p, MS_TOK_RPAREN, "expected ')' after parameters");

  // msParseBlock 要求调用者先消耗 '{'；用 parseBlock（T024 静态函数）则无需
  // parseFuncDecl 定义在 ms_parser.c，无法访问 ms_parse_expr.c 的 static parseBlock
  msParserExpect(p, MS_TOK_LBRACE, "expected '{' before function body");
  MsNode* body = msParseBlock(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = isAsync ? MS_ND_ASYNC_FUNC : MS_ND_FUNC_DECL;
  n->pos = pos;
  n->funcDecl.name = name;
  n->funcDecl.params = params;
  n->funcDecl.body = body;
  n->funcDecl.isAsync = isAsync;
  return n;
}
```

### 4. class 声明

```c
// msParseStmt 中，MS_TOK_CLASS 分支：
static MsNode* parseClassDecl(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;

  msParserExpect(p, MS_TOK_IDENT, "expected class name");
  const char* name = p->prev.start;
  uint32_t nameLen = p->prev.len;

  // 基类（可选，单继承）：class Foo extends Bar { }
  // syntax.md §2.1: extends 后只接单个 identifier
  MsNode* base = NULL;
  if (msParserMatch(p, MS_TOK_EXTENDS)) {
    msParserExpect(p, MS_TOK_IDENT, "expected base class name");
    base = MS_ARENA_NEW(p->arena, MsNode);
    base->kind = MS_ND_IDENT;
    base->pos = p->prev.pos;
    base->ident.name = p->prev.start;
    base->ident.len = p->prev.len;
  }

  msParserExpect(p, MS_TOK_LBRACE, "expected '{' after class declaration");
  MsNodeList* body = NULL;
  MsNodeList** bodyTail = &body;
  while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {}

  while (!msParserCheck(p, MS_TOK_RBRACE) && !msParserCheck(p, MS_TOK_EOF)) {
    MsNode* member = msParseStmt(p);
    if (member) {
      msNodeListAppend(p, &bodyTail, member);
    }
    while (msParserMatch(p, MS_TOK_NEWLINE) || msParserMatch(p, MS_TOK_SEMICOLON)) {}
  }
  msParserExpect(p, MS_TOK_RBRACE, "expected '}' to close class body");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind = MS_ND_CLASS_DECL;
  n->pos = pos;
  n->classDecl.name = name;
  n->classDecl.base = base;
  n->classDecl.body = body;
  return n;
}
```

> **class body 成员校验：** `syntax.md §2.1` 文法限定类体仅含 `MethodDecl`（`[ 'async' ] 'func' ...`），但 `type-system.md §3.6` 描述类属性在 `class {}` 块顶层赋值。初版 parser 使用 `msParseStmt` 解析类体成员，接受 `MS_ND_FUNC_DECL`/`MS_ND_ASYNC_FUNC`（方法）以及 `MS_ND_ASSIGN`/`MS_ND_VAR_DECL`/`MS_ND_SHORT_DECL`（类属性赋值）。实现时应校验 `member->kind`，对其他语句类型（`if`/`for`/`return` 等）报错。

---

## 验收标准（checklist）

- [ ] `"func f() {}"` → `MS_ND_FUNC_DECL(name="f", params=[], body=BLOCK)`。
- [ ] `"func f(a, b) { return a + b }"` → params=[a, b]。
- [ ] `"func f(a, b=1, ...args, **kw) {}"` → 4 个参数，顺序正确。
- [ ] `"func f(b=1, a) {}"` → 语法错误（non-default parameter after default parameter）。
- [ ] `"async func f() {}"` → `MS_ND_ASYNC_FUNC(funcDecl.isAsync=true)`。
- [ ] `"class Foo {}"` → `MS_ND_CLASS_DECL(name="Foo", base=NULL, body=NULL)`。
- [ ] `"class Foo extends Bar {}"` → `classDecl.base` = `MS_ND_IDENT("Bar")`（单继承）。
- [ ] class body 中的方法解析为 `MS_ND_FUNC_DECL` 节点。
- [ ] class body 中的赋值解析为 `MS_ND_ASSIGN`/`MS_ND_VAR_DECL`（类属性）。
- [ ] `"func f(a=1+2) {}"` → `param.defaultVal` 为 `MS_ND_BINARY` 节点（默认值保留为 AST）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_func_class_decl.c`）

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

static void testFuncDecl(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "func add(a, b) { return a + b }");
  MS_ASSERT_EQ(n->kind, MS_ND_FUNC_DECL, "func decl");
  MS_ASSERT_TRUE(n->funcDecl.name != NULL, "named");
  int cnt = 0;
  for (MsNodeList* l = n->funcDecl.params; l; l = l->next) cnt++;
  MS_ASSERT_EQ(cnt, 2, "2 params");
  msArenaFree(&a);
}

static void testClassDecl(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "class Point extends Base { func __init__(self) { } }");
  MS_ASSERT_EQ(n->kind, MS_ND_CLASS_DECL, "class");
  MS_ASSERT_TRUE(n->classDecl.base != NULL, "has base");
  MS_ASSERT_TRUE(n->classDecl.body != NULL, "has body");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testFuncDecl);
  MS_RUN(testClassDecl);
  return msTestSummary();
}
```

### .ms 使用示例（T067/T072 后验证）

```ms
// 函数
func greet(name, greeting="Hello") {
    return $"{greeting}, {name}!"
}
print(greet("world"))         // Hello, world!
print(greet("you", "Hi"))     // Hi, you!


// vararg / kwarg（syntax.md §2.1/§3.4: ...args 须有前导位置参数）
func show(first, ...args, **kwargs) {
    print(first)   // 首个位置参数
    print(args)    // list
    print(kwargs)  // map
}
show(1, 2, 3, a=4, b=5)


// class（syntax.md §2.1: 类体仅含 MethodDecl；类属性待文法扩展确认后补充）
class Animal {
    func __init__(self, name) {
        self.name = name
    }

    func speak(self) {
        print($"{self.name} says ...")
    }
}


class Dog extends Animal {
    func speak(self) {
        print($"{self.name} says Woof")
    }
}


d := Dog("Rex")
d.speak()   // Rex says Woof
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`self` 参数**：mslang 中 `self` 是约定（非关键字），任何标识符名均可用作实例参数（与 Python 相同）。
- **方法 vs 函数**：class body 中的 `func` 声明是方法；parser 不区分（均为 `MS_ND_FUNC_DECL`），编译器（T044）在 `MAKE_CLASS` 时按位置判断。
- **装饰器**：初版不支持 `@decorator` 装饰器语法；预留节点字段，后续版本扩展。
- **`extends` 关键字**：需在 T007 关键字表中确认（`syntax.md §1.4` 已列 `extends` 为关键字 `MS_TOK_EXTENDS`）。
- **`async func` 分派**：`msParseStmt` 遇 `MS_TOK_ASYNC` 时需前瞻——若后续为 `MS_TOK_FUNC` 且再后为 `MS_TOK_IDENT`，走 `parseFuncDecl`（语句级声明）；否则回退到表达式路径由 T024 `parseFuncLit` 处理匿名 `async func(){}` 字面量。
- **class body 校验**：`syntax.md §2.1` 文法限定类体仅含 `MethodDecl`，但 `type-system.md §3.6` 允许类属性赋值。初版 parser 对 `msParseStmt` 返回的成员校验 `kind`，仅接受方法与赋值类节点。若确需支持类属性 `:=` 声明，须先在 `syntax.md §2.1` 扩展 `ClassDecl` 文法。
- **`parseBlock` 可见性**：`parseFuncDecl`/`parseClassDecl` 定义在 `ms_parser.c`，无法访问 `ms_parse_expr.c` 的 `static parseBlock`；须用 `msParserExpect(MS_TOK_LBRACE)` + `msParseBlock(p)` 组合。
