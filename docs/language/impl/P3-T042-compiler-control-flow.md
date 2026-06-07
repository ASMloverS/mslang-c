# P3-T042 控制流 + 跳转回填（if / for / switch / break / continue）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `ND_IF`/`ND_FOR`/`ND_SWITCH`/`ND_BREAK`/`ND_CONTINUE` 的字节码编译，核心技术是**跳转回填**（先 emit 占位跳转，执行完 then/body 后回填真实偏移）和循环的**break/continue 补丁列表**。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T039 | `compileExpr` |
| P3-T038 | `MsCompiler` scope |
| P2-T027 ~ T030 | `ND_IF`/`ND_FOR`/`ND_SWITCH`/`ND_BREAK`/`ND_CONTINUE` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §2 跳转指令（JUMP/JUMP_FALSE/LOOP/FOR_ITER） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/compiler/ms_compiler.c   # compileIf / compileFor / compileSwitch / compileStmt
```

---

## 实现要点

### 1. 跳转辅助

```c
// emit 占位跳转（2 字节偏移为 0xFFFF），返回需回填的偏移位置
static uint32_t emitJump(MsCompiler* c, MsOpCode op, uint32_t line) {
  emit(c, op, line);
  emit(c, 0xFF, line);  // 高字节
  emit(c, 0xFF, line);  // 低字节
  return c->chunk->codeLen - 2;  // 返回需回填的位置
}

// 回填跳转目标为当前位置
static void patchJump(MsCompiler* c, uint32_t patchOffset) {
  uint32_t target = c->chunk->codeLen;
  uint32_t dist   = target - patchOffset - 2;  // 相对偏移
  if (dist > 0xFFFF) compilerError(c, ..., "jump too far");
  c->chunk->code[patchOffset]     = (uint8_t)(dist >> 8);
  c->chunk->code[patchOffset + 1] = (uint8_t)(dist & 0xFF);
}

