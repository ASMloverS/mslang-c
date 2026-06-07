# P7-T089 包（`__init__.ms`）与子模块

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

支持多文件包（package）：目录内有 `__init__.ms` 时，该目录为包，`import http` 加载 `http/__init__.ms`；`import http.client` 先加载 `http/__init__.ms`，再加载 `http/client.ms`，并将 `client` 挂载为 `http.client` 属性。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P7-T087 | import 路径解析 |
| P7-T088 | 模块缓存 |

---

## 实现要点

### 1. 路径解析扩展

```c
// resolveModulePath 增加包检测：
// 先尝试 <dir>/<name>.ms
// 再尝试 <dir>/<name>/__init__.ms（包）
static bool resolveModulePath(const char* modName, ...) {
  char relPath[256];
  dotToSlash(modName, relPath, sizeof(relPath));

  // 1. 直接文件
  for each searchDir in MSLANG_PATH {
    snprintf(outPath, outLen, "%s/%s.ms", searchDir, relPath);
    if (fileExists(outPath)) return true;
  }

  // 2. 包目录
  for each searchDir in MSLANG_PATH {
    snprintf(outPath, outLen, "%s/%s/__init__.ms", searchDir, relPath);
    if (fileExists(outPath)) return true;
  }
  return false;
}
```

### 2. 父包加载（import http.client → 先加载 http）

```c
// msLoadModule(name) 在加载前递归加载父包：
static MsValue msLoadModule(const char* fullName) {
  // 找最后一个 '.' 确定父名
  const char* dot = strrchr(fullName, '.');
  if (dot) {
    char parentName[256];
    size_t parentLen = dot - fullName;
    memcpy(parentName, fullName, parentLen);
    parentName[parentLen] = '\0';

    MsValue parent = msLoadModule(parentName);  // 递归加载父包
    if (MS_IS_ERROR(parent)) return parent;
  }

  // 加载自身
  char path[MAX_PATH];
  if (!resolveModulePath(fullName, ..., path, sizeof(path)))
    return msRaiseModuleNotFoundError(gThread, fullName);

  MsValue mod = msNewModule(fullName, strlen(fullName));
  msModuleCacheSet(fullName, mod);
  // ... 编译 + 执行
  return mod;
}
```

### 3. 子模块挂载到父包

```c
// http.client 加载后，把 client 挂到 http 包：
if (dot) {
  MsValue parent = msModuleCacheGet(parentName);
  if (!MS_IS_NIL(parent)) {
    // http.client 在 http 模块下注册为 client 属性
    const char* subName = dot + 1;
    MsValue subKey = msNewStrIntern(subName, strlen(subName));
    msMapSet(MS_OBJ_VAL(((MsModuleObj*)MS_AS_OBJ(parent))->globals),
                 subKey, mod);
  }
}
```

### 4. 包内 __init__.ms 中的 `__path__`

```c
// 执行包 __init__.ms 时，在其命名空间设置 __path__
MsValue pathList = msNewList();
msListAppend(pathList, msNewStr(packageDir, strlen(packageDir)));
msMapSet(MS_OBJ_VAL(m->globals), msInternStr("__path__"), pathList);
```

---

## 验收标准（checklist）

- [ ] `import http` → 加载 `http/__init__.ms`。
- [ ] `import http.client` → 先加 `http/__init__.ms`，再加 `http/client.ms`。
- [ ] `http.client` 可通过属性访问（`http.client.get(...)`）。
- [ ] 包内 `__init__.ms` 可从子模块导入（`from http.client import *`）。
- [ ] 包的 `__path__` 属性存在。

---

## 测试用例（.ms）

```ms
// 目录结构：
// mylib/__init__.ms
// mylib/utils.ms
// mylib/net/client.ms

// mylib/__init__.ms
from mylib.utils import helper
version := "1.0"

// main.ms
import mylib
print(mylib.version)   // 1.0

import mylib.net.client
c := mylib.net.client.Client()
```

---

## 风险与边界

- **`__init__.ms` 中的相对导入**：包内文件可用 `from .utils import foo` 相对导入同包模块；`fromFile` 指向包目录下的 `__init__.ms`，相对解析从包目录出发。
