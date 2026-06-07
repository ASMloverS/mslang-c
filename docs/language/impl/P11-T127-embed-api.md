# P11-T127 嵌入 API（msNew / msRunFile / msRunString / 全局读写）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 mslang 的**嵌入 API**：允许 C/C++ 宿主程序创建 VM 实例、执行 `.ms` 文件或字符串、读写全局变量，将 mslang 作为嵌入式脚本引擎使用。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P11-T126 | 句柄 / 本地帧 |
| P7-T086 | 模块系统 |
| P4-T051 | VM + eval 循环 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `c-api.md` | §4 嵌入 API（Embedding） |

---

## 实现要点

### 1. VM 生命周期

```c
// mslang.h (public API header)

// 创建新的 VM 实例（c-api.md §4.1）
MsVM* msNew(void);

// 销毁 VM（触发终结，释放所有内存）
void  msFree(MsVM* vm);

// 内部实现
MsVM* msNew(void) {
  MsVM* vm = msAlloc(sizeof(*vm));
  memset(vm, 0, sizeof(MsVM));
  msGCInit(vm);
  msRegisterBuiltins(vm);
  msInitSearchPath(NULL);
  return vm;
}

void msFree(MsVM* vm) {
  msGCShutdown(vm);
  msAllocFree(vm);
}
```

### 2. 执行 API

```c
// 执行文件，返回最后一个表达式的值（或 nil）
// 失败返回 MS_ERROR_VALUE，错误信息在 vm->lastError
MsValue msRunFile(MsVM* vm, const char* path);

// 执行字符串
MsValue msRunString(MsVM* vm, const char* src, const char* name);

// 内部实现
MsValue msRunFile(MsVM* vm, const char* path) {
  char* src = msReadFile(path);
  if (!src) {
    msSetError(vm, "cannot open file: %s", path);
    return MS_ERROR_VALUE;
  }
  MsChunk* chunk = msCompileFile(path, src, strlen(src));
  msFree(src);
  if (!chunk) return MS_ERROR_VALUE;

  // 创建顶层模块并执行
  MsValue mod = msNewModule("<main>", 6);
  MsModuleObj* m = (MsModuleObj*)MS_AS_OBJ(mod);
  return msModuleExec(m, chunk);
}

MsValue msRunString(MsVM* vm, const char* src, const char* name) {
  MsChunk* chunk = msCompileFile(name, src, strlen(src));
  if (!chunk) return MS_ERROR_VALUE;
  MsValue mod = msNewModule(name, strlen(name));
  return msModuleExec((MsModuleObj*)MS_AS_OBJ(mod), chunk);
}
```

### 3. 全局变量读写

```c
// 读取全局变量（在主模块全局命名空间中查找）
MsValue msGetGlobal(MsVM* vm, const char* name);

// 设置全局变量
void    msSetGlobal(MsVM* vm, const char* name, MsValue val);

// 实现
MsValue msGetGlobal(MsVM* vm, const char* name) {
  MsValue key = msNewStrIntern(name, strlen(name));
  return msMapGet(vm->mainThread.globals, key);
}

void msSetGlobal(MsVM* vm, const char* name, MsValue val) {
  MsValue key = msNewStrIntern(name, strlen(name));
  msMapSet(vm->mainThread.globals, key, val);
}
```

### 4. 错误处理

```c
// 检查 API 返回值是否为错误哨兵（c-api.md §4.4）
int     msIsError(MsValue v);

// 获取当前异常对象（返回 MsValue，含 message/traceback 等属性）
MsValue msGetError(MsVM* vm);

// 清除当前异常
void    msClearError(MsVM* vm);
```

### 5. 嵌入示例

```c
// embed_example.c
#include "mslang.h"

int main(void) {
  MsVM* vm = msNew();

  // 注入 C 函数
  msSetGlobal(vm, "multiply",
    msNewCFunction(myMultiply, "multiply", 2));

  // 执行脚本
  MsValue result = msRunString(vm,
    "result := multiply(6, 7)\nprint(result)", "<test>");

  if (msIsError(result)) {
    MsValue exc = msGetError(vm);
    fprintf(stderr, "Error: %s\n", msStrData(msGetAttr(vm, exc, "message")));
    msClearError(vm);
  }

  msFree(vm);
  return 0;
}
```

---

## 验收标准（checklist）

- [ ] `msNew()` + `msFree()` 无内存泄漏（原函数名已对齐 c-api.md §4.1）。
- [ ] `msRunString(vm, "print(42)", "<test>")` → 打印 `42`。
- [ ] `msRunFile(vm, "nonexistent.ms")` → `MS_ERROR_VALUE` + `msHasError()` = true。
- [ ] `msSetGlobal` + `msGetGlobal` 正确读写。
- [ ] 脚本中的运行时错误可被 `msHasError` 检测到。
- [ ] 多次调用 `msRunString` 共享全局变量。

---

## 测试用例（C 单测）

```c
// tests/test_embed.c
void testEmbedBasic(void) {
  MsVM* vm = msNew();

  MsValue r = msRunString(vm, "1 + 2", "<test>");
  // ms 顶层表达式的值（若需要，通过 msGetGlobal 传回）

  msSetGlobal(vm, "x", MS_INT_VAL(42));
  MsValue x = msGetGlobal(vm, "x");
  MS_ASSERT(MS_AS_INT(x) == 42);

  msFree(vm);
}
```

---

## Benchmark

N/A（嵌入 API 是初始化路径）。

---

## 风险与边界

- **线程安全**：`MsVM` 实例不是线程安全的；多线程宿主需要每个线程一个 VM，或用互斥锁保护。
- **信号处理**：嵌入时，宿主程序的信号处理器与 mslang 的冲突（尤其是 SIGSEGV 栈溢出检测）；文档说明需要协作配置。
