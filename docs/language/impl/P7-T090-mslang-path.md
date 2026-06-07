# P7-T090 MSLANG_PATH + 内置模块注册

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现模块搜索路径（`MSLANG_PATH` 环境变量 + 默认路径），以及将所有内置 C 模块（`math`、`os`、`sys` 等）预先注册到模块缓存，使其可以被 `import` 到。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P7-T088 | 模块缓存 |
| P8-T096 ~ T104 | 各内置模块（注册前提） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `modules.md` | §4 MSLANG_PATH 搜索路径 |

---

## 实现要点

### 1. 搜索路径初始化

```c
// MsVM 新增：
typedef struct MsSearchPath {
  char** dirs;
  uint32_t count, cap;
} MsSearchPath;

MsSearchPath gSearchPath;

// msVMInit 时初始化搜索路径顺序：
// 1. 当前执行文件所在目录（动态：每次 run 时设置）
// 2. MSLANG_PATH 环境变量（冒号分隔，Windows 用分号）
// 3. <executable_dir>/lib/          （内置 stdlib .ms 文件位置）
// 4. <executable_dir>/lib/stdlib/   （内置 stdlib fallback）
void msInitSearchPath(const char* scriptFile) {
  msSearchPathClear(&gSearchPath);

  // 1. 脚本所在目录
  if (scriptFile) {
    char dir[MAX_PATH];
    strlcpy(dir, scriptFile, sizeof(dir));
    char* sep = strrchr(dir, '/');
    if (sep) { *sep = '\0'; msSearchPathAdd(&gSearchPath, dir); }
  }

  // 2. MSLANG_PATH
  const char* envPath = getenv("MSLANG_PATH");
  if (envPath) {
    char* copy = msStrdup(envPath);
    char* tok = strtok(copy, ":");
    while (tok) { msSearchPathAdd(&gSearchPath, tok); tok = strtok(NULL, ":"); }
    msFree(copy);
  }

  // 3. 可执行文件目录下的 lib/
  char exeDir[MAX_PATH];
  msGetExeDir(exeDir, sizeof(exeDir));
  char libDir[MAX_PATH];
  snprintf(libDir, sizeof(libDir), "%s/lib", exeDir);
  msSearchPathAdd(&gSearchPath, libDir);
}
```

### 2. 内置 C 模块注册

```c
// 内置模块描述符
typedef struct MsBuiltinModuleDef {
  const char*       name;
  struct MsMethodDef*   funcs;    // NULL 结尾
  MsBuiltinConst*   consts;   // NULL 结尾
} MsBuiltinModuleDef;

MsValue msNewBuiltinModule(const MsBuiltinModuleDef* def) {
  MsValue mod = msNewModule(def->name, strlen(def->name));
  MsModuleObj* m = (MsModuleObj*)MS_AS_OBJ(mod);
  // 将函数和常量填入模块全局命名空间
  for (struct MsMethodDef* fn = def->funcs; fn->name; fn++) {
    MsValue key = msNewStrIntern(fn->name, strlen(fn->name));
    MsValue val = msNewCFunction(fn->func, fn->name, fn->arity);
    msMapSet(MS_OBJ_VAL(m->globals), key, val);
  }
  for (MsBuiltinConst* c = def->consts; c->name; c++) {
    MsValue key = msNewStrIntern(c->name, strlen(c->name));
    msMapSet(MS_OBJ_VAL(m->globals), key, c->value);
  }
  m->initialized = true;
  return mod;
}

// msVMInit 时注册所有内置模块（P8 之后逐步填入）：
void msRegisterBuiltins(void) {
  // sys（P8-T096 之前提供 basic sys）
  msModuleCacheSet("sys",   msNewBuiltinModule(&msSysModuleDef));
  // math（P12-T133 之后完整，这里先 stub）
  // msModuleCacheSet("math",  msNewBuiltinModule(&msMathModuleDef));
  // ...
}
```

### 3. `sys` 模块基础（早期占位）

```c
// sys.argv / sys.path / sys.version
MsBuiltinModuleDef msSysModuleDef = {
  .name   = "sys",
  .funcs  = (struct MsMethodDef[]) {
    { "exit", sysExit, 1 },
    { NULL }
  },
  .consts = (MsBuiltinConst[]) {
    { "version", MSLANG_VERSION_STR },  // "mslang 0.1.0"
    { NULL }
  }
};
```

---

## 验收标准（checklist）

- [ ] `import math` 找不到 .ms 时先从内置模块缓存查找。
- [ ] `MSLANG_PATH=/tmp/mods mslang run foo.ms` → `/tmp/mods` 加入搜索路径第 2 位。
- [ ] `import sys; print(sys.version)` → 打印版本字符串。
- [ ] 内置模块与用户同名模块时，用户模块优先（搜索路径靠前）。

---

## 测试用例（.ms）

```ms
import sys
print(sys.version)    // mslang 0.1.0
print(type(sys.argv)) // list

import sys
import sys            // 第二次 import 命中缓存，不重复注册
```

---

## Benchmark

N/A。

---

## 风险与边界

- **`sys.argv`**：在 `mslang run foo.ms arg1 arg2` 时，`sys.argv = ["foo.ms", "arg1", "arg2"]`；需要在 CLI 入口（T004）设置后再注册 sys 模块。
