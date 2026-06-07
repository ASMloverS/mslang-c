# P7-T088 模块缓存 + 循环导入

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现模块缓存（`sys.modules` 等价物）：确保每个模块只被加载执行一次；同时正确处理循环导入（A 导入 B，B 又导入 A）——返回部分初始化的模块对象，而不是死循环。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P7-T086 | MsModuleObj |
| P7-T087 | import 解析（OP_IMPORT） |
| P4-T060 | MsMapObj（模块缓存表） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `modules.md` | §2 模块缓存（import 查找顺序） |

---

## 实现要点

### 1. 全局模块注册表

```c
// 在 MsVM 中增加：
typedef struct MsVM {
  // ...
  MsObject* moduleCache;   // MsMapObj*: name → MsModuleObj
} MsVM;

MsValue msModuleCacheGet(const char* name) {
  MsValue key = msNewStrIntern(name, strlen(name));
  return msMapGet(MS_OBJ_VAL(gVM.moduleCache), key);
}

void msModuleCacheSet(const char* name, MsValue mod) {
  MsValue key = msNewStrIntern(name, strlen(name));
  msMapSet(MS_OBJ_VAL(gVM.moduleCache), key, mod);
}
```

### 2. 循环导入处理策略

**提前注册**（Python-like）：
1. 先创建空 `MsModuleObj`
2. 立即注册到 `moduleCache`
3. 然后执行模块代码

这样，当 A 执行过程中导入 B，B 又导入 A 时，A 的空模块对象已在缓存中，B 拿到的是部分初始化的 A。

```c
// OP_IMPORT 中（参见 T087）：
MsValue mod = msNewModule(name, strlen(name));
msModuleCacheSet(name, mod);          // 提前注册！
MsValue result = msModuleExec(m, chunk);
if (MS_IS_ERROR(result)) {
  // 执行失败：从缓存中移除（允许重试）
  msModuleCacheRemove(name);
  return result;
}
```

### 3. 内置模块预注册

```c
// msVMInit 时注册所有内置模块（T090）：
void msRegisterBuiltins(void) {
  msModuleCacheSet("sys",  msNewBuiltinModule(&msSysModuleDef));
  msModuleCacheSet("math", msNewBuiltinModule(&msMathModuleDef));
  // ...
}
```

---

## 验收标准（checklist）

- [ ] 同一模块 `import` 两次 → 只执行一次（缓存命中）。
- [ ] `sys.modules["math"]` 与第二次 `import math` 得到同一对象（指针相等）。
- [ ] 循环导入 A↔B → 不死循环，B 拿到部分初始化的 A。
- [ ] 模块执行失败 → 从缓存中移除（下次 import 可重试）。

---

## 测试用例（.ms）

```ms
// a.ms
import b
print("a loaded, b.val =", b.val)
val := 10

// b.ms
import a
print("b loaded, a.val =", a.val)  // 此时 a 还没完全初始化
val := 20

// main.ms（循环导入测试）
import a
// 预期输出（循环导入，a.val 在 b 导入时还未初始化）：
// b loaded, a.val = nil
// a loaded, b.val = 20
```

```ms
// 重复导入测试
import math
import math         // 应不重新执行 math.ms

x := import math    // 同一对象
y := import math
print(x is y)       // true（同一模块对象）
```

---

## 风险与边界

- **循环导入的属性顺序**：循环导入时，B 能看到 A 的已定义属性（执行到循环点之前的部分）；未执行部分为 `nil`。这是已知语义，文档中明确说明。
- **模块缓存 GC 根**：`gVM.moduleCache` 是 GC 根，须在 `markRoots` 中标记。
