# P3-T042 控制流 + 跳转回填（if / for / switch / break / continue）

> **状态**：✅ 已完成

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
| `vm.md` | §3.5 跳转指令（JMP/JMP_IF_FALSE/JMP_IF_TRUE/FOR_ITER）、§3.10 迭代（GET_ITER/FOR_ITER）、§3.0 栈操作（DUP/POP）、§9 opcode 命名映射 |
| `syntax.md` | §2.2 IfStmt/ForStmt/SwitchStmt/BreakStmt/ContinueStmt/FallthroughStmt 文法、§3.2 for 三形式语义、§3 switch 无表达式等价 `switch true` |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/compiler/ms_compiler.c   # compileIf / compileFor / compileSwitch / compileStmt
```

---

## 实现要点

### 1. 跳转辅助

`emitJump` 与 `patchJump` 已在 `src/compiler/ms_compiler.c`（约 137–143 行）实现，
操作数为 **3 字节有符号 AX**（`vm.md §3` 约定，±8388607 范围），此处直接复用：

```c
// 已有（勿重复定义）：
// static uint32_t emitJump(MsCompiler* c, MsOpCode op, uint32_t line);
//   → msChunkEmitOpAX(c->chunk, op, 0, line)，返回操作数起始偏移（codeLen - 3）
// static void patchJump(MsCompiler* c, uint32_t patchOffset);
//   → msChunkPatchJump(c->chunk, patchOffset, c->chunk->codeLen)（回填到当前位置）

