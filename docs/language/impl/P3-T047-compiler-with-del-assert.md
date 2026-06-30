# P3-T047 with / go / select / send / import 编译

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `ND_WITH`、`ND_GO`、`ND_SELECT`、`ND_SEND` 以及导入语句的字节码编译，补全 P3 阶段剩余语句编译路径，确保编译器对所有合法 AST 节点均有处理（`compileStmt` 无漏分支）。注：`ND_DEL`/`ND_ASSERT` 已由现有编译器实现（`compileDel` 第 792 行、`compileAssert` 第 1029 行），本任务不再重复。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T039 | `compileExpr` |
| P2-T033 | `ND_WITH` 节点 |
| P2-T032 | `ND_GO`/`ND_SELECT`/`ND_SEND` |
| P2-T035 | `ND_IMPORT` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3.11 with 语义（WITH_ENTER/WITH_EXIT） |
| `vm.md` | §3.12 并发指令（GO / MAKE_CHAN / CHAN_SEND/RECV / SELECT_BEGIN/END） |
| `vm.md` | §9 opcode 命名映射（OP_IMPORT/OP_IMPORT_FROM） |
| `modules.md` | §4 import 命名空间 / §5 子模块访问 |
| `syntax.md` | §2.2 语句文法（WithStmt / SelectStmt / SendStmt / ImportDecl） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/compiler/ms_compiler.c   # compileWith / compileGo
                              # compileSelect / compileSend / compileImport
                              # compileStmt（新增 MS_ND_WITH/GO/SELECT/SEND/IMPORT case）
```

---

## 实现要点

### 1. with 编译

```
compileExpr(ctx_mgr)
OP_WITH_ENTER            ← 调用 ctx.__enter__()，结果留在栈顶（或 nil）
[bind as name → SET_LOCAL]
OP_PUSH_EXCEPT [handler]  ← 注册异常处理器
[body]
OP_POP_EXCEPT
OP_WITH_EXIT [0]          ← 正常退出：调用 ctx.__exit__(nil, nil, nil)
OP_JUMP [after]

[handler]:
OP_WITH_EXIT [1]          ← 异常退出：调用 ctx.__exit__(exc_type, exc_val, tb)
                            若 __exit__ 返回 true → POP_EXCEPT（吞异常）
                            否则 → RERAISE
