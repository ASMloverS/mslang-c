# P3-T039 表达式编译（算术 / 比较 / 短路逻辑）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 AST 表达式节点到字节码的编译，覆盖：字面量（int/float/string/bool/nil）、一元/二元算术与位运算、比较运算、短路逻辑（`and`/`or`）、以及 f-string 内插编译。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T037 | `MsChunk` emit API |
| P3-T038 | `MsCompiler` / 符号表 |
| P2-T019 | `ND_UNARY`/`ND_BINARY`/`ND_FSTRING` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3 指令集（算术/比较/逻辑指令）、§9 opcode 命名映射 |
| `syntax.md` | §1.8.1 f-string 语义糖 |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
src/compiler/ms_compiler.c      # 编译器主体（compileExpr 等）
include/mslang/ms_compiler.h    # 公共 API
```

---

## 实现要点

### 1. 编译器顶层 API

```c
// include/mslang/ms_compiler.h
typedef struct MsCompileResult {
  MsChunk*   chunk;    // 编译产出的顶层 chunk
  bool       hadError;
  char       errBuf[256];
} MsCompileResult;

MsCompileResult msCompile(const char* src, uint32_t srcLen, const char* fileName);
void            msCompileResultFree(MsCompileResult* r);
```

### 1.1 编译器错误上报

`MsCompiler`（T038）需持有一个指向调用方 `MsCompileResult` 的指针字段 `result`，以便内部函数直接回填错误状态。`compilerError` 定义在 `src/compiler/ms_compiler.c`：

```c
static void compilerError(MsCompiler* c, MsPos pos,
                          const char* fmt, ...) {
  if (c->result->hadError) return;  // 仅记录首个错误
  c->result->hadError = true;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(c->result->errBuf, sizeof(c->result->errBuf), fmt, ap);
  va_end(ap);
}
```

### 2. `compileExpr(compiler, node)` 骨架

```c
static void compileExpr(MsCompiler* c, MsNode* node) {
  if (!node) return;
  switch (node->kind) {
    case MS_ND_INT:
      compileInt(c, node);
      break;
    case MS_ND_FLOAT:
      compileFloat(c, node);
      break;
    case MS_ND_STRING:
      compileString(c, node);
      break;
    case MS_ND_BYTES:
      compileBytes(c, node);  // 见 §3 说明
      break;
    case MS_ND_BOOL:
      compileBool(c, node);
      break;
    case MS_ND_NIL:
      msChunkEmitOp(c->chunk, OP_CONST_NIL, node->pos.line);
      break;
    case MS_ND_IDENT:
      compileIdent(c, node);
      break;
    case MS_ND_UNARY:
      compileUnary(c, node);
      break;
    case MS_ND_BINARY:
      compileBinary(c, node);
      break;
    case MS_ND_FSTRING:
      compileFString(c, node);
      break;
    // … T040–T047 中注册其余 case
    default:
      compilerError(c, node->pos, "cannot compile expression kind %d",
                    node->kind);
      break;
  }
}
```

### 3. 字面量编译

```c
static void compileInt(MsCompiler* c, MsNode* n) {
  MsValue v = MS_INT_VAL(n->litInt.ival);
  uint32_t idx = msChunkAddConst(c->chunk, v);
  msChunkEmitOpAX(c->chunk, OP_CONST, idx, n->pos.line);
}
// compileBool → emit OP_CONST_TRUE / OP_CONST_FALSE（vm.md §3.1）
// MS_ND_NIL   → msChunkEmitOp(c->chunk, OP_CONST_NIL, line)（骨架已处理）
// compileFloat → MS_FLOAT_VAL(fval) → msChunkAddConst → msChunkEmitOpAX OP_CONST
// compileString → msNewStr(...) → MS_OBJ_VAL → msChunkAddConst → msChunkEmitOpAX
//   注意：T049 之前无 msNewStr，先 stub（占位 NULL 指针，T049 替换）
// compileBytes → 同 compileString，将 bytes 对象入常量池后 msChunkEmitOpAX OP_CONST
```

### 4. 一元运算

```c
static void compileUnary(MsCompiler* c, MsNode* n) {
  compileExpr(c, n->unary.operand);
  uint32_t line = n->pos.line;
  switch (n->unary.op) {
    case MS_TOK_MINUS:
      msChunkEmitOp(c->chunk, OP_NEG,  line);
      break;
    case MS_TOK_NOT:
      msChunkEmitOp(c->chunk, OP_NOT,  line);
      break;
    case MS_TOK_TILDE:
      msChunkEmitOp(c->chunk, OP_BNOT, line);
      break;
    case MS_TOK_PLUS:
      break;  // +x 无操作
    default:
      compilerError(c, n->pos, "unknown unary op");
      break;
  }
}
```

### 5. 二元运算（含短路）

```c
static void compileBinary(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  // 短路逻辑（and/or）特殊处理
  // OP_AND_JMP：栈顶为假则跳（保留栈顶值），为真则弹出继续求右侧
  // OP_OR_JMP ：栈顶为真则跳（保留栈顶值），为假则弹出继续求右侧
  if (n->binary.op == MS_TOK_AND) {
    compileExpr(c, n->binary.left);
    uint32_t jumpAnd = emitJump(c, OP_AND_JMP, line);
    compileExpr(c, n->binary.right);
    patchJump(c, jumpAnd);
    return;
  }
  if (n->binary.op == MS_TOK_OR) {
    compileExpr(c, n->binary.left);
    uint32_t jumpOr = emitJump(c, OP_OR_JMP, line);
    compileExpr(c, n->binary.right);
    patchJump(c, jumpOr);
    return;
  }

  // 普通二元：先编译左右，再 emit 操作码
  compileExpr(c, n->binary.left);
  compileExpr(c, n->binary.right);
  switch (n->binary.op) {
    case MS_TOK_PLUS:
      msChunkEmitOp(c->chunk, OP_ADD,    line); break;
    case MS_TOK_MINUS:
      msChunkEmitOp(c->chunk, OP_SUB,    line); break;
    case MS_TOK_STAR:
      msChunkEmitOp(c->chunk, OP_MUL,    line); break;
    case MS_TOK_SLASH:
      msChunkEmitOp(c->chunk, OP_DIV,    line); break;
    case MS_TOK_PERCENT:
      msChunkEmitOp(c->chunk, OP_MOD,    line); break;
    case MS_TOK_STAR_STAR:
      msChunkEmitOp(c->chunk, OP_POW,    line); break;
    case MS_TOK_SHL:
      msChunkEmitOp(c->chunk, OP_SHL,    line); break;
    case MS_TOK_SHR:
      msChunkEmitOp(c->chunk, OP_SHR,    line); break;
    case MS_TOK_AMP:
      msChunkEmitOp(c->chunk, OP_BAND,   line); break;
    case MS_TOK_PIPE:
      msChunkEmitOp(c->chunk, OP_BOR,    line); break;
    case MS_TOK_CARET:
      msChunkEmitOp(c->chunk, OP_BXOR,   line); break;
    case MS_TOK_EQ_EQ:
      msChunkEmitOp(c->chunk, OP_EQ,     line); break;
    case MS_TOK_NEQ:
      msChunkEmitOp(c->chunk, OP_NE,     line); break;
    case MS_TOK_LT:
      msChunkEmitOp(c->chunk, OP_LT,     line); break;
    case MS_TOK_GT:
      msChunkEmitOp(c->chunk, OP_GT,     line); break;
    case MS_TOK_LE:
      msChunkEmitOp(c->chunk, OP_LE,     line); break;
    case MS_TOK_GE:
      msChunkEmitOp(c->chunk, OP_GE,     line); break;
    case MS_TOK_IS:
      msChunkEmitOp(c->chunk, OP_IS,     line); break;
    case MS_TOK_IN:
      msChunkEmitOp(c->chunk, OP_IN,     line); break;
    // MS_TOK_IS_NOT / MS_TOK_NOT_IN（虚拟 token，vm.md §9 扩展项）
    // 语义：OP_IS / OP_IN 结果取反，VM 实现时在同一帧内处理
    case MS_TOK_IS_NOT:
      msChunkEmitOp(c->chunk, OP_IS_NOT, line); break;
    case MS_TOK_NOT_IN:
      msChunkEmitOp(c->chunk, OP_NOT_IN, line); break;
    default:
      compilerError(c, n->pos, "unknown binary op");
      break;
  }
}
```

### 6. f-string 编译

```c
// syntax.md §1.8.1：f-string 语义糖——{expr} 等价于 str(expr)；文本片段已是字符串，无需转换。
// OP_TO_STR 与 OP_BUILD_STR 须在 vm.md §3/§9 正式登记后方可使用。
static void compileFString(MsCompiler* c, MsNode* n) {
  // parts：MS_ND_STRING（文本片段）和任意表达式节点的混合列表
  uint32_t partCount = 0;
  for (MsNodeList* l = n->fstring.parts; l; l = l->next) {
    compileExpr(c, l->node);
    // 仅对表达式片段 emit OP_TO_STR；文本片段已是字符串，跳过
    if (l->node->kind != MS_ND_STRING) {
      msChunkEmitOp(c->chunk, OP_TO_STR, n->pos.line);
    }
    partCount++;
  }
  // OP_BUILD_STR 操作数为单字节（A），拼接栈顶 partCount 个字符串
  msChunkEmitOpA(c->chunk, OP_BUILD_STR, (uint8_t)partCount, n->pos.line);
}
// 待办：在 vm.md §3 新增 OP_TO_STR（值→字符串）与 OP_BUILD_STR（拼接 N 个字符串）条目
```

---

## 验收标准（checklist）

- [ ] `compileExpr(MS_ND_INT(42))` → `chunk.code = [OP_CONST, 0, 0, 0]`（3 字节 AX），`chunk.consts[0] = MS_INT_VAL(42)`。<!-- v:ctest:T039_int_literal -->
- [ ] `compileExpr(MS_ND_BINARY(+, MS_ND_INT(1), MS_ND_INT(2)))` → `[OP_CONST, …, OP_CONST, …, OP_ADD]`。<!-- v:ctest:T039_addition -->
- [ ] `compileExpr(MS_ND_BINARY(AND, a, b))` → 生成 `OP_AND_JMP`（保留栈顶值短路跳转），为真时弹出左侧继续求右侧。<!-- v:ctest:T039_and_shortcircuit -->
- [ ] `compileExpr(MS_ND_BINARY(OR, a, b))` → `OP_OR_JMP` 短路正确，`1 or 2` 返回 `1`。<!-- v:ctest:T039_or_shortcircuit -->
- [ ] `compileExpr(MS_ND_UNARY(NOT, x))` → `[… OP_NOT]`。<!-- v:ctest:T039_unary_not -->
- [ ] `compileExpr(MS_ND_NIL)` → `[OP_CONST_NIL]`。<!-- v:ctest:T039_nil -->
- [ ] `compileExpr(MS_ND_BOOL(true))` → `[OP_CONST_TRUE]`。<!-- v:ctest:T039_bool_true -->

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_expr_compile.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

// 辅助：编译字符串，返回 chunk（caller 负责 free）
static MsChunk* compileStr(const char* src) {
  MsCompileResult r = msCompile(src, (uint32_t)strlen(src), "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no compile error");
  return r.chunk;
}

static void testIntLiteral(void) {
  MsChunk* ck = compileStr("42");
  // 顶层脚本：OP_CONST 42, OP_POP（表达式语句），OP_RETURN
  MS_ASSERT_EQ(ck->code[0], OP_CONST, "CONST");
  MS_ASSERT_EQ(ck->constLen, 1, "1 const");
  msChunkFree(ck);
}

static void testAddition(void) {
  MsChunk* ck = compileStr("1 + 2");
  // 期望：OP_CONST(1), OP_CONST(2), OP_ADD, OP_POP, OP_RETURN
  bool hasAdd = false;
  for (uint32_t i = 0; i < ck->codeLen; i++) {
    if (ck->code[i] == OP_ADD) hasAdd = true;
  }
  MS_ASSERT_TRUE(hasAdd, "has OP_ADD");
  msChunkFree(ck);
}

int main(void) {
  MS_RUN(testIntLiteral);
  MS_RUN(testAddition);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// 算术
print(1 + 2 * 3)     // 7
print(2 ** 10)       // 1024
print(-5)            // -5
print(~3)            // -4

// 比较
print(1 < 2)         // true
print("a" == "a")    // true

// 短路
x := 0
print(false and (x := 1))  // false（x 仍为 0）
print(x)                   // 0
print(true or (x := 2))    // true（x 仍为 0）

// f-string
name := "world"
print($"hello {name}!")  // hello world!
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **字符串常量生命周期**：`compileString` 需创建 `MsStr` 对象并放入常量池；T049 之前 GC 未实现，使用 `msAlloc` 直接分配（不被 GC 管理），T050 之后升级。
- **f-string 新增指令**：`OP_TO_STR`（将栈顶值转换为字符串）和 `OP_BUILD_STR`（拼接栈顶 N 个字符串）须在 vm.md §3/§9 正式登记后，再写入 T037 的操作码枚举，方可使用。文本片段（`MS_ND_STRING`）不 emit `OP_TO_STR`，仅表达式片段需要。
- **短路逻辑值语义**：`and`/`or` 返回**操作数本身**（非 bool），与 Python 相同：`1 and 2` → `2`，`nil or 3` → `3`。VM（T051）中 `OP_AND_JMP`/`OP_OR_JMP` 在栈顶为假/真时**保留栈顶值并跳转**，为真/假时弹出栈顶继续求右侧；二者均不使用 `OP_POP` 辅助弹出（vm.md §3.4）。
