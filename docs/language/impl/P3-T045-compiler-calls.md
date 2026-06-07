# P3-T045 调用编译（CALL / CALL_EX / CALL_KW / CALL_ASYNC）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `ND_CALL` 节点的字节码编译，生成适当的调用指令：普通调用 `OP_CALL`、关键字参数调用 `OP_CALL_KW`、展开调用 `OP_CALL_EX`，以及 await 表达式（`OP_AWAIT`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T039 | `compileExpr` |
| P2-T021 | `ND_CALL`/`ND_KWARG_PAIR`/`ND_STAR_EXPR` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §4 调用约定（参数布局/callee 在栈底） |
| `vm.md` | §4.1 `OP_CALL` 编码 |
| `vm.md` | §4.2 `OP_CALL_KW`（关键字参数名列表） |
| `vm.md` | §4.3 `OP_CALL_EX`（*args/**kwargs 展开） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/compiler/ms_compiler.c   # compileCall / compileAwait
```

---

## 实现要点

### 调用约定（栈布局）

```
栈底 → [..., callee, arg0, arg1, ..., argN]
OP_CALL  arg=N（位置参数个数）
```

- callee 在最低处，参数从左到右压栈。
- `OP_CALL` 执行后，callee+args 全部弹出，返回值压栈。

### 1. 普通调用

```c
static void compileCall(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  // 编译 callee
  compileExpr(c, n->call.callee);

  // 检测调用类型
  bool hasKwarg = false;
  bool hasStar  = false;
  for (MsNodeList* l = n->call.kwargs; l; l = l->next) hasKwarg = true;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    if (l->node->kind == ND_STAR_EXPR) { hasStar = true; break; }
  }

  if (hasStar || hasKwarg) {
    compileCallEx(c, n, line);
    return;
  }

  // 纯位置参数调用
  int argc = 0;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    if (l->node->kind == ND_KWARG_PAIR) { hasKwarg = true; continue; }
    compileExpr(c, l->node);
    argc++;
  }

  if (hasKwarg) {
    // 混合位置 + 关键字
    compileCallKw(c, n, argc, line);
  } else {
    emitOp8(c->chunk, OP_CALL, (uint8_t)argc, line);
  }
}
```

### 2. 关键字参数调用（`OP_CALL_KW`）

```
[callee, pos_arg0, ..., pos_argM, kw_val0, ..., kw_valK]
OP_CALL_KW  [1B: argc_pos]  [2B: kwNamesTupleIdx]
```

`kwNamesTupleIdx`：常量池中一个存储关键字名称 tuple 的 `MsTuple` 对象的索引。

```c
static void compileCallKw(MsCompiler* c, MsNode* n, int posArgc, uint32_t line) {
  // 已压完 posArgc 个位置参数
  // 现在压关键字值，同时收集关键字名称
  const char* kwNames[256]; uint32_t kwLens[256]; int kwCount = 0;

  for (MsNodeList* l = n->call.kwargs; l; l = l->next) {
    MsNode* kw = l->node;
    if (kw->kind != ND_KWARG_PAIR) continue;
    kwNames[kwCount] = kw->attr.name;
    kwLens[kwCount]  = kw->attr.nameLen;
    compileExpr(c, kw->attr.obj);  // kw value
    kwCount++;
  }
  // 从 args 列表中取 kwarg pair（若 args 也有 kwarg）
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    MsNode* kw = l->node;
    if (kw->kind != ND_KWARG_PAIR) continue;
    kwNames[kwCount] = kw->attr.name;
    kwLens[kwCount]  = kw->attr.nameLen;
    compileExpr(c, kw->attr.obj);
    kwCount++;
  }

  // 构建 kwNames tuple 常量
  // MsValue nameTuple = buildStringTuple(kwNames, kwLens, kwCount);
  // uint16_t kwIdx = msChunkAddConst(c->chunk, nameTuple);
  uint16_t kwIdx = buildKwNamesTupleConst(c, kwNames, kwLens, kwCount);

  emit(c, OP_CALL_KW, line);
  emit(c, (uint8_t)posArgc, line);
  emit(c, (uint8_t)(kwIdx >> 8), line);
  emit(c, (uint8_t)(kwIdx & 0xFF), line);
}
```

### 3. 展开调用（`OP_CALL_EX`）

```
[callee, args_tuple/list, kwargs_dict]
OP_CALL_EX  [1B: has_kwargs]
```

```c
static void compileCallEx(MsCompiler* c, MsNode* n, uint32_t line) {
  // 位置参数：普通参数 + *expr 收集为 tuple/list，拼接
  // 关键字参数：收集为 dict，合并 **dict 展开
  // 初版简化：
  //   1. 编译所有位置 args（含 *expr）为 OP_BUILD_LIST + OP_CALL_EX
  //   2. 编译所有 kwarg 为 OP_BUILD_MAP + OP_CALL_EX

  // 位置参数列表
  bool hasStar = false;
  int plainArgc = 0;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    MsNode* a = l->node;
    if (a->kind == ND_STAR_EXPR) {
      hasStar = true;
      if (plainArgc > 0) emitOp16(c->chunk, OP_BUILD_LIST, (uint16_t)plainArgc, line);
      compileExpr(c, a->unary.operand);  // 展开的 iterable
      // TODO: extend 操作
      plainArgc = 0;
    } else if (a->kind != ND_KWARG_PAIR) {
      compileExpr(c, a);
      plainArgc++;
    }
  }
  if (plainArgc > 0 || !hasStar) {
    emitOp16(c->chunk, OP_BUILD_LIST, (uint16_t)plainArgc, line);
  }

  // 关键字参数 dict
  bool hasKwarg = false;
  int kwCount = 0;
  for (MsNodeList* l = n->call.kwargs; l; l = l->next) {
    MsNode* kw = l->node;
    if (kw->kind == ND_DOUBLESTAR_EXPR) {
      hasKwarg = true;
      if (kwCount > 0) emitOp16(c->chunk, OP_BUILD_MAP, (uint16_t)kwCount, line);
      compileExpr(c, kw->unary.operand);
      kwCount = 0;
    } else if (kw->kind == ND_KWARG_PAIR) {
      hasKwarg = true;
      uint16_t nameIdx = addStringConst(c, kw->attr.name, kw->attr.nameLen);
      emitOp16(c->chunk, OP_CONST, nameIdx, line);
      compileExpr(c, kw->attr.obj);
      kwCount++;
    }
  }
  if (kwCount > 0) emitOp16(c->chunk, OP_BUILD_MAP, (uint16_t)kwCount, line);
  if (!hasKwarg) emit(c, OP_NIL, line);  // kwargs=nil → 无 kwargs

  emit(c, OP_CALL_EX, line);
  emit(c, hasKwarg ? 1 : 0, line);
}
```

### 4. await 编译

```c
case ND_AWAIT:
  compileExpr(c, n->await_expr.expr);
  emit(c, OP_AWAIT, n->pos.line);
  break;
```

---

## 验收标准（checklist）

- [ ] `"f()"` → `OP_GET_GLOBAL(f)`, `OP_CALL(0)`。
- [ ] `"f(1, 2)"` → `OP_CALL(2)`。
- [ ] `"f(a=1, b=2)"` → `OP_CALL_KW(0, kwIdx)`，kwIdx 常量为 `("a", "b")`。
- [ ] `"f(1, x=2)"` → `OP_CALL_KW(1, kwIdx)`。
- [ ] `"f(*args)"` → `OP_CALL_EX`，args 为列表。
- [ ] `"f(**kw)"` → `OP_CALL_EX(has_kwargs=1)`。
- [ ] `"await f()"` → `OP_CALL(0)`, `OP_AWAIT`。
- [ ] 方法调用 `"obj.method(1)"` → `OP_GET_ATTR`, `OP_CALL(1)`（但实际需 `obj` 在栈上；T068 调用约定需双操作数形式处理 bound method）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_call_compile.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

static void testSimpleCall(void) {
  MsCompileResult r = msCompile("f(1, 2)", 7, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  bool hasCall = false;
  for (uint32_t i = 0; i < r.chunk->codeLen; i++)
    if (r.chunk->code[i] == OP_CALL) hasCall = true;
  MS_ASSERT_TRUE(hasCall, "has CALL");
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testSimpleCall);
  return msTestSummary();
}
```

### .ms 使用示例（T067/T068 后验证）

```ms
func add(a, b=0) { return a + b }

// 位置调用
print(add(1, 2))    // 3

// 关键字调用
print(add(a=5, b=3)) // 8

// 展开调用
args := [10, 20]
print(add(*args))    // 30

kw := {"a": 7, "b": 8}
print(add(**kw))     // 15

// await（需 T111 async 支持）
// async func fetch(url) { return await httpGet(url) }
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **方法调用 bound method 问题**：`obj.method(args)` 中，`obj` 需传给 `self`；初版实现方式：`OP_GET_ATTR` 返回绑定方法对象（`MsBoundMethod`，含 obj 和 func），`OP_CALL` 调用时 VM 自动提取 obj 作为第一个参数（T068/T073 处理）。
- **`OP_CALL_EX` 参数顺序**：栈布局为 `[callee, args_list, kwargs_dict]`；VM（T068）需正确展开并构建调用帧。
- **`OP_CALL_KW` kwNames 常量**：常量池中存储 `MsTuple(str…)` 对象，需 GC 管理；T050 之前直接 `msAlloc`。
