# P2-T017 AST 节点定义与内存管理

> **状态**：✅ 已完成

---

## 任务目标 / 背景

定义 mslang 语法树（AST）的全部节点类型，包括节点枚举、节点 `union` 结构体、位置信息嵌入，以及基于 arena 分配器的 AST 内存管理（避免逐节点 malloc/free，parser 整体释放）。这是 P2 所有 parser 任务的基础。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T006 | `MsSrcPos` 位置类型 / `MsTokKind` token 枚举（`ms_ast.h` 须 `#include "mslang/ms_lexer.h"`） |
| P0-T002 | `msAlloc`/`MS_ALLOC` 内存分配工具 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2 全部语法规则（表达式/语句/声明） |
| `syntax.md` | §2.3 表达式节点分类（含三目 if-expr） |
| `syntax.md` | §2.4–2.8 各类语句 |
| `errors.md` | 异常语义（raise/catch/assert，errors.md §7） |
| `concurrency.md` | 并发语义（chan/select/go，make 表达式） |
| `type-system.md` | §3.1 self 约定与单继承规则 |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
include/mslang/ms_ast.h      # 全部 AST 节点类型定义（公共头）
src/parser/ms_arena.h        # Arena 分配器（仅内部使用）
src/parser/ms_arena.c        # Arena 实现
```

> **注意：**
> - `ms_ast.h` 和 `ms_arena.h` 顶部使用 `#pragma once`（禁止 `#ifndef` guard，见 c-style.md §2.2）。
> - `ms_ast.h` 须在首行 `#include "mslang/ms_lexer.h"` 以使用 `MsTokKind`（c-style.md §2.3 头文件自包含）。

---

## 实现要点

### 1. Arena 分配器

AST 节点生命周期与整次解析绑定，使用 arena（线性分配）避免碎片：

```c
// src/parser/ms_arena.h
typedef struct MsArenaBlock MsArenaBlock;

struct MsArena {
  MsArenaBlock* head;    // current block
  uint8_t*      ptr;     // allocation cursor
  uint8_t*      end;     // end of current block
};

void  msArenaInit(struct MsArena* a);
void* msArenaAlloc(struct MsArena* a, size_t size, size_t align);
void  msArenaFree(struct MsArena* a);  // free all blocks
```

- 每个 block 默认 64KB（`MS_ARENA_BLOCK_SIZE 65536`）。
- 分配对齐：8 字节（`_Alignof(max_align_t)` 或 8）。
- 超出 block 时链式追加新 block（大分配 > block/2 时单独分配同等大小 block）。

```c
// convenience macros
#define MS_ARENA_NEW(arena, T) ((T*)msArenaAlloc((arena), sizeof(T), _Alignof(T)))
#define MS_ARENA_NEWN(arena, T, n) ((T*)msArenaAlloc((arena), sizeof(T)*(n), _Alignof(T)))
```

### 2. 节点种类枚举（`MsNodeKind`）