[after]:
```

```c
static void compileWith(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  compileExpr(c, n->withStmt.expr);
  msChunkEmitOp(c->chunk, OP_WITH_ENTER, line);

  if (n->withStmt.asName) {
    int slot = msScopeDeclareLocal(c, n->withStmt.asName, identLen(n->withStmt.asName));
    msScopeMarkInitialized(c);
    msChunkEmitOpA(c->chunk, OP_SET_LOCAL, (uint8_t)slot, line);
  } else {
    msChunkEmitOp(c->chunk, OP_POP, line);
  }

  uint32_t exceptPatch = emitJump(c, OP_PUSH_EXCEPT, line);

  msScopeBegin(c);
  compileBlock(c, n->withStmt.body);
  msScopeEnd(c);

  msChunkEmitOp(c->chunk, OP_POP_EXCEPT, line);
  msChunkEmitOpA(c->chunk, OP_WITH_EXIT, 0, line);   // 正常退出（vm.md §3.11）
  uint32_t jumpAfter = emitJump(c, OP_JUMP, line);

  patchJump(c, exceptPatch);
  msChunkEmitOpA(c->chunk, OP_WITH_EXIT, 1, line);   // 异常退出：VM 根据 __exit__ 返回值决定 POP_EXCEPT 或 RERAISE

  patchJump(c, jumpAfter);
}
```

### 2. del 编译

del 在 v1 支持三种目标：
- `del name`：全局变量（`OP_DEL_GLOBAL`）；局部变量 del 不支持（类 Python 语义，报编译错误）。
- `del obj.attr`：`OP_DEL_ATTR(nameIdx)`。
- `del obj[key]`：`OP_DEL_INDEX`。

// 注：compileDel 已在现有编译器中实现（src/compiler/ms_compiler.c 第 792 行），此处为规格参考。
```c
static void compileDel(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  MsNode* target = n->singleExpr.expr;
  switch (target->kind) {
  case MS_ND_IDENT: {
    if (msScopeResolveLocal(c, target->ident.name, target->ident.len) >= 0) {
      compilerError(c, target->pos, "cannot del local variable in current version");
      return;
    }
    uint32_t nameIdx = addStringConst(c, target->ident.name, target->ident.len);
    msChunkEmitOpAX(c->chunk, OP_DEL_GLOBAL, nameIdx, line);
    break;
  }
  case MS_ND_ATTR: {
    compileExpr(c, target->attr.obj);
    uint32_t nameIdx = addStringConst(c, target->attr.name, target->attr.nameLen);
    msChunkEmitOpAX(c->chunk, OP_DEL_ATTR, nameIdx, line);
    break;
  }
  case MS_ND_INDEX:
    compileExpr(c, target->index.obj);
    compileExpr(c, target->index.idx);
    msChunkEmitOp(c->chunk, OP_DEL_INDEX, line);
    break;
  default:
    compilerError(c, target->pos, "invalid del target");
  }
}
```

### 3. go 编译

```c
static void compileGo(MsCompiler* c, MsNode* n) {
  // go 语句：分别压入 callee 和各实参，再 OP_GO [A: argc]
  // 不能用 compileExpr 编译完整 call——那会 emit OP_CALL 立即调用，与 OP_GO 语义冲突
  MsNode* call = n->goStmt.call;
  compileExpr(c, call->callExpr.callee);
  uint32_t argc = 0;
  for (MsNodeList* arg = call->callExpr.args; arg; arg = arg->next) {
    compileExpr(c, arg->node);
    argc++;
  }
  msChunkEmitOpA(c->chunk, OP_GO, (uint8_t)argc, n->pos.line);
}
```

注：`OP_GO` 的 VM 语义是弹出 callee + argc 个实参，创建新 `MsCoroutine` 并将其加入调度队列（T107 实现）。

### 4. send 编译

```c
// ch <- val  （send 是语句，不是表达式）
static void compileSend(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  compileExpr(c, n->send.chanExpr);    // 信道
  compileExpr(c, n->send.val);         // 值
  msChunkEmitOp(c->chunk, OP_CHAN_SEND, line);
}
```

### 5. select 编译

select 是并发原语，按 `vm.md §3.12` 编译为四条指令序列（`OP_SELECT_BEGIN`/`OP_SELECT_CASE_SEND`/`OP_SELECT_CASE_RECV`/`OP_SELECT_END`），VM 运行时阻塞直到任一信道就绪再跳转对应 case body。

```
OP_SELECT_BEGIN [AX: case_count]
  // 逐 case 压入通信表达式，emit 对应 CASE 指令
  OP_SELECT_CASE_SEND   ← send case：ch expr + val expr 先入栈
  OP_SELECT_CASE_RECV   ← recv case：ch expr 先入栈
  ...
OP_SELECT_END           ← 阻塞等待，跳到就绪 case body
[case 0 body]
OP_JUMP [after_select]
[case 1 body]
OP_JUMP [after_select]
...
[default body]
[after_select]:
```

初版（T047）只需保证编译层结构正确；信道阻塞语义由 T110 完整实现。

### 6. import 编译

```c
static void compileImport(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  // import foo.bar [as baz]（modules.md §4/§5；from…import 不在语言范围内）
  // 遍历 importStmt.path（MsNodeList）拼接点分字符串，addStringConst 暂占位（T049 后替换为真实 MsStr）
  uint32_t nameIdx = addStringConst(c, /* dotted-name string */, /* len */);
  msChunkEmitOpAX(c->chunk, OP_IMPORT, nameIdx, line);
  // 无别名时绑定首段名（modules.md §4：import os.path 绑定 os，非末段 path）
  const char* bindName = n->importStmt.asName
                             ? n->importStmt.asName
                             : n->importStmt.path->node->ident.name;
  uint32_t bindIdx = addStringConst(c, bindName, identLen(bindName));
  msChunkEmitOpAX(c->chunk, OP_SET_GLOBAL, bindIdx, line);
}
```

### 7. compileStmt 新增分支

在 `compileStmt` 的 `switch` 中补全以下 `case`：

```c
case MS_ND_WITH:
  compileWith(c, node);
  break;
case MS_ND_GO:
  compileGo(c, node);
  break;
