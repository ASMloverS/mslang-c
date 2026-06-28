# P3-T043 函数编译 + MAKE_CLOSURE / upvalue 解析

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `ND_FUNC_DECL`/`ND_ASYNC_FUNC` 节点的字节码编译：为函数体创建新 `MsChunk`、递归编译函数体、解析 upvalue 引用，并 emit `OP_MAKE_FUNC` 指令（在 VM 运行时将 chunk 包装为闭包对象）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T038 | `MsCompiler` / `resolveUpvalue` |
| P3-T042 | 控制流编译 |
| P2-T024 | `ND_FUNC_DECL` 节点（含 params/body） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3 调用帧 / `MsFunc` 对象布局 |
| `vm.md` | §3.3 upvalue open/close 语义 |
| `vm.md` | §4 `OP_MAKE_FUNC` / `OP_MAKE_CLOSURE` 编码 |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/compiler/ms_compiler.c   # compileFuncDecl / compileParams / compileReturn
```

---

## 实现要点

### 1. 函数编译总体流程

```c
static void compileFuncDecl(MsCompiler* outer, MsNode* n) {
  // 1. 创建子编译器（新 chunk）
  MsChunk* funcChunk = msAlloc(sizeof(*funcChunk));
  msChunkInit(funcChunk, outer->chunk->sourceName);
  MsCompiler funcC;
  compilerInit(&funcC, outer, funcChunk, true);

  // 2. 将 self（第一个参数）或所有参数注册为局部变量槽
  uint32_t line = n->pos.line;
  for (MsNodeList* l = n->funcDecl.params; l; l = l->next) {
    MsNode* param = l->node;
    if (param->kind == ND_PARAM) {
      declareLocal(&funcC, param->param.name, param->param.nameLen);
      markInitialized(&funcC);
      // 默认值在 caller 处理（T045 调用编译）
    }
  }

  // 3. 编译函数体
  compileBlock(&funcC, n->funcDecl.body);

  // 4. 末尾隐式 return nil（OP_RETURN_NIL 等价于 OP_CONST_NIL + OP_RETURN）
  emit(funcC.chunk, OP_RETURN_NIL, line);

  // 5. 将函数 chunk 包装为常量，放入外层常量池
  //    MsValue funcVal = msNewFuncProto(funcChunk, paramCount, funcName);
  //    uint32_t funcIdx = msChunkAddConst(outer->chunk, funcVal);
  //
  //    注：T049 之前无 msNewFuncProto，先使用裸指针（不被 GC 管理）
  uint32_t funcIdx = addFuncProtoConst(outer, funcChunk, n);

  // 6. emit MAKE_FUNC（若有 upvalue 则为 MAKE_CLOSURE）
  int upvalueCount = funcC.upvalueCount;
  emitAX(outer->chunk, OP_MAKE_FUNC, funcIdx, line);
  emit(outer->chunk, (uint8_t)upvalueCount, line);
  // 每个 upvalue 的描述（isLocal + index）
  for (int i = 0; i < upvalueCount; i++) {
    emit(outer->chunk, funcC.upvalues[i].isLocal ? 1 : 0, line);
    emit(outer->chunk, funcC.upvalues[i].index, line);
  }

  // 7. 若是命名函数（非匿名），emit SET_LOCAL/SET_GLOBAL 绑定名称
  if (n->funcDecl.name) {
    emitSetVar(outer, n->funcDecl.name,
                   (uint32_t)strlen(n->funcDecl.name), line);
    emit(outer->chunk, OP_POP, line);
  }

  compilerFree(&funcC);
}
```

### 2. `OP_MAKE_FUNC` 编码规范

```
OP_MAKE_FUNC  [3B: funcIdx]  [1B: upvalueCount]
    for each upvalue:
        [1B: isLocal]  [1B: index]