```c
// include/mslang/ms_ast.h
typedef enum MsNodeKind {
  // ─── 字面量 ───────────────────────────────────────────────────
  MS_ND_INT,          // integer literal
  MS_ND_FLOAT,        // float literal
  MS_ND_STRING,       // string literal (decoded)
  MS_ND_BYTES,        // bytes literal
  MS_ND_BOOL,         // true/false
  MS_ND_NIL,          // nil
  MS_ND_FSTRING,      // f-string (list of string/expr parts)

  // ─── 变量/名称 ─────────────────────────────────────────────────
  MS_ND_IDENT,        // identifier reference

  // ─── 表达式 ───────────────────────────────────────────────────
  MS_ND_UNARY,        // unary op (-x, not x, ~x)
  MS_ND_BINARY,       // binary op (x + y, etc.)
  MS_ND_IF_EXPR,      // ternary/if expr: a if cond else b (syntax.md §2.3)
  MS_ND_CALL,         // function call
  MS_ND_ATTR,         // attribute access (obj.name)
  MS_ND_INDEX,        // subscript (obj[key])
  MS_ND_SLICE,        // slice (a[lo:hi:step])
  MS_ND_STAR_EXPR,    // *expr (splat in call / unpack in assign)
  MS_ND_DOUBLESTAR_EXPR, // **expr (dict splat)

  // ─── 容器字面量 ────────────────────────────────────────────────
  MS_ND_LIST,         // [a, b, c]
  MS_ND_MAP,          // {k: v, …}
  MS_ND_SET,          // {a, b, c} (disambiguated from map)
  MS_ND_TUPLE,        // (a, b, c) or a, b, c
  MS_ND_LISTCOMP,     // [expr for …] (optional, later task)

  // ─── 并发表达式 ────────────────────────────────────────────────
  MS_ND_MAKE,         // make(chan) / make(chan, n) (syntax.md §2.3 MakeExpr)
  MS_ND_RECV,         // <-ch (receive)
  MS_ND_SEND,         // ch <- val (send statement)

  // ─── 赋值 / 声明 ───────────────────────────────────────────────
  MS_ND_VAR_DECL,     // var x = expr / var x T
  MS_ND_ASSIGN,       // x = expr
  MS_ND_SHORT_DECL,   // x := expr
  MS_ND_COMPOUND_ASSIGN, // x += expr etc.
  MS_ND_INC_DEC,      // x++ / x-- (postfix expr or stmt, syntax.md §2.2/§2.3)

  // ─── 语句 ─────────────────────────────────────────────────────
  MS_ND_EXPR_STMT,    // bare expression statement
  MS_ND_BLOCK,        // { stmt… }
  MS_ND_IF,           // if / else if / else
  MS_ND_FOR,          // for (3 forms)
  MS_ND_SWITCH,       // switch/case/default
  MS_ND_RETURN,       // return [expr]
  MS_ND_BREAK,        // break [label]
  MS_ND_CONTINUE,     // continue [label]
  MS_ND_PASS,         // pass
  MS_ND_DEL,          // del expr
  MS_ND_ASSERT,       // assert expr [, msg] (errors.md §7)
  MS_ND_RAISE,        // raise [expr] [from expr] (errors.md)
  MS_ND_TRY,          // try/catch/finally (errors.md)
  MS_ND_GO,           // go func(args) (concurrency.md)
  MS_ND_SELECT,       // select { case … } (concurrency.md)
  MS_ND_WITH,         // with expr as name { … }
  MS_ND_FALLTHROUGH,  // fallthrough

  // ─── 声明 ─────────────────────────────────────────────────────
  MS_ND_FUNC_DECL,    // func / async func decl (name=NULL for anonymous FuncLiteral)
  MS_ND_CLASS_DECL,   // class decl (type-system.md §3.1, single inheritance)
  MS_ND_IMPORT,       // import statement
  MS_ND_ASYNC_FUNC,   // async func (sets funcDecl.isAsync=true)
  MS_ND_AWAIT,        // await expr

  // ─── 顶层 ─────────────────────────────────────────────────────
  MS_ND_PROGRAM,      // source file AST root

  MS_ND_COUNT,        // sentinel
} MsNodeKind;
```

### 3. 节点基础结构

