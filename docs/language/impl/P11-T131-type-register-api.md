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
| `c-api.md` | §6.4 类型（MsType）注册 |

---

## 实现要点

### 1. MsTypeSpec（类型规格，C 侧填充）

```c
typedef struct MsTypeSpec {
  const char*      name;           // 类型名（如 "File"）
  size_t           instanceSize;   // sizeof(自定义结构)
  struct MsMethodDef* methods;     // 方法表（C 函数，c-api.md §6.2）
  MsConstDef*      classAttrs;     // 类属性（常量）

  // tp_* 槽（可选，NULL = 使用默认行为）
  MsValue  (*tpInit)   (MsVM*, MsValue self, MsValue* argv, int argc);
  void     (*traverse) (MsObject*, MsRootVisitor, void*);  // GC 遍历
  void     (*destroy)  (MsObject*);                        // 析构
  MsValue  (*tpRepr)   (MsValue);
  MsValue  (*tpStr)    (MsValue);
  uint32_t (*tpHash)   (MsValue);
  MsValue  (*tpGetattr)(MsValue, MsValue name);
  MsValue  (*tpSetattr)(MsValue, MsValue name, MsValue val);
  MsValue  (*tpCall)   (MsVM*, MsValue, MsValue* argv, int argc);
  int64_t  (*tpLen)    (MsValue);
  MsValue  (*tpIter)   (MsValue);
  MsValue  (*tpNext)   (MsValue);
  MsValue  (*tpGetitem)(MsValue, MsValue key);
  MsValue  (*tpSetitem)(MsValue, MsValue key, MsValue val);
  // 算术槽...
} MsTypeSpec;
```

### 2. 注册自定义类型

```c
// 从 MsTypeSpec 创建 MsType 并注册到 VM
MsValue msRegisterType(MsVM* vm, const MsTypeSpec* spec,
                       MsValue bases[], int nbases) {
  // 1. 创建内部 MsType
  MsType* ty = msAlloc(sizeof(*ty));
  ty->name         = spec->name;
  ty->instanceSize = spec->instanceSize;
  ty->traverse    = spec->traverse;
  ty->destroy     = spec->destroy;
  ty->tpRepr      = spec->tpRepr;
  // ... 填充所有 tp_* 槽

  // 2. 创建 MsTypeObj（GC 管理的类型对象）
  MsTypeObj* typeObj = msGCAlloc(sizeof(*typeObj), &msTypeType);
  typeObj->mstype = *ty;
  msFree(ty);

  // 3. 将方法表填入 typeObj->methods
  typeObj->methods = (MsMapObj*)MS_AS_OBJ(msNewMap());
  if (spec->methods) {
    for (const struct MsMethodDef* f = spec->methods; f->name; f++) {
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
// C 侧实例化（分配 instanceSize 字节 + 调用 tpInit）
MsValue msNewCustomInstance(MsVM* vm, MsValue typeObj,
              MsValue* argv, int argc) {
  MsTypeObj* klass = (MsTypeObj*)MS_AS_OBJ(typeObj);
  MsInstanceObj* inst = msGCAlloc(klass->mstype.instanceSize, &klass->mstype);
  inst->klass = klass;
  inst->attrs = (MsMapObj*)MS_AS_OBJ(msNewMap());

  // 调用 tpInit（如果有）
  if (klass->mstype.tpInit) {
    MsValue r = klass->mstype.tpInit(vm, MS_OBJ_VAL((MsObject*)inst), argv, argc);
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

static MsValue pointInit(MsVM* vm, MsValue self,
                          MsValue* argv, int argc) {
  if (argc != 2) return msRaiseTypeError(vm, "Point(x, y) takes 2 args");
  PointObj* p = (PointObj*)MS_AS_OBJ(self);
  if (!MS_IS_FLOAT(argv[0]) && !MS_IS_INT(argv[0]))
    return msRaiseTypeError(vm, "Point args must be numeric");
  p->x = MS_IS_FLOAT(argv[0]) ? MS_AS_FLOAT(argv[0]) : (double)MS_AS_INT(argv[0]);
  p->y = MS_IS_FLOAT(argv[1]) ? MS_AS_FLOAT(argv[1]) : (double)MS_AS_INT(argv[1]);
  return MS_NIL_VAL;
}

static MsValue pointRepr(MsValue v) {
  PointObj* p = (PointObj*)MS_AS_OBJ(v);
  return msStrFormat("Point(%g, %g)", p->x, p->y);
}

static MsValue pointGetX(MsVM* vm, MsValue* argv, int argc) {
  PointObj* p = (PointObj*)MS_AS_OBJ(argv[0]);
  return MS_FLOAT_VAL(p->x);
}

static struct MsMethodDef pointMethods[] = {
  { "getX", pointGetX, 1, NULL }, { NULL }
};

// 注册
MsValue pointType = msRegisterType(vm,
  &(MsTypeSpec){ .name="Point", .instanceSize=sizeof(PointObj),
                   .tpInit=pointInit, .tpRepr=pointRepr,
                   .methods=pointMethods },
  NULL, 0);
msModuleAddConst(myMod, "Point", pointType);
```

---

## 验收标准（checklist）

- [ ] `msRegisterType` 创建可实例化的类型对象。
- [ ] `.ms` 中 `pt := Point(1.0, 2.0)` 调用 C `tpInit`。
- [ ] `repr(pt)` 调用 C `tpRepr`，返回正确字符串。
- [ ] `pt.getX()` 调用 C 方法。
- [ ] `isinstance(pt, Point)` → `true`。
- [ ] GC 正确处理 C 自定义对象（`traverse`/`destroy` 被调用）。

---

## 测试用例（C 单测）

```c
void testCustomType(void) {
  MsVM* vm = msNew();
  // 注册 Point 类型（如上）
  registerPointType(vm);

  MsValue r = msRunString(vm,
    "p := Point(3.0, 4.0)\n"
    "print(repr(p))\n"       // Point(3, 4)
    "print(p.getX())",       // 3.0
    "<test>");
  MS_ASSERT(!msIsError(r));
  msFree(vm);
}
```

---

## Benchmark

N/A。

---

## 风险与边界

- **`instanceSize` 必须 >= `sizeof(MsInstanceObj)`**：C 自定义类型结构的第一个字段必须是 `MsInstanceObj base`（包含 GC 头），`instanceSize` 必须包含整个结构体大小。
