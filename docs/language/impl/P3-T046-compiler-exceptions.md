# P3-T046 异常编译（PUSH/POP_EXCEPT / RAISE / finally 内联）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `ND_TRY`/`ND_RAISE`/`ND_ASSERT` 的字节码编译，核心是异常处理器注册（`OP_PUSH_EXCEPT`）、catch 类型匹配分支、finally 块内联（无论正常/异常路径都执行），以及重抛（`OP_RERAISE`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T042 | 跳转回填工具 |
| P2-T031 | `ND_TRY`/`ND_CATCH_CLAUSE`/`ND_RAISE` 节点 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §6 异常机制（ExceptEntry 栈 / MS_ERROR_VALUE 传播） |
| `errors.md` | §1 异常层次（BaseException / Exception / …） |

---

## 待实现（C 文件 / 结构 / 函数）

### 修改文件

```
src/compiler/ms_compiler.c   # compileTry / compileRaise / compileAssert
```

---

## 实现要点

### 1. try/catch/finally 字节码布局

```
OP_PUSH_EXCEPT  [2B: handler_offset]  ← 注册处理器，跳转到 handler 区
[try body]
OP_POP_EXCEPT   ← 正常退出 try，移除处理器
[finally inline]（若有 finally）
OP_JUMP [2B: after_handlers]

[catch handlers]:
  OP_DUP         ← 复制异常对象（MS_ERROR_VALUE 包裹的异常）
  OP_ISINSTANCE  ← 与 ExcType 比较（T050 前 stub）
  OP_POP_JUMP_FALSE [next_handler]
  OP_POP         ← 不匹配时弹出异常
  [bind name]    ← catch ExcType as name → SET_LOCAL(name)
  [catch body]
  OP_POP_EXCEPT
  [finally inline]
  OP_JUMP [after_handlers]

[default_handler or re-raise]:
  OP_RERAISE     ← 若无 catch 匹配，重抛
  [finally inline]

[after_handlers]:
[finally as separate block if needed]
```

### 2. 实现

```c
static void compileTry(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;

  // 注册异常处理器（跳转偏移在 handler 区起始）
  uint32_t pushExcept = emitJump(c, OP_PUSH_EXCEPT, line);  // 2B 偏移

  // 编译 try body
  compileBlock(c, n->try_stmt.body);

  // 正常退出：弹出处理器
  emit(c, OP_POP_EXCEPT, line);

  // finally 内联（正常路径）
  if (n->try_stmt.finally_block) {
    compileBlock(c, n->try_stmt.finally_block);
  }
  uint32_t jumpAfter = emitJump(c, OP_JUMP, line);  // 跳过 handlers

  // handler 区起始：回填 PUSH_EXCEPT 的目标
  patchJump(c, pushExcept);

  // 编译 catch 子句
  uint32_t nextHandlerPatches[32]; int nhCount = 0;
  uint32_t handlerEndPatches[32];  int heCount = 0;

  for (MsNodeList* l = n->try_stmt.handlers; l; l = l->next) {
    MsNode* handler = l->node;
    // handler->catch_clause.exc_type  / .bind_name / .body

    if (handler->catch_clause.exc_type != NULL) {
      // 类型检查
      emit(c, OP_DUP, line);                   // 复制异常
      compileExpr(c, handler->catch_clause.exc_type);
      emit(c, OP_ISINSTANCE, line);
      uint32_t notMatch = emitJump(c, OP_POP_JUMP_FALSE, line);
      nextHandlerPatches[nhCount++] = notMatch;

      // 匹配：bind as name（可选）
      if (handler->catch_clause.bind_name) {
        // 将异常对象绑定到局部变量
        int slot = declareLocal(c, handler->catch_clause.bind_name,
                    handler->catch_clause.bind_len);
        markInitialized(c);
        emitOp8(c->chunk, OP_SET_LOCAL, (uint8_t)slot, line);
      } else {
        emit(c, OP_POP, line);  // 丢弃异常对象
      }
    } else {
      // 无类型过滤（catch all）：直接弹出异常
      emit(c, OP_POP, line);
    }

    // 编译 catch body
    scopeBegin(c);
    compileBlock(c, handler->catch_clause.body);
    scopeEnd(c);

    // 清除绑定（将绑定名置 nil）
    if (handler->catch_clause.bind_name) {
      emit(c, OP_NIL, line);
      emitSetVar(c, handler->catch_clause.bind_name,
                       handler->catch_clause.bind_len, line);
      emit(c, OP_POP, line);
    }

    emit(c, OP_POP_EXCEPT, line);

    // finally 内联（catch 成功路径）
    if (n->try_stmt.finally_block) {
      compileBlock(c, n->try_stmt.finally_block);
    }

    uint32_t p = emitJump(c, OP_JUMP, line);
    handlerEndPatches[heCount++] = p;

    // 回填"不匹配"跳转到下一 handler
    for (int i = 0; i < nhCount; i++) patchJump(c, nextHandlerPatches[i]);
    nhCount = 0;
  }

  // 没有匹配的 catch：重抛
  if (n->try_stmt.finally_block) {
    compileBlock(c, n->try_stmt.finally_block);  // finally 内联（重抛路径）
  }
  emit(c, OP_RERAISE, line);

  // 回填所有 handler end 跳转
  for (int i = 0; i < heCount; i++) patchJump(c, handlerEndPatches[i]);
  patchJump(c, jumpAfter);
}
```

