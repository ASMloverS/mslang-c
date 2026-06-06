# P3-T047 with / del / assert 编译

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `ND_WITH`、`ND_DEL`、`ND_GO`、`ND_SELECT`、`ND_SEND` 以及导入语句的字节码编译，补全 P3 阶段剩余语句编译路径，确保编译器对所有合法 AST 节点均有处理（`compileStmt` 无漏分支）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T039 | `compileExpr` |
| P2-T033 | `ND_WITH` 节点 |
| P2-T030 | `ND_DEL`/`ND_ASSERT` |
| P2-T032 | `ND_GO`/`ND_SELECT`/`ND_SEND` |
| P2-T035 | `ND_IMPORT` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §7 with 语义（WITH_ENTER/WITH_EXIT） |
| `vm.md` | §8 并发指令（GO / MAKE_CHAN / CHAN_SEND/RECV / SELECT） |
| `vm.md` | §9 模块指令（IMPORT / IMPORT_FROM） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/compiler/ms_compiler.c   # compileWith / compileDel / compileGo
                              # compileSelect / compileSend / compileImport
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

    compileExpr(c, n->with_stmt.ctx_expr);
    emit(c, OP_WITH_ENTER, line);

    if (n->with_stmt.bind_name) {
        int slot = declareLocal(c, n->with_stmt.bind_name, n->with_stmt.bind_len);
        markInitialized(c);
        emitOp8(c->chunk, OP_SET_LOCAL, (uint8_t)slot, line);
    } else {
        emit(c, OP_POP, line);
    }

    uint32_t exceptPatch = emitJump(c, OP_PUSH_EXCEPT, line);

    scopeBegin(c);
    compileBlock(c, n->with_stmt.body);
    scopeEnd(c);

    emit(c, OP_POP_EXCEPT, line);
    emitOp8(c->chunk, OP_WITH_EXIT, 0, line);   // 正常退出
    uint32_t jumpAfter = emitJump(c, OP_JUMP, line);

    patchJump(c, exceptPatch);
    emitOp8(c->chunk, OP_WITH_EXIT, 1, line);   // 异常退出（__exit__ 决定是否吞）
    // VM 根据 __exit__ 返回值决定 POP_EXCEPT 或 RERAISE，编译层无需额外 emit

    patchJump(c, jumpAfter);
}
```

### 2. del 编译

del 在 v1 支持三种目标：
- `del name`：全局变量（`OP_DEL_GLOBAL`）；局部变量 del 不支持（类 Python 语义，报编译错误）。
- `del obj.attr`：`OP_DEL_ATTR(nameIdx)`。
- `del obj[key]`：`OP_DEL_INDEX`。

```c
static void compileDel(MsCompiler* c, MsNode* n) {
    uint32_t line = n->pos.line;
    MsNode* target = n->single_expr.expr;
    switch (target->kind) {
    case ND_IDENT: {
        if (resolveLocal(c, target->ident.name, target->ident.nameLen) >= 0) {
            compilerError(c, target->pos, "cannot del local variable");
            return;
        }
        uint16_t nameIdx = addStringConst(c, target->ident.name, target->ident.nameLen);
        emitOp16(c->chunk, OP_DEL_GLOBAL, nameIdx, line);
        break;
    }
    case ND_ATTR:
        compileExpr(c, target->attr.obj);
        uint16_t nameIdx = addStringConst(c, target->attr.name, target->attr.nameLen);
        emitOp16(c->chunk, OP_DEL_ATTR, nameIdx, line);
        break;
    case ND_INDEX:
        compileExpr(c, target->index.obj);
        compileExpr(c, target->index.idx);
        emit(c, OP_DEL_INDEX, line);
        break;
    default:
        compilerError(c, target->pos, "invalid del target");
    }
}
```

### 3. go 编译

```c
static void compileGo(MsCompiler* c, MsNode* n) {
    // go 语句：编译调用（callee + args），然后 emit OP_GO
    // OP_GO 从栈顶弹出可调用对象 + 参数列表，创建新协程并调度
    compileExpr(c, n->single_expr.expr);  // 整个 call 表达式（含参数入栈）
    emit(c, OP_GO, n->pos.line);
}
```

注：`OP_GO` 的 VM 语义是弹出调用帧准备好的函数+参数，创建新 `MsCoroutine` 并将其加入调度队列（T107 实现）。

### 4. send 编译

```c
// ch <- val  （send 是语句，不是表达式）
static void compileSend(MsCompiler* c, MsNode* n) {
    uint32_t line = n->pos.line;
    compileExpr(c, n->send_stmt.chan);    // 信道
    compileExpr(c, n->send_stmt.val);    // 值
    emit(c, OP_CHAN_SEND, line);
}
```

### 5. select 编译

select 是并发原语，编译时将各 case 的通信操作与 body 位置打包成描述表，`OP_SELECT` 让 VM 在运行时根据就绪信道选择分支。

```
[compile each case's channel expression]
OP_SELECT  [1B: case_count]  [1B: has_default]
    [for each case: 1B case_kind, 2B body_offset]
[case 0 body]
[case 1 body]
...
[default body / fallthrough]
```

初版简化：`OP_SELECT` 的 case body 作为 inline 分支（类似 switch 跳转表）。

### 6. import 编译

```c
static void compileImport(MsCompiler* c, MsNode* n) {
    uint32_t line = n->pos.line;

    if (!n->import_stmt.from_import) {
        // import foo.bar [as baz]
        uint16_t nameIdx = addDottedNameConst(c, n->import_stmt.path);
        emitOp16(c->chunk, OP_IMPORT, nameIdx, line);
        // 结果为模块对象，绑定到 alias 或最后一段名称
        const char* bindName = n->import_stmt.alias
                               ? n->import_stmt.alias
                               : n->import_stmt.lastName;
        emitSetVar(c, bindName, strlen(bindName), line);
        emit(c, OP_POP, line);
    } else {
        // from foo.bar import name1 [as a], name2 [as b]
        uint16_t nameIdx = addDottedNameConst(c, n->import_stmt.path);
        emitOp16(c->chunk, OP_IMPORT, nameIdx, line);   // 导入模块，留在栈顶
        for (MsNodeList* l = n->import_stmt.from_names; l; l = l->next) {
            MsNode* item = l->node;
            emit(c, OP_DUP, line);                      // 复制模块对象
            uint16_t attrIdx = addStringConst(c, item->ident.name, item->ident.nameLen);
            emitOp16(c->chunk, OP_IMPORT_FROM, attrIdx, line);  // 取属性
            // 绑定到 alias 或原名
            const char* bind = item->ident.alias ? item->ident.alias : item->ident.name;
            emitSetVar(c, bind, strlen(bind), line);
            emit(c, OP_POP, line);
        }
        emit(c, OP_POP, line);   // 弹出模块对象
    }
}
```

---

## 验收标准（checklist）

- [ ] `"with f() as x { }"` → `OP_WITH_ENTER`, `OP_SET_LOCAL`, `OP_PUSH_EXCEPT`, body, `OP_POP_EXCEPT`, `OP_WITH_EXIT(0)`, `OP_JUMP`, handler: `OP_WITH_EXIT(1)`。
- [ ] `"with f() { }"` → 无绑定时 `OP_POP` 替代 `OP_SET_LOCAL`。
- [ ] `"del x"` → `OP_DEL_GLOBAL`（x 为全局）。
- [ ] `"del obj.a"` → `OP_GET_GLOBAL(obj)`, `OP_DEL_ATTR("a")`。
- [ ] `"del arr[0]"` → `OP_GET_GLOBAL(arr)`, `OP_CONST(0)`, `OP_DEL_INDEX`。
- [ ] `"del localVar"` → 编译错误（cannot del local variable）。
- [ ] `"go f()"` → 编译 f 调用后 `OP_GO`。
- [ ] `"ch <- 42"` → `OP_GET_GLOBAL(ch)`, `OP_CONST(42)`, `OP_CHAN_SEND`。
- [ ] `"import os"` → `OP_IMPORT("os")`, `OP_SET_GLOBAL("os")`, `OP_POP`。
- [ ] `"from os import path, getcwd"` → `OP_IMPORT("os")`, ×2 `OP_DUP`+`OP_IMPORT_FROM`, 绑定到各名称, `OP_POP`。

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
// with 语句（需 T082 __enter__/__exit__ 支持）
// with open("file.txt") as f {
//     data := f.read()
// }

// del
x := 42
del x
// print(x) → NameError

// import
import math
print(math.pi)        // 3.14159...

from math import sqrt
print(sqrt(16))       // 4.0

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
- **`select` 初版**：单线程调度器（T106）实现 select 的"第一个就绪者优先"语义；`OP_SELECT` 在 T110 才完整实现，T047 只负责编译层结构正确。
