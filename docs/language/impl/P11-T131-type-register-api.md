# P11-T131 MsType 注册 API

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 C API 层的**自定义类型注册**：C 代码可以定义新的 `MsType`（含 tp_* 槽函数）并将其注册到 VM，使 .ms 代码可以实例化 C 自定义类型、调用其方法、在 `isinstance` 中识别。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T072 | MsTypeObj + 实例化机制 |
| P11-T130 | 扩展模块 API |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `c-api.md` | §7 自定义类型注册 |

---

## 实现要点

### 1. MsTypeSpec（类型规格，C 侧填充）

```c
typedef struct MsTypeSpec {
    const char*      name;           // 类型名（如 "File"）
    size_t           instanceSize;   // sizeof(自定义结构)
    MsCFunctionDef*  methods;        // 方法表（C 函数）
    MsConstDef*      classAttrs;     // 类属性（常量）

    // tp_* 槽（可选，NULL = 使用默认行为）
    MsValue  (*tp_init)   (MsThread*, MsValue self, MsValue* args, int argc);
    void     (*tp_mark)   (MsObject*);
    void     (*tp_free)   (MsObject*);
    MsValue  (*tp_repr)   (MsValue);
    MsValue  (*tp_str)    (MsValue);
    uint32_t (*tp_hash)   (MsValue);
    MsValue  (*tp_getattr)(MsValue, MsValue name);
    MsValue  (*tp_setattr)(MsValue, MsValue name, MsValue val);
    MsValue  (*tp_call)   (MsThread*, MsValue, MsValue* args, int argc);
    int64_t  (*tp_len)    (MsValue);
    MsValue  (*tp_iter)   (MsValue);
    MsValue  (*tp_next)   (MsValue);
    MsValue  (*tp_getitem)(MsValue, MsValue key);
    MsValue  (*tp_setitem)(MsValue, MsValue key, MsValue val);
    // 算术槽...
} MsTypeSpec;
```

### 2. 注册自定义类型

```c
// 从 MsTypeSpec 创建 MsType 并注册到 VM
MsValue msRegisterType(MsVM* vm, const MsTypeSpec* spec,
                       MsValue bases[], int nbases) {
    // 1. 创建内部 MsType
    MsType* ty = msAlloc(sizeof(MsType));
    ty->name         = spec->name;
    ty->instanceSize = spec->instanceSize;
    ty->tp_mark      = spec->tp_mark;
    ty->tp_free      = spec->tp_free;
    ty->tp_repr      = spec->tp_repr;
    // ... 填充所有 tp_* 槽

    // 2. 创建 MsTypeObj（GC 管理的类型对象）
    MsTypeObj* typeObj = msGCAlloc(sizeof(MsTypeObj), &msTypeType);
    typeObj->mstype = *ty;
    msFree(ty);

    // 3. 将方法表填入 typeObj->methods
    typeObj->methods = (MsMapObj*)MS_AS_OBJ(msNewMap());
    if (spec->methods) {
        for (const MsCFunctionDef* f = spec->methods; f->name; f++) {
            MsValue fn = msNewCFunction(f->func, f->name, f->arity);
            msMapSetStr(MS_OBJ_VAL(typeObj->methods), f->name, fn);
        }
    }

    // 4. 构建 MRO（基类 + 自身）
    msBuildMROFromBases(typeObj, bases, nbases);

    return MS_OBJ_VAL((MsObject*)typeObj);
}
```

### 3. 实例化 C 自定义类型

```c
// C 侧实例化（分配 instanceSize 字节 + 调用 tp_init）
MsValue msNewCustomInstance(MsThread* t, MsValue typeObj,
                            MsValue* args, int argc) {
    MsTypeObj* klass = (MsTypeObj*)MS_AS_OBJ(typeObj);
    MsInstanceObj* inst = msGCAlloc(klass->mstype.instanceSize, &klass->mstype);
    inst->klass = klass;
    inst->attrs = (MsMapObj*)MS_AS_OBJ(msNewMap());

    // 调用 tp_init（如果有）
    if (klass->mstype.tp_init) {
        MsValue r = klass->mstype.tp_init(t, MS_OBJ_VAL((MsObject*)inst), args, argc);
        if (MS_IS_ERROR(r)) return r;
    }
    return MS_OBJ_VAL((MsObject*)inst);
}
```

### 4. 完整 C 自定义类型示例

```c
// C 自定义类型：Point(x, y)
typedef struct PointObj {
    MsInstanceObj base;  // 必须是第一个字段
    double x, y;
} PointObj;

static MsValue pointInit(MsThread* t, MsValue self,
                          MsValue* args, int argc) {
    if (argc != 2) return msRaiseTypeError(t, "Point(x, y) takes 2 args");
    PointObj* p = (PointObj*)MS_AS_OBJ(self);
    if (!msToFloat(args[0], &p->x) || !msToFloat(args[1], &p->y))
        return msRaiseTypeError(t, "Point args must be numeric");
    return MS_NIL_VAL;
}

static MsValue pointRepr(MsValue v) {
    PointObj* p = (PointObj*)MS_AS_OBJ(v);
    return msStrFormat("Point(%g, %g)", p->x, p->y);
}

static MsValue pointGetX(MsThread* t, MsValue* args, int argc) {
    PointObj* p = (PointObj*)MS_AS_OBJ(args[0]);
    return MS_FLOAT_VAL(p->x);
}

static MsCFunctionDef pointMethods[] = {
    { "getX", pointGetX, 1 }, { NULL }
};

// 注册
MsValue pointType = msRegisterType(vm,
    &(MsTypeSpec){ .name="Point", .instanceSize=sizeof(PointObj),
                   .tp_init=pointInit, .tp_repr=pointRepr,
                   .methods=pointMethods },
    NULL, 0);
msModuleAddConst(myMod, "Point", pointType);
```

---

## 验收标准（checklist）

- [ ] `msRegisterType` 创建可实例化的类型对象。
- [ ] `.ms` 中 `pt := Point(1.0, 2.0)` 调用 C `tp_init`。
- [ ] `repr(pt)` 调用 C `tp_repr`，返回正确字符串。
- [ ] `pt.getX()` 调用 C 方法。
- [ ] `isinstance(pt, Point)` → `true`。
- [ ] GC 正确处理 C 自定义对象（`tp_mark`/`tp_free` 被调用）。

---

## 测试用例（C 单测）

```c
void test_custom_type(void) {
    MsVM* vm = msNewVM();
    // 注册 Point 类型（如上）
    registerPointType(vm);

    MsValue r = msRunString(vm,
        "p := Point(3.0, 4.0)\n"
        "print(repr(p))\n"       // Point(3, 4)
        "print(p.getX())",       // 3.0
        "<test>");
    MS_ASSERT(!msHasException(&vm->mainThread));
    msFreeVM(vm);
}
```

---

## Benchmark

N/A。

---

## 风险与边界

- **`instanceSize` 必须 >= `sizeof(MsInstanceObj)`**：C 自定义类型结构的第一个字段必须是 `MsInstanceObj base`（包含 GC 头），`instanceSize` 必须包含整个结构体大小。
