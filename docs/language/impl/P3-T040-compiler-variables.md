# P3-T040 变量 load/store（local / global / upvalue）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现变量读写的字节码生成：局部变量（`OP_GET_LOCAL`/`OP_SET_LOCAL`）、全局变量（`OP_GET_GLOBAL`/`OP_SET_GLOBAL`）、upvalue（`OP_GET_UPVALUE`/`OP_SET_UPVALUE`），以及 `var`/`:=` 声明、普通赋值、复合赋值和 `del` 的编译。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T038 | 符号表（局部/upvalue 解析） |
| P3-T039 | `compileExpr` 骨架 |
| P2-T026 | `ND_VAR_DECL`/`ND_SHORT_DECL`/`ND_ASSIGN` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3 调用帧与局部变量槽 |
| `vm.md` | §3.3 upvalue open/close 语义 |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/compiler/ms_compiler.c   # compileIdent / compileVarDecl / compileAssign / compileDel
```

---

## 实现要点

### 1. 标识符读取（`ND_IDENT`）

```c
static void compileIdent(MsCompiler* c, MsNode* n) {
  const char* name = n->ident.name;
  uint32_t    len  = n->ident.len;
  uint32_t    line = n->pos.line;

  // 1. 尝试局部变量
  int local = resolveLocal(c, name, len);
  if (local >= 0) {
    emitOp8(c->chunk, OP_GET_LOCAL, (uint8_t)local, line);
    return;
  }

  // 2. 尝试 upvalue
  int upval = resolveUpvalue(c, name, len);
  if (upval >= 0) {
    emitOp8(c->chunk, OP_GET_UPVALUE, (uint8_t)upval, line);
    return;
  }

  // 3. 全局变量（按名称字符串索引）
  uint16_t nameIdx = addStringConst(c, name, len);
  emitOp16(c->chunk, OP_GET_GLOBAL, nameIdx, line);
}
```

### 2. 标识符写入辅助（`emitSetVar`）

```c
static void emitSetVar(MsCompiler* c, const char* name, uint32_t len, uint32_t line) {
  int local = resolveLocal(c, name, len);
  if (local >= 0) {
    emitOp8(c->chunk, OP_SET_LOCAL, (uint8_t)local, line);
    return;
  }
  int upval = resolveUpvalue(c, name, len);
  if (upval >= 0) {
    emitOp8(c->chunk, OP_SET_UPVALUE, (uint8_t)upval, line);
    return;
  }
  uint16_t nameIdx = addStringConst(c, name, len);
  emitOp16(c->chunk, OP_SET_GLOBAL, nameIdx, line);
}
```

### 3. `var` 声明与 `:=` 短声明

```c
static void compileVarDecl(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  if (n->var_decl.init) {
    compileExpr(c, n->var_decl.init);
  } else {
    emit(c, OP_NIL, line);  // 零值初始化
  }

  if (c->isFunction) {
    // 函数内：局部变量
    int slot = declareLocal(c, n->var_decl.name, n->var_decl.nameLen);
    markInitialized(c);
    // 值已在栈顶，对应 locals[slot]（调用帧的栈槽）
    (void)slot;  // slot 即当前 localCount-1，与栈顶对应
  } else {
    // 顶层全局：OP_SET_GLOBAL
    uint16_t nameIdx = addStringConst(c, n->var_decl.name, n->var_decl.nameLen);
    emitOp16(c->chunk, OP_SET_GLOBAL, nameIdx, line);
    emit(c, OP_POP, line);
  }
}
```

### 4. 普通赋值（`ND_ASSIGN`）

```c
static void compileAssign(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  MsNode* target = n->assign.target;
  compileExpr(c, n->assign.value);

  switch (target->kind) {
  case ND_IDENT:
    emitSetVar(c, target->ident.name, target->ident.len, line);
    break;
  case ND_ATTR:
    compileExpr(c, target->attr.obj);
    uint16_t nameIdx = addStringConst(c, target->attr.name, target->attr.nameLen);
    emitOp16(c->chunk, OP_SET_ATTR, nameIdx, line);
    break;
  case ND_INDEX:
    compileExpr(c, target->index.obj);
    compileExpr(c, target->index.key);
    emit(c, OP_SET_INDEX, line);
    break;
  case ND_TUPLE:
    // 解包赋值：value 应为 tuple/list，用 OP_UNPACK
    {
      int count = 0;
      for (MsNodeList* l = target->container.elems; l; l = l->next) count++;
      emitOp8(c->chunk, OP_UNPACK, (uint8_t)count, line);
      // OP_UNPACK 将栈顶解包为 count 个值（从右向左），按逆序 SET
      // 实际实现：UNPACK 解包后，逐个 SET（T051 VM 中实现）
      // 这里反向 emit SET：
      MsNodeList* targets[256]; int i = 0;
      for (MsNodeList* l = target->container.elems; l; l = l->next) targets[i++] = l;
      for (int j = i - 1; j >= 0; j--) {
        emitSetVarNode(c, targets[j]->node, line);
      }
    }
    break;
  default:
    compilerError(c, n->pos, "invalid assignment target");
  }
  // 赋值是语句，emit POP（赋值不产生值）
  emit(c, OP_POP, line);
}
```

### 5. 复合赋值（`ND_COMPOUND_ASSIGN`）

```c
static void compileCompoundAssign(MsCompiler* c, MsNode* n) {
  // 编译为：target = target op rhs
  // 先 load target，再 compile rhs，emit op，再 store target
  MsNode* target = n->binary.left;
  emitLoadVar(c, target);        // 读取旧值
  compileExpr(c, n->binary.right);
  uint32_t line = n->pos.line;
  // emit 对应操作码
  MsOpCode op = compoundOpToOpCode(n->binary.op);
  emit(c, op, line);
  // store 新值
  emitStoreVar(c, target, line);
  emit(c, OP_POP, line);
}
```

### 6. `del` 编译

```c
static void compileDel(MsCompiler* c, MsNode* n) {
  MsNode* target = n->single_expr.expr;
  uint32_t line  = n->pos.line;
  switch (target->kind) {
  case ND_IDENT: {
    uint16_t nameIdx = addStringConst(c, target->ident.name, target->ident.len);
    emitOp16(c->chunk, OP_DEL_GLOBAL, nameIdx, line);  // 局部变量 del → 复杂，初版仅支持全局
    break;
  }
  case ND_ATTR:
    compileExpr(c, target->attr.obj);
    uint16_t nameIdx = addStringConst(c, target->attr.name, target->attr.nameLen);
    emitOp16(c->chunk, OP_DEL_ATTR, nameIdx, line);
    break;
  case ND_INDEX:
    compileExpr(c, target->index.obj);
    compileExpr(c, target->index.key);
    emit(c, OP_DEL_INDEX, line);
    break;
  default:
    compilerError(c, n->pos, "invalid del target");
  }
}
```

---

## 验收标准（checklist）

- [ ] `"var x = 42"` 在函数内 → `OP_CONST(42)`；`x` 被注册为 `local[0]`。
- [ ] `"x"` 在函数内（`x` 为 local[0]）→ `OP_GET_LOCAL(0)`。
- [ ] `"x = 1"` 在函数内 → `OP_CONST(1)`, `OP_SET_LOCAL(0)`, `OP_POP`。
- [ ] `"x"` 在顶层（`x` 未声明为局部）→ `OP_GET_GLOBAL(<nameIdx>)`。
- [ ] `"x += 5"` → 等价于 `x = x + 5` 的字节码。
- [ ] `"a, b = 1, 2"` → `OP_CONST(1)`, `OP_CONST(2)`, `OP_BUILD_TUPLE(2)`, `OP_UNPACK(2)`, `SET_LOCAL(b)`, `SET_LOCAL(a)`（反向）。
- [ ] upvalue：外层函数局部 `x`，内层函数访问 → `OP_GET_UPVALUE(0)`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_variables.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

static bool chunkHasOp(MsChunk* ck, MsOpCode op) {
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == op) return true;
  }
  return false;
}

static void testGlobalVar(void) {
  MsCompileResult r = msCompile("x = 42", 6, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  MS_ASSERT_TRUE(chunkHasOp(r.chunk, OP_SET_GLOBAL), "has SET_GLOBAL");
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testGlobalVar);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// 局部变量
func demo() {
    x := 10
    y := x + 5
    return y
}
print(demo())   // 15

// 闭包 upvalue
func makeCounter() {
    count := 0
    return func() {
        count += 1
        return count
    }
}
c := makeCounter()
print(c())  // 1
print(c())  // 2
print(c())  // 3

// 解包赋值
a, b := 1, 2
print(a, b)   // 1 2
a, b = b, a   // 交换
print(a, b)   // 2 1

// del
d := {"k": 1}
del d["k"]
print(d)   // {}
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **局部变量 del**：`del x`（局部变量）语义为"将 x 设为未定义"，类 Python 行为；初版简化：仅支持全局变量 `del`，局部变量 `del` 报编译错误（"cannot del local variable in current version"）。
- **解包赋值右侧求值顺序**：`a, b = b, a` 中右侧先完整求值（构建 tuple），再解包赋值，无临时变量问题（字节码自然正确）。
- **全局 vs 模块作用域**：顶层变量在单文件脚本中视为模块全局；当模块系统（T086）引入后，`OP_GET_GLOBAL` 将查询 `MsModule.globals` 表而非进程全局表。
