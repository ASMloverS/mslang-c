# P2-T035 import 语句

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现模块导入语句的解析：
- `import foo.bar`
- `import foo.bar as baz`

产生 `ND_IMPORT` 节点，供编译器（T087）生成模块加载字节码。`from…import`、`import *`、相对导入均不在 syntax.md 定义范围内，本任务不实现。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | `msParseStmt` 骨架 |
| P2-T017 | `ND_IMPORT` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `syntax.md` | §2.8 import 语句 |
| `modules.md` | §1 导入解析（点号路径） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c   # parseImportStmt
```

---

## 实现要点

### 1. 点号路径（`DottedName`）

```c
// 解析 "a.b.c" 为 ["a", "b", "c"] 的 ND_IDENT 列表
static MsNodeList* parseDottedName(MsParser* p) {
  MsNodeList* parts = NULL;
  MsNodeList** tail = &parts;
  do {
    expect(p, TOK_IDENT, "expected module name");
    MsNode* part = MS_ARENA_NEW(p->arena, MsNode);
    part->kind       = ND_IDENT;
    part->pos        = p->prev.pos;
    part->ident.name = p->prev.start;
    part->ident.len  = p->prev.len;
    MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
    item->node = part; item->next = NULL;
    *tail = item; tail = &item->next;
  } while (check(p, TOK_DOT) && (advance(p), true));
  return parts;
}
```

### 2. `import` 语句

```c
// msParseStmt 中，match(TOK_IMPORT) 分支：
static MsNode* parseImportStmt(MsParser* p) {
  MsSrcPos pos = p->prev.pos;

  MsNodeList* path = parseDottedName(p);

  const char* asName = NULL;
  uint32_t    asLen  = 0;
  if (match(p, TOK_AS)) {
    expect(p, TOK_IDENT, "expected alias name after 'as'");
    asName = p->prev.start;
    asLen  = p->prev.len;
  }

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind                    = ND_IMPORT;
  n->pos                     = pos;
  n->import_stmt.path        = path;
  n->import_stmt.as_name     = asName;
  n->import_stmt.from_names  = NULL;
  n->import_stmt.from_import = false;
  return n;
}
```

---

## 验收标准（checklist）

- [ ] `"import foo"` → `ND_IMPORT(path=[foo], from_import=false)`。
- [ ] `"import foo.bar.baz"` → `path=[foo, bar, baz]`。
- [ ] `"import foo as f"` → `as_name="f"`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_import.c`）

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

static void testImportBasic(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "import math");
  MS_ASSERT_EQ(n->kind, ND_IMPORT, "import");
  MS_ASSERT_TRUE(!n->import_stmt.from_import, "not from-import");
  MS_ASSERT_TRUE(n->import_stmt.path != NULL, "has path");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testImportBasic);
  return msTestSummary();
}
```

### .ms 使用示例（T086/T087 后验证）

```ms
import math
print(math.pi)          // 3.141592653589793

import math as m
print(m.sqrt(16))       // 4.0

import collections.deque
q := collections.deque.Deque()
q.append(1)
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **点号路径解析**：`a.b.c` 产生三个 `ND_IDENT` 节点组成的列表，编译器（T087）负责拼接为完整模块路径字符串后查找模块缓存。