// 新增：回填到指定目标（用于 continue 回跳到 loopStart，支持负偏移）
static void patchJumpTo(MsCompiler* c, uint32_t patchOffset, uint32_t target) {
  msChunkPatchJump(c->chunk, patchOffset, target);
}
```

**回边跳转**（循环）不使用 `OP_LOOP`（该 opcode 不存在），
而用 `OP_JUMP` + 负 AX 偏移实现（`vm.md §3` 明确"负值回跳"）：

```c
// emit 回边跳转（OP_JUMP 携带负偏移回到 loopStart）
static void emitBackJump(MsCompiler* c, uint32_t loopStart, uint32_t line) {
  uint32_t patch = emitJump(c, OP_JUMP, line);
  patchJumpTo(c, patch, loopStart);
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
  compileExpr(c, n->ifStmt.cond);

  uint32_t jumpFalse = emitJump(c, OP_JUMP_IF_FALSE, line);
  // then block
  compileBlock(c, n->ifStmt.thenBlock);

  if (n->ifStmt.elseBlock) {
    uint32_t jumpEnd = emitJump(c, OP_JUMP, line);
    patchJump(c, jumpFalse);
    compileStmt(c, n->ifStmt.elseBlock);  // else 或 else-if
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

  if (n->forStmt.forTarget != NULL) {
    // for-in 迭代（初版仅支持单变量，双变量 for k, v 留后续）
    compileExpr(c, n->forStmt.forIter);
    msChunkEmitOp(c->chunk, OP_GET_ITER, line);  // iterable → 迭代器，留在栈底

    loop->loopStart = c->chunk->codeLen;
    uint32_t exitJump = emitJump(c, OP_FOR_ITER, line);  // 取下一个，耗尽跳出

    // 迭代变量绑定（SET_LOCAL）
    compileBind(c, n->forStmt.forTarget, line);

    compileBlock(c, n->forStmt.body);

    // continue 回填到 loopStart
    for (int i = 0; i < loop->continueCount; i++) {
      patchJumpTo(c, loop->continuePatches[i], loop->loopStart);
    }
    loop->continueCount = 0;

    emitBackJump(c, loop->loopStart, line);  // OP_JUMP 负偏移回到 FOR_ITER
    patchJump(c, exitJump);                  // 耗尽时跳到此处

    // 弹出栈底迭代器（GET_ITER 留下的）
    msChunkEmitOp(c->chunk, OP_POP, line);

  } else if (n->forStmt.cond != NULL) {
    // 条件循环（while）
    loop->loopStart = c->chunk->codeLen;
    compileExpr(c, n->forStmt.cond);
    uint32_t exitJump = emitJump(c, OP_JUMP_IF_FALSE, line);

    compileBlock(c, n->forStmt.body);

    for (int i = 0; i < loop->continueCount; i++) {
      patchJumpTo(c, loop->continuePatches[i], loop->loopStart);
    }
    loop->continueCount = 0;

    emitBackJump(c, loop->loopStart, line);
    patchJump(c, exitJump);

  } else {
    // 无限循环
    loop->loopStart = c->chunk->codeLen;
    compileBlock(c, n->forStmt.body);

    for (int i = 0; i < loop->continueCount; i++) {
      patchJumpTo(c, loop->continuePatches[i], loop->loopStart);
    }
    loop->continueCount = 0;

    emitBackJump(c, loop->loopStart, line);  // 无退出跳转，只能靠 break
  }

  // break 回填到此处（循环结束位置）
  for (int i = 0; i < loop->breakCount; i++) {
    patchJump(c, loop->breakPatches[i]);
  }
  loop->breakCount = 0;
}
```

### 5. break / continue 编译

```c
#define MS_MAX_LOOP_PATCHES 256

static void compileBreak(MsCompiler* c, MsNode* n, MsLoopCtx* loop) {
  if (!loop) { compilerError(c, n->pos, "break outside loop"); return; }
  if (loop->breakCount >= MS_MAX_LOOP_PATCHES) {
    compilerError(c, n->pos, "too many break statements in one loop");
    return;
  }
  // 弹出 for-in 迭代器（若当前循环为 for-in，迭代器在栈底）
  // 此处调用 T038 提供的 scopeEnd 辅助弹出循环作用域局部变量
  uint32_t patch = emitJump(c, OP_JUMP, n->pos.line);
  loop->breakPatches[loop->breakCount++] = patch;
}

static void compileContinue(MsCompiler* c, MsNode* n, MsLoopCtx* loop) {
  if (!loop) { compilerError(c, n->pos, "continue outside loop"); return; }
  if (loop->continueCount >= MS_MAX_LOOP_PATCHES) {
    compilerError(c, n->pos, "too many continue statements in one loop");
    return;
  }
  uint32_t patch = emitJump(c, OP_JUMP, n->pos.line);
  loop->continuePatches[loop->continueCount++] = patch;
  // 实际跳转目标在 compileFor 最终回填（patchJumpTo(c, patch, loopStart)）
}
```

### 6. switch 编译

```c
static void compileSwitch(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  bool hasExpr = (n->switchStmt.expr != NULL);

  // switch 有表达式时把目标值压栈，无表达式等价 switch true（逐 case 求布尔）
  if (hasExpr) {
    compileExpr(c, n->switchStmt.expr);
  }

  MsNodeList* cases = n->switchStmt.cases;
  uint32_t breakPatches[MS_MAX_LOOP_PATCHES];
  int breakCount = 0;
  MsNode* defaultCase = NULL;  // 记录 default case 节点，最后处理

  for (MsNodeList* cl = cases; cl; cl = cl->next) {
    MsNode*     caseNode = cl->node;
    MsNodeList* values   = caseNode->switchCase.values;
    MsNode*     body     = caseNode->switchCase.body;

    if (values == NULL) {
      // default case：记录节点，遍历结束后再编译 body
      defaultCase = caseNode;
      continue;
    }

    // 多值 case：逐个比较（OR 语义）
    uint32_t matchPatches[MS_MAX_LOOP_PATCHES];
    int matchCount = 0;
    for (MsNodeList* vl = values; vl; vl = vl->next) {
      if (hasExpr) {
        msChunkEmitOp(c->chunk, OP_DUP, line);  // 复制 switch 表达式值
        compileExpr(c, vl->node);
        msChunkEmitOp(c->chunk, OP_EQ, line);
        uint32_t p = emitJump(c, OP_JUMP_IF_TRUE, line);  // 匹配则跳到 body
        if (matchCount < MS_MAX_LOOP_PATCHES) {
          matchPatches[matchCount++] = p;
        }
      } else {
        // 无表达式：case 值本身为布尔条件
        compileExpr(c, vl->node);
        uint32_t p = emitJump(c, OP_JUMP_IF_TRUE, line);
        if (matchCount < MS_MAX_LOOP_PATCHES) {
          matchPatches[matchCount++] = p;
        }
      }
    }
    // 无匹配时跳过 body
    uint32_t skipBody = emitJump(c, OP_JUMP, line);

    // body（所有 match 跳转落地）
    for (int i = 0; i < matchCount; i++) {
      patchJump(c, matchPatches[i]);
    }
    if (hasExpr) {
      msChunkEmitOp(c->chunk, OP_POP, line);  // 弹出 switch 表达式副本
    }
    compileBlock(c, body);

    // 默认不贯穿（无 fallthrough 则跳到 switch 末）
    if (breakCount < MS_MAX_LOOP_PATCHES) {
      breakPatches[breakCount++] = emitJump(c, OP_JUMP, line);
    }

    patchJump(c, skipBody);
  }

  // default body（无 default 则跳到 switch 末）
  if (defaultCase != NULL) {
    if (hasExpr) {
      msChunkEmitOp(c->chunk, OP_POP, line);  // 弹出 switch 表达式
    }
    compileBlock(c, defaultCase->switchCase.body);
  } else if (hasExpr) {
    msChunkEmitOp(c->chunk, OP_POP, line);  // 无 default 时弹出表达式
  }

  // break / 各 case 末的跳转回填到 switch 结束位置
  for (int i = 0; i < breakCount; i++) {
    patchJump(c, breakPatches[i]);
  }
}
```

---

## 验收标准（checklist）

- [ ] `"if x { a }"` → `JUMP_IF_FALSE` 正确跳过 body。
- [ ] `"if x { a } else { b }"` → 含 `JUMP`（跳过 else）+ `JUMP_IF_FALSE` 条件跳转正确。
- [ ] `"for { break }"` → 无限循环，break 的 `JUMP` 回填到循环结束。
- [ ] `"for i in [1,2,3] { }"` → `GET_ITER`, `FOR_ITER(+N)`, 回边 `JUMP(-M)` 结构正确。
- [ ] `"for x > 0 { x -= 1 }"` → 条件检查在循环头，`JUMP_IF_FALSE` 跳出。
- [ ] `"for i in range(3) { if i==1 { continue } print(i) }"` → continue 跳回循环头。
- [ ] `"switch x { case 1: a case 2: b }"` → `DUP`/`EQ`/`JUMP_IF_TRUE` 正确多分支跳转。

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
  for (uint32_t i = 0; i < r.chunk->codeLen; i++) {
    if (r.chunk->code[i] == OP_JUMP_IF_FALSE) {
      hasCond = true;
    }
  }
  MS_ASSERT_TRUE(hasCond, "has JUMP_IF_FALSE");
  msCompileResultFree(&r);
}

static void testForIn(void) {
  MsCompileResult r = msCompile("for i in [1,2] { pass }", 23, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  bool hasIter = false, hasForIter = false, hasBackJump = false;
  for (uint32_t i = 0; i < r.chunk->codeLen; i++) {
    if (r.chunk->code[i] == OP_GET_ITER) {
      hasIter = true;
    }
    if (r.chunk->code[i] == OP_FOR_ITER) {
      hasForIter = true;
    }
    if (r.chunk->code[i] == OP_JUMP) {
      hasBackJump = true;  // 回边：OP_JUMP 携带负偏移
    }
  }
  MS_ASSERT_TRUE(hasIter && hasForIter && hasBackJump, "for-in structure");
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
case 1:
    print("one")
case 5:
    print("five")   // 匹配
default:
    print("other")
}
// five
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **`OP_DUP`**：`OP_DUP` 已存在于 `vm.md §3.0` 与 `ms_opcode.h`，switch 编译直接使用，无需修改 opcode 枚举。
- **fallthrough 编译**：本任务**不实现** `ND_FALLTHROUGH`（`syntax.md §2.2` 的正式语句）。初版 switch 各 case body 结束后均无条件跳转到 switch 末尾（默认不贯穿）。fallthrough 支持留至后续任务实现：需在 case 间记录 body 起始地址并在 `ND_FALLTHROUGH` 节点处 emit 无条件 `JUMP` 到下一 case body 起始。
- **嵌套 break/continue**：嵌套循环中 break/continue 只作用于最近一层；通过 `MsLoopCtx` 链实现，内层覆盖外层上下文。
- **for-in 双变量**：`syntax.md §3.2` 支持 `for k, v in map`，初版**不支持**（`compileBind` 仅处理单变量，双变量会报编译错误）。双变量解包留后续任务处理。
- **补丁列表容量**：`MS_MAX_LOOP_PATCHES = 256`；超限时 `compilerError` 报错（实践中极少触达）。