### 3. raise 编译

```c
static void compileRaise(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  if (n->single_expr.expr) {
    compileExpr(c, n->single_expr.expr);  // 异常对象
    if (n->single_expr.expr2) {
      compileExpr(c, n->single_expr.expr2);  // from cause
    } else {
      emit(c, OP_NIL, line);
    }
    emit(c, OP_RAISE, line);
  } else {
    emit(c, OP_RERAISE, line);
  }
}
```

### 4. assert 编译

```c
static void compileAssert(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  // 在 Release 构建（-DNDEBUG）下，assert 被完全删除（emit 0 字节）
#ifndef NDEBUG
  compileExpr(c, n->single_expr.expr);    // 条件
  uint32_t skip = emitJump(c, OP_POP_JUMP_TRUE, line);  // 为真跳过
  if (n->single_expr.expr2) {
    compileExpr(c, n->single_expr.expr2);  // 消息
  } else {
    emit(c, OP_NIL, line);
  }
  emit(c, OP_ASSERT, line);  // 抛出 AssertionError
  patchJump(c, skip);
#endif
}
```

---

## 验收标准（checklist）

- [ ] `"try { } catch E { }"` → `OP_PUSH_EXCEPT`, try body, `OP_POP_EXCEPT`, `OP_JUMP`, handler, `OP_RERAISE`。
- [ ] `"try { } finally { }"` → finally 在正常/异常两路径都内联。
- [ ] `"raise ValueError()"` → `OP_CALL(0)`, `OP_NIL`, `OP_RAISE`。
- [ ] `"raise"` → `OP_RERAISE`。
- [ ] `"assert x > 0"` → `OP_POP_JUMP_TRUE`, `OP_NIL`, `OP_ASSERT`（debug 模式）。
- [ ] `"assert x, \"msg\""` → 有消息参数时压入消息字符串再 `OP_ASSERT`。
- [ ] `try { } catch E as e { }` → 异常对象绑定到局部变量 `e`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_exception_compile.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

static void testTryCatch(void) {
  MsCompileResult r = msCompile(
    "try { pass } catch Exception { pass }", 38, "<t>");
  MS_ASSERT_TRUE(!r.hadError, "no error");
  bool hasPushExcept = false;
  for (uint32_t i = 0; i < r.chunk->codeLen; i++)
    if (r.chunk->code[i] == OP_PUSH_EXCEPT) hasPushExcept = true;
  MS_ASSERT_TRUE(hasPushExcept, "has PUSH_EXCEPT");
  msCompileResultFree(&r);
}

int main(void) {
  MS_RUN(testTryCatch);
  return msTestSummary();
}
```

### .ms 使用示例（T079–T084 后验证）

```ms
// 基本 try/catch
try {
    x := 1 / 0
} catch ZeroDivisionError as e {
    print("caught:", e)      // caught: ZeroDivisionError
}

// finally 保证执行
func readFile(path) {
    f := open(path)
    try {
        return f.read()
    } finally {
        f.close()
    }
}

// 多 catch + reraise
try {
    raise ValueError("bad")
} catch TypeError {
    print("type error")
} catch ValueError as e {
    print("value error")
    raise   // 重抛
} catch Exception as e {
    print("handled:", e)
}
// value error → (重抛) → ...
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **finally 内联复制**：finally 块内联到每条执行路径（正常退出、每个 catch、异常传播路径），代码体积增大；超大 finally 块可能导致字节码膨胀。初版接受此代价；后续可改为 finally 子程序（`jsr/ret` 模式）。
- **`OP_ISINSTANCE` 在 T050 前**：`OP_ISINSTANCE` 需要运行时类型信息；T050 之前作为 stub（总返回 true，即 catch all）。
- **异常绑定变量生命周期**：Python3 风格，`catch E as e` 中 `e` 在 catch 块结束后被置为 `nil`（避免循环引用）；编译器在 catch 块末尾 emit `OP_NIL + SET_LOCAL`。
