# P2-T035 import 语句

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现模块导入语句的解析：
- `import foo.bar`
- `import foo.bar as baz`

产生 `MS_ND_IMPORT` 节点，供编译器（T087）生成模块加载字节码。`from…import`、`import *` 不在本任务范围。相对导入（前缀点，如 `.utils`、`..common.util`）已在 `syntax.md §2.1` 与 `modules.md §1` 定义，本任务暂不实现，留待后续任务扩展。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P2-T018 | `msParseStmt` 骨架 |
| P2-T017 | `MS_ND_IMPORT` 节点 |

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
// 解析 "a.b.c" 为 ["a", "b", "c"] 的 MS_ND_IDENT 列表
static MsNodeList* parseDottedName(MsParser* p) {
  MsNodeList* parts = NULL;
  MsNodeList** tail = &parts;

  msParserExpect(p, MS_TOK_IDENT, "expected module name");
  MsNode* part = MS_ARENA_NEW(p->arena, MsNode);
  part->kind       = MS_ND_IDENT;
  part->pos        = p->prev.pos;
  part->ident.name = p->prev.start;
  part->ident.len  = p->prev.len;
  msNodeListAppend(p, &tail, part);

  while (msParserMatch(p, MS_TOK_DOT)) {
    msParserExpect(p, MS_TOK_IDENT, "expected module name after '.'");
    MsNode* seg = MS_ARENA_NEW(p->arena, MsNode);
    seg->kind       = MS_ND_IDENT;
    seg->pos        = p->prev.pos;
    seg->ident.name = p->prev.start;
    seg->ident.len  = p->prev.len;
    msNodeListAppend(p, &tail, seg);
  }
  return parts;
}
```

### 2. `import` 语句

```c
// msParseStmt 中，MS_TOK_IMPORT 分支：
static MsNode* parseImportStmt(MsParser* p) {
  struct MsSrcPos pos = p->prev.pos;

  MsNodeList* path = parseDottedName(p);

  const char* asName = NULL;
  if (msParserMatch(p, MS_TOK_AS)) {
    msParserExpect(p, MS_TOK_IDENT, "expected alias name after 'as'");
    asName = p->prev.start;
  }

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind               = MS_ND_IMPORT;
  n->pos                = pos;
  n->importStmt.path    = path;
  n->importStmt.asName  = asName;
  // fromNames / fromImport 为后续 from-import 任务预留，本任务恒置默认值
  n->importStmt.fromNames  = NULL;
  n->importStmt.fromImport = false;
  return n;
}
```

> **语句终止符：** `syntax.md §2.1` 文法中 `ImportDecl` 以 `';'` 结尾。实际项目中语句终止符（换行/分号）由 `msParseStmt` 在调用各语句解析函数返回后统一消费，`parseImportStmt` 无需自行处理。

---

## 验收标准（checklist）

- [ ] `"import foo"` → `MS_ND_IMPORT(path=[foo], fromImport=false)`。
- [ ] `"import foo.bar.baz"` → `path` 链表长度 == 3，首节点 `ident.name` == `"foo"`。
- [ ] `"import foo as f"` → `importStmt.asName` == `"f"`。
- [ ] `"import foo.bar as fb"` → `path` 长度 == 2，`asName` == `"fb"`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/parser/test_import.c`）

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

static void testImportBasic(void) {
  struct MsArena a;
  msArenaInit(&a);
  MsNode* n = pStmt(&a, "import math");
  MS_ASSERT_EQ(n->kind, MS_ND_IMPORT, "import");
  MS_ASSERT_TRUE(!n->importStmt.fromImport, "not from-import");
  MS_ASSERT_TRUE(n->importStmt.path != NULL, "has path");
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

- **点号路径解析**：`a.b.c` 产生三个 `MS_ND_IDENT` 节点组成的列表，编译器（T087）负责拼接为完整模块路径字符串后查找模块缓存。
- **相对导入**：`syntax.md §2.1` 与 `modules.md §1` 已定义前缀点语法（`.utils`、`..common.util`），本任务不实现，后续任务需扩展 `parseDottedName` 增加前导点计数逻辑。
