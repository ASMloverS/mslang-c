# P2-T033 with 语句

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `with expr as name { body }` 语句的解析，产生 `MS_ND_WITH` 节点。`with` 是上下文管理器协议的语法糖，等价于 `__enter__`/`__exit__` 调用模式。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T027 | `msParseBlock` |
| P2-T017 | `MS_ND_WITH` 节点（`ms_ast.h`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.2 语句文法（WithStmt） |
| `type-system.md` | §3.4 魔法方法表（`__enter__`/`__exit__`） |
| `ms_ast.h` | `MS_ND_WITH`、`withStmt{expr, asName, body}` |
| `ms_lexer.h` | `MS_TOK_WITH`、`MS_TOK_AS` |
| `ms_parser.h` | `msParserMatch`/`msParserExpect`/`msParseBlock`/`msParseExpr` API |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c   # parseWithStmt（在 msParseStmt 分支中）
```

---

## 实现要点

### 语法

```
with expr [as name] {
    body
}
```

- `as name` 可选。
- 多重 with（`with a, b { }`）初版不支持（`syntax.md §2.2 WithStmt` 文法仅含单个 `Expr`，嵌套 with 代替）。

### 实现

`msParseStmt` 中 `if (msParserMatch(p, MS_TOK_WITH))` 分支调用 `parseWithStmt`，进入时 `with` 已消费：

```c
static MsNode* parseWithStmt(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;

  MsNode* expr = msParseExpr(p);

  // asName 为 source slice（与 catchClause.asName 约定一致），非 NUL 结尾
  const char* asName = NULL;
  if (msParserMatch(p, MS_TOK_AS)) {
    msParserExpect(p, MS_TOK_IDENT, "expected name after 'as'");
    asName = p->prev.start;
  }

  msParserExpect(p, MS_TOK_LBRACE, "expected '{' after with expression");
  MsNode* body = msParseBlock(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind           = MS_ND_WITH;
  n->pos            = pos;
  n->withStmt.expr  = expr;
  n->withStmt.asName = asName;
  n->withStmt.body  = body;
  return n;
}
```

---

## 验收标准（checklist）

- [ ] `"with open(\"f\") as f { }"` → `MS_ND_WITH(expr=MS_ND_CALL, asName="f", body=MS_ND_BLOCK)`。
- [ ] `"with ctx { }"` 无 `as` → `MS_ND_WITH(asName=NULL)`。
- [ ] `"with a { with b as x { } }"` → 嵌套 with 合法。
- [ ] `"with ctx as { }"` → `as` 后非标识符，parse error。
- [ ] `"with ctx as f pass"` → 缺少 `{`，parse error。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_with_stmt.c`）

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

static void testWithAs(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "with ctx as c { }");
  MS_ASSERT_EQ(n->kind, MS_ND_WITH, "with");
  MS_ASSERT_TRUE(n->withStmt.asName != NULL, "has as");
  msArenaFree(&a);
}

static void testWithNoAs(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "with ctx { }");
  MS_ASSERT_EQ(n->kind, MS_ND_WITH, "with");
  MS_ASSERT_TRUE(n->withStmt.asName == NULL, "no as");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testWithAs);
  MS_RUN(testWithNoAs);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// with 文件操作（需 io 模块）
with open("data.txt", "r") as f {
    content := f.read()
    print(content)
}  // f.__exit__ 自动调用


// 自定义上下文管理器
class Timer {
    func __enter__(self) {
        self.start = time.now()
        return self
    }
    func __exit__(self, exc) {
        elapsed := time.now() - self.start
        print($"elapsed: {elapsed:.3f}s")
        return false  // 不抑制异常
    }
}

with Timer() as t {
    // 耗时操作
    for i in range(1000000) { pass }
}
// elapsed: 0.042s（示例数值）
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`with` 异常抑制**：`__exit__` 返回 `true` 时抑制异常（类 Python 语义）；编译器（T047）处理。
- **多重 `with`**：Python 支持 `with a, b:`；mslang 初版不支持（用嵌套 `with` 代替）。
