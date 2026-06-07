# P7-T087 import 路径解析

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `OP_IMPORT` 的路径查找算法：将模块名（如 `os`、`http.client`、`.utils`）转换为文件系统路径，并编译/加载 `.ms` 文件。支持绝对导入、相对导入（`.`/`..`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P7-T086 | MsModuleObj |
| P3-T037 | MsChunk + 编译器 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §9 import 解析 |
| `syntax.md` | §import 语句 |

---

## 实现要点

### 1. 模块名 → 文件路径映射

```c
// 模块名规则：
//   "math"           → <search_path>/math.ms 或 <search_path>/math/__init__.ms
//   "http.client"    → <search_path>/http/client.ms
//   ".utils"         → <当前模块目录>/utils.ms
//   "..common"       → <当前模块目录的父目录>/common.ms

static bool resolveModulePath(
  const char* modName,   // e.g. "http.client"
  const char* fromFile,  // 当前模块路径（用于相对导入）
  char*       outPath,   // 输出：绝对路径
  uint32_t    outLen
) {
  bool isRelative = (modName[0] == '.');

  if (isRelative) {
    // 从 fromFile 所在目录出发，根据点数向上走
    // ".utils"  → 1点→ same dir
    // "..utils" → 2点→ parent dir
    int dots = 0;
    while (modName[dots] == '.') dots++;
    const char* relName = modName + dots;  // strip dots

    // 取 fromFile 目录，上走 dots-1 级
    char base[MAX_PATH];
    strlcpy(base, fromFile, sizeof(base));
    for (int i = 0; i < dots; i++) {
      char* sep = strrchr(base, '/');
      if (sep) *sep = '\0';
    }
    snprintf(outPath, outLen, "%s/%s.ms", base,
                 *relName ? relName : "__init__");
  } else {
    // 绝对导入：替换 '.' → '/'，在 MSLANG_PATH 中搜索
    char relPath[256];
    dotToSlash(modName, relPath, sizeof(relPath));
    return searchMslangPath(relPath, outPath, outLen);
  }
  return fileExists(outPath);
}
```

### 2. OP_IMPORT 实现

```c
// OP_IMPORT [2B nameIdx]  → MsModuleObj（推送到栈）
case OP_IMPORT: {
  uint16_t nameIdx = READ_U16();
  MsValue  nameVal = t->frame->chunk->constants.vals[nameIdx];
  const char* name = ((MsStrObj*)MS_AS_OBJ(nameVal))->data;

  // 1. 先查模块缓存（T088）
  MsValue cached = msModuleCacheGet(name);
  if (!MS_IS_NIL(cached)) { PUSH(cached); DISPATCH(); }

  // 2. 解析路径
  char path[MAX_PATH];
  if (!resolveModulePath(name, t->frame->chunk->fileName, path, sizeof(path))) {
    return msRaiseModuleNotFoundError(t, name);
  }

  // 3. 读取 + 编译
  char* src = msReadFile(path);
  MsChunk* chunk = msCompileFile(path, src, strlen(src));
  msFree(src);
  if (!chunk) return MS_ERROR_VALUE;  // SyntaxError 已设

  // 4. 创建模块对象 + 执行
  MsValue mod = msNewModule(name, strlen(name));
  MsModuleObj* m = (MsModuleObj*)MS_AS_OBJ(mod);
  m->chunk = chunk;
  msModuleCacheSet(name, mod);  // 提前注册（防循环导入, T088）
  MsValue result = msModuleExec(m, chunk);
  if (MS_IS_ERROR(result)) return result;

  PUSH(mod);
  DISPATCH();
}
```

### 3. OP_IMPORT_FROM

```c
// OP_IMPORT_FROM [2B nameIdx]  → 从栈顶模块取属性，推送（不弹模块）
case OP_IMPORT_FROM: {
  uint16_t nameIdx = READ_U16();
  MsValue  nameVal = t->frame->chunk->constants.vals[nameIdx];
  MsValue  mod     = PEEK(0);
  MsValue  attr    = moduleGetAttr(mod, nameVal);
  if (MS_IS_ERROR(attr)) return msRaiseImportError(t, nameVal);
  PUSH(attr);
  DISPATCH();
}
```

---

## 验收标准（checklist）

- [ ] `import math` → 在 `MSLANG_PATH` 中找到 `math.ms` 并加载。
- [ ] `import http.client` → 找到 `http/client.ms`。
- [ ] `from math import pi` → 取模块属性，赋到当前全局。
- [ ] `.utils` 相对导入 → 从当前文件目录查找。
- [ ] 模块不存在 → `ModuleNotFoundError`。

---

## 测试用例（.ms）

```ms
// tests/ms/p7/import_basic.ms
import math
print(math.pi)     // 3.141592653589793
print(math.sqrt(16))  // 4.0

from math import pi, sqrt
print(pi)          // 3.141592653589793
print(sqrt(25))    // 5.0

// 嵌套模块
import http.client
c := http.client.HttpClient("localhost", 8080)
```

---

## Benchmark

N/A（import 是启动期，不在热路径）。

---

## 风险与边界

- **相对导入的 `fromFile` 来源**：`fromFile` 字段存在 `MsChunk.fileName` 中（T037）；顶层模块（`ms run foo.ms`）的 `fromFile` 为 `foo.ms` 的绝对路径。
- **路径分隔符**：Windows 下 `MAX_PATH=260`，路径分隔符统一为 `/`（POSIX 风格在 Windows 上也可用）。
