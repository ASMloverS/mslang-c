# P2-T017 AST 节点定义与内存管理

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

定义 mslang 语法树（AST）的全部节点类型，包括节点枚举、节点 `union` 结构体、位置信息嵌入，以及基于 arena 分配器的 AST 内存管理（避免逐节点 malloc/free，parser 整体释放）。这是 P2 所有 parser 任务的基础。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P1-T006 | `MsSrcPos` 位置类型 |
| P0-T002 | `msAlloc`/`MS_ALLOC` 内存分配工具 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2 全部语法规则（表达式/语句/声明） |
| `syntax.md` | §2.3 表达式节点分类 |
| `syntax.md` | §2.4–2.8 各类语句 |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
include/mslang/ms_ast.h      # 全部 AST 节点类型定义（公共头）
src/parser/ms_arena.h        # Arena 分配器（仅内部使用）
src/parser/ms_arena.c        # Arena 实现
```

---

## 实现要点

### 1. Arena 分配器

AST 节点生命周期与整次解析绑定，使用 arena（线性分配）避免碎片：

```c
// src/parser/ms_arena.h
typedef struct MsArenaBlock MsArenaBlock;

struct MsArena {
  MsArenaBlock* head;    // 当前块
  uint8_t*      ptr;     // 当前分配游标
  uint8_t*      end;     // 当前块末尾
};

