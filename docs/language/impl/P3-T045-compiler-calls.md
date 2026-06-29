# P3-T045 调用编译（CALL / CALL_EX / CALL_KW / CALL_ASYNC）

> **状态**：✅ 已完成

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
| `vm.md` | §3.6 函数调用与返回（CALL / CALL_KW / CALL_EX / CALL_ASYNC 语义与栈布局） |
| `vm.md` | §3.8 容器构建（BUILD_LIST / BUILD_MAP） |
| `vm.md` | §3.12 并发（AWAIT） |
| `vm.md` | §9 实现层 opcode 命名映射 |

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

判定状态机：
- 含 `*args`（`MS_ND_STAR_EXPR`）或 `**kw`（`MS_ND_DOUBLESTAR_EXPR`）→ `CALL_EX`
- 含具名 kwarg（`call.kwargs` 非空）→ `CALL_KW`
- 否则 → `CALL`

```c
static void compileCall(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  compileExpr(c, n->call.callee);

  bool hasStar = false;
  bool hasDoublestar = false;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    if (l->node->kind == MS_ND_STAR_EXPR) { hasStar = true; break; }
  }
  for (MsNodeList* l = n->call.kwargs; l; l = l->next) {
    if (l->node->kind == MS_ND_DOUBLESTAR_EXPR) { hasDoublestar = true; break; }
  }

  if (hasStar || hasDoublestar) {
    compileCallEx(c, n, line);
    return;
  }

  if (n->call.kwargs != NULL) {
    compileCallKw(c, n, line);
    return;
  }

  // 纯位置参数
  int argc = 0;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    compileExpr(c, l->node);
    argc++;
  }
  msChunkEmitOpA(c->chunk, OP_CALL, (uint8_t)argc, line);
}
```

### 2. 关键字参数调用（`OP_CALL_KW`）

按 vm.md §3.6：

```
[callee, pos_arg0, ..., pos_argM, kwargs_map]
OP_CALL_KW  A: argc_pos
```

关键字参数在运行期以单个 map 对象置于栈顶；操作数仅为 1 字节位置参数个数。

```c
// §3.6 CALL_KW: [callee, pos_arg0..M, kwargs_map]  A = argc_pos
static void compileCallKw(MsCompiler* c, MsNode* n, uint32_t line) {
  // 位置参数逐个压栈
  int posArgc = 0;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    compileExpr(c, l->node);
    posArgc++;
  }

  // 关键字参数：压 key/val 对，再 BUILD_MAP
  int kwCount = 0;
  for (MsNodeList* l = n->call.kwargs; l; l = l->next) {
    MsNode* kw = l->node;
    if (kw->kind != MS_ND_KWARG_PAIR) continue;
    uint32_t nameIdx = addStringConst(c, kw->kwargPair.name, kw->kwargPair.nameLen);
    msChunkEmitOpAX(c->chunk, OP_CONST, nameIdx, line);
    compileExpr(c, kw->kwargPair.value);
    kwCount++;
  }
  msChunkEmitOpA(c->chunk, OP_BUILD_MAP, (uint8_t)kwCount, line);

  msChunkEmitOpA(c->chunk, OP_CALL_KW, (uint8_t)posArgc, line);
}
```

### 3. 展开调用（`OP_CALL_EX`）

按 vm.md §3.6：

```
[callee, args_list]
OP_CALL_EX  — （无操作数）
```

CALL_EX 展开最后一个 list 参数。`**kw` 展开（`MS_ND_DOUBLESTAR_EXPR`）的 VM 协议尚未在 §3.6 明确定义，留 TODO T068 处理。

```c
// §3.6 CALL_EX: [callee, args_list]  — no operand; expands last list arg
static void compileCallEx(MsCompiler* c, MsNode* n, uint32_t line) {
  // 位置参数（含 *expr）→ BUILD_LIST
  int plainArgc = 0;
  for (MsNodeList* l = n->call.args; l; l = l->next) {
    MsNode* a = l->node;
    if (a->kind == MS_ND_STAR_EXPR) {
      if (plainArgc > 0) {
        msChunkEmitOpA(c->chunk, OP_BUILD_LIST, (uint8_t)plainArgc, line);
        plainArgc = 0;
      }
      compileExpr(c, a->starExpr.expr);  // 展开的 iterable
      // TODO T068: list extend/concat with preceding segment
    } else {
      compileExpr(c, a);
      plainArgc++;
    }
  }
  if (plainArgc > 0) {
    msChunkEmitOpA(c->chunk, OP_BUILD_LIST, (uint8_t)plainArgc, line);
  }

  // **kw 展开：TODO T068（vm.md §3.6 尚未定义 CALL_EX + kwargs 协议）
  // 当前仅处理 MS_ND_DOUBLESTAR_EXPR 存在时直接编译 kw expr，由 VM 扩展支持
  for (MsNodeList* l = n->call.kwargs; l; l = l->next) {
    MsNode* kw = l->node;
    if (kw->kind == MS_ND_DOUBLESTAR_EXPR) {
      compileExpr(c, kw->starExpr.expr);
    }
  }

  msChunkEmitOp(c->chunk, OP_CALL_EX, line);
}
```

### 4. await 编译

```c
case MS_ND_AWAIT:
  compileExpr(c, n->awaitExpr.expr);
  msChunkEmitOp(c->chunk, OP_AWAIT, n->pos.line);
  break;
```

---

## 验收标准（checklist）

- [ ] `"f()"` → `OP_GET_GLOBAL(f)`, `OP_CALL(0)`。
- [ ] `"f(1, 2)"` → `OP_CALL(2)`。
- [ ] `"f(a=1, b=2)"` → `OP_BUILD_MAP(2)`, `OP_CALL_KW(0)`（argc_pos=0，kwargs_map 在栈顶）。
- [ ] `"f(1, x=2)"` → `OP_CALL(1)` 位置参数 + `OP_BUILD_MAP(1)`, `OP_CALL_KW(1)`（argc_pos=1）。
- [ ] `"f(*args)"` → `compileExpr(args)`, `OP_CALL_EX`（无操作数，展开最后一个 list）。
- [ ] `"f(**kw)"` → TODO T068（vm.md §3.6 未定义 CALL_EX + kwargs 协议，暂标为待实现）。
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
func add(a, b=0) {
    return a + b
}


// 位置调用
print(add(1, 2))     // 3


// 关键字调用
print(add(a=5, b=3)) // 8


// 展开调用
args := [10, 20]
print(add(*args))    // 30


kw := {"a": 7, "b": 8}
print(add(**kw))     // 15 （TODO T068：**kw 展开依赖 VM 扩展）


// await（需 T111 async 支持）
// async func fetch(url) { return await httpGet(url) }
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **方法调用 bound method 问题**：`obj.method(args)` 中，`obj` 需传给 `self`；初版实现方式：`OP_GET_ATTR` 返回绑定方法对象（`MsBoundMethod`，含 obj 和 func），`OP_CALL` 调用时 VM 自动提取 obj 作为第一个参数（T068/T073 处理）。
- **`OP_CALL_EX` 栈布局**：按 vm.md §3.6，栈布局为 `[callee, args_list]`，无操作数，VM（T068）展开最后一个 list 参数。`**kw` 展开协议待 vm.md §3.6 补充后由 T068 实现。
- **`OP_CALL_KW` kwargs 生命周期**：BUILD_MAP 构建的 map 对象需 GC 管理；T050 之前直接 `msAlloc`。
