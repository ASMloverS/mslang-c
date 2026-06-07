# P2-T034 func / class 声明 + ParamList（默认值/vararg/kwarg）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现语句级的命名函数声明（`func name(params) { body }`）与 class 声明（`class Name [extends Base] { body }`），以及完整的参数列表解析（`parseParamList`，与 T024 共用）。这是 P2 最复杂的语句解析任务。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T024 | `parseFuncLit`/`parseParamList` 基础版 |
| P2-T027 | `parseBlock` |
| P2-T017 | `ND_FUNC_DECL`/`ND_CLASS_DECL`/`ND_PARAM` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.5 函数声明（func/async func） |
| `syntax.md` | §2.5.3 参数列表（默认值/`*args`/`**kwargs`/位置专用） |
| `syntax.md` | §2.7 class 声明（继承/方法/属性） |
| `type-system.md` | §3 class（MRO/方法解析/魔术方法） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c       # parseFuncDecl / parseClassDecl
src/parser/ms_parse_expr.c   # parseParamList 完整版（升级 T024 骨架）
```

---

## 实现要点

### 1. `ND_PARAM` 节点字段（T017 追加）

```c
// ND_PARAM（参数定义）
struct {
  const char* name;       // 参数名
  uint32_t    nameLen;
  MsNode*     default_val;  // 默认值（NULL → 无）
  bool        is_vararg;    // *args
  bool        is_kwarg;     // **kwargs
  bool        kw_only;      // 关键字专用参数（*args 后的普通参数）
} param;
```

### 2. `parseParamList` 完整版

```c
MsNodeList* parseParamList(MsParser* p) {
  MsNodeList* params  = NULL;
  MsNodeList** tail   = &params;
  bool sawDefault = false;
  bool sawVararg  = false;
  bool sawKwarg   = false;
  bool kwOnly     = false;  // 进入 kw-only 区域（*args 之后）

  while (!check(p, TOK_RPAREN) && !check(p, TOK_EOF)) {
    MsNode* param = MS_ARENA_NEW(p->arena, MsNode);
    param->kind = ND_PARAM;
    param->pos  = p->cur.pos;

    if (match(p, TOK_STARSTAR)) {
      // **kwargs
      if (sawKwarg) parserError(p, "only one **kwargs allowed");
      expect(p, TOK_IDENT, "expected name after '**'");
      param->param.name       = p->prev.start;
      param->param.nameLen    = p->prev.len;
      param->param.is_kwarg   = true;
      param->param.default_val = NULL;
      sawKwarg = true;
    } else if (match(p, TOK_STAR)) {
      // *args（或裸 * 分隔位置参数与关键字专用参数）
      if (sawVararg || sawKwarg) parserError(p, "only one *args allowed");
      if (check(p, TOK_COMMA) || check(p, TOK_RPAREN)) {
        // 裸 * → 开启 kw_only 区域
        kwOnly = true;
        // 不产生 param 节点，仅设标志
        MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
        item->node = NULL; item->next = NULL;  // sentinel
        // 实际跳过这个 item：简化为继续下一 comma
      } else {
        expect(p, TOK_IDENT, "expected name after '*'");
        param->param.name     = p->prev.start;
        param->param.nameLen  = p->prev.len;
        param->param.is_vararg = true;
        sawVararg = true;
        kwOnly    = true;  // 后续参数为 kw_only
      }
    } else {
      // 普通参数
      if (sawKwarg) parserError(p, "parameter after **kwargs");
      expect(p, TOK_IDENT, "expected parameter name");
      param->param.name     = p->prev.start;
      param->param.nameLen  = p->prev.len;
      param->param.kw_only  = kwOnly;
      param->param.is_vararg = false;
      param->param.is_kwarg  = false;
      if (match(p, TOK_ASSIGN)) {
        param->param.default_val = parsePrecedence(p, PREC_OR);
        sawDefault = true;
      } else {
        if (sawDefault && !kwOnly) {
          parserError(p, "non-default argument after default argument");
        }
        param->param.default_val = NULL;
      }
    }

    MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
    item->node = param; item->next = NULL;
    *tail = item; tail = &item->next;

    if (!match(p, TOK_COMMA)) break;
  }
  return params;
}
```

### 3. 命名函数声明

```c
// msParseStmt 中，match(TOK_FUNC) 分支（语句位置）：
static MsNode* parseFuncDecl(MsParser* p) {
  MsSrcPos pos = p->prev.pos;
  bool is_async = false;

  // 支持 async func（从 TOK_ASYNC 分支跳转过来）
  if (p->prev.kind == TOK_ASYNC) {
    expect(p, TOK_FUNC, "expected 'func' after 'async'");
    is_async = true;
  }

  // 函数名（可选修饰：装饰器在初版不支持）
  const char* name = NULL;
  uint32_t nameLen = 0;
  if (check(p, TOK_IDENT)) {
    advance(p);
    name    = p->prev.start;
    nameLen = p->prev.len;
  } else {
    // 匿名函数在语句位置：var f = func() {} 中，func 作为表达式（T024）
    // 语句位置的 func 必须有名字
    parserError(p, "expected function name");
  }

  expect(p, TOK_LPAREN, "expected '(' after function name");
  MsNodeList* params = parseParamList(p);
  expect(p, TOK_RPAREN, "expected ')' after parameters");
  MsNode* body = parseBlock(p);  // 需先消耗 '{'
  // 实际：parseBlock 内部消耗 '{'；这里 expect('{') 在 parseBlock 之前？
  // 统一约定：parseBlock 调用者先 expect(LBRACE)，parseBlock 从 '{' 内部开始。
  // 修正：expect(p, TOK_LBRACE, ...); body = parseBlock(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind               = is_async ? ND_ASYNC_FUNC : ND_FUNC_DECL;
  n->pos                = pos;
  n->func_decl.name     = name;
  n->func_decl.params   = params;
  n->func_decl.body     = body;
  n->func_decl.is_async = is_async;
  return n;
}
```

### 4. class 声明

```c
// msParseStmt 中，match(TOK_CLASS) 分支：
static MsNode* parseClassDecl(MsParser* p) {
  MsSrcPos pos = p->prev.pos;

  expect(p, TOK_IDENT, "expected class name");
  const char* name    = p->prev.start;
  uint32_t    nameLen = p->prev.len;

  // 基类（可选）：class Foo extends Bar { }
  MsNodeList* bases = NULL;
  if (match(p, TOK_EXTENDS)) {
    bases = NULL;
    MsNodeList** bt = &bases;
    do {
      MsNode* base = msParseExpr(p);  // 基类表达式（ND_IDENT 或 ND_ATTR）
      MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
      item->node = base; item->next = NULL;
      *bt = item; bt = &item->next;
    } while (match(p, TOK_COMMA));
  }

  expect(p, TOK_LBRACE, "expected '{' after class declaration");
  // class body：仅允许 func 声明和赋值（类属性）
  MsNodeList* body  = NULL;
  MsNodeList** bodyTail = &body;
  while (match(p, TOK_NEWLINE) || match(p, TOK_SEMICOLON)) {}

  while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
    MsNode* member = msParseStmt(p);
    if (member) {
      MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
      item->node = member; item->next = NULL;
      *bodyTail = item; bodyTail = &item->next;
    }
    while (match(p, TOK_NEWLINE) || match(p, TOK_SEMICOLON)) {}
  }
  expect(p, TOK_RBRACE, "expected '}' to close class body");

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind             = ND_CLASS_DECL;
  n->pos              = pos;
  n->class_decl.name  = name;
  n->class_decl.bases = bases;
  n->class_decl.body  = body;
  return n;
}
```

---

## 验收标准（checklist）

- [ ] `"func f() {}"` → `ND_FUNC_DECL(name="f", params=[], body=BLOCK)`。
- [ ] `"func f(a, b) { return a + b }"` → params=[a, b]。
- [ ] `"func f(a, b=1, *args, **kw) {}"` → 4 个参数，顺序正确。
- [ ] `"func f(b=1, a) {}"` → 语法错误（无默认值参数在默认值参数之后）。
- [ ] `"async func f() {}"` → `ND_ASYNC_FUNC`。
- [ ] `"class Foo {}"` → `ND_CLASS_DECL(name="Foo", bases=NULL, body=NULL)`。
- [ ] `"class Foo extends Bar {}"` → bases=[ND_IDENT("Bar")]。
- [ ] `"class Foo extends Bar, Baz {}"` → 多继承 bases=[Bar, Baz]。
- [ ] class body 中的方法解析为 `ND_FUNC_DECL` 节点。
- [ ] class body 中的赋值解析为 `ND_ASSIGN`/`ND_VAR_DECL`（类属性）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_func_class_decl.c`）

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