```

- `funcIdx`：外层常量池中指向 `MsFuncProto`（函数原型，含 chunk 指针、参数数量、名称等）的索引。
- VM 在执行 `OP_MAKE_FUNC` 时，从常量池取出原型，捕获 upvalue（open/close），创建 `MsFunc` 闭包对象，压栈。

### 3. 参数默认值

默认值表达式在**定义时**编译到外层函数的 chunk 中（非函数体 chunk），在 `OP_MAKE_FUNC` 之前压栈，VM 在构建调用帧时将默认值绑定到参数槽。

```c
// 在 compileFuncDecl 中，step 5 之前：
for (MsNodeList* l = params; l; l = l->next) {
  MsNode* param = l->node;
  if (param->param.defaultVal) {
    compileExpr(outer, param->param.defaultVal);  // 在外层编译默认值
  }
}
// OP_MAKE_FUNC 的扩展编码中包含 default_count
```

初版简化：默认值在 VM 调用时通过常量池查找（将默认值预编译到常量）。

### 4. `return` 编译

```c
static void compileReturn(MsCompiler* c, MsNode* n) {
  if (!c->isFunction) {
    compilerError(c, n->pos, "return outside function");
    return;
  }
  if (n->singleExpr.expr) {
    compileExpr(c, n->singleExpr.expr);
  } else {
    emit(c, OP_CONST_NIL, n->pos.line);
  }
  // 关闭所有开放 upvalue
  // （scopeEnd 在 compileBlock 中处理，return 前需手动 close 未关闭的 upvalue）
  emit(c, OP_RETURN, n->pos.line);
}
```

### 5. async func

`ND_ASYNC_FUNC` 与 `ND_FUNC_DECL` 编译相同，但产生的 `MsFuncProto` 标记 `is_async=true`；VM 在调用 async 函数时返回 `MsFuture` 对象而非直接执行（T111 实现）。

---

## 验收标准（checklist）

- [ ] `"func f() { return 42 }"` → 外层 chunk 含 `OP_MAKE_FUNC`；内层 chunk 含 `OP_CONST(42)`, `OP_RETURN`。
- [ ] `"func f(a, b) { return a + b }"` → 内层 locals=[a,b]，`OP_GET_LOCAL(0)`, `OP_GET_LOCAL(1)`, `OP_ADD`, `OP_RETURN`。
- [ ] 函数末尾无 `return` → 隐式 `OP_RETURN_NIL`。
- [ ] 闭包捕获：外层 `x`，内层函数引用 `x` → `OP_MAKE_FUNC` 含 upvalue 描述 `[is_local=1, idx=X]`；内层 chunk 含 `OP_GET_UPVALUE(0)`。
- [ ] upvalue of upvalue：三级嵌套正确（`is_local=0`）。
- [ ] `"async func f() {}"` → proto 标记 `is_async`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_func_compile.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

static void testFuncMake(void) {
  MsCompileResult r = msCompile("func f() { return 1 }", 21, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  bool hasMakeFunc = false;
  for (uint32_t i = 0; i < r.chunk->codeLen; i++)
    if (r.chunk->code[i] == OP_MAKE_FUNC) hasMakeFunc = true;
  MS_ASSERT_TRUE(hasMakeFunc, "has MAKE_FUNC");
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testFuncMake);
  return msTestSummary();
}
```

### .ms 使用示例（T067/T071 后验证）

```ms
// 基本函数
func square(x) { return x * x }
print(square(5))   // 25


// 默认参数
func greet(name, prefix="Hello") {
    return $"{prefix}, {name}!"
}
print(greet("world"))        // Hello, world!
print(greet("you", "Hi"))    // Hi, you!


// 闭包（upvalue）
func makeMultiplier(n) {
    return func(x) { return x * n }
}
triple := makeMultiplier(3)
print(triple(7))   // 21


// 递归（函数名即 upvalue 或全局）
func fib(n) {
    if n <= 1 { return n }
    return fib(n-1) + fib(n-2)
}
print(fib(10))   // 55
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **函数 chunk 内存管理**：`MsFuncProto` 存储在常量池中（`MsValue` → `MsObj`），需 GC 管理；T050 之前使用 `msAlloc` 直接分配，不被 GC 追踪（泄漏可接受，T050 后修复）。
- **深度嵌套**：超过 255 个 upvalue 报编译错误；超过 255 个参数同样报错。
- **`return` 与 finally**：在 try/finally 内的 `return` 需先执行 finally 块再返回（T046 处理）。
