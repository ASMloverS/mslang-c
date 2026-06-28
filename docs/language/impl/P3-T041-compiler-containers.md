# P3-T041 容器构建指令（list / map / tuple / set / slice）

> **状态**：✅ 已完成

---

## 任务目标 / 背景

实现 `ND_LIST`/`ND_MAP`/`ND_TUPLE`/`ND_SET`/`ND_SLICE` 节点到字节码的编译，发出 `OP_BUILD_LIST`/`OP_BUILD_MAP`/`OP_BUILD_TUPLE`/`OP_BUILD_SET`/`OP_BUILD_SLICE` 指令。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T039 | `compileExpr` 骨架 |
| P2-T022 | `ND_LIST`/`ND_MAP`/`ND_SET` 节点 |
| P2-T023 | `ND_TUPLE` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §2 容器构建指令（`OP_BUILD_LIST` 等） |
| `type-system.md` | §2.6 list / §2.8 set / §2.9 map / §2.10 tuple |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/compiler/ms_compiler.c   # compileContainer / compileSlice / compileIndex
```

---

## 实现要点

### 1. list 编译

```c
static void compileList(MsCompiler* c, MsNode* n) {
  uint32_t line  = n->pos.line;
  int      count = 0;
  for (MsNodeList* l = n->container.elems; l; l = l->next) {
    MsNode* elem = l->node;
    if (elem->kind == MS_ND_STAR_EXPR) {
      // *expr 展开：先 BUILD_LIST 已有元素，再 EXTEND
      // 初版简化：*expr 展开不支持，报编译错误
      compilerError(c, elem->pos, "list unpacking in literal not supported in v0.1");
      return;
    }
    compileExpr(c, elem);
    count++;
  }
  msChunkEmitOpA(c->chunk, OP_BUILD_LIST, (uint8_t)count, line);
}
```

### 2. map 编译

```c
static void compileMap(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  int      count = 0;
  for (MsNodeList* l = n->map.pairs; l; l = l->next) {
    MsNode* pair = l->node;
    if (pair->kind == MS_ND_DOUBLESTAR_EXPR) {
      // **d 展开：初版不支持
      compilerError(c, pair->pos, "dict unpacking in literal not supported in v0.1");
      return;
    }
    // pair = MS_ND_BINARY(MS_TOK_COLON, key, val)
    compileExpr(c, pair->binary.left);   // key
    compileExpr(c, pair->binary.right);  // value
    count++;
  }
  msChunkEmitOpA(c->chunk, OP_BUILD_MAP, (uint8_t)count, line);
}
```

### 3. set 编译

```c
static void compileSet(MsCompiler* c, MsNode* n) {
  uint32_t line  = n->pos.line;
  int      count = 0;
  for (MsNodeList* l = n->container.elems; l; l = l->next) {
    compileExpr(c, l->node);
    count++;
  }
  msChunkEmitOpA(c->chunk, OP_BUILD_SET, (uint8_t)count, line);
}
```

### 4. tuple 编译

```c
static void compileTuple(MsCompiler* c, MsNode* n) {
  uint32_t line  = n->pos.line;
  int      count = 0;
  for (MsNodeList* l = n->container.elems; l; l = l->next) {
    compileExpr(c, l->node);
    count++;
  }
  msChunkEmitOpA(c->chunk, OP_BUILD_TUPLE, (uint8_t)count, line);
}
```

### 5. slice 编译

```c
// 对应 a[lo:hi:step] 的读操作（写操作 SET_SLICE 另行处理）
// BUILD_SLICE 仅消费栈顶 popcount(flags) 个值，不消费 obj；
// 执行后栈为 [obj, slice]，再由 GET_ITEM 完成下标。
static void compileSliceExpr(MsCompiler* c, MsNode* n) {
  uint32_t line  = n->pos.line;
  uint8_t  flags = 0;
  compileExpr(c, n->slice.obj);            // 被切片对象（BUILD_SLICE 不消费）
  if (n->slice.lo)   { compileExpr(c, n->slice.lo);   flags |= 0x1; }
  if (n->slice.hi)   { compileExpr(c, n->slice.hi);   flags |= 0x2; }
  if (n->slice.step) { compileExpr(c, n->slice.step); flags |= 0x4; }
  msChunkEmitOpA(c->chunk, OP_BUILD_SLICE, flags, line);  // A = flags
  msChunkEmitOp(c->chunk, OP_GET_ITEM, line);
}
```

**注**：`OP_BUILD_SLICE` 操作数 A 为 flags 位掩码（bit0=lo，bit1=hi，bit2=step），仅已置位的参数在栈上，VM 按 flags 逐位出栈。追加 `OP_GET_SLICE`（取 4 个参数：obj、lo、hi、step，避免创建中间 slice 对象）到操作码枚举更高效；初版可先用 `OP_BUILD_SLICE + OP_GET_ITEM` 实现，后续优化。

### 6. 下标访问 / 属性访问编译（在 `compileExpr` 中）

```c
case MS_ND_INDEX:
  compileExpr(c, n->index.obj);
  compileExpr(c, n->index.key);
  msChunkEmitOp(c->chunk, OP_GET_ITEM, n->pos.line);
  break;

