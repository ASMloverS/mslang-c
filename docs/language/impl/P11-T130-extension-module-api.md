# P11-T130 扩展模块 API（MsCFunction / 方法表 / msNewModule / msAdd*）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现**扩展模块 API**：允许 C 代码注册函数、常量、类型到 mslang 模块中，并将模块注册到 VM 的模块缓存，使 `.ms` 代码可以 `import` 加载 C 实现的模块（类似 Python 的 C 扩展）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P11-T128 | 值 API |
| P11-T129 | 错误处理 API |
| P7-T088 | 模块缓存 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `c-api.md` | §6 扩展模块 |

---

## 实现要点

### 1. MsCFunction 类型定义

```c
// C 函数签名（所有扩展函数必须符合此签名）
typedef MsValue (*MsCFunc)(MsThread* t, MsValue* args, int argc);

// C 函数定义条目（以 NULL 结尾的数组）
typedef struct MsCFunctionDef {
  const char* name;
  MsCFunc     func;
  int         arity;  // 期望参数数，-1 = 可变
} MsCFunctionDef;

// 常量定义条目
typedef struct MsConstDef {
  const char* name;
  MsValue     value;
} MsConstDef;
```

### 2. 模块创建与注册

```c
// 创建模块并注册函数
MsValue msNewExtModule(MsVM* vm, const char* name,
                       const MsCFunctionDef* funcs,
                       const MsConstDef*     consts) {
  MsValue mod = msNewModule(name, strlen(name));
  MsModuleObj* m = (MsModuleObj*)MS_AS_OBJ(mod);

  if (funcs) {
    for (const MsCFunctionDef* f = funcs; f->name; f++) {
      MsValue fn = msNewCFunction(f->func, f->name, f->arity);
      msMapSetStr(MS_OBJ_VAL(m->globals), f->name, fn);
    }
  }
  if (consts) {
    for (const MsConstDef* c = consts; c->name; c++) {
      msMapSetStr(MS_OBJ_VAL(m->globals), c->name, c->value);
    }
  }
  m->initialized = true;
  return mod;
}

// 注册到 VM 模块缓存（供 import 使用）
void msRegisterExtModule(MsVM* vm, MsValue mod) {
  MsModuleObj* m = (MsModuleObj*)MS_AS_OBJ(mod);
  msModuleCacheSet(m->name->data, mod);
}
```

### 3. 向模块动态添加成员

```c
// 向模块添加函数
void msModuleAddFunc(MsValue mod, const char* name,
                     MsCFunc func, int arity) {
  MsModuleObj* m = (MsModuleObj*)MS_AS_OBJ(mod);
  MsValue fn = msNewCFunction(func, name, arity);
  msMapSetStr(MS_OBJ_VAL(m->globals), name, fn);
}

// 向模块添加常量
void msModuleAddConst(MsValue mod, const char* name, MsValue val) {
  MsModuleObj* m = (MsModuleObj*)MS_AS_OBJ(mod);
  msMapSetStr(MS_OBJ_VAL(m->globals), name, val);
}

// 向模块添加子模块
void msModuleAddSubModule(MsValue parent, const char* name, MsValue sub) {
  msModuleAddConst(parent, name, sub);
}
```

### 4. 完整 C 扩展示例

```c
// myext.c（C 扩展模块）
#include "mslang.h"

static MsValue myextGreet(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "greet() takes 1 argument");
  const char* name; size_t len;
  if (!msToCStr(args[0], &name, &len))
    return msRaiseTypeError(t, "greet() argument must be str");
  return msStrFormat("Hello, %.*s!", (int)len, name);
}

static MsCFunctionDef myextFuncs[] = {
  { "greet", myextGreet, 1 },
  { NULL }
};

static MsConstDef myextConsts[] = {
  { "version", MS_INT_VAL(1) },
  { NULL }
};

// 模块初始化函数（惯用命名：ms_init_<modname>）
MsValue msInitMyext(MsVM* vm) {
  MsValue mod = msNewExtModule(vm, "myext", myextFuncs, myextConsts);
  msRegisterExtModule(vm, mod);
  return mod;
}
```

```ms
// main.ms（使用 C 扩展）
import myext
print(myext.greet("world"))   // Hello, world!
print(myext.version)          // 1
```

---

## 验收标准（checklist）

- [ ] `msNewExtModule` 创建含函数和常量的模块。
- [ ] `msRegisterExtModule` 后，`.ms` 中 `import myext` 可找到模块。
- [ ] C 函数在 .ms 中可以正常调用（参数传递、返回值）。
- [ ] C 函数抛出的异常可在 .ms 中 catch。
- [ ] 动态 `msModuleAddFunc` 在模块初始化后也能生效。

---

## 测试用例（C 单测）

```c
// tests/test_ext_module.c
void testCExtension(void) {
  MsVM* vm = msNewVM();
  msInitMyext(vm);  // 注册 C 扩展

  MsValue r = msRunString(vm,
    "import myext\nmyext.greet('Claude')", "<test>");
  // 期望通过（无异常）
  MS_ASSERT(!msHasException(&vm->mainThread));

  msFreeVM(vm);
}
```

---

## Benchmark

N/A（扩展模块加载是启动期操作）。

---

## 风险与边界

- **共享库加载**（动态 C 扩展）：生产版本中，`import myext` 可先查找 `myext.so`/`myext.dll` 并调用 `msInitMyext`（dlopen 方案）；初版不实现动态加载，只支持**静态链接**注册（`ms_init_*` 手动调用）。