```c
// 前置声明
typedef struct MsNode MsNode;

// 节点链表（用于子节点序列：参数、语句列表等）
typedef struct MsNodeList {
  MsNode*           node;
  struct MsNodeList* next;
} MsNodeList;

struct MsNode {
  MsNodeKind kind;
  MsSrcPos   pos;   // start position of this node

  union {
    // MS_ND_INT
    struct { int64_t  ival; } litInt;
    // MS_ND_FLOAT
    struct { double   fval; } litFloat;
    // MS_ND_STRING / MS_ND_BYTES (decoded content stored in arena)
    struct { const char* data; uint32_t len; } litStr;
    // MS_ND_BOOL
    struct { bool bval; } litBool;
    // MS_ND_FSTRING (parts: mixed MS_ND_STRING / expr in MsNodeList)
    struct { MsNodeList* parts; } fstring;
    // MS_ND_IDENT
    struct { const char* name; uint32_t len; } ident;
    // MS_ND_UNARY
    struct { MsTokKind op; MsNode* operand; } unary;
    // MS_ND_BINARY / MS_ND_COMPOUND_ASSIGN
    struct { MsTokKind op; MsNode* left; MsNode* right; } binary;
    // MS_ND_IF_EXPR: a if cond else b (syntax.md §2.3 TernaryExpr)
    struct { MsNode* cond; MsNode* thenExpr; MsNode* elseExpr; } ifExpr;
    // MS_ND_CALL
    struct {
      MsNode*     callee;
      MsNodeList* args;     // positional args (including *expr)
      MsNodeList* kwargs;   // keyword args (MS_ND_KWARG_PAIR list)
    } call;
    // MS_ND_ATTR
    struct { MsNode* obj; const char* name; uint32_t nameLen; } attr;
    // MS_ND_INDEX
    struct { MsNode* obj; MsNode* key; } index;
    // MS_ND_SLICE
    struct { MsNode* obj; MsNode* lo; MsNode* hi; MsNode* step; } slice;
    // MS_ND_LIST / MS_ND_SET / MS_ND_TUPLE
    struct { MsNodeList* elems; } container;
    // MS_ND_STAR_EXPR / MS_ND_DOUBLESTAR_EXPR
    struct { MsNode* expr; } starExpr;
    // MS_ND_LISTCOMP
    struct { MsNode* expr; MsNodeList* comprehensions; } listComp;
    // MS_ND_MAP (pairs: MS_ND_BINARY(TOK_COLON) node list)
    struct { MsNodeList* pairs; } map;
    // MS_ND_MAKE: make(chan) / make(chan, n) (syntax.md §2.3 MakeExpr)
    struct { MsNode* capExpr; } makeExpr;
    // MS_ND_RECV
    struct { MsNode* chanExpr; } recv;
    // MS_ND_SEND
    struct { MsNode* chanExpr; MsNode* val; } send;
    // MS_ND_VAR_DECL / MS_ND_SHORT_DECL
    struct { const char* name; uint32_t nameLen; MsNode* init; } varDecl;
    // MS_ND_ASSIGN
    struct { MsNode* target; MsNode* value; } assign;
    // MS_ND_INC_DEC: postfix expr or inc/dec stmt (syntax.md §2.2/§2.3)
    struct { MsNode* target; bool isInc; } incDec;
    // MS_ND_EXPR_STMT
    struct { MsNode* expr; } exprStmt;
    // MS_ND_BLOCK
    struct { MsNodeList* stmts; } block;
    // MS_ND_IF
    struct {
      MsNode* cond;
      MsNode* thenBlock;
      MsNode* elseBlock;  // NULL / MS_ND_IF (else if) / MS_ND_BLOCK (else)
    } ifStmt;
    // MS_ND_FOR (3 forms distinguished by field combination)
    struct {
      MsNode* init;       // NULL=range for; MS_ND_ASSIGN=c-style
      MsNode* cond;
      MsNode* post;
      MsNode* body;
      MsNode* forTarget;  // for x in …
      MsNode* forIter;
    } forStmt;
    // MS_ND_SWITCH
    struct {
      MsNode*     expr;
      MsNodeList* cases;  // each case is MS_ND_SWITCH_CASE
    } switchStmt;
    // MS_ND_RETURN / MS_ND_RAISE / MS_ND_ASSERT / MS_ND_DEL
    struct { MsNode* expr; MsNode* expr2; } singleExpr;
    // MS_ND_BREAK / MS_ND_CONTINUE
    struct { const char* label; } jump;
    // MS_ND_TRY
    struct {
      MsNode*     body;
      MsNodeList* handlers;      // MS_ND_CATCH_CLAUSE list (errors.md)
      MsNode*     finallyBlock;
    } tryStmt;
    // MS_ND_GO (concurrency.md)
    struct { MsNode* call; } goStmt;
    // MS_ND_SELECT (concurrency.md)
    struct { MsNodeList* cases; } selectStmt;
    // MS_ND_WITH
    struct { MsNode* expr; const char* asName; MsNode* body; } withStmt;
    // MS_ND_FUNC_DECL / MS_ND_ASYNC_FUNC (name=NULL for anonymous FuncLiteral)
    struct {
      const char* name;
      MsNodeList* params;   // MS_ND_PARAM list
      MsNode*     body;
      bool        isAsync;
    } funcDecl;
    // MS_ND_CLASS_DECL (type-system.md §3.1, single inheritance)
    struct {
      const char* name;
      MsNode*     base;         // extends clause (NULL if no base)
      MsNodeList* body;
    } classDecl;
    // MS_ND_IMPORT
    struct {
      MsNodeList* path;         // dotted name parts (MS_ND_IDENT)
      const char* asName;       // import foo as bar → "bar"
      MsNodeList* fromNames;    // from foo import a, b → [a, b]
      bool        fromImport;
    } importStmt;
    // MS_ND_AWAIT
    struct { MsNode* expr; } awaitExpr;
    // MS_ND_PROGRAM
    struct { MsNodeList* stmts; const char* filename; } program;
    // MS_ND_PARAM (auxiliary, value >= MS_ND_COUNT)
    struct { const char* name; uint32_t nameLen; MsNode* defaultVal; bool isVararg; bool isKwarg; } param;
    // MS_ND_CATCH_CLAUSE (auxiliary, errors.md)
    struct { MsNodeList* typeFilter; const char* asName; MsNode* body; } catchClause;
    // MS_ND_SWITCH_CASE (auxiliary)
    struct { MsNodeList* values; MsNode* body; bool isDefault; } switchCase;
    // MS_ND_KWARG_PAIR (auxiliary)
    struct { const char* name; MsNode* value; } kwargPair;
  };
};
```