void  msArenaInit(struct MsArena* a);
void* msArenaAlloc(struct MsArena* a, size_t size, size_t align);
void  msArenaFree(struct MsArena* a);  // 释放全部块
```

- 每个 block 默认 64KB（`MS_ARENA_BLOCK_SIZE 65536`）。
- 分配对齐：8 字节（`_Alignof(max_align_t)` 或 8）。
- 超出 block 时链式追加新 block（大分配 > block/2 时单独分配同等大小 block）。

```c
// 便捷宏
#define MS_ARENA_NEW(arena, T) ((T*)msArenaAlloc((arena), sizeof(T), _Alignof(T)))
#define MS_ARENA_NEWN(arena, T, n) ((T*)msArenaAlloc((arena), sizeof(T)*(n), _Alignof(T)))
```

### 2. 节点种类枚举（`MsNodeKind`）

```c
// include/mslang/ms_ast.h
typedef enum MsNodeKind {
  // ─── 字面量 ───────────────────────────────────────────────────
  ND_INT,          // 整数字面量
  ND_FLOAT,        // 浮点字面量
  ND_STRING,       // 字符串字面量（已解码）
  ND_BYTES,        // bytes 字面量
  ND_BOOL,         // true/false
  ND_NIL,          // nil
  ND_FSTRING,      // f-string 拼接（含子节点列表）

  // ─── 变量/名称 ─────────────────────────────────────────────────
  ND_IDENT,        // 标识符引用

  // ─── 表达式 ───────────────────────────────────────────────────
  ND_UNARY,        // 一元运算（-x, not x, ~x）
  ND_BINARY,       // 二元运算（x + y 等）
  ND_TERNARY,      // 三目（cond ? a : b 等，mslang 无三目；保留或替换为 if-expr）
  ND_IF_EXPR,      // if 表达式（x if cond else y）—— 如语法支持
  ND_CALL,         // 函数调用
  ND_ATTR,         // 属性访问（obj.name）
  ND_INDEX,        // 下标访问（obj[key]）
  ND_SLICE,        // 切片（a[lo:hi:step]）
  ND_STAR_EXPR,    // *expr（展开，函数调用 / 赋值解包）
  ND_DOUBLESTAR_EXPR, // **expr（dict 展开）

  // ─── 容器字面量 ────────────────────────────────────────────────
  ND_LIST,         // [a, b, c]
  ND_MAP,          // {k: v, …}
  ND_SET,          // {a, b, c}（消歧于 map）
  ND_TUPLE,        // (a, b, c) 或 a, b, c
  ND_LISTCOMP,     // [expr for …]（可选，后续任务）

  // ─── 并发表达式 ────────────────────────────────────────────────
  ND_MAKE,         // make(chan T, n) / make([]T, n)
  ND_RECV,         // <-ch（接收）
  ND_SEND,         // ch <- val（发送语句）

  // ─── 赋值 / 声明 ───────────────────────────────────────────────
  ND_VAR_DECL,     // var x = expr / var x T
  ND_ASSIGN,       // x = expr（普通赋值）
  ND_SHORT_DECL,   // x := expr（短声明）
  ND_COMPOUND_ASSIGN, // x += expr 等
  ND_INC_DEC,      // x++ / x--

  // ─── 语句 ─────────────────────────────────────────────────────
  ND_EXPR_STMT,    // 裸表达式语句
  ND_BLOCK,        // { stmt… }
  ND_IF,           // if / else if / else
  ND_FOR,          // for 三形式
  ND_SWITCH,       // switch/case/default
  ND_RETURN,       // return [expr]
  ND_BREAK,        // break [label]
  ND_CONTINUE,     // continue [label]
  ND_PASS,         // pass
  ND_DEL,          // del expr
  ND_ASSERT,       // assert expr [, msg]
  ND_RAISE,        // raise [expr] [from expr]
  ND_TRY,          // try/catch/finally
  ND_GO,           // go func(args)
  ND_SELECT,       // select { case … }
  ND_WITH,         // with expr as name { … }
  ND_FALLTHROUGH,  // fallthrough

  // ─── 声明 ─────────────────────────────────────────────────────
  ND_FUNC_DECL,    // func 函数声明（含方法）
  ND_CLASS_DECL,   // class 声明
  ND_IMPORT,       // import 语句
  ND_ASYNC_FUNC,   // async func
  ND_AWAIT,        // await expr

  // ─── 顶层 ─────────────────────────────────────────────────────
  ND_PROGRAM,      // 整个源文件 AST 根节点

  ND_COUNT,        // sentinel（枚举数量）
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
  MsSrcPos   pos;   // 节点起始位置

  union {
    // ND_INT
    struct { int64_t  ival; } lit_int;
    // ND_FLOAT
    struct { double   fval; } lit_float;
    // ND_STRING / ND_BYTES（已解码内容存 arena 中）
    struct { const char* data; uint32_t len; } lit_str;
    // ND_BOOL
    struct { bool bval; } lit_bool;
    // ND_FSTRING（parts 为 ND_STRING / 表达式混合的 MsNodeList）
    struct { MsNodeList* parts; } fstring;
    // ND_IDENT
    struct { const char* name; uint32_t len; } ident;
    // ND_UNARY
    struct { MsTokKind op; MsNode* operand; } unary;
    // ND_BINARY / ND_COMPOUND_ASSIGN
    struct { MsTokKind op; MsNode* left; MsNode* right; } binary;
    // ND_CALL
    struct {
      MsNode*     callee;
      MsNodeList* args;     // 位置参数（含 *expr）
      MsNodeList* kwargs;   // 关键字参数（ND_ASSIGN 形式）
    } call;
    // ND_ATTR
    struct { MsNode* obj; const char* name; uint32_t nameLen; } attr;
    // ND_INDEX
    struct { MsNode* obj; MsNode* key; } index;
    // ND_SLICE
    struct { MsNode* obj; MsNode* lo; MsNode* hi; MsNode* step; } slice;
    // ND_LIST / ND_SET / ND_TUPLE
    struct { MsNodeList* elems; } container;
    // ND_MAP（elems 为 ND_BINARY(TOK_COLON) 节点列表）
    struct { MsNodeList* pairs; } map;
    // ND_MAKE
    struct { MsNode* typeExpr; MsNode* sizeExpr; } make_expr;
    // ND_RECV
    struct { MsNode* chan_expr; } recv;
    // ND_SEND
    struct { MsNode* chan_expr; MsNode* val; } send;
    // ND_VAR_DECL / ND_SHORT_DECL
    struct { const char* name; uint32_t nameLen; MsNode* init; } var_decl;
    // ND_ASSIGN
    struct { MsNode* target; MsNode* value; } assign;
    // ND_INC_DEC
    struct { MsNode* target; bool isInc; } inc_dec;
    // ND_EXPR_STMT
    struct { MsNode* expr; } expr_stmt;
    // ND_BLOCK
    struct { MsNodeList* stmts; } block;
    // ND_IF
    struct {
      MsNode* cond;
      MsNode* then_block;
      MsNode* else_block;  // NULL / ND_IF（else if）/ ND_BLOCK（else）
    } if_stmt;
    // ND_FOR（三形式通过字段组合区分）
    struct {
      MsNode* init;       // NULL→range for；ND_ASSIGN→c-style
      MsNode* cond;
      MsNode* post;
      MsNode* body;
      // for-in: target 存 init，iter 存 cond，init=NULL 时 post 存 iter
      MsNode* for_target; // for x in …
      MsNode* for_iter;
    } for_stmt;
    // ND_SWITCH
    struct {
      MsNode*     expr;
      MsNodeList* cases;  // 每个 case 是 ND_SWITCH_CASE（扩展节点）
    } switch_stmt;
    // ND_RETURN / ND_RAISE / ND_ASSERT / ND_DEL
    struct { MsNode* expr; MsNode* expr2; } single_expr;
    // ND_BREAK / ND_CONTINUE
    struct { const char* label; } jump;
    // ND_TRY
    struct {
      MsNode*     body;
      MsNodeList* handlers;  // ND_CATCH_CLAUSE
      MsNode*     finally_block;
    } try_stmt;
    // ND_GO
    struct { MsNode* call; } go_stmt;
    // ND_SELECT
    struct { MsNodeList* cases; } select_stmt;
    // ND_WITH
    struct { MsNode* expr; const char* as_name; MsNode* body; } with_stmt;
    // ND_FUNC_DECL / ND_ASYNC_FUNC
    struct {
      const char* name;
      MsNodeList* params;   // ND_PARAM
      MsNode*     body;
      bool        is_async;
    } func_decl;
    // ND_CLASS_DECL
    struct {
      const char* name;
      MsNodeList* bases;
      MsNodeList* body;
    } class_decl;
    // ND_IMPORT
    struct {
      MsNodeList* path;      // dotted name parts（ND_IDENT）
      const char* as_name;   // import foo as bar → "bar"
      MsNodeList* from_names; // from foo import a, b → [a, b]
      bool        from_import;
    } import_stmt;
    // ND_AWAIT
    struct { MsNode* expr; } await_expr;
    // ND_PROGRAM
    struct { MsNodeList* stmts; const char* filename; } program;
  };
};
```

### 4. 辅助节点类型（扩展）

以下节点类型不在主枚举中，但由 parser 内部使用（以 `MsNodeKind` 值位于 `ND_COUNT` 之后）：

```c
// 函数参数节点（ND_PARAM）—— 追加到枚举末尾
ND_PARAM,        // 参数定义（name, default, is_vararg, is_kwarg）
ND_CATCH_CLAUSE, // catch 子句（type 过滤, name, body）
ND_SWITCH_CASE,  // switch case（values 列表, body）
ND_KWARG_PAIR,   // 关键字参数对（name=expr）
```

---

## 验收标准（checklist）

- [ ] `ms_ast.h` 编译无警告（`-Wall -Wextra -Wpedantic`）。
- [ ] `msArenaAlloc` 返回对齐到 8 字节的指针。
- [ ] `msArenaFree` 后内存无泄漏（AddressSanitizer 验证）。
- [ ] `MS_ARENA_NEW(arena, MsNode)` 宏正确实例化节点（C 单测验证）。
- [ ] 所有节点 union 字段通过 `static_assert(sizeof(MsNode) <= 128, "...")` 检查（避免节点过胖；如超出，拆分或改用指针）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_arena.c`）

