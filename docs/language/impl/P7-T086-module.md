# P7-T086 MsModule + 全局命名空间

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `MsModuleObj`（模块对象）：每个 `.ms` 文件对应一个模块，拥有独立的全局命名空间（`MsMapObj`）。`OP_IMPORT` 加载模块后返回 `MsModuleObj`，其属性（全局变量）通过 `OP_GET_ATTR` 访问。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T060 | map（全局命名空间） |
| P5-T068 | 调用约定（模块顶层执行如同函数调用） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §9 模块系统 |

---

## 实现要点

### 1. MsModuleObj 结构

```c
typedef struct MsModuleObj {
    MsObject  header;
    MsStrObj* name;     // 模块名（如 "math"、"os.path"）
    MsObject* globals;  // MsMapObj*（模块全局命名空间）
    MsChunk*  chunk;    // 顶层字节码（首次执行后可以保留或置 NULL）
    bool      initialized;  // 是否已执行过模块代码
} MsModuleObj;

// 创建空模块
MsValue msNewModule(const char* name, uint32_t nameLen);

// 模块属性访问（tp_getattr → 查全局命名空间）
static MsValue moduleGetAttr(MsValue v, MsValue name) {
    MsModuleObj* m = (MsModuleObj*)MS_AS_OBJ(v);
    MsValue val = msMapGet(MS_OBJ_VAL(m->globals), name);
    if (MS_IS_NIL(val)) return MS_ERROR_VALUE;  // AttributeError
    return val;
}

MsType msModuleType = {
    .name = "module", .instanceSize = sizeof(MsModuleObj),
    .tp_getattr = moduleGetAttr,
    .tp_mark    = moduleMark,
};
```

### 2. 模块执行

```c
// 在 VM 中执行模块顶层代码（类似调用无参函数）
MsValue msModuleExec(MsModuleObj* mod, MsChunk* chunk) {
    MsFrame* frame = msNewFrame();
    frame->chunk   = chunk;
    frame->ip      = chunk->code;
    frame->slots   = gVM.mainThread.sp;  // 顶层无局部变量参数
    frame->closure = MS_NIL_VAL;
    frame->caller  = gVM.mainThread.frame;

    // 切换到模块的全局命名空间
    MsValue savedGlobals = gVM.mainThread.globals;
    gVM.mainThread.globals = MS_OBJ_VAL(mod->globals);
    gVM.mainThread.frame   = frame;

    MsValue result = eval(&gVM.mainThread);

    gVM.mainThread.globals = savedGlobals;
    gVM.mainThread.frame   = frame->caller;
    msFreeFrame(frame);
    return result;
}
```

---

## 验收标准（checklist）

- [ ] `msNewModule("math", 4)` → 创建空模块，全局命名空间为空 map。
- [ ] 模块执行后，顶层变量可通过 `module.var` 访问。
- [ ] `type(module)` → "module"。
- [ ] 模块 GC：模块对象被 mark（globals 字典 + name + chunk）。

---

## 测试用例（.ms）

```ms
// mymod.ms
x := 42
func hello() { return "hello from mymod" }

// main.ms
import mymod
print(mymod.x)        // 42
print(mymod.hello())  // hello from mymod
```

---

## 风险与边界

- **全局命名空间切换**：多模块执行时，`gVM.mainThread.globals` 被临时替换；并发（P9）后每个协程有独立的 globals 引用，无需切换全局状态。
