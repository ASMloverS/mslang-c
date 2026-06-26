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
| `vm.md` | §4（调用帧 MsFrame / 局部槽布局） |
| `vm.md` | §5（闭包与 Upvalue，open/closed 机制） |
| `vm.md` | §9（opcode 命名映射，OP_POP / OP_CLOSE_UPVALUE） |

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
  int         depth;    // 所属块深度（-1=已声明未初始化哨兵；0=函数顶层）
  bool        captured; // 是否被内层闭包捕获（成为 upvalue）
  bool        isConst;  // 常量（初版暂不区分，预留）
};
```

### 2. Upvalue 描述符

```c
struct MsUpvalue {
  bool    isLocal;  // true：直接从外层局部变量捕获；false：从外层 upvalue 捕获
  uint8_t index;    // 外层局部槽号或外层 upvalue 索引
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
void msCompilerInit(MsCompiler* c, MsCompiler* enclosing, MsChunk* chunk,
                    bool isFunc);
void msCompilerFree(MsCompiler* c);

// 作用域管理
void msScopeBegin(MsCompiler* c);
void msScopeEnd(MsCompiler* c);   // 发出 OP_POP / OP_CLOSE_UPVALUE 清理

// 局部变量
// declareLocal：depth 置 -1（未初始化哨兵），返回槽号；已存在同名同深度则报错
int  msScopeDeclareLocal(MsCompiler* c, const char* name, uint32_t len);
// resolveLocal：命中 depth==-1 的条目时报"cannot read local in its own initializer"
int  msScopeResolveLocal(MsCompiler* c, const char* name, uint32_t len);
// markInitialized：将最新 local 的 depth 从 -1 改为 c->scopeDepth
void msScopeMarkInitialized(MsCompiler* c);

// Upvalue
int  msScopeResolveUpvalue(MsCompiler* c, const char* name, uint32_t len);
int  msScopeAddUpvalue(MsCompiler* c, uint8_t index, bool isLocal);
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

`msScopeResolveUpvalue` 递归向上查找：

1. 在 `enclosing` 中调 `msScopeResolveLocal`，若命中（返回 slot ≥ 0）：
   - 置 `enclosing->locals[slot].captured = true`（**必须在此步骤中置位，否则 `msScopeEnd` 永远只会 emit `OP_POP`，闭包捕获失效**）
   - 调 `msScopeAddUpvalue(c, slot, /*isLocal=*/true)`，返回 upvalue 索引
2. 否则递归：`msScopeResolveUpvalue(enclosing, …)`，若命中则 `msScopeAddUpvalue(c, uvIdx, /*isLocal=*/false)`
3. 两级均未命中，返回 -1（全局变量路径）

---

## 验收标准（checklist）

- [ ] `msScopeDeclareLocal("x", 1)` 返回 0（depth=-1 哨兵），`msScopeMarkInitialized` 后 `msScopeResolveLocal("x", 1)` 返回 0。
- [ ] `msScopeResolveLocal("z", 1)` 对未声明变量返回 -1。
- [ ] 在 `msScopeDeclareLocal` 之后、`msScopeMarkInitialized` 之前调用 `msScopeResolveLocal` 同名变量，应报"cannot read local in its own initializer"错误。
- [ ] `msScopeEnd` 对 `depth=1` 的局部变量：若 `captured=false`，emit `OP_POP`；若 `captured=true`，emit `OP_CLOSE_UPVALUE`。
- [ ] 256 个局部变量超出时报编译错误（"too many local variables"）；256 个 upvalue（`upvalueCount == 256`）超出时同样报错。
- [ ] `msScopeResolveUpvalue` 跨两级函数嵌套正确找到变量（upvalue of upvalue），且外层 local 的 `captured` 被置 true。

---

## 测试用例（C 单测 / .ms）

### C 单测（`tests/compiler/test_scope.c`）

```c
#include "ms_test.h"
#include "ms_scope.h"
#include "mslang/ms_chunk.h"

static void testLocalResolve(void) {
  MsChunk ck;
  msChunkInit(&ck, NULL);  // sourceName=NULL，单测不需要 MsStr*
  MsCompiler c;
  msCompilerInit(&c, NULL, &ck, true);

  int idx0 = msScopeDeclareLocal(&c, "x", 1);
  msScopeMarkInitialized(&c);
  int idx1 = msScopeDeclareLocal(&c, "y", 1);
  msScopeMarkInitialized(&c);

  MS_ASSERT_EQ(idx0, 0, "x=slot0");
  MS_ASSERT_EQ(idx1, 1, "y=slot1");
  MS_ASSERT_EQ(msScopeResolveLocal(&c, "x", 1), 0, "resolve x");
  MS_ASSERT_EQ(msScopeResolveLocal(&c, "z", 1), -1, "resolve z not found");

  msCompilerFree(&c);
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