### 4. 辅助节点类型（扩展）

以下节点类型值位于 `MS_ND_COUNT` 之后，与主枚举定义在同一 `MsNodeKind` 中，仅供 parser 内部使用；外部 visitor 须处理 `default:` 分支。union 分支已在 §3 `MsNode` 结构体中定义。

```c
// 追加到主枚举 MS_ND_COUNT 之后
MS_ND_PARAM = MS_ND_COUNT,  // param def (name, defaultVal, isVararg, isKwarg)
MS_ND_CATCH_CLAUSE,         // catch clause (typeFilter, asName, body) (errors.md)
MS_ND_SWITCH_CASE,          // switch case (values, body, isDefault)
MS_ND_KWARG_PAIR,           // keyword arg pair (name=value)
```

---

## 验收标准（checklist）

- [x] `ms_ast.h` 编译无警告（`-Wall -Wextra -Wpedantic`）。
- [x] `msArenaAlloc` 返回对齐到 8 字节的指针。
- [x] `msArenaFree` 后内存无泄漏（AddressSanitizer 验证）。
- [x] `MS_ARENA_NEW(arena, MsNode)` 宏正确实例化节点（C 单测验证）。
- [x] 所有节点 union 字段通过 `static_assert(sizeof(MsNode) <= 128, "MsNode too large")` 检查（避免节点过胖；如超出，拆分或改用指针）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_arena.c`）

```c
#include "ms_test.h"
#include "parser/ms_arena.h"

static void testArenaBasic(void) {
  struct MsArena a;
  msArenaInit(&a);

  int* p1 = MS_ARENA_NEW(&a, int);
  *p1 = 42;
  int* p2 = MS_ARENA_NEW(&a, int);
  *p2 = 99;

  MS_ASSERT_EQ(*p1, 42, "p1 intact");
  MS_ASSERT_EQ(*p2, 99, "p2 intact");
  // verify alignment
  MS_ASSERT_EQ(((uintptr_t)p1) % 8, 0, "aligned p1");
  MS_ASSERT_EQ(((uintptr_t)p2) % 8, 0, "aligned p2");

  msArenaFree(&a);
}

static void testArenaLargeAlloc(void) {
  struct MsArena a;
  msArenaInit(&a);
  // allocation exceeding block size
  char* big = MS_ARENA_NEWN(&a, char, 70000);
  MS_ASSERT_TRUE(big != NULL, "large alloc non-null");
  big[69999] = 'X';
  MS_ASSERT_EQ(big[69999], 'X', "large alloc writable");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testArenaBasic);
  MS_RUN(testArenaLargeAlloc);
  return msTestSummary();
}
```

---

## .ms 使用示例

N/A（AST 定义为 C 内部结构，用 `mslang parse` 子命令（T036）验证）。

---

## Benchmark

N/A（arena 性能归入 T036 整体 parse bench）。

---

## 风险与边界

- **`sizeof(MsNode)` 控制**：union 中最大分支决定节点大小；若超过 128 字节，应将大字段改为指针（指向另一个 arena 分配的结构体），保持 `MsNode` 紧凑。
- **switch_case / catch_clause 扩展节点**：这些节点在 `MsNode.kind` 枚举超出 `MS_ND_COUNT` 范围，仅内部使用；外部 visitor 需处理 `default:` 分支。
- **节点位置精度**：`pos` 记录节点起始位置（token 开始位置）；对于多 token 节点（如 `if`），`pos` 取关键字位置（`if` 的位置），不记录结束位置（初版不需要 span）。