static void testFuncDecl(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "func add(a, b) { return a + b }");
  MS_ASSERT_EQ(n->kind, ND_FUNC_DECL, "func decl");
  MS_ASSERT_TRUE(n->func_decl.name != NULL, "named");
  int cnt = 0;
  for (MsNodeList* l = n->func_decl.params; l; l = l->next) cnt++;
  MS_ASSERT_EQ(cnt, 2, "2 params");
  msArenaFree(&a);
}

static void testClassDecl(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "class Point extends Base { func __init__(self) { } }");
  MS_ASSERT_EQ(n->kind, ND_CLASS_DECL, "class");
  MS_ASSERT_TRUE(n->class_decl.bases != NULL, "has base");
  MS_ASSERT_TRUE(n->class_decl.body != NULL, "has body");
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

// vararg / kwarg
func show(*args, **kwargs) {
    print(args)    // tuple
    print(kwargs)  // map
}
show(1, 2, 3, a=4, b=5)

// class
class Animal {
    sound := "..."
    func __init__(self, name) {
        self.name = name
    }
    func speak(self) {
        print($"{self.name} says {self.sound}")
    }
}

class Dog extends Animal {
    sound := "Woof"
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
- **方法 vs 函数**：class body 中的 `func` 声明是方法；parser 不区分（均为 `ND_FUNC_DECL`），编译器（T044）在 `MAKE_CLASS` 时按位置判断。
- **装饰器**：初版不支持 `@decorator` 装饰器语法；预留节点字段，后续版本扩展。
- **`extends` 关键字**：需在 T007 关键字表中确认（`syntax.md §1.4` 已列 `extends` 为关键字 `TOK_EXTENDS`）。