// emit 向后循环跳转（LOOP，偏移为负）
static void emitLoop(MsCompiler* c, uint32_t loopStart, uint32_t line) {
  emit(c, OP_LOOP, line);
  uint32_t dist = c->chunk->codeLen - loopStart + 2;
  if (dist > 0xFFFF) compilerError(c, ..., "loop too large");
  emit(c, (uint8_t)(dist >> 8), line);
  emit(c, (uint8_t)(dist & 0xFF), line);
}
```

### 2. break/continue 补丁列表

```c
// 编译器中维护循环上下文
typedef struct MsLoopCtx {
  uint32_t breakPatches[256];    // break 跳转的回填位置
  int      breakCount;
  uint32_t continuePatches[256]; // continue 跳转的回填位置
  int      continueCount;
  uint32_t loopStart;            // 循环开始位置（continue 跳回）
  struct MsLoopCtx* outer;       // 外层循环上下文
} MsLoopCtx;
```

### 3. if 编译

```c
static void compileIf(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  compileExpr(c, n->if_stmt.cond);

  uint32_t jumpFalse = emitJump(c, OP_POP_JUMP_FALSE, line);
  // then block
  compileBlock(c, n->if_stmt.then_block);

  if (n->if_stmt.else_block) {
    uint32_t jumpEnd = emitJump(c, OP_JUMP, line);
    patchJump(c, jumpFalse);
    compileStmt(c, n->if_stmt.else_block);  // else 或 else-if
    patchJump(c, jumpEnd);
  } else {
    patchJump(c, jumpFalse);
  }
}
```

### 4. for 编译（三形式）

```c
static void compileFor(MsCompiler* c, MsNode* n, MsLoopCtx* loop) {
  uint32_t line = n->pos.line;

  if (n->for_stmt.for_target != NULL) {
    // for-in 迭代
    compileExpr(c, n->for_stmt.for_iter);
    emit(c, OP_GET_ITER, line);              // 将 iterable 转为迭代器

    loop->loopStart = c->chunk->codeLen;
    uint32_t exitJump = emitJump(c, OP_FOR_ITER, line);  // 取下一个，若耗尽跳出

    // 迭代变量绑定（OP_UNPACK 或直接 SET_LOCAL）
    compileBind(c, n->for_stmt.for_target, line);

    compileBlock(c, n->for_stmt.body);

    // continue 回填到 loopStart
    for (int i = 0; i < loop->continueCount; i++)
      patchJumpTo(c, loop->continuePatches[i], loop->loopStart);
    loop->continueCount = 0;

    emitLoop(c, loop->loopStart, line);  // 回到迭代器取下一个
    patchJump(c, exitJump);              // 耗尽时跳到此处

  } else if (n->for_stmt.cond != NULL) {
    // 条件循环（while）
    loop->loopStart = c->chunk->codeLen;
    compileExpr(c, n->for_stmt.cond);
    uint32_t exitJump = emitJump(c, OP_POP_JUMP_FALSE, line);

    compileBlock(c, n->for_stmt.body);

    for (int i = 0; i < loop->continueCount; i++)
      patchJumpTo(c, loop->continuePatches[i], loop->loopStart);
    loop->continueCount = 0;

    emitLoop(c, loop->loopStart, line);
    patchJump(c, exitJump);

  } else {
    // 无限循环
    loop->loopStart = c->chunk->codeLen;
    compileBlock(c, n->for_stmt.body);

    for (int i = 0; i < loop->continueCount; i++)
      patchJumpTo(c, loop->continuePatches[i], loop->loopStart);
    loop->continueCount = 0;

    emitLoop(c, loop->loopStart, line);
    // no exit jump（只能靠 break）
  }

  // break 回填到此处（循环结束位置）
  for (int i = 0; i < loop->breakCount; i++)
    patchJump(c, loop->breakPatches[i]);
  loop->breakCount = 0;
}
```

### 5. break / continue 编译

```c
static void compileBreak(MsCompiler* c, MsNode* n, MsLoopCtx* loop) {
  if (!loop) { compilerError(c, n->pos, "break outside loop"); return; }
  // 清理当前作用域的局部变量（scopeEnd）
  uint32_t patch = emitJump(c, OP_JUMP, n->pos.line);
  loop->breakPatches[loop->breakCount++] = patch;
}
static void compileContinue(MsCompiler* c, MsNode* n, MsLoopCtx* loop) {
  if (!loop) { compilerError(c, n->pos, "continue outside loop"); return; }
  uint32_t patch = emitJump(c, OP_JUMP, n->pos.line);
  loop->continuePatches[loop->continueCount++] = patch;
  // 实际跳转目标在 compileFor 最终回填（patchJumpTo(c, patch, loopStart)）
}
```

### 6. switch 编译

```c
static void compileSwitch(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  bool hasExpr = (n->switch_stmt.expr != NULL);

  if (hasExpr) compileExpr(c, n->switch_stmt.expr);  // switch 目标值在栈顶

  MsNodeList* cases = n->switch_stmt.cases;
  uint32_t breakPatches[256]; int breakCount = 0;
  uint32_t defaultPatch = UINT32_MAX;

  for (MsNodeList* cl = cases; cl; cl = cl->next) {
    MsNode* caseNode = cl->node;
    MsNodeList* values = caseNode->switch_case.values;
    MsNode*     body   = caseNode->switch_case.body;

    if (values == NULL) {
      // default case：先跳过，最后处理
      defaultPatch = emitJump(c, OP_JUMP, line);  // 占位
      // 记录 default 位置：在遍历结束后回填
      continue;
    }

    // 多值 case：逐个比较（OR 语义）
    uint32_t matchPatches[256]; int matchCount = 0;
    for (MsNodeList* vl = values; vl; vl = vl->next) {
      if (hasExpr) emit(c, OP_DUP, line);  // 复制 switch 表达式值（OP_DUP 需追加）
      compileExpr(c, vl->node);
      emit(c, OP_EQ, line);
      uint32_t p = emitJump(c, OP_POP_JUMP_TRUE, line);  // 匹配则跳到 body
      matchPatches[matchCount++] = p;
    }
    // 无匹配时跳过 body
    uint32_t skipBody = emitJump(c, OP_JUMP, line);

    // body
    for (int i = 0; i < matchCount; i++) patchJump(c, matchPatches[i]);
    if (hasExpr) emit(c, OP_POP, line);  // 弹出 switch 表达式值
    compileBlock(c, body);

    // break 处理
    uint32_t endPatch = emitJump(c, OP_JUMP, line);
    breakPatches[breakCount++] = endPatch;

    patchJump(c, skipBody);
  }

  // default body
  if (defaultPatch != UINT32_MAX) {
    patchJump(c, defaultPatch);
    // 找 default case 并编译 body
    // ...
  }

  // break 回填
  for (int i = 0; i < breakCount; i++) patchJump(c, breakPatches[i]);
  if (hasExpr) emit(c, OP_POP, line);  // 弹出 switch 表达式（若未弹）
}
```

---

## 验收标准（checklist）

- [ ] `"if x { a }"` → `POP_JUMP_FALSE` 正确跳过 body。
- [ ] `"if x { a } else { b }"` → 含 `JUMP`（跳过 else）+ 条件跳转正确。
- [ ] `"for { break }"` → 无限循环，break 的 `JUMP` 回填到循环结束。
- [ ] `"for i in [1,2,3] { }"` → `GET_ITER`, `FOR_ITER(+N)`, `LOOP(-M)` 结构正确。
- [ ] `"for x > 0 { x -= 1 }"` → 条件检查在循环头，`POP_JUMP_FALSE` 跳出。
- [ ] `"for i in range(3) { if i==1 { continue } print(i) }"` → continue 跳回循环头。
- [ ] `"switch x { case 1: a case 2: b }"` → 正确多分支跳转。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_control_flow.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

static void testIfCompile(void) {
  MsCompileResult r = msCompile("if x { pass }", 13, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  bool hasCond = false;
  for (uint32_t i = 0; i < r.chunk->codeLen; i++)
    if (r.chunk->code[i] == OP_POP_JUMP_FALSE) hasCond = true;
  MS_ASSERT_TRUE(hasCond, "has POP_JUMP_FALSE");
  msCompileResultFree(&r);
}

static void testForIn(void) {
  MsCompileResult r = msCompile("for i in [1,2] { pass }", 23, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  bool hasIter = false, hasForIter = false, hasLoop = false;
  for (uint32_t i = 0; i < r.chunk->codeLen; i++) {
    if (r.chunk->code[i] == OP_GET_ITER)  hasIter    = true;
    if (r.chunk->code[i] == OP_FOR_ITER)  hasForIter = true;
    if (r.chunk->code[i] == OP_LOOP)      hasLoop    = true;
  }
  MS_ASSERT_TRUE(hasIter && hasForIter && hasLoop, "for-in structure");
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testIfCompile);
  MS_RUN(testForIn);
  return msTestSummary();
}
```

### .ms 使用示例（T067 后验证）

```ms
// if/else
x := 5
if x > 0 {
    print("positive")
} else {
    print("non-positive")
}
// positive

// for-in + break/continue
sum := 0
for i in range(10) {
    if i % 2 == 0 { continue }
    if i > 7 { break }
    sum += i
}
print(sum)  // 1+3+5+7 = 16

// switch
switch x {
case 1: print("one")
case 5: print("five")   // 匹配
default: print("other")
}
// five
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **`OP_DUP`**：switch 编译需要复制 switch 表达式值（用于多次比较），需在操作码枚举中追加 `OP_DUP`。
- **fallthrough 编译**：`ND_FALLTHROUGH` 直接跳到下一 case 的 body 起始（不经过条件检查），需在 case 间记录 body 起始地址并回填。初版可简化：fallthrough 跳到下一 case body（无条件 JUMP）。
- **嵌套 break/continue**：嵌套循环中 break/continue 只作用于最近一层；通过 `MsLoopCtx` 链实现，内层覆盖外层上下文。