```c
#include "ms_test.h"
#include "ms_arena.h"

static void testArenaBasic(void) {
  struct MsArena a;
  msArenaInit(&a);

  int* p1 = MS_ARENA_NEW(&a, int);
  *p1 = 42;
  int* p2 = MS_ARENA_NEW(&a, int);
  *p2 = 99;

  MS_ASSERT_EQ(*p1, 42, "p1 intact");
  MS_ASSERT_EQ(*p2, 99, "p2 intact");
  // 验证对齐
  MS_ASSERT_EQ(((uintptr_t)p1) % 8, 0, "aligned p1");
  MS_ASSERT_EQ(((uintptr_t)p2) % 8, 0, "aligned p2");

  msArenaFree(&a);
}

static void testArenaLargeAlloc(void) {
  struct MsArena a;
  msArenaInit(&a);
  // 超过 block 大小的分配
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

- **`sizeof(MsNode)` 控制**：union 中最大分支决定节点大小；若超过 96–128 字节，应将大字段改为指针（指向另一个 arena 分配的结构体），保持 `MsNode` 紧凑。
- **switch_case / catch_clause 扩展节点**：这些节点在 `MsNode.kind` 枚举超出 `ND_COUNT` 范围，仅内部使用；外部 visitor 需处理 `default:` 分支。
- **节点位置精度**：`pos` 记录节点起始位置（token 开始位置）；对于多 token 节点（如 `if`），`pos` 取关键字位置（`if` 的位置），不记录结束位置（初版不需要 span）。
