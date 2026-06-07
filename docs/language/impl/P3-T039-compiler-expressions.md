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
| `vm.md` | §2 指令集（算术/比较/逻辑指令） |

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

### 2. `compileExpr(compiler, node)` 骨架

```c
static void compileExpr(MsCompiler* c, MsNode* node) {
  if (!node) return;
  switch (node->kind) {
  case ND_INT:    compileInt(c, node);    break;
  case ND_FLOAT:  compileFloat(c, node);  break;
  case ND_STRING: compileString(c, node); break;
  case ND_BYTES:  compileBytes(c, node);  break;
  case ND_BOOL:   compileBool(c, node);   break;
  case ND_NIL:    emit(c, OP_NIL, node->pos.line); break;
  case ND_IDENT:  compileIdent(c, node);  break;
  case ND_UNARY:  compileUnary(c, node);  break;
  case ND_BINARY: compileBinary(c, node); break;
  case ND_FSTRING:compileFString(c, node);break;
  // … T040–T047 中注册其余 case
  default:
    compilerError(c, node->pos, "cannot compile expression kind %d", node->kind);
  }
}
```

### 3. 字面量编译

```c
static void compileInt(MsCompiler* c, MsNode* n) {
  MsValue v = MS_INT_VAL(n->lit_int.ival);
  uint16_t idx = msChunkAddConst(c->chunk, v);
  emitOp16(c->chunk, OP_CONST, idx, n->pos.line);
}
// compileBool → emit OP_TRUE / OP_FALSE
// compileNil  → emit OP_NIL
// compileFloat → MS_FLOAT_VAL(fval) → OP_CONST
// compileString → msNewStr(...) → MS_OBJ_VAL → OP_CONST
//   注意：T049 之前无 msNewStr，先 stub（占位 NULL 指针，T049 替换）
```

### 4. 一元运算

```c
static void compileUnary(MsCompiler* c, MsNode* n) {
  compileExpr(c, n->unary.operand);
  uint32_t line = n->pos.line;
  switch (n->unary.op) {
  case TOK_MINUS: emit(c, OP_NEG,    line); break;
  case TOK_NOT:   emit(c, OP_NOT,    line); break;
  case TOK_TILDE: emit(c, OP_BITNOT, line); break;
  case TOK_PLUS:                              break;  // +x 无操作
  default: compilerError(c, n->pos, "unknown unary op");
  }
}
```

### 5. 二元运算（含短路）

```c
static void compileBinary(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  // 短路逻辑（and/or）特殊处理
  if (n->binary.op == TOK_AND) {
    compileExpr(c, n->binary.left);
    // 若左侧为假，跳过右侧，保留左侧值（短路）
    uint32_t jumpFalse = emitJump(c, OP_JUMP_FALSE, line);
    emit(c, OP_POP, line);             // 弹出左侧
    compileExpr(c, n->binary.right);
    patchJump(c, jumpFalse);
    return;
  }
  if (n->binary.op == TOK_OR) {
    compileExpr(c, n->binary.left);
    uint32_t jumpTrue = emitJump(c, OP_JUMP_TRUE, line);
    emit(c, OP_POP, line);
    compileExpr(c, n->binary.right);
    patchJump(c, jumpTrue);
    return;
  }

  // 普通二元：先编译左右，再 emit 操作码
  compileExpr(c, n->binary.left);
  compileExpr(c, n->binary.right);
  switch (n->binary.op) {
  case TOK_PLUS:    emit(c, OP_ADD,    line); break;
  case TOK_MINUS:   emit(c, OP_SUB,    line); break;
  case TOK_STAR:    emit(c, OP_MUL,    line); break;
  case TOK_SLASH:   emit(c, OP_DIV,    line); break;
  case TOK_PERCENT: emit(c, OP_MOD,    line); break;
  case TOK_STARSTAR:emit(c, OP_POW,    line); break;
  case TOK_SHL:     emit(c, OP_SHL,    line); break;
  case TOK_SHR:     emit(c, OP_SHR,    line); break;
  case TOK_AMP:     emit(c, OP_BITAND, line); break;
  case TOK_PIPE:    emit(c, OP_BITOR,  line); break;
  case TOK_CARET:   emit(c, OP_BITXOR, line); break;
  case TOK_EQ:      emit(c, OP_EQ,     line); break;
  case TOK_NEQ:     emit(c, OP_NEQ,    line); break;
  case TOK_LT:      emit(c, OP_LT,     line); break;
  case TOK_GT:      emit(c, OP_GT,     line); break;
  case TOK_LE:      emit(c, OP_LE,     line); break;
  case TOK_GE:      emit(c, OP_GE,     line); break;
  case TOK_IS:      emit(c, OP_IS,     line); break;
  case TOK_IN:      emit(c, OP_IN,     line); break;
  // TOK_IS_NOT / TOK_NOT_IN（虚拟 token）：
  case TOK_IS_NOT:  emit(c, OP_IS_NOT, line); break;
  case TOK_NOT_IN:  emit(c, OP_NOT_IN, line); break;
  default: compilerError(c, n->pos, "unknown binary op");
  }
}
```

### 6. f-string 编译

```c
static void compileFString(MsCompiler* c, MsNode* n) {
  // parts 是 ND_STRING（文本片段）和表达式节点的混合列表
  int partCount = 0;
  for (MsNodeList* l = n->fstring.parts; l; l = l->next) {
    compileExpr(c, l->node);  // 字符串片段或任意表达式
    // 若是表达式，需 str() 转换：emit OP_CALL(str, 1)
    // 简化：emit OP_TO_STR（新增专用指令）
    emit(c, OP_TO_STR, n->pos.line);
    partCount++;
  }
  // 拼接所有片段
  emitOp16(c->chunk, OP_BUILD_STR, (uint16_t)partCount, n->pos.line);
}
// 需追加 OP_TO_STR 和 OP_BUILD_STR 到 MsOpCode 枚举
```

---

## 验收标准（checklist）

- [ ] `compileExpr(ND_INT(42))` → `chunk.code = [OP_CONST, 0, 0]`，`chunk.consts[0] = MS_INT_VAL(42)`。
- [ ] `compileExpr(ND_BINARY(+, ND_INT(1), ND_INT(2)))` → `[OP_CONST, …, OP_CONST, …, OP_ADD]`。
- [ ] `compileExpr(ND_BINARY(AND, a, b))` → 生成正确的短路跳转（`JUMP_FALSE` + `POP` + b + patch）。
- [ ] `compileExpr(ND_BINARY(OR, a, b))` → 短路 OR 跳转正确。
- [ ] `compileExpr(ND_UNARY(NOT, x))` → `[… OP_NOT]`。
- [ ] `compileExpr(ND_NIL)` → `[OP_NIL]`。
- [ ] `compileExpr(ND_BOOL(true))` → `[OP_TRUE]`。

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
- **f-string `OP_BUILD_STR`**：需在 T037 的操作码枚举中追加 `OP_TO_STR`（值转字符串）和 `OP_BUILD_STR`（拼接 N 个字符串）。
- **短路逻辑值语义**：`and`/`or` 返回**操作数本身**（非 bool），与 Python 相同：`1 and 2` → `2`，`nil or 3` → `3`。VM（T051）中 `OP_JUMP_FALSE`/`OP_JUMP_TRUE` 基于真值测试，不改变栈顶值。
