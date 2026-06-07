# P2-T035 import / from … import 语句

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现模块导入语句的解析：
- `import foo.bar`
- `import foo.bar as baz`
- `from foo.bar import name1, name2`
- `from foo.bar import *`
- `from ..pkg import mod`（相对导入）

产生 `ND_IMPORT` 节点，供编译器（T087）生成模块加载字节码。

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
| `module.md` | §1 导入解析（绝对/相对/点号路径） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/parser/ms_parser.c   # parseImportStmt / parseFromImportStmt
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

### 2. 相对导入前缀

```c
// 解析相对导入前导点（.. 或 . 序列）
// 返回相对层级（0=绝对, 1=.pkg, 2=..pkg 等）
static int parseRelativeLevel(MsParser* p) {
  int level = 0;
  // TOK_DOT 和 TOK_DOTDOTDOT 的混合
  while (check(p, TOK_DOT) || check(p, TOK_DOTDOTDOT)) {
    if (match(p, TOK_DOTDOTDOT)) {
      level += 3;  // '...' 不是相对导入，但 '..' 是
      // 注意：mslang 中相对导入前导可能用 '..' 表示上一层
      // 实际词法：'..' 是 TOK_ERROR，相对路径通过多个 '.' 表示
      // 初版简化：每个 TOK_DOT 为一层
    } else if (match(p, TOK_DOT)) {
      level++;
    }
  }
  return level;
}
```

**注**：相对导入语法细节以 `syntax.md §2.8` 为准；若 `..` 在词法层不合法（T013），则相对导入仅支持单级 `.`（`from . import foo`）。

### 3. `import` 语句

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

### 4. `from … import` 语句

```c
// msParseStmt 中，match(TOK_FROM) 分支：
static MsNode* parseFromImportStmt(MsParser* p) {
  MsSrcPos pos = p->prev.pos;

  // 相对级别（. 或 ..）
  // 初版：相对导入直接从 parseDottedName 开始（'.' 位于标识符前）
  MsNodeList* path = parseDottedName(p);

  expect(p, TOK_IMPORT, "expected 'import' after module path");

  MsNodeList* fromNames = NULL;
  MsNodeList** fTail    = &fromNames;

  if (match(p, TOK_STAR)) {
    // from foo import *
    MsNode* star = MS_ARENA_NEW(p->arena, MsNode);
    star->kind = ND_STAR_EXPR;
    MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
    item->node = star; item->next = NULL;
    fromNames = item;
  } else {
    // from foo import a [as x], b [as y], …
    do {
      expect(p, TOK_IDENT, "expected name to import");
      MsNode* nameNode = MS_ARENA_NEW(p->arena, MsNode);
      nameNode->kind       = ND_IDENT;
      nameNode->pos        = p->prev.pos;
      nameNode->ident.name = p->prev.start;
      nameNode->ident.len  = p->prev.len;

      if (match(p, TOK_AS)) {
        expect(p, TOK_IDENT, "expected alias after 'as'");
        // alias 存在：将 alias 附到 nameNode 的某字段
        // 简化：用 ND_ASSIGN(target=alias, value=name) 节点表示别名
        MsNode* alias = MS_ARENA_NEW(p->arena, MsNode);
        alias->kind = ND_IDENT;
        alias->ident.name = p->prev.start;
        alias->ident.len  = p->prev.len;
        MsNode* pair = MS_ARENA_NEW(p->arena, MsNode);
        pair->kind = ND_ASSIGN;
        pair->assign.target = alias;
        pair->assign.value  = nameNode;
        nameNode = pair;
      }

      MsNodeList* item = MS_ARENA_NEW(p->arena, MsNodeList);
      item->node = nameNode; item->next = NULL;
      *fTail = item; fTail = &item->next;
    } while (match(p, TOK_COMMA));
  }

  MsNode* n = MS_ARENA_NEW(p->arena, MsNode);
  n->kind                    = ND_IMPORT;
  n->pos                     = pos;
  n->import_stmt.path        = path;
  n->import_stmt.as_name     = NULL;
  n->import_stmt.from_names  = fromNames;
  n->import_stmt.from_import = true;
  return n;
}
```

---

## 验收标准（checklist）

- [ ] `"import foo"` → `ND_IMPORT(path=[foo], from_import=false)`。
- [ ] `"import foo.bar.baz"` → `path=[foo, bar, baz]`。
- [ ] `"import foo as f"` → `as_name="f"`。
- [ ] `"from foo import bar"` → `from_import=true`, `from_names=[bar]`。
- [ ] `"from foo import bar as b, baz"` → `from_names=[ASSIGN(b,bar), baz]`。
- [ ] `"from foo import *"` → `from_names=[ND_STAR_EXPR]`。
- [ ] `"from . import mod"` → 相对导入（`path=[]`, 相对级别=1，初版可简化处理）。

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

static void testFromImport(void) {
  MsArena a; msArenaInit(&a);
  MsNode* n = pStmt(&a, "from math import sqrt, pi");
  MS_ASSERT_EQ(n->kind, ND_IMPORT, "from-import");
  MS_ASSERT_TRUE(n->import_stmt.from_import, "is from-import");
  int cnt = 0;
  for (MsNodeList* l = n->import_stmt.from_names; l; l = l->next) cnt++;
  MS_ASSERT_EQ(cnt, 2, "2 names");
  msArenaFree(&a);
}

int main(void) {
  MS_RUN(testImportBasic);
  MS_RUN(testFromImport);
  return msTestSummary();
}
```

### .ms 使用示例（T086/T087 后验证）

```ms
import math
print(math.pi)          // 3.141592653589793

import math as m
print(m.sqrt(16))       // 4.0

from math import sqrt, floor
print(sqrt(25))         // 5.0
print(floor(3.7))       // 3

from collections import deque as Deque
q := Deque()
q.append(1)

// 相对导入（在包内）
// from . import utils
// from ..base import Base
```

---

## Benchmark

N/A（归入 T036 整体 parse bench）。

---

## 风险与边界

- **`from` 关键字共用**：`from` 在 `raise … from …` 和 `from … import …` 两处使用（`TOK_FROM`，T031 已引用）。
- **相对导入 `..`**：若词法器对 `..` 产生 `TOK_ERROR`（T013 设计），则相对导入路径 `from .. import foo` 不可直接扫描；解决方案：词法器对 `..` 在 `import` 上下文中特殊处理，或用 `TOK_DOT TOK_DOT` 序列表示（需 parser 向前探测）。初版可仅支持单级 `.`。
- **`import *` 的作用域**：`from foo import *` 导入所有公共名称到当前模块命名空间；模块（T086）和字节码（T091）层处理。