case MS_ND_SELECT:
  compileSelect(c, node);
  break;
case MS_ND_SEND:       // send 是语句节点，在 compileStmt 中分发
  compileSend(c, node);
  break;
case MS_ND_IMPORT:
  compileImport(c, node);
  break;
```

注：`MS_ND_FALLTHROUGH` 仅在 switch case 内合法（`syntax.md §2.2`），由 `compileSwitch` 内部处理，不在此分发。`MS_ND_SEND` 在 `ms_ast.h` 中虽归 channel ops 节点，仍作为语句在 `compileStmt` 分发。

---

## 验收标准（checklist）

- [ ] `"with f() as x { }"` → `OP_WITH_ENTER`, `OP_SET_LOCAL`, `OP_PUSH_EXCEPT`, body, `OP_POP_EXCEPT`, `OP_WITH_EXIT(0)`, `OP_JUMP`, handler: `OP_WITH_EXIT(1)`。
- [ ] `"with f() { }"` → 无绑定时 `OP_POP` 替代 `OP_SET_LOCAL`。
- [ ] `"del x"` → `OP_DEL_GLOBAL`（x 为全局）。
- [ ] `"del obj.a"` → `OP_GET_GLOBAL(obj)`, `OP_DEL_ATTR("a")`。
- [ ] `"del arr[0]"` → `OP_GET_GLOBAL(arr)`, `OP_CONST(0)`, `OP_DEL_INDEX`。
- [ ] `"del localVar"` → 编译错误（cannot del local variable in current version）。
- [ ] `"go f()"` → 压入 callee + args 后 `OP_GO [A: argc]`（不 emit `OP_CALL`）。
- [ ] `"ch <- 42"` → `OP_GET_GLOBAL(ch)`, `OP_CONST(42)`, `OP_CHAN_SEND`。
- [ ] `"import os"` → `OP_IMPORT("os")`, `OP_SET_GLOBAL("os")`。
- [ ] `"import os.path"` → `OP_IMPORT("os.path")`, 绑定到 `os`（首段，modules.md §4）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_misc_compile.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

static void testWithEnter(void) {
  MsCompileResult r = msCompile("with f() { pass }", 17, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  bool hasEnter = false;
  for (uint32_t i = 0; i < r.chunk->codeLen; i++)
    if (r.chunk->code[i] == OP_WITH_ENTER) hasEnter = true;
  MS_ASSERT_TRUE(hasEnter, "has WITH_ENTER");
  msCompileResultFree(&r);
}

static void testDelGlobal(void) {
  MsCompileResult r = msCompile("del x", 5, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  bool hasDel = false;
  for (uint32_t i = 0; i < r.chunk->codeLen; i++)
    if (r.chunk->code[i] == OP_DEL_GLOBAL) hasDel = true;
  MS_ASSERT_TRUE(hasDel, "has DEL_GLOBAL");
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testWithEnter);
  MS_RUN(testDelGlobal);
  return msTestSummary();
}
```

### .ms 使用示例（VM 就绪后验证）

```ms
import math


// del
x := 42
del x
// print(x) → NameError


// import
print(math.pi)        // 3.14159...
print(math.sqrt(16))  // 4.0


// with 语句（需 T082 __enter__/__exit__ 支持）
// with open("file.txt") as f {
//     data := f.read()
// }


// go（需 T107 调度器）
// go func() { print("hello from goroutine") }()


// send/recv（需 T108 channel）
// ch := make(chan int, 1)
// ch <- 99
// v := <-ch
// print(v)  // 99
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **`OP_WITH_EXIT` 异常吞并**：VM（T082）需检查 `__exit__` 返回值；编译层不需额外判断，VM 根据返回值决定是否 `POP_EXCEPT`（吞异常）或 `RERAISE`。
- **`del` 局部变量**：Python 支持 `del` 局部变量（置为 undefined），mslang v1 简化为不支持，报编译错误。
- **`select` 初版**：单线程调度器（T106）实现 select 的"第一个就绪者优先"语义；`OP_SELECT_BEGIN`/`OP_SELECT_CASE_SEND`/`OP_SELECT_CASE_RECV`/`OP_SELECT_END` 的信道阻塞语义由 T110 完整实现，T047 只负责编译层结构正确。
