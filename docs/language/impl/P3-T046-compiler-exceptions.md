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
  OP_LOAD_EXCEPTION  ← 取当前异常对象（errors.md §5.4）
  CONST <ExcType>    ← 压入待匹配类型
  OP_ISINSTANCE      ← 类型匹配检查（T050 前 stub）
  OP_JUMP_IF_FALSE [next_handler]
  OP_LOAD_EXCEPTION  ← 取异常对象绑定到局部变量（有 bind name 时）
  OP_SET_LOCAL(slot) ← 写入局部变量（有 bind name 时）
  OP_CLEAR_EXCEPTION ← 清除异常寄存器（errors.md §5.4）
  [catch body]
  OP_POP_EXCEPT
  [finally inline]
  OP_JUMP [after_handlers]

[default_handler or re-raise]:
  OP_RERAISE     ← 若无 catch 匹配，重抛
  [finally inline]

[after_handlers]:
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
    // handler->catch_clause.exc_types (MsNodeList*)  / .bind_name / .body

    int slot = -1;  // 绑定变量槽（-1 表示无绑定）

    if (handler->catch_clause.exc_types != NULL) {
      // 多类型 OR 匹配：逐类型检查（errors.md §5.4：LOAD_EXCEPTION / ISINSTANCE / JUMP_IF_FALSE）
      uint32_t matchPatches[16]; int matchCount = 0;
      for (MsNodeList* tl = handler->catch_clause.exc_types; tl; tl = tl->next) {
        emit(c, OP_LOAD_EXCEPTION, line);
        compileExpr(c, tl->node);
        emit(c, OP_ISINSTANCE, line);
        if (tl->next) {
          MS_ASSERT(matchCount < 16);
          matchPatches[matchCount++] = emitJump(c, OP_JUMP_IF_TRUE, line);
        } else {
          MS_ASSERT(nhCount < 32);
          uint32_t notMatch = emitJump(c, OP_JUMP_IF_FALSE, line);
          nextHandlerPatches[nhCount++] = notMatch;
        }
      }
      for (int i = 0; i < matchCount; i++) patchJump(c, matchPatches[i]);

      // 匹配：bind as name（可选），按 errors.md §5.4 用 LOAD_EXCEPTION 取对象
      if (handler->catch_clause.bind_name) {
        slot = declareLocal(c, handler->catch_clause.bind_name,
            handler->catch_clause.bind_len);
        markInitialized(c);
        emit(c, OP_LOAD_EXCEPTION, line);
        emitOp8(c->chunk, OP_SET_LOCAL, (uint8_t)slot, line);
      }
      emit(c, OP_CLEAR_EXCEPTION, line);
    } else {
      // 无类型过滤（catch all）：直接清除异常
      emit(c, OP_CLEAR_EXCEPTION, line);
    }

    // 编译 catch body
    scopeBegin(c);
    compileBlock(c, handler->catch_clause.body);
    scopeEnd(c);

    // 清除绑定（将绑定名置 nil，errors.md §3：catch (e) 作用域仅限块内）
    if (slot >= 0) {
      emit(c, OP_CONST_NIL, line);
      emitOp8(c->chunk, OP_SET_LOCAL, (uint8_t)slot, line);
    }

    emit(c, OP_POP_EXCEPT, line);

    // finally 内联（catch 成功路径）
    if (n->try_stmt.finally_block) {
      compileBlock(c, n->try_stmt.finally_block);
    }

    MS_ASSERT(heCount < 32);
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
    emit(c, OP_RAISE, line);
  } else {
    emit(c, OP_RERAISE, line);             // 裸 raise：重抛当前异常
  }
}
```

### 4. assert 编译

```c
static void compileAssert(MsCompiler* c, MsNode* n) {
  uint32_t line = n->pos.line;
  compileExpr(c, n->single_expr.expr);    // 条件
  uint32_t skip = emitJump(c, OP_JUMP_IF_TRUE, line);  // 为真跳过
  if (n->single_expr.expr2) {
    compileExpr(c, n->single_expr.expr2);  // 消息
  } else {
    emit(c, OP_CONST_NIL, line);
  }
  emit(c, OP_RAISE_ASSERT, line);  // 抛出 AssertionError
  patchJump(c, skip);
}
```

---

## 验收标准（checklist）

- [ ] `"try { } catch (e: Exception) { }"` → `OP_PUSH_EXCEPT`, try body, `OP_POP_EXCEPT`, `OP_JUMP`, handler, `OP_RERAISE`。
- [ ] `"try { } finally { }"` → finally 在正常/异常两路径都内联。
- [ ] `"raise ValueError()"` → `OP_CALL(0)`, `OP_RAISE`。
- [ ] `"raise"` → `OP_RERAISE`。
- [ ] `"assert x > 0"` → `OP_JUMP_IF_TRUE`, `OP_CONST_NIL`, `OP_RAISE_ASSERT`。
- [ ] `"assert x, \"msg\""` → 有消息参数时压入消息字符串再 `OP_RAISE_ASSERT`。
- [ ] `"try { } catch (e: TypeError, ValueError) { }"` → 多类型 OR 匹配均进入同一 handler。
- [ ] `catch (e: E)` → 异常对象绑定到局部变量 `e`。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_exception_compile.c`）

```c
#include "ms_test.h"
#include "mslang/ms_compiler.h"
#include "mslang/ms_opcode.h"

static void testTryCatch(void) {
  MsCompileResult r = msCompile(
    "try { pass } catch (e: Exception) { pass }", 43, "<t>");
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
} catch (e: ZeroDivisionError) {
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
} catch (e: TypeError) {
    print("type error")
} catch (e: ValueError) {
    print("value error")
    raise   // 重抛
} catch (e: Exception) {
    print("handled:", e)
}
// value error → (重抛) → ...

// 多类型合并 catch
try {
    raise IndexError("oob")
} catch (e: TypeError, IndexError) {
    print("type or index error:", e)
}
```

---

## Benchmark

N/A（归入 T048 整体编译 bench）。

---

## 风险与边界

- **finally 内联复制**：finally 块内联到每条执行路径（正常退出、每个 catch、异常传播路径），代码体积增大；超大 finally 块可能导致字节码膨胀。初版接受此代价；后续可改为 finally 子程序（`jsr/ret` 模式）。
- **`OP_ISINSTANCE` 在 T050 前**：`OP_ISINSTANCE` 需要运行时类型信息；T050 之前作为 stub（总返回 true，即 catch all）。
- **异常绑定变量生命周期**：Python3 风格，`catch (e: E)` 中 `e` 在 catch 块结束后被置为 `nil`（避免循环引用）；编译器在 catch 块末尾 emit `OP_CONST_NIL + OP_SET_LOCAL`。
