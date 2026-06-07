# P2-T033 with 语句

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `with expr as name { body }` 语句的解析，产生 `ND_WITH` 节点。`with` 是上下文管理器协议的语法糖，等价于 `__enter__`/`__exit__` 调用模式。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T027 | `parseBlock` |
| P2-T017 | `ND_WITH` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.4.18 with 语句（上下文管理器） |
| `type-system.md` | §5.3 `__enter__`/`__exit__` 协议 |

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
- 多重 with（`with a, b { }`）初版不支持（嵌套 with 代替）。

### 实现

```c
static MsNode* parseWithStmt(MsParser* p) {
  MsSrcPos pos = p->prev.pos;

  MsNode* expr = msParseExpr(p);

  const char* asName = NULL;
  uint32_t    asLen  = 0;
  if (match(p, TOK_AS)) {
    expect(p, TOK_IDENT, "expected name after 'as'");
    asName = p->prev.start;
    asLen  = p->prev.len;
  }

  expect(p, TOK_LBRACE, "expected '{' after with expression");
  MsNode* body = parseBlock(p);

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind             = ND_WITH;
  n->pos              = pos;
  n->with_stmt.expr   = expr;
  n->with_stmt.as_name = asName;
  n->with_stmt.body   = body;
  return n;
}
```

---

## 验收标准（checklist）

- [ ] `"with open(\"f\") as f { }"` → `ND_WITH(expr=CALL, as_name="f", body=BLOCK)`。
- [ ] `"with ctx { }"` 无 `as` → `ND_WITH(as_name=NULL)`。
- [ ] `"with a { with b as x { } }"` → 嵌套 with 合法。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_with_stmt.c`）

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

static void testWithAs(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "with ctx as c { }");
  MS_ASSERT_EQ(n->kind, ND_WITH, "with");
  MS_ASSERT_TRUE(n->with_stmt.as_name != NULL, "has as");
  msArenaFree(&a);
}

static void testWithNoAs(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "with ctx { }");
  MS_ASSERT_EQ(n->kind, ND_WITH, "with");
  MS_ASSERT_TRUE(n->with_stmt.as_name == NULL, "no as");
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