case MS_ND_SLICE:
  compileSliceExpr(c, n);
  break;

case MS_ND_ATTR: {
  compileExpr(c, n->attr.obj);
  uint32_t nameIdx = addStringConst(c, n->attr.name, n->attr.nameLen);
  msChunkEmitOpAX(c->chunk, OP_GET_ATTR, nameIdx, n->pos.line);
  break;
}
```

---

## 验收标准（checklist）

- [ ] `"[1, 2, 3]"` → `OP_CONST(1)`, `OP_CONST(2)`, `OP_CONST(3)`, `OP_BUILD_LIST(3)`。
- [ ] `"[]"` → `OP_BUILD_LIST(0)`。
- [ ] `"{\"a\": 1}"` → `OP_CONST("a")`, `OP_CONST(1)`, `OP_BUILD_MAP(1)`。
- [ ] `"{}"` → `OP_BUILD_MAP(0)`。
- [ ] `"{1, 2}"` → `OP_CONST(1)`, `OP_CONST(2)`, `OP_BUILD_SET(2)`。
- [ ] `"(1, 2, 3)"` → `OP_BUILD_TUPLE(3)`。
- [ ] `"()"` → `OP_BUILD_TUPLE(0)`。
- [ ] `"a[1]"` → `OP_GET_LOCAL(a)`, `OP_CONST(1)`, `OP_GET_ITEM`。
- [ ] `"a[1:3]"` → obj + `OP_CONST(1)` + `OP_CONST(3)` + `OP_BUILD_SLICE(flags=0x3)` + `OP_GET_ITEM`。
- [ ] `"obj.x"` → `OP_GET_LOCAL(obj)`, `OP_GET_ATTR(<"x" idx>)`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_containers.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

static bool hasOp(MsChunk* ck, MsOpCode op) {
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == op) {
      return true;
    }
  }
  return false;
}

static void testBuildList(void) {
  MsCompileResult r = msCompile("[1,2,3]", 7, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  MS_ASSERT_TRUE(hasOp(r.chunk, OP_BUILD_LIST), "BUILD_LIST");
  msCompileResultFree(&r);
}

static void testBuildMap(void) {
  MsCompileResult r = msCompile("{\"a\":1}", 7, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  MS_ASSERT_TRUE(hasOp(r.chunk, OP_BUILD_MAP), "BUILD_MAP");
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testBuildList);
  MS_RUN(testBuildMap);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// list
nums := [1, 2, 3, 4, 5]
print(nums[2])      // 3
print(nums[1:4])    // [2, 3, 4]
print(nums[::2])    // [1, 3, 5]

// map
userInfo := {"name": "Alice", "age": 30}
print(userInfo["name"])    // Alice

// set
numSet := {1, 2, 3, 2}
print(len(numSet))         // 3

// tuple（不可变）
coord := (1, 2, 3)
print(coord[0])            // 1
// coord[0] = 9            // 运行时错误：tuple 不可变

// 嵌套
matrix := [[1, 2], [3, 4]]
print(matrix[1][0]) // 3
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **超过 255 个元素**：`OP_BUILD_*` 操作数为单字节（A 字段，0~255）；单字面量元素数超出时报编译错误（实践中极少见）。
- **set 哈希限制**：set 元素必须可哈希；编译时不检查，运行时（T062）验证。
- **slice 写操作**：`a[1:3] = [x, y]` 目前不支持（初版不实现 slice 赋值）；需要 `OP_SET_SLICE` 指令，留后续。
