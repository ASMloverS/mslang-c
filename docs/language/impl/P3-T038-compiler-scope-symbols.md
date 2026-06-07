# P3-T038 作用域 / 局部变量槽 / 符号表

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现编译器的作用域管理（`MsScope`）：局部变量分配（基于栈槽编号）、嵌套作用域（函数/块）、upvalue 捕获预分析。这是表达式/变量编译（T039–T040）的基础。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P3-T037 | `MsChunk` |
| P0-T002 | `MsVec` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3 调用帧（MsFrame / 局部槽布局） |
| `vm.md` | §3.3 upvalue 解析（开放/关闭） |

---

## 待实现（C 文件 / 结构 / 函数）

### 新增文件

```
src/compiler/ms_scope.h   # 作用域管理
src/compiler/ms_scope.c
```

---

## 实现要点

### 1. 局部变量记录

```c
struct MsLocal {
  const char* name;     // 变量名（指向源文件字节）
  uint32_t    nameLen;
  int         depth;    // 所属块深度（0=函数顶层）
  bool        captured; // 是否被内层闭包捕获（成为 upvalue）
  bool        is_const; // 常量（var x = …，初版暂不区分）
};
```

### 2. Upvalue 描述符

```c
struct MsUpvalue {
  bool     is_local;  // true：直接从外层局部变量捕获；false：从外层 upvalue 捕获
  uint8_t  index;     // 外层局部槽号或外层 upvalue 索引
};
```

### 3. 编译器状态（`MsCompiler`）

```c
typedef struct MsCompiler {
  struct MsCompiler* enclosing;  // 外层编译器（NULL → 顶层）
  MsChunk*    chunk;      // 当前函数的字节码块
  struct MsLocal     locals[256]; // 局部变量表（静态上限 256 个，与 VM 槽位对应）
  int         localCount;
  int         scopeDepth;  // 当前块嵌套深度（函数内计数，与 upvalue 无关）
  struct MsUpvalue   upvalues[256];
  int         upvalueCount;
  bool        isFunction;  // 是否在函数内（非顶层全局作用域）
} MsCompiler;
```

### 4. 核心 API

```c
void compilerInit(MsCompiler* c, MsCompiler* enclosing, MsChunk* chunk, bool isFunc);
void compilerFree(MsCompiler* c);

// 作用域管理
void scopeBegin(MsCompiler* c);
void scopeEnd(MsCompiler* c);   // 发出 OP_POP / OP_CLOSE_UPVALUE 清理

// 局部变量
int  declareLocal(MsCompiler* c, const char* name, uint32_t len);  // 返回槽号
int  resolveLocal(MsCompiler* c, const char* name, uint32_t len);  // 返回槽号或 -1
void markInitialized(MsCompiler* c);  // 标记最新局部变量已初始化（防止自引用）

// Upvalue
int  resolveUpvalue(MsCompiler* c, const char* name, uint32_t len); // 返回 upvalue 索引或 -1
int  addUpvalue(MsCompiler* c, uint8_t index, bool isLocal);
```

### 5. 作用域示例

```
func outer() {
    var x = 1         // local[0] depth=0
    {                 // scopeBegin → depth=1
        var y = 2     // local[1] depth=1
    }                 // scopeEnd → OP_POP for y
    func inner() {
        print(x)      // x is upvalue from outer
    }
    inner()
}
```

`resolveUpvalue` 递归向上查找：在当前函数找不到 → 在 `enclosing` 中 `resolveLocal`，若找到则 `addUpvalue(isLocal=true)`；若 `enclosing` 也找不到，则 `resolveUpvalue(enclosing, …)` 并 `addUpvalue(isLocal=false)`。

---

## 验收标准（checklist）

- [ ] `declareLocal("x", 1)` 返回 0；再次 `declareLocal("y", 1)` 返回 1。
- [ ] `resolveLocal("x", 1)` 返回 0；`resolveLocal("z", 1)` 返回 -1。
- [ ] `scopeEnd` 对 `depth=1` 的局部变量：若未被捕获，emit `OP_POP`；若已被捕获（`captured=true`），emit `OP_CLOSE_UPVALUE`。
- [ ] 256 个局部变量超出时报编译错误（"too many local variables"）。
- [ ] `resolveUpvalue` 跨两级函数嵌套正确找到变量（upvalue of upvalue）。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_scope.c`）

```c
#include "ms_test.h"
#include "ms_scope.h"
#include "mslang/ms_chunk.h"

static void testLocalResolve(void) {
  MsChunk ck; msChunkInit(&ck, "<t>");
  MsCompiler c;
  compilerInit(&c, NULL, &ck, true);

  int idx0 = declareLocal(&c, "x", 1);
  int idx1 = declareLocal(&c, "y", 1);

  MS_ASSERT_EQ(idx0, 0, "x=slot0");
  MS_ASSERT_EQ(idx1, 1, "y=slot1");
  MS_ASSERT_EQ(resolveLocal(&c, "x", 1), 0, "resolve x");
  MS_ASSERT_EQ(resolveLocal(&c, "z", 1), -1, "resolve z not found");

  compilerFree(&c);
  msChunkFree(&ck);
}

int main(void) {
  MS_RUN(testLocalResolve);
  return msTestSummary();
}
```

---

## .ms 使用示例

N/A（作用域管理为 C 内部实现，通过 disasm 间接验证变量槽编号）。

---

## Benchmark

N/A（作用域管理开销在编译阶段，归入 T048 disasm/整体编译 bench）。

---

## 风险与边界

- **同名变量遮蔽**：同一作用域不允许重复声明（`declareLocal` 时检查 `depth == c.scopeDepth`，已存在则报错）；不同 `depth` 允许遮蔽（内层覆盖外层）。
- **全局变量**：顶层（`isFunction=false`）的变量视为全局变量（`OP_GET_GLOBAL`/`OP_SET_GLOBAL`，按名称字符串索引），不分配局部槽。
- **upvalue 上限**：256 个 upvalue（`uint8_t index`），超出报编译错误。
